#include "BPy_GetDirectionalViewMapDensityF1D.h"

#include "../../../stroke/AdvancedFunctions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for GetDirectionalViewMapDensityF1D instance  -----------*/
static int GetDirectionalViewMapDensityF1D___init__(BPy_GetDirectionalViewMapDensityF1D* self, PyObject *args);

/*-----------------------BPy_GetDirectionalViewMapDensityF1D type definition ------------------------------*/

PyTypeObject GetDirectionalViewMapDensityF1D_Type = {
	PyObject_HEAD_INIT(NULL)
	"GetDirectionalViewMapDensityF1D", /* tp_name */
	sizeof(BPy_GetDirectionalViewMapDensityF1D), /* tp_basicsize */
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
	"GetDirectionalViewMapDensityF1D objects", /* tp_doc */
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
	(initproc)GetDirectionalViewMapDensityF1D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int GetDirectionalViewMapDensityF1D___init__( BPy_GetDirectionalViewMapDensityF1D* self, PyObject *args)
{
	PyObject *obj = 0;
	unsigned int u1, u2;
	float f = 2.0;

	if( !PyArg_ParseTuple(args, "II|O!f", &u1, &u2, &IntegrationType_Type, &obj, &f) )
		return -1;
	
	IntegrationType t = ( obj ) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
	self->py_uf1D_double.uf1D_double = new Functions1D::GetDirectionalViewMapDensityF1D(u1, u2, t, f);
	return 0;

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
