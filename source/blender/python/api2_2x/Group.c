#include "Group.h" /* This must come first */

#include "DNA_group_types.h"
#include "DNA_scene_types.h" /* for Base */

#include "BKE_mesh.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_group.h"

#include "BLI_blenlib.h"

#include "blendef.h"
#include "Object.h"
#include "gen_utils.h"


/*****************************************************************************/
/* Python API function prototypes for the Blender module.		 */
/*****************************************************************************/
static PyObject *M_Group_New( PyObject * self, PyObject * args );
PyObject *M_Group_Get( PyObject * self, PyObject * args );
PyObject *M_Group_Unlink( PyObject * self, PyObject * args );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.	 */
/* In Python these will be written to the console when doing a		 */
/* Blender.Group.__doc__						 */
/*****************************************************************************/
char M_Group_doc[] = "The Blender Group module\n\n\
This module provides access to **Group Data** in Blender.\n";

char M_Group_New_doc[] =
	"(name) Add a new empty group";

char M_Group_Get_doc[] =
	"(name) - return the group with the name 'name', returns None if not\
	found.\n\
	If 'name' is not specified, it returns a list of all groups.";
	
char M_Group_Unlink_doc[] =
"(group) - Unlink (delete) this group from Blender.";

/*****************************************************************************/
/* Python method structure definition for Blender.Object module:	 */
/*****************************************************************************/
struct PyMethodDef M_Group_methods[] = {
	{"New", ( PyCFunction ) M_Group_New, METH_VARARGS,
	 M_Group_New_doc},
	{"Get", ( PyCFunction ) M_Group_Get, METH_VARARGS,
	 M_Group_Get_doc},
	{"Unlink", ( PyCFunction ) M_Group_Unlink, METH_VARARGS,
	 M_Group_Unlink_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Group methods table:					   */
/*****************************************************************************/
static PyMethodDef BPy_Group_methods[] = {
	/* name, method, flags, doc */
	{NULL, NULL, 0, NULL}
};


/************************************************************************
 *
 * Python BPy_Object attributes
 *
 ************************************************************************/
static PyObject *M_Group_getObjects( BPy_Group * self )
{
	BPy_MGroupObSeq *seq = PyObject_NEW( BPy_MGroupObSeq, &MGroupObSeq_Type);
	seq->bpygroup = self; Py_INCREF(self);
	return (PyObject *)seq;
}


void add_to_group_wraper(Group *group, Object *ob) {
	Base *base;
	add_to_group(group, ob);
	
	if (!(ob->flag & OB_FROMGROUP)) { /* do this to avoid a listbase lookup */
		ob->flag |= OB_FROMGROUP;
		
		base= object_in_scene(ob, G.scene);
		if (base)
			base->flag |= OB_FROMGROUP;
	}
}

/* only for internal use Blender.Group.Get("MyGroup").objects= []*/
static int M_Group_setObjects( BPy_Group * self, PyObject * args )
{
	int i, list_size;
	Group *group;
	Object *blen_ob;
	group= self->group;
	
	if( PyList_Check( args ) ) {
		if( EXPP_check_sequence_consistency( args, &Object_Type ) != 1)
			return ( EXPP_ReturnIntError( PyExc_TypeError, 
					"expected a list of objects" ) );
		
		/* remove all from the list and add the new items */
		free_group(group); /* unlink all objects from this group, keep the group */
		list_size= PyList_Size( args );
		for( i = 0; i < list_size; i++ ) {
			blen_ob= ((BPy_Object *)PyList_GET_ITEM( args, i ))->object;
			add_to_group_wraper(group, blen_ob);
		}
	/*
	} else if( args->ob_type == &MGroupObSeq_Type ) {
	*/
		/* todo, handle sequences here */
	
	} else if (PyIter_Check(args)) {
		PyObject *iterator = PyObject_GetIter(args);
		PyObject *item;
		if (iterator == NULL) {
			Py_DECREF(iterator);
			return EXPP_ReturnIntError( PyExc_TypeError, 
			"expected a list of objects, This iterator cannot be used." );
		}
		free_group(group); /* unlink all objects from this group, keep the group */
		while ((item = PyIter_Next(iterator))) {
			if ( PyObject_TypeCheck(item, &Object_Type) ) {
				blen_ob= ((BPy_Object *)item)->object;
				add_to_group_wraper(group, blen_ob);
			}
			Py_DECREF(item);
		}

		Py_DECREF(iterator);

		if (PyErr_Occurred()) {
			return EXPP_ReturnIntError( PyExc_RuntimeError, 
			"An unknown error occured while adding iterator objects to the group.\nThe group has been modified." );
		}

	} else
		return EXPP_ReturnIntError( PyExc_TypeError, 
				"expected a list or sequence of objects" );
	return 0;
}



/*****************************************************************************/
/* PythonTypeObject callback function prototypes			 */
/*****************************************************************************/
static void Group_dealloc( BPy_Group * obj );
static PyObject *Group_repr( BPy_Group * obj );
static int Group_compare( BPy_Group * a, BPy_Group * b );

/*****************************************************************************/
/* Python BPy_Group methods:                                                  */
/*****************************************************************************/
static int Group_setName( BPy_Group * self, PyObject * value )
{
	char *name = NULL;
	char buf[21];
	
	if( !(self->group) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					      "Blender Group was deleted!" );
	
	name = PyString_AsString ( value );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	rename_id( &self->group->id, buf );

	return 0;
}


static PyObject *Group_getName( BPy_Group * self, PyObject * args )
{
	PyObject *attr;
	if( !(self->group) )
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Group was deleted!" ) );
	
	attr = PyString_FromString( self->group->id.name + 2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Group.name attribute" ) );
}

static PyObject *Group_getUsers( BPy_Group * self )
{
	return PyInt_FromLong( self->group->id.us );
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_Group_getseters[] = {
	{"name",
	 (getter)Group_getName, (setter)Group_setName,
	 "Group name",
	 NULL},
	{"users",
	 (getter)Group_getUsers, (setter)NULL,
	 "Number of group users",
	 NULL},
	{"objects",
	 (getter)M_Group_getObjects, (setter)M_Group_setObjects,
	 "objects in this group",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python TypeGroup structure definition:                                     */
/*****************************************************************************/
PyTypeObject Group_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Group",             /* char *tp_name; */
	sizeof( BPy_Group ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) Group_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Group_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) Group_repr,     /* reprfunc tp_repr; */

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
	BPy_Group_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Group_getseters,         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
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





/*****************************************************************************/
/* Function:			  M_Group_New				 */
/* Python equivalent:	  Blender.Group.New				 */
/*****************************************************************************/
PyObject *M_Group_New( PyObject * self, PyObject * args )
{
	char *name = "Group";
	char buf[21];
	BPy_Group *py_group;	/* for Group Data object wrapper in Python */
	struct Group *bl_group;
	
	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"string expected as argument" );
	
	bl_group= add_group();
	
	if( bl_group )		/* now create the wrapper grp in Python */
		py_group = ( BPy_Group * ) Group_CreatePyObject( bl_group );
	else
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create Group Data in Blender" ) );
	
	
	if( strcmp( name, "Group" ) != 0 ) {
		PyOS_snprintf( buf, sizeof( buf ), "%s", name );
		rename_id( &bl_group->id, buf );
	}
	
	/* user count be incremented in Group_CreatePyObject */
	bl_group->id.us = 0;
	
	return ( PyObject * ) py_group;
}

/*****************************************************************************/
/* Function:	  M_Group_Get						*/
/* Python equivalent:	  Blender.Group.Get				*/
/*****************************************************************************/
PyObject *M_Group_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Group *group_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	group_iter = G.main->group.first;

	if( name ) {		/* (name) - Search group by name */

		BPy_Group *wanted_group = NULL;

		while( ( group_iter ) && ( wanted_group == NULL ) ) {

			if( strcmp( name, group_iter->id.name + 2 ) == 0 )
				wanted_group =
					( BPy_Group * )
					Group_CreatePyObject( group_iter );

			group_iter = group_iter->id.next;
		}

		if( wanted_group == NULL ) { /* Requested group doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Group \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) wanted_group;
	}

	else {		/* () - return a list of all groups in the scene */
		int index = 0;
		PyObject *grouplist, *pyobj;

		grouplist = PyList_New( BLI_countlist( &( G.main->group ) ) );

		if( grouplist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create group list" ) );

		while( group_iter ) {
			pyobj = Group_CreatePyObject( group_iter );

			if( !pyobj )
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create Object" ) );

			PyList_SET_ITEM( grouplist, index, pyobj );

			group_iter = group_iter->id.next;
			index++;
		}

		return grouplist;
	}
}


/*****************************************************************************/
/* Function:	  M_Group_Unlink						*/
/* Python equivalent:	  Blender.Group.Unlink				*/
/*****************************************************************************/
PyObject *M_Group_Unlink( PyObject * self, PyObject * args )
{
	PyObject *pyob=NULL;
	BPy_Group *pygrp=NULL;
	Group *group;
	if( !PyArg_ParseTuple( args, "O!", &Group_Type, &pyob) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected a group" ) );
	
	pygrp= (BPy_Group *)pyob;
	group= pygrp->group;
	
	if( !group )
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Group was deleted!" ) );
	
	pygrp->group= NULL;
	free_group(group);
	unlink_group(group);
	group->id.us= 0;
	free_libblock( &G.main->group, group );
	Py_INCREF( Py_None );
	return Py_None;
}


/*****************************************************************************/
/* Function:	 initObject						*/
/*****************************************************************************/
PyObject *Group_Init( void )
{
	PyObject *submodule;
	if( PyType_Ready( &Group_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MGroupObSeq_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Group", M_Group_methods,
				 M_Group_doc );

	//Add SUBMODULES to the module
	//PyDict_SetItemString(dict, "Constraint", Constraint_Init()); //creates a *new* module
	return submodule;
}


/*****************************************************************************/
/* Function:	Group_CreatePyObject					 */
/* Description: This function will create a new BlenObject from an existing  */
/*		Object structure.					 */
/*****************************************************************************/
PyObject *Group_CreatePyObject( struct Group * grp )
{
	BPy_Group *pygrp;

	if( !grp )
		return EXPP_incr_ret( Py_None );

	pygrp =
		( BPy_Group * ) PyObject_NEW( BPy_Group, &Group_Type );

	if( pygrp == NULL ) {
		return ( NULL );
	}
	pygrp->group = grp;
	return ( ( PyObject * ) pygrp );
}

/*****************************************************************************/
/* Function:	Group_CheckPyObject					 */
/* Description: This function returns true when the given PyObject is of the */
/*		type Group. Otherwise it will return false.		 */
/*****************************************************************************/
int Group_CheckPyObject( PyObject * py_grp)
{
	return ( py_grp->ob_type == &Group_Type );
}

/*****************************************************************************/
/* Function:	Group_FromPyObject					 */
/* Description: This function returns the Blender group from the given	 */
/*		PyObject.						 */
/*****************************************************************************/
Group *Group_FromPyObject( PyObject * py_grp )
{
	BPy_Group *blen_grp;

	blen_grp = ( BPy_Group * ) py_grp;
	return ( blen_grp->group );
}

/*****************************************************************************/
/* Description: Returns the object with the name specified by the argument  */
/*		name. Note that the calling function has to remove the first */
/*		two characters of the object name. These two characters	   */
/*		specify the type of the object (OB, ME, WO, ...)	 */
/*		The function will return NULL when no object with the given  */
/*		name is found.						 */
/*****************************************************************************/
Group *GetGroupByName( char *name )
{
	Group *grp_iter;

	grp_iter = G.main->group.first;
	while( grp_iter ) {
		if( StringEqual( name, GetIdName( &( grp_iter->id ) ) ) ) {
			return ( grp_iter );
		}
		grp_iter = grp_iter->id.next;
	}

	/* There is no object with the given name */
	return ( NULL );
}

/*****************************************************************************/
/* Function:	Group_dealloc						 */
/* Description: This is a callback function for the BlenObject type. It is  */
/*		the destructor function.				 */
/*****************************************************************************/
static void Group_dealloc( BPy_Group * grp )
{
	PyObject_DEL( grp );
}

/*****************************************************************************/
/* Function:	Group_compare						 */
/* Description: This is a callback function for the BPy_Group type. It	 */
/*		compares two Group_Type objects. Only the "==" and "!="  */
/*		comparisons are meaninful. Returns 0 for equality and -1 if  */
/*		they don't point to the same Blender Object struct.	 */
/*		In Python it becomes 1 if they are equal, 0 otherwise.	 */
/*****************************************************************************/
static int Group_compare( BPy_Group * a, BPy_Group * b )
{
	Group *pa = a->group, *pb = b->group;
	return ( pa == pb ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:	Group_repr						 */
/* Description: This is a callback function for the BPy_Group type. It	 */
/*		builds a meaninful string to represent object objects.	 */
/*****************************************************************************/
static PyObject *Group_repr( BPy_Group * self )
{
	return PyString_FromFormat( "[Group \"%s\"]",
				    self->group->id.name + 2 );
}


/************************************************************************
 *
 * GroupOb sequence 
 *
 ************************************************************************/
/*
 * create a thin MGroupOb object
 */

static PyObject *MGroupObSeq_CreatePyObject( Group *group, int i )
{
	int index=0;
	PyObject *bpy_obj;
	GroupObject *gob;
	
	for (gob= group->gobject.first; gob && i!=index; gob= gob->next, index++) {}
	
	if (!(gob))
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );
	
	bpy_obj = Object_CreatePyObject( gob->ob );

	if( !bpy_obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	return (PyObject *)bpy_obj;
}


static int MGroupObSeq_len( BPy_MGroupObSeq * self )
{
	return BLI_countlist( &( self->bpygroup->group->gobject ) );
}

/*
 * retrive a single MGroupOb from somewhere in the GroupObex list
 */

static PyObject *MGroupObSeq_item( BPy_MGroupObSeq * self, int i )
{
	return MGroupObSeq_CreatePyObject( self->bpygroup->group, i );
}

static PySequenceMethods MGroupObSeq_as_sequence = {
	( inquiry ) MGroupObSeq_len,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) MGroupObSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	0,0,0,
};

/************************************************************************
 *
 * Python MGroupObSeq_Type iterator (iterates over GroupObjects)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MGroupObSeq_getIter( BPy_MGroupObSeq * self )
{
	self->iter = self->bpygroup->group->gobject.first;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MGroupOb.
 */

static PyObject *MGroupObSeq_nextIter( BPy_MGroupObSeq * self )
{
	PyObject *object;
	if( !(self->iter) ||  !(self->bpygroup->group) )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	
	object= Object_CreatePyObject( self->iter->ob ); 
	self->iter= self->iter->next;
	return object;
}


static PyObject *MGroupObSeq_append( BPy_MGroupObSeq * self, PyObject *args )
{
	PyObject *pyobj;
	Object *blen_ob;
	Base *base= NULL;
	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &pyobj ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a python object as an argument" ) );
	
	blen_ob = ( ( BPy_Object * ) pyobj )->object;
	
	base= object_in_scene(blen_ob, G.scene);
	
	add_to_group_wraper(self->bpygroup->group, blen_ob); /* this checks so as not to add the object into the group twice*/
	
	return EXPP_incr_ret( Py_None );
}



static PyObject *MGroupObSeq_remove( BPy_MGroupObSeq * self, PyObject *args )
{
	PyObject *pyobj;
	Object *blen_ob;
	Base *base= NULL;
	
	if( !(self->bpygroup->group) )
		return (EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Group was deleted!" ));
	
	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &pyobj ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a python object as an argument" ) );
	
	blen_ob = ( ( BPy_Object * ) pyobj )->object;
	

	
	rem_from_group(self->bpygroup->group, blen_ob);
	
	if(find_group(blen_ob)==NULL) {
		blen_ob->flag &= ~OB_FROMGROUP;
		
		base= object_in_scene(blen_ob, G.scene);
		if (base)
			base->flag &= ~OB_FROMGROUP;
	}
	return EXPP_incr_ret( Py_None );
}


static struct PyMethodDef BPy_MGroupObSeq_methods[] = {
	{"append", (PyCFunction)MGroupObSeq_append, METH_VARARGS,
		"add object to group"},
	{"remove", (PyCFunction)MGroupObSeq_remove, METH_VARARGS,
		"remove object from group"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python MGroupObSeq_Type standard operations
 *
 ************************************************************************/

static void MGroupObSeq_dealloc( BPy_MGroupObSeq * self )
{
	Py_DECREF(self->bpygroup);
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Python MGroupObSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject MGroupObSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MGroupObSeq",           /* char *tp_name; */
	sizeof( BPy_MGroupObSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MGroupObSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MGroupObSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
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
	( getiterfunc) MGroupObSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MGroupObSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MGroupObSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
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
