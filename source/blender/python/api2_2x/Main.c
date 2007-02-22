/* 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/


#include "MEM_guardedalloc.h"	/* for MEM_callocN */
#include "DNA_space_types.h"	/* SPACE_VIEW3D, SPACE_SEQ */
#include "DNA_scene_types.h"
#include "DNA_object_types.h" /* MainSeq_new */
#include "DNA_texture_types.h"
#include "DNA_ipo_types.h"
#include "DNA_group_types.h"
#include "DNA_world_types.h"
#include "DNA_vfont_types.h"
#include "DNA_armature_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_action_types.h"
#include "DNA_meta_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_sca.h" /*free_text_controllers*/
#include "BKE_font.h"
#include "BKE_mball.h" /* add_mball */
#include "BKE_mesh.h" /* add_mesh */
#include "BKE_curve.h" /* add_curve */
#include "BKE_material.h"
#include "BKE_group.h"

#include "BLI_blenlib.h" /* BLI_countlist */
#include "BIF_drawscene.h"	/* for set_scene */
#include "BIF_screen.h"		/* curarea */
#include "BIF_drawimage.h" /* what image */
#include "BIF_drawtext.h" /* unlink_text */

/* python types */
#include "../BPY_extern.h" /* clearing scriptlinks */

#include "gen_utils.h"

#include "Object.h"
#include "Camera.h"
#include "Armature.h"
#include "Lamp.h"
#include "CurNurb.h"
#include "NMesh.h"
#include "Mesh.h"
#include "Lattice.h"
#include "Metaball.h"
#include "Text3d.h"
#include "Font.h"
#include "Group.h"
#include "World.h"
#include "Texture.h"
#include "Ipo.h"
#include "Text.h"
#include "Sound.h"
#include "NLA.h"
#include "Main.h"
#include "Scene.h"

static PyObject *MainSeq_CreatePyObject( Link *iter, int type )
{
	BPy_MainSeq *seq = PyObject_NEW( BPy_MainSeq, &MainSeq_Type);
	seq->iter = iter;
	seq->type = type;
	return (PyObject *)seq;
}

static PyObject *Link_as_BPyData( Link *link, short type )
{
	switch (type) {
	case ID_SCE:
		return Scene_CreatePyObject( ( Scene *) link );
		break;
	case ID_OB:
		return Object_CreatePyObject( (Object *) link );
		break;
	case ID_ME:
		return Mesh_CreatePyObject( (Mesh *)link, NULL );
		break;
	case ID_CU: /*todo, support curnurbs?*/
		return Curve_CreatePyObject((Curve *)link);
		break;
	case ID_MB:
		return Metaball_CreatePyObject((MetaBall *)link);
		break;
	case ID_MA:
		return Material_CreatePyObject((Material *)link);
		break;
	case ID_TE:
		return Texture_CreatePyObject((Tex *)link);
		break;
	case ID_IM:
		return Image_CreatePyObject((Image *)link);
		break;
	case ID_LT:
		return Lattice_CreatePyObject((Lattice *)link);
		break;
	case ID_LA:
		return Lamp_CreatePyObject((Lamp *)link);
		break;
	case ID_CA:
		return Camera_CreatePyObject((Camera *)link);
		break;
	case ID_IP:
		return Ipo_CreatePyObject((Ipo *)link);
		break;
	case ID_WO:
		return World_CreatePyObject((World *)link);
		break;
	case ID_VF:
		return Font_CreatePyObject((VFont *)link);
		break;
	case ID_TXT:
		return Text_CreatePyObject((Text *)link);
		break;
	case ID_SO:
		return Sound_CreatePyObject((bSound *)link);
		break;
	case ID_GR:
		return Group_CreatePyObject((Group *)link);
		break;
	case ID_AR:
		return Armature_CreatePyObject((bArmature *)link);
		break;
	case ID_AC:
		return Action_CreatePyObject((bAction *)link);
		break;
	}
	Py_RETURN_NONE;
}

static int MainSeq_len( BPy_MainSeq * self )
{
	ListBase *lb = wich_libbase(G.main, self->type);
	return BLI_countlist( lb );
}

static PyObject * MainSeq_subscript(BPy_MainSeq * self, PyObject *key)
{
	char *name;
	char *lib= NULL;
	char use_lib = 0;
	ID *id;
	
	id = (ID *)wich_libbase(G.main, self->type)->first;
	
	if ( PyString_Check(key) ) {
		name = PyString_AsString ( key );
	} else if (PyTuple_Check(key) && (PyTuple_Size(key) == 2) ) {
		PyObject *pydata;
		use_lib = 1;
		
		/* Get the first arg */
		pydata = PyTuple_GET_ITEM(key, 0);
		if (!PyString_Check(pydata)) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"the data name must be a string" );
		}
		
		name = PyString_AsString ( pydata );
		
		/* Get the second arg */
		pydata = PyTuple_GET_ITEM(key, 1);
		if (pydata == Py_None) {
			lib = NULL; /* data must be local */
		} else if (PyString_Check(pydata)) {
			lib = PyString_AsString ( pydata );
			if (!strcmp( "", lib)) {
				lib = NULL; /* and empty string also means data must be local */
			}
		} else {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"the lib name must be a string or None" );
		}
	} else {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected the a name string or a tuple (lib, name)" );
	}
	
	for (; id; id = id->next) {
		if(!strcmp( name, id->name+2 )) {
			if (
				(!use_lib) || /* any data, local or external lib data */
				(use_lib && !lib && !id->lib) || /* only local */
				(lib && use_lib && id->lib && (!strcmp( id->lib->name, lib))) /* only external lib */
			)
			{
				return Link_as_BPyData((Link *)id, self->type);
			}
		}
	}
	return ( EXPP_ReturnPyObjError
				 ( PyExc_KeyError, "Requested data does not exist") );
}

static PyMappingMethods MainSeq_as_mapping = {
	( inquiry ) MainSeq_len,	/* mp_length */
	( binaryfunc ) MainSeq_subscript,	/* mp_subscript */
	( objobjargproc ) 0,	/* mp_ass_subscript */
};


/************************************************************************
 *
 * Python MainSeq_Type iterator (iterates over GroupObjects)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MainSeq_getIter( BPy_MainSeq * self )
{
	/* we need to get the first base, but for selected context we may need to advance
	to the first selected or first conext base */
	
	ListBase *lb;
	Link *link;
	lb = wich_libbase(G.main, self->type);
	
	link = lb->first;
	
	/* create a new iterator if were alredy using this one */
	if (self->iter==NULL) {
		self->iter = link;
		return EXPP_incr_ret ( (PyObject *) self );
	} else {
		return MainSeq_CreatePyObject(link, self->type);
	}
}

/*
 * Return next MainOb.
 */

static PyObject *MainSeq_nextIter( BPy_MainSeq * self )
{
	PyObject *object;
	Link *link;
	if( !(self->iter) ) {
		self->iter= NULL;
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	object = Link_as_BPyData(self->iter, self->type);
	
	link= self->iter->next;
	self->iter= link;
	return object;
}

PyObject *MainSeq_getActive(BPy_MainSeq *self)
{
	switch (self->type) {
	case ID_SCE:
		if ( !G.scene ) {
			Py_RETURN_NONE;
		} else {
			return Scene_CreatePyObject( ( Scene * ) G.scene );
		}
		
		break;
	case ID_IM:
		if (!G.sima || !G.sima->image) {
			Py_RETURN_NONE;
		} else {
			what_image( G.sima );	/* make sure image data exists */
			return Image_CreatePyObject( G.sima->image );
		}
		break;
	}
	
	return EXPP_ReturnPyObjError( PyExc_TypeError,
			"Only Scene and Image types have the active attribute" );
}

static int MainSeq_setActive(BPy_MainSeq *self, PyObject *value)
{
	switch (self->type) {
	case ID_SCE:
		if (!Scene_CheckPyObject(value)) {
			return EXPP_ReturnIntError(PyExc_TypeError,
					"Must be a scene" );
		} else {
			BPy_Scene *bpydata;
			Scene *data;
			
			bpydata = (BPy_Scene *)value;
			data= bpydata->scene;
			
			if (!data)
				return EXPP_ReturnIntError(PyExc_RuntimeError,
					"This Scene has been removed" );
			
			if (data != G.scene) {
				set_scene( data );
				scene_update_for_newframe(data, data->lay);
			}
		}
		return 0;
		
	case ID_IM:
		if (!Image_CheckPyObject(value)) {
			return EXPP_ReturnIntError(PyExc_TypeError,
					"Must be a scene" );
		} else {
			BPy_Image *bpydata;
			Image *data;
			
			if (!G.sima) 
				return 0;
			
			bpydata = (BPy_Image *)value;
			data= bpydata->image;
			
			if (!data)
				return EXPP_ReturnIntError(PyExc_RuntimeError,
					"This Scene has been removed" );
			
			if (data != G.sima->image)
				G.sima->image= data;
		}
		return 0;
	}
	
	return EXPP_ReturnIntError( PyExc_TypeError,
			"Only Scene and Image types have the active attribute" );
}


Mesh *add_mesh__internal(char *name)
{
	Mesh *mesh = add_mesh(); /* doesn't return NULL now, but might someday */
	
	/* Bound box set to null needed because a new mesh is initialized
	with a bounding box of -1 -1 -1 -1 -1 -1
	if its not set to null the bounding box is not re-calculated
	when ob.getBoundBox() is called.*/
	MEM_freeN(mesh->bb);
	mesh->bb= NULL;
	mesh->id.us = 0;
	rename_id( &mesh->id, name );
	return mesh;
}

Curve *add_curve__internal(char *name)
{
	Curve *blcurve = NULL;	/* for actual Curve Data we create in Blender */
	
	blcurve = add_curve( OB_CURVE );	/* first create the Curve Data in Blender */

	/* null check? */

	/* return user count to zero because add_curve() inc'd it */
	blcurve->id.us = 0;
	rename_id( &blcurve->id, name );
	return blcurve;
}

MetaBall *add_metaball__internal(char *name)
{
	MetaBall *blmball;	/* for actual Data we create in Blender */
	blmball = add_mball(  );	/* first create the MetaBall Data in Blender */

	/* null check? */

	/* return user count to zero because add_curve() inc'd it */
	blmball->id.us = 0;
	rename_id( &blmball->id, name );
	return blmball;
}


PyObject *MainSeq_new(BPy_MainSeq *self, PyObject * args)
{
	
	char *name;
	if( !PyArg_ParseTuple( args, "s#", &name, 21 ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"string expected as argument" );
	
	/* TODO, New data functions */
	
	/*
	switch (type) {
	case ID_SCE:
		return Scene_CreatePyObject( add_scene( name ) );
	case ID_OB:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"Add objects through the scenes objects iterator" );
	case ID_ME:
		return Mesh_CreatePyObject( add_mesh__internal( name ) );
	case ID_CU:
		return Curve_CreatePyObject(add_curve__internal( name ) );
	case ID_MB:
		return Metaball_CreatePyObject((MetaBall *)link);
		
		break;
	case ID_MA:
		
		break;
	case ID_TE:
		
		break;
	case ID_IM:
		
		break;
	case ID_LT:
		
		break;
	case ID_LA:
		
		break;
	
	case ID_CA:
		
		break;
	case ID_IP:
		
		break;
	case ID_WO:
		
		break;
	case ID_VF:
		
		break;
	case ID_TXT:
		
		break;
	case ID_SO:
		
		break;
	case ID_GR:
		
		break;
	case ID_AR:
		
		break;
	case ID_AC:
		
		break;
	}
	*/
	Py_RETURN_NONE;
}


PyObject *MainSeq_unlink(BPy_MainSeq *self, PyObject * args)
{
	PyObject *pyobj;
	
	switch (self->type) {
	case ID_SCE:
		if( !PyArg_ParseTuple( args, "O!", &Scene_Type, &pyobj ) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected Scene object" );
		} else {
			BPy_Scene *bpydata;
			Scene *data;
			
			bpydata = (BPy_Scene *)pyobj;
			data = bpydata->scene;
			
			if (!data)
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"This Scene has been removed" );
			
			/* Run the removal code */
			free_libblock( &G.main->scene, data );
			bpydata->scene= NULL;
			
			Py_RETURN_NONE;
		}
	case ID_GR:
		if( !PyArg_ParseTuple( args, "O!", &Group_Type, &pyobj ) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected Group object" );
		} else {
			BPy_Group *bpydata;
			Group *data;
			
			bpydata = (BPy_Group *)pyobj;
			data = bpydata->group;
			
			if (!data)
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"This Group has been removed alredy" );
			
			/* Run the removal code */
			free_group(data);
			unlink_group(data);
			data->id.us= 0;
			free_libblock( &G.main->group, data );
			bpydata->group= NULL;
			
			Py_RETURN_NONE;
		}

	case ID_TXT:
		if( !PyArg_ParseTuple( args, "O!", &Text_Type, &pyobj ) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected Text object" );
		} else {
			BPy_Text *bpydata;
			Text *data;
			bpydata = (BPy_Text *)pyobj;
			data = bpydata->text;
			
			if (!data)
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"This Group has been removed alredy" );
			
			/* Run the removal code */
			BPY_clear_bad_scriptlinks( data );
			free_text_controllers( data );
			unlink_text( data );
			free_libblock( &G.main->text, data );
			bpydata->text = NULL;
			
			Py_RETURN_NONE;
		}
	}
	return EXPP_ReturnPyObjError( PyExc_TypeError,
				      "Only types Scene, Group and Text can unlink" );	
}

/************************************************************************
 *
 * Python MainSeq_Type standard operations
 *
 ************************************************************************/
static void MainSeq_dealloc( BPy_MainSeq * self )
{
	PyObject_DEL( self );
}

static int MainSeq_compare( BPy_MainSeq * a, BPy_MainSeq * b )
{
	return ( a->type == b->type) ? 0 : -1;	
}

/*
 * repr function
 * callback functions building meaninful string to representations
 */
static PyObject *MainSeq_repr( BPy_MainSeq * self )
{
	return PyString_FromFormat( "[Main Iterator]");
}

static PyGetSetDef MainSeq_getseters[] = {
	{"active",
	 (getter)MainSeq_getActive, (setter)MainSeq_setActive,
	 "active object",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static struct PyMethodDef BPy_MainSeq_methods[] = {
	{"new", (PyCFunction)MainSeq_new, METH_VARARGS,
		"Create a new object in this scene from the obdata given and return a new object"},
	{"unlink", (PyCFunction)MainSeq_unlink, METH_VARARGS,
		"unlinks the object from the scene"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python MainSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject MainSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MainSeq",           /* char *tp_name; */
	sizeof( BPy_MainSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MainSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) MainSeq_compare, /* cmpfunc tp_compare; */
	( reprfunc ) MainSeq_repr,   /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	    /* PySequenceMethods *tp_as_sequence; */
	&MainSeq_as_mapping,                       /* PyMappingMethods *tp_as_mapping; */

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
	( getiterfunc) MainSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MainSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MainSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	MainSeq_getseters,       /* struct PyGetSetDef *tp_getset; */
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


/*-----------------------Main module Init())-----------------------------*/

static char M_Main_doc[] = "The Blender.Main submodule";

PyObject *Main_Init( void )
{
	PyObject *submodule;

	if( PyType_Ready( &MainSeq_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.Main", NULL, M_Main_doc );
	
	PyModule_AddObject( submodule, "scenes", MainSeq_CreatePyObject(NULL, ID_SCE) );
	PyModule_AddObject( submodule, "objects", MainSeq_CreatePyObject(NULL, ID_OB) );
	PyModule_AddObject( submodule, "meshes", MainSeq_CreatePyObject(NULL, ID_ME) );
	PyModule_AddObject( submodule, "curves", MainSeq_CreatePyObject(NULL, ID_CU) );
	PyModule_AddObject( submodule, "metaballs", MainSeq_CreatePyObject(NULL, ID_MB) );
	PyModule_AddObject( submodule, "materials", MainSeq_CreatePyObject(NULL, ID_MA) );
	PyModule_AddObject( submodule, "textures", MainSeq_CreatePyObject(NULL, ID_TE) );
	PyModule_AddObject( submodule, "images", MainSeq_CreatePyObject(NULL, ID_IM) );
	PyModule_AddObject( submodule, "lattices", MainSeq_CreatePyObject(NULL, ID_LT) );
	PyModule_AddObject( submodule, "lamps", MainSeq_CreatePyObject(NULL, ID_LA) );
	PyModule_AddObject( submodule, "cameras", MainSeq_CreatePyObject(NULL, ID_CA) );
	PyModule_AddObject( submodule, "ipos", MainSeq_CreatePyObject(NULL, ID_IP) );
	PyModule_AddObject( submodule, "worlds", MainSeq_CreatePyObject(NULL, ID_WO) );
	PyModule_AddObject( submodule, "fonts", MainSeq_CreatePyObject(NULL, ID_VF) );
	PyModule_AddObject( submodule, "texts", MainSeq_CreatePyObject(NULL, ID_TXT) );
	PyModule_AddObject( submodule, "sounds", MainSeq_CreatePyObject(NULL, ID_SO) );
	PyModule_AddObject( submodule, "groups", MainSeq_CreatePyObject(NULL, ID_GR) );
	PyModule_AddObject( submodule, "armatures", MainSeq_CreatePyObject(NULL, ID_AR) );
	PyModule_AddObject( submodule, "actions", MainSeq_CreatePyObject(NULL, ID_AC) );
	
	return submodule;
}
