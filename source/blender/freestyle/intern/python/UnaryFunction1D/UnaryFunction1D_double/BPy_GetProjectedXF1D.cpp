#include "BPy_GetProjectedXF1D.h"

#include "../../../view_map/Functions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char GetProjectedXF1D___doc__[] =
".. method:: __init__(iType=IntegrationType.MEAN)\n"
"\n"
"   Builds a GetProjectedXF1D object.\n"
"\n"
"   :arg iType: The integration method used to compute a single value\n"
"      from a set of values. \n"
"   :type iType: :class:`IntegrationType`\n"
"\n"
".. method:: __call__(inter)\n"
"\n"
"   Returns the projected X 3D coordinate of an Interface1D.\n"
"\n"
"   :arg inter: An Interface1D object.\n"
"   :type inter: :class:`Interface1D`\n"
"   :return: The projected X 3D coordinate of an Interface1D.\n"
"   :rtype: float\n";

static int GetProjectedXF1D___init__( BPy_GetProjectedXF1D* self, PyObject *args )
{
	PyObject *obj = 0;

	if( !PyArg_ParseTuple(args, "|O!", &IntegrationType_Type, &obj) )
		return -1;
	
	IntegrationType t = ( obj ) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
	self->py_uf1D_double.uf1D_double = new Functions1D::GetProjectedXF1D(t);
	return 0;
}


/*-----------------------BPy_GetProjectedXF1D type definition ------------------------------*/

PyTypeObject GetProjectedXF1D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"GetProjectedXF1D",             /* tp_name */
	sizeof(BPy_GetProjectedXF1D),   /* tp_basicsize */
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
	GetProjectedXF1D___doc__,       /* tp_doc */
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
	(initproc)GetProjectedXF1D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
