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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Nathan Letwory, Stephen Swaney,
 * Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include "Lamp.h" /*This must come first*/

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_library.h"
#include "BKE_texture.h"
#include "BLI_blenlib.h"
#include "BIF_keyframing.h"
#include "BIF_space.h"
#include "BSE_editipo.h"
#include "mydevice.h"
#include "Ipo.h"
#include "MTex.h" 
#include "constant.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "BKE_utildefines.h"
#include "DNA_userdef_types.h"
#include "MEM_guardedalloc.h"

/*****************************************************************************/
/* Python BPy_Lamp defaults:                                                 */
/*****************************************************************************/

/* Lamp types */

/* NOTE:
 these are the same values as LA_* from DNA_lamp_types.h
 is there some reason we are not simply using those #defines?
 s. swaney 8-oct-2004
*/

#define EXPP_LAMP_TYPE_LAMP 0
#define EXPP_LAMP_TYPE_SUN  1
#define EXPP_LAMP_TYPE_SPOT 2
#define EXPP_LAMP_TYPE_HEMI 3
#define EXPP_LAMP_TYPE_AREA 4
#define EXPP_LAMP_TYPE_YF_PHOTON 5
/*
  define a constant to keep magic numbers out of the code
  this value should be equal to the last EXPP_LAMP_TYPE_*
*/
#define EXPP_LAMP_TYPE_MAX  5

/* Lamp mode flags */

#define EXPP_LAMP_MODE_SHADOWS       1
#define EXPP_LAMP_MODE_HALO          2
#define EXPP_LAMP_MODE_LAYER         4
#define EXPP_LAMP_MODE_QUAD          8
#define EXPP_LAMP_MODE_NEGATIVE     16
#define EXPP_LAMP_MODE_ONLYSHADOW   32
#define EXPP_LAMP_MODE_SPHERE       64
#define EXPP_LAMP_MODE_SQUARE      128
#define EXPP_LAMP_MODE_TEXTURE     256
#define EXPP_LAMP_MODE_OSATEX      512
#define EXPP_LAMP_MODE_DEEPSHADOW 1024
#define EXPP_LAMP_MODE_NODIFFUSE  2048
#define EXPP_LAMP_MODE_NOSPECULAR 4096
#define EXPP_LAMP_MODE_SHAD_RAY	  8192
#define EXPP_LAMP_MODE_LAYER_SHADOW 32768

/* Lamp MIN, MAX values */

#define EXPP_LAMP_SAMPLES_MIN 1
#define EXPP_LAMP_SAMPLES_MAX 16
#define EXPP_LAMP_BUFFERSIZE_MIN 512
#define EXPP_LAMP_BUFFERSIZE_MAX 5120
#define EXPP_LAMP_ENERGY_MIN  0.0
#define EXPP_LAMP_ENERGY_MAX 10.0
#define EXPP_LAMP_DIST_MIN    0.1f
#define EXPP_LAMP_DIST_MAX 5000.0
#define EXPP_LAMP_SPOTSIZE_MIN   1.0
#define EXPP_LAMP_SPOTSIZE_MAX 180.0
#define EXPP_LAMP_SPOTBLEND_MIN 0.00
#define EXPP_LAMP_SPOTBLEND_MAX 1.00
#define EXPP_LAMP_CLIPSTART_MIN    0.1f
#define EXPP_LAMP_CLIPSTART_MAX 1000.0
#define EXPP_LAMP_CLIPEND_MIN    1.0
#define EXPP_LAMP_CLIPEND_MAX 5000.0
#define EXPP_LAMP_BIAS_MIN 0.01f
#define EXPP_LAMP_BIAS_MAX 5.00
#define EXPP_LAMP_SOFTNESS_MIN   1.0
#define EXPP_LAMP_SOFTNESS_MAX 100.0
#define EXPP_LAMP_HALOINT_MIN 0.0
#define EXPP_LAMP_HALOINT_MAX 5.0
#define EXPP_LAMP_HALOSTEP_MIN  0
#define EXPP_LAMP_HALOSTEP_MAX 12
#define EXPP_LAMP_QUAD1_MIN 0.0
#define EXPP_LAMP_QUAD1_MAX 1.0
#define EXPP_LAMP_QUAD2_MIN 0.0
#define EXPP_LAMP_QUAD2_MAX 1.0
#define EXPP_LAMP_COL_MIN 0.0
#define EXPP_LAMP_COL_MAX 1.0
#define EXPP_LAMP_FALLOFF_MIN LA_FALLOFF_CONSTANT
#define EXPP_LAMP_FALLOFF_MAX LA_FALLOFF_SLIDERS

/* Raytracing settings */
#define EXPP_LAMP_RAYSAMPLES_MIN 1
#define EXPP_LAMP_RAYSAMPLES_MAX 16
#define EXPP_LAMP_AREASIZE_MIN 0.01f
#define EXPP_LAMP_AREASIZE_MAX 100.0f

/* Lamp_setComponent() keys for which color to get/set */
#define	EXPP_LAMP_COMP_R			0x00
#define	EXPP_LAMP_COMP_G			0x01
#define	EXPP_LAMP_COMP_B			0x02

#define IPOKEY_RGB       0
#define IPOKEY_ENERGY    1
#define IPOKEY_SPOTSIZE  2
#define IPOKEY_OFFSET    3
#define IPOKEY_SIZE      4

/*****************************************************************************/
/* Python API function prototypes for the Lamp module.                       */
/*****************************************************************************/
static PyObject *M_Lamp_New( PyObject * self, PyObject * args,
			     PyObject * keywords );
static PyObject *M_Lamp_Get( PyObject * self, PyObject * args );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Lamp.__doc__                                                      */
/*****************************************************************************/
static char M_Lamp_doc[] = "The Blender Lamp module\n\n\
This module provides control over **Lamp Data** objects in Blender.\n\n\
Example::\n\n\
  from Blender import Lamp\n\
  l = Lamp.New('Spot')            # create new 'Spot' lamp data\n\
  l.setMode('square', 'shadow')   # set these two lamp mode flags\n\
  ob = Object.New('Lamp')         # create new lamp object\n\
  ob.link(l)                      # link lamp obj with lamp data\n";

static char M_Lamp_New_doc[] = "Lamp.New (type = 'Lamp', name = 'LampData'):\n\
        Return a new Lamp Data object with the given type and name.";

static char M_Lamp_Get_doc[] = "Lamp.Get (name = None):\n\
        Return the Lamp Data with the given name, None if not found, or\n\
        Return a list with all Lamp Data objects in the current scene,\n\
        if no argument was given.";

/*****************************************************************************/
/* Python method structure definition for Blender.Lamp module:               */
/*****************************************************************************/
struct PyMethodDef M_Lamp_methods[] = {
	{"New", ( PyCFunction ) M_Lamp_New, METH_VARARGS | METH_KEYWORDS,
	 M_Lamp_New_doc},
	{"Get", M_Lamp_Get, METH_VARARGS, M_Lamp_Get_doc},
	{"get", M_Lamp_Get, METH_VARARGS, M_Lamp_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Lamp methods declarations:                                     */
/*****************************************************************************/
static PyObject *Lamp_getType( BPy_Lamp * self );
static PyObject *Lamp_getTypesConst( void );
static PyObject *Lamp_getMode( BPy_Lamp * self );
static PyObject *Lamp_getModesConst( void );
static PyObject *Lamp_getSamples( BPy_Lamp * self );
static PyObject *Lamp_getRaySamplesX( BPy_Lamp * self );
static PyObject *Lamp_getRaySamplesY( BPy_Lamp * self );
static PyObject *Lamp_getAreaSizeX( BPy_Lamp * self );
static PyObject *Lamp_getAreaSizeY( BPy_Lamp * self );
static PyObject *Lamp_getBufferSize( BPy_Lamp * self );
static PyObject *Lamp_getHaloStep( BPy_Lamp * self );
static PyObject *Lamp_getEnergy( BPy_Lamp * self );
static PyObject *Lamp_getDist( BPy_Lamp * self );
static PyObject *Lamp_getSpotSize( BPy_Lamp * self );
static PyObject *Lamp_getSpotBlend( BPy_Lamp * self );
static PyObject *Lamp_getClipStart( BPy_Lamp * self );
static PyObject *Lamp_getClipEnd( BPy_Lamp * self );
static PyObject *Lamp_getBias( BPy_Lamp * self );
static PyObject *Lamp_getSoftness( BPy_Lamp * self );
static PyObject *Lamp_getHaloInt( BPy_Lamp * self );
static PyObject *Lamp_getQuad1( BPy_Lamp * self );
static PyObject *Lamp_getQuad2( BPy_Lamp * self );
static PyObject *Lamp_getCol( BPy_Lamp * self );
static PyObject *Lamp_getIpo( BPy_Lamp * self );
static PyObject *Lamp_getComponent( BPy_Lamp * self, void * closure );
static PyObject *Lamp_getTextures( BPy_Lamp * self );
static PyObject *Lamp_clearIpo( BPy_Lamp * self );
static PyObject *Lamp_insertIpoKey( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetIpo( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetType( BPy_Lamp * self, PyObject * value );
static PyObject *Lamp_oldsetMode( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetSamples( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetRaySamplesX( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetRaySamplesY( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetAreaSizeX( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetAreaSizeY( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetBufferSize( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetHaloStep( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetEnergy( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetDist( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetSpotSize( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetSpotBlend( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetClipStart( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetClipEnd( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetBias( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetSoftness( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetHaloInt( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetQuad1( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetQuad2( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_oldsetCol( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_copy( BPy_Lamp * self );
static int Lamp_setIpo( BPy_Lamp * self, PyObject * args );
static int Lamp_setType( BPy_Lamp * self, PyObject * args );
static int Lamp_setMode( BPy_Lamp * self, PyObject * args );
static int Lamp_setSamples( BPy_Lamp * self, PyObject * args );
static int Lamp_setRaySamplesX( BPy_Lamp * self, PyObject * args );
static int Lamp_setRaySamplesY( BPy_Lamp * self, PyObject * args );
static int Lamp_setAreaSizeX( BPy_Lamp * self, PyObject * args );
static int Lamp_setAreaSizeY( BPy_Lamp * self, PyObject * args );
static int Lamp_setBufferSize( BPy_Lamp * self, PyObject * args );
static int Lamp_setHaloStep( BPy_Lamp * self, PyObject * args );
static int Lamp_setEnergy( BPy_Lamp * self, PyObject * args );
static int Lamp_setDist( BPy_Lamp * self, PyObject * args );
static int Lamp_setSpotSize( BPy_Lamp * self, PyObject * args );
static int Lamp_setSpotBlend( BPy_Lamp * self, PyObject * args );
static int Lamp_setClipStart( BPy_Lamp * self, PyObject * args );
static int Lamp_setClipEnd( BPy_Lamp * self, PyObject * args );
static int Lamp_setBias( BPy_Lamp * self, PyObject * args );
static int Lamp_setSoftness( BPy_Lamp * self, PyObject * args );
static int Lamp_setHaloInt( BPy_Lamp * self, PyObject * args );
static int Lamp_setQuad1( BPy_Lamp * self, PyObject * args );
static int Lamp_setQuad2( BPy_Lamp * self, PyObject * args );
static int Lamp_setCol( BPy_Lamp * self, PyObject * args );
static int Lamp_setTextures( BPy_Lamp * self, PyObject * value );
static PyObject *Lamp_getScriptLinks( BPy_Lamp * self, PyObject * value );
static PyObject *Lamp_addScriptLink( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_clearScriptLinks( BPy_Lamp * self, PyObject * args );
static int Lamp_setComponent( BPy_Lamp * self, PyObject * value, void * closure );
static PyObject *Lamp_getFalloffType( BPy_Lamp * self );
static int Lamp_setFalloffType( BPy_Lamp * self, PyObject * value );

/*****************************************************************************/
/* Python BPy_Lamp methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_Lamp_methods[] = {
	/* name, method, flags, doc */
	
	{"getType", ( PyCFunction ) Lamp_getType, METH_NOARGS,
	 "() - return Lamp type - 'Lamp':0, 'Sun':1, 'Spot':2, 'Hemi':3, 'Area':4, 'Photon':5"},
	{"getMode", ( PyCFunction ) Lamp_getMode, METH_NOARGS,
	 "() - return Lamp mode flags (or'ed value)"},
	{"getSamples", ( PyCFunction ) Lamp_getSamples, METH_NOARGS,
	 "() - return Lamp samples value"},
	{"getRaySamplesX", ( PyCFunction ) Lamp_getRaySamplesX, METH_NOARGS,
	 "() - return Lamp raytracing samples on the X axis"},
	{"getRaySamplesY", ( PyCFunction ) Lamp_getRaySamplesY, METH_NOARGS,
	 "() - return Lamp raytracing samples on the Y axis"},
	{"getAreaSizeX", ( PyCFunction ) Lamp_getAreaSizeX, METH_NOARGS,
	 "() - return Lamp area size on the X axis"},
	{"getAreaSizeY", ( PyCFunction ) Lamp_getAreaSizeY, METH_NOARGS,
	 "() - return Lamp area size on the Y axis"},
	{"getBufferSize", ( PyCFunction ) Lamp_getBufferSize, METH_NOARGS,
	 "() - return Lamp buffer size value"},
	{"getHaloStep", ( PyCFunction ) Lamp_getHaloStep, METH_NOARGS,
	 "() - return Lamp halo step value"},
	{"getEnergy", ( PyCFunction ) Lamp_getEnergy, METH_NOARGS,
	 "() - return Lamp energy value"},
	{"getDist", ( PyCFunction ) Lamp_getDist, METH_NOARGS,
	 "() - return Lamp clipping distance value"},
	{"getSpotSize", ( PyCFunction ) Lamp_getSpotSize, METH_NOARGS,
	 "() - return Lamp spot size value"},
	{"getSpotBlend", ( PyCFunction ) Lamp_getSpotBlend, METH_NOARGS,
	 "() - return Lamp spot blend value"},
	{"getClipStart", ( PyCFunction ) Lamp_getClipStart, METH_NOARGS,
	 "() - return Lamp clip start value"},
	{"getClipEnd", ( PyCFunction ) Lamp_getClipEnd, METH_NOARGS,
	 "() - return Lamp clip end value"},
	{"getBias", ( PyCFunction ) Lamp_getBias, METH_NOARGS,
	 "() - return Lamp bias value"},
	{"getSoftness", ( PyCFunction ) Lamp_getSoftness, METH_NOARGS,
	 "() - return Lamp softness value"},
	{"getHaloInt", ( PyCFunction ) Lamp_getHaloInt, METH_NOARGS,
	 "() - return Lamp halo intensity value"},
	{"getQuad1", ( PyCFunction ) Lamp_getQuad1, METH_NOARGS,
	 "() - return light intensity value #1 for a Quad Lamp"},
	{"getQuad2", ( PyCFunction ) Lamp_getQuad2, METH_NOARGS,
	 "() - return light intensity value #2 for a Quad Lamp"},
	{"getCol", ( PyCFunction ) Lamp_getCol, METH_NOARGS,
	 "() - return light rgb color triplet"},
	{"setName", ( PyCFunction ) GenericLib_setName_with_method, METH_VARARGS,
	 "(str) - rename Lamp"},
	{"setType", ( PyCFunction ) Lamp_oldsetType, METH_O,
	 "(str) - change Lamp type, which can be 'Lamp', 'Sun', 'Spot', 'Hemi', 'Area', 'Photon'"},
	{"setMode", ( PyCFunction ) Lamp_oldsetMode, METH_VARARGS,
	 "([up to eight str's]) - Set Lamp mode flag(s)"},
	{"setSamples", ( PyCFunction ) Lamp_oldsetSamples, METH_VARARGS,
	 "(int) - change Lamp samples value"},
	{"setRaySamplesX", ( PyCFunction ) Lamp_oldsetRaySamplesX, METH_VARARGS,
	 "(int) - change Lamp ray X samples value in [1,16]"},
	{"setRaySamplesY", ( PyCFunction ) Lamp_oldsetRaySamplesY, METH_VARARGS,
	 "(int) - change Lamp ray Y samples value in [1,16]"},
	{"setAreaSizeX", ( PyCFunction ) Lamp_oldsetAreaSizeX, METH_VARARGS,
	 "(float) - change Lamp ray X size for area lamps, value in [0.01, 100.0]"},
	{"setAreaSizeY", ( PyCFunction ) Lamp_oldsetAreaSizeY, METH_VARARGS,
	 "(float) - change Lamp ray Y size for area lamps, value in [0.01, 100.0]"},
	{"setBufferSize", ( PyCFunction ) Lamp_oldsetBufferSize, METH_VARARGS,
	 "(int) - change Lamp buffer size value"},
	{"setHaloStep", ( PyCFunction ) Lamp_oldsetHaloStep, METH_VARARGS,
	 "(int) - change Lamp halo step value"},
	{"setEnergy", ( PyCFunction ) Lamp_oldsetEnergy, METH_VARARGS,
	 "(float) - change Lamp energy value"},
	{"setDist", ( PyCFunction ) Lamp_oldsetDist, METH_VARARGS,
	 "(float) - change Lamp clipping distance value"},
	{"setSpotSize", ( PyCFunction ) Lamp_oldsetSpotSize, METH_VARARGS,
	 "(float) - change Lamp spot size value"},
	{"setSpotBlend", ( PyCFunction ) Lamp_oldsetSpotBlend, METH_VARARGS,
	 "(float) - change Lamp spot blend value"},
	{"setClipStart", ( PyCFunction ) Lamp_oldsetClipStart, METH_VARARGS,
	 "(float) - change Lamp clip start value"},
	{"setClipEnd", ( PyCFunction ) Lamp_oldsetClipEnd, METH_VARARGS,
	 "(float) - change Lamp clip end value"},
	{"setBias", ( PyCFunction ) Lamp_oldsetBias, METH_VARARGS,
	 "(float) - change Lamp draw size value"},
	{"setSoftness", ( PyCFunction ) Lamp_oldsetSoftness, METH_VARARGS,
	 "(float) - change Lamp softness value"},
	{"setHaloInt", ( PyCFunction ) Lamp_oldsetHaloInt, METH_VARARGS,
	 "(float) - change Lamp halo intensity value"},
	{"setQuad1", ( PyCFunction ) Lamp_oldsetQuad1, METH_VARARGS,
	 "(float) - change light intensity value #1 for a Quad Lamp"},
	{"setQuad2", ( PyCFunction ) Lamp_oldsetQuad2, METH_VARARGS,
	 "(float) - change light intensity value #2 for a Quad Lamp"},
	{"setCol", ( PyCFunction ) Lamp_oldsetCol, METH_VARARGS,
	 "(f,f,f) or ([f,f,f]) - change light's rgb color triplet"},
	{"getScriptLinks", ( PyCFunction ) Lamp_getScriptLinks, METH_O,
	 "(eventname) - Get a list of this lamp's scriptlinks (Text names) "
	 "of the given type\n"
	 "(eventname) - string: FrameChanged, Redraw or Render."},
	{"addScriptLink", ( PyCFunction ) Lamp_addScriptLink, METH_VARARGS,
	 "(text, evt) - Add a new lamp scriptlink.\n"
	 "(text) - string: an existing Blender Text name;\n"
	 "(evt) string: FrameChanged, Redraw or Render."},
	{"clearScriptLinks", ( PyCFunction ) Lamp_clearScriptLinks,
	 METH_VARARGS,
	 "() - Delete all scriptlinks from this lamp.\n"
	 "([s1<,s2,s3...>]) - Delete specified scriptlinks from this lamp."},
	{"getIpo", ( PyCFunction ) Lamp_getIpo, METH_NOARGS,
	 "() - get IPO for this lamp"},
	{"clearIpo", ( PyCFunction ) Lamp_clearIpo, METH_NOARGS,
	 "() - unlink the IPO for this lamp"},
	{"setIpo", ( PyCFunction ) Lamp_oldsetIpo, METH_VARARGS,
	 "( lamp-ipo ) - link an IPO to this lamp"},
	 {"insertIpoKey", ( PyCFunction ) Lamp_insertIpoKey, METH_VARARGS,
	 "( Lamp IPO type ) - Inserts a key into IPO"},
	{"__copy__", ( PyCFunction ) Lamp_copy, METH_NOARGS,
	 "() - Makes a copy of this lamp."},
	{"copy", ( PyCFunction ) Lamp_copy, METH_NOARGS,
	 "() - Makes a copy of this lamp."},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_Lamp_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"bias",
	 (getter)Lamp_getBias, (setter)Lamp_setBias,
	 "Lamp shadow map sampling bias",
	 NULL},
	{"bufferSize",
	 (getter)Lamp_getBufferSize, (setter)Lamp_setBufferSize,
	 "Lamp shadow buffer size",
	 NULL},
	{"clipEnd",
	 (getter)Lamp_getClipEnd, (setter)Lamp_setClipEnd,
	 "Lamp shadow map clip end",
	 NULL},
	{"clipStart",
	 (getter)Lamp_getClipStart, (setter)Lamp_setClipStart,
	 "Lamp shadow map clip start",
	 NULL},
	{"col",
	 (getter)Lamp_getCol, (setter)Lamp_setCol,
	 "Lamp RGB color triplet",
	 NULL},
	{"dist",
	 (getter)Lamp_getDist, (setter)Lamp_setDist,
	 "Lamp clipping distance",
	 NULL},
	{"energy",
	 (getter)Lamp_getEnergy, (setter)Lamp_setEnergy,
	 "Lamp light intensity",
	 NULL},
	{"haloInt",
	 (getter)Lamp_getHaloInt, (setter)Lamp_setHaloInt,
	 "Lamp spotlight halo intensity",
	 NULL},
	{"haloStep",
	 (getter)Lamp_getHaloStep, (setter)Lamp_setHaloStep,
	 "Lamp volumetric halo sampling frequency",
	 NULL},
	{"ipo",
	 (getter)Lamp_getIpo, (setter)Lamp_setIpo,
	 "Lamp Ipo",
	 NULL},
	{"mode",
	 (getter)Lamp_getMode, (setter)Lamp_setMode,
	 "Lamp mode bitmask",
	 NULL},
	{"quad1",
	 (getter)Lamp_getQuad1, (setter)Lamp_setQuad1,
	 "Quad lamp linear distance attenuation",
	 NULL},
	{"quad2",
	 (getter)Lamp_getQuad2, (setter)Lamp_setQuad2,
	 "Quad lamp quadratic distance attenuation",
	 NULL},
	{"samples",
	 (getter)Lamp_getSamples, (setter)Lamp_setSamples,
	 "Lamp shadow map samples",
	 NULL},
	{"raySamplesX",
	 (getter)Lamp_getRaySamplesX, (setter)Lamp_setRaySamplesX,
	 "Lamp raytracing samples on the X axis",
	 NULL},
	{"raySamplesY",
	 (getter)Lamp_getRaySamplesY, (setter)Lamp_setRaySamplesY,
	 "Lamp raytracing samples on the Y axis",
	 NULL},
	{"areaSizeX",
	 (getter)Lamp_getAreaSizeX, (setter)Lamp_setAreaSizeX,
	 "Lamp X size for an arealamp",
	 NULL},
	{"areaSizeY",
	 (getter)Lamp_getAreaSizeY, (setter)Lamp_setAreaSizeY,
	 "Lamp Y size for an arealamp",
	 NULL},
	{"softness",
	 (getter)Lamp_getSoftness, (setter)Lamp_setSoftness,
	 "Lamp shadow sample area size",
	 NULL},
	{"spotBlend",
	 (getter)Lamp_getSpotBlend, (setter)Lamp_setSpotBlend,
	 "Lamp spotlight edge softness",
	 NULL},
	{"spotSize",
	 (getter)Lamp_getSpotSize, (setter)Lamp_setSpotSize,
	 "Lamp spotlight beam angle (in degrees)",
	 NULL},
	{"type",
	 (getter)Lamp_getType, (setter)Lamp_setType,
	 "Lamp type",
	 NULL},
	{"falloffType",
	 (getter)Lamp_getFalloffType, (setter)Lamp_setFalloffType,
	 "Lamp falloff type",
	 NULL},
	{"R",
	 (getter)Lamp_getComponent, (setter)Lamp_setComponent,
	 "Lamp color red component",
	 (void *)EXPP_LAMP_COMP_R},
	{"r",
	 (getter)Lamp_getComponent, (setter)Lamp_setComponent,
	 "Lamp color red component",
	 (void *)EXPP_LAMP_COMP_R},
	{"G",
	 (getter)Lamp_getComponent, (setter)Lamp_setComponent,
	 "Lamp color green component",
	 (void *)EXPP_LAMP_COMP_G},
	{"g",
	 (getter)Lamp_getComponent, (setter)Lamp_setComponent,
	 "Lamp color green component",
	 (void *)EXPP_LAMP_COMP_G},
	{"B",
	 (getter)Lamp_getComponent, (setter)Lamp_setComponent,
	 "Lamp color blue component",
	 (void *)EXPP_LAMP_COMP_B},
	{"b",
	 (getter)Lamp_getComponent, (setter)Lamp_setComponent,
	 "Lamp color blue component",
	 (void *)EXPP_LAMP_COMP_B},
	{"textures",
	 (getter)Lamp_getTextures, (setter)Lamp_setTextures,
     "The Lamp's texture list as a tuple",
	 NULL},
	{"Modes",
	 (getter)Lamp_getModesConst, (setter)NULL,
	 "Dictionary of values for 'mode' attribute",
	 NULL},
	{"Types",
	 (getter)Lamp_getTypesConst, (setter)NULL,
	 "Dictionary of values for 'type' attribute",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python TypeLamp callback function prototypes:                             */
/*****************************************************************************/
static void Lamp_dealloc( BPy_Lamp * lamp );
static int Lamp_compare( BPy_Lamp * a, BPy_Lamp * b );
static PyObject *Lamp_repr( BPy_Lamp * lamp );

/*****************************************************************************/
/* Python TypeLamp structure definition:                                     */
/*****************************************************************************/
PyTypeObject Lamp_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Lamp",             /* char *tp_name; */
	sizeof( BPy_Lamp ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) Lamp_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Lamp_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) Lamp_repr,     /* reprfunc tp_repr; */

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
	BPy_Lamp_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Lamp_getseters,         /* struct PyGetSetDef *tp_getset; */
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
/* Function:              M_Lamp_New                                         */
/* Python equivalent:     Blender.Lamp.New                                   */
/*****************************************************************************/
static PyObject *M_Lamp_New( PyObject * self, PyObject * args,
			     PyObject * keywords )
{
	char *type_str = "Lamp";
	char *name_str = "Lamp";
	static char *kwlist[] = { "type_str", "name_str", NULL };
	BPy_Lamp *py_lamp;	/* for Lamp Data object wrapper in Python */
	Lamp *bl_lamp;		/* for actual Lamp Data we create in Blender */

	if( !PyArg_ParseTupleAndKeywords( args, keywords, "|ss", kwlist,
					  &type_str, &name_str ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string(s) or empty argument" ) );
	
	bl_lamp = add_lamp( name_str );	/* first create in Blender */
	
	if( bl_lamp )		/* now create the wrapper obj in Python */
		py_lamp = ( BPy_Lamp * ) Lamp_CreatePyObject( bl_lamp );
	else
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create Lamp Data in Blender" ) );

	/* let's return user count to zero, because ... */
	bl_lamp->id.us = 0;	/* ... add_lamp() incref'ed it */

	if( py_lamp == NULL )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create Lamp Data object" ) );

	if( strcmp( type_str, "Lamp" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_LAMP;
	else if( strcmp( type_str, "Sun" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_SUN;
	else if( strcmp( type_str, "Spot" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_SPOT;
	else if( strcmp( type_str, "Hemi" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_HEMI;
	else if( strcmp( type_str, "Area" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_AREA;
	else if( strcmp( type_str, "Photon" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_YF_PHOTON;
	else
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"unknown lamp type" ) );

	return ( PyObject * ) py_lamp;
}

/*****************************************************************************/
/* Function:              M_Lamp_Get                                         */
/* Python equivalent:     Blender.Lamp.Get                                   */
/* Description:           Receives a string and returns the lamp data obj    */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all lamp data names in the    */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_Lamp_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Lamp *lamp_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	lamp_iter = G.main->lamp.first;

	if( name ) {		/* (name) - Search lamp by name */

		BPy_Lamp *wanted_lamp = NULL;

		while( ( lamp_iter ) && ( wanted_lamp == NULL ) ) {

			if( strcmp( name, lamp_iter->id.name + 2 ) == 0 )
				wanted_lamp =
					( BPy_Lamp * )
					Lamp_CreatePyObject( lamp_iter );

			lamp_iter = lamp_iter->id.next;
		}

		if( wanted_lamp == NULL ) { /* Requested lamp doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Lamp \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) wanted_lamp;
	}

	else {		/* () - return a list of all lamps in the scene */
		int index = 0;
		PyObject *lamplist, *pyobj;

		lamplist = PyList_New( BLI_countlist( &( G.main->lamp ) ) );

		if( lamplist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( lamp_iter ) {
			pyobj = Lamp_CreatePyObject( lamp_iter );

			if( !pyobj ) {
				Py_DECREF(lamplist);
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create PyLamp" ) );
			}

			PyList_SET_ITEM( lamplist, index, pyobj );

			lamp_iter = lamp_iter->id.next;
			index++;
		}

		return lamplist;
	}
}

static PyObject *Lamp_TypesDict( void )
{	/* create the Blender.Lamp.Types constant dict */
	PyObject *Types = PyConstant_New(  );

	if( Types ) {
		BPy_constant *c = ( BPy_constant * ) Types;

		PyConstant_Insert( c, "Lamp",
				 PyInt_FromLong( EXPP_LAMP_TYPE_LAMP ) );
		PyConstant_Insert( c, "Sun",
				 PyInt_FromLong( EXPP_LAMP_TYPE_SUN ) );
		PyConstant_Insert( c, "Spot",
				 PyInt_FromLong( EXPP_LAMP_TYPE_SPOT ) );
		PyConstant_Insert( c, "Hemi",
				 PyInt_FromLong( EXPP_LAMP_TYPE_HEMI ) );
		PyConstant_Insert( c, "Area",
				 PyInt_FromLong( EXPP_LAMP_TYPE_AREA ) );
		PyConstant_Insert( c, "Photon",
				 PyInt_FromLong( EXPP_LAMP_TYPE_YF_PHOTON ) );
	}

	return Types;
}

static PyObject *Lamp_ModesDict( void )
{			/* create the Blender.Lamp.Modes constant dict */
	PyObject *Modes = PyConstant_New(  );

	if( Modes ) {
		BPy_constant *c = ( BPy_constant * ) Modes;

		PyConstant_Insert( c, "Shadows",
				 PyInt_FromLong( EXPP_LAMP_MODE_SHADOWS ) );
		PyConstant_Insert( c, "Halo",
				 PyInt_FromLong( EXPP_LAMP_MODE_HALO ) );
		PyConstant_Insert( c, "Layer",
				 PyInt_FromLong( EXPP_LAMP_MODE_LAYER ) );
		PyConstant_Insert( c, "Quad",
				 PyInt_FromLong( EXPP_LAMP_MODE_QUAD ) );
		PyConstant_Insert( c, "Negative",
				 PyInt_FromLong( EXPP_LAMP_MODE_NEGATIVE ) );
		PyConstant_Insert( c, "Sphere",
				 PyInt_FromLong( EXPP_LAMP_MODE_SPHERE ) );
		PyConstant_Insert( c, "Square",
				 PyInt_FromLong( EXPP_LAMP_MODE_SQUARE ) );
		PyConstant_Insert( c, "OnlyShadow",
				 PyInt_FromLong( EXPP_LAMP_MODE_ONLYSHADOW ) );
		PyConstant_Insert( c, "NoDiffuse",
				 PyInt_FromLong( EXPP_LAMP_MODE_NODIFFUSE ) );
		PyConstant_Insert( c, "NoSpecular",
				 PyInt_FromLong( EXPP_LAMP_MODE_NOSPECULAR ) );
		PyConstant_Insert( c, "RayShadow",
				 PyInt_FromLong( EXPP_LAMP_MODE_SHAD_RAY ) );
		PyConstant_Insert( c, "LayerShadow",
				 PyInt_FromLong( EXPP_LAMP_MODE_LAYER_SHADOW ) );
	}

	return Modes;
}

static PyObject *Lamp_FalloffsDict( void )
{			/* create the Blender.Lamp.Modes constant dict */
	PyObject *Falloffs = PyConstant_New(  );

	if( Falloffs ) {
		BPy_constant *c = ( BPy_constant * ) Falloffs;

		PyConstant_Insert( c, "CONSTANT",
				 PyInt_FromLong( LA_FALLOFF_CONSTANT ) );
		PyConstant_Insert( c, "INVLINEAR",
				 PyInt_FromLong( LA_FALLOFF_INVLINEAR ) );
		PyConstant_Insert( c, "INVSQUARE",
				 PyInt_FromLong( LA_FALLOFF_INVSQUARE ) );
		PyConstant_Insert( c, "CUSTOM",
				 PyInt_FromLong( LA_FALLOFF_CURVE ) );
		PyConstant_Insert( c, "LINQUAD",
				 PyInt_FromLong( LA_FALLOFF_SLIDERS ) );
	}

	return Falloffs;
}

/*****************************************************************************/
/* Function:              Lamp_Init                                          */
/*****************************************************************************/
/* Needed by the Blender module, to register the Blender.Lamp submodule */
PyObject *Lamp_Init( void )
{
	PyObject *submodule, *Types, *Modes, *Falloffs;

	if( PyType_Ready( &Lamp_Type ) < 0)
		return NULL;

	Types = Lamp_TypesDict(  );
	Modes = Lamp_ModesDict(  );
	Falloffs = Lamp_FalloffsDict(  );

	submodule =
		Py_InitModule3( "Blender.Lamp", M_Lamp_methods, M_Lamp_doc );

	if( Types )
		PyModule_AddObject( submodule, "Types", Types );
	if( Modes )
		PyModule_AddObject( submodule, "Modes", Modes );
	if( Falloffs )
		PyModule_AddObject( submodule, "Falloffs", Falloffs );

	PyModule_AddIntConstant( submodule, "RGB",      IPOKEY_RGB );
	PyModule_AddIntConstant( submodule, "ENERGY",   IPOKEY_ENERGY );
	PyModule_AddIntConstant( submodule, "SPOTSIZE", IPOKEY_SPOTSIZE );
	PyModule_AddIntConstant( submodule, "OFFSET",   IPOKEY_OFFSET );
	PyModule_AddIntConstant( submodule, "SIZE",     IPOKEY_SIZE );
	
	return submodule;
}

/* Three Python Lamp_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    Lamp_CreatePyObject                                          */
/* Description: This function will create a new BPy_Lamp from an existing    */
/*              Blender lamp structure.                                      */
/*****************************************************************************/
PyObject *Lamp_CreatePyObject( Lamp * lamp )
{
	BPy_Lamp *pylamp;
	float *rgb[3];

	pylamp = ( BPy_Lamp * ) PyObject_NEW( BPy_Lamp, &Lamp_Type );

	if( !pylamp )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Lamp object" );

	pylamp->lamp = lamp;

	rgb[0] = &lamp->r;
	rgb[1] = &lamp->g;
	rgb[2] = &lamp->b;

	pylamp->color = ( BPy_rgbTuple * ) rgbTuple_New( rgb );
	Py_INCREF(pylamp->color);
	
	return ( PyObject * ) pylamp;
}

/*****************************************************************************/
/* Function:    Lamp_FromPyObject                                            */
/* Description: This function returns the Blender lamp from the given        */
/*              PyObject.                                                    */
/*****************************************************************************/
Lamp *Lamp_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Lamp * ) pyobj )->lamp;
}

/*****************************************************************************/
/* Python BPy_Lamp methods:                                                  */
/*****************************************************************************/

/* Lamp.__copy__ */
static PyObject *Lamp_copy( BPy_Lamp * self )
{
	Lamp *lamp = copy_lamp(self->lamp );
	lamp->id.us = 0;
	return Lamp_CreatePyObject(lamp);
}

static PyObject *Lamp_getType( BPy_Lamp * self )
{
	return PyInt_FromLong( self->lamp->type );
}

static PyObject *Lamp_getMode( BPy_Lamp * self )
{
	return PyInt_FromLong( self->lamp->mode );
}

static PyObject *Lamp_getSamples( BPy_Lamp * self )
{
	return PyInt_FromLong( self->lamp->samp );
}

static PyObject *Lamp_getRaySamplesX( BPy_Lamp * self )
{
	return PyInt_FromLong( self->lamp->ray_samp );
}

static PyObject *Lamp_getRaySamplesY( BPy_Lamp * self )
{
	return PyInt_FromLong( self->lamp->ray_sampy );
}

static PyObject *Lamp_getAreaSizeX( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->area_size );
}

static PyObject *Lamp_getAreaSizeY( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->area_sizey );
}

static PyObject *Lamp_getBufferSize( BPy_Lamp * self )
{
	return PyInt_FromLong( self->lamp->bufsize );
}

static PyObject *Lamp_getHaloStep( BPy_Lamp * self )
{
	return PyInt_FromLong( self->lamp->shadhalostep );
}

static PyObject *Lamp_getEnergy( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->energy );
}

static PyObject *Lamp_getDist( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->dist );
}

static PyObject *Lamp_getSpotSize( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->spotsize );
}

static PyObject *Lamp_getSpotBlend( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->spotblend );
}

static PyObject *Lamp_getClipStart( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->clipsta );
}

static PyObject *Lamp_getClipEnd( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->clipend );
}

static PyObject *Lamp_getBias( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->bias );
}

static PyObject *Lamp_getSoftness( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->soft );
}

static PyObject *Lamp_getHaloInt( BPy_Lamp * self )
{
	return PyFloat_FromDouble( self->lamp->haint );
}

static PyObject *Lamp_getQuad1( BPy_Lamp * self )
{				/* should we complain if Lamp is not of type Quad? */
	return PyFloat_FromDouble( self->lamp->att1 );
}

static PyObject *Lamp_getQuad2( BPy_Lamp * self )
{			/* should we complain if Lamp is not of type Quad? */
	return PyFloat_FromDouble( self->lamp->att2 );
}

static PyObject *Lamp_getCol( BPy_Lamp * self )
{
	return rgbTuple_getCol( self->color );
}

static PyObject *Lamp_getFalloffType( BPy_Lamp * self )
{
	return PyInt_FromLong( (int)self->lamp->falloff_type );
}

static int Lamp_setType( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setIValueRange ( value, &self->lamp->type,
				  				0, EXPP_LAMP_TYPE_MAX, 'h' );
}

static int Lamp_setMode( BPy_Lamp * self, PyObject * value )
{
	int param;
	static int bitmask = EXPP_LAMP_MODE_SHADOWS
				| EXPP_LAMP_MODE_HALO
				| EXPP_LAMP_MODE_LAYER
				| EXPP_LAMP_MODE_QUAD
				| EXPP_LAMP_MODE_NEGATIVE
				| EXPP_LAMP_MODE_ONLYSHADOW
				| EXPP_LAMP_MODE_SPHERE
				| EXPP_LAMP_MODE_SQUARE
				| EXPP_LAMP_MODE_NODIFFUSE
				| EXPP_LAMP_MODE_NOSPECULAR
				| EXPP_LAMP_MODE_SHAD_RAY
				| EXPP_LAMP_MODE_LAYER_SHADOW;

	if( !PyInt_Check ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = PyInt_AS_LONG ( value );

	if ( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	self->lamp->mode = param; 

	return 0;
}

static int Lamp_setSamples( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->lamp->samp,
								EXPP_LAMP_SAMPLES_MIN,
								EXPP_LAMP_SAMPLES_MAX, 'h' );
}


static int Lamp_setRaySamplesX( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->lamp->ray_samp, 
								EXPP_LAMP_RAYSAMPLES_MIN,
								EXPP_LAMP_RAYSAMPLES_MAX, 'h' );
}

static int Lamp_setRaySamplesY( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->lamp->ray_sampy,
								EXPP_LAMP_RAYSAMPLES_MIN,
								EXPP_LAMP_RAYSAMPLES_MAX, 'h' );
}

static int Lamp_setAreaSizeX( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->area_size, 
								EXPP_LAMP_AREASIZE_MIN,
								EXPP_LAMP_AREASIZE_MAX );
}

static int Lamp_setAreaSizeY( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->area_sizey, 
								EXPP_LAMP_AREASIZE_MIN,
								EXPP_LAMP_AREASIZE_MAX );
}

static int Lamp_setBufferSize( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->lamp->bufsize,
								EXPP_LAMP_BUFFERSIZE_MIN,
								EXPP_LAMP_BUFFERSIZE_MAX, 'h' );
}

static int Lamp_setHaloStep( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->lamp->shadhalostep,
								EXPP_LAMP_HALOSTEP_MIN,
								EXPP_LAMP_HALOSTEP_MAX, 'h' );
}

static int Lamp_setEnergy( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->energy, 
								EXPP_LAMP_ENERGY_MIN,
								EXPP_LAMP_ENERGY_MAX );
}

static int Lamp_setDist( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->dist, 
								EXPP_LAMP_DIST_MIN,
								EXPP_LAMP_DIST_MAX );
}

static int Lamp_setSpotSize( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->spotsize, 
								EXPP_LAMP_SPOTSIZE_MIN,
								EXPP_LAMP_SPOTSIZE_MAX );
}

static int Lamp_setSpotBlend( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->spotblend, 
								EXPP_LAMP_SPOTBLEND_MIN,
								EXPP_LAMP_SPOTBLEND_MAX );
}

static int Lamp_setClipStart( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->clipsta, 
								EXPP_LAMP_CLIPSTART_MIN,
								EXPP_LAMP_CLIPSTART_MAX );
}

static int Lamp_setClipEnd( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->clipend, 
								EXPP_LAMP_CLIPEND_MIN,
								EXPP_LAMP_CLIPEND_MAX );
}

static int Lamp_setBias( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->bias,
								EXPP_LAMP_BIAS_MIN,
								EXPP_LAMP_BIAS_MAX );
}

static int Lamp_setSoftness( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->soft,
								EXPP_LAMP_SOFTNESS_MIN,
								EXPP_LAMP_SOFTNESS_MAX );
}

static int Lamp_setHaloInt( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->haint,
								EXPP_LAMP_HALOINT_MIN,
								EXPP_LAMP_HALOINT_MAX );
}

static int Lamp_setQuad1( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->att1,
								EXPP_LAMP_QUAD1_MIN,
								EXPP_LAMP_QUAD1_MAX );
}

static int Lamp_setQuad2( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->lamp->att2,
								EXPP_LAMP_QUAD2_MIN,
								EXPP_LAMP_QUAD2_MAX );
}

static int Lamp_setFalloffType( BPy_Lamp * self, PyObject * value )
{
	return EXPP_setIValueRange ( value, &self->lamp->falloff_type,
				  				EXPP_LAMP_FALLOFF_MIN, EXPP_LAMP_FALLOFF_MAX, 'h' );
}


static PyObject *Lamp_getComponent( BPy_Lamp * self, void * closure )
{
	switch ( GET_INT_FROM_POINTER(closure) ) {
	case EXPP_LAMP_COMP_R:
		return PyFloat_FromDouble( self->lamp->r );
	case EXPP_LAMP_COMP_G:
		return PyFloat_FromDouble( self->lamp->g );
	case EXPP_LAMP_COMP_B:
		return PyFloat_FromDouble( self->lamp->b );
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"unknown color component specified" );
	}
}

static int Lamp_setComponent( BPy_Lamp * self, PyObject * value,
							void * closure )
{
	float color;

	if( !PyNumber_Check ( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected float argument in [0.0,1.0]" );

	color = (float)PyFloat_AsDouble( value );
	color = EXPP_ClampFloat( color, EXPP_LAMP_COL_MIN, EXPP_LAMP_COL_MAX );

	switch ( GET_INT_FROM_POINTER(closure) ) {
	case EXPP_LAMP_COMP_R:
		self->lamp->r = color;
		return 0;
	case EXPP_LAMP_COMP_G:
		self->lamp->g = color;
		return 0;
	case EXPP_LAMP_COMP_B:
		self->lamp->b = color;
		return 0;
	}
	return EXPP_ReturnIntError( PyExc_RuntimeError,
				"unknown color component specified" );
}

static int Lamp_setCol( BPy_Lamp * self, PyObject * args )
{
	return rgbTuple_setCol( self->color, args );
}

/* lamp.addScriptLink */
static PyObject *Lamp_addScriptLink( BPy_Lamp * self, PyObject * args )
{
	Lamp *lamp = self->lamp;
	ScriptLink *slink = NULL;

	slink = &( lamp )->scriptlink;

	return EXPP_addScriptLink( slink, args, 0 );
}

/* lamp.clearScriptLinks */
static PyObject *Lamp_clearScriptLinks( BPy_Lamp * self, PyObject * args )
{
	Lamp *lamp = self->lamp;
	ScriptLink *slink = NULL;

	slink = &( lamp )->scriptlink;

	return EXPP_clearScriptLinks( slink, args );
}

/* mat.getScriptLinks */
static PyObject *Lamp_getScriptLinks( BPy_Lamp * self, PyObject * value )
{
	Lamp *lamp = self->lamp;
	ScriptLink *slink = NULL;
	PyObject *ret = NULL;

	slink = &( lamp )->scriptlink;

	ret = EXPP_getScriptLinks( slink, value, 0 );

	if( ret )
		return ret;
	else
		return NULL;
}

/*****************************************************************************/
/* Function:    Lamp_dealloc                                                 */
/* Description: This is a callback function for the BPy_Lamp type. It is     */
/*              the destructor function.                                     */
/*****************************************************************************/
static void Lamp_dealloc( BPy_Lamp * self )
{
	Py_DECREF( self->color );
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:    Lamp_compare                                                 */
/* Description: This is a callback function for the BPy_Lamp type. It        */
/*              compares two Lamp_Type objects. Only the "==" and "!="       */
/*              comparisons are meaningful. Returns 0 for equality and -1    */
/*              if they don't point to the same Blender Lamp struct.         */
/*              In Python it becomes 1 if they are equal, 0 otherwise.       */
/*****************************************************************************/
static int Lamp_compare( BPy_Lamp * a, BPy_Lamp * b )
{
	return ( a->lamp == b->lamp ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:    Lamp_repr                                                    */
/* Description: This is a callback function for the BPy_Lamp type. It        */
/*              builds a meaninful string to represent lamp objects.         */
/*****************************************************************************/
static PyObject *Lamp_repr( BPy_Lamp * self )
{
	return PyString_FromFormat( "[Lamp \"%s\"]", self->lamp->id.name + 2 );
}

static PyObject *Lamp_getIpo( BPy_Lamp * self )
{
	struct Ipo *ipo = self->lamp->ipo;

	if( !ipo )
		Py_RETURN_NONE;

	return Ipo_CreatePyObject( ipo );
}

/*
 * this should accept a Py_None argument and just delete the Ipo link
 * (as Lamp_clearIpo() does)
 */

static int Lamp_setIpo( BPy_Lamp * self, PyObject * value )
{
	return GenericLib_assignData(value, (void **) &self->lamp->ipo, 0, 1, ID_IP, ID_LA);
}

/*
 * Lamp_insertIpoKey()
 *  inserts Lamp IPO key for RGB,ENERGY,SPOTSIZE,OFFSET,SIZE
 */

static PyObject *Lamp_insertIpoKey( BPy_Lamp * self, PyObject * args )
{
	int key = 0, flag = 0, map;

	if( !PyArg_ParseTuple( args, "i", &( key ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
										"expected int argument" ) );

	map = texchannel_to_adrcode(self->lamp->texact);

	/* flag should be initialised with the 'autokeying' flags like for normal keying */
	if (IS_AUTOKEY_FLAG(INSERTNEEDED)) flag |= INSERTKEY_NEEDED;
	
	if (key == IPOKEY_RGB ) {
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL, LA_COL_R, flag);
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL,LA_COL_G, flag);
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL,LA_COL_B, flag);
	}
	if (key == IPOKEY_ENERGY ) {
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL,LA_ENERGY, flag);
	}	
	if (key == IPOKEY_SPOTSIZE ) {
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL,LA_SPOTSI, flag);
	}
	if (key == IPOKEY_OFFSET ) {
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL, map+MAP_OFS_X, flag);
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL, map+MAP_OFS_Y, flag);
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL, map+MAP_OFS_Z, flag);
	}
	if (key == IPOKEY_SIZE ) {
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL, map+MAP_SIZE_X, flag);
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL, map+MAP_SIZE_Y, flag);
		insertkey((ID *)self->lamp, ID_LA, NULL, NULL, map+MAP_SIZE_Z, flag);
	}

	allspace(REMAKEIPO, 0);
	EXPP_allqueue(REDRAWIPO, 0);
	EXPP_allqueue(REDRAWVIEW3D, 0);
	EXPP_allqueue(REDRAWACTION, 0);
	EXPP_allqueue(REDRAWNLA, 0);

	Py_RETURN_NONE;
}

static PyObject *Lamp_getModesConst( void )
{
	return Py_BuildValue
			( "{s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h}",
			  "Shadows", EXPP_LAMP_MODE_SHADOWS, "Halo",
			  EXPP_LAMP_MODE_HALO, "Layer", EXPP_LAMP_MODE_LAYER,
			  "Quad", EXPP_LAMP_MODE_QUAD, "Negative",
			  EXPP_LAMP_MODE_NEGATIVE, "OnlyShadow",
			  EXPP_LAMP_MODE_ONLYSHADOW, "Sphere",
			  EXPP_LAMP_MODE_SPHERE, "Square",
			  EXPP_LAMP_MODE_SQUARE, "NoDiffuse",
			  EXPP_LAMP_MODE_NODIFFUSE, "NoSpecular",
			  EXPP_LAMP_MODE_NOSPECULAR, "RayShadow",
			  EXPP_LAMP_MODE_SHAD_RAY, "LayerShadow",
			  EXPP_LAMP_MODE_LAYER_SHADOW);
}

static PyObject *Lamp_getTypesConst( void )
{
	return Py_BuildValue( "{s:h,s:h,s:h,s:h,s:h,s:h}",
				      "Lamp", EXPP_LAMP_TYPE_LAMP,
				      "Sun", EXPP_LAMP_TYPE_SUN,
				      "Spot", EXPP_LAMP_TYPE_SPOT,
				      "Hemi", EXPP_LAMP_TYPE_HEMI, 
				      "Area", EXPP_LAMP_TYPE_AREA, 
				      "Photon", EXPP_LAMP_TYPE_YF_PHOTON );
}

static PyObject *Lamp_getTextures( BPy_Lamp * self )
{
	int i;
	PyObject *tuple;

	/* build a texture list */
	tuple = PyTuple_New( MAX_MTEX );
	if( !tuple )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create PyTuple" );

	for( i = 0; i < MAX_MTEX; ++i ) {
		struct MTex *mtex = self->lamp->mtex[i];
		if( mtex ) {
			PyTuple_SET_ITEM( tuple, i, MTex_CreatePyObject( mtex, ID_LA ) );
		} else {
			Py_INCREF( Py_None );
			PyTuple_SET_ITEM( tuple, i, Py_None );
		}
	}

	return tuple;
}

static int Lamp_setTextures( BPy_Lamp * self, PyObject * value )
{
	int i;

	if( !PyList_Check( value ) && !PyTuple_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected tuple or list of integers" );

	/* don't allow more than MAX_MTEX items */
	if( PySequence_Size(value) > MAX_MTEX )
		return EXPP_ReturnIntError( PyExc_AttributeError,
						"size of sequence greater than number of allowed textures" );

	/* get a fast sequence; in Python 2.5, this just return the original
	 * list or tuple and INCREFs it, so we must DECREF */
	value = PySequence_Fast( value, "" );

	/* check the list for valid entries */
	for( i= 0; i < PySequence_Size(value) ; ++i ) {
		PyObject *item = PySequence_Fast_GET_ITEM( value, i );
		if( item == Py_None || ( BPy_MTex_Check( item ) &&
						((BPy_MTex *)item)->type == ID_LA ) ) {
			continue;
		} else {
			Py_DECREF(value);
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected tuple or list containing lamp MTex objects and NONE" );
		}
	}

	/* for each MTex object, copy to this structure */
	for( i= 0; i < PySequence_Size(value) ; ++i ) {
		PyObject *item = PySequence_Fast_GET_ITEM( value, i );
		struct MTex *mtex = self->lamp->mtex[i];
		if( item != Py_None ) {
			BPy_MTex *obj = (BPy_MTex *)item;

			/* if MTex is already at this location, just skip it */
			if( obj->mtex == mtex )	continue;

			/* create a new entry if needed, otherwise update reference count
			 * for texture that is being replaced */
			if( !mtex )
				mtex = self->lamp->mtex[i] = add_mtex(  );
			else
				mtex->tex->id.us--;

			/* copy the data */
			mtex->tex = obj->mtex->tex;
			id_us_plus( &mtex->tex->id );
			mtex->texco = obj->mtex->texco;
			mtex->mapto = obj->mtex->mapto;
		}
	}

	/* now go back and free any entries now marked as None */
	for( i= 0; i < PySequence_Size(value) ; ++i ) {
		PyObject *item = PySequence_Fast_GET_ITEM( value, i );
		struct MTex *mtex = self->lamp->mtex[i];
		if( item == Py_None && mtex ) {
			mtex->tex->id.us--;
			MEM_freeN( mtex );
			self->lamp->mtex[i] = NULL;
		} 
	}

	Py_DECREF(value);
	return 0;
}

/* #####DEPRECATED###### */

static PyObject *Lamp_oldsetSamples( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setSamples );
}

static PyObject *Lamp_oldsetRaySamplesX( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setRaySamplesX );
}

static PyObject *Lamp_oldsetRaySamplesY( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setRaySamplesY );
}

static PyObject *Lamp_oldsetAreaSizeX( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setAreaSizeX );
}

static PyObject *Lamp_oldsetAreaSizeY( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setAreaSizeY );
}

static PyObject *Lamp_oldsetBufferSize( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setBufferSize );
}

static PyObject *Lamp_oldsetHaloStep( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setHaloStep );
}

static PyObject *Lamp_oldsetEnergy( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setEnergy );
}

static PyObject *Lamp_oldsetDist( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setDist );
}

static PyObject *Lamp_oldsetSpotSize( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setSpotSize );
}

static PyObject *Lamp_oldsetSpotBlend( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setSpotBlend );
}

static PyObject *Lamp_oldsetClipStart( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setClipStart );
}

static PyObject *Lamp_oldsetClipEnd( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setClipEnd );
}

static PyObject *Lamp_oldsetBias( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setBias );
}

static PyObject *Lamp_oldsetSoftness( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setSoftness );
}

static PyObject *Lamp_oldsetHaloInt( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setHaloInt );
}

static PyObject *Lamp_oldsetQuad1( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setQuad1 );
}

static PyObject *Lamp_oldsetQuad2( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setQuad2 );
}

static PyObject *Lamp_oldsetIpo( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setIpo );
}

static PyObject *Lamp_oldsetCol( BPy_Lamp * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Lamp_setCol );
}

/* 
 * the "not-well-behaved" methods which require more processing than 
 * just the simple wrapper
 */

/*
 * clearIpo() returns True/False depending on whether lamp has an Ipo
 */

static PyObject *Lamp_clearIpo( BPy_Lamp * self )
{
	/* if Ipo defined, delete it and return true */

	if( self->lamp->ipo ) {
		PyObject *value = Py_BuildValue( "(O)", Py_None );
		EXPP_setterWrapper ( (void *)self, value, (setter)Lamp_setIpo );
		Py_DECREF ( value );
		return EXPP_incr_ret_True();
	}
	return EXPP_incr_ret_False(); /* no ipo found */
}

/*
 * setType() accepts a string while mode setter takes an integer
 */

static PyObject *Lamp_oldsetType( BPy_Lamp * self, PyObject * value )
{
	char *type = PyString_AsString(value);
	PyObject *arg, *error;

	/* parse string argument */	
	if( !type ) 
		return EXPP_ReturnPyObjError ( PyExc_TypeError,
					       "expected string argument" );
	
	/* check for valid arguments, set type accordingly */

	if( !strcmp( type, "Lamp" ) )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_LAMP;
	else if( !strcmp( type, "Sun" ) )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_SUN;
	else if( !strcmp( type, "Spot" ) )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_SPOT;
	else if( !strcmp( type, "Hemi" ) )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_HEMI;
	else if( !strcmp( type, "Area" ) )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_AREA;
	else if( !strcmp( type, "Photon" ) )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_YF_PHOTON;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
						"unknown lamp type" );

	/* build tuple, call wrapper */

	arg = Py_BuildValue( "(i)", self->lamp->type );
	error = EXPP_setterWrapper ( (void *)self, arg, (setter)Lamp_setType );
	Py_DECREF ( arg );
	return error;
}

/*
 * setMode() accepts up to ten strings while mode setter takes an integer
 */

static PyObject *Lamp_oldsetMode( BPy_Lamp * self, PyObject * args )
{
	short i, flag = 0;
	PyObject *error, *value;
	char *name;

	/* check that we're passed a tuple of no more than 10 args*/

	if ( !PyTuple_Check( args ) || PyTuple_Size( args ) > 10 )
		return EXPP_ReturnPyObjError ( PyExc_AttributeError,
					"expected up to 10 string arguments" );

	/* check each argument for type, find its value */

	for ( i = (short)PyTuple_Size( args ); i-- ; ) {
 		name = PyString_AsString ( PyTuple_GET_ITEM( args, i ) );
		if( !name )
			return EXPP_ReturnPyObjError ( PyExc_AttributeError,
					"expected string argument" );

		if( !strcmp( name, "Shadows" ) )
			flag |= ( short ) EXPP_LAMP_MODE_SHADOWS;
		else if( !strcmp( name, "Halo" ) )
			flag |= ( short ) EXPP_LAMP_MODE_HALO;
		else if( !strcmp( name, "Layer" ) )
			flag |= ( short ) EXPP_LAMP_MODE_LAYER;
		else if( !strcmp( name, "Quad" ) )
			flag |= ( short ) EXPP_LAMP_MODE_QUAD;
		else if( !strcmp( name, "Negative" ) )
			flag |= ( short ) EXPP_LAMP_MODE_NEGATIVE;
		else if( !strcmp( name, "OnlyShadow" ) )
			flag |= ( short ) EXPP_LAMP_MODE_ONLYSHADOW;
		else if( !strcmp( name, "Sphere" ) )
			flag |= ( short ) EXPP_LAMP_MODE_SPHERE;
		else if( !strcmp( name, "Square" ) )
			flag |= ( short ) EXPP_LAMP_MODE_SQUARE;
		else if( !strcmp( name, "NoDiffuse" ) )
			flag |= ( short ) EXPP_LAMP_MODE_NODIFFUSE;
		else if( !strcmp( name, "NoSpecular" ) )
			flag |= ( short ) EXPP_LAMP_MODE_NOSPECULAR;
		else if( !strcmp( name, "RayShadow" ) )
			flag |= ( short ) EXPP_LAMP_MODE_SHAD_RAY;
		else if( !strcmp( name, "LayerShadow" ) )
			flag |= ( short ) EXPP_LAMP_MODE_LAYER_SHADOW;
		else
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
							"unknown lamp flag argument" );
	}

	/* build tuple, call wrapper */

	value = Py_BuildValue( "(i)", flag );
	error = EXPP_setterWrapper ( (void *)self, value, (setter)Lamp_setMode );
	Py_DECREF ( value );
	return error;
}

