#include "Pose.h"
#include "NLA.h" 
//Action is still there, may move to Action.h
// Action_Type used for typechecking
#include "Types.h"
#include <Python.h>
//#include <DNA_action_types.h>
#include "gen_utils.h"
#include "BKE_action.h"

static void Pose_dealloc( PyObject *self );
static PyObject *Pose_repr( BPy_Pose *self );
static PyObject *Pose_fromAction( BPy_Pose *self, PyObject *args);

static struct PyMethodDef Pose_methods[] = {
	{"fromAction", ( PyCFunction ) Pose_fromAction,
	 METH_VARARGS, "() - sets the pose based on an action and given time in it."},
	{0, 0, 0, 0}
};

static PyGetSetDef BPy_Pose_getsetters[] = {
//		{"type",(getter)Key_getType, (setter)NULL,
		{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
	};

PyTypeObject Pose_Type = {
	PyObject_HEAD_INIT( NULL ) 0,	/*ob_size */
	"Blender Pose",					/*tp_name */
	sizeof( BPy_Pose ),				/*tp_basicsize */
	0,								/*tp_itemsize */
	/* methods */
	( destructor ) Pose_dealloc,		/*tp_dealloc */
	( printfunc ) 0,				/*tp_print */
	( getattrfunc ) 0,	/*tp_getattr */
	( setattrfunc ) 0,			 	/*tp_setattr */
	0, 								/*tp_compare*/
	( reprfunc ) Pose_repr, 			/* tp_repr */
	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	Pose_methods,           		/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Pose_getsetters,     	/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                      	/* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

static void Pose_dealloc( PyObject * self )
{
	PyObject_DEL( self );
}

PyObject *Pose_CreatePyObject( struct bPose * pose )
	{
		BPy_Pose *pypose;
	
		pypose =
			( BPy_Pose * ) PyObject_NEW( BPy_Pose, &Pose_Type );
	
		if( !pypose )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
						      "couldn't create BPy_Pose object" );

		PyType_Ready(&Pose_Type);

		pypose->pose = pose;
		return ( ( PyObject * ) pypose );
	}

static PyObject *Pose_fromAction( BPy_Pose *self, PyObject *args)  {
	BPy_Action *action;
	float time;
	float factor = 1.0;

	if (!PyArg_ParseTuple(args, "O!f|f", &Action_Type, &action, &time, &factor))
				return EXPP_ReturnPyObjError(PyExc_AttributeError, "An action and a time (as number) required as arguments.");

	//todo: range check for time

	extract_pose_from_action(self->pose, action->action, time);

	Py_RETURN_NONE;
}

static PyObject *Pose_repr( BPy_Pose * self )
{
	return PyString_FromFormat( "[Pose]"); // \"%s\"]", self->key->id.name + 2 );
}
