#include <Python.h>

#include <stdio.h>
#include <stdarg.h>

#include "lib/m68k-decode.h"

static char *cc_to_str(int cc)
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

	return NULL;
}

static char *sz_to_str(m68k_insn_t *insn)
{
	m68k_opcode_info_t *info = m68k_opcode_info + insn->opcode;

	if (!info->has_sz)
		return NULL;

	switch (insn->sz) {
	case 0: return "b";
	case 1: return "w";
	case 2: return "l";
	}

	return NULL;
}

static PyObject *build_opcode(m68k_insn_t *insn)
{
	m68k_opcode_info_t *info = m68k_opcode_info + insn->opcode;
	char *ccstr = NULL;

	if (info->has_cc)
		ccstr = cc_to_str(insn->cc);

	return Py_BuildValue("(ss)", info->str, ccstr);
}

static PyObject *build_operand(m68k_operand_t *op)
{
	return Py_BuildValue("(iii)", op->mode, op->d, op->rn);
}

static PyObject *build_insn(m68k_insn_t *insn, Py_ssize_t sz)
{
	PyObject *pyinsn, *operands, *op;
	int i;

	if (insn->opcode == M68K_OP_INVALID)
		Py_RETURN_NONE;

	if (!(operands = PyList_New(insn->opcount)))
		return NULL;

	for (i=0; i<insn->opcount; i++) {
		if (!(op = build_operand(insn->op + i))) {
			Py_XDECREF(operands);
			return NULL;
		}

		if (PyList_SetItem(operands, i, op) < 0) {
			Py_XDECREF(operands);
			return NULL;
		}
	}

	if (!(op = build_opcode(insn))) {
		Py_XDECREF(operands);
		return NULL;
	}

	pyinsn = Py_BuildValue("(OsOi)", op, sz_to_str(insn), operands, sz);

	Py_XDECREF(op);
	Py_XDECREF(operands);

	return pyinsn;
}

static PyObject *do_decode(uint8_t *rawmem, Py_ssize_t sz, Py_ssize_t *tsz)
{
	uint16_t *nextmem, mem[32];
	int i;
	m68k_insn_t insn;

	for (i=0; i<32 && i<sz; i++) {
		mem[i] = rawmem[0] << 8 | rawmem[1];
		rawmem += 2;
	}

	nextmem = m68k_decode(&insn, mem);
	*tsz = 2 * (nextmem - mem);

	return build_insn(&insn, *tsz);
}

/* exodus.codec.decode(big-endian bytes) -> list of instructions */
static PyObject *exodus_decode(PyObject *self, PyObject *args)
{
	PyObject *arg, *insns, *insn;
	uint8_t *mem;
	Py_ssize_t sz, tsz;

	if (!(arg = PyTuple_GetItem(args, 0)))
		return NULL;

	if (PyBytes_AsStringAndSize(arg, (char**)&mem, &sz) < 0)
		return NULL;

	if (!(insns = PyList_New(0)))
		return NULL;

	while (sz > 0) {
		insn = do_decode(mem, sz, &tsz);

		if (insn != NULL) {
			if (PyList_Append(insns, insn) < 0) {
				Py_DECREF(insns);
				Py_DECREF(insn);
				return NULL;
			}
		}

		if (tsz > sz) tsz = sz; /* ??? */
		sz -= tsz;
		mem += tsz;

		Py_DECREF(insn);
	}

	return insns;
}

static PyObject *exodus_fmtop(PyObject *self, PyObject *args)
{
	unsigned long mode, d, rn;

	if (PyArg_ParseTuple(args, "(iii)", &mode, &d, &rn) < 0)
		return NULL;

	switch (mode) {
	case M68K_M_DATA:
		return PyUnicode_FromFormat("%%d%d", rn);
	case M68K_M_ADDR:
		return PyUnicode_FromFormat("%%a%d", rn);
	case M68K_M_INDIR:
		return PyUnicode_FromFormat("(%%a%d)", rn);
	case M68K_M_POST:
		return PyUnicode_FromFormat("(%%a%d)+", rn);
	case M68K_M_PRE:
		return PyUnicode_FromFormat("-(%%a%d)", rn);
	case M68K_M_DISP:
		return PyUnicode_FromFormat("(%u,%%a%d)", d, rn);
	case M68K_M_PCINDIR:
		return PyUnicode_FromFormat("(%u,%%pc)", d);
	case M68K_M_MEM_W:
	case M68K_M_MEM_L:
		return PyUnicode_FromFormat("(%u)", d);
	case M68K_M_IMM:
		return PyUnicode_FromFormat("#%u", d);
	case M68K_M_SPECIAL:
		switch (rn) {
		case M68K_R_CCR:
			return PyUnicode_FromString("%ccr");
		case M68K_R_SR:
			return PyUnicode_FromString("%sr");
		case M68K_R_USP:
			return PyUnicode_FromString("%usp");
		}
		break;
	}

	PyErr_SetString(PyExc_ValueError, "Unknown operand type");
	return NULL;
}

static PyMethodDef codec_method_table[] = {
	{ "decode", exodus_decode, METH_VARARGS, "Decode an instruction" },
	{ "fmtop",  exodus_fmtop,  METH_VARARGS, "Format operand" },
	{ }
};

static PyModuleDef exodus_codec_module = {
	PyModuleDef_HEAD_INIT,
	"codec", NULL, -1, codec_method_table,
};

PyMODINIT_FUNC PyInit_codec(void)
{
	PyObject *m;

	if (!(m = PyModule_Create(&exodus_codec_module)))
		return NULL;

	return m;
}
