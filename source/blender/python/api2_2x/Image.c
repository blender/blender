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
 * Contributor(s): Willian P. Germano, Campbell Barton, Joilnen B. Leite,
 * Austin Benesh
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
#include "Image.h"		/*This must come first */

#include "BDR_drawmesh.h"	/* free_realtime_image */
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_image.h"
#include "BIF_drawimage.h"
#include "BLI_blenlib.h"
#include "DNA_space_types.h"	/* FILE_MAXDIR = 160 */
#include "IMB_imbuf_types.h"	/* for the IB_rect define */
#include "BIF_gl.h"
#include "gen_utils.h"
#include "BKE_packedFile.h"
#include "DNA_packedFile_types.h"
#include "BKE_icons.h"
#include "IMB_imbuf.h"

/* used so we can get G.scene->r.cfra for getting the
current image frame, some images change frame if they are a sequence */
#include "DNA_scene_types.h"

/*****************************************************************************/
/* Python BPy_Image defaults:																								 */
/*****************************************************************************/
#define EXPP_IMAGE_REP			1
#define EXPP_IMAGE_REP_MIN	1
#define EXPP_IMAGE_REP_MAX 16


/************************/
/*** The Image Module ***/
/************************/

/*****************************************************************************/
/* Python API function prototypes for the Image module.	 */
/*****************************************************************************/
static PyObject *M_Image_New( PyObject * self, PyObject * args );
static PyObject *M_Image_Get( PyObject * self, PyObject * args );
static PyObject *M_Image_GetCurrent( PyObject * self );
static PyObject *M_Image_Load( PyObject * self, PyObject * args );


/*****************************************************************************/
/* Python BPy_Image methods declarations:	 */
/*****************************************************************************/
static PyObject *Image_getName( BPy_Image * self );
static PyObject *Image_getFilename( BPy_Image * self );
static PyObject *Image_getSize( BPy_Image * self );
static PyObject *Image_getDepth( BPy_Image * self );
static PyObject *Image_getXRep( BPy_Image * self );
static PyObject *Image_getYRep( BPy_Image * self );
static PyObject *Image_getBindCode( BPy_Image * self );
static PyObject *Image_getStart( BPy_Image * self );
static PyObject *Image_getEnd( BPy_Image * self );
static PyObject *Image_getSpeed( BPy_Image * self );
static PyObject *Image_setName( BPy_Image * self, PyObject * args );
static PyObject *Image_setFilename( BPy_Image * self, PyObject * args );
static PyObject *Image_setXRep( BPy_Image * self, PyObject * args );
static PyObject *Image_setYRep( BPy_Image * self, PyObject * args );
static PyObject *Image_setStart( BPy_Image * self, PyObject * args );
static PyObject *Image_setEnd( BPy_Image * self, PyObject * args );
static PyObject *Image_setSpeed( BPy_Image * self, PyObject * args );
static PyObject *Image_reload( BPy_Image * self );
static PyObject *Image_glLoad( BPy_Image * self );
static PyObject *Image_glFree( BPy_Image * self );
static PyObject *Image_getPixelF( BPy_Image * self, PyObject * args );
static PyObject *Image_getPixelI( BPy_Image * self, PyObject * args );
static PyObject *Image_setPixelF( BPy_Image * self, PyObject * args );
static PyObject *Image_setPixelI( BPy_Image * self, PyObject * args );
static PyObject *Image_getMaxXY( BPy_Image * self );
static PyObject *Image_getMinXY( BPy_Image * self );
static PyObject *Image_save( BPy_Image * self );
static PyObject *Image_unpack( BPy_Image * self, PyObject * args );
static PyObject *Image_pack( BPy_Image * self );


/*****************************************************************************/
/* Python BPy_Image methods table:	 */
/*****************************************************************************/
static PyMethodDef BPy_Image_methods[] = {
	/* name, method, flags, doc */
	{"getPixelF", ( PyCFunction ) Image_getPixelF, METH_VARARGS,
	 "(int, int) - Get pixel color as floats 0.0-1.0 returns [r,g,b,a]"},
	{"getPixelI", ( PyCFunction ) Image_getPixelI, METH_VARARGS,
	 "(int, int) - Get pixel color as ints 0-255 returns [r,g,b,a]"},
	{"setPixelF", ( PyCFunction ) Image_setPixelF, METH_VARARGS,
	 "(int, int, [f r,f g,f b,f a]) - Set pixel color using floats 0.0-1.0"},
	{"setPixelI", ( PyCFunction ) Image_setPixelI, METH_VARARGS,
	 "(int, int, [i r, i g, i b, i a]) - Set pixel color using ints 0-255"},
	{"getMaxXY", ( PyCFunction ) Image_getMaxXY, METH_NOARGS,
	 "() - Get maximum x & y coordinates of current image as [x, y]"},
	{"getMinXY", ( PyCFunction ) Image_getMinXY, METH_NOARGS,
	 "() - Get minimun x & y coordinates of image as [x, y]"},
	{"getName", ( PyCFunction ) Image_getName, METH_NOARGS,
	 "() - Return Image object name"},
	{"getFilename", ( PyCFunction ) Image_getFilename, METH_NOARGS,
	 "() - Return Image object filename"},
	{"getSize", ( PyCFunction ) Image_getSize, METH_NOARGS,
	 "() - Return Image object [width, height] dimension in pixels"},
	{"getDepth", ( PyCFunction ) Image_getDepth, METH_NOARGS,
	 "() - Return Image object pixel depth"},
	{"getXRep", ( PyCFunction ) Image_getXRep, METH_NOARGS,
	 "() - Return Image object x repetition value"},
	{"getYRep", ( PyCFunction ) Image_getYRep, METH_NOARGS,
	 "() - Return Image object y repetition value"},
	{"getStart", ( PyCFunction ) Image_getStart, METH_NOARGS,
	 "() - Return Image object start frame."},
	{"getEnd", ( PyCFunction ) Image_getEnd, METH_NOARGS,
	 "() - Return Image object end frame."},
	{"getSpeed", ( PyCFunction ) Image_getSpeed, METH_NOARGS,
	 "() - Return Image object speed (fps)."},
	{"getBindCode", ( PyCFunction ) Image_getBindCode, METH_NOARGS,
	 "() - Return Image object's bind code value"},
	{"reload", ( PyCFunction ) Image_reload, METH_NOARGS,
	 "() - Reload the image from the filesystem"},
	{"glLoad", ( PyCFunction ) Image_glLoad, METH_NOARGS,
	 "() - Load the image data in OpenGL texture memory.\n\
	The bindcode (int) is returned."},
	{"glFree", ( PyCFunction ) Image_glFree, METH_NOARGS,
	 "() - Free the image data from OpenGL texture memory only,\n\
		see also image.glLoad()."},
	{"setName", ( PyCFunction ) Image_setName, METH_VARARGS,
	 "(str) - Change Image object name"},
	{"setFilename", ( PyCFunction ) Image_setFilename, METH_VARARGS,
	 "(str) - Change Image file name"},
	{"setXRep", ( PyCFunction ) Image_setXRep, METH_VARARGS,
	 "(int) - Change Image object x repetition value"},
	{"setYRep", ( PyCFunction ) Image_setYRep, METH_VARARGS,
	 "(int) - Change Image object y repetition value"},
	{"setStart", ( PyCFunction ) Image_setStart, METH_VARARGS,
	 "(int) - Change Image object animation start value"},
	{"setEnd", ( PyCFunction ) Image_setEnd, METH_VARARGS,
	 "(int) - Change Image object animation end value"},
	{"setSpeed", ( PyCFunction ) Image_setSpeed, METH_VARARGS,
	 "(int) - Change Image object animation speed (fps)"},
	{"save", ( PyCFunction ) Image_save, METH_NOARGS,
	 "() - Write image buffer to file"},
	{"unpack", ( PyCFunction ) Image_unpack, METH_VARARGS,
	 "(int) - Unpack image. Uses the values defined in Blender.UnpackModes."},
	{"pack", ( PyCFunction ) Image_pack, METH_NOARGS,
	 "() Pack the image"},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* The following string definitions are used for documentation strings.	 */
/* In Python these will be written to the console when doing a		 */
/* Blender.Image.__doc__		 */
/*****************************************************************************/
static char M_Image_doc[] = "The Blender Image module\n\n";

static char M_Image_New_doc[] =
	"() - return a new Image object";

static char M_Image_Get_doc[] =
	"(name) - return the image with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all images in the\ncurrent scene.";

static char M_Image_GetCurrent_doc[] =
	"() - return the current image, from last active the uv/image view, \
returns None no image is in the view.\n";

static char M_Image_Load_doc[] =
	"(filename) - return image from file filename as Image Object, \
returns None if not found.\n";

/*****************************************************************************/
/* Python method structure definition for Blender.Image module:		 */
/*****************************************************************************/
struct PyMethodDef M_Image_methods[] = {
	{"New", M_Image_New, METH_VARARGS, M_Image_New_doc},
	{"Get", M_Image_Get, METH_VARARGS, M_Image_Get_doc},
	{"GetCurrent", ( PyCFunction ) M_Image_GetCurrent, METH_NOARGS, M_Image_GetCurrent_doc},	
	{"get", M_Image_Get, METH_VARARGS, M_Image_Get_doc},
	{"Load", M_Image_Load, METH_VARARGS, M_Image_Load_doc},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Function:    M_Image_New     */
/* Python equivalent:        Blender.Image.New    */
/*****************************************************************************/
static PyObject *M_Image_New( PyObject * self, PyObject * args)
{
	int width, height, depth;
	char *name;
	Image *image;
	if( !PyArg_ParseTuple( args, "siii", &name, &width, &height, &depth ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected 1 string and 3 ints" ) );
	if (width > 5000 || height > 5000 || width < 1 || height < 1)
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
					"Image width and height must be between 1 and 5000" ) );
	image = new_image(width, height, name, 0);
	if( !image )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject Image_Type" ) );

	/* reset usage count, since new_image() incremented it */
	image->id.us--;
	return Image_CreatePyObject( image );
}



/*****************************************************************************/
/* Function:		M_Image_Get	 */
/* Python equivalent:	Blender.Image.Get   */
/* Description:		Receives a string and returns the image object	 */
/*			whose name matches the string.	If no argument is  */
/*			passed in, a list of all image names in the	 */
/*			current scene is returned.			 */
/*****************************************************************************/
static PyObject *M_Image_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Image *img_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	img_iter = G.main->image.first;

	if( name ) {		/* (name) - Search image by name */

		BPy_Image *wanted_image = NULL;

		while( ( img_iter ) && ( wanted_image == NULL ) ) {
			if( strcmp( name, img_iter->id.name + 2 ) == 0 ) {
				wanted_image = ( BPy_Image * )
					PyObject_NEW( BPy_Image, &Image_Type );
				if( wanted_image )
					wanted_image->image = img_iter;
			}
			img_iter = img_iter->id.next;
		}

		if( wanted_image == NULL ) {	/* Requested image doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Image \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) wanted_image;
	}

	else {			/* () - return a list of all images in the scene */
		int index = 0;
		PyObject *img_list, *pyobj;

		img_list = PyList_New( BLI_countlist( &( G.main->image ) ) );

		if( img_list == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( img_iter ) {
			pyobj = Image_CreatePyObject( img_iter );

			PyList_SET_ITEM( img_list, index, pyobj );

			img_iter = img_iter->id.next;
			index++;
		}

		return ( img_list );
	}
}



/*****************************************************************************/
/* Function:		M_Image_GetCurrent*/
/* Python equivalent:	Blender.Image.GetCurrent   */
/* Description:		Returns the active current (G.sima)	 */
/*			This will be the image last under the mouse cursor */
/*			None if there is no Image.			 */
/*****************************************************************************/
static PyObject *M_Image_GetCurrent( PyObject * self )
{
	if (!G.sima || !G.sima->image) {
		Py_RETURN_NONE;
	}
	return Image_CreatePyObject( G.sima->image );
}



/*****************************************************************************/
/* Function:	M_Image_Load		 */
/* Python equivalent:	Blender.Image.Load   */
/* Description:		Receives a string and returns the image object	 */
/*			whose filename matches the string.		 */
/*****************************************************************************/
static PyObject *M_Image_Load( PyObject * self, PyObject * args )
{
	char *fname;
	Image *img_ptr;
	BPy_Image *image;

	if( !PyArg_ParseTuple( args, "s", &fname ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument" ) );

	image = ( BPy_Image * ) PyObject_NEW( BPy_Image, &Image_Type );

	if( !image )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject Image_Type" ) );

	img_ptr = add_image( fname );
	if( !img_ptr )
		return ( EXPP_ReturnPyObjError( PyExc_IOError,
						"couldn't load image" ) );

	image->image = img_ptr;

	return ( PyObject * ) image;
}


/**
 * getPixelF( x, y )
 *  returns float list of pixel colors in rgba order.
 *  returned values are floats normalized to 0.0 - 1.0.
 *  blender images are all 4x8 bit at the moment apr-2005
 */

static PyObject *Image_getPixelF( BPy_Image * self, PyObject * args )
{

	PyObject *attr;
	Image *image = self->image;
	char *pixel;		/* image data */
	int index;		/* offset into image data */
	int x = 0;
	int y = 0;
	int pixel_size = 4;	/* each pixel is 4 x 8-bits packed in unsigned int */

	if( !PyArg_ParseTuple( args, "ii", &x, &y ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 2 integers" );

	if( !image->ibuf || !image->ibuf->rect )	/* if no image data available */
		load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

	if( !image->ibuf || !image->ibuf->rect )	/* didn't work */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't load image data in Blender" );

	if( image->ibuf->type == 1 )	/* bitplane image */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "unsupported bitplane image format" );

	if( x > ( image->ibuf->x - 1 )
	    || y > ( image->ibuf->y - 1 )
	    || x < image->ibuf->xorig || y < image->ibuf->yorig )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "x or y is out of range" );

	/* 
	   assumption: from looking at source, skipx is often not set,
	   so we calc ourselves
	 */

	index = ( x + y * image->ibuf->x ) * pixel_size;

	pixel = ( char * ) image->ibuf->rect;
	attr = Py_BuildValue( "[f,f,f,f]",
			      ( ( float ) pixel[index] ) / 255.0,
			      ( ( float ) pixel[index + 1] ) / 255.0,
			      ( ( float ) pixel[index + 2] ) / 255.0,
			      ( ( float ) pixel[index + 3] / 255.0 ) );

	if( attr )		/* normal return */
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get pixel colors" );
}


/**
 * getPixelI( x, y )
 *  returns integer list of pixel colors in rgba order.
 *  returned values are ints normalized to 0-255.
 *  blender images are all 4x8 bit at the moment apr-2005
 */

static PyObject *Image_getPixelI( BPy_Image * self, PyObject * args )
{
	PyObject *attr;
	Image *image = self->image;
	char *pixel;		/* image data */
	int index;		/* offset into image data */
	int x = 0;
	int y = 0;
	int pixel_size = 4;	/* each pixel is 4 x 8-bits packed in unsigned int */

	if( !PyArg_ParseTuple( args, "ii", &x, &y ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 2 integers" );

	if( !image->ibuf || !image->ibuf->rect )	/* if no image data available */
		load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

	if( !image->ibuf || !image->ibuf->rect )	/* didn't work */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't load image data in Blender" );

	if( image->ibuf->type == 1 )	/* bitplane image */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "unsupported bitplane image format" );

	if( x > ( image->ibuf->x - 1 )
	    || y > ( image->ibuf->y - 1 )
	    || x < image->ibuf->xorig || y < image->ibuf->yorig )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "x or y is out of range" );

	/* 
	   assumption: from looking at source, skipx is often not set,
	   so we calc ourselves
	 */

	index = ( x + y * image->ibuf->x ) * pixel_size;

	pixel = ( char * ) image->ibuf->rect;
	attr = Py_BuildValue( "[i,i,i,i]",
			      pixel[index],
			      pixel[index + 1],
			      pixel[index + 2], pixel[index + 3] );

	if( attr )		/* normal return */
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get pixel colors" );
}


/* set pixel as floats */

static PyObject *Image_setPixelF( BPy_Image * self, PyObject * args )
{
	Image *image = self->image;
	char *pixel;		/* image data */
	int index;		/* offset into image data */
	int x = 0;
	int y = 0;
	int a = 0;
	int pixel_size = 4;	/* each pixel is 4 x 8-bits packed in unsigned int */
	float p[4];

	if( !PyArg_ParseTuple
	    ( args, "ii(ffff)", &x, &y, &p[0], &p[1], &p[2], &p[3] ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 2 integers and an array of 4 floats" );

	if( !image->ibuf || !image->ibuf->rect )	/* if no image data available */
		load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

	if( !image->ibuf || !image->ibuf->rect )	/* didn't work */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't load image data in Blender" );

	if( image->ibuf->type == 1 )	/* bitplane image */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "unsupported bitplane image format" );

	if( x > ( image->ibuf->x - 1 )
	    || y > ( image->ibuf->y - 1 )
	    || x < image->ibuf->xorig || y < image->ibuf->yorig )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "x or y is out of ruange" );

	for( a = 0; a < 4; a++ ) {
		if( p[a] > 1.0 || p[a] < 0.0 )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "r, g, b, or a is out of range" );
	}


	/* 
	   assumption: from looking at source, skipx is often not set,
	   so we calc ourselves
	 */

	index = ( x + y * image->ibuf->x ) * pixel_size;

	pixel = ( char * ) image->ibuf->rect;

	pixel[index] = ( char ) ( p[0] * 255.0 );
	pixel[index + 1] = ( char ) ( p[1] * 255.0 );
	pixel[index + 2] = ( char ) ( p[2] * 255.0 );
	pixel[index + 3] = ( char ) ( p[3] * 255.0 );

	Py_RETURN_NONE;
}


/* set pixel as ints */

static PyObject *Image_setPixelI( BPy_Image * self, PyObject * args )
{
	Image *image = self->image;
	char *pixel;		/* image data */
	int index;		/* offset into image data */
	int x = 0;
	int y = 0;
	int a = 0;
	int pixel_size = 4;	/* each pixel is 4 x 8-bits packed in unsigned int */
	int p[4];

	if( !PyArg_ParseTuple
	    ( args, "ii(iiii)", &x, &y, &p[0], &p[1], &p[2], &p[3] ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 2 integers and an list of 4 ints" );

	if( !image->ibuf || !image->ibuf->rect )	/* if no image data available */
		load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

	if( !image->ibuf || !image->ibuf->rect )	/* didn't work */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't load image data in Blender" );

	if( image->ibuf->type == 1 )	/* bitplane image */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "unsupported bitplane image format" );

	if( x > ( image->ibuf->x - 1 )
	    || y > ( image->ibuf->y - 1 )
	    || x < image->ibuf->xorig || y < image->ibuf->yorig )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "x or y is out of range" );

	for( a = 0; a < 4; a++ ) {
		if( p[a] > 255 || p[a] < 0 )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "r, g, b, or a is out of range" );
	}

	/* 
	   assumption: from looking at source, skipx is often not set,
	   so we calc ourselves
	 */

	index = ( x + y * image->ibuf->x ) * pixel_size;

	pixel = ( char * ) image->ibuf->rect;

	pixel[index] = ( char ) p[0];
	pixel[index + 1] = ( char ) p[1];
	pixel[index + 2] = ( char ) p[2];
	pixel[index + 3] = ( char ) p[3];

	Py_RETURN_NONE;
}


/* get max extent of image */

static PyObject *Image_getMaxXY( BPy_Image * self )
{
	Image *image = self->image;
	PyObject *attr;

	if( !image->ibuf || !image->ibuf->rect )	/* if no image data available */
		load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

	if( !image->ibuf || !image->ibuf->rect )	/* didn't work */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't load image data in Blender" );

	attr = Py_BuildValue( "[i,i]", image->ibuf->x, image->ibuf->y );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "could not determine max x or y" );
}


/* get min extent of image */

static PyObject *Image_getMinXY( BPy_Image * self )
{
	Image *image = self->image;
	PyObject *attr;

	if( !image->ibuf || !image->ibuf->rect )	/* if no image data available */
		load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

	if( !image->ibuf || !image->ibuf->rect )	/* didn't work */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't load image data in Blender" );

	attr = Py_BuildValue( "[i,i]", image->ibuf->xorig,
			      image->ibuf->yorig );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "could not determine min x or y" );
}

/* unpack image */

static PyObject *Image_unpack( BPy_Image * self, PyObject * args )
{
	Image *image = self->image;
	int mode;
	
	/*get the absolute path */
	if( !PyArg_ParseTuple( args, "i", &mode ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 1 integer" );
	
	if (image->packedfile==NULL)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "image not packed" );
	
	if (unpackImage(image, mode) == RET_ERROR)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"error unpacking image" );
	Py_RETURN_NONE;
}

/* pack image */

static PyObject *Image_pack( BPy_Image * self )
{
	Image *image = self->image;	
	char expandpath[FILE_MAXDIR + FILE_MAXFILE];
	BLI_strncpy(expandpath, image->name, FILE_MAXDIR+FILE_MAXFILE);
	BLI_convertstringcode(expandpath, G.sce, 1);

	if (image->packedfile )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "image alredy packed" );
	if (!BLI_exists(expandpath))
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "image path does not exist" );
		
	image->packedfile = newPackedFile(image->name);
	Py_RETURN_NONE;
}


/* save image to file */

static PyObject *Image_save( BPy_Image * self )
{
	Py_INCREF( Py_None );

	if( !IMB_saveiff
	    ( self->image->ibuf, self->image->name,
	      self->image->ibuf->flags ) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "could not save image" );

	Py_RETURN_NONE;		/*  normal return, image saved */
}

/*****************************************************************************/
/* Function:		Image_Init	 */
/*****************************************************************************/
PyObject *Image_Init( void )
{
	PyObject *submodule;

	Image_Type.ob_type = &PyType_Type;

	submodule =
		Py_InitModule3( "Blender.Image", M_Image_methods,
				M_Image_doc );

	return ( submodule );
}

/*****************************************************************************/
/* Python Image_Type callback function prototypes:	 */
/*****************************************************************************/
static void Image_dealloc( BPy_Image * self );
static int Image_setAttr( BPy_Image * self, char *name, PyObject * v );
static int Image_compare( BPy_Image * a, BPy_Image * b );
static PyObject *Image_getAttr( BPy_Image * self, char *name );
static PyObject *Image_repr( BPy_Image * self );

/*****************************************************************************/
/* Python Image_Type structure definition:   */
/*****************************************************************************/
PyTypeObject Image_Type = {
	PyObject_HEAD_INIT( NULL ) /*     required macro. ( no comma needed )  */ 
	0,	/* ob_size */
	"Blender Image",	/* tp_name */
	sizeof( BPy_Image ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) Image_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) Image_getAttr,	/* tp_getattr */
	( setattrfunc ) Image_setAttr,	/* tp_setattr */
	( cmpfunc ) Image_compare,	/* tp_compare */
	( reprfunc ) Image_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_Image_methods,	/* tp_methods */
	0,			/* tp_members */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* up to tp_del, to avoid a warning */
};

/*****************************************************************************/
/* Function:		Image_dealloc		 */
/* Description: This is a callback function for the BPy_Image type. It is  */
/*		the destructor function.	 */
/*****************************************************************************/
static void Image_dealloc( BPy_Image * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:		Image_CreatePyObject	 */
/* Description: This function will create a new BPy_Image from an existing  */
/*		Blender image structure.	 */
/*****************************************************************************/
PyObject *Image_CreatePyObject( Image * image )
{
	BPy_Image *py_img;
	py_img = ( BPy_Image * ) PyObject_NEW( BPy_Image, &Image_Type );
	
	if( !py_img )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Image object" );
	
	py_img->image = image;
	return ( PyObject * ) py_img;
}

/*****************************************************************************/
/* Function:		Image_CheckPyObject	 */
/* Description: This function returns true when the given PyObject is of the */
/*		type Image. Otherwise it will return false.	 */
/*****************************************************************************/
int Image_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &Image_Type );
}

/*****************************************************************************/
/* Function:	Image_FromPyObject	 */
/* Description: Returns the Blender Image associated with this object  	 */
/*****************************************************************************/
Image *Image_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Image * ) pyobj )->image;
}

/*****************************************************************************/
/* Python BPy_Image methods:		 */
/*****************************************************************************/
static PyObject *Image_getName( BPy_Image * self )
{
	PyObject *attr = PyString_FromString( self->image->id.name + 2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Image.name attribute" ) );
}

static PyObject *Image_getFilename( BPy_Image * self )
{
	PyObject *attr = PyString_FromString( self->image->name );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Image.filename attribute" ) );
}

static PyObject *Image_getSize( BPy_Image * self )
{
	PyObject *attr;
	Image *image = self->image;
	
	
	if( !image->ibuf ) /* if no image data available */
		load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

	if( !image->ibuf )	/* didn't work */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't load image data in Blender" );

	attr = Py_BuildValue( "[hh]", image->ibuf->x, image->ibuf->y );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Image.size attribute" );
}

static PyObject *Image_getDepth( BPy_Image * self )
{
	PyObject *attr;
	Image *image = self->image;

	if( !image->ibuf )	/* if no image data available */
		load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

	if( !image->ibuf )	/* didn't work */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't load image data in Blender" );

	attr = Py_BuildValue( "h", image->ibuf->depth );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Image.depth attribute" );
}


static PyObject *Image_getXRep( BPy_Image * self )
{
	PyObject *attr = PyInt_FromLong( self->image->xrep );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Image.xrep attribute" );
}

static PyObject *Image_getYRep( BPy_Image * self )
{
	PyObject *attr = PyInt_FromLong( self->image->yrep );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Image.yrep attribute" );
}

static PyObject *Image_getStart( BPy_Image * self )
{
	PyObject *attr = PyInt_FromLong( self->image->twsta );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Image.start attribute" );
}

static PyObject *Image_getEnd( BPy_Image * self )
{
	PyObject *attr = PyInt_FromLong( self->image->twend );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Image.end attribute" );
}

static PyObject *Image_getSpeed( BPy_Image * self )
{
	PyObject *attr = PyInt_FromLong( self->image->animspeed );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Image.speed attribute" );
}

static PyObject *Image_getBindCode( BPy_Image * self )
{
	PyObject *attr = PyLong_FromUnsignedLong( self->image->bindcode );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Image.bindcode attribute" );
}

static PyObject *Image_reload( BPy_Image * self )
{
	Image *image = self->image;

	free_image_buffers( image );	/* force read again */
	image->ok = 1;
	Py_RETURN_NONE;
}

static PyObject *Image_glFree( BPy_Image * self )
{
	Image *image = self->image;

	free_realtime_image( image );
	/* remove the nocollect flag, image is available for garbage collection again */
	image->flag &= ~IMA_NOCOLLECT;
	Py_RETURN_NONE;
}

static PyObject *Image_glLoad( BPy_Image * self )
{
	Image *image = self->image;
	unsigned int *bind = &image->bindcode;

	if( *bind == 0 ) {

		if( !image->ibuf )	/* if no image data is available */
			load_image( image, IB_rect, G.sce,  G.scene->r.cfra );	/* loading it */

		if( !image->ibuf )	/* didn't work */
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "couldn't load image data in Blender" );

		glGenTextures( 1, ( GLuint * ) bind );
		glBindTexture( GL_TEXTURE_2D, *bind );

		gluBuild2DMipmaps( GL_TEXTURE_2D, GL_RGBA, image->ibuf->x,
				   image->ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE,
				   image->ibuf->rect );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				 GL_LINEAR_MIPMAP_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				 GL_LINEAR );
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, image->ibuf->x,
			      image->ibuf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
			      image->ibuf->rect );

		/* raise the nocollect flag, 
		   image is not available for garbage collection 
		   (python GL might use it directly)
		 */
		image->flag |= IMA_NOCOLLECT;
	}

	return PyLong_FromUnsignedLong( image->bindcode );
}

static PyObject *Image_setName( BPy_Image * self, PyObject * args )
{
	char *name;
	char buf[21];

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument" ) );

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	rename_id( &self->image->id, buf );

	Py_RETURN_NONE;
}

static PyObject *Image_setFilename( BPy_Image * self, PyObject * args )
{
	char *name;
	int namelen = 0;

	/* max len is FILE_MAXDIR = 160 chars like done in DNA_image_types.h */

	if( !PyArg_ParseTuple( args, "s#", &name, &namelen ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected a string argument" ) );

	if( namelen >= FILE_MAXDIR )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"string argument is limited to 160 chars at most" ) );

	PyOS_snprintf( self->image->name, FILE_MAXDIR * sizeof( char ), "%s",
		       name );

	Py_RETURN_NONE;
}

static PyObject *Image_setXRep( BPy_Image * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [1,16]" ) );

	if( value >= EXPP_IMAGE_REP_MIN && value <= EXPP_IMAGE_REP_MAX )
		self->image->xrep = value;
	else
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"expected int argument in [1,16]" ) );

	Py_RETURN_NONE;
}

static PyObject *Image_setYRep( BPy_Image * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [1,16]" ) );

	if( value >= EXPP_IMAGE_REP_MIN && value <= EXPP_IMAGE_REP_MAX )
		self->image->yrep = value;
	else
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"expected int argument in [1,16]" ) );

	Py_RETURN_NONE;
}


static PyObject *Image_setStart( BPy_Image * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [0,128]" ) );

	if( value >= 0 && value <= 128 )
		self->image->twsta = value;
	else
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"expected int argument in [0,128]" ) );

	Py_RETURN_NONE;
}


static PyObject *Image_setEnd( BPy_Image * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [0,128]" ) );

	if( value >= 0 && value <= 128 )
		self->image->twend = value;
	else
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"expected int argument in [0,128]" ) );

	Py_RETURN_NONE;
}

static PyObject *Image_setSpeed( BPy_Image * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [0,128]" ) );

	if( value >= 1 && value <= 100 )
		self->image->animspeed = value;
	else
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"expected int argument in [0,128]" ) );

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:		Image_getAttr		 */
/* Description: This is a callback function for the BPy_Image type. It is */
/*		the function that accesses BPy_Image member variables and   */
/*		methods.	 */
/*****************************************************************************/
static PyObject *Image_getAttr( BPy_Image * self, char *name )
{
	PyObject *attr = Py_None;

	if( strcmp( name, "name" ) == 0 )
		attr = PyString_FromString( self->image->id.name + 2 );
	else if( strcmp( name, "filename" ) == 0 )
		attr = PyString_FromString( self->image->name );
	else if( strcmp( name, "size" ) == 0 )
		return Image_getSize( self );
	else if( strcmp( name, "depth" ) == 0 )
		return Image_getDepth( self );
	else if( strcmp( name, "xrep" ) == 0 )
		attr = PyInt_FromLong( self->image->xrep );
	else if( strcmp( name, "yrep" ) == 0 )
		attr = PyInt_FromLong( self->image->yrep );
	else if( strcmp( name, "start" ) == 0 )
		attr = PyInt_FromLong( self->image->twsta );
	else if( strcmp( name, "end" ) == 0 )
		attr = PyInt_FromLong( self->image->twend );
	else if( strcmp( name, "speed" ) == 0 )
		attr = PyInt_FromLong( self->image->animspeed );
	else if( strcmp( name, "packed" ) == 0 ) {
		if (self->image->packedfile)
			attr = EXPP_incr_ret_True();
		else
			attr = EXPP_incr_ret_False();
	
	} else if( strcmp( name, "bindcode" ) == 0 )
		attr = PyInt_FromLong( self->image->bindcode );
	else if( strcmp( name, "users" ) == 0 )
		attr = PyInt_FromLong( self->image->id.us );
	else if( strcmp( name, "__members__" ) == 0 )
		attr = Py_BuildValue( "[s,s,s,s,s,s,s,s,s,s,s]",
				      "name", "filename", "size", "depth",
				      "xrep", "yrep", "start", "end",
				      "speed", "packed",
				      "bindcode", "users" );

	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject" ) );

	if( attr != Py_None )
		return attr;	/* attribute found, return its value */

	/* not an attribute, search the methods table */
	return Py_FindMethod( BPy_Image_methods, ( PyObject * ) self, name );
}

/*****************************************************************************/
/* Function:		Image_setAttr		 */
/* Description: This is a callback function for the BPy_Image type. It is the*/
/*		function that changes Image object members values. If this  */
/*		data is linked to a Blender Image, it also gets updated.  */
/*****************************************************************************/
static int Image_setAttr( BPy_Image * self, char *name, PyObject * value )
{
	PyObject *valtuple;
	PyObject *error = NULL;

/* We're playing a trick on the Python API users here.	Even if they use
 * Image.member = val instead of Image.setMember(value), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Image structure when necessary. */

	valtuple = Py_BuildValue( "(O)", value );	/*the set* functions expect a tuple */

	if( !valtuple )
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "ImageSetAttr: couldn't create PyTuple" );

	if( strcmp( name, "name" ) == 0 )
		error = Image_setName( self, valtuple );
	if( strcmp( name, "filename" ) == 0 )
		error = Image_setFilename( self, valtuple );
	else if( strcmp( name, "xrep" ) == 0 )
		error = Image_setXRep( self, valtuple );
	else if( strcmp( name, "yrep" ) == 0 )
		error = Image_setYRep( self, valtuple );
	else if( strcmp( name, "start" ) == 0 )
		error = Image_setStart( self, valtuple );
	else if( strcmp( name, "end" ) == 0 )
		error = Image_setEnd( self, valtuple );
	else if( strcmp( name, "speed" ) == 0 )
		error = Image_setSpeed( self, valtuple );
	else {			/* Error: no such member in the Image object structure */
		/*Py_DECREF( value ); borrowed ref, no need to decref */
		Py_DECREF( valtuple );
		return ( EXPP_ReturnIntError( PyExc_KeyError,
					      "attribute not found or immutable" ) );
	}

	Py_DECREF( valtuple );

	if( error != Py_None )
		return -1;

	Py_DECREF( Py_None );	/* incref'ed by the called set* function */
	return 0;		/* normal exit */
}

/*****************************************************************************/
/* Function:	Image_compare			 */
/* Description: This is a callback function for the BPy_Image type. It	 */
/*		compares two Image_Type objects. Only the "==" and "!="	 */
/*		comparisons are meaninful. Returns 0 for equality and -1 if  */
/*		they don't point to the same Blender Image struct.	 */
/*		In Python it becomes 1 if they are equal, 0 otherwise.	 */
/*****************************************************************************/
static int Image_compare( BPy_Image * a, BPy_Image * b )
{
	Image *pa = a->image, *pb = b->image;
	return ( pa == pb ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:	Image_repr	 */
/* Description: This is a callback function for the BPy_Image type. It	 */
/*		builds a meaninful string to represent image objects.	 */
/*****************************************************************************/
static PyObject *Image_repr( BPy_Image * self )
{
	return PyString_FromFormat( "[Image \"%s\"]",
				    self->image->id.name + 2 );
}
