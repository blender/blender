#include <Python.h>

struct ID;
struct IDProperty;
struct BPy_IDGroup_Iter;

typedef struct BPy_IDProperty {
	PyObject_VAR_HEAD
	struct ID *id;
	struct IDProperty *prop;
	PyObject *data_wrap;
} BPy_IDProperty;

typedef struct BPy_IDArray {
	PyObject_VAR_HEAD
	struct ID *id;
	struct IDProperty *prop;
} BPy_IDArray;

typedef struct BPy_IDGroup_Iter {
	PyObject_VAR_HEAD
	BPy_IDProperty *group;
	struct IDProperty *cur;
} BPy_IDGroup_Iter;

PyObject *BPy_Wrap_IDProperty(struct ID *id, struct IDProperty *prop);
