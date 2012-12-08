#include "BPy_Id.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

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

static char Id___doc__[] =
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: An Id object.\n"
"   :type iBrother: :class:`Id`\n"
"\n"
".. method:: __init__(iFirst)\n"
"\n"
"   Builds an Id from an integer. The second number is set to 0.\n"
"\n"
"   :arg iFirst: The first Id number.\n"
"   :type iFirst: int\n"
"\n"
".. method:: __init__(iFirst, iSecond)\n"
"\n"
"   Builds the Id from the two numbers.\n"
"\n"
"   :arg iFirst: The first Id number.\n"
"   :type iFirst: int\n"
"   :arg iSecond: The second Id number.\n"
"   :type iSecond: int\n";

static int Id___init__(BPy_Id *self, PyObject *args, PyObject *kwds)
{
    int first = 0, second = 0;
    static const char *kwlist[] = {"first", "second", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|ii", (char**)kwlist, &first, &second) )
        return -1;

	self->id = new Id( first, second );

    return 0;
}

static void Id___dealloc__(BPy_Id* self)
{
	delete self->id;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * Id___repr__(BPy_Id* self)
{
    return PyUnicode_FromFormat("[ first: %i, second: %i ](BPy_Id)", self->id->getFirst(), self->id->getSecond() );
}

static char Id_getFirst___doc__[] =
".. method:: getFirst()\n"
"\n"
"   Returns the first Id number.\n"
"\n"
"   :return: The first Id number.\n"
"   :rtype: int\n";

static PyObject *Id_getFirst( BPy_Id *self ) {
	return PyLong_FromLong( self->id->getFirst() );
}

static char Id_getSecond___doc__[] =
".. method:: getSecond()\n"
"\n"
"   Returns the second Id number.\n"
"\n"
"   :return: The second Id number.\n"
"   :rtype: int\n";

static PyObject *Id_getSecond( BPy_Id *self) {
	return PyLong_FromLong( self->id->getSecond() );
}

static char Id_setFirst___doc__[] =
".. method:: setFirst(iFirst)\n"
"\n"
"   Sets the first number constituting the Id.\n"
"\n"
"   :arg iFirst: The first number constituting the Id.\n"
"   :type iFirst: int\n";

static PyObject *Id_setFirst( BPy_Id *self , PyObject *args) {
	unsigned int i;

	if( !PyArg_ParseTuple(args, "i", &i) )
		return NULL;

	self->id->setFirst( i );

	Py_RETURN_NONE;
}

static char Id_setSecond___doc__[] =
".. method:: setSecond(iSecond)\n"
"\n"
"   Sets the second number constituting the Id.\n"
"\n"
"   :arg iSecond: The second number constituting the Id.\n"
"   :type iSecond: int\n";

static PyObject *Id_setSecond( BPy_Id *self , PyObject *args) {
	unsigned int i;

	if( !PyArg_ParseTuple(args, "i", &i) )
		return NULL;

	self->id->setSecond( i );

	Py_RETURN_NONE;
}

static PyObject * Id_RichCompare(BPy_Id *o1, BPy_Id *o2, int opid) {
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

/*----------------------Id instance definitions ----------------------------*/
static PyMethodDef BPy_Id_methods[] = {
	{"getFirst", ( PyCFunction ) Id_getFirst, METH_NOARGS, Id_getFirst___doc__},
	{"getSecond", ( PyCFunction ) Id_getSecond, METH_NOARGS, Id_getSecond___doc__},
	{"setFirst", ( PyCFunction ) Id_setFirst, METH_VARARGS, Id_setFirst___doc__},
	{"setSecond", ( PyCFunction ) Id_setSecond, METH_VARARGS, Id_setSecond___doc__},
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
	Id___doc__,                     /* tp_doc */
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

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
