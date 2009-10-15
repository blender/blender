#include "BPy_Id.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Id instance  -----------*/
static int Id___init__(BPy_Id *self, PyObject *args, PyObject *kwds);
static void Id___dealloc__(BPy_Id *self);
static PyObject * Id___repr__(BPy_Id* self);
static PyObject * Id_RichCompare(BPy_Id *o1, BPy_Id *o2, int opid);

static PyObject * Id_getFirst( BPy_Id *self );
static PyObject * Id_getSecond( BPy_Id *self);
static PyObject * Id_setFirst( BPy_Id *self , PyObject *args);
static PyObject * Id_setSecond( BPy_Id *self , PyObject *args);

/*----------------------Id instance definitions ----------------------------*/
static PyMethodDef BPy_Id_methods[] = {
	{"getFirst", ( PyCFunction ) Id_getFirst, METH_NOARGS, "Returns the first Id number"},
	{"getSecond", ( PyCFunction ) Id_getSecond, METH_NOARGS, "Returns the second Id number" },
	{"setFirst", ( PyCFunction ) Id_setFirst, METH_VARARGS, "Sets the first number constituing the Id" },
	{"setSecond", ( PyCFunction ) Id_setSecond, METH_VARARGS, "Sets the second number constituing the Id" },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Id type definition ------------------------------*/

PyTypeObject Id_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Id",                           /* tp_name */
	sizeof(BPy_Id),                 /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)Id___dealloc__,     /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)Id___repr__,          /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"Id objects",                   /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	(richcmpfunc)Id_RichCompare,    /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Id_methods,                 /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Id___init__,          /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------
int Id_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &Id_Type ) < 0 )
		return -1;

	Py_INCREF( &Id_Type );
	PyModule_AddObject(module, "Id", (PyObject *)&Id_Type);
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int Id___init__(BPy_Id *self, PyObject *args, PyObject *kwds)
{
    int first = 0, second = 0;
    static char *kwlist[] = {"first", "second", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &first, &second) )
        return -1;

	self->id = new Id( first, second );

    return 0;
}

void Id___dealloc__(BPy_Id* self)
{
	delete self->id;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyObject * Id___repr__(BPy_Id* self)
{
    return PyUnicode_FromFormat("[ first: %i, second: %i ](BPy_Id)", self->id->getFirst(), self->id->getSecond() );
}

PyObject *Id_getFirst( BPy_Id *self ) {
	return PyLong_FromLong( self->id->getFirst() );
}


PyObject *Id_getSecond( BPy_Id *self) {
	return PyLong_FromLong( self->id->getSecond() );
}


PyObject *Id_setFirst( BPy_Id *self , PyObject *args) {
	unsigned int i;

	if( !PyArg_ParseTuple(args, "i", &i) )
		return NULL;

	self->id->setFirst( i );

	Py_RETURN_NONE;
}


PyObject *Id_setSecond( BPy_Id *self , PyObject *args) {
	unsigned int i;

	if( !PyArg_ParseTuple(args, "i", &i) )
		return NULL;

	self->id->setSecond( i );

	Py_RETURN_NONE;
}

PyObject * Id_RichCompare(BPy_Id *o1, BPy_Id *o2, int opid) {
	switch(opid){
		case Py_LT:
			return PyBool_from_bool( o1->id->operator<(*(o2->id)) );
			break;
		case Py_LE:
			return PyBool_from_bool( o1->id->operator<(*(o2->id)) || o1->id->operator<(*(o2->id)) );
			break;
		case Py_EQ:
			return PyBool_from_bool( o1->id->operator==(*(o2->id)) );
			break;
		case Py_NE:
			return PyBool_from_bool( o1->id->operator!=(*(o2->id)) );
			break;
		case Py_GT:
			return PyBool_from_bool(!( o1->id->operator<(*(o2->id)) || o1->id->operator<(*(o2->id)) ));
			break;
		case Py_GE:
			return PyBool_from_bool(!( o1->id->operator<(*(o2->id)) ));
			break;
	}
	
	Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
