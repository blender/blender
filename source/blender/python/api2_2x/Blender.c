/* 
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten, Willian P. Germano, Joseph Gilbert,
 * Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
*/
struct ID; /*keep me up here */

#include "Blender.h" /*This must come first */

/* for open, close in Blender_Load */
#include <fcntl.h>
#include "BDR_editobject.h"	/* exit_editmode() */
#include "BDR_drawmesh.h"	/* set_mipmap() */
#include "BIF_usiblender.h"
#include "BLI_blenlib.h"
#include "BLI_bpath.h"
#include "BLO_writefile.h"
#include "BKE_blender.h"
#include "BKE_exotic.h"
#include "BKE_global.h"
#include "BKE_packedFile.h"
#include "BKE_utildefines.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_text.h"
#include "BKE_ipo.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BIF_space.h"
#include "DNA_space_types.h"		/* script struct */
#include "BSE_headerbuttons.h"
#include "DNA_screen_types.h"	/* for SPACE_VIEW3D */
#include "DNA_userdef_types.h"
#include "DNA_packedFile_types.h"
#include "EXPP_interface.h" /* for bpy_gethome() */
#include "gen_utils.h"
#include "modules.h"
#include "constant.h"
#include "../BPY_extern.h" /* BPY_txt_do_python_Text */
#include "../BPY_menus.h"	/* to update menus */
#include "Armature.h"
#include "BezTriple.h"
#include "Camera.h"
#include "Constraint.h"
#include "Curve.h"
#include "CurNurb.h"
#include "Draw.h"
#include "Effect.h"
#include "Ipo.h"
#include "Ipocurve.h"
#include "IDProp.h"
#include "Key.h"
#include "Lamp.h"
#include "Lattice.h"
#include "Library.h"
#include "Mathutils.h"
#include "Geometry.h"
#include "Mesh.h"
#include "Metaball.h"
#include "Modifier.h"
#include "NMesh.h"
#include "Node.h"
#include "Object.h"
#include "Group.h"
#include "Registry.h"
#include "Scene.h"
#include "Sound.h"
#include "SurfNurb.h"
#include "Sys.h"
#include "Text.h"
#include "Texture.h"
#include "Window.h"
#include "World.h"
#include "Types.h"
#include "Particle.h"

/**********************************************************/
/* Python API function prototypes for the Blender module.	*/
/**********************************************************/
static PyObject *Blender_Set( PyObject * self, PyObject * args );
static PyObject *Blender_Get( PyObject * self, PyObject * value );
static PyObject *Blender_Redraw( PyObject * self, PyObject * args );
static PyObject *Blender_Quit( PyObject * self );
static PyObject *Blender_Load( PyObject * self, PyObject * args );
static PyObject *Blender_Save( PyObject * self, PyObject * args );
static PyObject *Blender_Run( PyObject * self, PyObject * value );
static PyObject *Blender_ShowHelp( PyObject * self, PyObject * script );
static PyObject *Blender_UpdateMenus( PyObject * self);
static PyObject *Blender_PackAll( PyObject * self);
static PyObject *Blender_UnpackAll( PyObject * self, PyObject * value);
static PyObject *Blender_CountPackedFiles( PyObject * self );
static PyObject *Blender_GetPaths( PyObject * self, PyObject *args, PyObject *keywds );
static PyObject *Blender_SaveUndoState( PyObject * self, PyObject *args );
extern PyObject *Text3d_Init( void ); /* missing in some include */

/*****************************************************************************/
/* The following string definitions are used for documentation strings.	 */
/* In Python these will be written to the console when doing a		 */
/* Blender.__doc__	 */
/*****************************************************************************/
static char Blender_Set_doc[] =
	"(request, data) - Update settings in Blender\n\
\n\
(request) A string identifying the setting to change\n\
	'curframe'	- Sets the current frame using the number in data";

static char Blender_Get_doc[] = "(request) - Retrieve settings from Blender\n\
\n\
(request) A string indentifying the data to be returned\n\
	'curframe'	- Returns the current animation frame\n\
	'curtime'	- Returns the current animation time\n\
	'staframe'	- Returns the start frame of the animation\n\
	'endframe'	- Returns the end frame of the animation\n\
	'filename'	- Returns the name of the last file read or written\n\
	'homedir' - Returns Blender's home dir\n\
	'datadir' - Returns the dir where scripts can save their data, if available\n\
	'scriptsdir' - Returns the main dir where scripts are kept, if available\n\
	'uscriptsdir' - Returns the user defined dir for scripts, if available\n\
	'version'	- Returns the Blender version number";

static char Blender_Redraw_doc[] = "() - Redraw all 3D windows";

static char Blender_Quit_doc[] =
	"() - Quit Blender.  The current data is saved as 'quit.blend' before leaving.";

static char Blender_Load_doc[] = "(filename) - Load the given file.\n\
Supported formats:\n\
Blender, DXF, Inventor 1.0 ASCII, VRML 1.0 asc, STL, Videoscape, radiogour.\n\
\n\
Notes:\n\
1 - () - an empty argument loads the default .B.blend file;\n\
2 - if the substring '.B.blend' occurs inside 'filename', the default\n\
.B.blend file is loaded;\n\
3 - If a Blender file is loaded the script ends immediately.\n\
4 - The current data is always preserved as an autosave file, for safety;\n\
5 - This function only works if the script where it's executed is the\n\
only one running at the moment.";

static char Blender_Save_doc[] =
	"(filename) - Save data to a file based on the filename's extension.\n\
Supported are: Blender's .blend and the builtin exporters:\n\
VRML 1.0 (.wrl), Videoscape (.obj), DXF (.dxf) and STL (.stl)\n\
(filename) - A filename with one of the supported extensions.\n\
Note 1: 'filename' should not contain the substring \".B.blend\" in it.\n\
Note 2: only .blend raises an error if file wasn't saved.\n\
\tYou can use Blender.sys.exists(filename) to make sure the file was saved\n\
\twhen writing to one of the other formats.";

static char Blender_Run_doc[] =
	"(script) - Run the given Python script.\n\
(script) - the path to a file or the name of an available Blender Text.";

static char Blender_ShowHelp_doc[] =
"(script) - Show help for the given Python script.\n\
  This will try to open the 'Scripts Help Browser' script, so to have\n\
any help displayed the passed 'script' must be properly documented\n\
with the expected strings (check API ref docs or any bundled script\n\
for examples).\n\n\
(script) - the filename of a script in the default or user defined\n\
           scripts dir (no need to supply the full path name)."; 

static char Blender_UpdateMenus_doc[] =
	"() - Update the menus where scripts are registered.  Only needed for\n\
scripts that save other new scripts in the default or user defined folders.";

static char Blender_PackAll_doc[] =
"() - Pack all files.\n\
All files will packed into the blend file.";
static char Blender_UnpackAll_doc[] =
"(mode) - Unpack files.\n\
All files will be unpacked using specified mode.\n\n\
(mode) - the unpack mode.";

static char Blender_CountPackedFiles_doc[] =
"() - Returns the number of packed files.";

static char Blender_GetPaths_doc[] =
"() - Returns a list of paths used in this blend file.";

static char Blender_SaveUndoState_doc[] =
"(s) - Push an undo with blenders current state.";

/*****************************************************************************/
/* Python method structure definition.		 */
/*****************************************************************************/
static struct PyMethodDef Blender_methods[] = {
	{"Set", Blender_Set, METH_VARARGS, Blender_Set_doc},
	{"Get", Blender_Get, METH_O, Blender_Get_doc},
	{"Redraw", Blender_Redraw, METH_VARARGS, Blender_Redraw_doc},
	{"Quit", ( PyCFunction ) Blender_Quit, METH_NOARGS, Blender_Quit_doc},
	{"Load", Blender_Load, METH_VARARGS, Blender_Load_doc},
	{"Save", Blender_Save, METH_VARARGS, Blender_Save_doc},
	{"Run", Blender_Run, METH_O, Blender_Run_doc},
	{"ShowHelp", Blender_ShowHelp, METH_O, Blender_ShowHelp_doc},
	{"CountPackedFiles", ( PyCFunction ) Blender_CountPackedFiles, METH_NOARGS, Blender_CountPackedFiles_doc},
	{"GetPaths", ( PyCFunction ) Blender_GetPaths, METH_VARARGS|METH_KEYWORDS, Blender_GetPaths_doc},
	{"PackAll", ( PyCFunction ) Blender_PackAll, METH_NOARGS, Blender_PackAll_doc},
	{"UnpackAll", Blender_UnpackAll, METH_O, Blender_UnpackAll_doc},
	{"UpdateMenus", ( PyCFunction ) Blender_UpdateMenus, METH_NOARGS, Blender_UpdateMenus_doc},
	{"SaveUndoState", Blender_SaveUndoState, METH_VARARGS, Blender_SaveUndoState_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Global variables	 */
/*****************************************************************************/
PyObject *g_blenderdict;

/*****************************************************************************/
/* Function:	Blender_Set		 */
/* Python equivalent:	Blender.Set		 */
/*****************************************************************************/
static PyObject *Blender_Set( PyObject * self, PyObject * args )
{
	char *name, *dir = NULL;
	PyObject *arg;

	if( !PyArg_ParseTuple( args, "sO", &name, &arg ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError, "expected 2 args, where the first is always a string" );

	if( StringEqual( name, "curframe" ) ) {
		if( !PyInt_Check( arg ) )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected an integer" );

		G.scene->r.cfra = (int)PyInt_AsLong( arg ) ;
		CLAMP(G.scene->r.cfra, 1, MAXFRAME);

		/*	update all objects, so python scripts can export all objects
		 in a scene without worrying about the view layers */
		scene_update_for_newframe(G.scene, (1<<20) - 1);
		
	} else if (StringEqual( name , "uscriptsdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.pythondir, dir, FILE_MAXDIR);
	} else if (StringEqual( name , "yfexportdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.yfexportdir, dir, FILE_MAXDIR);		
	} else if (StringEqual( name , "fontsdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.fontdir, dir, FILE_MAXDIR);
	} else if (StringEqual( name , "texturesdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.textudir, dir, FILE_MAXDIR);
	} else if (StringEqual( name , "texpluginsdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.plugtexdir, dir, FILE_MAXDIR);
	} else if (StringEqual( name , "seqpluginsdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.plugseqdir, dir, FILE_MAXDIR);
	} else if (StringEqual( name , "renderdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.renderdir, dir, FILE_MAXDIR);
	} else if (StringEqual( name , "soundsdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.sounddir, dir, FILE_MAXDIR);
	} else if (StringEqual( name , "tempdir" ) ) {
		if ( !PyArg_Parse( arg , "s" , &dir ))
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string" );
		BLI_strncpy(U.tempdir, dir, FILE_MAXDIR);
		BLI_where_is_temp( btempdir, 1 );
	} else if (StringEqual( name , "compressfile" ) ) {
		int value = PyObject_IsTrue( arg );
		
		if (value==-1)
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected an integer" );
		
		if (value)
			U.flag |= USER_FILECOMPRESS;
		else
			U.flag &= ~USER_FILECOMPRESS;
	} else if (StringEqual( name , "mipmap" ) ) {
		int value = PyObject_IsTrue( arg );
		
		if (value==-1)
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected an integer" );
		
		if (value)
			U.gameflags &= ~USER_DISABLE_MIPMAP;
		else
			U.gameflags |= USER_DISABLE_MIPMAP;
		
		set_mipmap(!(U.gameflags & USER_DISABLE_MIPMAP));
	}else
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"value given is not a blender setting" ) );
	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:		Blender_Get	 */
/* Python equivalent:	Blender.Get		 */
/*****************************************************************************/
static PyObject *Blender_Get( PyObject * self, PyObject * value )
{
	PyObject *ret = NULL;
	char *str = PyString_AsString(value);

	if( !str )
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected string argument");

	if( StringEqual( str, "curframe" ) )
		ret = PyInt_FromLong( G.scene->r.cfra );
	else if( StringEqual( str, "curtime" ) )
		ret = PyFloat_FromDouble( frame_to_float( G.scene->r.cfra ) );
	else if( StringEqual( str, "rt" ) )
		ret = PyInt_FromLong( (long)frame_to_float( G.rt ) );
	else if( StringEqual( str, "staframe" ) )
		ret = PyInt_FromLong( G.scene->r.sfra );
	else if( StringEqual( str, "endframe" ) )
		ret = PyInt_FromLong( G.scene->r.efra );
	else if( StringEqual( str, "filename" ) ) {
		if (!G.relbase_valid)
			ret = PyString_FromString("");
		else
			ret = PyString_FromString(G.sce);
	}
	else if( StringEqual( str, "homedir" ) ) {
		char *hdir = bpy_gethome(0);
		if( hdir && BLI_exists( hdir ))
			ret = PyString_FromString( hdir );
		else
			ret = EXPP_incr_ret( Py_None );
	}
	else if( StringEqual( str, "datadir" ) ) {
		char datadir[FILE_MAXDIR];
		char *sdir = bpy_gethome(1);

		if (sdir) {
			BLI_make_file_string( "/", datadir, sdir, "bpydata" );
			if( BLI_exists( datadir ) )
				ret = PyString_FromString( datadir );
		}
		if (!ret) ret = EXPP_incr_ret( Py_None );
	}
	else if(StringEqual(str, "udatadir")) {
		if (U.pythondir[0] != '\0') {
			char upydir[FILE_MAX];

			BLI_strncpy(upydir, U.pythondir, FILE_MAX);
			BLI_convertstringcode(upydir, G.sce);

			if (BLI_exists(upydir)) {
				char udatadir[FILE_MAXDIR];

				BLI_make_file_string("/", udatadir, upydir, "bpydata");

				if (BLI_exists(udatadir))
					ret = PyString_FromString(udatadir);
			}
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}
	else if( StringEqual( str, "scriptsdir" ) ) {
		char *sdir = bpy_gethome(1);

		if (sdir)
			ret = PyString_FromString(sdir);
		else
			ret = EXPP_incr_ret( Py_None );
	}
	else if( StringEqual( str, "uscriptsdir" ) ) {
		if (U.pythondir[0] != '\0') {
			char upydir[FILE_MAX];

			BLI_strncpy(upydir, U.pythondir, FILE_MAX);
			BLI_convertstringcode(upydir, G.sce);

			if( BLI_exists( upydir ) )
				ret = PyString_FromString( upydir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}
	/* USER PREFS: */
	else if( StringEqual( str, "yfexportdir" ) ) {
		if (U.yfexportdir[0] != '\0') {
			char yfexportdir[FILE_MAX];

			BLI_strncpy(yfexportdir, U.yfexportdir, FILE_MAX);
			BLI_convertstringcode(yfexportdir, G.sce);

			if( BLI_exists( yfexportdir ) )
				ret = PyString_FromString( yfexportdir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}
	/* fontsdir */
	else if( StringEqual( str, "fontsdir" ) ) {
		if (U.fontdir[0] != '\0') {
			char fontdir[FILE_MAX];

			BLI_strncpy(fontdir, U.fontdir, FILE_MAX);
			BLI_convertstringcode(fontdir, G.sce);

			if( BLI_exists( fontdir ) )
				ret = PyString_FromString( fontdir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}	
	/* texturesdir */
	else if( StringEqual( str, "texturesdir" ) ) {
		if (U.textudir[0] != '\0') {
			char textudir[FILE_MAX];

			BLI_strncpy(textudir, U.textudir, FILE_MAX);
			BLI_convertstringcode(textudir, G.sce);

			if( BLI_exists( textudir ) )
				ret = PyString_FromString( textudir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}		
	/* texpluginsdir */
	else if( StringEqual( str, "texpluginsdir" ) ) {
		if (U.plugtexdir[0] != '\0') {
			char plugtexdir[FILE_MAX];

			BLI_strncpy(plugtexdir, U.plugtexdir, FILE_MAX);
			BLI_convertstringcode(plugtexdir, G.sce);

			if( BLI_exists( plugtexdir ) )
				ret = PyString_FromString( plugtexdir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}			
	/* seqpluginsdir */
	else if( StringEqual( str, "seqpluginsdir" ) ) {
		if (U.plugseqdir[0] != '\0') {
			char plugseqdir[FILE_MAX];

			BLI_strncpy(plugseqdir, U.plugseqdir, FILE_MAX);
			BLI_convertstringcode(plugseqdir, G.sce);

			if( BLI_exists( plugseqdir ) )
				ret = PyString_FromString( plugseqdir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}			
	/* renderdir */
	else if( StringEqual( str, "renderdir" ) ) {
		if (U.renderdir[0] != '\0') {
			char renderdir[FILE_MAX];

			BLI_strncpy(renderdir, U.renderdir, FILE_MAX);
			BLI_convertstringcode(renderdir, G.sce);

			if( BLI_exists( renderdir ) )
				ret = PyString_FromString( renderdir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}		
	/* soundsdir */
	else if( StringEqual( str, "soundsdir" ) ) {
		if (U.sounddir[0] != '\0') {
			char sounddir[FILE_MAX];

			BLI_strncpy(sounddir, U.sounddir, FILE_MAX);
			BLI_convertstringcode(sounddir, G.sce);

			if( BLI_exists( sounddir ) )
				ret = PyString_FromString( sounddir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}		
	/* tempdir */
	else if( StringEqual( str, "tempdir" ) ) {
		if (U.tempdir[0] != '\0') {
			char tempdir[FILE_MAX];

			BLI_strncpy(tempdir, U.tempdir, FILE_MAX);
			BLI_convertstringcode(tempdir, G.sce);

			if( BLI_exists( tempdir ) )
				ret = PyString_FromString( tempdir );
		}
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}
	/* icondir */
	else if( StringEqual( str, "icondir" ) ) {
		
		char icondirstr[FILE_MAXDIR];

		#ifdef WIN32
			BLI_make_file_string("/", icondirstr, BLI_gethome(), "icons");
		#else
			BLI_make_file_string("/", icondirstr, BLI_gethome(), ".blender/icons");
		#endif

		if( BLI_exists( icondirstr ) )
			ret = PyString_FromString( icondirstr );
		
		if (!ret) ret = EXPP_incr_ret(Py_None);
	}
	/* According to the old file (opy_blender.c), the following if
	   statement is a quick hack and needs some clean up. */
	else if( StringEqual( str, "vrmloptions" ) ) {
		ret = PyDict_New(  );

		EXPP_dict_set_item_str( ret, "twoside",
			PyInt_FromLong( U.vrmlflag & USER_VRML_TWOSIDED ) );

		EXPP_dict_set_item_str( ret, "layers",
			PyInt_FromLong( U.vrmlflag & USER_VRML_LAYERS ) );

		EXPP_dict_set_item_str( ret, "autoscale",
			PyInt_FromLong( U.vrmlflag & USER_VRML_AUTOSCALE ) );

	} /* End 'quick hack' part. */
	else if(StringEqual( str, "version" ))
		ret = PyInt_FromLong( G.version );
		
	else if(StringEqual( str, "compressfile" ))
		ret = PyInt_FromLong( (U.flag & USER_FILECOMPRESS) >> 15  );
	else if(StringEqual( str, "mipmap" ))
		ret = PyInt_FromLong( (U.gameflags & USER_DISABLE_MIPMAP)!=0  );
	else if(StringEqual( str, "add_view_align" ))
		ret = PyInt_FromLong( ((U.flag & USER_ADD_VIEWALIGNED)!=0)  );
	else if(StringEqual( str, "add_editmode" ))
		ret = PyInt_FromLong( ((U.flag & USER_ADD_EDITMODE)!=0)  );
	else 
		return EXPP_ReturnPyObjError( PyExc_AttributeError, "unknown attribute" );

	if (ret) return ret;
	else
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
			"could not create the PyObject!");
}

/*****************************************************************************/
/* Function:		Blender_Redraw		 */
/* Python equivalent:	Blender.Redraw			 */
/*****************************************************************************/
static PyObject *Blender_Redraw( PyObject * self, PyObject * args )
{
	return M_Window_Redraw( self, args );
}

/*****************************************************************************/
/* Function:		Blender_Quit		 */
/* Python equivalent:	Blender.Quit			 */
/*****************************************************************************/
static PyObject *Blender_Quit( PyObject * self )
{
	BIF_write_autosave(  );	/* save the current data first */

	exit_usiblender(  );	/* renames last autosave to quit.blend */

	Py_RETURN_NONE;
}

/**
 * Blender.Load
 * loads Blender's .blend, DXF, radiogour(?), STL, Videoscape,
 * Inventor 1.0 ASCII, VRML 1.0 asc.
 */
static PyObject *Blender_Load( PyObject * self, PyObject * args )
{
	char *fname = NULL;
	int keep_oldfname = 0;
	Script *script = NULL;
	char str[32], name[FILE_MAXDIR];
	int file, is_blend_file = 0;

	if( !PyArg_ParseTuple( args, "|si", &fname, &keep_oldfname ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected filename and optional int or nothing as arguments" );

	if( fname ) {
		if( strlen( fname ) > FILE_MAXDIR )	/* G.main->name's max length */
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
						      "filename too long!" );
		else if( !BLI_exists( fname ) )
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
						      "requested file doesn't exist!" );

		if( keep_oldfname )
			BLI_strncpy( name, G.sce, FILE_MAXDIR );
	}

	/* We won't let a new .blend file be loaded if there are still other
	 * scripts running, since loading a new file will close and remove them. */

	if( G.main->script.first != G.main->script.last )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "there are other scripts running at the Scripts win, close them first!" );

	if( fname ) {
		file = open( fname, O_BINARY | O_RDONLY );

		if( file <= 0 ) {
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "cannot open file!" );
		} else {
			read( file, str, 31 );
			close( file );

			if( strncmp( str, "BLEN", 4 ) == 0 )
				is_blend_file = 1;
		}
	} else
		is_blend_file = 1;	/* no arg given means default: .B.blend */

	if( is_blend_file ) {

		int during_slink = during_scriptlink(  );

		/* when loading a .blend file from a scriptlink, the scriptlink pointer
		 * in BPY_do_pyscript becomes invalid during a loop.  Inform it here.
		 * Also do not allow a nested scriptlink (called from inside another)
		 * to load .blend files, to avoid nasty problems. */
		if( during_slink >= 1 ) {
			if( during_slink == 1 )
				disable_where_scriptlink( -1 );
			else {
				return EXPP_ReturnPyObjError
					( PyExc_EnvironmentError,
					  "Blender.Load: cannot load .blend files from a nested scriptlink." );
			}
		}

		/* trick: mark the script so that its script struct won't be freed after
		 * the script is executed (to avoid a double free warning on exit): */
		script = G.main->script.first;
		if( script )
			script->flags |= SCRIPT_GUI;

		BIF_write_autosave(  );	/* for safety let's preserve the current data */
	}

	if( G.obedit )
		exit_editmode(EM_FREEDATA);

	if (G.background) { /* background mode */
		if (is_blend_file)
			BKE_read_file(fname, NULL);
		else {
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"only .blend files can be loaded from command line,\n\
	other file types require interactive mode.");
		}
	}
	else { /* interactive mode */
		/* for safety, any filename with .B.blend is considered the default one.
		 * It doesn't seem necessary to compare file attributes (like st_ino and
		 * st_dev, according to the glibc info pages) to find out if the given
		 * filename, that may have been given with a twisted misgiving path, is the
		 * default one for sure.  Taking any .B.blend file as the default is good
		 * enough here.  Note: the default file requires extra clean-up done by
		 * BIF_read_homefile: freeing the user theme data. */
		if( !fname || ( strstr( fname, ".B.blend" ) && is_blend_file ) )
			BIF_read_homefile(0);
		else
			BIF_read_file( fname );

		if( fname && keep_oldfname ) {
			/*BLI_strncpy(G.main->name, name, FILE_MAXDIR); */
			BLI_strncpy( G.sce, name, FILE_MAXDIR );
		}
	}

	Py_RETURN_NONE;
}

static PyObject *Blender_Save( PyObject * self, PyObject * args )
{
	char *fname = NULL;
	int overwrite = 0, len = 0;
	char *error = NULL;
	Library *li;

	if( !PyArg_ParseTuple( args, "s|i", &fname, &overwrite ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected filename and optional int (overwrite flag) as arguments" );

	for( li = G.main->library.first; li; li = li->id.next ) {
		if( li->parent==NULL && BLI_streq( li->name, fname ) ) {
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
						      "cannot overwrite used library" );
		}
	}

	/* for safety, any filename with .B.blend is considered the default one
	 * and not accepted here. */
	if( strstr( fname, ".B.blend" ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "filename can't contain the substring \".B.blend\" in it." );

	len = strlen( fname );

	if( len > FILE_MAXDIR + FILE_MAXFILE )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "filename is too long!" );
	else if( BLI_exists( fname ) && !overwrite )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "file already exists and overwrite flag was not given." );

	disable_where_script( 1 );	/* to avoid error popups in the write_* functions */

	if( BLI_testextensie( fname, ".blend" ) ) {
		int writeflags;
		if( G.fileflags & G_AUTOPACK )
			packAll(  );
		
		writeflags= G.fileflags & ~G_FILE_COMPRESS;
		if(U.flag & USER_FILECOMPRESS)
		writeflags |= G_FILE_COMPRESS;
		
		if( !BLO_write_file( fname, writeflags, &error ) ) {
			disable_where_script( 0 );
			return EXPP_ReturnPyObjError( PyExc_SystemError,
						      error );
		}
	} else if( BLI_testextensie( fname, ".dxf" ) )
		write_dxf( fname );
	else if( BLI_testextensie( fname, ".stl" ) )
		write_stl( fname );
	else if( BLI_testextensie( fname, ".wrl" ) )
		write_vrml( fname );
	else if( BLI_testextensie( fname, ".obj" ) )
		write_videoscape( fname );
	else {
		disable_where_script( 0 );
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "unknown file extension." );
	}

	disable_where_script( 0 );

	Py_RETURN_NONE;
}

static PyObject *Blender_ShowHelp(PyObject *self, PyObject *script)
{
	char hspath[FILE_MAXDIR + FILE_MAXFILE]; /* path to help_browser.py */
	char *sdir = bpy_gethome(1);
	PyObject *rkeyd = NULL, *arglist = NULL;

	if (!PyString_Check(script))
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"expected a script filename as argument");

	/* first we try to find the help_browser script */

	if (sdir) BLI_make_file_string("/", hspath, sdir, "help_browser.py");

	if (!sdir || (!BLI_exists(hspath) && (U.pythondir[0] != '\0'))) {
			char upydir[FILE_MAX];

			BLI_strncpy(upydir, U.pythondir, FILE_MAX);
			BLI_convertstringcode(upydir, G.sce);
			BLI_make_file_string("/", hspath, upydir, "help_browser.py");

			if (!BLI_exists(hspath))
				return EXPP_ReturnPyObjError(PyExc_RuntimeError,
					"can't find script help_browser.py");
	}

	/* now we store the passed script in the registry dict and call the
	 * help_browser to show help info for it */
	rkeyd = PyDict_New();
	if (!rkeyd)
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"can't create py dictionary!");

	/* note: don't use EXPP_dict_set_item_str for 'script', which is an
	 * argument to the function we're in and so shouldn't be decref'ed: */
	PyDict_SetItemString(rkeyd, "script", script);

	EXPP_dict_set_item_str(bpy_registryDict, "__help_browser", rkeyd);

	arglist = Py_BuildValue("s", hspath);
	Blender_Run(self, arglist);
	Py_DECREF(arglist);

	Py_RETURN_NONE;
}

static PyObject *Blender_Run(PyObject *self, PyObject *value)
{
	char *fname = PyString_AsString(value);
	Text *text = NULL;
	int is_blender_text = 0;
	Script *script = NULL;

	if (!fname)
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"expected a filename or a Blender Text name as argument");

	if (!BLI_exists(fname)) {	/* if there's no such filename ... */
		text = G.main->text.first;	/* try an already existing Blender Text */

		while (text) {
			if (!strcmp(fname, text->id.name + 2)) break;
			text = text->id.next;
		}

		if (!text) {
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"no such file or Blender text");
		}
		else is_blender_text = 1;	/* fn is already a Blender Text */
	}

	else {
		text = add_text(fname);

		if (!text) {
			return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"couldn't create Blender Text from given file");
		}
	}

	/* (this is messy, check Draw.c's Method_Register and Window.c's file
	 * selector for more info)
	 * - caller script is the one that called this Blender_Run function;
	 * - called script is the argument to this function: fname;
	 * To mark scripts whose global dicts can't be freed right after
	 * the script execution (or better, 'first pass', since these scripts
	 * leave callbacks for gui or file/image selectors) we flag them.  But to
	 * get a pointer to them we need to check which one is currently
	 * running (if none we're already at a spacescript).  To make sure only
	 * the called script will have the SCRIPT_RUNNING flag on, we unset it
	 * for the caller script here: */
	script = G.main->script.first;
	while (script) {
		if (script->flags & SCRIPT_RUNNING) break;
		script = script->id.next;
	}

	if (script) script->flags &= ~SCRIPT_RUNNING; /* unset */

	BPY_txt_do_python_Text(text); /* call new script */

	if (script) script->flags |= SCRIPT_RUNNING; /* set */

	if (!is_blender_text) {
		
		/* nice to remember the original filename, so the script can run on reload */
		if (script) {
			strncpy(script->scriptname, fname, sizeof(script->scriptname));
			script->scriptarg[0] = '\0';
		}	
		free_libblock(&G.main->text, text);
	}
	Py_RETURN_NONE;
}

static PyObject * Blender_UpdateMenus( PyObject * self )
{

	BPyMenu_RemoveAllEntries();

	if (BPyMenu_Init(1) == -1)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"invalid scripts dir");

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:		Blender_PackAll		 */
/* Python equivalent:	Blender.PackAll			 */
/*****************************************************************************/
static PyObject *Blender_PackAll( PyObject * self)
{
	packAll();
	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:		Blender_UnpackAll		 */
/* Python equivalent:	Blender.UnpackAll			 */
/*****************************************************************************/
static PyObject *Blender_UnpackAll( PyObject * self, PyObject *value)
{
	int mode = PyInt_AsLong(value);
	
	if (mode==-1)
		return EXPP_ReturnPyObjError( PyExc_ValueError, "expected an int Blender.UnpackModes");
	unpackAll(mode);
	Py_RETURN_NONE;
}
 
/*****************************************************************************/
/* Function:		Blender_CountPackedFiles		 */
/* Python equivalent:	Blender.CountPackedFiles			 */
/*****************************************************************************/
static PyObject *Blender_CountPackedFiles( PyObject * self )
{
	int nfiles = countPackedFiles();
	return PyInt_FromLong( nfiles );
}

/*****************************************************************************/
/* Function:		Blender_GetPaths		 */
/* Python equivalent:	Blender.GetPaths			 */
/*****************************************************************************/
static PyObject *Blender_GetPaths( PyObject * self, PyObject *args, PyObject *keywds )
{
	struct BPathIterator bpi;
	PyObject *list = PyList_New(0), *st; /* stupidly big string to be safe */
	/* be sure there is low chance of the path being too short */
	char filepath_expanded[FILE_MAXDIR*2]; 
	char *lib;
	
	int absolute = 0;
	static char *kwlist[] = {"absolute", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "|i", kwlist, &absolute ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"expected nothing or one bool (0 or 1) as argument" );
	
	BLI_bpathIterator_init(&bpi);
	
	while (!BLI_bpathIterator_isDone(&bpi)) {
		
		/* build the list */
		if (absolute) {
			BLI_bpathIterator_getPathExpanded( &bpi, filepath_expanded );
		} else {
			lib = BLI_bpathIterator_getLib( &bpi );
			if ( lib && ( strcmp(lib, G.sce) ) ) { /* relative path to the library is NOT the same as our blendfile path, return an absolute path */
				BLI_bpathIterator_getPathExpanded( &bpi, filepath_expanded );
			} else {
				BLI_bpathIterator_getPath( &bpi, filepath_expanded );
			}
		}
		st = PyString_FromString(filepath_expanded);
		
		PyList_Append(list, st);
		Py_DECREF(st);
		
		BLI_bpathIterator_step(&bpi);
	}
	BLI_bpathIterator_free(&bpi);
	return list;
}

static PyObject *Blender_SaveUndoState( PyObject * self, PyObject *args )
{
	char *str;
	if ( !PyArg_ParseTuple( args , "s" , &str ))	
		return EXPP_ReturnPyObjError( PyExc_TypeError, "expected a string" );
	BIF_undo_push(str);
	Py_RETURN_NONE;
}

static PyObject *Blender_UnpackModesDict( void )
{
	PyObject *UnpackModes = PyConstant_New(  );
	if( UnpackModes ) {
		BPy_constant *d = ( BPy_constant * ) UnpackModes;
		PyConstant_Insert( d, "EQUAL", PyInt_FromLong((long)PF_EQUAL) );
		PyConstant_Insert( d, "DIFFERS",PyInt_FromLong((long)PF_DIFFERS) );
		PyConstant_Insert( d, "NOFILE", PyInt_FromLong((long)PF_NOFILE) );
		PyConstant_Insert( d, "WRITE_ORIGINAL", PyInt_FromLong((long)PF_WRITE_ORIGINAL) );
		PyConstant_Insert( d, "WRITE_LOCAL", PyInt_FromLong((long)PF_WRITE_LOCAL) );
		PyConstant_Insert( d, "USE_LOCAL", PyInt_FromLong((long)PF_USE_LOCAL) );
		PyConstant_Insert( d, "USE_ORIGINAL", PyInt_FromLong((long)PF_USE_ORIGINAL) );
		PyConstant_Insert( d, "KEEP", PyInt_FromLong((long)PF_KEEP) );
		PyConstant_Insert( d, "NOOP", PyInt_FromLong((long)PF_NOOP) );
		PyConstant_Insert( d, "ASK", PyInt_FromLong((long)PF_EQUAL) );
	}
	return UnpackModes;
}

/*****************************************************************************/
/* Function:		initBlender		 */
/*****************************************************************************/

void M_Blender_Init(void)
{
	PyObject *module;
	PyObject *dict, *smode, *SpaceHandlers, *UnpackModes;
	
	/* G.scene should only aver be NULL if blender is executed in 
	background mode, not loading a blend file and executing a python script eg.
	blender -P somescript.py -b
	The if below solves the segfaults that happen when python runs and
	G.scene is NULL */
	if(G.background && G.main->scene.first==NULL) {
		Scene *sce= add_scene("1");
		/*set_scene(sce);*/ /* causes a crash */
		G.scene= sce;
	}
	
	module = Py_InitModule3("Blender", Blender_methods,
		"The main Blender module");

	types_InitAll();	/* set all our pytypes to &PyType_Type */

	/* constants for packed files*/
	UnpackModes = Blender_UnpackModesDict(  );
	if( UnpackModes )
		PyModule_AddObject( module, "UnpackModes", UnpackModes );
 
	SpaceHandlers = PyConstant_New();
	if (SpaceHandlers) {
		BPy_constant *d = (BPy_constant *)SpaceHandlers;

		PyConstant_Insert(d,"VIEW3D_EVENT",PyInt_FromLong(SPACEHANDLER_VIEW3D_EVENT));
		PyConstant_Insert(d,"VIEW3D_DRAW", PyInt_FromLong(SPACEHANDLER_VIEW3D_DRAW));

		PyModule_AddObject(module, "SpaceHandlers", SpaceHandlers);
	}

	if (G.background)
		smode = PyString_FromString("background");
	else
		smode = PyString_FromString("interactive");

	dict = PyModule_GetDict(module);
	g_blenderdict = dict;

	PyModule_AddIntConstant(module, "TRUE", 1);
	PyModule_AddIntConstant( module, "FALSE", 0 );

	EXPP_dict_set_item_str(dict, "bylink", EXPP_incr_ret_False());
	PyDict_SetItemString(dict, "link", Py_None);
	EXPP_dict_set_item_str(dict, "event", PyString_FromString(""));
	EXPP_dict_set_item_str(dict, "mode", smode);

	PyDict_SetItemString(dict, "Armature", Armature_Init());
	PyDict_SetItemString(dict, "BezTriple", BezTriple_Init());
	PyDict_SetItemString(dict, "BGL", BGL_Init());
	PyDict_SetItemString(dict, "CurNurb", CurNurb_Init());
	PyDict_SetItemString(dict, "Constraint", Constraint_Init());
	PyDict_SetItemString(dict, "Curve", Curve_Init());
	PyDict_SetItemString(dict, "Camera", Camera_Init());
	PyDict_SetItemString(dict, "Draw", Draw_Init());
	PyDict_SetItemString(dict, "Effect", Effect_Init());
	PyDict_SetItemString(dict, "Ipo", Ipo_Init());
	PyDict_SetItemString(dict, "IpoCurve", IpoCurve_Init());
	PyDict_SetItemString(dict, "Image", Image_Init());
	PyDict_SetItemString(dict, "Key", Key_Init());
	PyDict_SetItemString(dict, "Lamp", Lamp_Init());
	PyDict_SetItemString(dict, "Lattice", Lattice_Init());
	PyDict_SetItemString(dict, "Library", oldLibrary_Init());
	PyDict_SetItemString(dict, "Material", Material_Init());
	PyDict_SetItemString(dict, "Mesh", Mesh_Init()); 
	PyDict_SetItemString(dict, "Metaball", Metaball_Init());
	PyDict_SetItemString(dict, "Mathutils", Mathutils_Init());
	PyDict_SetItemString(dict, "Geometry", Geometry_Init());
	PyDict_SetItemString(dict, "Modifier", Modifier_Init());
	PyDict_SetItemString(dict, "NMesh", NMesh_Init());
	PyDict_SetItemString(dict, "Node", Node_Init());
	PyDict_SetItemString(dict, "Noise", Noise_Init());
	PyDict_SetItemString(dict, "Object", Object_Init());
	PyDict_SetItemString(dict, "Particle", ParticleSys_Init());
	PyDict_SetItemString(dict, "Group", Group_Init());
	PyDict_SetItemString(dict, "Registry", Registry_Init());
	PyDict_SetItemString(dict, "Scene", Scene_Init());
	PyDict_SetItemString(dict, "Sound", Sound_Init());
	PyDict_SetItemString(dict, "SurfNurb", SurfNurb_Init());
	PyDict_SetItemString(dict, "sys", sys_Init());
	PyDict_SetItemString(dict, "Types", Types_Init());
	PyDict_SetItemString(dict, "Text", Text_Init());
	PyDict_SetItemString(dict, "Text3d", Text3d_Init());
	PyDict_SetItemString(dict, "Texture", Texture_Init());
	PyDict_SetItemString(dict, "Window", Window_Init());
	PyDict_SetItemString(dict, "World", World_Init());

}
