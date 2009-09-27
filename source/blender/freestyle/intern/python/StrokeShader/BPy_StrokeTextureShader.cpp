#include "BPy_StrokeTextureShader.h"

#include "../../stroke/BasicStrokeShaders.h"
#include "../BPy_Convert.h"
#include "../BPy_MediumType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for StrokeTextureShader instance  -----------*/
static int StrokeTextureShader___init__( BPy_StrokeTextureShader* self, PyObject *args);

/*-----------------------BPy_StrokeTextureShader type definition ------------------------------*/

PyTypeObject StrokeTextureShader_Type = {
	PyObject_HEAD_INIT(NULL)
	"StrokeTextureShader",          /* tp_name */
	sizeof(BPy_StrokeTextureShader), /* tp_basicsize */
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
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"StrokeTextureShader objects",  /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&StrokeShader_Type,             /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)StrokeTextureShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int StrokeTextureShader___init__( BPy_StrokeTextureShader* self, PyObject *args)
{
	const char *s1;
	PyObject *obj2 = 0, *obj3 = 0;
	
	if(!( PyArg_ParseTuple(args, "s|O!O", &s1, &MediumType_Type, &obj2, &obj3) ))
		return -1;

	Stroke::MediumType mt = (obj2) ? MediumType_from_BPy_MediumType(obj2) : Stroke::OPAQUE_MEDIUM; 
	bool b = (obj3) ? bool_from_PyBool(obj3) : true;
	
	self->py_ss.ss = new StrokeShaders::StrokeTextureShader(s1,mt,b);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
