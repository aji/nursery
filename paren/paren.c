// Copyright (c) 2015 Alex Iadicicco <alex@ajitek.net>
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// paren.c -- main Paren source file
// Copyright (C) 2015 Alex Iadicicco

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "paren.h"

static EvalScope *scope_new(EvalScope *up);
static EvalScope *SC_CLONE(EvalScope *sc);
static EvalScope *SC_FREE(EvalScope *sc);

static void fatal(const char *fmt, ...) {
	char buf[4096];
	va_list va;

	va_start(va, fmt);
	vsnprintf(buf, 4096, fmt, va);
	va_end(va);

	fprintf(stderr, "fatal: %s\n", buf);
	exit(1);
}

// Strings
// ===========================================================================

String *pa_str_clone(String *str) {
	if (str->refs == PERMANENT)
		return str;
	str->refs++;
	return str;
}

void pa_str_free(String *str) {
	if (str->refs == PERMANENT)
		return;
	str->refs--;
	if (str->refs > 0)
		return;
	free(str->buf);
	free(str);
}

String *pa_str_from_cstr(char *s) {
	String *str = acalloc(1, sizeof(*str));
	str->buf = strdup(s);
	str->len = strlen(s);
	str->refs = 1;
	return str;
}

String *pa_str_from_buf(char *s, size_t len) {
	String *str = acalloc(1, sizeof(*str));
	str->buf = amalloc(len);
	memcpy(str->buf, s, len);
	str->len = len;
	str->refs = 1;
	return str;
}

char pa_str_char_at(String *s, int n) {
	if (n < 0 || n >= s->len)
		return '\0';

	return s->buf[n];
}

// Structures
// ===========================================================================

static const char *char_names[256] = {
	['\n']   = "newline",
	['\r']   = "return",
	['\t']   = "tab",
	[' ']    = "space",
};

const char *pa_tag_names[] = {
	[T_NIL]       = "nil",
	[T_INTERNAL]  = "internal",
	[T_BUILTIN]   = "builtin",
	[T_SPECIAL]   = "special form",

	[T_CLOSURE]   = "closure",

	[T_CONS]      = "cons cell",
	[T_ATOM]      = "atom",
	[T_INTEGER]   = "integer",
	[T_CHARACTER] = "character",
	[T_STRSLICE]  = "string slice",
	[T_EXCEPTION] = "exception",
};

static Node _NIL    = { .tag = T_NIL,                  .refs = PERMANENT };
static Node _T      = { .tag = T_ATOM, .s = "t",       .refs = PERMANENT };
Node *NIL    = &_NIL;
Node *T      = &_T;

static Node _QUOTE  = { .tag = T_ATOM, .s = "quote",   .refs = PERMANENT };
static Node *QUOTE  = &_QUOTE;

int pa_allocs = 0;
int pa_frees  = 0;

void pa_node_delete(Node *n) {
	switch (n->tag) {
	case T_NIL:
		return;

	case T_INTERNAL:
		if (n->raw.dtor != NULL)
			n->raw.dtor(n->raw.p);
		break;

	case T_BUILTIN:
	case T_SPECIAL:
		break;

	case T_CLOSURE:
		DECREF(n->cl.args);
		DECREF(n->cl.body);
		SC_FREE(n->cl.sc);
		break;

	case T_CONS:
		DECREF(n->cons.cdr);
		DECREF(n->cons.car);
		break;

	case T_ATOM:
		free(n->s);
		break;

	case T_INTEGER:
	case T_CHARACTER:
		break;

	case T_STRSLICE:
		pa_str_free(n->str.s);
		break;

	case T_EXCEPTION:
		pa_str_free(n->exc.msg);
		break;
	}

	pa_frees++;

	free(n);
}

Node *pa_exc(const char *fmt, ...) {
	va_list va;
	char buf[8192];
	int n;

	va_start(va, fmt);
	n = vsnprintf(buf, 8192, fmt, va);
	va_end(va);

	return EXCEPTION(pa_str_from_buf(buf, n));
}

void pa_lb_init(ListBuilder *lb) {
	lb->head = lb->tail = NIL;
}

Node *pa_lb_finish(ListBuilder *lb) {
	Node *n = lb->head;
	lb->head = lb->tail = NIL;
	return n;
}

void pa_lb_append(ListBuilder *lb, Node *n) {
	if (!IS_CONS(lb->head)) {
		lb->head = CONS(n, NIL);
		lb->tail = lb->head;
	} else {
		lb->tail->cons.cdr = CONS(n, NIL);
		lb->tail = CDR(lb->tail);
	}
}

ssize_t pa_str_val(char *buf, ssize_t len, Node *n) {
	buf[0] = '\0';
	switch (n->tag) {
	case T_STRSLICE:
		len--;
		if (len > n->str.len)
			len = n->str.len;
		memcpy(buf, n->str.s->buf + n->str.start, len);
		buf[len] = '\0';
		break;

	case T_ATOM:
		snprintf(buf, len, "%s", n->s);
		break;

	default:
		return -1;
	}

	return len;
}

// Printer
// ===========================================================================

struct DumpContext {
	FILE *f;
};

static void pa_print_real(struct DumpContext *ctx, Node *n) {
	Node *cur;
	int i;

	if (!n) {
		fprintf(ctx->f, "nil");
		return;
	}

	switch (n->tag) {
	case T_NIL:
		fprintf(ctx->f, "nil");
		return;
	case T_INTERNAL:
		fprintf(ctx->f, "<%p>", n->p);
		return;
	case T_BUILTIN:
		fprintf(ctx->f, "<built-in %p>", n->fn);
		return;
	case T_SPECIAL:
		fprintf(ctx->f, "<special %p>", n->spec);
		return;

	case T_CLOSURE:
		fprintf(ctx->f, "<closure %p>", n);
		return;

	case T_CONS:
		fprintf(ctx->f, "(");
		cur = n;
		while (!IS_NIL(cur)) {
			pa_print_real(ctx, CAR(cur));
			if (!IS_NIL(CDR(cur))) {
				fprintf(ctx->f, " ");
				if (!IS_CONS(CDR(cur))) {
					fprintf(ctx->f, ". ");
					pa_print_real(ctx, CDR(cur));
					break;
				}
			}
			cur = CDR(cur);
		}
		fprintf(ctx->f, ")");
		return;

	case T_ATOM:
		fprintf(ctx->f, "%s", n->s);
		return;

	case T_INTEGER:
		fprintf(ctx->f, "%ld", n->v);
		return;

	case T_CHARACTER:
		if (char_names[n->v] != NULL) {
			fprintf(ctx->f, "#\\%s", char_names[n->v]);
		} else if (n->v > 0x20) {
			fprintf(ctx->f, "#\\%c", (unsigned int)(n->v));
		} else {
			fprintf(ctx->f, "#\\x%02x", (unsigned int)(n->v));
		}
		return;

	case T_STRSLICE:
		fprintf(ctx->f, "\"");
		for (i=n->str.start; i<n->str.start+n->str.len; i++) {
			char c = n->str.s->buf[i];
			if (c >= 0x20) {
				fprintf(ctx->f, "%c", c);
			} else {
				fprintf(ctx->f, "\\x%02x", c);
			}
		}
		fprintf(ctx->f, "\"");
		return;

	case T_EXCEPTION:
		fprintf(ctx->f, "<exc: %.*s>",
		        n->exc.msg->len,
		        n->exc.msg->buf);
		return;
	}
}

void pa_print(Node *n) {
	struct DumpContext ctx;
	ctx.f = stdout;
	pa_print_real(&ctx, n);
	DECREF(n);
}

static char *fmt_char(char *buf, size_t n, char c) {
	if (c >= 0x20 && c < 0x7f) {
		snprintf(buf, n, "%c", c);
		return buf;
	}

	switch (c) {
	case '\n': snprintf(buf, n, "\\n"); break;
	case '\t': snprintf(buf, n, "\\t"); break;
	case '\r': snprintf(buf, n, "\\r"); break;
	default:
		snprintf(buf, n, "\\x%02x", c);
	}

	return buf;
}

// Re-entrant Reader
// ===========================================================================

struct ReReadStack {
	Node *head, *tail;
	ReReadStack *next;

	Token kind;
	int dotcount;
};

void pa_reread_init(ReReadContext *rd) {
	rd->head = NULL;
	rd->tail = NULL;
	rd->eof_signaled = 0;

	rd->unchar = -1;
	rd->tokstr[0] = '\0';
	rd->tokpos = 0;
	rd->lexstate = LEX_TOP;
	rd->untok = TK_INVALID;

	rd->stack = NULL;
}

void pa_reread_put(ReReadContext *rd, char *buf, unsigned len) {
	ReReadBuffer *rb;

	if (rd->eof_signaled) {
		fatal("Tried to put data after eof signaled!");
		return;
	}

	rb = amalloc(sizeof(*rb));
	rb->data = buf;
	rb->len = len;
	rb->pos = 0;
	rb->next = NULL;

	if (rd->tail == NULL) {
		rd->head = rd->tail = rb;
		return;
	}

	rd->tail->next = rb;
	rd->tail = rb;
}

void pa_reread_eof(ReReadContext *rd) {
	rd->eof_signaled = 1;
}

static int re_get_ch(ReReadContext *rd) {
	int ch;

	if (rd->unchar != -1) {
		ch = rd->unchar;
		rd->unchar = -1;
		return ch;
	}

	if (rd->head == NULL)
		return rd->eof_signaled ? EOF : 0;

	ch = rd->head->data[rd->head->pos++];

	if (rd->head->pos >= rd->head->len) {
		ReReadBuffer *rb = rd->head;
		rd->head = rd->head->next;
		if (rd->head == NULL)
			rd->tail = NULL;
		free(rb->data);
		free(rb);
	}

	return ch;
}

static void re_unget_ch(ReReadContext *rd, char ch) {
	if (rd->unchar != -1) {
		char buf1[16], buf2[16];
		fatal("Attempted to unget character '%s' on top of '%s'",
		      fmt_char(buf1, 16, ch),
		      fmt_char(buf2, 16, rd->unchar));
		return;
	}

	rd->unchar = ch;
}

static int decode_char(char *s) {
	if (s[1] == '\0')
		return s[0];

	if (s[0] == 'x')
		return strtol(s+1, 0, 16);

	if (!strcmp(s, "linefeed") || !strcmp(s, "newline"))
		return '\n';
	if (!strcmp(s, "return"))
		return '\r';
	if (!strcmp(s, "tab"))
		return '\t';
	if (!strcmp(s, "space"))
		return ' ';

	return -1;
}

static Token re_get_tok(ReReadContext *rd) {
	int ch;

next_char:
	ch = re_get_ch(rd);
	if (ch == 0)
		return TK_BREAK;

again:
	switch (rd->lexstate) {
	case LEX_TOP:
		if (isspace(ch))
			goto next_char;

		switch (ch) {
		case ';':
			rd->lexstate = LEX_EOL_COMMENT;
			goto next_char;
		case '"':
			rd->lexstate = LEX_STRING;
			goto next_char;
		case EOF:
			return TK_EOF;
		case '(':
			return TK_LPAREN;
		case ')':
			return TK_RPAREN;
		case '\'':
			return TK_QUOTE;
		case '.':
			return TK_DOT;
		}

		rd->lexstate = LEX_GRAB;
		goto again;

	case LEX_EOL_COMMENT:
		if (ch != '\n')
			goto next_char;

		rd->lexstate = LEX_TOP;
		goto next_char;

	case LEX_GRAB:
		if (isspace(ch) || ch == '(' || ch == ')'
		    || ch == ';' || ch == EOF) {
			re_unget_ch(rd, ch);
			rd->tokstr[rd->tokpos] = '\0';
			rd->lexstate = LEX_IDENTIFY;
			goto again;
		}

		rd->tokstr[rd->tokpos++] = ch;
		goto next_char;

	case LEX_IDENTIFY:
		rd->lexstate = LEX_TOP;
		rd->tokpos = 0;

		if (rd->tokstr[0] == '#' && rd->tokstr[1] == '\\') {
			ch = decode_char(rd->tokstr + 2);
			if (ch != -1) {
				rd->tokstr[0] = ch;
				rd->tokstr[1] = '\0';
				return TK_CHARACTER;
			}
		}

		if (isdigit(rd->tokstr[0]) ||
		    (rd->tokstr[0] == '-' && isdigit(rd->tokstr[1])))
			return TK_NUMBER;

		return TK_ATOM;

	case LEX_STRING:
		if (ch == '"') {
			rd->lexstate = LEX_TOP;
			rd->tokstr[rd->tokpos] = '\0';
			rd->tokpos = 0;
			return TK_STRING;
		}

		if (ch == '\\') {
			rd->lexstate = LEX_STRING_ESC;
			goto next_char;
		}

		rd->tokstr[rd->tokpos++] = ch;

		goto next_char;

	case LEX_STRING_ESC:
		switch (ch) {
		case 'r': rd->tokstr[rd->tokpos++] = '\r'; break;
		case 'n': rd->tokstr[rd->tokpos++] = '\n'; break;
		case 't': rd->tokstr[rd->tokpos++] = '\t'; break;
		case 'b': rd->tokstr[rd->tokpos++] = '\b'; break;
		default: rd->tokstr[rd->tokpos++] = ch;
		}
		rd->lexstate = LEX_STRING;

		goto next_char;

	default:
		fatal("lexer in unknown state");
	}

	return TK_INVALID;
}

int pa_reread_empty(ReReadContext *rd) {
	return rd->stack == NULL;
}

int pa_reread_finished(ReReadContext *rd) {
	return pa_reread_empty(rd) && rd->eof_signaled;
}

Node *pa_reread(ReReadContext *rd) {
	Token tk;
	ReReadStack *rs;
	Node *n;

next_tok:
	tk = re_get_tok(rd);
	if (tk == TK_BREAK)
		return NULL;

	switch (tk) {
	default:
		fatal("invalid token");
		return NULL;

	case TK_EOF:
		if (rd->stack != NULL)
			fatal("unexpected EOF");
		return NULL;

	case TK_RPAREN:
		goto ascend;

	case TK_LPAREN:
	case TK_QUOTE:
		// descend
		rs = amalloc(sizeof(*rs));

		rs->kind = tk;
		rs->head = NIL;
		rs->tail = NIL;
		rs->next = rd->stack;
		rs->dotcount = 0;
		rd->stack = rs;

		if (rs->kind == TK_QUOTE) {
			rs->head = CONS(QUOTE, NIL);
			rs->tail = rs->head;
		}

		goto next_tok;

	case TK_NUMBER:
		n = INT(strtol(rd->tokstr, NULL, 0));
		goto have_node;
	case TK_CHARACTER:
		n = CHAR(rd->tokstr[0]);
		goto have_node;
	case TK_ATOM:
		if (!strcmp(rd->tokstr, "nil")) {
			n = NIL;
		} else {
			n = ATOM(rd->tokstr);
		}
		goto have_node;
	case TK_STRING:
		n = NODE(T_STRSLICE);
		n->str.s = pa_str_from_cstr(rd->tokstr);
		n->str.start = 0;
		n->str.len = n->str.s->len;
		goto have_node;

	case TK_DOT:
		if (!rd->stack || rs->head == NIL) {
			fatal("unexpected `.'");
			return NULL;
		}

		rd->stack->kind = TK_DOT;
		goto next_tok;
	}

	fatal("invalid parser status");
	return NULL;

have_node:
	if (rd->stack == NULL)
		return n;

	if (rd->stack->kind == TK_DOT) {
		if (rd->stack->dotcount != 0) {
			fatal("unexpected node");
			return NULL;
		}
		rd->stack->dotcount++;
		rd->stack->tail->cons.cdr = n;
		goto next_tok;
	}

	if (IS_NIL(rd->stack->head)) {
		rd->stack->head = CONS(n, NIL);
		rd->stack->tail = rd->stack->head;
	} else {
		rd->stack->tail->cons.cdr = CONS(n, NIL);
		rd->stack->tail = CDR(rd->stack->tail);
	}

	if (rd->stack->kind == TK_QUOTE)
		goto ascend;

	goto next_tok;

ascend:
	if (rd->stack == NULL) {
		fatal("unbalanced parentheses");
		return NULL;
	}

	rs = rd->stack;
	rd->stack = rs->next;
	n = rs->head;
	free(rs);

	if (rd->stack == NULL)
		return n;

	goto have_node;
}

// Hash Table
// ===========================================================================

struct HashRow {
	char *key; Node *val;
	HashRow *next;
};

#define MOD_ADLER 65521
 
unsigned adler32(char *s) {
	unsigned a = 1, b = 0;

	for (; *s; s++) {
		a = (a + *s) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}

	return (b << 16) | a;
}

HashMap *pa_hash_new(void) {
	HashMap *h = acalloc(1, sizeof(*h));
	h->size = 32;
	h->table = acalloc(h->size, sizeof(*h->table));
	return h;
}

void pa_hash_delete(HashMap *h) {
	int i;
	HashRow *r, *next;

	for (i=0; i<h->size; i++) {
		for (r=h->table[i]; r; r=next) {
			free(r->key);
			DECREF(r->val);
			next = r->next;
			free(r);
		}
	}

	free(h->table);
	free(h);
}

static HashRow *hash_get_row(HashMap *h, char *key) {
	unsigned hash = adler32(key);
	HashRow *r;

	r = h->table[hash % h->size];

	for (; r; r=r->next) {
		if (!strcmp(r->key, key))
			return r;
	}

	return NULL;
}

void pa_hash_put(HashMap *h, char *key, Node *val) {
	unsigned hash;
	HashRow *r;

	if ((r = hash_get_row(h, key)) != NULL) {
		DECREF(r->val);
		r->val = val;
		return;
	}

	hash = adler32(key);

	r = acalloc(1, sizeof(HashRow));
	r->key = strdup(key);
	r->val = val;

	hash %= h->size;
	r->next = h->table[hash];
	h->table[hash] = r;
}

Node *pa_hash_get(HashMap *h, char *key) {
	HashRow *r = hash_get_row(h, key);
	return r ? INCREF(r->val) : NULL;
}

// Evaluation
// ===========================================================================

static EvalScope NO_TCO;

static int pa_eq(Node *A, Node *B);
static Node *eval_with_scope(EvalContext*, EvalScope*, Node*);

static Node *builtin_eval(EvalContext *ctx, Node *args) {
	return pa_eval(ctx, XCAR(args));
}

static Node *builtin_print(EvalContext *ctx, Node *args) {
	for (; IS_CONS(args); args=XCDR(args)) {
		pa_print(INCREF(CAR(args)));

		if (IS_CONS(CDR(args))) {
			printf(" ");
		} else {
			printf("\n");
		}
	}
	return NIL;
}

static Node *builtin_message(EvalContext *ctx, Node *args) {
	Node *fmt = INCREF(CAR(args));
	char *s;
	int i, n;
	args = XCDR(args);

	if (fmt->tag != T_STRSLICE)
		goto exit;

	s = fmt->str.s->buf + fmt->str.start;
	n = fmt->str.len;

	for (i=0; i<n; i++) {
		switch (s[i]) {
		case '{':
			if (i+1<n && s[i+1] == '}') {
				i++;
				pa_print(INCREF(CAR(args)));
				args = XCDR(args);
			} else if (i+1<n && s[i+1] == '{') {
				i++;
				putchar('{');
			} else {
				putchar('{');
			}
			break;

		case '}':
			if (i+1<n && s[i+1] == '}') {
				putchar('}');
				i++;
			} else {
				putchar('}');
			}
			break;

		default:
			putchar(s[i]);
			break;
		}
	}

	putchar('\n');

exit:
	DECREF(fmt);
	DECREF(args);
	return NIL;
}

static Node *builtin_set(EvalContext *ctx, Node *args) {
	Node *val = CADR(args);
	char buf[512];
	pa_str_val(buf, 512, CAR(args));
	if (strchr(buf, ':')) {
		DECREF(args);
		return pa_exc("cannot assign to a module symbol");
	}
	pa_hash_put(ctx->global, buf, INCREF(val));
	DECREF(args);
	return INCREF(val);
}

static Node *builtin_cons(EvalContext *ctx, Node *args) {
	Node *res = CONS(INCREF(CAR(args)), INCREF(CADR(args)));
	DECREF(args);
	return res;
}

static Node *builtin_car(EvalContext *ctx, Node *args) {
	Node *res = INCREF(CAAR(args));
	DECREF(args);
	return res;
}

static Node *builtin_cdr(EvalContext *ctx, Node *args) {
	Node *res = INCREF(CDAR(args));
	DECREF(args);
	return res;
}

static Node *builtin_list(EvalContext *ctx, Node *args) {
	return args;
}

static Node *builtin_len(EvalContext *ctx, Node *args) {
	int n = 0;
	for (; IS_CONS(args); args=XCDR(args))
		n++;
	return INT(n);
}

static Node *builtin_concat(EvalContext *ctx, Node *args) {
	Node *n, *head, *tail;

	head = tail = NIL;

	for (; IS_CONS(args); args=XCDR(args)) {
		for (n=CAR(args); IS_CONS(n); n=CDR(n)) {
			if (IS_NIL(head)) {
				head = CONS(INCREF(CAR(n)), NIL);
				tail = head;
			} else {
				tail->cons.cdr = CONS(INCREF(CAR(n)), NIL);
				tail = CDR(tail);
			}
		}
	}

	return head;
}

static Node *builtin_map(EvalContext *ctx, Node *args) {
	Node *fn = CAR(args);
	Node *over = CADR(args);
	Node *head, *tail, *n;

	head = tail = NIL;

	if (over->tag == T_STRSLICE) {
		char c, *buf = alloca(over->str.len);
		int i;

		for (i=0; i<over->str.len; i++) {
			c = over->str.s->buf[i + over->str.start];
			n = pa_apply(ctx, INCREF(fn), LIST1(CHAR(c)), NULL);
			if (n->tag == T_EXCEPTION) {
				DECREF(args);
				return n;
			}
			buf[i] = 0xff & (unsigned)(n->v);
			DECREF(n);
		}

		n = NODE(T_STRSLICE);
		n->str.s = pa_str_from_buf(buf, over->str.len);
		n->str.start = 0;
		n->str.len = n->str.s->len;

		DECREF(args);
		return n;
	}

	if (!IS_CONS(over) && !IS_NIL(over)) {
		n = pa_exc("cannot map over a %s", pa_tag_names[over->tag]);
		DECREF(args);
		return n;
	}

	for (; IS_CONS(over); over=CDR(over)) {
		n = pa_apply(ctx, INCREF(fn), LIST1(INCREF(CAR(over))), NULL);

		if (n->tag == T_EXCEPTION) {
			DECREF(head);
			DECREF(args);
			return n;
		}

		if (IS_NIL(head)) {
			head = CONS(n, NIL);
			tail = head;
		} else {
			tail->cons.cdr = CONS(n, NIL);
			tail = CDR(tail);
		}
	}

	DECREF(args);
	return head;
}

static Node *builtin_filter(EvalContext *ctx, Node *args) {
	Node *fn = CAR(args);
	Node *over = CADR(args);
	Node *head, *tail, *n;

	head = tail = NIL;

	if (over->tag == T_STRSLICE) {
		char c, *buf = alloca(over->str.len);
		int i, j;

		for (i=0, j=0; i<over->str.len; i++) {
			c = over->str.s->buf[i + over->str.start];
			n = pa_apply(ctx, INCREF(fn), LIST1(CHAR(c)), NULL);
			if (n->tag == T_EXCEPTION) {
				DECREF(args);
				return n;
			}
			if (IS_NIL(n))
				continue;
			buf[j++] = c;
			DECREF(n);
		}

		n = STR(pa_str_from_buf(buf, j));

		DECREF(args);
		return n;
	}

	if (!IS_CONS(over) && !IS_NIL(over)) {
		n = pa_exc("cannot filter a %s", pa_tag_names[over->tag]);
		DECREF(args);
		return n;
	}

	for (; IS_CONS(over); over=CDR(over)) {
		n = pa_apply(ctx, INCREF(fn), LIST1(INCREF(CAR(over))), NULL);

		if (n->tag == T_EXCEPTION) {
			DECREF(args);
			DECREF(head);
			return n;
		}

		if (IS_NIL(n))
			continue;

		DECREF(n);
		n = INCREF(CAR(over));

		if (IS_NIL(head)) {
			head = CONS(n, NIL);
			tail = head;
		} else {
			tail->cons.cdr = CONS(n, NIL);
			tail = CDR(tail);
		}
	}

	DECREF(args);

	return head;
}

static Node *builtin_reduce(EvalContext *ctx, Node *args) {
	Node *fn = CAR(args);
	Node *over = CADR(args);
	Node *val = INCREF(CADDR(args));

	if (IS_NIL(val)) {
		val = pa_apply(ctx, INCREF(fn), NIL, NULL);
		if (val->tag == T_EXCEPTION) {
			DECREF(args);
			return val;
		}
	}

	if (!IS_CONS(over) && !IS_NIL(over)) {
		DECREF(val);
		val = pa_exc("cannot reduce a %s", pa_tag_names[over->tag]);
		DECREF(args);
		return val;
	}

	for (; IS_CONS(over); over=CDR(over)) {
		val = pa_apply(ctx, INCREF(fn), LIST2(val, INCREF(CAR(over))),
		               NULL);
		if (val->tag == T_EXCEPTION) {
			DECREF(args);
			return val;
		}
	}

	DECREF(args);

	return val;
}

static Node *builtin_apply(EvalContext *ctx, Node *args) {
	Node *fn = CAR(args);
	Node *over = CADR(args);

	Node *res = pa_apply(ctx, INCREF(fn), INCREF(over), NULL);
	DECREF(args);
	return res;
}

static Node *builtin_gen(EvalContext *ctx, Node *args) {
	Node *fn = CAR(args);
	Node *over = CADR(args);
	Node *n;
	ListBuilder res;

	pa_lb_init(&res);

	for (;;) {
		n = pa_apply(ctx, INCREF(fn), INCREF(over), NULL);
		if (IS_NIL(n))
			break;
		if (n->tag == T_EXCEPTION) {
			DECREF(args);
			DECREF(pa_lb_finish(&res));
			return n;
		}
		pa_lb_append(&res, n);
	}

	DECREF(args);
	return pa_lb_finish(&res);
}

static Node *index_over_list(EvalContext *ctx, Node *args) {
	Node *fn = CAR(args);
	Node *over = CADR(args);
	Node *cur, *res, *n;
	int i;

	res = NIL;
	for (i=0, cur=over; IS_CONS(cur); ++i, cur=CDR(cur)) {
		if (fn->tag == T_BUILTIN || fn->tag == T_CLOSURE) {
			n = pa_apply(ctx, INCREF(fn),
			             LIST1(INCREF(CAR(cur))), NULL);
			if (n->tag == T_EXCEPTION) {
				DECREF(args);
				return n;
			}
			if (!IS_NIL(n)) {
				res = INT(i);
				DECREF(n);
				break;
			}
		} else {
			if (pa_eq(fn, CAR(cur))) {
				res = INT(i);
				break;
			}
		}
	}

	DECREF(args);
	return res;
}

static Node *index_over_str(EvalContext *ctx, Node *args) {
	Node *fn = CAR(args);
	Node *over = CADR(args);
	Node *res, *n;
	int i;
	char *s = over->str.s->buf + over->str.start;

	res = NIL;
	for (i=0; i < over->str.len; i++) {
		if (fn->tag == T_BUILTIN || fn->tag == T_CLOSURE) {
			n = pa_apply(ctx, INCREF(fn),
			             LIST1(INCREF(CHAR(s[i]))), NULL);
			if (n->tag == T_EXCEPTION) {
				DECREF(args);
				return n;
			}
			if (!IS_NIL(n)) {
				res = INT(i);
				DECREF(n);
				break;
			}
		} else {
			n = CHAR(s[i]);
			if (pa_eq(fn, n)) {
				res = INT(i);
				DECREF(n);
				break;
			}
			DECREF(n);
		}
	}

	DECREF(args);
	return res;
}

static Node *builtin_index(EvalContext *ctx, Node *args) {
	Node *n;

	if (IS_CONS(CADR(args))) {
		return index_over_list(ctx, args);
	} else if (CADR(args)->tag == T_STRSLICE) {
		return index_over_str(ctx, args);
	}

	n = pa_exc("cannot index a %s", pa_tag_names[CADR(args)->tag]);
	DECREF(args);
	return n;
}

static Node *b_caar(EvalContext *ctx, Node *args){return XCAAR(XCAR(args));}
static Node *b_cadr(EvalContext *ctx, Node *args){return XCADR(XCAR(args));}
static Node *b_cdar(EvalContext *ctx, Node *args){return XCDAR(XCAR(args));}
static Node *b_cddr(EvalContext *ctx, Node *args){return XCDDR(XCAR(args));}

static Node *b_caaar(EvalContext *ctx, Node *args){return XCAAAR(XCAR(args));}
static Node *b_caadr(EvalContext *ctx, Node *args){return XCAADR(XCAR(args));}
static Node *b_cadar(EvalContext *ctx, Node *args){return XCADAR(XCAR(args));}
static Node *b_caddr(EvalContext *ctx, Node *args){return XCADDR(XCAR(args));}
static Node *b_cdaar(EvalContext *ctx, Node *args){return XCDAAR(XCAR(args));}
static Node *b_cdadr(EvalContext *ctx, Node *args){return XCDADR(XCAR(args));}
static Node *b_cddar(EvalContext *ctx, Node *args){return XCDDAR(XCAR(args));}
static Node *b_cdddr(EvalContext *ctx, Node *args){return XCDDDR(XCAR(args));}

static Node *b_caaaar(EvalContext *ctx, Node *args){return XCAAAAR(XCAR(args));}
static Node *b_caaadr(EvalContext *ctx, Node *args){return XCAAADR(XCAR(args));}
static Node *b_caadar(EvalContext *ctx, Node *args){return XCAADAR(XCAR(args));}
static Node *b_caaddr(EvalContext *ctx, Node *args){return XCAADDR(XCAR(args));}
static Node *b_cadaar(EvalContext *ctx, Node *args){return XCADAAR(XCAR(args));}
static Node *b_cadadr(EvalContext *ctx, Node *args){return XCADADR(XCAR(args));}
static Node *b_caddar(EvalContext *ctx, Node *args){return XCADDAR(XCAR(args));}
static Node *b_cadddr(EvalContext *ctx, Node *args){return XCADDDR(XCAR(args));}
static Node *b_cdaaar(EvalContext *ctx, Node *args){return XCDAAAR(XCAR(args));}
static Node *b_cdaadr(EvalContext *ctx, Node *args){return XCDAADR(XCAR(args));}
static Node *b_cdadar(EvalContext *ctx, Node *args){return XCDADAR(XCAR(args));}
static Node *b_cdaddr(EvalContext *ctx, Node *args){return XCDADDR(XCAR(args));}
static Node *b_cddaar(EvalContext *ctx, Node *args){return XCDDAAR(XCAR(args));}
static Node *b_cddadr(EvalContext *ctx, Node *args){return XCDDADR(XCAR(args));}
static Node *b_cdddar(EvalContext *ctx, Node *args){return XCDDDAR(XCAR(args));}
static Node *b_cddddr(EvalContext *ctx, Node *args){return XCDDDDR(XCAR(args));}

static Node *builtin_nilp(EvalContext *ctx, Node *args) {
	Node *res = COND(IS_NIL(CAR(args)));
	DECREF(args);
	return res;
}

static Node *builtin_listp(EvalContext *ctx, Node *args) {
	Node *res = COND(IS_CONS(CAR(args)));
	DECREF(args);
	return res;
}

static int pa_eq(Node *A, Node *B) {
	if (A->tag != B->tag)
		return 0;

	switch (A->tag) {
	case T_NIL: return 1;
	case T_INTERNAL:
		return A->raw.p == B->raw.p;
	case T_BUILTIN:
		return A->fn == B->fn;
	case T_SPECIAL:
		return A->spec == B->spec;
	case T_CLOSURE:
		return 0;

	case T_ATOM:
		return !strcmp(A->s, B->s);
	case T_INTEGER:
	case T_CHARACTER:
		return A->v == B->v;

	case T_STRSLICE:
		return A->str.len == B->str.len &&
		       !memcmp(A->str.s->buf + A->str.start,
		               B->str.s->buf + B->str.start,
		               A->str.len);

	case T_CONS:
		return pa_eq(CAR(A), CAR(B)) && pa_eq(CDR(A), CDR(B));

	case T_EXCEPTION:
		return 0;
	}

	fatal("unknown tag type");
	return 0;
}

static Node *builtin_eq(EvalContext *ctx, Node *args) {
	Node *res = COND(pa_eq(CAR(args), CADR(args)));
	DECREF(args);
	return res;
}

static Node *builtin_or(EvalContext *ctx, Node *args) {
	int or = 0;
	for (; !IS_NIL(args); args=XCDR(args))
		or = or || !IS_NIL(CAR(args));
	return COND(or);
}

static Node *builtin_and(EvalContext *ctx, Node *args) {
	int and = 1;
	for (; !IS_NIL(args); args=XCDR(args))
		and = and && !IS_NIL(CAR(args));
	return COND(and);
}

static Node *builtin_add(EvalContext *ctx, Node *args) {
	int sum = 0;
	for (; !IS_NIL(args); args=XCDR(args))
		sum += CAR(args)->v;
	return INT(sum);
}

static Node *builtin_sub(EvalContext *ctx, Node *args) {
	int sum = 0;
	if (IS_NIL(args)) // empty
		return INT(0);
	sum = CAR(args)->v;
	args = XCDR(args);
	if (IS_NIL(args)) // unary
		return INT(-sum);
	for (; !IS_NIL(args); args=XCDR(args)) // n-ary
		sum -= CAR(args)->v;
	return INT(sum);
}

static Node *builtin_mul(EvalContext *ctx, Node *args) {
	int prod = 1;
	for (; !IS_NIL(args); args=XCDR(args))
		prod *= CAR(args)->v;
	return INT(prod);
}

static Node *builtin_lt(EvalContext *ctx, Node *args) {
	Node *res = COND(CAR(args)->v < CADR(args)->v);
	DECREF(args);
	return res;
}

static Node *builtin_gt(EvalContext *ctx, Node *args) {
	Node *res = COND(CAR(args)->v > CADR(args)->v);
	DECREF(args);
	return res;
}

static Node *builtin_le(EvalContext *ctx, Node *args) {
	Node *res = COND(CAR(args)->v <= CADR(args)->v);
	DECREF(args);
	return res;
}

static Node *builtin_ge(EvalContext *ctx, Node *args) {
	Node *res = COND(CAR(args)->v >= CADR(args)->v);
	DECREF(args);
	return res;
}

static Node *builtin_hash_new(EvalContext *ctx, Node *args) {
	DECREF(args);
	return INTERNAL(pa_hash_new, pa_hash_new(), (InternalDtor*)pa_hash_delete);
}

static Node *builtin_hash_put(EvalContext *ctx, Node *args) {
	Node *n, *tab = CAR(args);
	char buf[512];
	if (tab->tag != T_INTERNAL || tab->raw.kind != pa_hash_new) {
		n = pa_exc("%s is not a hash table", pa_tag_names[tab->tag]);
		DECREF(args);
		return n;
	}
	pa_str_val(buf, 512, CADR(args));
	pa_hash_put(tab->raw.p, buf, INCREF(CADDR(args)));
	Node *res = INCREF(CADDR(args));
	DECREF(args);
	return res;
}

static Node *builtin_hash_get(EvalContext *ctx, Node *args) {
	Node *n, *tab = CAR(args);
	char buf[512];
	if (tab->tag != T_INTERNAL || tab->raw.kind != pa_hash_new) {
		n = pa_exc("%s is not a hash table", pa_tag_names[tab->tag]);
		DECREF(args);
		return n;
	}
	pa_str_val(buf, 512, CADR(args));
	n = pa_hash_get(tab->raw.p, buf);
	DECREF(args);
	return n ? n : NIL;
}

static Node *builtin_head(EvalContext *ctx, Node *args) {
	Node *res, *str = XCAR(args);
	if (str->tag != T_STRSLICE) {
		DECREF(str);
		return pa_exc("head: arg must be str");
	}
	res = CHAR(str->str.s->buf[str->str.start]);
	DECREF(str);
	return res;
}

static Node *builtin_tail(EvalContext *ctx, Node *args) {
	Node *res, *str = XCAR(args);
	if (str->tag != T_STRSLICE) {
		DECREF(str);
		return pa_exc("tail: arg must be str");
	}
	if (str->str.len == 0) {
		DECREF(str);
		return STR(pa_str_from_cstr(""));
	}
	res = STR(pa_str_clone(str->str.s));
	res->str.start = str->str.start + 1;
	res->str.len = str->str.len - 1;
	DECREF(str);
	return res;
}

static Node *builtin_strlen(EvalContext *ctx, Node *args) {
	Node *res, *str = XCAR(args);
	if (str->tag != T_STRSLICE) {
		DECREF(str);
		return pa_exc("strlen: arg must be str");
	}
	res = INT(str->str.len);
	DECREF(str);
	return res;
}

static Node *builtin_strcat(EvalContext *ctx, Node *args) {
	Node *cur;
	char *buf;
	int sz, i;

	sz = 0;
	for (cur=args; IS_CONS(cur); cur=CDR(cur)) {
		if (CAR(cur)->tag != T_STRSLICE) {
			DECREF(args);
			return pa_exc("strcat: args must be strs");
		}
		sz += CAR(cur)->str.len;
	}

	buf = alloca(sz);
	i = 0;
	for (cur=args; IS_CONS(cur); cur=CDR(cur)) {
		memcpy(buf+i, CAR(cur)->str.s->buf + CAR(cur)->str.start,
		       CAR(cur)->str.len);
		i += CAR(cur)->str.len;
	}

	DECREF(args);
	return STR(pa_str_from_buf(buf, sz));
}

static Node *builtin_substr(EvalContext *ctx, Node *args) {
	Node *s = CAR(args);
	Node *start = CADR(args);
	Node *end = CADDR(args);
	Node *res;
	int realstart;

	if (s->tag != T_STRSLICE || start->tag != T_INTEGER
	    || (!IS_NIL(end) && end->tag != T_INTEGER)) {
		DECREF(args);
		return pa_exc("substr: invalid arguments");
	}

	res = STR(pa_str_clone(s->str.s));

	realstart = start->v;
	if (start->v > s->str.len)
		realstart = s->str.len;

	res->str.start = s->str.start + realstart;

	if (IS_NIL(end)) {
		res->str.len = s->str.len - realstart;
	} else if (end->v > s->str.len || end->v < realstart) {
		res->str.len = 0;
	} else {
		res->str.len = end->v - realstart;
	}

	DECREF(args);
	return res;
}

static Node *builtin_split(EvalContext *ctx, Node *args) {
	Node *n, *str = CAR(args);
	char *s;
	int ch, i, len, start, count, limit;
	struct ListBuilder res;

	if (IS_NIL(CADR(args)) || CADR(args)->tag != T_CHARACTER) {
		ch = -1;
	} else {
		ch = CADR(args)->v;
	}

	if (IS_NIL(CADDR(args)) || CADDR(args)->tag != T_INTEGER) {
		limit = -1;
	} else {
		limit = CADDR(args)->v;
	}

	if (str->tag != T_STRSLICE) {
		n = pa_exc("split: cannot split %s", pa_tag_names[str->tag]);
		DECREF(args);
		return n;
	}

	pa_lb_init(&res);

	s = str->str.s->buf + str->str.start;
	len = str->str.len;

	if (ch == -1) {
		for (count=0,i=0;;) {
			while (i<len && isspace(s[i]))
				i++;
			if (i>=len)
				break;

			n = STR(pa_str_clone(str->str.s));
			start = i;
			n->str.start = str->str.start + start;
			count++;
			if (limit>0 && count>limit) {
				n->str.len = str->str.len - i;
				pa_lb_append(&res, n);
				break;
			}
			while (s[i] && !isspace(s[i]))
				i++;
			n->str.len = i - start;
			pa_lb_append(&res, n);
			if (i>=len)
				break;
		}
	} else {
		for (count=0,i=0;;) {
			n = STR(pa_str_clone(str->str.s));
			start = i;
			n->str.start = str->str.start + start;
			count++;
			if (limit>0 && count>limit) {
				n->str.len = str->str.len - i;
				pa_lb_append(&res, n);
				break;
			}
			while (i<len && s[i] != ch)
				i++;
			n->str.len = i - start;
			pa_lb_append(&res, n);
			if (i>=len)
				break;
			i++;
		}
	}

	DECREF(args);
	return pa_lb_finish(&res);
}

HashMap *pa_builtins = NULL;

static struct built_in_function {
	char *name;
	Node *(*fn)(EvalContext*, Node*);
} built_in_functions[] = {
	{ "eval",       builtin_eval },

	{ "print",      builtin_print },
	{ "message",    builtin_message },

	{ "set",        builtin_set },

	{ "cons",       builtin_cons },
	{ "car",        builtin_car },
	{ "cdr",        builtin_cdr },

	{ "list",       builtin_list },
	{ "len",        builtin_len },
	{ "concat",     builtin_concat },

	{ "map",        builtin_map },
	{ "filter",     builtin_filter },
	{ "reduce",     builtin_reduce },
	{ "apply",      builtin_apply },
	{ "gen",        builtin_gen },
	{ "index",      builtin_index },

	{ "caar",       b_caar },
	{ "cadr",       b_cadr },
	{ "cdar",       b_cdar },
	{ "cddr",       b_cddr },

	{ "caaar",      b_caaar },
	{ "caadr",      b_caadr },
	{ "cadar",      b_cadar },
	{ "caddr",      b_caddr },
	{ "cdaar",      b_cdaar },
	{ "cdadr",      b_cdadr },
	{ "cddar",      b_cddar },
	{ "cdddr",      b_cdddr },

	{ "caaaar",     b_caaaar },
	{ "caaadr",     b_caaadr },
	{ "caadar",     b_caadar },
	{ "caaddr",     b_caaddr },
	{ "cadaar",     b_cadaar },
	{ "cadadr",     b_cadadr },
	{ "caddar",     b_caddar },
	{ "cadddr",     b_cadddr },
	{ "cdaaar",     b_cdaaar },
	{ "cdaadr",     b_cdaadr },
	{ "cdadar",     b_cdadar },
	{ "cdaddr",     b_cdaddr },
	{ "cddaar",     b_cddaar },
	{ "cddadr",     b_cddadr },
	{ "cdddar",     b_cdddar },
	{ "cddddr",     b_cddddr },

	{ "nilp",       builtin_nilp },
	{ "listp",      builtin_listp },
	{ "eq",         builtin_eq },
	{ "not",        builtin_nilp },
	{ "or",         builtin_or },
	{ "and",        builtin_and },

	{ "+",          builtin_add },
	{ "-",          builtin_sub },
	{ "*",          builtin_mul },
	{ "=",          builtin_eq },
	{ "<",          builtin_lt },
	{ ">",          builtin_gt },
	{ "<=",         builtin_le },
	{ ">=",         builtin_ge },

	{ "hash-new",   builtin_hash_new },
	{ "hash-put",   builtin_hash_put },
	{ "hash-get",   builtin_hash_get },

	{ "head",       builtin_head },
	{ "tail",       builtin_tail },
	{ "strlen",     builtin_strlen },
	{ "strcat",     builtin_strcat },
	{ "substr",     builtin_substr },

	{ "split",      builtin_split },

	{ 0 }
};

static Node *spec_quote(EvalContext *ctx, EvalScope *sc, Node *args,
                        EvalScope **tco) {
	Node *res;

	if (!IS_CONS(args)) {
		DECREF(args);
		return NIL;
	}

	res = INCREF(CAR(args));
	DECREF(args);

	return res;
}

static Node *spec_let(EvalContext *ctx, EvalScope *sc, Node *args,
                      EvalScope **tco) {
	EvalScope *scnext;
	Node *n, *cur;
	char buf[512];

	scnext = scope_new(sc);
	for (cur=CAR(args); IS_CONS(cur); cur=CDR(cur)) {
		pa_str_val(buf, 512, CAAR(cur));
		n = eval_with_scope(ctx, SC_CLONE(sc),
		                    INCREF(CADAR(cur)));
		if (n->tag == T_EXCEPTION) {
			SC_FREE(scnext);
			DECREF(args);
			return n;
		}
		pa_hash_put(scnext->data, buf, n);
	}
	sc = NULL;

	for (n=NIL, cur=CDR(args); IS_CONS(cur); cur=CDR(cur)) {
		if (tco && IS_NIL(CDR(cur))) {
			*tco = scnext;
			n = INCREF(CAR(cur));
			DECREF(args);
			return n;
		}
		n = eval_with_scope(ctx, SC_CLONE(scnext), INCREF(CAR(cur)));
		if (n->tag == T_EXCEPTION)
			break;
	}

	SC_FREE(scnext);
	DECREF(args);

	return n;
}

static Node *spec_let2(EvalContext *ctx, EvalScope *sc, Node *args,
                      EvalScope **tco) {
	EvalScope *scnext;
	Node *n, *cur;
	char buf[512];

	scnext = scope_new(sc);
	for (cur=CAR(args); IS_CONS(cur); cur=CDR(cur)) {
		pa_str_val(buf, 512, CAAR(cur));
		n = eval_with_scope(ctx, SC_CLONE(scnext),
		                    INCREF(CADAR(cur)));
		if (n->tag == T_EXCEPTION) {
			SC_FREE(scnext);
			DECREF(args);
			return n;
		}
		pa_hash_put(scnext->data, buf, n);
	}
	sc = NULL;

	for (n=NIL, cur=CDR(args); IS_CONS(cur); cur=CDR(cur)) {
		if (tco && IS_NIL(CDR(cur))) {
			*tco = scnext;
			n = INCREF(CAR(cur));
			DECREF(args);
			return n;
		}
		n = eval_with_scope(ctx, SC_CLONE(scnext), INCREF(CAR(cur)));
		if (n->tag == T_EXCEPTION)
			break;
	}

	SC_FREE(scnext);
	DECREF(args);

	return n;
}

static Node *spec_if(EvalContext *ctx, EvalScope *sc, Node *args,
                     EvalScope **tco) {
	Node *cond = eval_with_scope(ctx, SC_CLONE(sc), INCREF(CAR(args)));
	Node *res;

	if (cond->tag == T_EXCEPTION) {
		DECREF(args);
		return cond;
	}

	res = INCREF(IS_NIL(cond) ? CADDR(args) : CADR(args));

	DECREF(cond);
	cond = NULL;

	if (!tco) {
		res = eval_with_scope(ctx, sc, res);
	} else {
		*tco = sc;
	}

	DECREF(args);
	return res;
}

static Node *spec_setq(EvalContext *ctx, EvalScope *sc, Node *args,
                       EvalScope **tco) {
	Node *val = eval_with_scope(ctx, sc, INCREF(CADR(args)));
	Node *res;

	if (val->tag == T_EXCEPTION) {
		res = val;
	} else {
		res = builtin_set(ctx, LIST2(INCREF(CAR(args)), val));
	}

	DECREF(args);
	return res;
}

static Node *spec_lambda(EvalContext *ctx, EvalScope *sc, Node *args,
                         EvalScope **tco) {
	Node *cl = NODE(T_CLOSURE);
	cl->cl.args = INCREF(CAR(args));
	cl->cl.body = INCREF(CDR(args));
	cl->cl.sc = sc;
	DECREF(args);
	return cl;
}

static Node *spec_defun(EvalContext *ctx, EvalScope *sc, Node *args,
                        EvalScope **tco) {
	Node *cl = spec_lambda(ctx, sc, CDR(args), NULL);
	char buf[512];
	pa_str_val(buf, 512, CAR(args));
	pa_hash_put(ctx->global, buf, INCREF(cl));
	return cl;
}

static Node *spec_progn(EvalContext *ctx, EvalScope *sc, Node *args,
                        EvalScope **tco) {
	Node *res = NIL;
	for (; IS_CONS(args); args=XCDR(args)) {
		DECREF(res);
		if (tco && IS_NIL(CDR(args))) {
			*tco = sc;
			res = INCREF(CAR(args));
			DECREF(args);
			return res;
		}
		res = eval_with_scope(ctx, SC_CLONE(sc), INCREF(CAR(args)));
		if (res->tag == T_EXCEPTION)
			break;
	}
	DECREF(args);
	return res;
}

static Node *spec_try(EvalContext *ctx, EvalScope *sc, Node *args,
                      EvalScope **tco) {
	Node *res = eval_with_scope(ctx, SC_CLONE(sc), INCREF(CAR(args)));
	if (res->tag != T_EXCEPTION) {
		SC_FREE(sc);
		DECREF(args);
		return res;
	}
	args = XCADR(args);
	DECREF(res);
	if (tco) {
		*tco = sc;
		return args;
	}
	return eval_with_scope(ctx, sc, args);
}

static struct special_form {
	char *name;
	Node *(*fn)(EvalContext*, EvalScope*, Node*, EvalScope **tco);
} special_forms[] = {
	{ "quote",      spec_quote },
	{ "let",        spec_let },
	{ "let*",       spec_let2 },
	{ "if",         spec_if },
	{ "setq",       spec_setq },
	{ "lambda",     spec_lambda },
	{ "defun",      spec_defun },
	{ "progn",      spec_progn },
	{ "try",        spec_try },

	{ 0 }
};

void pa_eval_init(EvalContext *ctx) {
	if (pa_builtins == NULL) {
		struct built_in_function *fn;
		struct special_form *spec;

		pa_builtins = pa_hash_new();

		for (fn=built_in_functions; fn->name; fn++)
			pa_hash_put(pa_builtins, fn->name, BUILTIN(fn->fn));
		for (spec=special_forms; spec->name; spec++)
			pa_hash_put(pa_builtins, spec->name, SPECIAL(spec->fn));

		pa_hash_put(pa_builtins, "nil", NIL);
		pa_hash_put(pa_builtins, "t", T);
	}

	ctx->global = pa_hash_new();
	ctx->modules = pa_hash_new();
}

void pa_add_module(EvalContext *ctx, EvalContext *mod, char *name) {
	pa_hash_put(ctx->modules, name,
	            INTERNAL(pa_add_module, mod->global, NULL));
}

static Node *pa_module_lookup(EvalContext *ctx, char *s) {
	char *p, buf[65536];
	Node *tab, *n;

	snprintf(buf, 65536, "%s", s);
	p = strchr(buf, ':');
	if (p != NULL)
		*p++ = '\0';

	if ((tab = pa_hash_get(ctx->modules, buf)) == NULL)
		return pa_exc("no module: '%s'", buf);

	if (tab->tag != T_INTERNAL || tab->raw.kind != pa_add_module)
		return pa_exc("'%s' is not a module", buf);

	if ((n = pa_hash_get(tab->raw.p, p)) == NULL)
		return pa_exc("'%s': lookup failure", p);

	return n;
}

Node *pa_lookup(EvalContext *ctx, EvalScope *sc, char *s) {
	Node *n;

	if (strchr(s, ':') != NULL)
		return pa_module_lookup(ctx, s);

	for (; sc; sc=sc->up) {
		if ((n = pa_hash_get(sc->data, s)) != NULL)
			return n;
	}

	if ((n = pa_hash_get(ctx->global, s)) != NULL)
		return n;

	if ((n = pa_hash_get(pa_builtins, s)) != NULL)
		return n;

	return pa_exc("'%s': lookup failure", s);
}

Node *pa_apply(EvalContext *ctx, Node *fn, Node *args, EvalScope **tco) {
	EvalScope *sc;
	Node *res, *n, *cur;
	char buf[512];

	switch (fn->tag) {
	case T_BUILTIN:
		res = fn->fn(ctx, args);
		DECREF(fn);
		return res;

	case T_CLOSURE:
		sc = scope_new(SC_CLONE(fn->cl.sc));

		for (cur=fn->cl.args; IS_CONS(cur); cur=CDR(cur), args=XCDR(args)) {
			pa_str_val(buf, 512, CAR(cur));
			pa_hash_put(sc->data, buf,
			            INCREF(CAR(args)));
		}
		DECREF(args);

		for (n=NIL, cur=fn->cl.body; IS_CONS(cur); cur=CDR(cur)) {
			DECREF(n);

			if (tco && !IS_CONS(CDR(cur))) {
				*tco = sc;
				n = INCREF(CAR(cur));
				DECREF(fn);
				return n;
			}

			n = eval_with_scope(ctx, SC_CLONE(sc), INCREF(CAR(cur)));
			if (n->tag == T_EXCEPTION)
				break;
		}

		SC_FREE(sc);
		DECREF(fn);

		return n;

	default:
		pa_print(fn);
		printf(": ");
		fflush(stdout);
		fatal("not callable", fn->tag);
		return NIL;
	}
}

static Node *eval_with_scope(EvalContext *ctx, EvalScope *sc, Node *n) {
	Node *res, *fn, *cur, *args, *tail;
	EvalScope *tco;

again:
	switch (n->tag) {
	case T_CONS:
		if (IS_NIL(n)) {
			SC_FREE(sc);
			return NIL;
		}

		fn = eval_with_scope(ctx, SC_CLONE(sc), INCREF(CAR(n)));

		if (fn->tag == T_EXCEPTION) {
			SC_FREE(sc);
			return fn;
		}

		if (fn->tag == T_SPECIAL) {
			tco = &NO_TCO;
			res = fn->spec(ctx, SC_CLONE(sc), XCDR(n), &tco);
			DECREF(fn);
			goto try_tco;
		}

		args = NIL;
		tail = NIL;

		for (cur=XCDR(n); IS_CONS(cur); cur=XCDR(cur)) {
			res = eval_with_scope(ctx, SC_CLONE(sc),
			                      INCREF(CAR(cur)));

			if (res->tag == T_EXCEPTION) {
				SC_FREE(sc);
				DECREF(fn);
				DECREF(args);
				return res;
			}

			if (IS_NIL(args)) {
				args = CONS(res, NIL);
				tail = args;
			} else {
				tail->cons.cdr = CONS(res, NIL);
				tail = CDR(tail);
			}
		}

		tco = &NO_TCO;
		res = pa_apply(ctx, fn, args, &tco);
		goto try_tco;

	case T_ATOM:
		res = pa_lookup(ctx, sc, n->s);
		SC_FREE(sc);
		DECREF(n);
		return res;

	case T_NIL:
	case T_INTERNAL:
	case T_BUILTIN:
	case T_INTEGER:
	case T_SPECIAL:
	case T_CLOSURE:
	case T_CHARACTER:
	case T_STRSLICE:
	case T_EXCEPTION:
		SC_FREE(sc);
		return n;
	}

	fatal("unknown tag type\n");
	return NULL;

try_tco:
	SC_FREE(sc);

	if (tco != &NO_TCO) {
		sc = tco;
		n = res;
		goto again;
	}

	return res;
}

Node *pa_eval(EvalContext *ctx, Node *n) {
	return eval_with_scope(ctx, NULL, n);
}

static EvalScope *scope_new(EvalScope *up) {
	EvalScope *sc = acalloc(1, sizeof(*sc));
	sc->data = pa_hash_new();
	sc->up = up;
	sc->refs = 1;
	return sc;
}

static void scope_delete(EvalScope *sc) {
	pa_hash_delete(sc->data);
	SC_FREE(sc->up);
	free(sc);
}

static EvalScope *SC_CLONE(EvalScope *sc) {
	if (!sc) return sc;
	sc->refs++;
	return sc;
}

static EvalScope *SC_FREE(EvalScope *sc) {
	if (!sc) return sc;
	sc->refs--;
	if (sc->refs <= 0) {
		scope_delete(sc);
		return NULL;
	}
	return sc;
}

// Standard modules
// ===========================================================================

#ifndef PAREN_NO_STD_MODULES

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

struct mod_symbol {
	char *name;
	Node prot;
};

static void init_module(EvalContext *ctx, struct mod_symbol *syms) {
	if (ctx->global != NULL)
		return;

	pa_eval_init(ctx);

	for (; syms->name; syms++) {
		syms->prot.refs = PERMANENT;
		pa_hash_put(ctx->global, syms->name, &syms->prot);
	}
}

static int pa_argc = 0;
static char **pa_argv;

void pa_set_args(int argc, char **argv) {
	pa_argc = argc;
	pa_argv = argv;
}

static Node *core_args(EvalContext *ctx, Node *args) {
	ListBuilder res;
	int i;

	DECREF(args);

	pa_lb_init(&res);
	for (i=1; i<pa_argc; i++)
		pa_lb_append(&res, STR(pa_str_from_cstr(pa_argv[i])));

	return pa_lb_finish(&res);
}

static Node *core_exit(EvalContext *ctx, Node *args) {
	int code = IS_NIL(args) ? 0 : CAR(args)->v;
	exit(code);
}

static struct mod_symbol core_symbols[] = {
	{ "args",   { .tag = T_BUILTIN, .fn = core_args   } },
	{ "exit",   { .tag = T_BUILTIN, .fn = core_exit   } },
	{ },
};

EvalContext *pa_mod_core(void) {
	static EvalContext ctx;
	init_module(&ctx, core_symbols);
	return &ctx;
}

#define IO_BUFSIZE 512

struct IoFile {
	int fd;
	char *name;
	char rbuf[IO_BUFSIZE];
	int rbuflen;
};

static void io_close_real(struct IoFile *f) {
	if (f->fd >= 0)
		close(f->fd);

	free(f->name);
	free(f);
}

static Node *io_open(EvalContext *ctx, Node *args) {
	Node *name = CAR(args);
	Node *mode = CADR(args);

	int fd;
	int oflag = O_RDONLY;

	char namebuf[4096];
	struct IoFile *f;

	if (mode->tag == T_STRSLICE) {
		char *s = mode->str.s->buf + mode->str.start;
		switch (s[0]) {
		case 'r':
			oflag = O_RDONLY;
			if (s[1] == '+')
				oflag = O_RDWR;
			break;
		case 'w':
			oflag = O_WRONLY | O_CREAT;
			if (s[1] == '+')
				oflag = O_RDWR | O_CREAT;
			break;
		case 'a':
			oflag = O_WRONLY | O_APPEND;
			if (s[1] == '+')
				oflag = O_RDWR | O_APPEND;
			break;
		default:
			goto error;
		}
	}

	if (name->tag != T_STRSLICE)
		goto error;

	if (pa_str_val(namebuf, 4096, name) < 0)
		goto error;

	if ((fd = open(namebuf, oflag, 0644)) < 0)
		goto error;

	f = amalloc(sizeof(*f));
	f->name = strdup(namebuf);
	f->fd = fd;
	f->rbuflen = 0;
	DECREF(args);
	return INTERNAL(io_open, f, (InternalDtor*) io_close_real);

error:
	DECREF(args);
	return NIL;
}

static Node *io_close(EvalContext *ctx, Node *args) {
	args = XCAR(args);
	struct IoFile *f;

	if (args->tag != T_INTERNAL || args->raw.kind != io_open)
		goto error;

	f = args->raw.p;

	if (f->fd < 0)
		goto error;

	close(f->fd);
	f->fd = -1;

	DECREF(args);
	return T;

error:
	DECREF(args);
	return NIL;
}

static Node *io_gets(EvalContext *ctx, Node *args) {
	Node *file;
	struct IoFile *f;
	char *s, buf[4096];
	ssize_t buflen, readlen;

	file = CAR(args);

	if (file->tag != T_INTERNAL || file->raw.kind != io_open)
		goto error;

	f = file->raw.p;

	if (f->fd < 0)
		goto error;

	memcpy(buf, f->rbuf, f->rbuflen);
	buflen = f->rbuflen;

	for (;;) {
		s = memchr(buf, '\n', buflen);
		if (s != NULL)
			break;
		if (buflen >= 4096)
			break;

		readlen = IO_BUFSIZE;
		if (readlen > 4096 - buflen)
			readlen = 4096 - buflen;

		readlen = read(f->fd, buf + buflen, readlen);
		if (readlen <= 0)
			goto error;
		buflen += readlen;
	}

	if (s == NULL)
		goto error;

	readlen = (s - buf);
	*s++ = '\0';

	f->rbuflen = buflen - readlen - 1;
	memcpy(f->rbuf, s, f->rbuflen);

	Node *n = STR(pa_str_from_buf(buf, readlen));

	DECREF(args);
	return n;

error:
	DECREF(args);
	return NIL;
}

static struct mod_symbol io_symbols[] = {
	{ "open",   { .tag = T_BUILTIN, .fn = io_open   } },
	{ "close",  { .tag = T_BUILTIN, .fn = io_close  } },
	{ "gets",   { .tag = T_BUILTIN, .fn = io_gets   } },
	{ },
};

EvalContext *pa_mod_io(void) {
	static EvalContext ctx;
	init_module(&ctx, io_symbols);
	return &ctx;
}

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

static Node *net_socket(EvalContext *ctx, Node *args) {
	DECREF(args);
	return INT(socket(AF_INET, SOCK_STREAM, 0));
}

static Node *net_connect(EvalContext *ctx, Node *args) {
	Node *sock = CAR(args);
	Node *host = CADR(args);
	Node *port = CADDR(args);
	char hostbuf[4096];
	char servbuf[128];
	struct addrinfo hints;
	struct addrinfo *res;

	if (sock->tag != T_INTEGER || host->tag != T_STRSLICE
	    || port->tag != T_INTEGER) {
		DECREF(args);
		return NIL;
	}

	if (host->str.len > 4096) {
		DECREF(args);
		return NIL;
	}

	if (pa_str_val(hostbuf, 4096, host) < 0) {
		DECREF(args);
		return NIL;
	}

	snprintf(servbuf, 128, "%ld", port->v);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(hostbuf, servbuf, &hints, &res) < 0) {
		DECREF(args);
		return NIL;
	}

	if (connect(sock->v, res->ai_addr, res->ai_addrlen) < 0) {
		DECREF(args);
		freeaddrinfo(res);
		return NIL;
	}

	DECREF(args);
	return T;
}

static Node *net_recv(EvalContext *ctx, Node *args) {
	Node *sock = CAR(args);
	Node *size = CADR(args);
	char buf[4096];
	ssize_t sz;

	if (sock->tag != T_INTEGER || size->tag != T_INTEGER) {
		DECREF(args);
		return NIL;
	}

	sz = size->v;
	if (sz > 4096)
		sz = 4096;

	if ((sz = recv(sock->v, buf, sz, 0)) < 0) {
		DECREF(args);
		return NIL;
	}

	Node *n = STR(pa_str_from_buf(buf, sz));

	DECREF(args);
	return n;
}

static Node *net_send(EvalContext *ctx, Node *args) {
	Node *sock = CAR(args);
	Node *buf = CADR(args);
	size_t sz;
	ssize_t ssz;

	if (sock->tag != T_INTEGER || buf->tag != T_STRSLICE) {
		DECREF(args);
		return NIL;
	}

	for (sz=0; sz<buf->str.len; ) {
		ssz = send(sock->v,
		           buf->str.s->buf + buf->str.start + sz,
		           buf->str.len - sz, 0);

		if (ssz <= 0) {
			DECREF(args);
			return INT(sz);
		}

		sz += ssz;
	}

	DECREF(args);
	return T;
}

static struct mod_symbol net_symbols[] = {
	{ "socket",      { .tag = T_BUILTIN, .fn = net_socket } },
	{ "connect",     { .tag = T_BUILTIN, .fn = net_connect } },
	{ "recv",        { .tag = T_BUILTIN, .fn = net_recv } },
	{ "send",        { .tag = T_BUILTIN, .fn = net_send } },
	{ },
};

EvalContext *pa_mod_net(void) {
	static EvalContext ctx;
	init_module(&ctx, net_symbols);
	return &ctx;
}

#endif // PAREN_NO_STD_MODULES
