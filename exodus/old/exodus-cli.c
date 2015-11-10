#include <stdio.h>
#include <stdarg.h>
#include "rom.h"
#include "m68k-decode.h"

static void emit(const char *fmt, ...)
{
	char buf[65536];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, 65536, fmt, va);
	va_end(va);
	printf("%s\n", buf);
}

static const char *cc_name(int cc)
{
	switch (cc) {
	case 0: return "t";
	case 1: return "f";
	case 2: return "hi";
	case 3: return "ls";
	case 4: return "cc";
	case 5: return "lo";
	case 6: return "ne";
	case 7: return "eq";
	case 8: return "vc";
	case 9: return "vs";
	case 10: return "pl";
	case 11: return "mi";
	case 12: return "ge";
	case 13: return "lt";
	case 14: return "gt";
	case 15: return "le";
	}
	return "?";
}

static const char *sz_name(int sz)
{
	switch (sz) {
	case 0: return ".b";
	case 1: return ".w";
	case 2: return ".l";
	case -1: return "";
	}
	return ".?";
}

static char *decode_index(uint16_t ext, char *base, char *ops)
{
	return ops + sprintf(ops,
		"(%d,%s,%%%s%d)", (ext & 0xff), base,
		(ext & 0x8000) ? "a" : "d", (ext & 0x7) >> 12);
}

static char *decode_op(m68k_operand_t *op, char *ops)
{
	char buf[16];

	switch (op->mode) {
	case M68K_M_DATA:
		ops += sprintf(ops, "%%d%d", op->rn); break;
	case M68K_M_ADDR:
		ops += sprintf(ops, "%%a%d", op->rn); break;
	case M68K_M_INDIR:
		ops += sprintf(ops, "(%%a%d)", op->rn); break;
	case M68K_M_POST:
		ops += sprintf(ops, "(%%a%d)+", op->rn); break;
	case M68K_M_PRE:
		ops += sprintf(ops, "-(%%a%d)", op->rn); break;
	case M68K_M_DISP:
		ops += sprintf(ops, "(%d,%%a%d)", op->d, op->rn); break;
	case M68K_M_PCINDIR:
		ops += sprintf(ops, "(%d,%%pc)", op->d); break;
	case M68K_M_IMM:
		ops += sprintf(ops, "#0x%x", op->d); break;

	case M68K_M_MEM_W:
	case M68K_M_MEM_L:
		ops += sprintf(ops, "(0x%x)", op->d); break;

	case M68K_M_SPECIAL:
		switch (op->rn) {
		case M68K_R_CCR: ops += sprintf(ops, "%%ccr"); break;
		case M68K_R_SR: ops += sprintf(ops, "%%sr"); break;
		case M68K_R_USP: ops += sprintf(ops, "%%usp"); break;
		default: ops += sprintf(ops, "invalid"); break;
		}
		break;

	case M68K_M_INDEX:
		sprintf(buf, "%%a%d", op->rn);
		ops = decode_index(op->d, buf, ops);
		break;
	case M68K_M_PCINDEX:
		sprintf(buf, "%%pc");
		ops = decode_index(op->d, buf, ops);
		break;

	default:
		ops += sprintf(ops, "incomplete<%d>", op->mode); break;
	}

	return ops;
}

static void decode_ops(m68k_insn_t *insn, char *ops)
{
	if (insn->opcount > 0) {
		*ops++ = '\t';
		ops = decode_op(&insn->op[0], ops);
	}

	if (insn->opcount > 1) {
		*ops++ = ',';
		*ops++ = ' ';
		ops = decode_op(&insn->op[1], ops);
	}

	*ops = '\0';
}

static void print_insn(m68k_insn_t *insn, uint16_t w)
{
	char ops[512];
	m68k_opcode_info_t *info;

	if (insn->opcode == M68K_OP_INVALID) {
		emit("\t.word\t0x%x", w);
		return;
	}

	info = m68k_opcode_info + insn->opcode;

	decode_ops(insn, ops);
	emit("\t%s%s%s%s", info->str, info->has_cc ? cc_name(insn->cc) : "",
	     sz_name(insn->sz), ops);
}

int main(int argc, char *argv[])
{
	uint16_t w, *mem, *end;
	m68k_insn_t insn;

	if (argc < 2) {
		fprintf(stderr, "Missing filename\n");
		return 1;
	}

	rom_reset();
	if (rom_load(argv[1]) < 0) {
		fprintf(stderr, "rom_load failed\n");
		return 2;
	}

	mem = rom_data;
	end = mem + rom_words;
	while (mem < end) {
		w = *mem;
		mem = m68k_decode(&insn, mem);
		print_insn(&insn, w);
	}

	return 0;
}
