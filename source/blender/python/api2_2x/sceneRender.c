/* 
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can Redistribute it and/or
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
 * Contributor(s): Joseph Gilbert, Dietrich Bollmann
 *
 * ***** END GPL LICENSE BLOCK *****
*/
struct View3D; /* keep me up here */

#include "sceneRender.h" /*This must come first*/

#include "MEM_guardedalloc.h"
				 
#include "DNA_image_types.h"
#include "DNA_node_types.h"

#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_node.h"

#include "BIF_drawscene.h"
#include "BIF_renderwin.h"
#include "BIF_writeimage.h"
#include "BIF_meshtools.h"

#include "BLI_blenlib.h"

#include "RE_pipeline.h"

#include "mydevice.h"
#include "butspace.h"
#include "blendef.h"
#include "gen_utils.h"
#include "gen_library.h"

#include "../BPY_extern.h" /* for BPY_do_all_scripts() */
#include "Scene.h"
#include "Group.h"

/* local defines */
#define PY_NONE		     0
#define PY_LOW		     1
#define PY_MEDIUM	     2
#define PY_HIGH		     3
#define PY_HIGHER	     4
#define PY_BEST		     5
#define PY_USEAOSETTINGS 6
#define PY_SKYDOME	     1
#define PY_FULL	         2

enum rend_constants {
	EXPP_RENDER_ATTR_XPARTS = 0,
	EXPP_RENDER_ATTR_YPARTS,
	EXPP_RENDER_ATTR_ASPECTX,
	EXPP_RENDER_ATTR_ASPECTY,
	EXPP_RENDER_ATTR_CFRAME,
	EXPP_RENDER_ATTR_SFRAME,
	EXPP_RENDER_ATTR_EFRAME,
	EXPP_RENDER_ATTR_FPS,
	EXPP_RENDER_ATTR_FPS_BASE,
	EXPP_RENDER_ATTR_SIZEX,
	EXPP_RENDER_ATTR_SIZEY,
	EXPP_RENDER_ATTR_GAUSSFILTER,
	EXPP_RENDER_ATTR_MBLURFACTOR,
	EXPP_RENDER_ATTR_BAKEMARGIN,
	EXPP_RENDER_ATTR_BAKEMODE,
 	EXPP_RENDER_ATTR_BAKEDIST,
	EXPP_RENDER_ATTR_BAKENORMALSPACE,
	EXPP_RENDER_ATTR_BAKEBIAS
};

#define EXPP_RENDER_ATTR_CFRA                 2
#define EXPP_RENDER_ATTR_ANTISHIFT            3
#define EXPP_RENDER_ATTR_EDGEINT              4
#define EXPP_RENDER_ATTR_EFRA                 5
#define EXPP_RENDER_ATTR_QUALITY             11
#define EXPP_RENDER_ATTR_GAUSS               13
#define EXPP_RENDER_ATTR_BLURFAC             14

#define EXPP_RENDER_ATTR_YF_EXPOSURE         20
#define EXPP_RENDER_ATTR_YF_GAMMA            21
#define EXPP_RENDER_ATTR_YF_GIDEPTH          22
#define EXPP_RENDER_ATTR_YF_GICDEPTH         23
#define EXPP_RENDER_ATTR_YF_GIPHOTONCOUNT    24
#define EXPP_RENDER_ATTR_YF_GIPHOTONMIXCOUNT 25
#define EXPP_RENDER_ATTR_YF_GIPHOTONRADIUS   26
#define EXPP_RENDER_ATTR_YF_GIPIXPERSAMPLE   27
#define EXPP_RENDER_ATTR_YF_GIPOWER          28
#define EXPP_RENDER_ATTR_YF_GIREFINE         29
#define EXPP_RENDER_ATTR_YF_GISHADOWQUAL     30
#define EXPP_RENDER_ATTR_YF_RAYBIAS          31
#define EXPP_RENDER_ATTR_YF_PROCCOUNT        32
#define EXPP_RENDER_ATTR_YF_RAYDEPTH         33
#define EXPP_RENDER_ATTR_YF_GIMETHOD         34
#define EXPP_RENDER_ATTR_YF_GIQUALITY        35


/* Render doc strings */
static char M_Render_doc[] = "The Blender Render module";

/* deprecated callbacks */
static PyObject *RenderData_SetRenderPath( BPy_RenderData *self,
		PyObject *args );
static PyObject *RenderData_SetBackbufPath( BPy_RenderData *self,
		PyObject *args );
static PyObject *RenderData_SetOversamplingLevel( BPy_RenderData * self,
		PyObject * args );
static PyObject *RenderData_SetRenderWinSize( BPy_RenderData * self,
		PyObject * args );
static PyObject *RenderData_SetBorder( BPy_RenderData * self,
		PyObject * args );
static PyObject *RenderData_SetRenderer( BPy_RenderData * self,
		PyObject * args );
static PyObject *RenderData_SetImageType( BPy_RenderData * self,
		PyObject * args );
static PyObject *RenderData_Render( BPy_RenderData * self );
static PyObject *RenderData_Bake( BPy_RenderData * self );

/* BPy_RenderData Internal Protocols */

static PyObject *RenderData_repr( BPy_RenderData * self )
{
	if( self->renderContext )
		return PyString_FromFormat( "[RenderData \"%s\"]",
					    self->scene->id.name + 2 );
	else
		return PyString_FromString( "NULL" );
}

/***************************************************************************/
/* local utility routines for manipulating data                            */
/***************************************************************************/
static PyObject *M_Render_BitToggleInt( PyObject * args, int setting,
					int *structure )
{
	int flag;

	if( !PyArg_ParseTuple( args, "i", &flag ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected TRUE or FALSE (1 or 0)" ) );

	if( flag < 0 || flag > 1 )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected TRUE or FALSE (1 or 0)" ) );

	if( flag )
		*structure |= setting;
	else
		*structure &= ~setting;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;

}

static PyObject *M_Render_BitToggleShort( PyObject * args, short setting,
					  short *structure )
{
	int flag;

	if( !PyArg_ParseTuple( args, "i", &flag ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected TRUE or FALSE (1 or 0)" ) );

	if( flag < 0 || flag > 1 )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected TRUE or FALSE (1 or 0)" ) );

	if( flag )
		*structure |= setting;
	else
		*structure &= ~setting;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;

}

static PyObject *M_Render_GetSetAttributeFloat( PyObject * args,
						float *structure, float min,
						float max )
{
	float property = -10.0f;
	char error[48];

	if( !PyArg_ParseTuple( args, "|f", &property ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError, "expected float" ) );

	if( property != -10.0f ) {
		if( property < min || property > max ) {
			sprintf( error, "out of range - expected %f to %f",
				 min, max );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_AttributeError, error ) );
		}

		*structure = property;
		EXPP_allqueue( REDRAWBUTSSCENE, 0 );
		Py_RETURN_NONE;
	} else
		return PyFloat_FromDouble( *structure );
}

static PyObject *M_Render_GetSetAttributeShort( PyObject * args,
						short *structure, int min,
						int max )
{
	short property = -10;
	char error[48];

	if( !PyArg_ParseTuple( args, "|h", &property ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int" ) );

	if( property != -10 ) {
		if( property < min || property > max ) {
			sprintf( error, "out of range - expected %d to %d",
				 min, max );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_AttributeError, error ) );
		}

		*structure = property;
		EXPP_allqueue( REDRAWBUTSSCENE, 0 );
		Py_RETURN_NONE;
	} else
		return PyInt_FromLong( (long int)*structure );
}

static PyObject *M_Render_GetSetAttributeInt( PyObject * args, int *structure,
					      int min, int max )
{
	int property = -10;
	char error[48];

	if( !PyArg_ParseTuple( args, "|i", &property ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int" ) );

	if( property != -10 ) {
		if( property < min || property > max ) {
			sprintf( error, "out of range - expected %d to %d",
				 min, max );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_AttributeError, error ) );
		}

		*structure = property;
		
		/* compare memory locations, not values */
		if (&G.scene->r.cfra == structure) {
			/* are we changing the current frame?
			update all objects, so python scripts can export all objects
			in a scene without worrying about the view layers */
			scene_update_for_newframe(G.scene, (1<<20) - 1);
		}
		
		/*I dont think this should be here, whatif the scene is not the current scene - campbell*/
		EXPP_allqueue( REDRAWBUTSSCENE, 0 );
		
		Py_RETURN_NONE;
	} else
		return  PyInt_FromLong( *structure );
}


static void M_Render_DoSizePreset( BPy_RenderData * self, short xsch,
				   short ysch, short xasp, short yasp,
				   short size, short xparts, short yparts,
				   short fps, float fps_base,
				   float a, float b, float c,
				   float d )
{
	self->renderContext->xsch = xsch;
	self->renderContext->ysch = ysch;
	self->renderContext->xasp = xasp;
	self->renderContext->yasp = yasp;
	self->renderContext->size = size;
	self->renderContext->frs_sec = fps;
	self->renderContext->frs_sec_base = fps_base;
	self->renderContext->xparts = xparts;
	self->renderContext->yparts = yparts;

	BLI_init_rctf( &self->renderContext->safety, a, b, c, d );
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	EXPP_allqueue( REDRAWVIEWCAM, 0 );
}

/** set / get boolean */

static int M_Render_setBooleanShort( BPy_RenderData * self, PyObject *value, short* var )
{
	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
									"expected boolean value" );

	*var = (PyInt_AsLong( value )) ? 1 : 0;

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	return 0;
}

static PyObject *M_Render_getBooleanShort( BPy_RenderData * self, short var )
{
	return PyInt_FromLong( (long) var );
}

/** set / get float */

static int M_Render_setFloat( BPy_RenderData *self, PyObject *value, float *var, float min, float max )
{
	float val;
	char error[48];

	if( !PyFloat_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
									"expected float value" );

	val = (float) PyFloat_AsDouble( value );

	/* check range */
	if ( val < min || val > max ) {
		sprintf( error, "out of range - expected %f to %f", min, max );
		return EXPP_ReturnIntError( PyExc_TypeError,error );
	}

	*var = val;

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	return 0;
}

static PyObject *M_Render_getFloat( BPy_RenderData *self, float var )
{
	return PyFloat_FromDouble( (double) var );
}

/** set / get integer */

static int M_Render_setInt( BPy_RenderData *self, PyObject *value, int *var, int min, int max )
{
	int val;
	char error[48];

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
									"expected integer value" );

	val = (int) PyInt_AsLong( value );

	/* check range */
	if ( val < min || val > max ) {
		sprintf( error, "out of range - expected %d to %d", min, max );
		return EXPP_ReturnIntError( PyExc_TypeError,error );
	}

	*var = val;

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	return 0;
}

static PyObject *M_Render_getInt( BPy_RenderData *self, int var )
{
	return PyInt_FromLong( (long) var );
}

/***************************************************************************/
/* Render Module Function Definitions                                      */
/***************************************************************************/

PyObject *M_Render_CloseRenderWindow( PyObject * self )
{
	BIF_close_render_display(  );
	Py_RETURN_NONE;
}

PyObject *M_Render_SetRenderWinPos( PyObject * self, PyObject * args )
{
	PyObject *list = NULL;
	char *loc = NULL;
	int x;

	if( !PyArg_ParseTuple( args, "O!", &PyList_Type, &list ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected a list" ) );

	G.winpos = 0;
	for( x = 0; x < PyList_Size( list ); x++ ) {
		if( !PyArg_Parse( PyList_GetItem( list, x ), "s", &loc ) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
						      "python list not parseable" );
		}
		if( strcmp( loc, "SW" ) == 0 || strcmp( loc, "sw" ) == 0 )
			G.winpos |= 1;
		else if( strcmp( loc, "S" ) == 0 || strcmp( loc, "s" ) == 0 )
			G.winpos |= 2;
		else if( strcmp( loc, "SE" ) == 0 || strcmp( loc, "se" ) == 0 )
			G.winpos |= 4;
		else if( strcmp( loc, "W" ) == 0 || strcmp( loc, "w" ) == 0 )
			G.winpos |= 8;
		else if( strcmp( loc, "C" ) == 0 || strcmp( loc, "c" ) == 0 )
			G.winpos |= 16;
		else if( strcmp( loc, "E" ) == 0 || strcmp( loc, "e" ) == 0 )
			G.winpos |= 32;
		else if( strcmp( loc, "NW" ) == 0 || strcmp( loc, "nw" ) == 0 )
			G.winpos |= 64;
		else if( strcmp( loc, "N" ) == 0 || strcmp( loc, "n" ) == 0 )
			G.winpos |= 128;
		else if( strcmp( loc, "NE" ) == 0 || strcmp( loc, "ne" ) == 0 )
			G.winpos |= 256;
		else
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
						      "list contains unknown string" );
	}
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}

PyObject *M_Render_EnableDispView( PyObject * self )
{
	G.displaymode = R_DISPLAYIMAGE;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}

PyObject *M_Render_EnableDispWin( PyObject * self )
{
	G.displaymode = R_DISPLAYWIN;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}


/***************************************************************************/
/* BPy_RenderData Function Definitions                                     */
/***************************************************************************/

PyObject *RenderData_Render( BPy_RenderData * self )
{
	Scene *oldsce;
	/* unlock to prevent a deadlock when there are pynodes: */
	PyThreadState *tstate = NULL;

	if (!G.background) {
		oldsce = G.scene;
		set_scene( self->scene );
		tstate = PyEval_SaveThread();
		BIF_do_render( 0 );
		set_scene( oldsce );
	}
	else { /* background mode (blender -b file.blend -P script) */
		Render *re= RE_NewRender(G.scene->id.name);

		int end_frame = G.scene->r.efra;

		if (G.scene != self->scene)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
				"scene to render in bg mode must be the active scene");

		G.scene->r.efra = G.scene->r.sfra;

		if (G.f & G_DOSCRIPTLINKS)
			BPY_do_all_scripts(SCRIPT_RENDER, 0);

		tstate = PyEval_SaveThread();

		RE_BlenderAnim(re, G.scene, G.scene->r.sfra, G.scene->r.efra, G.scene->frame_step);

		BPY_do_all_scripts(SCRIPT_POSTRENDER, 0);

		G.scene->r.efra = end_frame;
	}

	PyEval_RestoreThread(tstate);
	Py_RETURN_NONE;
}

/***************************************************************************/
/* BPy_Bake Function Definitions                                           */
/***************************************************************************/

PyObject *RenderData_Bake( BPy_RenderData * self )
{
	char *error_msg = NULL;
	Scene *oldsce;

	oldsce = G.scene;
	set_scene( self->scene );
	
	objects_bake_render(0, &error_msg);
	
	set_scene( oldsce );
	
	if (error_msg)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError, error_msg );
	
	Py_RETURN_NONE;
}

/* 
 * This will save the rendered image to an output file path already defined.
 */
PyObject *RenderData_SaveRenderedImage ( BPy_RenderData * self, PyObject *args )
{
	char dir[FILE_MAXDIR * 2], str[FILE_MAXFILE * 2];
	char *name_str, filepath[FILE_MAXDIR+FILE_MAXFILE];
	RenderResult *rr = NULL;
	int zbuff;
	
	if( !PyArg_ParseTuple( args, "s|i", &name_str, &zbuff ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a filename (string) and optional int" );
	
	if (G.background)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"saveRenderedImage does not work in background mode, use renderAnim() instead" );
	
	if( strlen(self->renderContext->pic) + strlen(name_str)
			>= sizeof(filepath) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"full filename too long" );

	if (zbuff !=0	) zbuff = 1; /*required 1/0 */ /* removed! (ton) */

	BLI_strncpy( filepath, self->renderContext->pic, sizeof(filepath) );
	strcat(filepath, name_str);

	rr = RE_GetResult(RE_GetRender(G.scene->id.name));
	if(!rr) {
		return EXPP_ReturnPyObjError (PyExc_ValueError, "No image rendered");
	} else {
		if(G.ima[0]==0) {
			strcpy(dir, G.sce);
			BLI_splitdirstring(dir, str);
			strcpy(G.ima, dir);
		}
		BIF_save_rendered_image(filepath);
	}
	Py_RETURN_NONE;
}

PyObject *RenderData_RenderAnim( BPy_RenderData * self )
{
	Scene *oldsce;
	/* unlock to prevent a deadlock when there are pynodes: */
	PyThreadState *tstate = NULL;
		
	if (!G.background) {
		oldsce = G.scene;
		set_scene( self->scene );
		tstate = PyEval_SaveThread();
		BIF_do_render( 1 );
		set_scene( oldsce );
	}
	else { /* background mode (blender -b file.blend -P script) */
		Render *re= RE_NewRender(G.scene->id.name);
		
		if (G.scene != self->scene)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
				"scene to render in bg mode must be the active scene");

		if (G.scene->r.sfra > G.scene->r.efra)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
				"start frame must be less or equal to end frame");

		if (G.f & G_DOSCRIPTLINKS)
			BPY_do_all_scripts(SCRIPT_RENDER, 1);

		tstate = PyEval_SaveThread();
		RE_BlenderAnim(re, G.scene, G.scene->r.sfra, G.scene->r.efra, G.scene->frame_step);

		if (G.f & G_DOSCRIPTLINKS)
			BPY_do_all_scripts(SCRIPT_POSTRENDER, 1);
	}

	PyEval_RestoreThread(tstate);
	Py_RETURN_NONE;
}

PyObject *RenderData_Play( BPy_RenderData * self )
{
	char file[FILE_MAXDIR + FILE_MAXFILE];
	extern char bprogname[];
	char str[FILE_MAXDIR + FILE_MAXFILE];
	int pos[2], size[2];
	char txt[64];

#ifdef WITH_QUICKTIME
	if( self->renderContext->imtype == R_QUICKTIME ) {

		strcpy( file, self->renderContext->pic );
		BLI_convertstringcode( file, G.sce );
		BLI_make_existing_file( file );
		if( BLI_strcasecmp( file + strlen( file ) - 4, ".mov" ) ) {
			sprintf( txt, "%04d_%04d.mov",
				 ( self->renderContext->sfra ),
				 ( self->renderContext->efra ) );
			strcat( file, txt );
		}
	} else
#endif
	{

		strcpy( file, self->renderContext->pic );
		BLI_convertstringcode( file, G.sce );
		
		BLI_make_existing_file( file );
		if( BLI_strcasecmp( file + strlen( file ) - 4, ".avi" ) ) {
			sprintf( txt, "%04d_%04d.avi",
				 ( self->renderContext->sfra ),
				 ( self->renderContext->efra ) );
			strcat( file, txt );
		}
	}
	if( BLI_exist( file ) ) {
		calc_renderwin_rectangle(640, 480, G.winpos, pos, size);
		sprintf( str, "%s -a -p %d %d \"%s\"", bprogname, pos[0],
			 pos[1], file );
		system( str );
	} else {
		BKE_makepicstring( file, G.scene->r.pic, self->renderContext->sfra, G.scene->r.imtype);
		if( BLI_exist( file ) ) {
			calc_renderwin_rectangle(640, 480, G.winpos, pos, size);
#ifdef WIN32
			sprintf( str, "%s -a -p %d %d \"%s\"", bprogname,
				 pos[0], pos[1], file );
#else
			sprintf( str, "\"%s\" -a -p %d %d \"%s\"", bprogname,
				 pos[0], pos[1], file );
#endif
			system( str );
		} else
			sprintf( "Can't find image: %s", file );
	}

	Py_RETURN_NONE;
}

PyObject *RenderData_EnableBackbuf( BPy_RenderData * self, PyObject * args )
{
	return M_Render_BitToggleShort( args, 1,
					&self->renderContext->bufflag );
}

PyObject *RenderData_EnableExtensions( BPy_RenderData * self, PyObject * args )
{
	return M_Render_BitToggleInt( args, R_EXTENSION,
					&self->renderContext->scemode );
}

PyObject *RenderData_EnableSequencer( BPy_RenderData * self, PyObject * args )
{
	return M_Render_BitToggleInt( args, R_DOSEQ,
					&self->renderContext->scemode );
}

PyObject *RenderData_EnableRenderDaemon( BPy_RenderData * self,
					 PyObject * args )
{
	return M_Render_BitToggleInt( args, R_BG_RENDER,
					&self->renderContext->scemode );
}

PyObject *RenderData_EnableToonShading( BPy_RenderData * self,
					PyObject * args )
{
	return M_Render_BitToggleInt( args, R_EDGE,
				      &self->renderContext->mode );
}

PyObject *RenderData_EdgeIntensity( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeShort( args,
					      &self->renderContext->edgeint, 0,
					      255 );
}

PyObject *RenderData_SetEdgeColor( BPy_RenderData * self, PyObject * args )
{
	float red, green, blue;

	if( !PyArg_ParseTuple( args, "fff", &red, &green, &blue ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected three floats" ) );

	if( red < 0 || red > 1 )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (red)" ) );
	if( green < 0 || green > 1 )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (green)" ) );
	if( blue < 0 || blue > 1 )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (blue)" ) );

	self->renderContext->edgeR = red;
	self->renderContext->edgeG = green;
	self->renderContext->edgeB = blue;

	Py_RETURN_NONE;
}

PyObject *RenderData_GetEdgeColor( BPy_RenderData * self )
{
	char rgb[24];

	sprintf( rgb, "[%.3f,%.3f,%.3f]", self->renderContext->edgeR,
		 self->renderContext->edgeG, self->renderContext->edgeB );
	return PyString_FromString( rgb );
}

PyObject *RenderData_EnableOversampling( BPy_RenderData * self,
					 PyObject * args )
{
	return M_Render_BitToggleInt( args, R_OSA,
				      &self->renderContext->mode );
}

static int RenderData_setOSALevel( BPy_RenderData * self,
		PyObject * value )
{
	int level;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	level = PyInt_AsLong( value );
	if( level != 5 && level != 8 && level != 11 && level != 16 )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"expected 5, 8, 11, or 16" );

	self->renderContext->osa = (short)level;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	return 0;
}

PyObject *RenderData_EnableMotionBlur( BPy_RenderData * self, PyObject * args )
{
	return M_Render_BitToggleInt( args, R_MBLUR,
				      &self->renderContext->mode );
}

PyObject *RenderData_MotionBlurLevel( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeFloat( args,
					      &self->renderContext->blurfac,
					      0.01f, 5.0f );
}

PyObject *RenderData_PartsX( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeShort( args,
					      &self->renderContext->xparts, 1,
					      512 );
}

PyObject *RenderData_PartsY( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeShort( args,
					      &self->renderContext->yparts, 1,
					      64 );
}

PyObject *RenderData_EnableSky( BPy_RenderData * self )
{
	self->renderContext->alphamode = R_ADDSKY;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}

PyObject *RenderData_EnablePremultiply( BPy_RenderData * self )
{
	self->renderContext->alphamode = R_ALPHAPREMUL;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}

PyObject *RenderData_EnableKey( BPy_RenderData * self )
{
	self->renderContext->alphamode = R_ALPHAKEY;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}

PyObject *RenderData_EnableShadow( BPy_RenderData * self, PyObject * args )
{
	return M_Render_BitToggleInt( args, R_SHADOW,
				      &self->renderContext->mode );
}

PyObject *RenderData_EnableEnvironmentMap( BPy_RenderData * self,
					   PyObject * args )
{
	return M_Render_BitToggleInt( args, R_ENVMAP,
				      &self->renderContext->mode );
}

PyObject *RenderData_EnablePanorama( BPy_RenderData * self, PyObject * args )
{
	return M_Render_BitToggleInt( args, R_PANORAMA,
				      &self->renderContext->mode );
}

PyObject *RenderData_EnableRayTracing( BPy_RenderData * self, PyObject * args )
{
	return M_Render_BitToggleInt( args, R_RAYTRACE,
				      &self->renderContext->mode );
}

PyObject *RenderData_EnableRadiosityRender( BPy_RenderData * self,
					    PyObject * args )
{
	return M_Render_BitToggleInt( args, R_RADIO,
				      &self->renderContext->mode );
}
PyObject *RenderData_EnableFieldRendering( BPy_RenderData * self,
					   PyObject * args )
{
	return M_Render_BitToggleInt( args, R_FIELDS,
				      &self->renderContext->mode );
}

PyObject *RenderData_EnableOddFieldFirst( BPy_RenderData * self,
					  PyObject * args )
{
	return M_Render_BitToggleInt( args, R_ODDFIELD,
				      &self->renderContext->mode );
}

PyObject *RenderData_EnableFieldTimeDisable( BPy_RenderData * self,
					     PyObject * args )
{
	return M_Render_BitToggleInt( args, R_FIELDSTILL,
				      &self->renderContext->mode );
}

PyObject *RenderData_EnableGaussFilter( BPy_RenderData * self,
					PyObject * args )
{
	return M_Render_BitToggleInt( args, R_GAUSS,
				      &self->renderContext->mode );
	
	/* note, this now is obsolete (ton) */
	/* we now need a call like RenderData_SetFilter() or so */
	/* choices are listed in DNA_scene_types.h (search filtertype) */
}

PyObject *RenderData_EnableBorderRender( BPy_RenderData * self,
					 PyObject * args )
{
	return M_Render_BitToggleInt( args, R_BORDER,
				      &self->renderContext->mode );
}

static int RenderData_setBorder( BPy_RenderData * self, PyObject * args )
{
	float xmin, ymin, xmax, ymax;

	if( PyList_Check( args ) )
		args = PySequence_Tuple( args );
	else
		Py_INCREF( args );

	if( !PyArg_ParseTuple( args, "ffff", &xmin, &ymin, &xmax, &ymax ) ) {
		Py_DECREF( args );
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected four floats" );
	}

	self->renderContext->border.xmin = EXPP_ClampFloat( xmin, 0.0, 1.0 );
	self->renderContext->border.xmax = EXPP_ClampFloat( xmax, 0.0, 1.0 );
	self->renderContext->border.ymin = EXPP_ClampFloat( ymin, 0.0, 1.0 );
	self->renderContext->border.ymax = EXPP_ClampFloat( ymax, 0.0, 1.0 );

	EXPP_allqueue( REDRAWVIEWCAM, 1 );

	Py_DECREF( args );
	return 0;
}

static PyObject *RenderData_getBorder( BPy_RenderData * self )
{
	return Py_BuildValue( "[ffff]", 
			self->renderContext->border.xmin,
			self->renderContext->border.ymin,
			self->renderContext->border.xmax,
			self->renderContext->border.ymax );
}

PyObject *RenderData_EnableGammaCorrection( BPy_RenderData * self,
					    PyObject * args )
{
	return M_Render_BitToggleInt( args, R_GAMMA,
				      &self->renderContext->mode );
}

PyObject *RenderData_GaussFilterSize( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeFloat( args,
					      &self->renderContext->gauss,
					      0.5f, 1.5f );
}

PyObject *RenderData_AspectRatioX( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeFloat( args, &self->renderContext->xasp,
					      1.0f, 200.0f );
}

PyObject *RenderData_AspectRatioY( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeFloat( args, &self->renderContext->yasp,
					      1.0f, 200.0f );
}

PyObject *RenderData_StartFrame( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeInt( args, &self->renderContext->sfra,
					    1, MAXFRAME );
}

PyObject *RenderData_CurrentFrame( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeInt( args, &self->renderContext->cfra,
					    1, MAXFRAME );
}

PyObject *RenderData_EndFrame( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeInt( args, &self->renderContext->efra,
					    1, MAXFRAME );
}

PyObject *RenderData_ImageSizeX( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeShort( args, &self->renderContext->xsch,
					      4, 10000 );
}

PyObject *RenderData_ImageSizeY( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeShort( args, &self->renderContext->ysch,
					      4, 10000 );
}

static int RenderData_setRenderer( BPy_RenderData * self, PyObject * value )
{
	int type;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected constant INTERNAL or YAFRAY" );

	type = PyInt_AsLong( value );
	if( type == R_INTERN )
		self->renderContext->renderer = R_INTERN;
	else if( type == R_YAFRAY )
		self->renderContext->renderer = R_YAFRAY;
	else
		return EXPP_ReturnIntError( PyExc_ValueError,
				"expected constant INTERNAL or YAFRAY" );

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	return 0;
}

PyObject *RenderData_EnableCropping( void )
{
/*	return M_Render_BitToggleInt( args, R_MOVIECROP,
				      &self->renderContext->mode );
*/
	printf("obsolete: movie cropping option is now default\n");
	Py_RETURN_NONE;
}


static int RenderData_setImageType( BPy_RenderData *self, PyObject *value )
{
	int type;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int constant" );

	type = PyInt_AS_LONG( value );

	/*
	 * this same logic and more is in buttons_scene.c imagetype_pup code but
	 * only in generating strings for the popup menu, no way to reuse that :(
	 */

	switch( type ) {
	case R_AVIRAW :
	case R_AVIJPEG :
	case R_TARGA :
	case R_RAWTGA :
	case R_RADHDR :
	case R_PNG :
	case R_BMP :
	case R_JPEG90 :
	case R_HAMX :
	case R_IRIS :
	case R_IRIZ :
	case R_TIFF :
	case R_CINEON :
	case R_DPX :
#ifdef _WIN32
	case R_AVICODEC :
#endif
#ifdef WITH_OPENEXR
	case R_OPENEXR :
	case R_MULTILAYER :
#endif
#ifdef WITH_FFMPEG
	case R_FFMPEG :
#endif
		self->renderContext->imtype = type;
		break;
	case R_QUICKTIME :
		if( G.have_quicktime ) {
			self->renderContext->imtype = R_QUICKTIME;
			break;
		}
	default:
		return EXPP_ReturnIntError( PyExc_ValueError,
				"unknown constant - see modules dict for help" );
	}

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	return 0;
}

PyObject *RenderData_Quality( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeShort( args,
					      &self->renderContext->quality,
					      10, 100 );
}

PyObject *RenderData_FramesPerSec( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeShort( args,
					      &self->renderContext->frs_sec, 1,
					      120 );
}

PyObject *RenderData_EnableGrayscale( BPy_RenderData * self )
{
	self->renderContext->planes = R_PLANESBW;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}

PyObject *RenderData_EnableRGBColor( BPy_RenderData * self )
{
	self->renderContext->planes = R_PLANES24;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}

PyObject *RenderData_EnableRGBAColor( BPy_RenderData * self )
{
	self->renderContext->planes = R_PLANES32;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	Py_RETURN_NONE;
}

PyObject *RenderData_SizePreset( BPy_RenderData * self, PyObject * args )
{
	int type;

	if( !PyArg_ParseTuple( args, "i", &type ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected constant" ) );

	if( type == B_PR_PAL ) {
		M_Render_DoSizePreset( self, 720, 576, 54, 51, 100,
				       self->renderContext->xparts,
				       self->renderContext->yparts, 25, 1.0f,
				       0.1f,
				       0.9f, 0.1f, 0.9f );
		self->renderContext->mode &= ~R_PANORAMA;
		BLI_init_rctf( &self->renderContext->safety, 0.1f, 0.9f, 0.1f,
			       0.9f );
	} else if( type == B_PR_NTSC ) {
		M_Render_DoSizePreset( self, 720, 480, 10, 11, 100, 1, 1,
				       30, 1.001f, 
				       0.1f, 0.9f, 0.1f, 0.9f );
		self->renderContext->mode &= ~R_PANORAMA;
		BLI_init_rctf( &self->renderContext->safety, 0.1f, 0.9f, 0.1f,
			       0.9f );
	} else if( type == B_PR_PRESET ) {
		M_Render_DoSizePreset( self, 720, 576, 54, 51, 100, 1, 1,
				       self->renderContext->frs_sec, 
				       self->renderContext->frs_sec_base, 
				       0.1f, 0.9f,
				       0.1f, 0.9f );
		self->renderContext->mode = R_OSA + R_SHADOW + R_FIELDS;
		self->renderContext->imtype = R_TARGA;
		BLI_init_rctf( &self->renderContext->safety, 0.1f, 0.9f, 0.1f,
			       0.9f );
	} else if( type == B_PR_PRV ) {
		M_Render_DoSizePreset( self, 640, 512, 1, 1, 50, 1, 1,
				       self->renderContext->frs_sec, 
				       self->renderContext->frs_sec_base, 
				       0.1f, 0.9f,
				       0.1f, 0.9f );
		self->renderContext->mode &= ~R_PANORAMA;
		BLI_init_rctf( &self->renderContext->safety, 0.1f, 0.9f, 0.1f,
			       0.9f );
	} else if( type == B_PR_PC ) {
		M_Render_DoSizePreset( self, 640, 480, 100, 100, 100, 1, 1,
				       self->renderContext->frs_sec, 
				       self->renderContext->frs_sec_base, 
				       0.0f, 1.0f,
				       0.0f, 1.0f );
		self->renderContext->mode &= ~R_PANORAMA;
		BLI_init_rctf( &self->renderContext->safety, 0.0f, 1.0f, 0.0f,
			       1.0f );
	} else if( type == B_PR_PAL169 ) {
		M_Render_DoSizePreset( self, 720, 576, 64, 45, 100, 1, 1,
				       25, 1.0f, 0.1f, 0.9f, 0.1f, 0.9f );
		self->renderContext->mode &= ~R_PANORAMA;
		BLI_init_rctf( &self->renderContext->safety, 0.1f, 0.9f, 0.1f,
			       0.9f );
	} else if( type == B_PR_PANO ) {
		M_Render_DoSizePreset( self, 36, 176, 115, 100, 100, 16, 1,
				       self->renderContext->frs_sec, 
				       self->renderContext->frs_sec_base, 
				       0.1f, 0.9f,
				       0.1f, 0.9f );
		self->renderContext->mode |= R_PANORAMA;
		BLI_init_rctf( &self->renderContext->safety, 0.1f, 0.9f, 0.1f,
			       0.9f );
	} else if( type == B_PR_FULL ) {
		M_Render_DoSizePreset( self, 1280, 1024, 1, 1, 100, 1, 1,
				       self->renderContext->frs_sec, 
				       self->renderContext->frs_sec_base, 
				       0.1f, 0.9f,
				       0.1f, 0.9f );
		self->renderContext->mode &= ~R_PANORAMA;
		BLI_init_rctf( &self->renderContext->safety, 0.1f, 0.9f, 0.1f,
			       0.9f );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"unknown constant - see modules dict for help" ) );

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	Py_RETURN_NONE;
}

/*

PyObject *RenderData_SetYafrayGIQuality( BPy_RenderData * self,
					 PyObject * args )
{
	int type;

	if( !PyArg_ParseTuple( args, "i", &type ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected constant" ) );

	if( type == PY_NONE || type == PY_LOW ||
	    type == PY_MEDIUM || type == PY_HIGH ||
	    type == PY_HIGHER || type == PY_BEST ) {
		self->renderContext->GIquality = (short)type;
	} else
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"unknown constant - see modules dict for help" ) );

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	Py_RETURN_NONE;
}

PyObject *RenderData_SetYafrayGIMethod( BPy_RenderData * self,
					PyObject * args )
{
	int type;

	if( !PyArg_ParseTuple( args, "i", &type ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected constant" ) );

	if( type == PY_NONE || type == PY_SKYDOME || type == PY_FULL ) {
		self->renderContext->GImethod = (short)type;
	} else
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"unknown constant - see modules dict for help" ) );

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	Py_RETURN_NONE;
}
*/

/* (die) beg */

/* YafRay - Yafray GI Method */

static int RenderData_setYafrayGIQuality( BPy_RenderData * self, PyObject * value )
{
	long type;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
									"expected constant" );

	type = PyInt_AsLong( value );

	if( type == PY_NONE   || type == PY_LOW  ||
	    type == PY_MEDIUM || type == PY_HIGH ||
	    type == PY_HIGHER || type == PY_BEST ||
	    type == PY_USEAOSETTINGS
		) {
		self->renderContext->GIquality = (short)type;
	} else {
		return EXPP_ReturnIntError( PyExc_TypeError,
									"expected constant NONE, LOW, MEDIUM, HIGHER or BEST" );
	}

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	return 0;
}

static PyObject *RenderData_getYafrayGIQuality( BPy_RenderData * self )
{
	return PyInt_FromLong( (long) self->renderContext->GIquality );
}

static PyObject *RenderData_SetYafrayGIQuality( BPy_RenderData * self,
												PyObject * args )
{
	return EXPP_setterWrapper( (void*) self, args,
							   (setter) RenderData_setYafrayGIQuality );
}

static PyObject *RenderData_GetYafrayGIQuality( BPy_RenderData * self )
{
	return RenderData_getYafrayGIQuality(self);
}

/* YafRay - Yafray GI Method */

static int RenderData_setYafrayGIMethod( BPy_RenderData * self, PyObject * value )
{
	int type;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected constant NONE, SKYDOME or FULL" );

	type = PyInt_AsLong( value );
	if( type == PY_NONE || type == PY_SKYDOME || type == PY_FULL ) {
		self->renderContext->GImethod = (short)type;
	} else {
		return EXPP_ReturnIntError( PyExc_TypeError,
									"expected constant NONE, SKYDOME or FULL" );
	}

	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	return 0;
}

static PyObject *RenderData_getYafrayGIMethod( BPy_RenderData * self )
{
	return PyInt_FromLong( (long)self->renderContext->GImethod );
}

static PyObject *RenderData_GetYafrayGIMethod( BPy_RenderData * self )
{
	return RenderData_getYafrayGIMethod(self);
}

static PyObject *RenderData_SetYafrayGIMethod( BPy_RenderData * self,
											   PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)RenderData_setYafrayGIMethod );
}


/* YafRay - Export to XML */

static int RenderData_setYafrayExportToXML( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setBooleanShort( self, value, &self->renderContext->YFexportxml );
}

static PyObject *RenderData_getYafrayExportToXML( BPy_RenderData * self )
{
	return M_Render_getBooleanShort( self, self->renderContext->YFexportxml );
}

/** Auto AA */

static int RenderData_setYafrayAutoAntiAliasing( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setBooleanShort( self, value, &self->renderContext->YF_AA );
}

static PyObject *RenderData_getYafrayAutoAntiAliasing( BPy_RenderData * self )
{
	return M_Render_getBooleanShort( self, self->renderContext->YF_AA );
}

/** Clamp RGB */

static int RenderData_setYafrayClampRGB( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setBooleanShort( self, value, &self->renderContext->YF_clamprgb );
}

static PyObject *RenderData_getYafrayClampRGB( BPy_RenderData * self )
{
	return M_Render_getBooleanShort( self, self->renderContext->YF_clamprgb );
}

/** YafRay - Anti-Aliasing Passes */

static int RenderData_setYafrayAntiAliasingPasses( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setInt( self, value, &self->renderContext->YF_AApasses, 0, 64 );
}

static PyObject *RenderData_getYafrayAntiAliasingPasses( BPy_RenderData * self )
{
	return M_Render_getInt( self, self->renderContext->YF_AApasses );
}

/** YafRay - Anti-Aliasing Samples */

static int RenderData_setYafrayAntiAliasingSamples( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setInt( self, value, &self->renderContext->YF_AAsamples, 0, 2048 );
}

static PyObject *RenderData_getYafrayAntiAliasingSamples( BPy_RenderData * self )
{
	return M_Render_getInt( self, self->renderContext->YF_AAsamples );
}

/* YafRay - Anti-Aliasing Pixel Filter Size */

static int RenderData_setYafrayAntiAliasingPixelSize( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setFloat( self, value, &self->renderContext->YF_AApixelsize, 1.0, 2.0 );
}

static PyObject *RenderData_getYafrayAntiAliasingPixelSize( BPy_RenderData * self )
{
	return M_Render_getFloat( self, self->renderContext->YF_AApixelsize );
}

/* YafRay - Anti-Aliasing threshold */

static int RenderData_setYafrayAntiAliasingThreshold( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setFloat( self, value, &self->renderContext->YF_AAthreshold, 0.05, 1.0 );
}

static PyObject *RenderData_getYafrayAntiAliasingThreshold( BPy_RenderData * self )
{
	return M_Render_getFloat( self, self->renderContext->YF_AAthreshold );
}

/* YafRay - Cache occlusion/irradiance samples (faster) */

static int RenderData_setYafrayGICache( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setBooleanShort( self, value, &self->renderContext->GIcache );
}

static PyObject *RenderData_getYafrayGICache( BPy_RenderData * self )
{
	return M_Render_getBooleanShort( self, self->renderContext->GIcache );
}

/* YafRay - Enable/disable bumpnormals for cache
   (faster, but no bumpmapping in total indirectly lit areas) */

static int RenderData_setYafrayGICacheBumpNormals( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setBooleanShort( self, value, &self->renderContext->YF_nobump );
}

static PyObject *RenderData_getYafrayGICacheBumpNormals( BPy_RenderData * self )
{
	return M_Render_getBooleanShort( self, self->renderContext->YF_nobump );
}

/* YafRay - Shadow quality, keep it under 0.95 :-) */

static int RenderData_setYafrayGICacheShadowQuality( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setFloat( self, value, &self->renderContext->GIshadowquality, 0.01, 1.0 );
}

static PyObject *RenderData_getYafrayGICacheShadowQuality( BPy_RenderData * self )
{
	return M_Render_getFloat( self, self->renderContext->GIshadowquality );
}

/* YafRay - Threshold to refine shadows EXPERIMENTAL. 1 = no refinement */

static int RenderData_setYafrayGICacheRefinement( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setFloat( self, value, &self->renderContext->GIrefinement, 0.001, 1.0 );
}

static PyObject *RenderData_getYafrayGICacheRefinement( BPy_RenderData * self )
{
	return M_Render_getFloat( self, self->renderContext->GIrefinement );
}

/* YafRay - Maximum number of pixels without samples, the lower the better and slower */

static int RenderData_setYafrayGICachePixelsPerSample( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setInt( self, value, &self->renderContext->GIpixelspersample, 1, 50 );
}

static PyObject *RenderData_getYafrayGICachePixelsPerSample( BPy_RenderData * self )
{
	return M_Render_getInt( self, self->renderContext->GIpixelspersample );
}

/** YafRay - Enable/disable use of global photons to help in GI */

static int RenderData_setYafrayGIPhotons( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setBooleanShort( self, value, &self->renderContext->GIphotons );
}

static PyObject *RenderData_getYafrayGIPhotons( BPy_RenderData * self )
{
	return M_Render_getBooleanShort( self, self->renderContext->GIphotons );
}

/** YafRay - If true the photonmap is shown directly in the render for tuning */

static int RenderData_setYafrayGITunePhotons( BPy_RenderData * self, PyObject * value )
{
	return M_Render_setBooleanShort( self, value, &self->renderContext->GIdirect );
}

static PyObject *RenderData_getYafrayGITunePhotons( BPy_RenderData * self )
{
	return M_Render_getBooleanShort( self, self->renderContext->GIdirect );
}

/* (die) end */

PyObject *RenderData_YafrayGIPower( BPy_RenderData * self, PyObject * args )
{
	if( self->renderContext->GImethod > 0 ) {
		return M_Render_GetSetAttributeFloat( args,
						      &self->renderContext->
						      GIpower, 0.01f,
						      100.00f );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'SKYDOME' or 'FULL'" ) );
}

PyObject *RenderData_YafrayGIIndirPower( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeFloat( args,
						  &self->renderContext->
						  GIindirpower, 0.01f,
						  100.00f );
}

PyObject *RenderData_YafrayGIDepth( BPy_RenderData * self, PyObject * args )
{
	if( self->renderContext->GImethod == 2 ) {
		return M_Render_GetSetAttributeInt( args,
						    &self->renderContext->
						    GIdepth, 1, 100 );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'" ) );
}

PyObject *RenderData_YafrayGICDepth( BPy_RenderData * self, PyObject * args )
{
	if( self->renderContext->GImethod == 2 ) {
		return M_Render_GetSetAttributeInt( args,
						    &self->renderContext->
						    GIcausdepth, 1, 100 );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'" ) );
}

PyObject *RenderData_EnableYafrayGICache( BPy_RenderData * self,
					  PyObject * args )
{
	if( self->renderContext->GImethod == 2 ) {
		return M_Render_BitToggleShort( args, 1,
						&self->renderContext->
						GIcache );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'" ) );
}

PyObject *RenderData_EnableYafrayGIPhotons( BPy_RenderData * self,
					    PyObject * args )
{
	if( self->renderContext->GImethod == 2 ) {
		return M_Render_BitToggleShort( args, 1,
						&self->renderContext->
						GIphotons );;
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'" ) );
}

PyObject *RenderData_YafrayGIPhotonCount( BPy_RenderData * self,
					  PyObject * args )
{
	if( self->renderContext->GImethod == 2
	    && self->renderContext->GIphotons == 1 ) {
		return M_Render_GetSetAttributeInt( args,
						    &self->renderContext->
						    GIphotoncount, 100000,
						    10000000 );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled" ) );
}

PyObject *RenderData_YafrayGIPhotonRadius( BPy_RenderData * self,
					   PyObject * args )
{
	if( self->renderContext->GImethod == 2
	    && self->renderContext->GIphotons == 1 ) {
		return M_Render_GetSetAttributeFloat( args,
						      &self->renderContext->
						      GIphotonradius, 0.00001f,
						      100.0f );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled" ) );
}

PyObject *RenderData_YafrayGIPhotonMixCount( BPy_RenderData * self,
					     PyObject * args )
{
	if( self->renderContext->GImethod == 2
	    && self->renderContext->GIphotons == 1 ) {
		return M_Render_GetSetAttributeInt( args,
						    &self->renderContext->
						    GImixphotons, 100, 1000 );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled" ) );
}

PyObject *RenderData_EnableYafrayGITunePhotons( BPy_RenderData * self,
						PyObject * args )
{
	if( self->renderContext->GImethod == 2
	    && self->renderContext->GIphotons == 1 ) {
		return M_Render_BitToggleShort( args, 1,
						&self->renderContext->
						GIdirect );;
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled" ) );
}

PyObject *RenderData_YafrayGIShadowQuality( BPy_RenderData * self,
					    PyObject * args )
{
	if( self->renderContext->GImethod == 2
	    && self->renderContext->GIcache == 1 ) {
		return M_Render_GetSetAttributeFloat( args,
						      &self->renderContext->
						      GIshadowquality, 0.01f,
						      1.0f );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled" ) );
}

PyObject *RenderData_YafrayGIPixelsPerSample( BPy_RenderData * self,
					      PyObject * args )
{
	if( self->renderContext->GImethod == 2
	    && self->renderContext->GIcache == 1 ) {
		return M_Render_GetSetAttributeInt( args,
						    &self->renderContext->
						    GIpixelspersample, 1, 50 );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled" ) );
}

PyObject *RenderData_YafrayGIRefinement( BPy_RenderData * self,
					 PyObject * args )
{
	if( self->renderContext->GImethod == 2
	    && self->renderContext->GIcache == 1 ) {
		return M_Render_GetSetAttributeFloat( args,
						      &self->renderContext->
						      GIrefinement, 0.001f,
						      1.0f );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled" ) );
}

PyObject *RenderData_YafrayRayBias( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeFloat( args,
					      &self->renderContext->YF_raybias,
					      0.0f, 10.0f );
}

PyObject *RenderData_YafrayRayDepth( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeInt( args,
					    &self->renderContext->YF_raydepth,
					    1, 80 );
}

PyObject *RenderData_YafrayGamma( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeFloat( args,
					      &self->renderContext->YF_gamma,
					      0.001f, 5.0f );
}

PyObject *RenderData_YafrayExposure( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeFloat( args,
					      &self->renderContext->
					      YF_exposure, 0.0f, 10.0f );
}

PyObject *RenderData_EnableGameFrameStretch( BPy_RenderData * self )
{
	self->scene->framing.type = SCE_GAMEFRAMING_SCALE;
	Py_RETURN_NONE;
}

PyObject *RenderData_EnableGameFrameExpose( BPy_RenderData * self )
{
	self->scene->framing.type = SCE_GAMEFRAMING_EXTEND;
	Py_RETURN_NONE;
}

PyObject *RenderData_EnableGameFrameBars( BPy_RenderData * self )
{
	self->scene->framing.type = SCE_GAMEFRAMING_BARS;
	Py_RETURN_NONE;
}

PyObject *RenderData_SetGameFrameColor( BPy_RenderData * self,
					PyObject * args )
{
	float red = 0.0f;
	float green = 0.0f;
	float blue = 0.0f;

	if( !PyArg_ParseTuple( args, "fff", &red, &green, &blue ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected three floats" ) );

	if( red < 0 || red > 1 )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (red)" ) );
	if( green < 0 || green > 1 )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (green)" ) );
	if( blue < 0 || blue > 1 )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (blue)" ) );

	self->scene->framing.col[0] = red;
	self->scene->framing.col[1] = green;
	self->scene->framing.col[2] = blue;

	Py_RETURN_NONE;
}

PyObject *RenderData_GetGameFrameColor( BPy_RenderData * self )
{
	char rgb[24];

	sprintf( rgb, "[%.3f,%.3f,%.3f]", self->scene->framing.col[0],
		 self->scene->framing.col[1], self->scene->framing.col[2] );
	return PyString_FromString( rgb );
}

#ifdef __sgi
PyObject *RenderData_SGIMaxsize( BPy_RenderData * self, PyObject * args )
{
	return M_Render_GetSetAttributeShort( args,
					      &self->renderContext->maximsize,
					      0, 500 );
}

PyObject *RenderData_EnableSGICosmo( BPy_RenderData *self, PyObject *args )
{
	return M_Render_BitToggleInt( args, R_COSMO,
				      &self->renderContext->mode );
}
#else
PyObject *RenderData_SGIMaxsize( void )
{
	return EXPP_ReturnPyObjError( PyExc_StandardError,
			"SGI is not defined on this machine" );
}

PyObject *RenderData_EnableSGICosmo( void )
{
	return EXPP_ReturnPyObjError( PyExc_StandardError,
			"SGI is not defined on this machine" );
}
#endif

PyObject *RenderData_OldMapValue( BPy_RenderData * self, PyObject * args )
{
	PyObject *tmp = M_Render_GetSetAttributeInt(args,
		&self->renderContext->framapto, 1, 900);
	self->renderContext->framelen =
		(float)self->renderContext->framapto / self->renderContext->images;
	return tmp;
}

PyObject *RenderData_NewMapValue( BPy_RenderData * self, PyObject * args )
{
	PyObject *tmp = M_Render_GetSetAttributeInt(args,
			&self->renderContext->images, 1, 900);
	self->renderContext->framelen =
		(float)self->renderContext->framapto / self->renderContext->images;
	return tmp;
}

static PyObject *RenderData_getTimeCode( BPy_RenderData * self) {
    char tc[12];
    int h, m, s, cfa;
    double fps;
    
    fps = (double) self->renderContext->frs_sec / 
	    self->renderContext->frs_sec_base;
    cfa = self->renderContext->cfra-1;
	s = cfa / fps;
	m = s / 60;
	h = m / 60;
    if( h > 99 )
        return PyString_FromString("Time Greater than 99 Hours!");	

	sprintf( tc, "%02d:%02d:%02d:%02d", h%60, m%60, s%60, 
		 (int) (cfa - ((int) (cfa / fps) * fps)));
	return PyString_FromString(tc);
}            


/***************************************************************************/
/* Render layer functions                                                  */
/***************************************************************************/
PyObject *RenderData_getRenderLayers(BPy_RenderData * self)
{
	PyObject *list, *layer;
	SceneRenderLayer *srl;
	
	list = PyList_New(0);
	
	for(srl= self->renderContext->layers.first; srl; srl= srl->next) {	
		layer = RenderLayer_CreatePyObject( self->scene, srl );
		PyList_Append(list, layer);
		Py_DECREF(layer);
	}
	return list;
}

PyObject *RenderData_removeRenderLayer(BPy_RenderData * self, BPy_RenderLayer *value)
{
	int index;
	if (!BPy_RenderLayer_Check(value))
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"can only remove a render layer" );
	
	index = BLI_findindex(&self->renderContext->layers, value->renderLayer);
	
	if (index == -1)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
			"render layer is not in this scene" );
	
	if (BLI_countlist(&self->renderContext->layers)<=1)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"cannot remove the last render layer" );
	
	BLI_remlink(&self->scene->r.layers, value->renderLayer);
	MEM_freeN(value->renderLayer);
	self->scene->r.actlay= 0;
	
	if(self->scene->nodetree) {
		bNode *node;
		for(node= self->scene->nodetree->nodes.first; node; node= node->next) {
			if(node->type==CMP_NODE_R_LAYERS && node->id==NULL) {
				if(node->custom1==index)
					node->custom1= 0;
				else if(node->custom1 > index)
					node->custom1--;
			}
		}
	}
	
	value->renderLayer = NULL;
	Py_RETURN_NONE;
}

PyObject *RenderData_addRenderLayer(BPy_RenderData * self ) {
	scene_add_render_layer(self->scene);
	return RenderLayer_CreatePyObject( self->scene, self->renderContext->layers.last );
	
}

/***************************************************************************/
/* generic handlers for getting/setting attributes                         */
/***************************************************************************/

/*
 * get floating point attributes
 */

static PyObject *RenderData_getFloatAttr( BPy_RenderData *self, void *type )
{
	float param;

	switch( (int)type ) {
	case EXPP_RENDER_ATTR_GAUSSFILTER:
		param = self->renderContext->gauss;
		break;
	case EXPP_RENDER_ATTR_MBLURFACTOR:
		param = self->renderContext->blurfac;
		break;
	case EXPP_RENDER_ATTR_FPS_BASE:
		param = self->renderContext->frs_sec_base;
		break;
	case EXPP_RENDER_ATTR_BAKEDIST:
		param = self->renderContext->bake_maxdist;
		break;
	case EXPP_RENDER_ATTR_BAKEBIAS:
		param = self->renderContext->bake_biasdist;
		break;
	case EXPP_RENDER_ATTR_ASPECTX:
		param = (long)self->renderContext->xasp;
		break;
	case EXPP_RENDER_ATTR_ASPECTY:
		param = (long)self->renderContext->yasp;
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"undefined type constant in RenderData_getFloatAttr" );
	}
	return PyFloat_FromDouble( param );
}

/*
 * set floating point attributes which require clamping
 */

static int RenderData_setFloatAttrClamp( BPy_RenderData *self, PyObject *value,
		void *type )
{
	float *param;
	float min, max;

	switch( (int)type ) {
	case EXPP_RENDER_ATTR_GAUSSFILTER:
		min = 0.5f;
		max = 1.5f;
		param = &self->renderContext->gauss;
		break;
	case EXPP_RENDER_ATTR_MBLURFACTOR:
	    min = 0.01f;
		max = 5.0f;
		param = &self->renderContext->blurfac;
		break;
	case EXPP_RENDER_ATTR_FPS_BASE:
		min = 1.0f;
		max = 120.0f;
		param = &self->renderContext->frs_sec_base;
		break;
	case EXPP_RENDER_ATTR_BAKEDIST:
		min = 0.0f;
		max = 1000.0f;
		param = &self->renderContext->bake_maxdist;
		break;
	case EXPP_RENDER_ATTR_BAKEBIAS:
		min = 0.0f;
		max = 1000.0f;
		param = &self->renderContext->bake_biasdist;
		break;
	case EXPP_RENDER_ATTR_ASPECTX:
		min = 1.0f;
		max = 200.0f;
		param = &self->renderContext->xasp;
		break;
	case EXPP_RENDER_ATTR_ASPECTY:
		min = 1.0f;
		max = 200.0f;
		param = &self->renderContext->yasp;
		break;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type constant in RenderData_setFloatAttrClamp" );
	}
	return EXPP_setFloatClamped( value, param, min, max );
}

/*
 * get integer attributes
 */

static PyObject *RenderData_getIValueAttr( BPy_RenderData *self, void *type )
{
	long param;

	switch( (int)type ) {
	case EXPP_RENDER_ATTR_XPARTS:
		param = (long)self->renderContext->xparts;
		break;
	case EXPP_RENDER_ATTR_YPARTS:
		param = (long)self->renderContext->yparts;
		break;
	case EXPP_RENDER_ATTR_CFRAME:
		param = (long)self->renderContext->cfra;
		break;
	case EXPP_RENDER_ATTR_EFRAME:
		param = (long)self->renderContext->efra;
		break;
	case EXPP_RENDER_ATTR_SFRAME:
		param = (long)self->renderContext->sfra;
		break;
	case EXPP_RENDER_ATTR_FPS:
		param = self->renderContext->frs_sec;
		break;
	case EXPP_RENDER_ATTR_SIZEX:
		param = self->renderContext->xsch;
		break;
	case EXPP_RENDER_ATTR_SIZEY:
		param = self->renderContext->ysch;
		break;
	case EXPP_RENDER_ATTR_BAKEMARGIN:
		param = self->renderContext->bake_filter;
		break;
	case EXPP_RENDER_ATTR_BAKEMODE:
		param = self->renderContext->bake_mode;
		break;
	case EXPP_RENDER_ATTR_BAKENORMALSPACE:
		param = self->renderContext->bake_normal_space;
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"undefined type constant in RenderData_setIValueAttrClamp" );
	}
	return PyInt_FromLong( param );
}

/*
 * set integer attributes which require clamping
 */

static int RenderData_setIValueAttrClamp( BPy_RenderData *self, PyObject *value,
		void *type )
{
	void *param;
	int min, max, size;

	switch( (int)type ) {
	case EXPP_RENDER_ATTR_XPARTS:
		min = 1;
		max = 512;
		size = 'h';
		param = &self->renderContext->xparts;
		break;
	case EXPP_RENDER_ATTR_YPARTS:
		min = 1;
		max = 64;
		size = 'h';
		param = &self->renderContext->yparts;
		break;
	case EXPP_RENDER_ATTR_CFRAME:
		min = 1;
		max = MAXFRAME;
	   	size = 'i';
		param = &self->renderContext->cfra;
		break;
	case EXPP_RENDER_ATTR_EFRAME:
		min = 1;
		max = MAXFRAME;
		size = 'i';
		param = &self->renderContext->efra;
		break;
	case EXPP_RENDER_ATTR_SFRAME:
		min = 1;
	    max = MAXFRAME;
		size = 'i';
		param = &self->renderContext->sfra;
		break;
	case EXPP_RENDER_ATTR_FPS:
		min = 1;
		max = 120;
		size = 'h';
		param = &self->renderContext->frs_sec;
		break;
	case EXPP_RENDER_ATTR_SIZEX:
		min = 4;
		max = 10000;
		size = 'h';
		param = &self->renderContext->xsch;
		break;
	case EXPP_RENDER_ATTR_SIZEY:
		min = 4;
		max = 10000;
		size = 'h';
		param = &self->renderContext->ysch;
		break;
	case EXPP_RENDER_ATTR_BAKEMARGIN:
		min = 0;
		max = 32;
		size = 'h';
		param = &self->renderContext->bake_filter;
		break;
	case EXPP_RENDER_ATTR_BAKEMODE:
		min = RE_BAKE_LIGHT;
		max = RE_BAKE_SHADOW;
		size = 'h';
		param = &self->renderContext->bake_mode;
		break;
	case EXPP_RENDER_ATTR_BAKENORMALSPACE:
		min = R_BAKE_SPACE_CAMERA;
		max = R_BAKE_SPACE_TANGENT;
		size = 'h';
		param = &self->renderContext->bake_normal_space;
		break;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type constant in RenderData_setIValueAttrClamp" );
	}
	return EXPP_setIValueClamped( value, param, min, max, size );
}

/***************************************************************************/
/* handlers for other getting/setting attributes                           */
/***************************************************************************/

static PyObject *RenderData_getSubImTypeBits( BPy_RenderData *self, void* type )
{
	return EXPP_getBitfield( &self->renderContext->subimtype, (int)type, 'h' );
}

static int RenderData_setSubImTypeBits( BPy_RenderData* self, PyObject *value,
		void* type )
{
	return EXPP_setBitfield( value, &self->renderContext->subimtype,
			(int)type, 'h' );
}

static PyObject *RenderData_getModeBit( BPy_RenderData *self, void* type )
{
	return EXPP_getBitfield( &self->renderContext->mode,
			(int)type, 'i' );
}

static int RenderData_setModeBit( BPy_RenderData* self, PyObject *value,
		void* type )
{
	return EXPP_setBitfield( value, &self->renderContext->mode,
			(int)type, 'i' );
}

#define MODE_MASK ( R_OSA | R_SHADOW | R_GAMMA | R_ENVMAP | R_EDGE | \
	R_FIELDS | R_FIELDSTILL | R_RADIO | R_BORDER | R_PANORAMA | R_CROP | \
	R_ODDFIELD | R_MBLUR | R_RAYTRACE | R_FIXED_THREADS )

static PyObject *RenderData_getMode( BPy_RenderData *self )
{
	return PyInt_FromLong( (long)(self->renderContext->mode & MODE_MASK) );
}

static int RenderData_setMode( BPy_RenderData* self, PyObject *arg )
{
	int value;

	if( !PyInt_Check( arg ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	value = PyInt_AsLong( arg );
	if( value & ~MODE_MASK )
		return EXPP_ReturnIntError( PyExc_ValueError, 
				"unexpected bits set in argument" );

	self->renderContext->mode = (short)value;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	return 0;
}

static PyObject *RenderData_getSceModeBits( BPy_RenderData *self, void* type )
{
	return EXPP_getBitfield( &self->renderContext->scemode, (int)type, 'i' );
}

static int RenderData_setSceModeBits( BPy_RenderData* self, PyObject *value,
		void* type )
{
	return EXPP_setBitfield( value, &self->renderContext->scemode,
			(int)type, 'i' );
}

static PyObject *RenderData_getSceMode( BPy_RenderData *self )
{
	return PyInt_FromLong ( (long)self->renderContext->scemode );
}

static int RenderData_setSceMode( BPy_RenderData* self, PyObject *arg )
{
	int value;

	if( !PyInt_Check( arg ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	value = PyInt_AsLong( arg );
	if( value & ~( R_EXTENSION | R_DOSEQ ) )
		return EXPP_ReturnIntError( PyExc_ValueError, 
				"unexpected bits set in argument" );

	self->renderContext->scemode = (int)value;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	return 0;
}
 
static PyObject *RenderData_getFramingType( BPy_RenderData *self )
{
	return PyInt_FromLong( (long)self->scene->framing.type );
}

static int RenderData_setFramingType( BPy_RenderData *self, PyObject *value )
{
	return EXPP_setIValueRange( value, &self->scene->framing.type,
			SCE_GAMEFRAMING_BARS, SCE_GAMEFRAMING_SCALE, 'b' );
}

static PyObject *RenderData_getEdgeColor( BPy_RenderData * self )
{
	return Py_BuildValue( "[fff]", self->renderContext->edgeR,
			self->renderContext->edgeG, self->renderContext->edgeB );
}

static int RenderData_setEdgeColor( BPy_RenderData * self, PyObject * args )
{
	float red, green, blue;

	/* if we get a list, convert to a tuple; otherwise hope for the best */
	if( PyList_Check( args ) )
		args = PySequence_Tuple( args );
	else
		Py_INCREF( args );

	if( !PyArg_ParseTuple( args, "fff", &red, &green, &blue ) ) {
		Py_DECREF( args );
		return EXPP_ReturnIntError( PyExc_TypeError, "expected three floats" );
	}
	Py_DECREF( args );

	self->renderContext->edgeR = EXPP_ClampFloat( red, 0.0, 1.0 );
	self->renderContext->edgeG = EXPP_ClampFloat( green, 0.0, 1.0 );
	self->renderContext->edgeB = EXPP_ClampFloat( blue, 0.0, 1.0 );
	return 0;
}

static PyObject *RenderData_getOSALevel( BPy_RenderData * self )
{
	return PyInt_FromLong( (long)self->renderContext->osa );
}

static PyObject *RenderData_getRenderer( BPy_RenderData * self )
{
	return PyInt_FromLong( (long)self->renderContext->renderer ); 
}

static PyObject *RenderData_getImageType( BPy_RenderData * self )
{
	return PyInt_FromLong( (long) self->renderContext->imtype );
}

static int RenderData_setGameFrameColor( BPy_RenderData * self,
		PyObject * args )
{
	float red, green, blue;

	/* if we get a list, convert to a tuple; otherwise hope for the best */
	if( PyList_Check( args ) )
		args = PySequence_Tuple( args );
	else
		Py_INCREF( args );

	if( !PyArg_ParseTuple( args, "fff", &red, &green, &blue ) ) {
		Py_DECREF( args );
		return EXPP_ReturnIntError( PyExc_TypeError, "expected three floats" );
	}
	Py_DECREF( args );

	self->scene->framing.col[0] = EXPP_ClampFloat( red, 0.0, 1.0 );
	self->scene->framing.col[1] = EXPP_ClampFloat( green, 0.0, 1.0 );
	self->scene->framing.col[2] = EXPP_ClampFloat( blue, 0.0, 1.0 );
	return 0;
}

static PyObject *RenderData_getGameFrameColor( BPy_RenderData * self )
{
	return Py_BuildValue( "[fff]", self->scene->framing.col[0],
		 self->scene->framing.col[1], self->scene->framing.col[2] );
}

static PyObject *RenderData_getBackbuf( BPy_RenderData * self )
{
	return EXPP_getBitfield( &self->renderContext->bufflag,
			R_BACKBUF, 'h' );
}

static int RenderData_setBackbuf( BPy_RenderData* self, PyObject *value )
{
	return EXPP_setBitfield( value, &self->renderContext->bufflag,
			R_BACKBUF, 'h' );
}

static int RenderData_setImagePlanes( BPy_RenderData *self, PyObject *value )
{
	int depth;
	char *errstr = "expected int argument of 8, 24, or 32";

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );

	depth = PyInt_AsLong( value );
	if( depth != 8 && depth != 24 && depth != 32 )
		return EXPP_ReturnIntError( PyExc_ValueError, errstr );

	self->renderContext->planes = (short)depth;

	return 0;
}

static PyObject *RenderData_getImagePlanes( BPy_RenderData * self )
{
	return PyInt_FromLong( (long) self->renderContext->planes );
}

static int RenderData_setAlphaMode( BPy_RenderData *self, PyObject *value )
{
	return EXPP_setIValueRange( value, &self->renderContext->alphamode,
			R_ADDSKY, R_ALPHAKEY, 'h' );
}

static PyObject *RenderData_getAlphaMode( BPy_RenderData * self )
{
	return PyInt_FromLong( (long) self->renderContext->alphamode );
}

static PyObject *RenderData_getDisplayMode( void )
{
	return PyInt_FromLong( (long) G.displaymode );
}

static int RenderData_setDisplayMode( BPy_RenderData *self,
		PyObject *value )
{
	return EXPP_setIValueRange( value, &G.displaymode,
			R_DISPLAYIMAGE, R_DISPLAYSCREEN, 'h' );
}

static PyObject *RenderData_getRenderPath( BPy_RenderData * self )
{
	return PyString_FromString( self->renderContext->pic );
}

static int RenderData_setRenderPath( BPy_RenderData * self, PyObject * value )
{
	char *name;

	name = PyString_AsString( value );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected a string" );

	if( strlen( name ) >= sizeof(self->renderContext->pic) )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"render path is too long" );

	strcpy( self->renderContext->pic, name );
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	return 0;
}

static PyObject *RenderData_getFrameFilename( BPy_RenderData * self, PyObject *args )
{
	char name[FILE_MAX];
	int frame = self->renderContext->cfra;
	
	if( !PyArg_ParseTuple( args, "|i", &( frame ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
										"expected int argument or nothing" ) );
	
	BKE_makepicstring(name, self->renderContext->pic, frame, self->renderContext->imtype);
	return PyString_FromString( name );
}

PyObject *RenderData_getBackbufPath( BPy_RenderData * self )
{
	return PyString_FromString( self->renderContext->backbuf );
}

static int RenderData_setBackbufPath( BPy_RenderData *self, PyObject *value )
{
	char *name;
	Image *ima;

	name = PyString_AsString( value );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected a string" );

	if( strlen( name ) >= sizeof(self->renderContext->backbuf) )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"backbuf path is too long" );

	strcpy( self->renderContext->backbuf, name );
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	ima = BKE_add_image_file( name );
	if( ima )
		BKE_image_signal( ima, NULL, IMA_SIGNAL_RELOAD );

	return 0;
}

PyObject *RenderData_getRenderWinSize( BPy_RenderData * self )
{
	return PyInt_FromLong( (long) self->renderContext->size );
}

static int RenderData_setRenderWinSize( BPy_RenderData *self, PyObject *value )
{
	int size;
	char *errstr = "expected int argument of 25, 50, 75, or 100";

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );

	size = PyInt_AsLong( value );
	if( size != 25 && size != 50 && size != 75 && size != 100 )
		return EXPP_ReturnIntError( PyExc_ValueError, errstr );

	self->renderContext->size = (short)size;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );

	return 0;
}

static PyObject *RenderData_getMapOld( BPy_RenderData *self )
{
	return PyInt_FromLong( (long)self->renderContext->framapto );
}

static int RenderData_setMapOld( BPy_RenderData *self, PyObject *value )
{
	int result = EXPP_setIValueClamped( value, &self->renderContext->framapto,
			1, 900, 'h' );
	self->renderContext->framelen =
		(float)self->renderContext->framapto / self->renderContext->images;
	return result;
}

static PyObject *RenderData_getMapNew( BPy_RenderData *self )
{
	return PyInt_FromLong( (long)self->renderContext->images );
}

static int RenderData_setMapNew( BPy_RenderData *self, PyObject *value )
{
	int result = EXPP_setIValueClamped( value, &self->renderContext->images,
			1, 900, 'h' );
	self->renderContext->framelen =
		(float)self->renderContext->framapto / self->renderContext->images;
	return result;
}

static PyObject *RenderData_getSet( BPy_RenderData *self )
{
	if( self->scene->set )
		return Scene_CreatePyObject( self->scene->set );
	Py_RETURN_NONE;
}

static int RenderData_setSet( BPy_RenderData *self, PyObject *value )
{
	BPy_Scene *sc;

	/* if "None", delete the link to the scene */
	if( value == Py_None ) {
		self->scene->set = NULL;
		return 0;
	}

	/* be sure argument is a Scene */
	if( !BPy_Scene_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected Scene as argument" );

	/* check for attempt to link to ourselves */
	sc = (BPy_Scene *)value;
	if( self->scene == sc->scene )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"cannot link a scene to itself" );

	/*
	 * Accept the set link, then check for a circular link.  If circular link
	 * exists, scene_check_setscene() sets self->scene->set to NULL.
	 */

	self->scene->set = sc->scene;
	if( !scene_check_setscene( self->scene ) )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"linking scene would create a cycle" );

	return 0;
}


PyObject *RenderData_getThreads( BPy_RenderData * self )
{
	return PyInt_FromLong( (long) self->renderContext->threads );
}

static int RenderData_setThreads( BPy_RenderData *self, PyObject *value )
{
	int threads;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "Error, threads must be an int" );

	threads = PyInt_AsLong( value );
	if (threads<1) threads = 1;
	else if (threads>8) threads = 8;
	
	self->renderContext->threads = (short)threads;
	EXPP_allqueue( REDRAWBUTSSCENE, 0 );
	return 0;
}

PyObject *RenderData_getActiveLayer( BPy_RenderData * self )
{
	return PyInt_FromLong( (long) self->renderContext->actlay );
}

static int RenderData_setActiveLayer( BPy_RenderData *self, PyObject *value )
{
	int layer;
	short nr;
    SceneRenderLayer *srl;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "active layer must be an int" );

	layer = PyInt_AsLong( value );
    for(nr=0, srl= self->renderContext->layers.first; srl; srl= srl->next, nr++) {
	}
	if(layer >= nr)
		return EXPP_ReturnIntError( PyExc_ValueError, "value larger than number of render layers" );

	self->renderContext->actlay = layer;
	EXPP_allqueue(REDRAWBUTSSCENE, 0);
	EXPP_allqueue(REDRAWNODE, 0);
	return 0;
}

static int RenderData_setBakeMode( BPy_RenderData *self, PyObject *value,
		void *type )
{
	/* use negative numbers to flip truth */
	int param = PyObject_IsTrue( value );
	
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if (param) {
		self->renderContext->bake_flag |= (int)type;
	} else {
		self->renderContext->bake_flag &= ~(int)type;
	}
	return 0;
}

static PyObject *RenderData_getBakeMode( BPy_RenderData *self, void *type )
{
	int itype = (int)type;
	/* use negative numbers to flip truth */
	if (self->renderContext->bake_flag & itype)	Py_RETURN_TRUE;
	else										Py_RETURN_FALSE;
}

/***************************************************************************/
/* BPy_RenderData attribute def                                            */
/***************************************************************************/
static PyGetSetDef BPy_RenderData_getseters[] = {
	{"oversampling",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Oversampling (anti-aliasing) enabled",
	 (void *)R_OSA},
	{"shadow",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Shadow calculation enabled",
	 (void *)R_SHADOW},
	{"gammaCorrection",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Gamma correction enabled",
	 (void *)R_GAMMA},
/* R_ORTHO	unused */
	{"environmentMap",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Environment map rendering enabled",
	 (void *)R_ENVMAP},
	{"toonShading",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Toon edge shading enabled",
	 (void *)R_EDGE},
	{"fieldRendering", 
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Field rendering enabled",
	 (void *)R_FIELDS},
	{"fieldTimeDisable",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Time difference in field calculations disabled ('X' in UI)",
	 (void *)R_FIELDSTILL},
	{"radiosityRender",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Radiosity rendering enabled",
	 (void *)R_RADIO},
	{"borderRender",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Small cut-out rendering enabled",
	 (void *)R_BORDER},
	{"panorama",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Panorama rendering enabled",
	 (void *)R_PANORAMA},
	{"crop",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Crop image during border renders",
	 (void *)R_CROP},
/* R_COSMO	unsupported */
	{"oddFieldFirst",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Odd field first rendering enabled",
	 (void *)R_ODDFIELD},
	{"motionBlur",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Motion blur enabled",
	 (void *)R_MBLUR},
	{"rayTracing",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Ray tracing enabled",
	 (void *)R_RAYTRACE},
	  
	{"touch",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Create an empry file with the frame name before rendering",
	 (void *)R_TOUCH},
	{"noOverwrite",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Skip rendering existing image files",
	 (void *)R_NO_OVERWRITE},
	{"fixedThreads",
	 (getter)RenderData_getModeBit, (setter)RenderData_setModeBit,
	 "Use the number of threads defined by the blend file",
	 (void *)R_FIXED_THREADS},
/* R_GAUSS unused */
/* R_FBUF unused */
	{"threads",
	 (getter)RenderData_getThreads, (setter)RenderData_setThreads,
	 "Number of threads used to render",
	 NULL},
/* R_SPEED unused */
	{"mode",
	 (getter)RenderData_getMode, (setter)RenderData_setMode,
	 "Mode bitfield",
	 NULL},

	/* scene modes */
	{"sequencer",
     (getter)RenderData_getSceModeBits, (setter)RenderData_setSceModeBits,
     "'Do Sequence' enabled",
     (void *)R_DOSEQ},
	{"extensions",
     (getter)RenderData_getSceModeBits, (setter)RenderData_setSceModeBits,
     "Add extensions to output (when rendering animations) enabled",
     (void *)R_EXTENSION},
	{"compositor",
     (getter)RenderData_getSceModeBits, (setter)RenderData_setSceModeBits,
     "'Do Compositor' enabled.",
     (void *)R_DOCOMP},
	{"freeImages",
     (getter)RenderData_getSceModeBits, (setter)RenderData_setSceModeBits,
     "Free texture images after render.",
     (void *)R_FREE_IMAGE},
	{"singleLayer",
     (getter)RenderData_getSceModeBits, (setter)RenderData_setSceModeBits,
     "Only render the active layer.",
     (void *)R_SINGLE_LAYER},
	{"saveBuffers",
     (getter)RenderData_getSceModeBits, (setter)RenderData_setSceModeBits,
     "Save render buffers to disk while rendering, saves memory.",
     (void *)R_EXR_TILE_FILE},
	{"compositeFree",
     (getter)RenderData_getSceModeBits, (setter)RenderData_setSceModeBits,
     "Free nodes that are not used while composite.",
     (void *)R_COMP_FREE},

	{"sceneMode",
     (getter)RenderData_getSceMode, (setter)RenderData_setSceMode,
     "Scene mode bitfield",
     NULL},
/* R_BG_RENDER unused */

	{"gameFrame",
	 (getter)RenderData_getFramingType, (setter)RenderData_setFramingType,
	 "Game framing type",
	 NULL},

	{"renderPath",
	 (getter)RenderData_getRenderPath, (setter)RenderData_setRenderPath,
	 "The path to output the rendered images to",
	 NULL},
	{"backbufPath",
	 (getter)RenderData_getBackbufPath, (setter)RenderData_setBackbufPath,
	 "Path to a background image (setting loads image)",
	 NULL},
	{"edgeColor",
	 (getter)RenderData_getEdgeColor, (setter)RenderData_setEdgeColor,
	 "RGB color triplet for edges in Toon shading",
	 NULL},
	{"OSALevel",
	 (getter)RenderData_getOSALevel, (setter)RenderData_setOSALevel,
	 "Oversampling (anti-aliasing) level",
	 NULL},
	{"renderwinSize",
	 (getter)RenderData_getRenderWinSize, (setter)RenderData_setRenderWinSize,
	 "Size of the rendering window (25, 50, 75, or 100)",
	 NULL},
	{"border",
	 (getter)RenderData_getBorder, (setter)RenderData_setBorder,
	 "The border for border rendering",
	 NULL},
	{"timeCode",
	 (getter)RenderData_getTimeCode, (setter)NULL,
	 "Get the current frame in HH:MM:SS:FF format",
	 NULL},
	{"renderer",
	 (getter)RenderData_getRenderer, (setter)RenderData_setRenderer,
	 "Rendering engine choice",
	 NULL},
	{"imageType",
	 (getter)RenderData_getImageType, (setter)RenderData_setImageType,
	 "File format for saving images",
	 NULL},
	{"gameFrameColor",
	 (getter)RenderData_getGameFrameColor,(setter)RenderData_setGameFrameColor,
	 "RGB color triplet for bars",
	 NULL},
	{"backbuf",
	 (getter)RenderData_getBackbuf, (setter)RenderData_setBackbuf,
	 "Backbuffer image enabled",
	 NULL},
	{"imagePlanes",
	 (getter)RenderData_getImagePlanes, (setter)RenderData_setImagePlanes,
	 "Image depth (8, 24, or 32 bits)",
	 NULL},
	{"alphaMode",
	 (getter)RenderData_getAlphaMode, (setter)RenderData_setAlphaMode,
	 "Setting for sky/background.",
	 NULL},
	{"displayMode",
	 (getter)RenderData_getDisplayMode, (setter)RenderData_setDisplayMode,
	 "Render output in separate window or 3D view",
	 NULL},

	{"xParts",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "Number of horizontal parts for image render",
	 (void *)EXPP_RENDER_ATTR_XPARTS},
	{"yParts",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "Number of vertical parts for image render",
	 (void *)EXPP_RENDER_ATTR_YPARTS},
	{"cFrame",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "The current frame for rendering",
	 (void *)EXPP_RENDER_ATTR_CFRAME},
	{"sFrame",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "Starting frame for rendering",
	 (void *)EXPP_RENDER_ATTR_SFRAME},
	{"eFrame",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "Ending frame for rendering",
	 (void *)EXPP_RENDER_ATTR_EFRAME},
	{"fps",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "Frames per second",
	 (void *)EXPP_RENDER_ATTR_FPS},
	{"fpsBase",
	 (getter)RenderData_getFloatAttr, (setter)RenderData_setFloatAttrClamp,
	 "Frames per second base",
	 (void *)EXPP_RENDER_ATTR_FPS_BASE},
	{"sizeX",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "Image width (in pixels)",
	 (void *)EXPP_RENDER_ATTR_SIZEX},
	{"sizeY",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "Image height (in pixels)",
	 (void *)EXPP_RENDER_ATTR_SIZEY},

	{"gaussFilter",
	 (getter)RenderData_getFloatAttr, (setter)RenderData_setFloatAttrClamp,
	 "Gauss filter size",
	 (void *)EXPP_RENDER_ATTR_GAUSSFILTER},
	{"mblurFactor",
	 (getter)RenderData_getFloatAttr, (setter)RenderData_setFloatAttrClamp,
	 "Motion blur factor",
	 (void *)EXPP_RENDER_ATTR_MBLURFACTOR},
	{"aspectX",
	 (getter)RenderData_getFloatAttr, (setter)RenderData_setFloatAttrClamp,
	 "Horizontal aspect ratio",
	 (void *)EXPP_RENDER_ATTR_ASPECTX},
	{"aspectY",
	 (getter)RenderData_getFloatAttr, (setter)RenderData_setFloatAttrClamp,
	 "Vertical aspect ratio",
	 (void *)EXPP_RENDER_ATTR_ASPECTY},
	{"mapOld",
	 (getter)RenderData_getMapOld, (setter)RenderData_setMapOld,
	 "Number of frames the Map Old will last",
	 NULL},
	{"mapNew",
	 (getter)RenderData_getMapNew, (setter)RenderData_setMapNew,
	 "New mapping value (in frames)",
	 NULL},
	{"set",
	 (getter)RenderData_getSet, (setter)RenderData_setSet,
	 "Scene link 'set' value",
	 NULL},

	/* renderlayers */
	{"activeLayer",
	 (getter)RenderData_getActiveLayer, (setter)RenderData_setActiveLayer,
	 "Active rendering layer",
	 NULL},
	{"renderLayers",
	 (getter)RenderData_getRenderLayers, (setter)NULL,
	 "Active rendering layer",
	 NULL},
  
	{"halfFloat",
     (getter)RenderData_getSubImTypeBits, (setter)RenderData_setSubImTypeBits,
     "'Half' openexr option enabled",
     (void *)R_OPENEXR_HALF},
	{"zbuf",
     (getter)RenderData_getSubImTypeBits, (setter)RenderData_setSubImTypeBits,
     "'ZBuf' openexr option enabled",
     (void *)R_OPENEXR_ZBUF},
	{"preview",
     (getter)RenderData_getSubImTypeBits, (setter)RenderData_setSubImTypeBits,
     "'preview' openexr option enabled",
     (void *)R_PREVIEW_JPG},

	{"yafrayGIMethod",
	 (getter)RenderData_getYafrayGIMethod, (setter)RenderData_setYafrayGIMethod,
	 "Global illumination method",
	 NULL},
	{"yafrayGIQuality",
	 (getter)RenderData_getYafrayGIQuality, (setter)RenderData_setYafrayGIQuality,
	 "Global Illumination quality",
	 NULL},
	{"yafrayExportToXML",
	 (getter)RenderData_getYafrayExportToXML, (setter)RenderData_setYafrayExportToXML,
	 "If true export to an xml file and call yafray instead of plugin",
	 NULL},
	{"yafrayAutoAntiAliasing",
	 (getter)RenderData_getYafrayAutoAntiAliasing, (setter)RenderData_setYafrayAutoAntiAliasing,
	 "Automatic anti-aliasing enabled/disabled",
	 NULL},
	{"yafrayClampRGB",
	 (getter)RenderData_getYafrayClampRGB, (setter)RenderData_setYafrayClampRGB,
	 "Clamp RGB enabled/disabled",
	 NULL},
	{"yafrayAntiAliasingPasses",
	 (getter)RenderData_getYafrayAntiAliasingPasses, (setter)RenderData_setYafrayAntiAliasingPasses,
	 "Number of anti-aliasing passes (0 is no anti-aliasing)",
	 NULL},
	{"yafrayAntiAliasingSamples",
	 (getter)RenderData_getYafrayAntiAliasingSamples, (setter)RenderData_setYafrayAntiAliasingSamples,
	 "Number of samples per pass",
	 NULL},
	{"yafrayAntiAliasingPixelSize",
	 (getter)RenderData_getYafrayAntiAliasingPixelSize, (setter)RenderData_setYafrayAntiAliasingPixelSize,
	 "Anti-aliasing pixel filter size",
	 NULL},
	{"yafrayAntiAliasingThreshold",
	 (getter)RenderData_getYafrayAntiAliasingThreshold, (setter)RenderData_setYafrayAntiAliasingThreshold,
	 "Anti-aliasing threshold",
	 NULL},
	{"yafrayGICache",
	 (getter)RenderData_getYafrayGICache, (setter)RenderData_setYafrayGICache,
	 "Cache occlusion/irradiance samples (faster)",
	 NULL},
	{"yafrayGICacheBumpNormals",
	 (getter)RenderData_getYafrayGICacheBumpNormals, (setter)RenderData_setYafrayGICacheBumpNormals,
	 "Enable/disable bumpnormals for cache",
	 NULL},
	{"yafrayGICacheShadowQuality",
	 (getter)RenderData_getYafrayGICacheShadowQuality, (setter)RenderData_setYafrayGICacheShadowQuality,
	 "Shadow quality, keep it under 0.95 :-)",
	 NULL},
	{"yafrayGICachePixelsPerSample",
	 (getter)RenderData_getYafrayGICachePixelsPerSample, (setter)RenderData_setYafrayGICachePixelsPerSample,
	 "Maximum number of pixels without samples, the lower the better and slower",
	 NULL},
	{"yafrayGICacheRefinement",
	 (getter)RenderData_getYafrayGICacheRefinement, (setter)RenderData_setYafrayGICacheRefinement,
	 "Threshold to refine shadows EXPERIMENTAL. 1 = no refinement",
	 NULL},
	{"yafrayGIPhotons",
	 (getter)RenderData_getYafrayGIPhotons, (setter)RenderData_setYafrayGIPhotons,
	 "Enable/disable use of global photons to help in GI",
	 NULL},
	{"yafrayGITunePhotons",
	 (getter)RenderData_getYafrayGITunePhotons, (setter)RenderData_setYafrayGITunePhotons,
	 "If true the photonmap is shown directly in the render for tuning",
	 NULL},
  
	/* Bake stuff */
	{"bakeClear",
	 (getter)RenderData_getBakeMode, (setter)RenderData_setBakeMode,
	 "Clear the image before baking",
	 (void *)R_BAKE_CLEAR},
	{"bakeToActive",
	 (getter)RenderData_getBakeMode, (setter)RenderData_setBakeMode,
	 "Bake selection to active",
	 (void *)R_BAKE_TO_ACTIVE},
	{"bakeNormalize",
	 (getter)RenderData_getBakeMode, (setter)RenderData_setBakeMode,
	 "Normalize AO and displacement to the dist range",
	 (void *)R_BAKE_NORMALIZE},
	{"bakeMargin",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "number of pixels to use as a margin for the edges of the image",
	 (void *)EXPP_RENDER_ATTR_BAKEMARGIN},
	{"bakeMode",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "The mode for baking, see Blender.Scene.Render.BakeModes",
	 (void *)EXPP_RENDER_ATTR_BAKEMODE},
	{"bakeNormalSpace",
	 (getter)RenderData_getIValueAttr, (setter)RenderData_setIValueAttrClamp,
	 "The mode for baking, see Blender.Scene.Render.BakeNormalSpaceModes",
	 (void *)EXPP_RENDER_ATTR_BAKENORMALSPACE},
	{"bakeDist",
	 (getter)RenderData_getFloatAttr, (setter)RenderData_setFloatAttrClamp,
	 "Distance (in blender units)",
	 (void *)EXPP_RENDER_ATTR_BAKEDIST},
	{"bakeBias",
	 (getter)RenderData_getFloatAttr, (setter)RenderData_setFloatAttrClamp,
	 "Bias towards faces further away from the object (in blender units)",
	 (void *)EXPP_RENDER_ATTR_BAKEDIST},
	{NULL,NULL,NULL,NULL,NULL}
};

/***************************************************************************/
/* BPy_RenderData method def                                               */
/***************************************************************************/
static PyMethodDef BPy_RenderData_methods[] = {
	{"render", ( PyCFunction ) RenderData_Render, METH_NOARGS,
	 "() - render the scene"},
	{"bake", ( PyCFunction ) RenderData_Bake, METH_NOARGS,
	 "() - bake current selection"},
	{"saveRenderedImage", (PyCFunction)RenderData_SaveRenderedImage, METH_VARARGS,
	 "(filename) - save an image generated by a call to render() (set output path first)"},
	{"renderAnim", ( PyCFunction ) RenderData_RenderAnim, METH_NOARGS,
	 "() - render a sequence from start frame to end frame"},
	{"play", ( PyCFunction ) RenderData_Play, METH_NOARGS,
	 "() - play animation of rendered images/avi (searches Pics: field)"},
	{"setRenderPath", ( PyCFunction ) RenderData_SetRenderPath,
	 METH_VARARGS,
	 "(string) - get/set the path to output the rendered images to"},
	{"getRenderPath", ( PyCFunction ) RenderData_getRenderPath,
	 METH_NOARGS,
	 "() - get the path to directory where rendered images will go"},
	{"getFrameFilename", ( PyCFunction ) RenderData_getFrameFilename,
	 METH_VARARGS,
	 "() - get the filename of the frame this will be rendered, taking into account extension and frame range"},
	{"setBackbufPath", ( PyCFunction ) RenderData_SetBackbufPath,
	 METH_VARARGS,
	 "(string) - get/set the path to a background image and load it"},
	{"getBackbufPath", ( PyCFunction ) RenderData_getBackbufPath,
	 METH_NOARGS,
	 "() - get the path to background image file"},
	{"enableBackbuf", ( PyCFunction ) RenderData_EnableBackbuf,
	 METH_VARARGS,
	 "(bool) - enable/disable the backbuf image"},
	{"enableExtensions", ( PyCFunction ) RenderData_EnableExtensions,
	 METH_VARARGS,
	 "(bool) - enable/disable windows extensions for output files"},
	{"enableSequencer", ( PyCFunction ) RenderData_EnableSequencer,
	 METH_VARARGS,
	 "(bool) - enable/disable Do Sequence"},
	{"enableRenderDaemon", ( PyCFunction ) RenderData_EnableRenderDaemon,
	 METH_VARARGS,
	 "(bool) - enable/disable Scene daemon"},
	{"enableToonShading", ( PyCFunction ) RenderData_EnableToonShading,
	 METH_VARARGS,
	 "(bool) - enable/disable Edge rendering"},
	{"edgeIntensity", ( PyCFunction ) RenderData_EdgeIntensity,
	 METH_VARARGS,
	 "(int) - get/set edge intensity for toon shading"},
	{"setEdgeColor", ( PyCFunction ) RenderData_SetEdgeColor, METH_VARARGS,
	 "(f,f,f) - set the edge color for toon shading - Red,Green,Blue expected."},
	{"getEdgeColor", ( PyCFunction ) RenderData_GetEdgeColor, METH_NOARGS,
	 "() - get the edge color for toon shading - Red,Green,Blue expected."},
	{"enableOversampling", ( PyCFunction ) RenderData_EnableOversampling,
	 METH_VARARGS,
	 "(bool) - enable/disable oversampling (anit-aliasing)."},
	{"setOversamplingLevel",
	 ( PyCFunction ) RenderData_SetOversamplingLevel, METH_VARARGS,
	 "(enum) - get/set the level of oversampling (anit-aliasing)."},
	{"enableMotionBlur", ( PyCFunction ) RenderData_EnableMotionBlur,
	 METH_VARARGS,
	 "(bool) - enable/disable MBlur."},
	{"motionBlurLevel", ( PyCFunction ) RenderData_MotionBlurLevel,
	 METH_VARARGS,
	 "(float) - get/set the length of shutter time for motion blur."},
	{"partsX", ( PyCFunction ) RenderData_PartsX, METH_VARARGS,
	 "(int) - get/set the number of parts to divide the render in the X direction"},
	{"partsY", ( PyCFunction ) RenderData_PartsY, METH_VARARGS,
	 "(int) - get/set the number of parts to divide the render in the Y direction"},
	{"enableSky", ( PyCFunction ) RenderData_EnableSky, METH_NOARGS,
	 "() - enable render background with sky"},
	{"enablePremultiply", ( PyCFunction ) RenderData_EnablePremultiply,
	 METH_NOARGS,
	 "() - enable premultiply alpha"},
	{"enableKey", ( PyCFunction ) RenderData_EnableKey, METH_NOARGS,
	 "() - enable alpha and color values remain unchanged"},
	{"enableShadow", ( PyCFunction ) RenderData_EnableShadow, METH_VARARGS,
	 "(bool) - enable/disable shadow calculation"},
	{"enablePanorama", ( PyCFunction ) RenderData_EnablePanorama,
	 METH_VARARGS,
	 "(bool) - enable/disable panorama rendering (output width is multiplied by Xparts)"},
	{"enableEnvironmentMap",
	 ( PyCFunction ) RenderData_EnableEnvironmentMap, METH_VARARGS,
	 "(bool) - enable/disable environment map rendering"},
	{"enableRayTracing", ( PyCFunction ) RenderData_EnableRayTracing,
	 METH_VARARGS,
	 "(bool) - enable/disable ray tracing"},
	{"enableRadiosityRender",
	 ( PyCFunction ) RenderData_EnableRadiosityRender, METH_VARARGS,
	 "(bool) - enable/disable radiosity rendering"},
	{"getRenderWinSize", ( PyCFunction ) RenderData_getRenderWinSize,
	 METH_NOARGS,
	 "() - get the size of the render window"},
	{"setRenderWinSize", ( PyCFunction ) RenderData_SetRenderWinSize,
	 METH_VARARGS,
	 "(int) - set the size of the render window"},
	{"enableFieldRendering",
	 ( PyCFunction ) RenderData_EnableFieldRendering, METH_VARARGS,
	 "(bool) - enable/disable field rendering"},
	{"enableOddFieldFirst", ( PyCFunction ) RenderData_EnableOddFieldFirst,
	 METH_VARARGS,
	 "(bool) - enable/disable Odd field first rendering (Default: Even field)"},
	{"enableFieldTimeDisable",
	 ( PyCFunction ) RenderData_EnableFieldTimeDisable, METH_VARARGS,
	 "(bool) - enable/disable time difference in field calculations"},
	{"enableGaussFilter", ( PyCFunction ) RenderData_EnableGaussFilter,
	 METH_VARARGS,
	 "(bool) - enable/disable Gauss sampling filter for antialiasing"},
	{"enableBorderRender", ( PyCFunction ) RenderData_EnableBorderRender,
	 METH_VARARGS,
	 "(bool) - enable/disable small cut-out rendering"},
	{"setBorder", ( PyCFunction ) RenderData_SetBorder, METH_VARARGS,
	 "(f,f,f,f) - set the border for border rendering"},
	{"gaussFilterSize", ( PyCFunction ) RenderData_GaussFilterSize,
	 METH_VARARGS,
	 "(float) - get/sets the Gauss filter size"},
	{"startFrame", ( PyCFunction ) RenderData_StartFrame, METH_VARARGS,
	 "(int) - get/set the starting frame for rendering"},
	{"currentFrame", ( PyCFunction ) RenderData_CurrentFrame, METH_VARARGS,
	 "(int) - get/set the current frame for rendering"},
	{"endFrame", ( PyCFunction ) RenderData_EndFrame, METH_VARARGS,
	 "(int) - get/set the ending frame for rendering"},
	{"getTimeCode", ( PyCFunction ) RenderData_getTimeCode, METH_NOARGS,
	 "get the current frame in HH:MM:SS:FF format"},
	{"imageSizeX", ( PyCFunction ) RenderData_ImageSizeX, METH_VARARGS,
	 "(int) - get/set the image width in pixels"},
	{"imageSizeY", ( PyCFunction ) RenderData_ImageSizeY, METH_VARARGS,
	 "(int) - get/set the image height in pixels"},
	{"aspectRatioX", ( PyCFunction ) RenderData_AspectRatioX, METH_VARARGS,
	 "(int) - get/set the horizontal aspect ratio"},
	{"aspectRatioY", ( PyCFunction ) RenderData_AspectRatioY, METH_VARARGS,
	 "(int) - get/set the vertical aspect ratio"},
	{"setRenderer", ( PyCFunction ) RenderData_SetRenderer, METH_VARARGS,
	 "(enum) - get/set which renderer to render the output"},
	{"enableCropping", ( PyCFunction ) RenderData_EnableCropping,
	 METH_VARARGS,
	 "(bool) - enable/disable exclusion of border rendering from total image"},
	{"setImageType", ( PyCFunction ) RenderData_SetImageType, METH_VARARGS,
	 "(enum) - get/set the type of image to output from the render"},
	{"quality", ( PyCFunction ) RenderData_Quality, METH_VARARGS,
	 "(int) - get/set quality get/setting for JPEG images, AVI Jpeg and SGI movies"},
	{"framesPerSec", ( PyCFunction ) RenderData_FramesPerSec, METH_VARARGS,
	 "(int) - get/set frames per second"},
	{"enableGrayscale", ( PyCFunction ) RenderData_EnableGrayscale,
	 METH_NOARGS,
	 "() - images are saved with BW (grayscale) data"},
	{"enableRGBColor", ( PyCFunction ) RenderData_EnableRGBColor,
	 METH_NOARGS,
	 "() - images are saved with RGB (color) data"},
	{"enableRGBAColor", ( PyCFunction ) RenderData_EnableRGBAColor,
	 METH_NOARGS,
	 "() - images are saved with RGB and Alpha data (if supported)"},
	{"sizePreset", ( PyCFunction ) RenderData_SizePreset, METH_VARARGS,
	 "(enum) - get/set the render to one of a few preget/sets"},
	{"setYafrayGIQuality", ( PyCFunction ) RenderData_SetYafrayGIQuality,
	 METH_VARARGS,
	 "(enum) - set yafray global Illumination quality"},
	{"getYafrayGIQuality", ( PyCFunction ) RenderData_GetYafrayGIQuality,
	 METH_VARARGS,
	 "(enum) - get yafray global Illumination quality"},
	{"setYafrayGIMethod", ( PyCFunction ) RenderData_SetYafrayGIMethod,
	 METH_VARARGS,
	 "(enum) - set yafray global Illumination method"},
	{"getYafrayGIMethod", ( PyCFunction ) RenderData_GetYafrayGIMethod,
	 METH_VARARGS,
	 "(enum) - get yafray global Illumination method"},
	{"yafrayGIPower", ( PyCFunction ) RenderData_YafrayGIPower,
	 METH_VARARGS,
	 "(float) - get/set GI lighting intensity scale"},
	{"yafrayGIIndirPower", ( PyCFunction ) RenderData_YafrayGIIndirPower,
	 METH_VARARGS,
	 "(float) - get/set GI indifect lighting intensity scale"},
	{"yafrayGIDepth", ( PyCFunction ) RenderData_YafrayGIDepth,
	 METH_VARARGS,
	 "(int) - get/set number of bounces of the indirect light"},
	{"yafrayGICDepth", ( PyCFunction ) RenderData_YafrayGICDepth,
	 METH_VARARGS,
	 "(int) - get/set number of bounces inside objects (for caustics)"},
	{"enableYafrayGICache", ( PyCFunction ) RenderData_EnableYafrayGICache,
	 METH_VARARGS,
	 "(bool) - enable/disable cache irradiance samples (faster)"},
	{"enableYafrayGIPhotons",
	 ( PyCFunction ) RenderData_EnableYafrayGIPhotons, METH_VARARGS,
	 "(bool) - enable/disable use global photons to help in GI"},
	{"yafrayGIPhotonCount", ( PyCFunction ) RenderData_YafrayGIPhotonCount,
	 METH_VARARGS,
	 "(int) - get/set number of photons to shoot"},
	{"yafrayGIPhotonRadius",
	 ( PyCFunction ) RenderData_YafrayGIPhotonRadius, METH_VARARGS,
	 "(float) - get/set radius to search for photons to mix (blur)"},
	{"yafrayGIPhotonMixCount",
	 ( PyCFunction ) RenderData_YafrayGIPhotonMixCount, METH_VARARGS,
	 "(int) - get/set number of photons to mix"},
	{"enableYafrayGITunePhotons",
	 ( PyCFunction ) RenderData_EnableYafrayGITunePhotons, METH_VARARGS,
	 "(bool) - enable/disable show the photonmap directly in the render for tuning"},
	{"yafrayGIShadowQuality",
	 ( PyCFunction ) RenderData_YafrayGIShadowQuality, METH_VARARGS,
	 "(float) - get/set the shadow quality, keep it under 0.95"},
	{"yafrayGIPixelsPerSample",
	 ( PyCFunction ) RenderData_YafrayGIPixelsPerSample, METH_VARARGS,
	 "(int) - get/set maximum number of pixels without samples, the lower the better and slower"},
	{"yafrayGIRefinement", ( PyCFunction ) RenderData_YafrayGIRefinement,
	 METH_VARARGS,
	 "(float) - get/setthreshold to refine shadows EXPERIMENTAL. 1 = no refinement"},
	{"yafrayRayBias", ( PyCFunction ) RenderData_YafrayRayBias,
	 METH_VARARGS,
	 "(float) - get/set shadow ray bias to avoid self shadowing"},
	{"yafrayRayDepth", ( PyCFunction ) RenderData_YafrayRayDepth,
	 METH_VARARGS,
	 "(int) - get/set maximum render ray depth from the camera"},
	{"yafrayGamma", ( PyCFunction ) RenderData_YafrayGamma, METH_VARARGS,
	 "(float) - get/set gamma correction, 1 is off"},
	{"yafrayExposure", ( PyCFunction ) RenderData_YafrayExposure,
	 METH_VARARGS,
	 "(float) - get/set exposure adjustment, 0 is off"},
	{"enableGameFrameStretch",
	 ( PyCFunction ) RenderData_EnableGameFrameStretch, METH_NOARGS,
	 "(l) - enble stretch or squeeze the viewport to fill the display window"},
	{"enableGameFrameExpose",
	 ( PyCFunction ) RenderData_EnableGameFrameExpose, METH_NOARGS,
	 "(l) - enable show the entire viewport in the display window, viewing more horizontally or vertically"},
	{"enableGameFrameBars", ( PyCFunction ) RenderData_EnableGameFrameBars,
	 METH_NOARGS,
	 "() - enable show the entire viewport in the display window, using bar horizontally or vertically"},
	{"setGameFrameColor", ( PyCFunction ) RenderData_SetGameFrameColor,
	 METH_VARARGS,
	 "(f,f,f) - set the red, green, blue component of the bars"},
	{"getGameFrameColor", ( PyCFunction ) RenderData_GetGameFrameColor,
	 METH_NOARGS,
	 "() - get the red, green, blue component of the bars"},
	{"SGIMaxsize", ( PyCFunction ) RenderData_SGIMaxsize, METH_VARARGS,
	 "(int) - get/set maximum size per frame to save in an SGI movie"},
	{"enableSGICosmo", ( PyCFunction ) RenderData_EnableSGICosmo,
	 METH_VARARGS,
	 "(bool) - enable/disable attempt to save SGI movies using Cosmo hardware"},
	{"oldMapValue", ( PyCFunction ) RenderData_OldMapValue, METH_VARARGS,
	 "(int) - get/set specify old map value in frames"},
	{"newMapValue", ( PyCFunction ) RenderData_NewMapValue, METH_VARARGS,
	 "(int) - get/set specify new map value in frames"},
  /* renderlayers */
	{"addRenderLayer", ( PyCFunction ) RenderData_addRenderLayer, METH_VARARGS,
	 "(string) - add a new render layer"},
	{"removeRenderLayer", ( PyCFunction ) RenderData_removeRenderLayer, METH_O,
	 "(renderLayer) - remove a render layer from this scene"},
	{NULL, NULL, 0, NULL}
};
 
/*------------------------------------BPy_RenderData Type defintion------ */
PyTypeObject RenderData_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender RenderData",       /* char *tp_name; */
	sizeof( BPy_RenderData ),   /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) RenderData_repr,     /* reprfunc tp_repr; */

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
	BPy_RenderData_methods,     /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_RenderData_getseters,   /* struct PyGetSetDef *tp_getset; */
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




/* render layers */

static PyObject *RenderLayer_repr( BPy_RenderLayer * self )
{
	if( self->renderLayer )
		return PyString_FromFormat( "[RenderLayer \"%s\"]",
					    self->renderLayer->name  );
	else
		return PyString_FromString( "NULL" );
}

static PyObject *RenderLayer_getName( BPy_RenderLayer * self )
{
	if( !self->renderLayer )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	return PyString_FromString( self->renderLayer->name );
}

static int RenderLayer_setName( BPy_RenderLayer * self, PyObject *value )
{
	char *name = NULL;
	int index = BLI_findindex(&self->scene->r.layers, self->renderLayer);
	
	if( !self->renderLayer )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	name = PyString_AsString ( value );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );
	
	/* No check for doubles? - blender dosnt so we dont have to! */
	BLI_strncpy( self->renderLayer->name, name, sizeof( self->renderLayer->name ) );
	
	if(self->scene->nodetree) {
		bNode *node;
		for(node= self->scene->nodetree->nodes.first; node; node= node->next) {
			if(node->type==CMP_NODE_R_LAYERS && node->id==NULL) {
				if(node->custom1==index)
					BLI_strncpy(node->name, self->renderLayer->name, NODE_MAXSTR);
			}
		}
	}
	
	return 0;
}

static PyObject *RenderLayer_getLightGroup( BPy_RenderLayer * self )
{
	if( !self->renderLayer )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	return Group_CreatePyObject( self->renderLayer->light_override );
}
static int RenderLayer_setLightGroup( BPy_RenderLayer * self, PyObject * value )
{
	if( !self->renderLayer )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	return GenericLib_assignData(value, (void **) &self->renderLayer->light_override, NULL, 1, ID_GR, 0);
}


/*****************************************************************************/
/* Python BPy_Render getsetattr funcs:                                       */
/*****************************************************************************/
static int RenderLayer_setLayers( BPy_RenderLayer * self, PyObject * value, void *zlay )
{
	unsigned int laymask = 0;
	
	if( !self->renderLayer )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected an integer (bitmask) as argument" );
	
	laymask = ( unsigned int )PyInt_AS_LONG( value );
	
	if((laymask < 0) ||  (!zlay && laymask == 0))
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "layer value too low" );
	
	if (zlay) {
		self->renderLayer->lay_zmask= laymask & ((1<<20) - 1);
	} else {
		self->renderLayer->lay= laymask & ((1<<20) - 1);
	}
	return 0;
}

static PyObject *RenderLayer_getLayers( BPy_RenderLayer * self, void *zlay )
{
	if (zlay) {
		return PyInt_FromLong( self->renderLayer->lay_zmask );
	} else {
		return PyInt_FromLong( self->renderLayer->lay );
	}
}

static PyObject *RenderLayer_getLayflagBits( BPy_RenderLayer *self, void *type )
{
	int itype = (int)type;
	if( !self->renderLayer )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	/* use negative numbers to flip truth */
	if (self->renderLayer->lay & itype)	Py_RETURN_TRUE;
	else								Py_RETURN_FALSE;
}

static int RenderLayer_setLayflagBits( BPy_RenderLayer *self, PyObject *value,
		void *type )
{
	/* use negative numbers to flip truth */
	int param = PyObject_IsTrue( value );
		
	if( !self->renderLayer )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if (param) {
		self->renderLayer->lay |= (int)type;
	} else {
		self->renderLayer->lay &= ~(int)type;
	}
	return 0;
}

static PyObject *RenderLayer_getPassBits( BPy_RenderLayer *self, void *type )
{
	int itype = (int)type;
	if( !self->renderLayer )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	/* use negative numbers to flip truth */
	if (self->renderLayer->passflag & itype)	Py_RETURN_TRUE;
	else										Py_RETURN_FALSE;
}

static int RenderLayer_setPassBits( BPy_RenderLayer *self, PyObject *value,
		void *type )
{
	/* use negative numbers to flip truth */
	int param = PyObject_IsTrue( value );
		
	if( !self->renderLayer )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if (param) {
		self->renderLayer->passflag |= ((int)type);
	} else {
		self->renderLayer->passflag &= ~((int)type);
	}
	return 0;
}

static PyObject *RenderLayer_getPassXorBits( BPy_RenderLayer *self, void *type )
{
	if( !self->renderLayer )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	/* use negative numbers to flip truth */
	if (self->renderLayer->pass_xor & (int)type)	Py_RETURN_TRUE;
	else										Py_RETURN_FALSE;
}

static int RenderLayer_setPassXorBits( BPy_RenderLayer *self, PyObject *value,
		void *type )
{
	int param = PyObject_IsTrue( value );
		
	if( !self->renderLayer )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This render layer has been removed!" );
	
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if (param) {
		self->renderLayer->passflag |= (int)type;
	} else {
		self->renderLayer->passflag &= ~(int)type;
	}
	return 0;
}

/***************************************************************************/
/* BPy_RenderData attribute def                                            */
/***************************************************************************/
static PyGetSetDef BPy_RenderLayer_getseters[] = {
	{"name",
	 (getter)RenderLayer_getName, (setter)RenderLayer_setName,
	 "",
	 (void *)NULL},
	{"lightGroup",
	 (getter)RenderLayer_getLightGroup, (setter)RenderLayer_setLightGroup,
	 "",
	 (void *)NULL},
	/*{"material",
	 (getter)RenderLayer_getMaterial, (setter)RenderLayer_setMaterial,
	 "",
	 (void *)NULL},*/
	{"enable",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "enable this render layer",
	 (void *)SCE_LAY_DISABLE},
	{"enableZMask",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "Only render what's in front of the solid z values",
	 (void *)SCE_LAY_ZMASK},
	{"enableZMaskAll",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "Fill in Z values for solid faces in invisible layers, for masking",
	 (void *)SCE_LAY_ALL_Z},
	  
	{"enableSolid",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "Render Solid faces in this Layer",
	 (void *)SCE_LAY_SOLID},
	{"enableZTra",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "Render Z-Transparent faces in this Layer (On top of Solid and Halos)",
	 (void *)SCE_LAY_ZTRA},
	{"enableHalo",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "Render Halos in this Layer (on top of Solid)",
	 (void *)SCE_LAY_HALO},
	{"enableEdge",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "Render Edge-enhance in this Layer (only works for Solid faces)",
	 (void *)SCE_LAY_EDGE},
	{"enableSky",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "Render Sky or backbuffer in this Layer",
	 (void *)SCE_LAY_SKY},
	{"enableStrand",
	 (getter)RenderLayer_getLayflagBits, (setter)RenderLayer_setLayflagBits,
	 "Render Strands in this Layer",
	 (void *)SCE_LAY_STRAND},

	{"layerMask",
	 (getter)RenderLayer_getLayers, (setter)RenderLayer_setLayers,
	 "",
	 (void *)0},
	{"zLayerMask",
	 (getter)RenderLayer_getLayers, (setter)RenderLayer_setLayers,
	 "",
	 (void *)1},
	  
	/* passes */
	{"passCombined",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver full combined RGBA buffer",
	 (void *)SCE_PASS_COMBINED},
	{"passZ",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Z values pass",
	 (void *)SCE_PASS_Z},
	{"passSpeed",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Speed Vector pass",
	 (void *)SCE_PASS_VECTOR},
	{"passNormal",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Normal pass",
	 (void *)SCE_PASS_NORMAL},
	{"passUV",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Texture UV pass",
	 (void *)SCE_PASS_UV},
	{"passMist",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Mist factor pass (0-1)",
	 (void *)SCE_PASS_MIST},
	{"passIndex",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Object Index pass",
	 (void *)SCE_PASS_INDEXOB},
	{"passColor",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver shade-less Color pass",
	 (void *)SCE_PASS_RGBA},
	{"passDiffuse",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Diffuse pass",
	 (void *)SCE_PASS_DIFFUSE},
	{"passSpecular",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Specular pass",
	 (void *)SCE_PASS_SPEC},
	{"passShadow",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Shadow pass",
	 (void *)SCE_PASS_SHADOW},
	{"passAO",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver AO pass",
	 (void *)SCE_PASS_AO},
	{"passReflect",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Raytraced Reflection pass",
	 (void *)SCE_PASS_REFLECT},
	{"passRefract",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Raytraced Reflection pass",
	 (void *)SCE_PASS_REFRACT},
	{"passRadiosiy",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Radiosity pass",
	 (void *)SCE_PASS_RADIO},
	
	/* xor */
	{"passSpecularXOR",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Specular pass XOR",
	 (void *)SCE_PASS_SPEC},
	{"passShadowXOR",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver Shadow pass XOR",
	 (void *)SCE_PASS_SHADOW},
	{"passAOXOR",
	 (getter)RenderLayer_getPassBits, (setter)RenderLayer_setPassBits,
	 "Deliver AO pass XOR",
	 (void *)SCE_PASS_AO},
	{"passRefractXOR",
	 (getter)RenderLayer_getPassXorBits, (setter)RenderLayer_setPassXorBits,
	 "Deliver Raytraced Reflection pass XOR",
	 (void *)SCE_PASS_REFRACT},
	{"passRadiosiyXOR",
	 (getter)RenderLayer_getPassXorBits, (setter)RenderLayer_setPassXorBits,
	 "Deliver Radiosity pass XOR",
	 (void *)SCE_PASS_RADIO},
	
	{NULL,NULL,NULL,NULL,NULL}
};

/* no methods */

/*------------------------------------BPy_RenderData Type defintion------ */
PyTypeObject RenderLayer_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender RenderLayer",       /* char *tp_name; */
	sizeof( BPy_RenderLayer ),   /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) RenderLayer_repr,     /* reprfunc tp_repr; */

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
	NULL,     					/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_RenderLayer_getseters,   /* struct PyGetSetDef *tp_getset; */
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

/***************************************************************************/
/* BPy_RenderData Callbacks                                                */
/***************************************************************************/

PyObject *RenderLayer_CreatePyObject( struct Scene * scene, struct SceneRenderLayer * renderLayer )
{
	BPy_RenderLayer *py_renderlayer;

	py_renderlayer =
		( BPy_RenderLayer * ) PyObject_NEW( BPy_RenderLayer,
						   &RenderLayer_Type );

	if( py_renderlayer == NULL ) {
		return ( NULL );
	}
	py_renderlayer->renderLayer = renderLayer;
	py_renderlayer->scene = scene;

	return ( ( PyObject * ) py_renderlayer );
}


/***************************************************************************/
/* Render method def                                                       */
/***************************************************************************/
struct PyMethodDef M_Render_methods[] = {
	{"CloseRenderWindow", ( PyCFunction ) M_Render_CloseRenderWindow,
	 METH_NOARGS,
	 "() - close the rendering window"},
	{"EnableDispView", ( PyCFunction ) M_Render_EnableDispView,
	 METH_NOARGS,
	 "(bool) - enable Sceneing in view"},
	{"EnableDispWin", ( PyCFunction ) M_Render_EnableDispWin, METH_NOARGS,
	 "(bool) - enable Sceneing in new window"},
	{"SetRenderWinPos", ( PyCFunction ) M_Render_SetRenderWinPos,
	 METH_VARARGS,
	 "([string list]) - position the rendering window in around the edge of the screen"},
	{NULL, NULL, 0, NULL}
};

static PyObject *M_Render_ModesDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "OSA", PyInt_FromLong( R_OSA ) );
		PyConstant_Insert( d, "SHADOW", PyInt_FromLong( R_SHADOW ) );
		PyConstant_Insert( d, "GAMMA", PyInt_FromLong( R_GAMMA ) );
		PyConstant_Insert( d, "ENVMAP", PyInt_FromLong( R_ENVMAP ) );
		PyConstant_Insert( d, "TOONSHADING", PyInt_FromLong( R_EDGE ) );
		PyConstant_Insert( d, "FIELDRENDER", PyInt_FromLong( R_FIELDS ) );
		PyConstant_Insert( d, "FIELDTIME", PyInt_FromLong( R_FIELDSTILL ) );
		PyConstant_Insert( d, "RADIOSITY", PyInt_FromLong( R_RADIO ) );
		PyConstant_Insert( d, "BORDER_RENDER", PyInt_FromLong( R_BORDER ) );
		PyConstant_Insert( d, "PANORAMA", PyInt_FromLong( R_PANORAMA ) );
		PyConstant_Insert( d, "CROP", PyInt_FromLong( R_CROP ) );
		PyConstant_Insert( d, "ODDFIELD", PyInt_FromLong( R_ODDFIELD ) );
		PyConstant_Insert( d, "MBLUR", PyInt_FromLong( R_MBLUR ) );
		PyConstant_Insert( d, "RAYTRACING", PyInt_FromLong( R_RAYTRACE ) );
		PyConstant_Insert( d, "FIXEDTHREADS", PyInt_FromLong( R_FIXED_THREADS ) );
	}
	return M;
}

static PyObject *M_Render_SceModesDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "SEQUENCER", PyInt_FromLong( R_DOSEQ ) );
		PyConstant_Insert( d, "EXTENSION", PyInt_FromLong( R_EXTENSION ) );
	}
	return M;
}

static PyObject *M_Render_GameFramingDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "BARS",
				PyInt_FromLong( SCE_GAMEFRAMING_BARS ) );
		PyConstant_Insert( d, "EXTEND",
				PyInt_FromLong( SCE_GAMEFRAMING_EXTEND ) );
		PyConstant_Insert( d, "SCALE",
				PyInt_FromLong( SCE_GAMEFRAMING_SCALE ) );
	}
	return M;
}

static PyObject *M_Render_BakeModesDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		
		PyConstant_Insert( d, "LIGHT", PyInt_FromLong( RE_BAKE_LIGHT ) );
		PyConstant_Insert( d, "ALL", PyInt_FromLong( RE_BAKE_ALL ) );
		PyConstant_Insert( d, "AO", PyInt_FromLong( RE_BAKE_AO ) );
		PyConstant_Insert( d, "NORMALS", PyInt_FromLong( RE_BAKE_NORMALS ) );
		PyConstant_Insert( d, "TEXTURE", PyInt_FromLong( RE_BAKE_TEXTURE ) );
		PyConstant_Insert( d, "DISPLACEMENT", PyInt_FromLong( RE_BAKE_DISPLACEMENT ) );
		PyConstant_Insert( d, "SHADOW", PyInt_FromLong( RE_BAKE_SHADOW ) );
	}
	return M;
}


static PyObject *M_Render_BakeNormalSpaceDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		
		PyConstant_Insert( d, "CAMERA", PyInt_FromLong( R_BAKE_SPACE_CAMERA ) );
		PyConstant_Insert( d, "WORLD", PyInt_FromLong( R_BAKE_SPACE_WORLD ) );
		PyConstant_Insert( d, "OBJECT", PyInt_FromLong( R_BAKE_SPACE_OBJECT ) );
		PyConstant_Insert( d, "TANGENT", PyInt_FromLong( R_BAKE_SPACE_TANGENT ) );
	}
	return M;
}

/***************************************************************************/
/* Render Module Init                                                      */
/***************************************************************************/
PyObject *Render_Init( void )
{
	PyObject *submodule;
	PyObject *ModesDict = M_Render_ModesDict( );
	PyObject *SceModesDict = M_Render_SceModesDict( );
	PyObject *GFramingDict = M_Render_GameFramingDict( );
	PyObject *BakeModesDict = M_Render_BakeModesDict( );
	PyObject *BakeNormalSpaceDict = M_Render_BakeNormalSpaceDict( );
	
	if( PyType_Ready( &RenderData_Type ) < 0 )
		return NULL;

	if( PyType_Ready( &RenderLayer_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Scene.Render",
				    M_Render_methods, M_Render_doc );

	if( ModesDict )
		PyModule_AddObject( submodule, "Modes", ModesDict );
	if( SceModesDict )
		PyModule_AddObject( submodule, "SceModes", SceModesDict );
	if( GFramingDict )
		PyModule_AddObject( submodule, "FramingModes", GFramingDict );
	if( BakeModesDict )
		PyModule_AddObject( submodule, "BakeModes", BakeModesDict );
	if( BakeNormalSpaceDict )
		PyModule_AddObject( submodule, "BakeNormalSpaceModes", BakeNormalSpaceDict );
	
	/* ugh: why aren't these in a constant dict? */

	PyModule_AddIntConstant( submodule, "INTERNAL", R_INTERN );
	PyModule_AddIntConstant( submodule, "YAFRAY", R_YAFRAY );
	PyModule_AddIntConstant( submodule, "AVIRAW", R_AVIRAW );
	PyModule_AddIntConstant( submodule, "AVIJPEG", R_AVIJPEG );
	PyModule_AddIntConstant( submodule, "AVICODEC", R_AVICODEC );
	PyModule_AddIntConstant( submodule, "QUICKTIME", R_QUICKTIME );
	PyModule_AddIntConstant( submodule, "TARGA", R_TARGA );
	PyModule_AddIntConstant( submodule, "RAWTGA", R_RAWTGA );
	PyModule_AddIntConstant( submodule, "HDR", R_RADHDR );
	PyModule_AddIntConstant( submodule, "PNG", R_PNG );
	PyModule_AddIntConstant( submodule, "BMP", R_BMP );
	PyModule_AddIntConstant( submodule, "JPEG", R_JPEG90 );
	PyModule_AddIntConstant( submodule, "HAMX", R_HAMX );
	PyModule_AddIntConstant( submodule, "IRIS", R_IRIS );
	PyModule_AddIntConstant( submodule, "IRISZ", R_IRIZ );
	PyModule_AddIntConstant( submodule, "PAL", B_PR_PAL );
	PyModule_AddIntConstant( submodule, "NTSC", B_PR_NTSC );
	PyModule_AddIntConstant( submodule, "DEFAULT", B_PR_PRESET );
	PyModule_AddIntConstant( submodule, "PREVIEW", B_PR_PRV );
	PyModule_AddIntConstant( submodule, "PC", B_PR_PC );
	PyModule_AddIntConstant( submodule, "PAL169", B_PR_PAL169 );
	PyModule_AddIntConstant( submodule, "PANO", B_PR_PANO );
	PyModule_AddIntConstant( submodule, "FULL", B_PR_FULL );
	PyModule_AddIntConstant( submodule, "NONE", PY_NONE );
	PyModule_AddIntConstant( submodule, "LOW", PY_LOW );
	PyModule_AddIntConstant( submodule, "MEDIUM", PY_MEDIUM );
	PyModule_AddIntConstant( submodule, "HIGH", PY_HIGH );
	PyModule_AddIntConstant( submodule, "HIGHER", PY_HIGHER );
	PyModule_AddIntConstant( submodule, "BEST", PY_BEST );
	PyModule_AddIntConstant( submodule, "USEAOSETTINGS", PY_USEAOSETTINGS );
	PyModule_AddIntConstant( submodule, "SKYDOME", PY_SKYDOME );
	PyModule_AddIntConstant( submodule, "GIFULL", PY_FULL );
	PyModule_AddIntConstant( submodule, "OPENEXR", R_OPENEXR );
	PyModule_AddIntConstant( submodule, "MULTILAYER", R_MULTILAYER );
	PyModule_AddIntConstant( submodule, "TIFF", R_TIFF );
	PyModule_AddIntConstant( submodule, "FFMPEG", R_FFMPEG );
	PyModule_AddIntConstant( submodule, "CINEON", R_CINEON );
	PyModule_AddIntConstant( submodule, "DPX", R_DPX );

	return ( submodule );
}

/***************************************************************************/
/* BPy_RenderData Callbacks                                                */
/***************************************************************************/

PyObject *RenderData_CreatePyObject( struct Scene * scene )
{
	BPy_RenderData *py_renderdata;

	py_renderdata =
		( BPy_RenderData * ) PyObject_NEW( BPy_RenderData,
						   &RenderData_Type );

	if( py_renderdata == NULL ) {
		return ( NULL );
	}
	py_renderdata->renderContext = &scene->r;
	py_renderdata->scene = scene;

	return ( ( PyObject * ) py_renderdata );
}

/* #####DEPRECATED###### */

static PyObject *RenderData_SetRenderPath( BPy_RenderData *self,
		PyObject *args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)RenderData_setRenderPath );
}

static PyObject *RenderData_SetBackbufPath( BPy_RenderData *self,
		PyObject *args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)RenderData_setBackbufPath );
}

static PyObject *RenderData_SetOversamplingLevel( BPy_RenderData * self,
		PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)RenderData_setOSALevel );
}

static PyObject *RenderData_SetRenderWinSize( BPy_RenderData * self,
		PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)RenderData_setRenderWinSize );
}

static PyObject *RenderData_SetBorder( BPy_RenderData * self,
		PyObject * args )
{
	return EXPP_setterWrapperTuple( (void *)self, args,
			(setter)RenderData_setBorder );
}

static PyObject *RenderData_SetRenderer( BPy_RenderData * self,
		PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)RenderData_setRenderer );
}

static PyObject *RenderData_SetImageType( BPy_RenderData * self,
		PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)RenderData_setImageType );
}
