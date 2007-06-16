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

#include "bpy_data.h"

#include "MEM_guardedalloc.h"	/* for MEM_callocN */
#include "DNA_space_types.h"	/* SPACE_VIEW3D, SPACE_SEQ */
#include "DNA_scene_types.h"
#include "DNA_object_types.h" /* LibBlockSeq_new */
#include "DNA_texture_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_group_types.h"
#include "DNA_world_types.h"
#include "DNA_vfont_types.h"
#include "DNA_armature_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_action_types.h"
#include "DNA_meta_types.h"

/* Use the add_* from BKE_* */
#include "BKE_global.h"
#include "BKE_utildefines.h" /* clamp */
#include "BKE_armature.h"
#include "BKE_ipo.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_lattice.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_sca.h" /*free_text_controllers*/
#include "BKE_font.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_curve.h"
#include "BKE_material.h"
#include "BKE_group.h"
#include "BKE_text.h"
#include "BKE_texture.h"
#include "BKE_world.h"

#include "BLI_blenlib.h" /* BLI_countlist */
#include "BIF_drawscene.h"	/* for set_scene */
#include "BIF_screen.h"		/* curarea */
#include "BIF_drawimage.h" /* what image */
#include "BIF_drawtext.h" /* unlink_text */
#include "BIF_editsound.h" /* sound_new_sound */
#include "BIF_editaction.h" /* add_empty_action */

/* python types */
#include "../BPY_extern.h" /* clearing scriptlinks */

#include "gen_utils.h"
#include "gen_library.h" /* generic ID functions */

#include "Object.h"
#include "Camera.h"
#include "Armature.h"
#include "Lamp.h"
#include "Curve.h"
#include "NMesh.h"
#include "Mesh.h"
#include "Lattice.h"
#include "Metaball.h"
#include "Text.h"
#include "Text3d.h"
#include "Font.h"
#include "Group.h"
#include "World.h"
#include "Texture.h"
#include "Ipo.h"
#include "Text.h"
#include "Sound.h"
#include "NLA.h"
#include "Scene.h"
#include "Library.h"

#include "bpy_config.h" /* config pydata */

/* used only for texts.active */
#include "BIF_screen.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"

extern VFont *get_builtin_font(void);

static PyObject *LibBlockSeq_CreatePyObject( Link *iter, int type )
{
	BPy_LibBlockSeq *seq = PyObject_NEW( BPy_LibBlockSeq, &LibBlockSeq_Type);
	seq->iter = iter;
	seq->type = type;
	return (PyObject *)seq;
}


static int LibBlockSeq_len( BPy_LibBlockSeq * self )
{
	ListBase *lb = wich_libbase(G.main, self->type);
	return BLI_countlist( lb );
}

static PyObject * LibBlockSeq_subscript(BPy_LibBlockSeq * self, PyObject *key)
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
				return GetPyObjectFromID(id);
			}
		}
	}
	return ( EXPP_ReturnPyObjError
				 ( PyExc_KeyError, "Requested data does not exist") );
}

static PyMappingMethods LibBlockSeq_as_mapping = {
	( inquiry ) LibBlockSeq_len,	/* mp_length */
	( binaryfunc ) LibBlockSeq_subscript,	/* mp_subscript */
	( objobjargproc ) 0,	/* mp_ass_subscript */
};


/************************************************************************
 *
 * Python LibBlockSeq_Type iterator (iterates over GroupObjects)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *LibBlockSeq_getIter( BPy_LibBlockSeq * self )
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
		return LibBlockSeq_CreatePyObject(link, self->type);
	}
}

/*
 * Return next LibBlockSeq iter.
 */

static PyObject *LibBlockSeq_nextIter( BPy_LibBlockSeq * self )
{
	PyObject *object;
	Link *link;
	if( !(self->iter) ) {
		self->iter= NULL;
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	object = GetPyObjectFromID((ID *)self->iter);
	
	link= self->iter->next;
	self->iter= link;
	return object;
}

PyObject *LibBlockSeq_getActive(BPy_LibBlockSeq *self)
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
	case ID_TXT: {
			SpaceText *st= curarea->spacedata.first;
			
			if (st->spacetype!=SPACE_TEXT || st->text==NULL) {
				Py_RETURN_NONE;
			} else {
				return Text_CreatePyObject( st->text );
			}
		}
	}
	
	return EXPP_ReturnPyObjError( PyExc_TypeError,
			"Only Scene and Image types have the active attribute" );
}

static int LibBlockSeq_setActive(BPy_LibBlockSeq *self, PyObject *value)
{
	switch (self->type) {
	case ID_SCE:
		if (!BPy_Scene_Check(value)) {
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
		if (!BPy_Image_Check(value)) {
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
	
	case ID_TXT:
		if (!BPy_Text_Check(value)) {
			return EXPP_ReturnIntError(PyExc_TypeError,
					"Must be a text" );
		} else {
			SpaceText *st= curarea->spacedata.first;	
			Text *data = ((BPy_Text *)value)->text;
			
			if( !data )
				return EXPP_ReturnIntError( PyExc_RuntimeError,
						      "This object isn't linked to a Blender Text Object" );
			if(st->spacetype!=SPACE_TEXT)
				return 0;
			st->text = data;
		}
		return 0;
	}
	
	return EXPP_ReturnIntError( PyExc_TypeError,
			"Only Scene and Image types have the active attribute" );
}

static int LibBlockSeq_setTag(BPy_LibBlockSeq *self, PyObject *value)
{
	int param = PyObject_IsTrue( value );
	ID *id;
	
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument in range [0,1]" );
	
	id = (ID *)wich_libbase(G.main, self->type)->first;
	
	if (param) {
		for (; id; id = id->next) {
			id->flag |= LIB_DOIT;
		}
	} else {
		for (; id; id = id->next) {
			id->flag &= ~LIB_DOIT;
		}
	}
	
	return 0;	
}


/* New Data, internal functions */
Mesh *add_mesh__internal(char *name)
{
	Mesh *mesh = add_mesh(name); /* doesn't return NULL now, but might someday */
	
	/* Bound box set to null needed because a new mesh is initialized
	with a bounding box of -1 -1 -1 -1 -1 -1
	if its not set to null the bounding box is not re-calculated
	when ob.getBoundBox() is called.*/
	MEM_freeN(mesh->bb);
	mesh->bb= NULL;
	return mesh;
}

/* used for new and load */
PyObject *LibBlockSeq_new(BPy_LibBlockSeq *self, PyObject * args, PyObject *kwd)
{
	ID *id = NULL;
	char *name=NULL, *filename=NULL, *data_type=NULL;
	int img_width=256, img_height=256;
	float color[] = {0, 0, 0, 1};
	short data_code = 0;
	int user_count = 0;
	
	/* Load from file */
	if ( (	self->type==ID_IM || self->type==ID_VF 		||
			self->type==ID_SO || self->type==ID_TXT) 	&&
				( PyTuple_Size( args ) < 3		))
	{
		static char *kwlist[] = {"name", "filename", NULL};
		
		if(PyArg_ParseTupleAndKeywords(args, kwd, "|ss", kwlist, &name, &filename) && filename ) {
			PyObject *ret;
			
			if (strlen(filename) > FILE_MAXDIR + FILE_MAXFILE - 1)
					return ( EXPP_ReturnPyObjError( PyExc_IOError,
								"filename too long" ) );
			
			if (self->type == ID_IM) {
				Image *img = BKE_add_image_file( filename );
				if (!img)
					return ( EXPP_ReturnPyObjError( PyExc_IOError,
								"couldn't load image" ) );
				ret = Image_CreatePyObject( img );
				
			} else if (self->type == ID_VF) {
				VFont *vf = load_vfont (filename);
				if (!vf)
					return EXPP_ReturnPyObjError( PyExc_IOError,
								"couldn't load font" );
				ret = Font_CreatePyObject(vf);
				
			} else if (self->type == ID_SO) {
				bSound  *snd = sound_new_sound( filename );
				if (!snd)
					return EXPP_ReturnPyObjError( PyExc_IOError,
								"couldn't load sound" );
				ret = Sound_CreatePyObject(snd);
				
			} else if (self->type == ID_TXT) {
				Text *txt = NULL;
				txt = add_text( filename );
				if( !txt )
					return EXPP_ReturnPyObjError( PyExc_IOError,
						      "couldn't load text" );
				ret = Text_CreatePyObject(txt);
			}
			
			if (!ret)
					return EXPP_ReturnPyObjError( PyExc_IOError,
						      "couldn't create pyobject on load, unknown error" );
			if (name) {
				ID *id = ((BPy_GenericLib *)ret)->id;
				rename_id( id, name );
			}
			return ret;
		}
	}
	
	/* New Data */
	if (self->type == ID_IM) {
		/* Image, accepts width and height*/
		if( !PyArg_ParseTuple( args, "|sii", &name, &img_width, &img_height ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"one string and two ints expected as arguments" );
		CLAMP(img_width,  4, 5000);
		CLAMP(img_height, 4, 5000);
	
	} else if (self->type == ID_CU) {
		/* Curve, needs name and type strings */
		if( !PyArg_ParseTuple( args, "ss", &name, &data_type ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"two strings expected as arguments" );
		
		if(      !strcmp( data_type, "Curve" ) )		data_code = OB_CURVE;
		else if( !strcmp( data_type, "Text3d" ) )			data_code = OB_FONT;/*
		else if( !strcmp( data_type, "Surf" ) )			data_code = OB_SURF;*/
		else return EXPP_ReturnPyObjError( PyExc_TypeError,
				"Second argument for Curve type incorrect\t\nmust be a string in (Curve or Text - Surf is not supported yet)" );
	
	} else if (self->type == ID_IP) {
		/* IPO, needs name and type strings */
		if( !PyArg_ParseTuple( args, "ss", &name, &data_type ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"two strings expected as arguments" );
		
		if(      !strcmp( data_type, "Object" ) )		data_code = ID_OB;
		else if( !strcmp( data_type, "Camera" ) )		data_code = ID_CA;
		else if( !strcmp( data_type, "World" ) )		data_code = ID_WO;
		else if( !strcmp( data_type, "Material" ) )		data_code = ID_MA;
		else if( !strcmp( data_type, "Texture" ) )		data_code = ID_TE;
		else if( !strcmp( data_type, "Lamp" ) )			data_code = ID_LA;
		else if( !strcmp( data_type, "Action" ) )		data_code = ID_PO;
		else if( !strcmp( data_type, "Constraint" ) )	data_code = ID_CO;
		else if( !strcmp( data_type, "Sequence" ) )		data_code = ID_SEQ;
		else if( !strcmp( data_type, "Curve" ) )		data_code = ID_CU;
		else if( !strcmp( data_type, "Key" ) )			data_code = ID_KE;
		else return EXPP_ReturnPyObjError( PyExc_TypeError,
				"Second argument for IPO type incorrect\t\nmust be a string in (Object, Camera, World, Material, Texture, Lamp, Action, Sequence, Curve, Key)" );
		
	} else {
		/* Other types only need the name */
		if( !PyArg_ParseTuple( args, "|s", &name ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"new(name) - name must be a string argument" );
	}
	
	switch (self->type) {
	case ID_SCE: 
		id = (ID *)add_scene( name?name:"Scene" );
		user_count = 1;
		break;
	case ID_OB:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"Add objects through the scenes objects iterator" );
	case ID_ME:
		id = (ID *)add_mesh__internal( name?name:"Mesh" );
		break;
	case ID_CU:
		id = (ID *)add_curve( name?name:"Curve", data_code );
		if (data_code==OB_FONT) {
			Text3d *text3d = (Text3d *)id;
			text3d->vfont= get_builtin_font();
			text3d->vfont->id.us++;
			text3d->str= MEM_mallocN(sizeof(wchar_t), "str");
			text3d->str[0] = '\0';
			text3d->totbox= text3d->actbox= 1;
			text3d->tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "textbox");
			text3d->tb[0].w = text3d->tb[0].h = 0.0;
		
		} /*else { CURVE - Dont need to do anything } */
		break;
	case ID_MB:
		id = (ID *)add_mball( name?name:"MBall" );
		break;
	case ID_MA:
		id = (ID *)add_material( name?name:"Material" );
		break;
	case ID_TE:
		id = (ID *)add_texture( name?name:"Texture" );
		break;
	case ID_IM: 
	{
		id = (ID *)BKE_add_image_size(img_width, img_height, name?name:"Image", 0, color);
		if( !id )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
				"couldn't create PyObject Image_Type" ) );
		/* new images have zero user count */
		break;
	}
	case ID_LT:
		id = (ID *)add_lattice( name?name:"Lattice" );
		break;
	case ID_LA:
		id = (ID *)add_lamp( name?name:"Lamp" );
		break;
	case ID_CA:
		id = (ID *)add_camera( name?name:"Camera" );
		break;
	case ID_IP:
		id = (ID *)add_ipo( name?name:"Ipo", data_code );
		break;
	case ID_WO:
		id = (ID *)add_world( name?name:"World" );
		break;
	case ID_VF:
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"Cannot create new fonts, use the load() function to load from a file" );
	case ID_TXT:
		id = (ID *)add_empty_text( name?name:"Text" );
		user_count = 1;
		break;
	case ID_SO:
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"Cannot create new sounds, use the load() function to load from a file" );
	case ID_GR:	
		id = (ID *)add_group( name?name:"Group" );
		user_count = 1;
		break;
	case ID_AR:
		id = (ID *)add_armature( name?name:"Armature" );
		break;
	case ID_AC:
		id = (ID *)add_empty_action( name?name:"Action" );
		user_count = 1;
		break;
	}

	if (!id)
		Py_RETURN_NONE;

	/* set some types user count to 1, otherwise zero */
	id->us = user_count;
	
	return GetPyObjectFromID(id);
}


PyObject *LibBlockSeq_unlink(BPy_LibBlockSeq *self, PyObject * value)
{
	switch (self->type) {
	case ID_SCE:
		if( !BPy_Scene_Check(value) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected Scene object" );
		} else {
			Scene *data = ((BPy_Scene *)value)->scene;
			
			if (!data)
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"This Scene has been removed" );
			
			/* Run the removal code */
			free_libblock( &G.main->scene, data );
			((BPy_Scene *)value)->scene = NULL;
			Py_RETURN_NONE;
		}
	case ID_GR:
		if( !BPy_Group_Check(value) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected Group object" );
		} else {
			Group *data = ((BPy_Group *)value)->group;
			
			if (!data)
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"This Group has been removed alredy" );
			
			/* Run the removal code */
			free_group(data);
			unlink_group(data);
			data->id.us= 0;
			free_libblock( &G.main->group, data );
			((BPy_Group *)value)->group = NULL;
			
			Py_RETURN_NONE;
		}

	case ID_TXT:
		if( !BPy_Text_Check(value) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected Text object" );
		} else {
			Text *data = ((BPy_Text *)value)->text;
			
			if (!data)
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"This Group has been removed alredy" );
			
			/* Run the removal code */
			BPY_clear_bad_scriptlinks( data );
			free_text_controllers( data );
			unlink_text( data );
			free_libblock( &G.main->text, data );
			((BPy_Text *)value)->text = NULL;
			
			Py_RETURN_NONE;
		}
	}
	return EXPP_ReturnPyObjError( PyExc_TypeError,
				      "Only types Scene, Group and Text can unlink" );	
}

static int LibBlockSeq_compare( BPy_LibBlockSeq * a, BPy_LibBlockSeq * b )
{
	return ( a->type == b->type) ? 0 : -1;	
}

/*
 * repr function
 * callback functions building meaninful string to representations
 */
static PyObject *LibBlockSeq_repr( BPy_LibBlockSeq * self )
{
	return PyString_FromFormat( "[LibBlockSeq Iterator]");
}

static PyGetSetDef LibBlockSeq_getseters[] = {
	{"active",
	 (getter)LibBlockSeq_getActive, (setter)LibBlockSeq_setActive,
	 "active object",
	 NULL},
	{"tag",
	 (getter)NULL, (setter)LibBlockSeq_setTag,
	 "tag all data in True or False (write only)",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static struct PyMethodDef BPy_LibBlockSeq_methods[] = {
	{"new", (PyCFunction)LibBlockSeq_new, METH_VARARGS | METH_KEYWORDS,
		"(name) - Create a new object in this scene from the obdata given and return a new object"},
	{"unlink", (PyCFunction)LibBlockSeq_unlink, METH_O,
		"unlinks the object from the scene"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python LibBlockSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject LibBlockSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender LibBlockSeq",           /* char *tp_name; */
	sizeof( BPy_LibBlockSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) LibBlockSeq_compare, /* cmpfunc tp_compare; */
	( reprfunc ) LibBlockSeq_repr,   /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	    /* PySequenceMethods *tp_as_sequence; */
	&LibBlockSeq_as_mapping,                       /* PyMappingMethods *tp_as_mapping; */

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
	( getiterfunc) LibBlockSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) LibBlockSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_LibBlockSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	LibBlockSeq_getseters,       /* struct PyGetSetDef *tp_getset; */
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


/*-----------------------------BPy module Init())-----------------------------*/

PyObject * Data_Init( void )
{
	PyObject *module;
	PyObject *dict;
	
	
	PyType_Ready( &LibBlockSeq_Type );
	PyType_Ready( &Config_Type );
	
	/*submodule = Py_InitModule3( "Blender.Main", NULL, M_Main_doc );*/
	module = Py_InitModule3( "bpy.data", NULL, "The bpy.data submodule" );
	dict = PyModule_GetDict( module );
	
	/* Python Data Types */
	PyModule_AddObject( module, "scenes", 	LibBlockSeq_CreatePyObject(NULL, ID_SCE) );
	PyModule_AddObject( module, "objects",	LibBlockSeq_CreatePyObject(NULL, ID_OB) );
	PyModule_AddObject( module, "meshes",	LibBlockSeq_CreatePyObject(NULL, ID_ME) );
	PyModule_AddObject( module, "curves",	LibBlockSeq_CreatePyObject(NULL, ID_CU) );
	PyModule_AddObject( module, "metaballs",LibBlockSeq_CreatePyObject(NULL, ID_MB) );
	PyModule_AddObject( module, "materials",LibBlockSeq_CreatePyObject(NULL, ID_MA) );
	PyModule_AddObject( module, "textures",	LibBlockSeq_CreatePyObject(NULL, ID_TE) );
	PyModule_AddObject( module, "images",	LibBlockSeq_CreatePyObject(NULL, ID_IM) );
	PyModule_AddObject( module, "lattices",	LibBlockSeq_CreatePyObject(NULL, ID_LT) );
	PyModule_AddObject( module, "lamps",	LibBlockSeq_CreatePyObject(NULL, ID_LA) );
	PyModule_AddObject( module, "cameras",	LibBlockSeq_CreatePyObject(NULL, ID_CA) );
	PyModule_AddObject( module, "ipos",		LibBlockSeq_CreatePyObject(NULL, ID_IP) );
	PyModule_AddObject( module, "worlds",	LibBlockSeq_CreatePyObject(NULL, ID_WO) );
	PyModule_AddObject( module, "fonts",	LibBlockSeq_CreatePyObject(NULL, ID_VF) );
	PyModule_AddObject( module, "texts",	LibBlockSeq_CreatePyObject(NULL, ID_TXT) );
	PyModule_AddObject( module, "sounds",	LibBlockSeq_CreatePyObject(NULL, ID_SO) );
	PyModule_AddObject( module, "groups",	LibBlockSeq_CreatePyObject(NULL, ID_GR) );
	PyModule_AddObject( module, "armatures",LibBlockSeq_CreatePyObject(NULL, ID_AR) );
	PyModule_AddObject( module, "actions",	LibBlockSeq_CreatePyObject(NULL, ID_AC) );
	return module;
}
