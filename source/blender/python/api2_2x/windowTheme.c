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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "windowTheme.h" /*This must come first*/

#include "BLI_blenlib.h"
#include "BIF_interface_icons.h"
#include "BIF_resources.h"
#include "MEM_guardedalloc.h"
#include "charRGBA.h"
#include "gen_utils.h"


#define EXPP_THEME_VTX_SIZE_MIN 1
#define EXPP_THEME_VTX_SIZE_MAX 10
#define EXPP_THEME_FDOT_SIZE_MIN 1
#define EXPP_THEME_FDOT_SIZE_MAX 10
#define EXPP_THEME_DRAWTYPE_MIN 1
#define EXPP_THEME_DRAWTYPE_MAX 4

#define EXPP_THEME_NUMBEROFTHEMES 16
static const EXPP_map_pair themes_map[] = {
	{"ui", -1},
	{"buts", SPACE_BUTS},
	{"view3d", SPACE_VIEW3D},
	{"file", SPACE_FILE},
	{"ipo", SPACE_IPO},
	{"info", SPACE_INFO},
	{"sound", SPACE_SOUND},
	{"action", SPACE_ACTION},
	{"nla", SPACE_NLA},
	{"seq", SPACE_SEQ},
	{"image", SPACE_IMAGE},
	{"imasel", SPACE_IMASEL},
	{"text", SPACE_TEXT},
	{"oops", SPACE_OOPS},
	{"time", SPACE_TIME},
	{"node", SPACE_NODE},
	{NULL, 0}
};

static PyObject *M_Theme_New( PyObject * self, PyObject * args );
static PyObject *M_Theme_Get( PyObject * self, PyObject * args );

static char M_Theme_doc[] = "The Blender Theme module\n\n\
This module provides access to UI Theme data in Blender";

static char M_Theme_New_doc[] = "Theme.New (name = 'New Theme',\
theme = <default>):\n\
	Return a new Theme Data object.\n\
(name) - string: the Theme's name, it defaults to 'New Theme';\n\
(theme) - bpy Theme: a base Theme to copy all data from, it defaults to the\n\
current one.";

static char M_Theme_Get_doc[] = "Theme.Get (name = None):\n\
	Return the theme data with the given 'name', None if not found, or\n\
	Return a list with all Theme Data objects if no argument was given.";

/*****************************************************************************/
/* Python method structure definition for Blender.Theme module:		   */
/*****************************************************************************/
struct PyMethodDef M_Theme_methods[] = {
	{"New", M_Theme_New, METH_VARARGS, M_Theme_New_doc},
	{"Get", M_Theme_Get, METH_VARARGS, M_Theme_Get_doc},
	{NULL, NULL, 0, NULL}
};

static void ThemeSpace_dealloc( BPy_ThemeSpace * self );
static int ThemeSpace_compare( BPy_ThemeSpace * a, BPy_ThemeSpace * b );
static PyObject *ThemeSpace_repr( BPy_ThemeSpace * self );
static PyObject *ThemeSpace_getAttr( BPy_ThemeSpace * self, char *name );
static int ThemeSpace_setAttr( BPy_ThemeSpace * self, char *name,
			       PyObject * val );

static PyMethodDef BPy_ThemeSpace_methods[] = {
	{NULL, NULL, 0, NULL}
};

PyTypeObject ThemeSpace_Type = {
	PyObject_HEAD_INIT( NULL ) 0,	/* ob_size */
	"Blender Space Theme",	/* tp_name */
	sizeof( BPy_ThemeSpace ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) ThemeSpace_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) ThemeSpace_getAttr,	/* tp_getattr */
	( setattrfunc ) ThemeSpace_setAttr,	/* tp_setattr */
	( cmpfunc ) ThemeSpace_compare,	/* tp_compare */
	( reprfunc ) ThemeSpace_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	0,			//BPy_ThemeSpace_methods,            /* tp_methods */
	0,			/* tp_members */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static void ThemeSpace_dealloc( BPy_ThemeSpace * self )
{
	PyObject_DEL( self );
}

#define ELSEIF_TSP_RGBA(attr)\
	else if (!strcmp(name, #attr))\
		attrib = charRGBA_New(&tsp->attr[0]);

/* Example: ELSEIF_TSP_RGBA(back) becomes:
 * else if (!strcmp(name, "back")
 * 	attrib = charRGBA_New(&tsp->back[0])
 */

static PyObject *ThemeSpace_getAttr( BPy_ThemeSpace * self, char *name )
{
	PyObject *attrib = Py_None;
	ThemeSpace *tsp = self->tsp;

	if( !strcmp( name, "theme" ) )
		attrib = PyString_FromString( self->theme->name );
		ELSEIF_TSP_RGBA( back )
		ELSEIF_TSP_RGBA( text )
		ELSEIF_TSP_RGBA( text_hi )
		ELSEIF_TSP_RGBA( header )
		ELSEIF_TSP_RGBA( panel )
		ELSEIF_TSP_RGBA( shade1 )
		ELSEIF_TSP_RGBA( shade2 )
		ELSEIF_TSP_RGBA( hilite )
		ELSEIF_TSP_RGBA( grid )
		ELSEIF_TSP_RGBA( wire )
		ELSEIF_TSP_RGBA( select )
		ELSEIF_TSP_RGBA( lamp )
		ELSEIF_TSP_RGBA( active )
		ELSEIF_TSP_RGBA( group )
		ELSEIF_TSP_RGBA( group_active )
		ELSEIF_TSP_RGBA( transform )
		ELSEIF_TSP_RGBA( vertex )
		ELSEIF_TSP_RGBA( vertex_select )
		ELSEIF_TSP_RGBA( edge )
		ELSEIF_TSP_RGBA( edge_select )
		ELSEIF_TSP_RGBA( edge_seam )
		ELSEIF_TSP_RGBA( edge_sharp )
		ELSEIF_TSP_RGBA( edge_facesel )
		ELSEIF_TSP_RGBA( face )
		ELSEIF_TSP_RGBA( face_select )
		ELSEIF_TSP_RGBA( face_dot )
		ELSEIF_TSP_RGBA( normal )
		ELSEIF_TSP_RGBA( bone_solid )
		ELSEIF_TSP_RGBA( bone_pose )
		ELSEIF_TSP_RGBA( strip )
		ELSEIF_TSP_RGBA( strip_select )
		ELSEIF_TSP_RGBA( syntaxl )
		ELSEIF_TSP_RGBA( syntaxn )
		ELSEIF_TSP_RGBA( syntaxb )
		ELSEIF_TSP_RGBA( syntaxv )
		ELSEIF_TSP_RGBA( syntaxc )
		ELSEIF_TSP_RGBA( movie )
		ELSEIF_TSP_RGBA( image )
		ELSEIF_TSP_RGBA( scene )
		ELSEIF_TSP_RGBA( audio )
		ELSEIF_TSP_RGBA( effect )
		ELSEIF_TSP_RGBA( plugin )
		ELSEIF_TSP_RGBA( transition )
		ELSEIF_TSP_RGBA( meta )
		else if( !strcmp( name, "vertex_size" ) )
		attrib = Py_BuildValue( "i", tsp->vertex_size );
		else if( !strcmp( name, "facedot_size" ) )
		attrib = Py_BuildValue( "i", tsp->facedot_size );
	else if( !strcmp( name, "__members__" ) )
		attrib = Py_BuildValue("[sssssssssssssssssssssssssssssssssssssssssssssss]", "theme",
					"back", "text", "text_hi", "header",
					"panel", "shade1", "shade2", "hilite",
					"grid", "wire", "select", "lamp", "active",
					"group", "group_active",
					"transform", "vertex", "vertex_select",
					"edge", "edge_select", "edge_seam", "edge_sharp",
					"edge_facesel", "face", "face_select",
					"face_dot", "normal", "bone_solid", "bone_pose",
					"strip", "strip_select",
					"syntaxl", "syntaxn", "syntaxb", "syntaxv", "syntaxc",
					"movie", "image", "scene", "audio", "effect", "plugin",
					"transition", "meta", 
					"vertex_size", "facedot_size" );

	if( attrib != Py_None )
		return attrib;

	return Py_FindMethod( BPy_ThemeSpace_methods, ( PyObject * ) self,
			      name );
}

static int ThemeSpace_setAttr( BPy_ThemeSpace * self, char *name,
			       PyObject * value )
{
	PyObject *attrib = NULL;
	ThemeSpace *tsp = self->tsp;
	int ret = -1;

	if( !strcmp( name, "back" ) )
		attrib = charRGBA_New( &tsp->back[0] );
		ELSEIF_TSP_RGBA( text )
		ELSEIF_TSP_RGBA( text_hi )
		ELSEIF_TSP_RGBA( header )
		ELSEIF_TSP_RGBA( panel )
		ELSEIF_TSP_RGBA( shade1 )
		ELSEIF_TSP_RGBA( shade2 )
		ELSEIF_TSP_RGBA( hilite )
		ELSEIF_TSP_RGBA( grid )
		ELSEIF_TSP_RGBA( wire )
		ELSEIF_TSP_RGBA( select )
		ELSEIF_TSP_RGBA( lamp )
		ELSEIF_TSP_RGBA( active )
		ELSEIF_TSP_RGBA( group )
		ELSEIF_TSP_RGBA( group_active )
		ELSEIF_TSP_RGBA( transform )
		ELSEIF_TSP_RGBA( vertex )
		ELSEIF_TSP_RGBA( vertex_select )
		ELSEIF_TSP_RGBA( edge )
		ELSEIF_TSP_RGBA( edge_select )
		ELSEIF_TSP_RGBA( edge_seam )
		ELSEIF_TSP_RGBA( edge_sharp )
		ELSEIF_TSP_RGBA( edge_facesel )
		ELSEIF_TSP_RGBA( face )
		ELSEIF_TSP_RGBA( face_select )
		ELSEIF_TSP_RGBA( face_dot )
		ELSEIF_TSP_RGBA( normal )
		ELSEIF_TSP_RGBA( bone_solid )
		ELSEIF_TSP_RGBA( bone_pose )
		ELSEIF_TSP_RGBA( strip )
		ELSEIF_TSP_RGBA( strip_select )
		ELSEIF_TSP_RGBA( syntaxl )
		ELSEIF_TSP_RGBA( syntaxn )
		ELSEIF_TSP_RGBA( syntaxb )
		ELSEIF_TSP_RGBA( syntaxv )
		ELSEIF_TSP_RGBA( syntaxc )
		ELSEIF_TSP_RGBA( movie )
		ELSEIF_TSP_RGBA( image )
		ELSEIF_TSP_RGBA( scene )
		ELSEIF_TSP_RGBA( audio )
		ELSEIF_TSP_RGBA( effect )
		ELSEIF_TSP_RGBA( plugin )
		ELSEIF_TSP_RGBA( transition )
		ELSEIF_TSP_RGBA( meta )
		else if( !strcmp( name, "vertex_size" ) ) {
		int val;

		if( !PyInt_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError,
						    "expected integer value" );

		val = ( int ) PyInt_AsLong( value );
		tsp->vertex_size = (char)EXPP_ClampInt( val,
						  EXPP_THEME_VTX_SIZE_MIN,
						  EXPP_THEME_VTX_SIZE_MAX );
		ret = 0;
	}
	else if( !strcmp( name, "facedot_size" ) ) {
		int val;

		if( !PyInt_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError,
						    "expected integer value" );

		val = ( int ) PyInt_AsLong( value );
		tsp->vertex_size = (char)EXPP_ClampInt( val,
						  EXPP_THEME_FDOT_SIZE_MIN,
						  EXPP_THEME_FDOT_SIZE_MAX );
		ret = 0;
	} else
		return EXPP_ReturnIntError( PyExc_AttributeError,
					    "attribute not found" );

	if( attrib ) {
		PyObject *pyret = NULL;
		PyObject *valtuple = Py_BuildValue( "(O)", value );

		if( !valtuple )
			return EXPP_ReturnIntError( PyExc_MemoryError,
						    "couldn't create tuple!" );

		pyret = charRGBA_setCol( ( BPy_charRGBA * ) attrib, valtuple );
		Py_DECREF( valtuple );

		if( pyret == Py_None ) {
			Py_DECREF( Py_None );	/* was increfed by charRGBA_setCol */
			ret = 0;
		}

		Py_DECREF( attrib );	/* we're done with it */
	}

	return ret;		/* 0 if all went well */
}

static int ThemeSpace_compare( BPy_ThemeSpace * a, BPy_ThemeSpace * b )
{
	ThemeSpace *pa = a->tsp, *pb = b->tsp;
	return ( pa == pb ) ? 0 : -1;
}

static PyObject *ThemeSpace_repr( BPy_ThemeSpace * self )
{
	return PyString_FromFormat( "[Space theme from theme \"%s\"]",
				    self->theme->name );
}

static void ThemeUI_dealloc( BPy_ThemeUI * self );
static int ThemeUI_compare( BPy_ThemeUI * a, BPy_ThemeUI * b );
static PyObject *ThemeUI_repr( BPy_ThemeUI * self );
static PyObject *ThemeUI_getAttr( BPy_ThemeUI * self, char *name );
static int ThemeUI_setAttr( BPy_ThemeUI * self, char *name, PyObject * val );

static PyMethodDef BPy_ThemeUI_methods[] = {
	{NULL, NULL, 0, NULL}
};

PyTypeObject ThemeUI_Type = {
	PyObject_HEAD_INIT( NULL ) 0,	/* ob_size */
	"Blender UI Theme",	/* tp_name */
	sizeof( BPy_ThemeUI ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) ThemeUI_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) ThemeUI_getAttr,	/* tp_getattr */
	( setattrfunc ) ThemeUI_setAttr,	/* tp_setattr */
	( cmpfunc ) ThemeUI_compare,	/* tp_compare */
	( reprfunc ) ThemeUI_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	0,			//BPy_ThemeUI_methods,         /* tp_methods */
	0,			/* tp_members */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static void ThemeUI_dealloc( BPy_ThemeUI * self )
{
	PyObject_DEL( self );
}

#define ELSEIF_TUI_RGBA(attr)\
	else if (!strcmp(name, #attr))\
		attrib = charRGBA_New(&tui->attr[0]);

/* Example: ELSEIF_TUI_RGBA(outline) becomes:
 * else if (!strcmp(name, "outline")
 * 	attr = charRGBA_New(&tui->outline[0])
 */

static PyObject *ThemeUI_getAttr( BPy_ThemeUI * self, char *name )
{
	PyObject *attrib = Py_None;
	ThemeUI *tui = self->tui;

	if( !strcmp( name, "theme" ) )
		attrib = PyString_FromString( self->theme->name );
	ELSEIF_TUI_RGBA( outline )
		ELSEIF_TUI_RGBA( neutral )
		ELSEIF_TUI_RGBA( action )
		ELSEIF_TUI_RGBA( setting )
		ELSEIF_TUI_RGBA( setting1 )
		ELSEIF_TUI_RGBA( setting2 )
		ELSEIF_TUI_RGBA( num )
		ELSEIF_TUI_RGBA( textfield )
		ELSEIF_TUI_RGBA( textfield_hi )
		ELSEIF_TUI_RGBA( popup )
		ELSEIF_TUI_RGBA( text )
		ELSEIF_TUI_RGBA( text_hi )
		ELSEIF_TUI_RGBA( menu_back )
		ELSEIF_TUI_RGBA( menu_item )
		ELSEIF_TUI_RGBA( menu_hilite )
		ELSEIF_TUI_RGBA( menu_text )
		ELSEIF_TUI_RGBA( menu_text_hi )
		else if( !strcmp( name, "drawType" ) )
		attrib = PyInt_FromLong( ( char ) tui->but_drawtype );
		else if( !strcmp( name, "iconTheme" ) )
		attrib = PyString_FromString( tui->iconfile );
	else if( !strcmp( name, "__members__" ) )
		attrib = Py_BuildValue( "[ssssssssssssssssssss]", "theme",
					"outline", "neutral", "action",
					"setting", "setting1", "setting2",
					"num", "textfield", "textfield_hi", "popup", "text",
					"text_hi", "menu_back", "menu_item",
					"menu_hilite", "menu_text",
					"menu_text_hi", "drawType", "iconTheme" );

	if( attrib != Py_None )
		return attrib;

	return Py_FindMethod( BPy_ThemeUI_methods, ( PyObject * ) self, name );
}

static int ThemeUI_setAttr( BPy_ThemeUI * self, char *name, PyObject * value )
{
	PyObject *attrib = NULL;
	ThemeUI *tui = self->tui;
	int ret = -1;

	if( !strcmp( name, "outline" ) )
		attrib = charRGBA_New( &tui->outline[0] );
	ELSEIF_TUI_RGBA( neutral )
		ELSEIF_TUI_RGBA( action )
		ELSEIF_TUI_RGBA( setting )
		ELSEIF_TUI_RGBA( setting1 )
		ELSEIF_TUI_RGBA( setting2 )
		ELSEIF_TUI_RGBA( num )
		ELSEIF_TUI_RGBA( textfield )
		ELSEIF_TUI_RGBA( textfield_hi )
		ELSEIF_TUI_RGBA( popup )
		ELSEIF_TUI_RGBA( text )
		ELSEIF_TUI_RGBA( text_hi )
		ELSEIF_TUI_RGBA( menu_back )
		ELSEIF_TUI_RGBA( menu_item )
		ELSEIF_TUI_RGBA( menu_hilite )
		ELSEIF_TUI_RGBA( menu_text )
		ELSEIF_TUI_RGBA( menu_text_hi )
		else if( !strcmp( name, "drawType" ) ) {
		int val;

		if( !PyInt_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError,
						    "expected integer value" );

		val = ( int ) PyInt_AsLong( value );
		tui->but_drawtype = (char)EXPP_ClampInt( val,
						   EXPP_THEME_DRAWTYPE_MIN,
						   EXPP_THEME_DRAWTYPE_MAX );
		ret = 0;
	} else if ( !strcmp( name, "iconTheme" ) ) {
		if ( !PyString_Check(value) )
			return EXPP_ReturnIntError( PyExc_TypeError,
						    "expected string value" );
		BLI_strncpy(tui->iconfile, PyString_AsString(value), 80);
		
		BIF_icons_free();
		BIF_icons_init(BIFICONID_LAST+1);
		
		ret = 0;
	} else
		return EXPP_ReturnIntError( PyExc_AttributeError,
					    "attribute not found" );

	if( attrib ) {
		PyObject *pyret = NULL;
		PyObject *valtuple = Py_BuildValue( "(O)", value );

		if( !valtuple )
			return EXPP_ReturnIntError( PyExc_MemoryError,
						    "couldn't create tuple!" );

		pyret = charRGBA_setCol( ( BPy_charRGBA * ) attrib, valtuple );
		Py_DECREF( valtuple );

		if( pyret == Py_None ) {
			Py_DECREF( Py_None );	/* was increfed by charRGBA_setCol */
			ret = 0;
		}

		Py_DECREF( attrib );	/* we're done with it */
	}

	return ret;		/* 0 if all went well */
}


static int ThemeUI_compare( BPy_ThemeUI * a, BPy_ThemeUI * b )
{
	ThemeUI *pa = a->tui, *pb = b->tui;
	return ( pa == pb ) ? 0 : -1;
}

static PyObject *ThemeUI_repr( BPy_ThemeUI * self )
{
	return PyString_FromFormat( "[UI theme from theme \"%s\"]",
				    self->theme->name );
}

static void Theme_dealloc( BPy_Theme * self );
static int Theme_compare( BPy_Theme * a, BPy_Theme * b );
static PyObject *Theme_getAttr( BPy_Theme * self, char *name );
static PyObject *Theme_repr( BPy_Theme * self );

static PyObject *Theme_get( BPy_Theme * self, PyObject * args );
static PyObject *Theme_getName( BPy_Theme * self );
static PyObject *Theme_setName( BPy_Theme * self, PyObject * args );

static PyMethodDef BPy_Theme_methods[] = {
	{"get", ( PyCFunction ) Theme_get, METH_VARARGS,
	 "(param) - Return UI or Space theme object.\n\
(param) - the chosen theme object as an int or a string:\n\
- () - default: UI;\n\
- (i) - int: an entry from the Blender.Window.Types dictionary;\n\
- (s) - string: 'UI' or a space name, like 'VIEW3D', etc."},
	{"getName", ( PyCFunction ) Theme_getName, METH_NOARGS,
	 "() - Return Theme name"},
	{"setName", ( PyCFunction ) Theme_setName, METH_VARARGS,
	 "(s) - Set Theme name"},
	{NULL, NULL, 0, NULL}
};

PyTypeObject Theme_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Blender Theme",	/* tp_name */
	sizeof( BPy_Theme ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) Theme_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) Theme_getAttr,	/* tp_getattr */
	0,			//(setattrfunc) Theme_setAttr,        /* tp_setattr */
	( cmpfunc ) Theme_compare,	/* tp_compare */
	( reprfunc ) Theme_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	0,			/*BPy_Theme_methods,*/ /* tp_methods */
	0,			/* tp_members */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static PyObject *M_Theme_New( PyObject * self, PyObject * args )
{
	char *name = "New Theme";
	BPy_Theme *pytheme = NULL, *base_pytheme = NULL;
	bTheme *btheme = NULL, *newtheme = NULL;

	if( !PyArg_ParseTuple
	    ( args, "|sO!", &name, &Theme_Type, &base_pytheme ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected nothing or a name and optional theme object as arguments" );

	if( base_pytheme )
		btheme = base_pytheme->theme;
	if( !btheme )
		btheme = U.themes.first;

	newtheme = MEM_callocN( sizeof( bTheme ), "theme" );

	if( newtheme )
		pytheme = PyObject_New( BPy_Theme, &Theme_Type );
	if( !pytheme )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't create Theme Data in Blender" );

	memcpy( newtheme, btheme, sizeof( bTheme ) );
	BLI_addhead( &U.themes, newtheme );
	BLI_strncpy( newtheme->name, name, 32 );

	pytheme->theme = newtheme;

	return ( PyObject * ) pytheme;
}

static PyObject *M_Theme_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	bTheme *iter;
	PyObject *ret;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument (or nothing)" );

	iter = U.themes.first;

	if( name ) {		/* (name) - return requested theme */
		BPy_Theme *wanted = NULL;

		while( iter ) {
			if( strcmp( name, iter->name ) == 0 ) {
				wanted = PyObject_New( BPy_Theme,
						       &Theme_Type );
				wanted->theme = iter;
				break;
			}
			iter = iter->next;
		}

		if( !wanted ) {
			char emsg[64];
			PyOS_snprintf( emsg, sizeof( emsg ),
				       "Theme \"%s\" not found", name );
			return EXPP_ReturnPyObjError( PyExc_NameError, emsg );
		}

		ret = ( PyObject * ) wanted;
	}

	else {			/* () - return list with all themes */
		int index = 0;
		PyObject *list = NULL;
		BPy_Theme *pytheme = NULL;

		list = PyList_New( BLI_countlist( &( U.themes ) ) );

		if( !list )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
						      "couldn't create PyList" );

		while( iter ) {
			pytheme = PyObject_New( BPy_Theme, &Theme_Type );
			pytheme->theme = iter;

			if( !pytheme ) {
				Py_DECREF(list);
				return EXPP_ReturnPyObjError
					( PyExc_MemoryError,
					  "couldn't create Theme PyObject" );
			}
			PyList_SET_ITEM( list, index, ( PyObject * ) pytheme );

			iter = iter->next;
			index++;
		}

		ret = list;
	}

	return ret;
}

static PyObject *Theme_get( BPy_Theme * self, PyObject * args )
{
	bTheme *btheme = self->theme;
	ThemeUI *tui = NULL;
	ThemeSpace *tsp = NULL;
	PyObject *pyob = NULL;
	BPy_ThemeUI *retUI = NULL;
	BPy_ThemeSpace *retSpc = NULL;
	int type;

	if( !PyArg_ParseTuple( args, "|O", &pyob ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string or int argument or nothing" );

	if( !pyob ) {		/* (): return list with all names */
		PyObject *ret = PyList_New( EXPP_THEME_NUMBEROFTHEMES );

		if( !ret )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
						      "couldn't create pylist!" );

		type = 0;	/* using as a counter only */

		while( type < EXPP_THEME_NUMBEROFTHEMES ) {
			PyList_SET_ITEM( ret, type,
					 PyString_FromString( themes_map[type].sval ) );
			type++;
		}

		return ret;
	}

	else if( PyInt_Check( pyob ) )	/* (int) */
		type = ( int ) PyInt_AsLong( pyob );
	else if( PyString_Check( pyob ) ) {	/* (str) */
		char *str = PyString_AsString( pyob );
		if( !EXPP_map_case_getIntVal( themes_map, str, &type ) )
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
						      "unknown string argument" );
	} else
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string or int argument or nothing" );

	switch ( type ) {
	case -1:		/* UI */
		tui = &btheme->tui;
		break;
	case SPACE_BUTS:
		tsp = &btheme->tbuts;
		break;
	case SPACE_VIEW3D:
		tsp = &btheme->tv3d;
		break;
	case SPACE_FILE:
		tsp = &btheme->tfile;
		break;
	case SPACE_IPO:
		tsp = &btheme->tipo;
		break;
	case SPACE_INFO:
		tsp = &btheme->tinfo;
		break;
	case SPACE_SOUND:
		tsp = &btheme->tsnd;
		break;
	case SPACE_ACTION:
		tsp = &btheme->tact;
		break;
	case SPACE_NLA:
		tsp = &btheme->tnla;
		break;
	case SPACE_SEQ:
		tsp = &btheme->tseq;
		break;
	case SPACE_IMAGE:
		tsp = &btheme->tima;
		break;
	case SPACE_IMASEL:
		tsp = &btheme->timasel;
		break;
	case SPACE_TEXT:
		tsp = &btheme->text;
		break;
	case SPACE_OOPS:
		tsp = &btheme->toops;
		break;
	case SPACE_TIME:
		tsp = &btheme->ttime;
		break;
	case SPACE_NODE:
		tsp = &btheme->tnode;
		break;
	}

	if( tui ) {
		retUI = PyObject_New( BPy_ThemeUI, &ThemeUI_Type );
		retUI->theme = btheme;
		retUI->tui = tui;
		return ( PyObject * ) retUI;
	} else if( tsp ) {
		retSpc = PyObject_New( BPy_ThemeSpace, &ThemeSpace_Type );
		retSpc->theme = btheme;
		retSpc->tsp = tsp;
		return ( PyObject * ) retSpc;
	} else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "invalid parameter" );
}

static PyObject *Theme_getName( BPy_Theme * self )
{
	return PyString_FromString( self->theme->name );
}

static PyObject *Theme_setName( BPy_Theme * self, PyObject * args )
{
	char *name = NULL;

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	BLI_strncpy( self->theme->name, name, 32 );

	return EXPP_incr_ret( Py_None );
}

PyObject *Theme_Init( void )
{
	PyObject *submodule;

	Theme_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3( "Blender.Window.Theme",
				    M_Theme_methods, M_Theme_doc );

	return submodule;
}

static void Theme_dealloc( BPy_Theme * self )
{
	PyObject_DEL( self );
}

static PyObject *Theme_getAttr( BPy_Theme * self, char *name )
{
	PyObject *attr = Py_None;

	if( !strcmp( name, "name" ) )
		attr = PyString_FromString( self->theme->name );
	else if( !strcmp( name, "__members__" ) )
		attr = Py_BuildValue( "[s]", "name" );

	if( attr != Py_None )
		return attr;

	return Py_FindMethod( BPy_Theme_methods, ( PyObject * ) self, name );
}

static int Theme_compare( BPy_Theme * a, BPy_Theme * b )
{
	bTheme *pa = a->theme, *pb = b->theme;
	return ( pa == pb ) ? 0 : -1;
}

static PyObject *Theme_repr( BPy_Theme * self )
{
	return PyString_FromFormat( "[Theme \"%s\"]", self->theme->name );
}
