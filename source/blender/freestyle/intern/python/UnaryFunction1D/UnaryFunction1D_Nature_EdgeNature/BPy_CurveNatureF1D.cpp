#include "BPy_CurveNatureF1D.h"

#include "../../../view_map/Functions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char CurveNatureF1D___doc__[] =
".. method:: __init__(iType=IntegrationType.MEAN)\n"
"\n"
"   Builds a CurveNatureF1D object.\n"
"\n"
"   :arg iType: The integration method used to compute a single value\n"
"      from a set of values.\n"
"   :type iType: :class:`IntegrationType`\n"
"\n"
".. method:: __call__(inter)\n"
"\n"
"   Returns the nature of the Interface1D (silhouette, ridge, crease,\n"
"   and so on).  Except if the Interface1D is a :class:`ViewEdge`, this\n"
"   result might be ambiguous.  Indeed, the Interface1D might result\n"
"   from the gathering of several 1D elements, each one being of a\n"
"   different nature.  An integration method, such as the MEAN, might\n"
"   give, in this case, irrelevant results.\n"
"\n"
"   :arg inter: An Interface1D object.\n"
"   :type inter: :class:`Interface1D`\n"
"   :return: The nature of the Interface1D.\n"
"   :rtype: :class:`Nature`\n";

static int CurveNatureF1D___init__( BPy_CurveNatureF1D* self, PyObject *args)
{
	PyObject *obj = 0;

	if( !PyArg_ParseTuple(args, "|O!", &IntegrationType_Type, &obj) )
		return -1;
	
	IntegrationType t = ( obj ) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
	self->py_uf1D_edgenature.uf1D_edgenature = new Functions1D::CurveNatureF1D(t);
	return 0;
}

/*-----------------------BPy_CurveNatureF1D type definition ------------------------------*/

PyTypeObject CurveNatureF1D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"CurveNatureF1D",               /* tp_name */
	sizeof(BPy_CurveNatureF1D),     /* tp_basicsize */
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
	CurveNatureF1D___doc__,         /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction1DEdgeNature_Type, /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)CurveNatureF1D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
