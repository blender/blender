/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Group.h" /* This must come first */

#include "MEM_guardedalloc.h"

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
#include "gen_library.h"

#include "vector.h"

/* checks for the group being removed */
#define GROUP_DEL_CHECK_PY(bpy_group) if (!(bpy_group->group)) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "Group has been removed" ) )
#define GROUP_DEL_CHECK_INT(bpy_group) if (!(bpy_group->group)) return ( EXPP_ReturnIntError( PyExc_RuntimeError, "Group has been removed" ) )

/*****************************************************************************/
/* Python API function prototypes for the Blender module.		 */
/*****************************************************************************/
static PyObject *M_Group_New( PyObject * self, PyObject * args );
PyObject *M_Group_Get( PyObject * self, PyObject * args );
PyObject *M_Group_Unlink( PyObject * self, BPy_Group * pygrp );

/* internal */
static PyObject *GroupObSeq_CreatePyObject( BPy_Group *self, GroupObject *iter );

/*****************************************************************************/
/* Python method structure definition for Blender.Object module:	 */
/*****************************************************************************/
struct PyMethodDef M_Group_methods[] = {
	{"New", ( PyCFunction ) M_Group_New, METH_VARARGS,
	 "(name) Add a new empty group"},
	{"Get", ( PyCFunction ) M_Group_Get, METH_VARARGS,
"(name) - return the group with the name 'name',\
returns None if notfound.\nIf 'name' is not specified, it returns a list of all groups."},
	{"Unlink", ( PyCFunction ) M_Group_Unlink, METH_O,
	 "(group) - Unlink (delete) this group from Blender."},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Group methods table:					   */
/*****************************************************************************/
static PyObject *BPy_Group_copy( BPy_Group * self );

static PyMethodDef BPy_Group_methods[] = {
	/* name, method, flags, doc */
	{"__copy__", ( PyCFunction ) BPy_Group_copy, METH_VARARGS,
	 "() - Return a copy of the group containing the same objects."},
	{"copy", ( PyCFunction ) BPy_Group_copy, METH_VARARGS,
	 "() - Return a copy of the group containing the same objects."},
	{NULL, NULL, 0, NULL}
};


static PyObject *BPy_Group_copy( BPy_Group * self )
{
	BPy_Group *py_group;	/* for Group Data object wrapper in Python */
	struct Group *bl_group;
	GroupObject *group_ob, *group_ob_new; /* Group object, copied and added to the groups */
	
	GROUP_DEL_CHECK_PY(self);
	
	bl_group= add_group( self->group->id.name + 2 );
	
	if( bl_group )		/* now create the wrapper grp in Python */
		py_group = ( BPy_Group * ) Group_CreatePyObject( bl_group );
	else
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create Group Data in Blender" ) );
	
	bl_group->id.us = 1;
	
	/* Now add the objects to the group */
	group_ob= self->group->gobject.first;
	while(group_ob) {
		/* save time by not using */
		group_ob_new= MEM_callocN(sizeof(GroupObject), "groupobject");
		group_ob_new->ob= group_ob->ob;
		BLI_addtail( &bl_group->gobject, group_ob_new);
		group_ob= group_ob->next;
	}
	
	return ( PyObject * ) py_group;
	
}


/************************************************************************
 *
 * Python BPy_Object attributes
 *
 ************************************************************************/
static PyObject *Group_getObjects( BPy_Group * self )
{
	return GroupObSeq_CreatePyObject(self, NULL);
}


static void add_to_group_wraper(Group *group, Object *ob) {
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
static int Group_setObjects( BPy_Group * self, PyObject * args )
{
	int i, list_size;
	Group *group;
	Object *blen_ob;
	group= self->group;
	
	GROUP_DEL_CHECK_INT(self);
	
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
	} else if (PyIter_Check(args)) {
		PyObject *iterator = PyObject_GetIter(args);
		PyObject *item;
		if (iterator == NULL) {
			Py_DECREF(iterator);
			return EXPP_ReturnIntError( PyExc_TypeError, 
			"expected a list of objects, This iterator cannot be used." );
		}
		free_group(group); /* unlink all objects from this group, keep the group */
		item = PyIter_Next(iterator);
		while (item) {
			if ( PyObject_TypeCheck(item, &Object_Type) ) {
				blen_ob= ((BPy_Object *)item)->object;
				add_to_group_wraper(group, blen_ob);
			}
			Py_DECREF(item);
			item = PyIter_Next(iterator);
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

static PyObject *Group_getDupliOfs( BPy_Group * self )
{
	GROUP_DEL_CHECK_PY(self);
	return newVectorObject( self->group->dupli_ofs, 3, Py_WRAP );
}

static int Group_setDupliOfs( BPy_Group * self, PyObject * value )
{	
	VectorObject *bpy_vec;
	GROUP_DEL_CHECK_INT(self);
	if (!VectorObject_Check(value))
		return ( EXPP_ReturnIntError( PyExc_TypeError,
			"expected a vector" ) );
	
	bpy_vec = (VectorObject *)value;
	
	if (bpy_vec->size != 3)
		return ( EXPP_ReturnIntError( PyExc_ValueError,
			"can only assign a 3D vector" ) );
	
	self->group->dupli_ofs[0] = bpy_vec->vec[0];
	self->group->dupli_ofs[1] = bpy_vec->vec[1];
	self->group->dupli_ofs[2] = bpy_vec->vec[2];
	return 0;
}


/*****************************************************************************/
/* PythonTypeObject callback function prototypes			 */
/*****************************************************************************/
static PyObject *Group_repr( BPy_Group * obj );
static int Group_compare( BPy_Group * a, BPy_Group * b );

/*****************************************************************************/
/* Python BPy_Group getsetattr funcs:                                        */
/*****************************************************************************/
static int Group_setLayers( BPy_Group * self, PyObject * value )
{
	unsigned int laymask = 0;
	
	GROUP_DEL_CHECK_INT(self);
	
	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected an integer (bitmask) as argument" );
	
	laymask = ( unsigned int )PyInt_AS_LONG( value );
	
	if( laymask <= 0 )
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "layer value cannot be zero or below" );
	
	self->group->layer= laymask & ((1<<20) - 1);
	
	return 0;
}

static PyObject *Group_getLayers( BPy_Group * self )
{
	return PyInt_FromLong( self->group->layer );
}


/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_Group_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"layers",
	 (getter)Group_getLayers, (setter)Group_setLayers,
	 "layer mask for this group",
	 NULL},
	{"objects",
	 (getter)Group_getObjects, (setter)Group_setObjects,
	 "objects in this group",
	 NULL},
	{"dupliOffset",
	 (getter)Group_getDupliOfs, (setter)Group_setDupliOfs,
	 "offset to use when instancing this group as a DupliGroup",
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

	NULL,						/* destructor tp_dealloc; */
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

	( hashfunc ) GenericLib_hash,	/* hashfunc tp_hash; */
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
	BPy_Group *py_group;	/* for Group Data object wrapper in Python */
	struct Group *bl_group;
	
	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"string expected as argument or nothing" );
	
	bl_group= add_group( name );
	
	if( bl_group )		/* now create the wrapper grp in Python */
		py_group = ( BPy_Group * ) Group_CreatePyObject( bl_group );
	else
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create Group Data in Blender" ) );
	
	bl_group->id.us = 1;
	
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

			if( !pyobj ) {
				Py_DECREF(grouplist);
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create Object" ) );
			}
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
PyObject *M_Group_Unlink( PyObject * self, BPy_Group * pygrp )
{
	Group *group;
	if( !BPy_Group_Check(pygrp) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected a group" ) );
	
	GROUP_DEL_CHECK_PY(pygrp);
	
	group= pygrp->group;
	
	pygrp->group= NULL;
	free_group(group);
	unlink_group(group);
	group->id.us= 0;
	free_libblock( &G.main->group, group );
	Py_RETURN_NONE;
}


/*****************************************************************************/
/* Function:	 initObject						*/
/*****************************************************************************/
PyObject *Group_Init( void )
{
	PyObject *submodule;
	if( PyType_Ready( &Group_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &GroupObSeq_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Group", M_Group_methods,
				 "The Blender Group module\n\n\
This module provides access to **Group Data** in Blender.\n" );

	/*Add SUBMODULES to the module*/
	/*PyDict_SetItemString(dict, "Constraint", Constraint_Init()); //creates a *new* module*/
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
		Py_RETURN_NONE;

	pygrp =
		( BPy_Group * ) PyObject_NEW( BPy_Group, &Group_Type );

	if( pygrp == NULL ) {
		return ( NULL );
	}
	pygrp->group = grp;
	return ( ( PyObject * ) pygrp );
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
	if (!self->group)
		return PyString_FromString( "[Group - Removed]" );
	
	return PyString_FromFormat( "[Group \"%s\"]",
				    self->group->id.name + 2 );
}


/************************************************************************
 *
 * GroupOb sequence 
 *
 ************************************************************************/
/*
 * create a thin GroupOb object
 */

static PyObject *GroupObSeq_CreatePyObject( BPy_Group *self, GroupObject *iter )
{
	BPy_GroupObSeq *seq = PyObject_NEW( BPy_GroupObSeq, &GroupObSeq_Type);
	seq->bpygroup = self; Py_INCREF(self);
	seq->iter= iter;
	return (PyObject *)seq;
}


static int GroupObSeq_len( BPy_GroupObSeq * self )
{
	GROUP_DEL_CHECK_INT(self->bpygroup);
	return BLI_countlist( &( self->bpygroup->group->gobject ) );
}

/*
 * retrive a single GroupOb from somewhere in the GroupObex list
 */

static PyObject *GroupObSeq_item( BPy_GroupObSeq * self, int i )
{
	Group *group= self->bpygroup->group;
	int index=0;
	PyObject *bpy_obj;
	GroupObject *gob;
	
	GROUP_DEL_CHECK_PY(self->bpygroup);
	
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

static PySequenceMethods GroupObSeq_as_sequence = {
	( inquiry ) GroupObSeq_len,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) GroupObSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	0,0,0,
};

/************************************************************************
 *
 * Python GroupObSeq_Type iterator (iterates over GroupObjects)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *GroupObSeq_getIter( BPy_GroupObSeq * self )
{
	GROUP_DEL_CHECK_PY(self->bpygroup);
	
	if (!self->iter) {
		self->iter = self->bpygroup->group->gobject.first;
		return EXPP_incr_ret ( (PyObject *) self );
	} else {
		return GroupObSeq_CreatePyObject(self->bpygroup, self->bpygroup->group->gobject.first);
	}
}

/*
 * Return next GroupOb.
 */

static PyObject *GroupObSeq_nextIter( BPy_GroupObSeq * self )
{
	PyObject *object;
	if( !(self->iter) ||  !(self->bpygroup->group) ) {
		self->iter = NULL; /* so we can add objects again */
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	object= Object_CreatePyObject( self->iter->ob ); 
	self->iter= self->iter->next;
	return object;
}


static PyObject *GroupObSeq_link( BPy_GroupObSeq * self, BPy_Object *value )
{
	Object *blen_ob;
	
	GROUP_DEL_CHECK_PY(self->bpygroup);
	
	if( !BPy_Object_Check(value) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a python object as an argument" ) );
	
	/*
	if (self->iter != NULL)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Cannot modify group objects while iterating" );
	*/
	
	blen_ob = value->object;
	
	add_to_group_wraper(self->bpygroup->group, blen_ob); /* this checks so as not to add the object into the group twice*/
	
	Py_RETURN_NONE;
}

static PyObject *GroupObSeq_unlink( BPy_GroupObSeq * self, BPy_Object *value )
{
	Object *blen_ob;
	Base *base= NULL;
	
	GROUP_DEL_CHECK_PY(self->bpygroup);
	
	if( !BPy_Object_Check(value) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a python object as an argument" ) );
	
	blen_ob = value->object;
	

	
	rem_from_group(self->bpygroup->group, blen_ob);
	
	if(find_group(blen_ob, NULL)==NULL) {
		blen_ob->flag &= ~OB_FROMGROUP;
		
		base= object_in_scene(blen_ob, G.scene);
		if (base)
			base->flag &= ~OB_FROMGROUP;
	}
	Py_RETURN_NONE;
}

static struct PyMethodDef BPy_GroupObSeq_methods[] = {
	{"link", (PyCFunction)GroupObSeq_link, METH_O,
		"make the object a part of this group"},
	{"unlink", (PyCFunction)GroupObSeq_unlink, METH_O,
		"unlink an object from this group"},
	{NULL, NULL, 0, NULL}	
};

/************************************************************************
 *
 * Python GroupObSeq_Type standard operations
 *
 ************************************************************************/

static void GroupObSeq_dealloc( BPy_GroupObSeq * self )
{
	Py_DECREF(self->bpygroup);
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Python GroupObSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject GroupObSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender GroupObSeq",           /* char *tp_name; */
	sizeof( BPy_GroupObSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) GroupObSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&GroupObSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
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
	( getiterfunc) GroupObSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) GroupObSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_GroupObSeq_methods,       /* struct PyMethodDef *tp_methods; */
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
