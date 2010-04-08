#include "BPy_MediumType.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*-----------------------BPy_MediumType type definition ------------------------------*/

PyTypeObject MediumType_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"MediumType",                   /* tp_name */
	sizeof(PyLongObject),           /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,             /* tp_flags */
	"MediumType objects",           /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&PyLong_Type,                   /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	0,                              /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

/*-----------------------BPy_IntegrationType instance definitions -------------------------*/

PyLongObject _BPy_MediumType_DRY_MEDIUM = {
	PyVarObject_HEAD_INIT(&MediumType_Type, 1)
	{ Stroke::DRY_MEDIUM }
};
PyLongObject _BPy_MediumType_HUMID_MEDIUM = {
	PyVarObject_HEAD_INIT(&MediumType_Type, 1)
	{ Stroke::HUMID_MEDIUM }
};
PyLongObject _BPy_MediumType_OPAQUE_MEDIUM = {
	PyVarObject_HEAD_INIT(&MediumType_Type, 1)
	{ Stroke::OPAQUE_MEDIUM }
};

//-------------------MODULE INITIALIZATION--------------------------------

int MediumType_Init( PyObject *module )
{	
	if( module == NULL )
		return -1;

	if( PyType_Ready( &MediumType_Type ) < 0 )
		return -1;
	Py_INCREF( &MediumType_Type );
	PyModule_AddObject(module, "MediumType", (PyObject *)&MediumType_Type);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

