#include "BPy_GetViewMapGradientNormF1D.h"

#include "../../../stroke/AdvancedFunctions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for GetViewMapGradientNormF1D instance  -----------*/
static int GetViewMapGradientNormF1D___init__(BPy_GetViewMapGradientNormF1D* self, PyObject *args);

/*-----------------------BPy_GetViewMapGradientNormF1D type definition ------------------------------*/

PyTypeObject GetViewMapGradientNormF1D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"GetViewMapGradientNormF1D",    /* tp_name */
	sizeof(BPy_GetViewMapGradientNormF1D), /* tp_basicsize */
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
	"GetViewMapGradientNormF1D objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction1DDouble_Type,    /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)GetViewMapGradientNormF1D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int GetViewMapGradientNormF1D___init__( BPy_GetViewMapGradientNormF1D* self, PyObject *args)
{
	PyObject *obj = 0;
	int i;
	float f = 2.0;

	if( !PyArg_ParseTuple(args, "i|O!f", &i, &IntegrationType_Type, &obj, &f) )
		return -1;
	
	IntegrationType t = ( obj ) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
	self->py_uf1D_double.uf1D_double = new Functions1D::GetViewMapGradientNormF1D(i,t,f);
	return 0;

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
