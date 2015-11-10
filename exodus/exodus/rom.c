#include <Python.h>

#include <stdio.h>
#include <stdarg.h>

/* exodus.rom.Notes definition */
/* --------------------------- */

#define INDEX_BITS 10 /* how many MSB of address */

#define ADDR_BITS 22
#define INDEX_SIZE (1 << INDEX_BITS)
#define INDEX_SHIFT (ADDR_BITS - INDEX_BITS)
#define ADDR_MAX ((1 << ADDR_BITS) - 1)

#define INDEX(addr) ((unsigned)(addr) >> INDEX_SHIFT)

static void clamp(int *a, int low, int hi)
{
	if (*a < low)
		*a = low;
	if (*a > hi)
		*a = hi;
}

struct note {
	int32_t addr;
	PyObject *data;
	struct note *next, *prev;
};

typedef struct Notes {
	PyObject_HEAD
	struct note *sentinel;
	struct note *index[INDEX_SIZE];
} Notes;

static PyTypeObject Notes_type;

static PyObject *start_notes_iter(struct note *n, int32_t end);

static struct note *note_alloc(void)
{
	return calloc(1, sizeof(struct note));
}

static void note_dealloc(struct note *n)
{
	Py_XDECREF(n->data);
	free(n);
}

static PyObject *build_note(struct note *n)
{
	return Py_BuildValue("(iO)", n->addr, n->data);
}

/* gets last note whose address is less than or equal to addr */
static struct note *note_at(Notes *notes, int32_t addr)
{
	struct note *n;
	int32_t idx = INDEX(addr);

	while (idx && notes->index[idx] == NULL)
		idx--;

	n = notes->index[idx];
	if (n == NULL)
		n = notes->sentinel;

	while (n->addr > addr)
		n = n->prev;

	for (; ; n=n->next) {
		if (n->next == NULL || n->next->addr > addr)
			return n;
	}

	/* should not be reached */
	return NULL;
}

static void note_insert(Notes *notes, struct note *after, struct note *n)
{
	struct note *old;

	n->prev = after;
	n->next = after->next;
	if (n->prev != NULL) /* always true, but makes the code patterned */
		n->prev->next = n;
	if (n->next != NULL)
		n->next->prev = n;

	old = notes->index[INDEX(n->addr)];
	if (old == NULL || old->addr > n->addr)
		notes->index[INDEX(n->addr)] = n;
}

static void note_remove(Notes *notes, struct note *n)
{
	int32_t idx;

	if (n->prev != NULL) /* always true, but makes the code patterned */
		n->prev->next = n->next;
	if (n->next != NULL)
		n->next->prev = n->prev;

	idx = INDEX(n->addr);

	if (n->next && idx == INDEX(n->next->addr))
		notes->index[idx] = n->next;
	else
		notes->index[idx] = NULL;
}

static void Notes_dealloc(Notes *self)
{
	struct note *next, *cur;

	for (cur = self->sentinel; cur; cur = next) {
		next = cur->next;
		note_dealloc(cur);
	}

	Py_TYPE(self)->tp_free((PyObject*)self);
}

static int Notes_init(Notes *notes, PyObject *args, PyObject *kwds)
{
	int i;

	for (i=0; i<INDEX_SIZE; i++)
		notes->index[i] = NULL;

	notes->sentinel = note_alloc();
	notes->sentinel->addr = -1;

	return 0;
}

static PyObject *Notes_add(PyObject *self, PyObject *args)
{
	Notes *notes = (Notes*)self;
	PyObject *data;
	struct note *n, *after;
	int at;

	if (PyArg_ParseTuple(args, "iO", &at, &data) < 0)
		return NULL;

	clamp(&at, 0, ADDR_MAX);

	n = after = note_at(notes, at);
	if (n && n->addr != at)
		n = NULL;

	if (n == NULL) {
		n = note_alloc();
		n->addr = at;
		note_insert(notes, after, n);
	}

	Py_XDECREF(n->data);
	n->data = data;
	Py_INCREF(data);

	return build_note(n);
}

static PyObject *Notes_delete(PyObject *self, PyObject *args)
{
	Notes *notes = (Notes*)self;
	struct note *n;
	int at;

	if (PyArg_ParseTuple(args, "i", &at) < 0)
		return NULL;

	clamp(&at, 0, ADDR_MAX);

	n = note_at(notes, at);
	if (n == NULL || n->addr != at) {
		PyErr_SetString(PyExc_IndexError, "No note at that address");
		return NULL;
	}

	note_remove(notes, n);
	note_dealloc(n);

	Py_RETURN_NONE;
}

static PyObject *Notes_get(PyObject *self, PyObject *args)
{
	Notes *notes = (Notes*)self;
	struct note *n;
	int low, high;

	if (PyTuple_Size(args) == 0) {
		low = 0;
		high = 1 << ADDR_BITS;

	} else if (PyTuple_Size(args) == 1) {
		if (PyArg_ParseTuple(args, "i", &low) < 0)
			return NULL;

		clamp(&low, 0, ADDR_MAX);
		n = note_at(notes, low);
		if (n == NULL || n->addr != low)
			Py_RETURN_NONE;

		return build_note(n);

	} else if (PyTuple_Size(args) == 2) {
		if (PyArg_ParseTuple(args, "ii", &low, &high) < 0)
			return NULL;

	} else {
		PyErr_SetString(PyExc_ValueError, "Expected 0 to 2 args");
		return NULL;
	}

	clamp(&low, 0, (1 << ADDR_BITS) - 1);
	clamp(&high, 0, (1 << ADDR_BITS));

	n = note_at(notes, low);
	while (n && n->addr < low)
		n = n->next;

	return start_notes_iter(n, high);
}

static PyObject *Notes_dump(PyObject *self, PyObject *args)
{
	Notes *notes = (Notes*)self;
	struct note *n;
	int i;

	n = notes->sentinel;

	printf("--dump of %p\n", notes);
	for (; n; n = n->next)
		printf("%06x %p\n", ((unsigned)n->addr) & 0xffffff, n->data);
	printf("--index:\n");
	for (i=0; i<INDEX_SIZE; i++) {
		if (!notes->index[i])
			continue;
		printf("%06x %06x\n", i << INDEX_SHIFT,
		       ((unsigned)notes->index[i]->addr) & 0xffffff);
	}

	Py_RETURN_NONE;
}

static PyObject *Notes_iter(PyObject *self)
{
	Notes *notes = (Notes*)self;
	PyObject *ni;

	ni = start_notes_iter(notes->sentinel->next, 1 << ADDR_BITS);
	Py_INCREF(ni);

	return ni;
}

static PyMethodDef Notes_method_table[] = {
	{ "add",    Notes_add,    METH_VARARGS, "Add an address note" },
	{ "delete", Notes_delete, METH_VARARGS, "Delete an address note" },
	{ "get",    Notes_get,    METH_VARARGS, "Get some notes" },
	{ "dump",   Notes_dump,   METH_VARARGS, "Dumps the data structure" },
	{ }
};

static PyTypeObject Notes_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name         = "exodus.rom.Notes",
	.tp_basicsize    = sizeof(Notes),
	.tp_dealloc      = (destructor)Notes_dealloc,
	.tp_flags        = Py_TPFLAGS_DEFAULT,
	.tp_doc          = "exodus ROM notes",
	.tp_methods      = Notes_method_table,
	.tp_init         = (initproc)Notes_init,
	.tp_iter         = Notes_iter,
};

typedef struct NotesIter {
	PyObject_HEAD
	struct note *cur;
	int32_t end;
} NotesIter;

static PyTypeObject NotesIter_type;

static PyObject *start_notes_iter(struct note *n, int32_t end)
{
	NotesIter *ni;

	ni = (NotesIter*)NotesIter_type.tp_alloc(&NotesIter_type, 0);

	ni->cur = n;
	ni->end = end;

	return (PyObject*)ni;
}

static PyObject *NotesIter_iter(PyObject *iter)
{
	Py_INCREF(iter);
	return iter;
}

static PyObject *NotesIter_next(PyObject *iter)
{
	NotesIter *ni = (NotesIter*)iter;
	PyObject *cur;

	if (ni->cur == NULL || ni->cur->addr >= ni->end) {
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}

	cur = build_note(ni->cur);
	ni->cur = ni->cur->next;

	return cur;
}

static PyTypeObject NotesIter_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name         = "exodus.rom.NotesIter",
	.tp_basicsize    = sizeof(NotesIter),
	.tp_flags        = Py_TPFLAGS_DEFAULT,
	.tp_doc          = "exodus ROM notes iterator",
	.tp_iter         = NotesIter_iter,
	.tp_iternext     = NotesIter_next,
};

/* exodus.rom.ROM definition */
/* ------------------------- */

typedef struct ROM {
	PyObject_HEAD

	unsigned char *data;
	unsigned char *oob;
	size_t size;
} ROM;

static PyTypeObject ROM_type;

static void ROM_dealloc(ROM *self)
{
	if (self->data != NULL)
		free(self->data);
	if (self->oob != NULL)
		free(self->oob);

	Py_TYPE(self)->tp_free((PyObject*)self);
}

static ROM *ROM_alloc(void)
{
	ROM *rom;

	rom = (ROM*)ROM_type.tp_alloc(&ROM_type, 0);

	rom->data = NULL;
	rom->oob = NULL;
	rom->size = 0;

	return rom;
}

static PyObject *do_array(ROM *rom, const char *data, PyObject *args)
{
	Py_ssize_t start, len;

	if (PyTuple_Size(args) == 0) {
		start = 0;
		len = rom->size;

	} else if (PyTuple_Size(args) == 2) {
		if (PyArg_ParseTuple(args, "ii", &start, &len) < 0)
			return NULL;

		if (start < 0 || len < 0 || start + len > rom->size) {
			PyErr_SetString(PyExc_IndexError,
			                "ROM indices out of range");
			return NULL;
		}

	} else {
		PyErr_SetString(PyExc_ValueError, "Expected 0 or 2 args");
		return NULL;
	}

	return PyBytes_FromStringAndSize(data + start, len);
}

static PyObject *ROM_bytes(PyObject *self, PyObject *args)
{
	ROM *rom = (ROM*)self;
	return do_array(rom, (char*)rom->data, args);
}

static PyObject *ROM_oob(PyObject *self, PyObject *args)
{
	ROM *rom = (ROM*)self;
	return do_array(rom, (char*)rom->oob, args);
}

static PyObject *ROM_oob_get(PyObject *self, PyObject *args)
{
	ROM *rom = (ROM*)self;
	unsigned addr;

	if (PyArg_ParseTuple(args, "i", &addr) < 0)
		return NULL;

	if (addr > rom->size) {
		PyErr_SetString(PyExc_IndexError, "Address out of range");
		return NULL;
	}

	return PyLong_FromLong(rom->oob[addr]);
}

static PyObject *ROM_oob_set(PyObject *self, PyObject *args)
{
	ROM *rom = (ROM*)self;
	unsigned addr, byte;

	if (PyArg_ParseTuple(args, "ii", &addr, &byte) < 0)
		return NULL;

	if (addr > rom->size) {
		PyErr_SetString(PyExc_IndexError, "Address out of range");
		return NULL;
	}

	rom->oob[addr] = byte;

	return PyLong_FromLong(rom->oob[addr]);
}

static PyObject *ROM_size(PyObject *self, PyObject *args)
{
	return PyLong_FromLong(((ROM*)self)->size);
}

static PyMethodDef ROM_method_table[] = {
	{ "bytes",   ROM_bytes,   METH_VARARGS, "Get some bytes" },
	{ "oob",     ROM_oob,     METH_VARARGS, "Get some OOB data" },
	{ "oob_get", ROM_oob_get, METH_VARARGS, "Get an OOB byte" },
	{ "oob_set", ROM_oob_set, METH_VARARGS, "Set an OOB byte" },
	{ "size",    ROM_size,    METH_VARARGS, "Get the size" },
	{ }
};

static PyTypeObject ROM_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name         = "exodus.rom.ROM",
	.tp_basicsize    = sizeof(ROM),
	.tp_dealloc      = (destructor)ROM_dealloc,
	.tp_flags        = Py_TPFLAGS_DEFAULT,
	.tp_doc          = "exodus ROM image",
	.tp_methods      = ROM_method_table,
};

/* exodus.rom methods */
/* ------------------ */

static int rom_read(ROM *rom, FILE *f)
{
	size_t r;

	for (r=0; r<rom->size; ) {
		r += fread(rom->data+r, 1, rom->size-r, f);
		if (feof(f) || ferror(f))
			break;
	}

	if (r < rom->size)
		return -1;

	return 0;
}

static PyObject *rom_open(PyObject *self, PyObject *args)
{
	const char *s;
	FILE *f;
	ROM *rom;
	struct stat st;

	f = NULL;
	rom = NULL;

	if (PyArg_ParseTuple(args, "s", &s) < 0)
		return NULL;

	if (stat(s, &st) < 0) {
		PyErr_SetFromErrno(PyExc_OSError);
		return NULL;
	}

	if (!(f = fopen(s, "r"))) {
		PyErr_SetFromErrno(PyExc_OSError);
		return NULL;
	}

	if (!(rom = ROM_alloc()))
		goto fail;

	rom->size = st.st_size;
	if (!(rom->data = malloc(rom->size))) {
		PyErr_SetString(PyExc_MemoryError, "Could not malloc() ROM");
		goto fail;
	}

	if (!(rom->oob = calloc(1, rom->size))) {
		PyErr_SetString(PyExc_MemoryError, "Could not malloc() ROM");
		goto fail;
	}

	if (rom_read(rom, f) < 0) {
		PyErr_SetString(PyExc_OSError, "Read error");
		goto fail;
	}

	fclose(f);

	return (PyObject*)rom;

fail:
	if (f != NULL)
		fclose(f);
	if (rom != NULL)
		ROM_dealloc(rom);
	return NULL;
}

static PyMethodDef exodus_rom_method_table[] = {
	{ "open", rom_open, METH_VARARGS, "Open a ROM" },
	{ }
};

static PyModuleDef exodus_rom_module = {
	PyModuleDef_HEAD_INIT,
	"rom", NULL, -1, exodus_rom_method_table,
};

PyMODINIT_FUNC PyInit_rom(void)
{
	PyObject *m;

	if (PyType_Ready(&ROM_type) < 0)
		return NULL;
	Notes_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&Notes_type) < 0)
		return NULL;
	if (PyType_Ready(&NotesIter_type) < 0)
		return NULL;

	if (!(m = PyModule_Create(&exodus_rom_module)))
		return NULL;

	Py_INCREF(&ROM_type);
	PyModule_AddObject(m, "ROM", (PyObject*)&ROM_type);
	Py_INCREF(&Notes_type);
	PyModule_AddObject(m, "Notes", (PyObject*)&Notes_type);
	Py_INCREF(&NotesIter_type);
	PyModule_AddObject(m, "NotesIter", (PyObject*)&NotesIter_type);

	return m;
}
