#include <stdlib.h>
#include <stdio.h>
#include "lexer.h"

#define dfprintf(X...) do{}while(0)

void lx_error(const char *m)
{
	fprintf(stderr, "lexer error: %s\n", m);
	exit(2);
}

FILE *lx_file;
char lx_tokstr[2048];

const char *lx_names[] = {
	[T_INVALID] = "(invalid token)",
	[T_EOF]     = "end of file",
	[T_ID]      = "identifier",
	[T_INT]     = "integer constant",
	[T_LPAR]    = "(",
	[T_RPAR]    = ")",
	[T_LBRACK]  = "[",
	[T_RBRACK]  = "]",
	[T_ADD]     = "+",
	[T_SUB]     = "-",
	[T_MUL]     = "*",
	[T_DIV]     = "/",
	[T_MOD]     = "%",
	[T_LSH]     = "<<",
	[T_RSH]     = ">>",
	[T_AND]     = "&",
	[T_OR]      = "|",
	[T_XOR]     = "^",
	[T_EQ]      = "==",
	[T_NE]      = "!=",
	[T_GT]      = ">",
	[T_LT]      = "<",
	[T_GE]      = ">=",
	[T_LE]      = "<=",
	[T_LAND]    = "&&",
	[T_LOR]     = "||",
	[T_NEG]     = "!",
	[T_QUES]    = "?",
	[T_COLON]   = ":",
	[T_COMMA]   = ",",
	[T_ASSIGN]  = "=",
	[T_FN]      = "fn",
};

static enum lx_token ungotten = T_INVALID;

static enum lx_token scan_id(void)
{
	char *s = lx_tokstr;
	int c;

	for (;;) {
		c = getc(lx_file);
		if (!isalnum(c) && c != '_') {
			ungetc(c, lx_file);
			break;
		}
		*s++ = c;
	}

	*s = '\0';

	if (!strcmp(lx_tokstr, "fn")) {
		dfprintf(stderr, "--- fn\n");
		return T_FN;
	}

	dfprintf(stderr, "--- %s %s\n", lx_names[T_ID], lx_tokstr);

	return T_ID;
}

static enum lx_token scan_int(void)
{
	char *s = lx_tokstr;
	int c;

	for (;;) {
		c = getc(lx_file);
		if (!isdigit(c)) {
			ungetc(c, lx_file);
			break;
		}
		*s++ = c;
	}

	*s = '\0';

	dfprintf(stderr, "--- %s %s\n", lx_names[T_INT], lx_tokstr);

	return T_INT;
}

enum lx_token lx_token(void)
{
	enum lx_token t = T_INVALID;
	int c, d;

	if (ungotten != T_INVALID) {
		t = ungotten;
		ungotten = T_INVALID;
		return t;
	}

	do {
		c = getc(lx_file);
		if (c == '#') do {
			c = getc(lx_file);
		} while (c != '\n' && c != EOF);
	} while (c != EOF && isspace(c));

	if (isalpha(c) || c == '_') {
		ungetc(c, lx_file);
		return scan_id();
	}

	if (isdigit(c)) {
		ungetc(c, lx_file);
		return scan_int();
	}

	lx_tokstr[2] = '\0';
	d = '\0';

	switch (c) {
	case EOF: t = T_EOF; break;
	case '(': t = T_LPAR; break;
	case ')': t = T_RPAR; break;
	case '[': t = T_LBRACK; break;
	case ']': t = T_RBRACK; break;
	case '+': t = T_ADD; break;
	case '-': t = T_SUB; break;
	case '*': t = T_MUL; break;
	case '/': t = T_DIV; break;
	case '%': t = T_MUL; break;
	case '^': t = T_XOR; break;
	case '?': t = T_QUES; break;
	case ':': t = T_COLON; break;
	case ',': t = T_COMMA; break;
	case '&':
		d = getc(lx_file);
		switch (d) {
		case '&': t = T_LAND; break;
		default:
			ungetc(d, lx_file);
			d = '\0';
			t = T_AND;
			break;
		}
		break;
	case '|':
		d = getc(lx_file);
		switch (d) {
		case '|': t = T_LOR; break;
		default:
			ungetc(d, lx_file);
			d = '\0';
			t = T_OR;
			break;
		}
		break;
	case '>':
		d = getc(lx_file);
		switch (d) {
		case '>': t = T_RSH; break;
		case '=': t = T_GE; break;
		default:
			ungetc(d, lx_file);
			d = '\0';
			t = T_GT;
			break;
		}
		break;
	case '<':
		d = getc(lx_file);
		switch (d) {
		case '<': t = T_LSH; break;
		case '=': t = T_LE; break;
		default:
			ungetc(d, lx_file);
			d = '\0';
			t = T_LT;
			break;
		}
		break;
	case '=':
		d = getc(lx_file);
		switch (d) {
		case '=': t = T_EQ; break;
		default:
			ungetc(d, lx_file);
			d = '\0';
			t = T_ASSIGN;
			break;
		}
		break;
	case '!':
		d = getc(lx_file);
		switch (d) {
		case '=': t = T_NE; break;
		default:
			ungetc(d, lx_file);
			d = '\0';
			t = T_NEG;
			break;
		}
		break;

	default: t = T_INVALID; break;
	}

	lx_tokstr[0] = c;
	lx_tokstr[1] = d;

	dfprintf(stderr, "--- %s\n", lx_names[t]);

	return t;
}

void lx_unget(enum lx_token t)
{
	if (t == T_INVALID)
		lx_error("tried to unget T_INVALID");
	if (ungotten != T_INVALID)
		lx_error("tried to unget twice");

	ungotten = t;
}

void lx_init(void)
{
	lx_file = stdin;
}
