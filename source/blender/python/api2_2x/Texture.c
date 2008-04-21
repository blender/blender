/*  
 *  $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA    02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Alex Mole, Nathan Letwory, Joilnen B. Leite, Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
*/
#include "Texture.h" /*This must come first*/

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "MTex.h"
#include "Image.h"
#include "Ipo.h"
#include "IDProp.h"
#include "constant.h"
#include "blendef.h"
#include "gen_utils.h"
#include "gen_library.h"

#include "vector.h" /* for Texture_evaluate(vec) */
#include "Material.h" /* for EXPP_Colorband_fromPyList and EXPP_PyList_fromColorband */
#include "RE_shader_ext.h"

/*****************************************************************************/
/* Blender.Texture constants                                                 */
/*****************************************************************************/
#define EXPP_TEX_TYPE_NONE                  0

#define EXPP_TEX_TYPE_MIN                   EXPP_TEX_TYPE_NONE
#define EXPP_TEX_TYPE_MAX                   TEX_DISTNOISE

#define EXPP_TEX_ANIMFRAME_MIN              0
#define EXPP_TEX_ANIMFRAME_MAX              ((int)MAXFRAMEF)
#define EXPP_TEX_ANIMLEN_MIN                0
#define EXPP_TEX_ANIMLEN_MAX                ((int)(MAXFRAMEF)/2)
#define EXPP_TEX_ANIMMONSTART_MIN           0
#define EXPP_TEX_ANIMMONSTART_MAX           ((int)MAXFRAMEF)
#define EXPP_TEX_ANIMMONDUR_MIN             0
#define EXPP_TEX_ANIMMONDUR_MAX             250
#define EXPP_TEX_ANIMOFFSET_MIN             -((int)MAXFRAMEF)
#define EXPP_TEX_ANIMOFFSET_MAX             ((int)MAXFRAMEF)
#define EXPP_TEX_ANIMSTART_MIN              1
#define EXPP_TEX_ANIMSTART_MAX              ((int)MAXFRAMEF)
#define EXPP_TEX_FIEIMA_MIN                 1
#define EXPP_TEX_FIEIMA_MAX                 200
#define EXPP_TEX_NOISEDEPTH_MIN             0
#define EXPP_TEX_NOISEDEPTH_MAX             6
/* max depth is different for magic type textures */
#define EXPP_TEX_NOISEDEPTH_MAX_MAGIC       10
#define EXPP_TEX_REPEAT_MIN                 1
#define EXPP_TEX_REPEAT_MAX                 512

#define EXPP_TEX_FILTERSIZE_MIN             0.1f
#define EXPP_TEX_FILTERSIZE_MAX             25.0f
#define EXPP_TEX_NOISESIZE_MIN              0.0001f
#define EXPP_TEX_NOISESIZE_MAX              2.0f
#define EXPP_TEX_BRIGHTNESS_MIN             0.0f
#define EXPP_TEX_BRIGHTNESS_MAX             2.0f
#define EXPP_TEX_CONTRAST_MIN               0.01f
#define EXPP_TEX_CONTRAST_MAX               5.0f
#define EXPP_TEX_CROP_MIN                   -10.0f
#define EXPP_TEX_CROP_MAX                   10.0f
#define EXPP_TEX_RGBCOL_MIN                 0.0f
#define EXPP_TEX_RGBCOL_MAX                 2.0f
#define EXPP_TEX_TURBULENCE_MIN             0.0f
#define EXPP_TEX_TURBULENCE_MAX             200.0f
#define EXPP_TEX_MH_G_MIN                   0.0001f
#define EXPP_TEX_MH_G_MAX                   2.0f
#define EXPP_TEX_LACUNARITY_MIN             0.0f
#define EXPP_TEX_LACUNARITY_MAX             6.0f
#define EXPP_TEX_OCTS_MIN                   0.0f
#define EXPP_TEX_OCTS_MAX                   8.0f
#define EXPP_TEX_ISCALE_MIN                 0.0f
#define EXPP_TEX_ISCALE_MAX                 10.0f
#define EXPP_TEX_EXP_MIN                    0.010f
#define EXPP_TEX_EXP_MAX                    10.0f
#define EXPP_TEX_WEIGHT1_MIN                -2.0f
#define EXPP_TEX_WEIGHT1_MAX                2.0f
#define EXPP_TEX_WEIGHT2_MIN                -2.0f
#define EXPP_TEX_WEIGHT2_MAX                2.0f
#define EXPP_TEX_WEIGHT3_MIN                -2.0f
#define EXPP_TEX_WEIGHT3_MAX                2.0f
#define EXPP_TEX_WEIGHT4_MIN                -2.0f
#define EXPP_TEX_WEIGHT4_MAX                2.0f
#define EXPP_TEX_DISTAMNT_MIN               0.0f
#define EXPP_TEX_DISTAMNT_MAX               10.0f

/* i can't find these defined anywhere- they're just taken from looking at   */
/* the button creation code in source/blender/src/buttons_shading.c          */
/* cloud stype */
#define EXPP_TEX_STYPE_CLD_DEFAULT          0
#define EXPP_TEX_STYPE_CLD_COLOR            1
/* wood stype */
#define EXPP_TEX_STYPE_WOD_BANDS            0
#define EXPP_TEX_STYPE_WOD_RINGS            1
#define EXPP_TEX_STYPE_WOD_BANDNOISE        2
#define EXPP_TEX_STYPE_WOD_RINGNOISE        3
/* magic stype */
#define EXPP_TEX_STYPE_MAG_DEFAULT          0
/* marble stype */
#define EXPP_TEX_STYPE_MBL_SOFT             0
#define EXPP_TEX_STYPE_MBL_SHARP            1
#define EXPP_TEX_STYPE_MBL_SHARPER          2
/* blend stype */
#define EXPP_TEX_STYPE_BLN_LIN              0
#define EXPP_TEX_STYPE_BLN_QUAD             1
#define EXPP_TEX_STYPE_BLN_EASE             2
#define EXPP_TEX_STYPE_BLN_DIAG             3
#define EXPP_TEX_STYPE_BLN_SPHERE           4
#define EXPP_TEX_STYPE_BLN_HALO             5
/* stucci stype */
#define EXPP_TEX_STYPE_STC_PLASTIC          0
#define EXPP_TEX_STYPE_STC_WALLIN           1
#define EXPP_TEX_STYPE_STC_WALLOUT          2
/* noise stype */
#define EXPP_TEX_STYPE_NSE_DEFAULT          0
/* image stype */
#define EXPP_TEX_STYPE_IMG_DEFAULT          0
/* plug-in stype */
#define EXPP_TEX_STYPE_PLG_DEFAULT          0
/* envmap stype */
#define EXPP_TEX_STYPE_ENV_STATIC           0
#define EXPP_TEX_STYPE_ENV_ANIM             1
#define EXPP_TEX_STYPE_ENV_LOAD             2
/* musgrave stype */
#define EXPP_TEX_STYPE_MUS_MFRACTAL         0
#define EXPP_TEX_STYPE_MUS_RIDGEDMF         1
#define EXPP_TEX_STYPE_MUS_HYBRIDMF         2
#define EXPP_TEX_STYPE_MUS_FBM              3
#define EXPP_TEX_STYPE_MUS_HTERRAIN         4
/* voronoi stype */
#define EXPP_TEX_STYPE_VN_INT               0
#define EXPP_TEX_STYPE_VN_COL1              1
#define EXPP_TEX_STYPE_VN_COL2              2
#define EXPP_TEX_STYPE_VN_COL3              3

#define EXPP_TEX_EXTEND_MIN                 TEX_EXTEND
#define EXPP_TEX_EXTEND_MAX                 TEX_CHECKER

#define	EXPP_TEX_NOISE_SINE					0
#define	EXPP_TEX_NOISE_SAW					1
#define	EXPP_TEX_NOISE_TRI					2
#define	EXPP_TEX_NOISEBASIS2				0xffff

/****************************************************************************/
/* Texture String->Int maps                                                 */
/****************************************************************************/

static const EXPP_map_pair tex_type_map[] = {
	{"None", EXPP_TEX_TYPE_NONE},
	{"Clouds", TEX_CLOUDS},
	{"Wood", TEX_WOOD},
	{"Marble", TEX_MARBLE},
	{"Magic", TEX_MAGIC},
	{"Blend", TEX_BLEND},
	{"Stucci", TEX_STUCCI},
	{"Noise", TEX_NOISE},
	{"Image", TEX_IMAGE},
	{"Plugin", TEX_PLUGIN},
	{"EnvMap", TEX_ENVMAP},
	{"Musgrave", TEX_MUSGRAVE},
	{"Voronoi", TEX_VORONOI},
	{"DistortedNoise", TEX_DISTNOISE},
	{NULL, 0}
};

static const EXPP_map_pair tex_flag_map[] = {
/* NOTE "CheckerOdd" and "CheckerEven" are new */
	{"ColorBand",  TEX_COLORBAND },
	{"FlipBlend", TEX_FLIPBLEND},
	{"NegAlpha", TEX_NEGALPHA},
	{"CheckerOdd",TEX_CHECKER_ODD},
	{"CheckerEven",TEX_CHECKER_EVEN},
	{"PreviewAlpha",TEX_PRV_ALPHA},
	{"RepeatXMirror",TEX_REPEAT_XMIR},
	{"RepeatYMirror",TEX_REPEAT_YMIR}, 
	{NULL, 0}
};

/* NOTE: flags moved to image... */
static const EXPP_map_pair tex_imageflag_map[] = {
	{"InterPol", TEX_INTERPOL},
	{"UseAlpha", TEX_USEALPHA},
	{"MipMap", TEX_MIPMAP},
	{"Rot90", TEX_IMAROT},
	{"CalcAlpha", TEX_CALCALPHA},
	{"NormalMap", TEX_NORMALMAP},
	{NULL, 0}
};

static const EXPP_map_pair tex_extend_map[] = {
	{"Extend", TEX_EXTEND},
	{"Clip", TEX_CLIP},
	{"ClipCube", TEX_CLIPCUBE},
	{"Repeat", TEX_REPEAT},
/* NOTE "Checker" is new */
	{"Checker", TEX_CHECKER},
	{NULL, 0}
};

/* array of maps for stype */
static const EXPP_map_pair tex_stype_default_map[] = {
	{"Default", 0},
	{NULL, 0}
};
static const EXPP_map_pair tex_stype_clouds_map[] = {
	{"Default", 0},
	{"CloudDefault", EXPP_TEX_STYPE_CLD_DEFAULT},
	{"CloudColor", EXPP_TEX_STYPE_CLD_COLOR},
	{NULL, 0}
};
static const EXPP_map_pair tex_stype_wood_map[] = {
	{"Default", 0},
	{"WoodBands", EXPP_TEX_STYPE_WOD_BANDS},
	{"WoodRings", EXPP_TEX_STYPE_WOD_RINGS},
	{"WoodBandNoise", EXPP_TEX_STYPE_WOD_BANDNOISE},
	{"WoodRingNoise", EXPP_TEX_STYPE_WOD_RINGNOISE},
	{NULL, 0}
};
static const EXPP_map_pair tex_stype_marble_map[] = {
	{"Default", 0},
	{"MarbleSoft", EXPP_TEX_STYPE_MBL_SOFT},
	{"MarbleSharp", EXPP_TEX_STYPE_MBL_SHARP},
	{"MarbleSharper", EXPP_TEX_STYPE_MBL_SHARPER},
	{NULL, 0}
};
static const EXPP_map_pair tex_stype_blend_map[] = {
	{"Default", 0},
	{"BlendLin", EXPP_TEX_STYPE_BLN_LIN},
	{"BlendQuad", EXPP_TEX_STYPE_BLN_QUAD},
	{"BlendEase", EXPP_TEX_STYPE_BLN_EASE},
	{"BlendDiag", EXPP_TEX_STYPE_BLN_DIAG},
	{"BlendSphere", EXPP_TEX_STYPE_BLN_SPHERE},
	{"BlendHalo", EXPP_TEX_STYPE_BLN_HALO},
	{NULL, 0}
};
static const EXPP_map_pair tex_stype_stucci_map[] = {
	{"Default", 0},
	{"StucciPlastic", EXPP_TEX_STYPE_STC_PLASTIC},
	{"StucciWallIn", EXPP_TEX_STYPE_STC_WALLIN},
	{"StucciWallOut", EXPP_TEX_STYPE_STC_WALLOUT},
	{NULL, 0}
};
static const EXPP_map_pair tex_stype_envmap_map[] = {
	{"Default", 0},
	{"EnvmapStatic", EXPP_TEX_STYPE_ENV_STATIC},
	{"EnvmapAnim", EXPP_TEX_STYPE_ENV_ANIM},
	{"EnvmapLoad", EXPP_TEX_STYPE_ENV_LOAD},
	{NULL, 0}
};

static const EXPP_map_pair tex_stype_musg_map[] = {
	{"Default", 0},
	{"MultiFractal", EXPP_TEX_STYPE_MUS_MFRACTAL},
	{"HeteroTerrain", EXPP_TEX_STYPE_MUS_HTERRAIN},
	{"RidgedMultiFractal", EXPP_TEX_STYPE_MUS_RIDGEDMF},
	{"HybridMultiFractal", EXPP_TEX_STYPE_MUS_HYBRIDMF},
	{"fBM", EXPP_TEX_STYPE_MUS_FBM},
	{NULL, 0}
};

static const EXPP_map_pair tex_stype_distortednoise_map[] = {
	{"Default", 0},
	{"BlenderOriginal", TEX_BLENDER},
	{"OriginalPerlin", TEX_STDPERLIN},
	{"ImprovedPerlin", TEX_NEWPERLIN},
	{"VoronoiF1", TEX_VORONOI_F1},
	{"VoronoiF2", TEX_VORONOI_F2},
	{"VoronoiF3", TEX_VORONOI_F3},
	{"VoronoiF4", TEX_VORONOI_F4},
	{"VoronoiF2-F1", TEX_VORONOI_F2F1},
	{"VoronoiCrackle", TEX_VORONOI_CRACKLE},
	{"CellNoise", TEX_CELLNOISE},
	{NULL, 0}
};

static const EXPP_map_pair tex_stype_voronoi_map[] = {
	{"Default", 0},
	{"Int", EXPP_TEX_STYPE_VN_INT},
	{"Col1", EXPP_TEX_STYPE_VN_COL1},
	{"Col2", EXPP_TEX_STYPE_VN_COL2},
	{"Col3", EXPP_TEX_STYPE_VN_COL3},
	{NULL, 0}
};

static const EXPP_map_pair tex_distance_voronoi_map[] = {
	{"Default", 0},
	{"Distance", TEX_DISTANCE},
	{"DistanceSquared", TEX_DISTANCE_SQUARED},
	{"Manhattan", TEX_MANHATTAN},
	{"Chebychev", TEX_CHEBYCHEV},
	{"MinkovskyHalf", TEX_MINKOVSKY_HALF},
	{"MinkovskyFour", TEX_MINKOVSKY_FOUR},
	{"Minkovsky", TEX_MINKOVSKY},
	{NULL, 0}
};

static const EXPP_map_pair *tex_stype_map[] = {
	tex_stype_default_map,	/* none */
	tex_stype_clouds_map,
	tex_stype_wood_map,
	tex_stype_marble_map,
	tex_stype_default_map,	/* magic */
	tex_stype_blend_map,
	tex_stype_stucci_map,
	tex_stype_default_map,	/* noise */
	tex_stype_default_map,	/* image */
	tex_stype_default_map,	/* plugin */
	tex_stype_envmap_map,
	tex_stype_musg_map,	/* musgrave */
	tex_stype_voronoi_map,	/* voronoi */
	tex_stype_distortednoise_map,	/* distorted noise */
	tex_distance_voronoi_map
};

/*****************************************************************************/
/* Python API function prototypes for the Texture module.                    */
/*****************************************************************************/
static PyObject *M_Texture_New( PyObject * self, PyObject * args,
				PyObject * keywords );
static PyObject *M_Texture_Get( PyObject * self, PyObject * args );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Texture.__doc__                                                   */
/*****************************************************************************/
static char M_Texture_doc[] = "The Blender Texture module\n\
\n\
This module provides access to **Texture** objects in Blender\n";

static char M_Texture_New_doc[] = "Texture.New (name = 'Tex'):\n\
        Return a new Texture object with the given type and name.";

static char M_Texture_Get_doc[] = "Texture.Get (name = None):\n\
        Return the texture with the given 'name', None if not found, or\n\
        Return a list with all texture objects in the current scene,\n\
        if no argument was given.";

/*****************************************************************************/
/* Python method structure definition for Blender.Texture module:            */
/*****************************************************************************/
struct PyMethodDef M_Texture_methods[] = {
	{"New", ( PyCFunction ) M_Texture_New, METH_VARARGS | METH_KEYWORDS,
	 M_Texture_New_doc},
	{"Get", M_Texture_Get, METH_VARARGS, M_Texture_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Texture methods declarations:                                  */
/*****************************************************************************/
#define GETFUNC(name)   static PyObject *Texture_##name(BPy_Texture *self)
#define OLDSETFUNC(name)   static PyObject *Texture_old##name(BPy_Texture *self,   \
                                                        PyObject *args)
#define SETFUNC(name)   static int Texture_##name(BPy_Texture *self,   \
                                                        PyObject *value)
#if 0
GETFUNC( getExtend );
GETFUNC( getImage );
GETFUNC( getType );
GETFUNC( getSType );
GETFUNC( clearIpo );
GETFUNC( getAnimMontage );
GETFUNC( getAnimLength );
SETFUNC( setAnimLength );
SETFUNC( setAnimMontage );
#endif

GETFUNC( oldgetSType );
GETFUNC( oldgetType );

GETFUNC( clearIpo );
GETFUNC( getAnimFrames );
GETFUNC( getAnimOffset );
GETFUNC( getAnimStart );
GETFUNC( getBrightness );
GETFUNC( getContrast );
GETFUNC( getCrop );
GETFUNC( getDistAmnt );
GETFUNC( getDistMetric );
GETFUNC( getExp );
GETFUNC( getExtend );
GETFUNC( getIntExtend );
GETFUNC( getFieldsPerImage );
GETFUNC( getFilterSize );
GETFUNC( getFlags );
GETFUNC( getHFracDim );
GETFUNC( getImage );
GETFUNC( getIpo );
GETFUNC( getIScale );
GETFUNC( getLacunarity );
GETFUNC( getNoiseBasis );
GETFUNC( getNoiseDepth );
GETFUNC( getNoiseSize );
GETFUNC( getNoiseType );
GETFUNC( getOcts );
GETFUNC( getRepeat );
GETFUNC( getRGBCol );
GETFUNC( getSType );
GETFUNC( getTurbulence );
GETFUNC( getType );
GETFUNC( getWeight1 );
GETFUNC( getWeight2 );
GETFUNC( getWeight3 );
GETFUNC( getWeight4 );
#if 0
/* not defined */
GETFUNC( getUsers );
#endif

OLDSETFUNC( setDistMetric );
OLDSETFUNC( setDistNoise );	/* special case used for ".noisebasis = ...  */
OLDSETFUNC( setExtend );
OLDSETFUNC( setFlags );
OLDSETFUNC( setImage );
OLDSETFUNC( setImageFlags );
OLDSETFUNC( setIpo );
OLDSETFUNC( setNoiseBasis );
OLDSETFUNC( setSType );
OLDSETFUNC( setType );

SETFUNC( setAnimFrames );
SETFUNC( setAnimOffset );
SETFUNC( setAnimStart );
SETFUNC( setBrightness );
SETFUNC( setContrast );
SETFUNC( setCrop );
SETFUNC( setDistAmnt );
SETFUNC( setDistMetric );
SETFUNC( setExp );
SETFUNC( setIntExtend );
SETFUNC( setFieldsPerImage );
SETFUNC( setFilterSize );
SETFUNC( setFlags );
SETFUNC( setHFracDim );
SETFUNC( setImage );
SETFUNC( setIpo );
SETFUNC( setIScale );
SETFUNC( setLacunarity );
SETFUNC( setNoiseBasis );
SETFUNC( setNoiseDepth );
SETFUNC( setNoiseSize );
SETFUNC( setNoiseType );
SETFUNC( setOcts );
SETFUNC( setRepeat );
SETFUNC( setRGBCol );
SETFUNC( setSType );
SETFUNC( setTurbulence );
SETFUNC( setType );
SETFUNC( setWeight1 );
SETFUNC( setWeight2 );
SETFUNC( setWeight3 );
SETFUNC( setWeight4 );

static PyObject *Texture_getImageFlags( BPy_Texture *self, void *type );
static PyObject *Texture_getIUserFlags( BPy_Texture *self, void *type );
static PyObject *Texture_getIUserCyclic( BPy_Texture *self );
static PyObject *Texture_getNoiseBasis2( BPy_Texture *self, void *type );
static int Texture_setImageFlags( BPy_Texture *self, PyObject *args,
								void *type );
static int Texture_setIUserFlags( BPy_Texture *self, PyObject *args,
								void *type );
static int Texture_setIUserCyclic( BPy_Texture *self, PyObject *args );
static int Texture_setNoiseBasis2( BPy_Texture *self, PyObject *args,
								void *type );
								
static PyObject *Texture_getColorband( BPy_Texture * self);
int Texture_setColorband( BPy_Texture * self, PyObject * value);
static PyObject *Texture_evaluate( BPy_Texture *self, PyObject *value );
static PyObject *Texture_copy( BPy_Texture *self );

/*****************************************************************************/
/* Python BPy_Texture methods table:                                         */
/*****************************************************************************/
static PyMethodDef BPy_Texture_methods[] = {
	/* name, method, flags, doc */
	{"getExtend", ( PyCFunction ) Texture_getExtend, METH_NOARGS,
	 "() - Return Texture extend mode"},
	{"getImage", ( PyCFunction ) Texture_getImage, METH_NOARGS,
	 "() - Return Texture Image"},
	{"getName", ( PyCFunction ) GenericLib_getName, METH_NOARGS,
	 "() - Return Texture name"},
	{"getSType", ( PyCFunction ) Texture_oldgetSType, METH_NOARGS,
	 "() - Return Texture stype as string"},
	{"getType", ( PyCFunction ) Texture_oldgetType, METH_NOARGS,
	 "() - Return Texture type as string"},
	{"getIpo", ( PyCFunction ) Texture_getIpo, METH_NOARGS,
	 "() - Return Texture Ipo"},
	{"setIpo", ( PyCFunction ) Texture_oldsetIpo, METH_VARARGS,
	 "(Blender Ipo) - Set Texture Ipo"},
	{"clearIpo", ( PyCFunction ) Texture_clearIpo, METH_NOARGS,
	 "() - Unlink Ipo from this Texture."},
	{"setExtend", ( PyCFunction ) Texture_oldsetExtend, METH_VARARGS,
	 "(s) - Set Texture extend mode"},
	{"setFlags", ( PyCFunction ) Texture_oldsetFlags, METH_VARARGS,
	 "(f1,f2,f3,f4,f5) - Set Texture flags"},
	{"setImage", ( PyCFunction ) Texture_oldsetImage, METH_VARARGS,
	 "(Blender Image) - Set Texture Image"},
	{"setImageFlags", ( PyCFunction ) Texture_oldsetImageFlags, METH_VARARGS,
	 "(s,s,s,s,...) - Set Texture image flags"},
	{"setName", ( PyCFunction ) GenericLib_setName_with_method, METH_VARARGS,
	 "(s) - Set Texture name"},
	{"setSType", ( PyCFunction ) Texture_oldsetSType, METH_VARARGS,
	 "(s) - Set Texture stype"},
	{"setType", ( PyCFunction ) Texture_oldsetType, METH_VARARGS,
	 "(s) - Set Texture type"},
	{"setNoiseBasis", ( PyCFunction ) Texture_oldsetNoiseBasis, METH_VARARGS,
	 "(s) - Set Noise basis"},
	{"setDistNoise", ( PyCFunction ) Texture_oldsetDistNoise, METH_VARARGS,
	 "(s) - Set Dist Noise"},
	{"setDistMetric", ( PyCFunction ) Texture_oldsetDistMetric, METH_VARARGS,
	 "(s) - Set Dist Metric"},
	{"evaluate", ( PyCFunction ) Texture_evaluate, METH_O,
	 "(vector) - evaluate the texture at this position"},
	{"__copy__", ( PyCFunction ) Texture_copy, METH_NOARGS,
	 "() - return a copy of the the texture"},
	{"copy", ( PyCFunction ) Texture_copy, METH_NOARGS,
	 "() - return a copy of the the texture"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Texture_Type attributes get/set structure:                         */
/*****************************************************************************/
static PyGetSetDef BPy_Texture_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"animFrames",
	 (getter)Texture_getAnimFrames, (setter)Texture_setAnimFrames,
	 "Number of frames of a movie to use",
	 NULL},
#if 0
	{"animLength",
	 (getter)Texture_getAnimLength, (setter)Texture_setAnimLength,
	 "Number of frames of a movie to use (0 for all)",
	 NULL},
	{"animMontage",
	 (getter)Texture_getAnimMontage, (setter)Texture_setAnimMontage,
	 "Montage mode, start frames and durations",
	 NULL},
#endif
	{"animOffset",
	 (getter)Texture_getAnimOffset, (setter)Texture_setAnimOffset,
	 "Offsets the number of the first movie frame to use",
	 NULL},
	{"animStart",
	 (getter)Texture_getAnimStart, (setter)Texture_setAnimStart,
	 "Starting frame of the movie to use",
	 NULL},
	{"brightness",
	 (getter)Texture_getBrightness, (setter)Texture_setBrightness,
	 "Changes the brightness of a texture's color",
	 NULL},
	{"contrast",
	 (getter)Texture_getContrast, (setter)Texture_setContrast,
	 "Changes the contrast of a texture's color",
	 NULL},
	{"crop",
	 (getter)Texture_getCrop, (setter)Texture_setCrop,
	 "Sets the cropping extents (for image textures)",
	 NULL},
	{"distAmnt",
	 (getter)Texture_getDistAmnt, (setter)Texture_setDistAmnt,
	 "Amount of distortion (for distorted noise textures)",
	 NULL},
	{"distMetric",
	 (getter)Texture_getDistMetric, (setter)Texture_setDistMetric,
	 "The distance metric (for Voronoi textures)",
	 NULL},
	{"exp",
	 (getter)Texture_getExp, (setter)Texture_setExp,
	 "Minkovsky exponent (for Minkovsky Voronoi textures)",
	 NULL},
	{"extend",
	 (getter)Texture_getIntExtend, (setter)Texture_setIntExtend,
	 "Texture's 'Extend' mode (for image textures)",
	 NULL},
	{"fieldsPerImage",
	 (getter)Texture_getFieldsPerImage, (setter)Texture_setFieldsPerImage,
	 "Number of fields per rendered frame",
	 NULL},
	{"filterSize",
	 (getter)Texture_getFilterSize, (setter)Texture_setFilterSize,
	 "The filter size (for image and envmap textures)",
	 NULL},
	{"flags",
	 (getter)Texture_getFlags, (setter)Texture_setFlags,
	 "Texture's 'Flag' bits",
	 NULL},
	{"hFracDim",
	 (getter)Texture_getHFracDim, (setter)Texture_setHFracDim,
	 "Highest fractional dimension (for Musgrave textures)",
	 NULL},
	{"imageFlags",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Texture's 'ImageFlags' bits",
	 NULL},
	{"image",
	 (getter)Texture_getImage, (setter)Texture_setImage,
	 "Texture's image object",
	 NULL},
	{"ipo",
	 (getter)Texture_getIpo, (setter)Texture_setIpo,
	 "Texture Ipo data",
	 NULL},
	{"iScale",
	 (getter)Texture_getIScale, (setter)Texture_setIScale,
	 "Intensity output scale (for Musgrave and Voronoi textures)",
	 NULL},
	{"lacunarity",
	 (getter)Texture_getLacunarity, (setter)Texture_setLacunarity,
	 "Gap between succesive frequencies (for Musgrave textures)",
	 NULL},
	{"noiseBasis",
	 (getter)Texture_getNoiseBasis, (setter)Texture_setNoiseBasis,
	 "Noise basis type (wood, stucci, marble, clouds, Musgrave, distorted noise)",
	 NULL},
	{"noiseBasis2",
	 (getter)Texture_getNoiseBasis2, (setter)Texture_setNoiseBasis2,
	 "Additional noise basis type (wood, marble, distorted noise)",
	 (void *)EXPP_TEX_NOISEBASIS2},
	{"noiseDepth",
	 (getter)Texture_getNoiseDepth, (setter)Texture_setNoiseDepth,
	 "Noise depth (magic, marble, clouds)",
	 NULL},
	{"noiseSize",
	 (getter)Texture_getNoiseSize, (setter)Texture_setNoiseSize,
	 "Noise size (wood, stucci, marble, clouds, Musgrave, distorted noise, Voronoi)",
	 NULL},
/* NOTE for API rewrite: should use dict constants instead of strings */
	{"noiseType",
	 (getter)Texture_getNoiseType, (setter)Texture_setNoiseType,
	 "Noise type (for wood, stucci, marble, clouds textures)",
	 NULL},
	{"octs",
	 (getter)Texture_getOcts, (setter)Texture_setOcts,
	 "Number of frequencies (for Musgrave textures)",
	 NULL},
	{"repeat",
	 (getter)Texture_getRepeat, (setter)Texture_setRepeat,
	 "Repetition multiplier (for image textures)",
	 NULL},
	{"rgbCol",
	 (getter)Texture_getRGBCol, (setter)Texture_setRGBCol,
	 "RGB color tuple",
	 NULL},
	{"stype",
	 (getter)Texture_getSType, (setter)Texture_setSType,
	 "Texture's 'SType' mode",
	 NULL},
	{"turbulence",
	 (getter)Texture_getTurbulence, (setter)Texture_setTurbulence,
	 "Turbulence (for magic, wood, stucci, marble textures)",
	 NULL},
	{"type",
	 (getter)Texture_getType, (setter)Texture_setType,
	 "Texture's 'Type' mode",
	 NULL},
	{"weight1",
	 (getter)Texture_getWeight1, (setter)Texture_setWeight1,
	 "Weight 1 (for Voronoi textures)",
	 NULL},
	{"weight2",
	 (getter)Texture_getWeight2, (setter)Texture_setWeight2,
	 "Weight 2 (for Voronoi textures)",
	 NULL},
	{"weight3",
	 (getter)Texture_getWeight3, (setter)Texture_setWeight3,
	 "Weight 3 (for Voronoi textures)",
	 NULL},
	{"weight4",
	 (getter)Texture_getWeight4, (setter)Texture_setWeight4,
	 "Weight 4 (for Voronoi textures)",
	 NULL},
	{"sine",
	 (getter)Texture_getNoiseBasis2, (setter)Texture_setNoiseBasis2,
	 "Produce bands using sine wave (marble, wood textures)",
	 (void *)EXPP_TEX_NOISE_SINE},
	{"saw",
	 (getter)Texture_getNoiseBasis2, (setter)Texture_setNoiseBasis2,
	 "Produce bands using saw wave (marble, wood textures)",
	 (void *)EXPP_TEX_NOISE_SAW},
	{"tri",
	 (getter)Texture_getNoiseBasis2, (setter)Texture_setNoiseBasis2,
	 "Produce bands using triangle wave (marble, wood textures)",
	 (void *)EXPP_TEX_NOISE_TRI},
	{"interpol",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Interpolate image's pixels to fit texture mapping enabled ('ImageFlags')",
	 (void *)TEX_INTERPOL},
	{"useAlpha",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Use of image's alpha channel enabled ('ImageFlags')",
	 (void *)TEX_USEALPHA},
	{"calcAlpha",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Calculation of image's alpha channel enabled ('ImageFlags')",
	 (void *)TEX_CALCALPHA},
	{"mipmap",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Mipmaps enabled ('ImageFlags')",
	 (void *)TEX_MIPMAP},
	{"rot90",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "X/Y flip for rendering enabled ('ImageFlags')",
	 (void *)TEX_IMAROT},
	{"autoRefresh",
	 (getter)Texture_getIUserFlags, (setter)Texture_setIUserFlags,
	 "Refresh image on frame changes enabled",
	 (void *)IMA_ANIM_ALWAYS},
	{"cyclic",
	 (getter)Texture_getIUserCyclic, (setter)Texture_setIUserCyclic,
	 "Cycling of animated frames enabled",
	 NULL},
#if 0
	/* disabled, moved to image */
	{"fields",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Use of image's fields enabled ('ImageFlags')",
	 (void *)TEX_FIELDS},
	{"movie",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Movie frames as images enabled ('ImageFlags')",
	 (void *)TEX_ANIM5},
	{"anti",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Image anti-aliasing enabled ('ImageFlags')",
	 (void *)TEX_ANTIALI},
	{"stField",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Standard field deinterlacing enabled ('ImageFlags')",
	 (void *)TEX_STD_FIELD},
#endif
	{"normalMap",
	 (getter)Texture_getImageFlags, (setter)Texture_setImageFlags,
	 "Use of image RGB values for normal mapping enabled ('ImageFlags')",
	 (void *)TEX_NORMALMAP},
	{"colorband",
	 (getter)Texture_getColorband, (setter)Texture_setColorband,
	 "The colorband for this texture",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Texture_Type callback function prototypes:                         */
/*****************************************************************************/
static int Texture_compare( BPy_Texture * a, BPy_Texture * b );
static PyObject *Texture_repr( BPy_Texture * self );

/*****************************************************************************/
/* Python Texture_Type structure definition:                                 */
/*****************************************************************************/
PyTypeObject Texture_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Texture",          /* char *tp_name; */
	sizeof( BPy_Texture ),      /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Texture_compare, /* cmpfunc tp_compare; */
	( reprfunc ) Texture_repr,  /* reprfunc tp_repr; */

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
	BPy_Texture_methods,        /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Texture_getseters,      /* struct PyGetSetDef *tp_getset; */
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

static PyObject *M_Texture_New( PyObject * self, PyObject * args,
				PyObject * kwords )
{
	char *name_str = "Tex";
	static char *kwlist[] = { "name_str", NULL };
	PyObject *pytex;	/* for Texture object wrapper in Python */
	Tex *bltex;		/* for actual Tex we create in Blender */

	/* Parse the arguments passed in by the Python interpreter */
	if( !PyArg_ParseTupleAndKeywords
	    ( args, kwords, "|s", kwlist, &name_str ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected zero, one or two strings as arguments" );

	bltex = add_texture( name_str );  /* first create the texture in Blender */

	if( bltex )		/* now create the wrapper obj in Python */
		pytex = Texture_CreatePyObject( bltex );
	else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't create Texture in Blender" );

	/* let's return user count to zero, because add_texture() incref'd it */
	bltex->id.us = 0;

	if( pytex == NULL )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create Tex PyObject" );

	return pytex;
}

static PyObject *M_Texture_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Tex *tex_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument (or nothing)" );

	tex_iter = G.main->tex.first;

	if( name ) {		/* (name) - Search for texture by name */

		PyObject *wanted_tex = NULL;

		while( tex_iter ) {
			if( STREQ( name, tex_iter->id.name + 2 ) ) {
				wanted_tex =
					Texture_CreatePyObject( tex_iter );
				break;
			}

			tex_iter = tex_iter->id.next;
		}

		if( !wanted_tex ) {	/* Requested texture doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Texture \"%s\" not found", name );
			return EXPP_ReturnPyObjError( PyExc_NameError,
						      error_msg );
		}

		return wanted_tex;
	}

	else {			/* () - return a list of wrappers for all textures in the scene */
		int index = 0;
		PyObject *tex_pylist, *pyobj;

		tex_pylist = PyList_New( BLI_countlist( &( G.main->tex ) ) );
		if( !tex_pylist )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
						      "couldn't create PyList" );

		while( tex_iter ) {
			pyobj = Texture_CreatePyObject( tex_iter );
			if( !pyobj ) {
				Py_DECREF(tex_pylist);
				return EXPP_ReturnPyObjError
					( PyExc_MemoryError,
					  "couldn't create Texture PyObject" );
			}
			PyList_SET_ITEM( tex_pylist, index, pyobj );

			tex_iter = tex_iter->id.next;
			index++;
		}

		return tex_pylist;
	}
}

static int Texture_compare( BPy_Texture * a, BPy_Texture * b )
{
	return ( a->texture == b->texture ) ? 0 : -1;
}

static PyObject *Texture_repr( BPy_Texture * self )
{
	return PyString_FromFormat( "[Texture \"%s\"]",
				    self->texture->id.name + 2 );
}

static PyObject *M_Texture_TypesDict( void )
{
	PyObject *Types = PyConstant_New(  );
	if( Types ) {
		BPy_constant *d = ( BPy_constant * ) Types;
		PyConstant_Insert(d, "NONE", PyInt_FromLong(EXPP_TEX_TYPE_NONE));
		PyConstant_Insert(d, "CLOUDS", PyInt_FromLong(TEX_CLOUDS));
		PyConstant_Insert(d, "WOOD", PyInt_FromLong(TEX_WOOD));
		PyConstant_Insert(d, "MARBLE", PyInt_FromLong(TEX_MARBLE));
		PyConstant_Insert(d, "MAGIC", PyInt_FromLong(TEX_MAGIC));
		PyConstant_Insert(d, "BLEND", PyInt_FromLong(TEX_BLEND));
		PyConstant_Insert(d, "STUCCI", PyInt_FromLong(TEX_STUCCI));
		PyConstant_Insert(d, "NOISE", PyInt_FromLong(TEX_NOISE));
		PyConstant_Insert(d, "IMAGE", PyInt_FromLong(TEX_IMAGE));
		PyConstant_Insert(d, "PLUGIN", PyInt_FromLong(TEX_PLUGIN));
		PyConstant_Insert(d, "ENVMAP", PyInt_FromLong(TEX_ENVMAP));
		PyConstant_Insert(d, "MUSGRAVE", PyInt_FromLong(TEX_MUSGRAVE));
		PyConstant_Insert(d, "VORONOI", PyInt_FromLong(TEX_VORONOI));
		PyConstant_Insert(d, "DISTNOISE", PyInt_FromLong(TEX_DISTNOISE)); 
	}
	return Types;
}

static PyObject *M_Texture_STypesDict( void )
{
	PyObject *STypes = PyConstant_New(  );
	if( STypes ) {
		BPy_constant *d = ( BPy_constant * ) STypes;

		PyConstant_Insert(d, "CLD_DEFAULT",
					PyInt_FromLong(EXPP_TEX_STYPE_CLD_DEFAULT));
		PyConstant_Insert(d, "CLD_COLOR",
					PyInt_FromLong(EXPP_TEX_STYPE_CLD_COLOR));
		PyConstant_Insert(d, "WOD_BANDS",
					PyInt_FromLong(EXPP_TEX_STYPE_WOD_BANDS));
		PyConstant_Insert(d, "WOD_RINGS",
					PyInt_FromLong(EXPP_TEX_STYPE_WOD_RINGS));
		PyConstant_Insert(d, "WOD_BANDNOISE",
					PyInt_FromLong(EXPP_TEX_STYPE_WOD_BANDNOISE));
		PyConstant_Insert(d, "WOD_RINGNOISE",
					PyInt_FromLong(EXPP_TEX_STYPE_WOD_RINGNOISE));
		PyConstant_Insert(d, "MAG_DEFAULT",
					PyInt_FromLong(EXPP_TEX_STYPE_MAG_DEFAULT));
		PyConstant_Insert(d, "MBL_SOFT",
					PyInt_FromLong(EXPP_TEX_STYPE_MBL_SOFT));
		PyConstant_Insert(d, "MBL_SHARP",
					PyInt_FromLong(EXPP_TEX_STYPE_MBL_SHARP));
		PyConstant_Insert(d, "MBL_SHARPER",
					PyInt_FromLong(EXPP_TEX_STYPE_MBL_SHARPER));
		PyConstant_Insert(d, "BLN_LIN",
					PyInt_FromLong(EXPP_TEX_STYPE_BLN_LIN));
		PyConstant_Insert(d, "BLN_QUAD",
					PyInt_FromLong(EXPP_TEX_STYPE_BLN_QUAD));
		PyConstant_Insert(d, "BLN_EASE",
					PyInt_FromLong(EXPP_TEX_STYPE_BLN_EASE));
		PyConstant_Insert(d, "BLN_DIAG",
					PyInt_FromLong(EXPP_TEX_STYPE_BLN_DIAG));
		PyConstant_Insert(d, "BLN_SPHERE",
					PyInt_FromLong(EXPP_TEX_STYPE_BLN_SPHERE));
		PyConstant_Insert(d, "BLN_HALO",
					PyInt_FromLong(EXPP_TEX_STYPE_BLN_HALO));
		PyConstant_Insert(d, "STC_PLASTIC",
					PyInt_FromLong(EXPP_TEX_STYPE_STC_PLASTIC));
		PyConstant_Insert(d, "STC_WALLIN",
					PyInt_FromLong(EXPP_TEX_STYPE_STC_WALLIN));
		PyConstant_Insert(d, "STC_WALLOUT",
					PyInt_FromLong(EXPP_TEX_STYPE_STC_WALLOUT));
		PyConstant_Insert(d, "NSE_DEFAULT",
					PyInt_FromLong(EXPP_TEX_STYPE_NSE_DEFAULT));
		PyConstant_Insert(d, "IMG_DEFAULT",
					PyInt_FromLong(EXPP_TEX_STYPE_IMG_DEFAULT));
		PyConstant_Insert(d, "PLG_DEFAULT",
					PyInt_FromLong(EXPP_TEX_STYPE_PLG_DEFAULT));
		PyConstant_Insert(d, "ENV_STATIC",
					PyInt_FromLong(EXPP_TEX_STYPE_ENV_STATIC));
		PyConstant_Insert(d, "ENV_ANIM",
					PyInt_FromLong(EXPP_TEX_STYPE_ENV_ANIM));
		PyConstant_Insert(d, "ENV_LOAD",
					PyInt_FromLong(EXPP_TEX_STYPE_ENV_LOAD));
		PyConstant_Insert(d, "MUS_MFRACTAL",
					PyInt_FromLong(EXPP_TEX_STYPE_MUS_MFRACTAL));
		PyConstant_Insert(d, "MUS_RIDGEDMF",
					PyInt_FromLong(EXPP_TEX_STYPE_MUS_RIDGEDMF));
		PyConstant_Insert(d, "MUS_HYBRIDMF",
					PyInt_FromLong(EXPP_TEX_STYPE_MUS_HYBRIDMF));
		PyConstant_Insert(d, "MUS_FBM",
					PyInt_FromLong(EXPP_TEX_STYPE_MUS_FBM));
		PyConstant_Insert(d, "MUS_HTERRAIN",
					PyInt_FromLong(EXPP_TEX_STYPE_MUS_HTERRAIN));
		PyConstant_Insert(d, "DN_BLENDER",
					PyInt_FromLong(TEX_BLENDER));
		PyConstant_Insert(d, "DN_PERLIN",
					PyInt_FromLong(TEX_STDPERLIN));
		PyConstant_Insert(d, "DN_IMPROVEDPERLIN",
					PyInt_FromLong(TEX_NEWPERLIN));
		PyConstant_Insert(d, "DN_VORONOIF1",
					PyInt_FromLong(TEX_VORONOI_F1));
		PyConstant_Insert(d, "DN_VORONOIF2",
					PyInt_FromLong(TEX_VORONOI_F2));
		PyConstant_Insert(d, "DN_VORONOIF3",
					PyInt_FromLong(TEX_VORONOI_F3));
		PyConstant_Insert(d, "DN_VORONOIF4",
					PyInt_FromLong(TEX_VORONOI_F4));
		PyConstant_Insert(d, "DN_VORONOIF2F1",
					PyInt_FromLong(TEX_VORONOI_F2F1));
		PyConstant_Insert(d, "DN_VORONOICRACKLE",
					PyInt_FromLong(TEX_VORONOI_CRACKLE));
		PyConstant_Insert(d, "DN_CELLNOISE",
					PyInt_FromLong(TEX_CELLNOISE));
		PyConstant_Insert(d, "VN_INT",
					PyInt_FromLong(EXPP_TEX_STYPE_VN_INT));
		PyConstant_Insert(d, "VN_COL1",
					PyInt_FromLong(EXPP_TEX_STYPE_VN_COL1));
		PyConstant_Insert(d, "VN_COL2",
					PyInt_FromLong(EXPP_TEX_STYPE_VN_COL2));
		PyConstant_Insert(d, "VN_COL3",
					PyInt_FromLong(EXPP_TEX_STYPE_VN_COL3));
		PyConstant_Insert(d, "VN_TEX_DISTANCE",
					PyInt_FromLong(TEX_DISTANCE));
		PyConstant_Insert(d, "VN_TEX_DISTANCE_SQUARED",
					PyInt_FromLong(TEX_DISTANCE_SQUARED));
		PyConstant_Insert(d, "VN_TEX_MANHATTAN",
					PyInt_FromLong(TEX_MANHATTAN));
		PyConstant_Insert(d, "VN_TEX_CHEBYCHEV",
					PyInt_FromLong(TEX_CHEBYCHEV));
		PyConstant_Insert(d, "VN_TEX_MINKOVSKY_HALF",
					PyInt_FromLong(TEX_MINKOVSKY_HALF));
		PyConstant_Insert(d, "VN_TEX_MINKOVSKY_FOUR",
					PyInt_FromLong(TEX_MINKOVSKY_FOUR));
		PyConstant_Insert(d, "VN_TEX_MINKOVSKY",
					PyInt_FromLong(TEX_MINKOVSKY));

	}
	return STypes;
}

static PyObject *M_Texture_TexCoDict( void )
{
	PyObject *TexCo = PyConstant_New(  );
	if( TexCo ) {
		BPy_constant *d = ( BPy_constant * ) TexCo;
		PyConstant_Insert(d, "ORCO", PyInt_FromLong(TEXCO_ORCO));
		PyConstant_Insert(d, "REFL", PyInt_FromLong(TEXCO_REFL));
		PyConstant_Insert(d, "NOR", PyInt_FromLong(TEXCO_NORM));
		PyConstant_Insert(d, "GLOB", PyInt_FromLong(TEXCO_GLOB));
		PyConstant_Insert(d, "UV", PyInt_FromLong(TEXCO_UV));
		PyConstant_Insert(d, "OBJECT", PyInt_FromLong(TEXCO_OBJECT));
		PyConstant_Insert(d, "WIN", PyInt_FromLong(TEXCO_WINDOW));
		PyConstant_Insert(d, "VIEW", PyInt_FromLong(TEXCO_VIEW));
		PyConstant_Insert(d, "STICK", PyInt_FromLong(TEXCO_STICKY));
		PyConstant_Insert(d, "STRESS", PyInt_FromLong(TEXCO_STRESS));
		PyConstant_Insert(d, "TANGENT", PyInt_FromLong(TEXCO_TANGENT));
	}
	return TexCo;
}

static PyObject *M_Texture_MapToDict( void )
{
	PyObject *MapTo = PyConstant_New(  );
	if( MapTo ) {
		BPy_constant *d = ( BPy_constant * ) MapTo;
		PyConstant_Insert(d, "COL", PyInt_FromLong(MAP_COL));
		PyConstant_Insert(d, "NOR", PyInt_FromLong(MAP_NORM));
		PyConstant_Insert(d, "CSP", PyInt_FromLong(MAP_COLSPEC));
		PyConstant_Insert(d, "CMIR", PyInt_FromLong(MAP_COLMIR));
		PyConstant_Insert(d, "REF", PyInt_FromLong(MAP_REF));
		PyConstant_Insert(d, "SPEC", PyInt_FromLong(MAP_SPEC));
		PyConstant_Insert(d, "HARD", PyInt_FromLong(MAP_HAR));
		PyConstant_Insert(d, "ALPHA", PyInt_FromLong(MAP_ALPHA));
		PyConstant_Insert(d, "EMIT", PyInt_FromLong(MAP_EMIT));
		PyConstant_Insert(d, "RAYMIR", PyInt_FromLong(MAP_RAYMIRR));
		PyConstant_Insert(d, "AMB", PyInt_FromLong(MAP_AMB));
		PyConstant_Insert(d, "TRANSLU", PyInt_FromLong(MAP_TRANSLU));
		PyConstant_Insert(d, "DISP", PyInt_FromLong(MAP_DISPLACE));
		PyConstant_Insert(d, "WARP", PyInt_FromLong(MAP_WARP));
	}
	return MapTo;
}

static PyObject *M_Texture_FlagsDict( void )
{
	PyObject *Flags = PyConstant_New(  );
	if( Flags ) {
		BPy_constant *d = ( BPy_constant * ) Flags;
		PyConstant_Insert(d, "COLORBAND", PyInt_FromLong(TEX_COLORBAND));
		PyConstant_Insert(d, "FLIPBLEND", PyInt_FromLong(TEX_FLIPBLEND));
		PyConstant_Insert(d, "NEGALPHA", PyInt_FromLong(TEX_NEGALPHA));
		PyConstant_Insert(d, "CHECKER_ODD", PyInt_FromLong(TEX_CHECKER_ODD)); 
		PyConstant_Insert(d, "CHECKER_EVEN", PyInt_FromLong(TEX_CHECKER_EVEN));
		PyConstant_Insert(d, "PREVIEW_ALPHA", PyInt_FromLong(TEX_PRV_ALPHA));
		PyConstant_Insert(d, "REPEAT_XMIR", PyInt_FromLong(TEX_REPEAT_XMIR));
		PyConstant_Insert(d, "REPEAT_YMIR", PyInt_FromLong(TEX_REPEAT_YMIR));
	}
	return Flags;
}

static PyObject *M_Texture_ExtendModesDict( void )
{
	PyObject *ExtendModes = PyConstant_New(  );
	if( ExtendModes ) {
		BPy_constant *d = ( BPy_constant * ) ExtendModes;
		PyConstant_Insert(d, "EXTEND", PyInt_FromLong(TEX_EXTEND));
		PyConstant_Insert(d, "CLIP", PyInt_FromLong(TEX_CLIP));
		PyConstant_Insert(d, "CLIPCUBE", PyInt_FromLong(TEX_CLIPCUBE));
		PyConstant_Insert(d, "REPEAT", PyInt_FromLong(TEX_REPEAT));
	}
	return ExtendModes;
}

static PyObject *M_Texture_ImageFlagsDict( void )
{
	PyObject *ImageFlags = PyConstant_New(  );
	if( ImageFlags ) {
		BPy_constant *d = ( BPy_constant * ) ImageFlags;
		PyConstant_Insert(d, "INTERPOL", PyInt_FromLong(TEX_INTERPOL));
		PyConstant_Insert(d, "USEALPHA", PyInt_FromLong(TEX_USEALPHA));
		PyConstant_Insert(d, "MIPMAP", PyInt_FromLong(TEX_MIPMAP));
		PyConstant_Insert(d, "ROT90", PyInt_FromLong(TEX_IMAROT));
		PyConstant_Insert(d, "CALCALPHA", PyInt_FromLong(TEX_CALCALPHA));
		PyConstant_Insert(d, "NORMALMAP", PyInt_FromLong(TEX_NORMALMAP));
	}
	return ImageFlags;
}

static PyObject *M_Texture_NoiseDict( void )
{
	PyObject *Noise = PyConstant_New(  );
	if( Noise ) {
		BPy_constant *d = ( BPy_constant * ) Noise;
		PyConstant_Insert(d, "SINE", PyInt_FromLong(EXPP_TEX_NOISE_SINE));
		PyConstant_Insert(d, "SAW", PyInt_FromLong(EXPP_TEX_NOISE_SAW));
		PyConstant_Insert(d, "TRI", PyInt_FromLong(EXPP_TEX_NOISE_TRI));
		PyConstant_Insert(d, "BLENDER", PyInt_FromLong(TEX_BLENDER));
		PyConstant_Insert(d, "PERLIN", PyInt_FromLong(TEX_STDPERLIN));
		PyConstant_Insert(d, "IMPROVEDPERLIN", PyInt_FromLong(TEX_NEWPERLIN));
		PyConstant_Insert(d, "VORONOIF1", PyInt_FromLong(TEX_VORONOI_F1));
		PyConstant_Insert(d, "VORONOIF2", PyInt_FromLong(TEX_VORONOI_F2));
		PyConstant_Insert(d, "VORONOIF3", PyInt_FromLong(TEX_VORONOI_F3));
		PyConstant_Insert(d, "VORONOIF4", PyInt_FromLong(TEX_VORONOI_F4));
		PyConstant_Insert(d, "VORONOIF2F1", PyInt_FromLong(TEX_VORONOI_F2F1));
		PyConstant_Insert(d, "VORONOICRACKLE",
					PyInt_FromLong(TEX_VORONOI_CRACKLE));
		PyConstant_Insert(d, "CELLNOISE", PyInt_FromLong(TEX_CELLNOISE));
	}
	return Noise;
}

static PyObject *M_Texture_BlendModesDict( void )
{
	PyObject *BlendModes = PyConstant_New(  );
	if( BlendModes ) {
		BPy_constant *d = ( BPy_constant * ) BlendModes;
		PyConstant_Insert(d, "MIX", PyInt_FromLong(MTEX_BLEND));
		PyConstant_Insert(d, "MULTIPLY", PyInt_FromLong(MTEX_MUL));
		PyConstant_Insert(d, "ADD", PyInt_FromLong(MTEX_ADD));
		PyConstant_Insert(d, "SUBTRACT", PyInt_FromLong(MTEX_SUB));
		PyConstant_Insert(d, "DIVIDE", PyInt_FromLong(MTEX_DIV));
		PyConstant_Insert(d, "DARKEN", PyInt_FromLong(MTEX_DARK));
		PyConstant_Insert(d, "DIFFERENCE", PyInt_FromLong(MTEX_DIFF));
		PyConstant_Insert(d, "LIGHTEN", PyInt_FromLong(MTEX_LIGHT));
		PyConstant_Insert(d, "SCREEN", PyInt_FromLong(MTEX_SCREEN));
	}
	return BlendModes;
}

static PyObject *M_Texture_MappingsDict( void )
{
	PyObject *Mappings = PyConstant_New(  );
	if( Mappings ) {
		BPy_constant *d = ( BPy_constant * ) Mappings;
		PyConstant_Insert(d, "FLAT", PyInt_FromLong(MTEX_FLAT));
		PyConstant_Insert(d, "CUBE", PyInt_FromLong(MTEX_CUBE));
		PyConstant_Insert(d, "TUBE", PyInt_FromLong(MTEX_TUBE));
		PyConstant_Insert(d, "SPHERE", PyInt_FromLong(MTEX_SPHERE));
	}
	return Mappings;
}

static PyObject *M_Texture_ProjDict( void )
{
	PyObject *Proj = PyConstant_New(  );
	if( Proj ) {
		BPy_constant *d = ( BPy_constant * ) Proj;
		PyConstant_Insert(d, "NONE", PyInt_FromLong(PROJ_N));
		PyConstant_Insert(d, "X", PyInt_FromLong(PROJ_X));
		PyConstant_Insert(d, "Y", PyInt_FromLong(PROJ_Y));
		PyConstant_Insert(d, "Z", PyInt_FromLong(PROJ_Z));
	}
	return Proj;
}

PyObject *Texture_Init( void )
{
	PyObject *submodule;
	PyObject *dict;

	/* constants */
	PyObject *Types = M_Texture_TypesDict(  );
	PyObject *STypes = M_Texture_STypesDict(  );
	PyObject *TexCo = M_Texture_TexCoDict(  );
	PyObject *MapTo = M_Texture_MapToDict(  );
	PyObject *Flags = M_Texture_FlagsDict(  );
	PyObject *ExtendModes = M_Texture_ExtendModesDict(  );
	PyObject *ImageFlags = M_Texture_ImageFlagsDict(  );
	PyObject *Noise = M_Texture_NoiseDict(  );
	PyObject *BlendModes = M_Texture_BlendModesDict(  );
	PyObject *Mappings = M_Texture_MappingsDict(  );
	PyObject *Proj = M_Texture_ProjDict(  );

	if( PyType_Ready( &Texture_Type ) < 0)
		return NULL;

	submodule = Py_InitModule3( "Blender.Texture",
				    M_Texture_methods, M_Texture_doc );

	if( Types )
		PyModule_AddObject( submodule, "Types", Types );
	if( STypes )
		PyModule_AddObject( submodule, "STypes", STypes );
	if( TexCo )
		PyModule_AddObject( submodule, "TexCo", TexCo );
	if( MapTo )
		PyModule_AddObject( submodule, "MapTo", MapTo );
	if( Flags )
		PyModule_AddObject( submodule, "Flags", Flags );
	if( ExtendModes )
		PyModule_AddObject( submodule, "ExtendModes", ExtendModes );
	if( ImageFlags )
		PyModule_AddObject( submodule, "ImageFlags", ImageFlags );
	if( Noise )
		PyModule_AddObject( submodule, "Noise", Noise );
	if ( BlendModes )
		PyModule_AddObject( submodule, "BlendModes", BlendModes );
	if ( Mappings )
		PyModule_AddObject( submodule, "Mappings", Mappings );
	if ( Proj )
		PyModule_AddObject( submodule, "Proj", Proj );

	/* Add the MTex submodule to this module */
	dict = PyModule_GetDict( submodule );
	PyDict_SetItemString( dict, "MTex", MTex_Init(  ) );

	return submodule;
}

PyObject *Texture_CreatePyObject( Tex * tex )
{
	BPy_Texture *pytex;

	pytex = ( BPy_Texture * ) PyObject_NEW( BPy_Texture, &Texture_Type );
	if( !pytex )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Texture PyObject" );

	pytex->texture = tex;
	return ( PyObject * ) pytex;
}

Tex *Texture_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Texture * ) pyobj )->texture;
}

/*****************************************************************************/
/* Python BPy_Texture methods:                                               */
/*****************************************************************************/

static PyObject *Texture_getExtend( BPy_Texture * self )
{
	const char *extend = NULL;

	if( EXPP_map_getStrVal
	    ( tex_extend_map, self->texture->extend, &extend ) )
		return PyString_FromString( extend );

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "invalid internal extend mode" );
}

static PyObject *Texture_getImage( BPy_Texture * self )
{
	/* we need this to be an IMAGE texture, and we must have an image */
	if( ( self->texture->type == TEX_IMAGE ||
				self->texture->type == TEX_ENVMAP )
			&& self->texture->ima )
		return Image_CreatePyObject( self->texture->ima );

	Py_RETURN_NONE;
}

static PyObject *Texture_oldgetSType( BPy_Texture * self )
{
	const char *stype = NULL;
	int n_stype;

	if( self->texture->type == TEX_VORONOI )
		n_stype = self->texture->vn_coltype;
#if 0
	else if( self->texture->type == TEX_MUSGRAVE )
		n_stype = self->texture->noisebasis;
#endif
	else if( self->texture->type == TEX_ENVMAP )
		n_stype = self->texture->env->stype;
	else 
		n_stype = self->texture->stype;

	if( EXPP_map_getStrVal( tex_stype_map[self->texture->type],
				n_stype, &stype ) )
		return PyString_FromString( stype );

	
	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "invalid texture stype internally" );
}

static PyObject *Texture_oldgetType( BPy_Texture * self )
{
	const char *type = NULL;

	if( EXPP_map_getStrVal( tex_type_map, self->texture->type, &type ) )
		return PyString_FromString( type );
	
	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "invalid texture type internally" );
}

static int Texture_setAnimFrames( BPy_Texture * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->texture->iuser.frames,
								EXPP_TEX_ANIMFRAME_MIN,
								EXPP_TEX_ANIMFRAME_MAX, 'h' );
}

static int Texture_setIUserCyclic( BPy_Texture * self, PyObject * value )
{
	int param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if( param )
		self->texture->iuser.cycl = 1;
	else
		self->texture->iuser.cycl = 0;
	return 0;
}

#if 0
/* this was stupid to begin with! (ton) */
static int Texture_setAnimLength( BPy_Texture * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->texture->len,
								EXPP_TEX_ANIMLEN_MIN,
								EXPP_TEX_ANIMLEN_MAX, 'h' );
}

/* this is too simple to keep supporting? disabled for time being (ton) */
static int Texture_setAnimMontage( BPy_Texture * self, PyObject * value )
{
	int fradur[4][2];
	int i;

	if( !PyArg_ParseTuple( value, "(ii)(ii)(ii)(ii)",
			       &fradur[0][0], &fradur[0][1],
			       &fradur[1][0], &fradur[1][1],
			       &fradur[2][0], &fradur[2][1],
			       &fradur[3][0], &fradur[3][1] ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected a tuple of tuples" );

	for( i = 0; i < 4; ++i ) {
		self->texture->fradur[i][0] = 
			(short)EXPP_ClampInt ( fradur[i][0], EXPP_TEX_ANIMMONSTART_MIN,
								EXPP_TEX_ANIMMONSTART_MAX );
		self->texture->fradur[i][1] = 
			(short)EXPP_ClampInt ( fradur[i][1], EXPP_TEX_ANIMMONDUR_MIN,
								EXPP_TEX_ANIMMONDUR_MAX );
	}

	return 0;
}
#endif

static int Texture_setAnimOffset( BPy_Texture * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->texture->iuser.offset,
								EXPP_TEX_ANIMOFFSET_MIN,
								EXPP_TEX_ANIMOFFSET_MAX, 'h' );
}

static int Texture_setAnimStart( BPy_Texture * self, PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->texture->iuser.sfra,
								EXPP_TEX_ANIMSTART_MIN,
								EXPP_TEX_ANIMSTART_MAX, 'h' );
}

static int Texture_setBrightness( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->bright,
								EXPP_TEX_BRIGHTNESS_MIN,
								EXPP_TEX_BRIGHTNESS_MAX );
}

static int Texture_setContrast( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->contrast,
								EXPP_TEX_CONTRAST_MIN,
								EXPP_TEX_CONTRAST_MAX );
}

static int Texture_setCrop( BPy_Texture * self, PyObject * value )
{
	float crop[4];

	if( !PyArg_ParseTuple( value, "ffff",
			       &crop[0], &crop[1], &crop[2], &crop[3] ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 4 floats" );

	self->texture->cropxmin = EXPP_ClampFloat( crop[0], EXPP_TEX_CROP_MIN,
												EXPP_TEX_CROP_MAX );
	self->texture->cropymin = EXPP_ClampFloat( crop[1], EXPP_TEX_CROP_MIN,
												EXPP_TEX_CROP_MAX );
	self->texture->cropxmax = EXPP_ClampFloat( crop[2], EXPP_TEX_CROP_MIN,
												EXPP_TEX_CROP_MAX );
	self->texture->cropymax = EXPP_ClampFloat( crop[3], EXPP_TEX_CROP_MIN,
												EXPP_TEX_CROP_MAX );

	return 0;
}

static int Texture_setIntExtend( BPy_Texture * self, PyObject * value )
{
	return EXPP_setIValueRange ( value, &self->texture->extend,
								EXPP_TEX_EXTEND_MIN,
								EXPP_TEX_EXTEND_MAX, 'h' );
}

static int Texture_setFieldsPerImage( BPy_Texture * self,
					    PyObject * value )
{
	return EXPP_setIValueClamped ( value, &self->texture->iuser.fie_ima,
								EXPP_TEX_FIEIMA_MIN,
								EXPP_TEX_FIEIMA_MAX, 'h' );

}

static int Texture_setFilterSize( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->filtersize,
								EXPP_TEX_FILTERSIZE_MIN,
								EXPP_TEX_FILTERSIZE_MAX );
}

static int Texture_setFlags( BPy_Texture * self, PyObject * value )
{
	int param;

	if( !PyInt_Check( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%08x", TEX_FLAG_MASK );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = PyInt_AS_LONG ( value );

	if ( ( param & TEX_FLAG_MASK ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	self->texture->flag = (short)param;

#if 0
	/* if Colorband enabled, make sure we allocate memory for it */

	if ( ( param & TEX_COLORBAND ) && !self->texture->coba )
		self->texture->coba = add_colorband();
#endif

	return 0;
}

static int Texture_setImage( BPy_Texture * self, PyObject * value )
{
	Image *blimg = NULL;

	if ( value != Py_None && !BPy_Image_Check (value) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected an Image or None" );


	if( self->texture->ima ) {
		self->texture->ima->id.us--;
		self->texture->ima = NULL;
	}

	if ( value == Py_None )
		return 0;

	blimg = Image_FromPyObject( value );

	self->texture->ima = blimg;
	self->texture->type = TEX_IMAGE;
	BKE_image_signal(blimg, &self->texture->iuser, IMA_SIGNAL_RELOAD );
	id_us_plus( &blimg->id );

	return 0;
}

static int Texture_setImageFlags( BPy_Texture * self, PyObject * value,
									void *type )
{
	short param;

	/*
	 * if type is non-zero, then attribute is "mipmap", "calcAlpha", etc.,
	 * so set/clear the bit in the bitfield based on the type
	 */

	if( GET_INT_FROM_POINTER(type) ) {
		int err;
		param = self->texture->imaflag;
		err = EXPP_setBitfield( value, &param, GET_INT_FROM_POINTER(type), 'h' );
		if( err )
			return err;

	/*
	 * if type is zero, then attribute is "imageFlags", so check
	 * value for a valid bitmap range.
	 */

	} else {
		int bitmask = TEX_INTERPOL
					| TEX_USEALPHA
					| TEX_MIPMAP
					| TEX_IMAROT
					| TEX_CALCALPHA
					| TEX_NORMALMAP;

		if( !PyInt_Check( value ) ) {
			char errstr[128];
			sprintf ( errstr , "expected int bitmask of 0x%08x", bitmask );
			return EXPP_ReturnIntError( PyExc_TypeError, errstr );
		}

		param = (short)PyInt_AS_LONG( value );
		if( ( param & bitmask ) != param )
			return EXPP_ReturnIntError( PyExc_ValueError,
							"invalid bit(s) set in mask" );
	}

	/* everything is OK; save the new flag setting */

	self->texture->imaflag = param;
	return 0;
}

static int Texture_setIUserFlags( BPy_Texture * self, PyObject * value,
									void *flag )
{
	int param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if( param )
		self->texture->iuser.flag |= GET_INT_FROM_POINTER(flag);
	else
		self->texture->iuser.flag &= ~GET_INT_FROM_POINTER(flag);
	return 0;
}

static int Texture_setNoiseDepth( BPy_Texture * self, PyObject * value )
{
	short max = EXPP_TEX_NOISEDEPTH_MAX;

	/* for whatever reason, magic texture has a different max value */

	if( self->texture->type == TEX_MAGIC )
		max = EXPP_TEX_NOISEDEPTH_MAX_MAGIC;

	return EXPP_setIValueClamped ( value, &self->texture->noisedepth,
								EXPP_TEX_NOISEDEPTH_MIN, max, 'h' );
}

static int Texture_setNoiseSize( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->noisesize,
								EXPP_TEX_NOISESIZE_MIN,
								EXPP_TEX_NOISESIZE_MAX );
}

static int Texture_setNoiseType( BPy_Texture * self, PyObject * value )
{
	char *param;

	if( !PyString_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );
	param = PyString_AS_STRING( value );

	if( STREQ( param, "soft" ) )
		self->texture->noisetype = TEX_NOISESOFT;
	else if( STREQ( param, "hard" ) )
		self->texture->noisetype = TEX_NOISEPERL;
	else
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "noise type must be 'soft' or 'hard'" );

	return 0;
}

static int Texture_setNoiseBasis( BPy_Texture * self, PyObject * value )
{
    int param;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, 
				"expected int (see 'Noise' constant dictionary)" );

	param = PyInt_AS_LONG ( value );

	if ( param < TEX_BLENDER
			|| ( param > TEX_VORONOI_CRACKLE
			&& param != TEX_CELLNOISE ) )
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "invalid noise type" );

	self->texture->noisebasis = (short)param;
	return 0;
}

static int Texture_setNoiseBasis2( BPy_Texture * self, PyObject * value,
								void *type )
{
	/*
	 * if type is EXPP_TEX_NOISEBASIS2, then this is the "noiseBasis2"
	 * attribute, so check the range and set the whole value
	 */

	if( GET_INT_FROM_POINTER(type) == EXPP_TEX_NOISEBASIS2 ) {
    	int param;
		if( !PyInt_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected int (see 'Noise' constant dictionary)" );

		param = PyInt_AS_LONG ( value );

		if ( param < TEX_BLENDER
				|| ( param > TEX_VORONOI_CRACKLE
				&& param != TEX_CELLNOISE ) )
			return EXPP_ReturnIntError( PyExc_ValueError,
							  "invalid noise type" );

		self->texture->noisebasis2 = (short)param;

	/*
	 * for other type values, the attribute is "sine", "saw" or "tri", 
	 * so set the noise basis to the supplied type if value is 1
	 */

	} else {
		if( !PyInt_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected int value of 1" );

		if( PyInt_AS_LONG ( value ) != 1 )
			return EXPP_ReturnIntError( PyExc_ValueError,
							  "expected int value of 1" );

		self->texture->noisebasis2 = (short)GET_INT_FROM_POINTER(type);
	}
	return 0;
}

static int Texture_setRepeat( BPy_Texture * self, PyObject * args )
{
	int repeat[2];

	if( !PyArg_ParseTuple( args, "ii", &repeat[0], &repeat[1] ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 2 ints" );

	self->texture->xrepeat = (short)EXPP_ClampInt( repeat[0], EXPP_TEX_REPEAT_MIN,
											EXPP_TEX_REPEAT_MAX );
	self->texture->yrepeat = (short)EXPP_ClampInt( repeat[1], EXPP_TEX_REPEAT_MIN,
											EXPP_TEX_REPEAT_MAX );

	return 0;
}

static int Texture_setRGBCol( BPy_Texture * self, PyObject * args )
{
	float rgb[3];

	if( !PyArg_ParseTuple( args, "fff", &rgb[0], &rgb[1], &rgb[2] ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 3 floats" );

	self->texture->rfac = EXPP_ClampFloat( rgb[0], EXPP_TEX_RGBCOL_MIN,
											EXPP_TEX_RGBCOL_MAX );
	self->texture->gfac = EXPP_ClampFloat( rgb[1], EXPP_TEX_RGBCOL_MIN,
											EXPP_TEX_RGBCOL_MAX );
	self->texture->bfac = EXPP_ClampFloat( rgb[2], EXPP_TEX_RGBCOL_MIN,
											EXPP_TEX_RGBCOL_MAX );

	return 0;
}

static int Texture_setSType( BPy_Texture * self, PyObject * value )
{
	short param;
	const char *dummy = NULL;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected int argument" );

	param = (short)PyInt_AS_LONG ( value );

	/* use the stype map to find out if this is a valid stype for this type *
	 * note that this will allow CLD_COLOR when type is ENVMAP. there's not *
	 * much that we can do about this though.                               */
	if( !EXPP_map_getStrVal
	    ( tex_stype_map[self->texture->type], param, &dummy ) )
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "invalid stype (for this type)" );

	if( self->texture->type == TEX_VORONOI )
		self->texture->vn_coltype = param;
#if 0
	else if( self->texture->type == TEX_MUSGRAVE )
		self->texture->noisebasis = param;
#endif
	else if( self->texture->type == TEX_ENVMAP )
		self->texture->env->stype = param;
	else 
		self->texture->stype = param;

	return 0;
}

static int Texture_setTurbulence( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->turbul,
								EXPP_TEX_TURBULENCE_MIN,
								EXPP_TEX_TURBULENCE_MAX );
}

static int Texture_setHFracDim( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->mg_H,
								EXPP_TEX_MH_G_MIN,
								EXPP_TEX_MH_G_MAX );
}

static int Texture_setLacunarity( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->mg_lacunarity,
								EXPP_TEX_LACUNARITY_MIN,
								EXPP_TEX_LACUNARITY_MAX );
}

static int Texture_setOcts( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->mg_octaves,
								EXPP_TEX_OCTS_MIN,
								EXPP_TEX_OCTS_MAX );
}

static int Texture_setIScale( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->ns_outscale,
								EXPP_TEX_ISCALE_MIN,
								EXPP_TEX_ISCALE_MAX );
}

static int Texture_setType( BPy_Texture * self, PyObject * value )
{
	int err = EXPP_setIValueRange ( value, &self->texture->type,
								EXPP_TEX_TYPE_MIN,
								EXPP_TEX_TYPE_MAX, 'h' );

	/*
	 * if we set the texture OK, and it's a environment map, and
	 * there is no environment map yet, allocate one (code borrowed
	 * from texture_panel_envmap() in source/blender/src/buttons_shading.c)
	 */

	if( !err && self->texture->type == TEX_ENVMAP 
			&& !self->texture->env ) {
		self->texture->env = BKE_add_envmap();
		self->texture->env->object= OBACT;
	}
	return err;
}

static int Texture_setDistMetric( BPy_Texture * self, PyObject * value )
{
#if 0
	char *dist = NULL;

	if( !PyArg_ParseTuple( value, "s", &dist ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	/* can we really trust texture->type? */
	if( self->texture->type == TEX_VORONOI &&
	    !EXPP_map_getShortVal( tex_stype_map[self->texture->type + 2],
				   dist, &self->texture->vn_distm ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "invalid dist metric type" );

	Py_RETURN_NONE;
#else
	return EXPP_setIValueRange ( value, &self->texture->vn_distm,
							TEX_DISTANCE,
							TEX_MINKOVSKY, 'h' );
#endif
}

static int Texture_setExp( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->vn_mexp,
								EXPP_TEX_EXP_MIN,
								EXPP_TEX_EXP_MAX );
}

static int Texture_setWeight1( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->vn_w1,
								EXPP_TEX_WEIGHT1_MIN,
								EXPP_TEX_WEIGHT1_MAX );
}

static int Texture_setWeight2( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->vn_w2,
								EXPP_TEX_WEIGHT2_MIN,
								EXPP_TEX_WEIGHT2_MAX );
}

static int Texture_setWeight3( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->vn_w3,
								EXPP_TEX_WEIGHT3_MIN,
								EXPP_TEX_WEIGHT3_MAX );
}

static int Texture_setWeight4( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->vn_w4,
								EXPP_TEX_WEIGHT4_MIN,
								EXPP_TEX_WEIGHT4_MAX );
}

static int Texture_setDistAmnt( BPy_Texture * self, PyObject * value )
{
	return EXPP_setFloatClamped ( value, &self->texture->dist_amount,
								EXPP_TEX_DISTAMNT_MIN,
								EXPP_TEX_DISTAMNT_MAX );
}

static PyObject *Texture_getIpo( BPy_Texture * self )
{
	struct Ipo *ipo = self->texture->ipo;

	if( !ipo )
		Py_RETURN_NONE;

	return Ipo_CreatePyObject( ipo );
}

/*
 * this should accept a Py_None argument and just delete the Ipo link
 * (as Texture_clearIpo() does)
 */

static int Texture_setIpo( BPy_Texture * self, PyObject * value )
{
	Ipo *ipo = NULL;
	Ipo *oldipo = self->texture->ipo;
	ID *id;

	/* if parameter is not None, check for valid Ipo */

	if ( value != Py_None ) {
		if ( !BPy_Ipo_Check( value ) )
			return EXPP_ReturnIntError( PyExc_RuntimeError,
					      	"expected an Ipo object" );

		ipo = Ipo_FromPyObject( value );

		if( !ipo )
			return EXPP_ReturnIntError( PyExc_RuntimeError,
					      	"null ipo!" );

		if( ipo->blocktype != ID_TE )
			return EXPP_ReturnIntError( PyExc_TypeError,
					      	"Ipo is not a texture data Ipo" );
	}

	/* if already linked to Ipo, delete link */

	if ( oldipo ) {
		id = &oldipo->id;
		if( id->us > 0 )
			id->us--;
	}

	/* assign new Ipo and increment user count, or set to NULL if deleting */

	self->texture->ipo = ipo;
	if ( ipo ) {
		id = &ipo->id;
		id_us_plus(id);
	}

	return 0;
}

static PyObject *Texture_getAnimFrames( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->iuser.frames );
}

static PyObject *Texture_getIUserCyclic( BPy_Texture *self )
{
	if( self->texture->iuser.cycl )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

#if 0
/* disabled. this option was too stupid! (ton) */
static PyObject *Texture_getAnimLength( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->len );
}

static PyObject *Texture_getAnimMontage( BPy_Texture *self )
{	
	return Py_BuildValue( "((i,i),(i,i),(i,i),(i,i))",
						self->texture->fradur[0][0],
						self->texture->fradur[0][1],
						self->texture->fradur[1][0],
						self->texture->fradur[1][1],
						self->texture->fradur[2][0],
						self->texture->fradur[2][1],
						self->texture->fradur[3][0],
						self->texture->fradur[3][1] );
}
#endif

static PyObject *Texture_getAnimOffset( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->iuser.offset );
}

static PyObject *Texture_getAnimStart( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->iuser.sfra );
}

static PyObject *Texture_getBrightness( BPy_Texture *self )
{
	return PyFloat_FromDouble ( self->texture->bright );
}

static PyObject *Texture_getContrast( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->contrast );
}

static PyObject *Texture_getCrop( BPy_Texture *self )
{
	return Py_BuildValue( "(f,f,f,f)",
							self->texture->cropxmin,
							self->texture->cropymin,
							self->texture->cropxmax,
							self->texture->cropymax );
}

static PyObject *Texture_getDistAmnt( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->dist_amount );
}

static PyObject *Texture_getDistMetric( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->vn_distm );
}

static PyObject *Texture_getExp( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->vn_mexp );
}

static PyObject *Texture_getIntExtend( BPy_Texture * self )
{
	return PyInt_FromLong( self->texture->extend );
}

static PyObject *Texture_getFieldsPerImage( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->iuser.fie_ima );
}

static PyObject *Texture_getFilterSize( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->filtersize );
}

static PyObject *Texture_getFlags( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->flag );
}

static PyObject *Texture_getHFracDim( BPy_Texture *self )
{
	return PyInt_FromLong( (long)self->texture->mg_H );
}

static PyObject *Texture_getImageFlags( BPy_Texture *self, void *type )
{
	/* type == 0 means attribute "imageFlags"
	 * other types means attribute "mipmap", "calcAlpha", etc
	 */

	if( GET_INT_FROM_POINTER(type) )
		return EXPP_getBitfield( &self->texture->imaflag, GET_INT_FROM_POINTER(type), 'h' );
	else
		return PyInt_FromLong( self->texture->imaflag );
}

static PyObject *Texture_getIUserFlags( BPy_Texture *self, void *flag )
{
	if( self->texture->iuser.flag & GET_INT_FROM_POINTER(flag) )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static PyObject *Texture_getIScale( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->ns_outscale );
}

static PyObject *Texture_getLacunarity( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->mg_lacunarity );
}

static PyObject *Texture_getNoiseBasis( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->noisebasis );
}

static PyObject *Texture_getNoiseBasis2( BPy_Texture *self, void *type )
{
	/* type == EXPP_TEX_NOISEBASIS2 means attribute "noiseBasis2"
	 * other types means attribute "sine", "saw", or "tri" attribute
	 */

	if( GET_INT_FROM_POINTER(type) == EXPP_TEX_NOISEBASIS2 )
		return PyInt_FromLong( self->texture->noisebasis2 );
	else
		return PyInt_FromLong( ( self->texture->noisebasis2 == GET_INT_FROM_POINTER(type) ) ? 1 : 0 );
}

static PyObject *Texture_getNoiseDepth( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->noisedepth );
}

static PyObject *Texture_getNoiseSize( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->noisesize );
}

static PyObject *Texture_getNoiseType( BPy_Texture *self )
{
	if ( self->texture->noisetype == TEX_NOISESOFT )
		return PyString_FromString( "soft" );
	else
		return PyString_FromString( "hard" );
}

static PyObject *Texture_getOcts( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->mg_octaves );
}

static PyObject *Texture_getRepeat( BPy_Texture *self )
{
	return Py_BuildValue( "(i,i)", self->texture->xrepeat,
									self->texture->yrepeat );
}

static PyObject *Texture_getRGBCol( BPy_Texture *self )
{
	return Py_BuildValue( "(f,f,f)", self->texture->rfac,
									self->texture->gfac, self->texture->bfac );
}

static PyObject *Texture_getSType( BPy_Texture *self )
{
	if( self->texture->type == TEX_VORONOI )
		return PyInt_FromLong( self->texture->vn_coltype );
#if 0
	if( self->texture->type == TEX_MUSGRAVE )
		return PyInt_FromLong( self->texture->noisebasis );
#endif
	if( self->texture->type == TEX_ENVMAP )
		return PyInt_FromLong( self->texture->env->stype );

	return PyInt_FromLong( self->texture->stype );
}

static PyObject *Texture_getTurbulence( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->turbul );
}

static PyObject *Texture_getType( BPy_Texture *self )
{
	return PyInt_FromLong( self->texture->type );
}

static PyObject *Texture_getWeight1( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->vn_w1 );
}

static PyObject *Texture_getWeight2( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->vn_w2 );
}

static PyObject *Texture_getWeight3( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->vn_w3 );
}

static PyObject *Texture_getWeight4( BPy_Texture *self )
{
	return PyFloat_FromDouble( self->texture->vn_w4 );
}

/* #####DEPRECATED###### */

static PyObject *Texture_oldsetImage( BPy_Texture * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
										(setter)Texture_setImage );
}

static PyObject *Texture_oldsetIpo( BPy_Texture * self, PyObject * args )
{
	return EXPP_setterWrapper ( (void *)self, args, (setter)Texture_setIpo );
}

/*
 * clearIpo() returns True/False depending on whether material has an Ipo
 */

static PyObject *Texture_clearIpo( BPy_Texture * self )
{
	/* if Ipo defined, delete it and return true */

	if( self->texture->ipo ) {
		PyObject *value = Py_BuildValue( "(O)", Py_None );
		EXPP_setterWrapper( (void *)self, value, (setter)Texture_setIpo );
		Py_DECREF( value );
		return EXPP_incr_ret_True();
	}
	return EXPP_incr_ret_False(); /* no ipo found */
}

/*
 * these older setter methods take strings as parameters; check the list of
 * strings to figure out which bits to set, then call new attribute setters
 * using the wrapper.
 */

static PyObject *Texture_oldsetFlags( BPy_Texture * self, PyObject * args )
{
	unsigned int i, flag = 0;
	PyObject *value, *error;

	/* check that we're passed a tuple */

	if ( !PyTuple_Check( args ) )
		return EXPP_ReturnPyObjError ( PyExc_AttributeError,
					"expected a tuple of string arguments" );

	/* check each argument for type, find its value */

	for ( i = PyTuple_Size( args ); i-- ; ) {
		short thisflag;
		char * name = PyString_AsString( PyTuple_GET_ITEM( args, i ) );
		if( !name )
			return EXPP_ReturnPyObjError ( PyExc_AttributeError,
					"expected string argument" );

		if( !EXPP_map_getShortVal( tex_flag_map, name, &thisflag ) )
			return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
							"unknown Texture flag argument" ) );

		flag |= thisflag;
	}

	/* build tuple, call wrapper */

	value = Py_BuildValue( "(i)", flag );
	error = EXPP_setterWrapper( (void *)self, value, (setter)Texture_setFlags );
	Py_DECREF ( value );
	return error;
}

/*
 * Texture_oldsetType() and Texture_oldsetExtend()
 *
 * These older setter methods convert a string into an integer setting, so
 * doesn't make sense to try wrapping them.
 */

static PyObject *Texture_oldsetType( BPy_Texture * self, PyObject * args )
{
	char *type = NULL;

	if( !PyArg_ParseTuple( args, "s", &type ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	if( !EXPP_map_getShortVal( tex_type_map, type, &self->texture->type ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "invalid texture type" );

	/*
	 * if we set the texture OK, and it's a environment map, and
	 * there is no environment map yet, allocate one (code borrowed
	 * from texture_panel_envmap() in source/blender/src/buttons_shading.c)
	 */

	if( self->texture->type == TEX_ENVMAP 
			&& !self->texture->env ) {
		self->texture->env = BKE_add_envmap();
		self->texture->env->object= OBACT;
	}

	Py_RETURN_NONE;
}

static PyObject *Texture_oldsetExtend( BPy_Texture * self, PyObject * args )
{
	char *extend = NULL;
	if( !PyArg_ParseTuple( args, "s", &extend ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	if( !EXPP_map_getShortVal
	    ( tex_extend_map, extend, &self->texture->extend ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "invalid extend mode" );

	Py_RETURN_NONE;
}

/*
 * Texture_oldsetNoiseBasis(), Texture_oldsetDistNoise()
 *   Texture_oldsetSType(), Texture_oldsetDistMetric(),
 *   Texture_oldsetImageFlags()
 *
 * these old setter methods behave differently from the attribute
 * setters, so they are left unchanged.
 */

static PyObject *Texture_oldsetNoiseBasis( BPy_Texture * self, PyObject * args )
{
/* NOTE: leave as-is: don't use setterWrapper */
	char *nbasis;

	if( !PyArg_ParseTuple( args, "s", &nbasis ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
						  "expected string argument" );
	if( self->texture->type == TEX_MUSGRAVE &&
	    EXPP_map_getShortVal( tex_stype_map[TEX_DISTNOISE],
				  nbasis, &self->texture->noisebasis ) );
	else if( self->texture->type == TEX_DISTNOISE &&
		 !EXPP_map_getShortVal( tex_stype_map[TEX_DISTNOISE],
					nbasis, &self->texture->noisebasis2 ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "invalid noise basis" );

	Py_RETURN_NONE;
}

static PyObject *Texture_oldsetDistNoise( BPy_Texture * self, PyObject * args )
{
/* NOTE: leave as-is: don't use setterWrapper */
	char *nbasis;

	if( !PyArg_ParseTuple( args, "s", &nbasis ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );
	if( self->texture->type == TEX_DISTNOISE &&
	    !EXPP_map_getShortVal( tex_stype_map[TEX_DISTNOISE],
				   nbasis, &self->texture->noisebasis ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "invalid noise basis" );

	Py_RETURN_NONE;
}

static PyObject *Texture_oldsetSType( BPy_Texture * self, PyObject * args )
{
	char *stype = NULL;
	if( !PyArg_ParseTuple( args, "s", &stype ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	/* can we really trust texture->type? */
	if( ( self->texture->type == TEX_VORONOI &&
	      EXPP_map_getShortVal( tex_stype_map[self->texture->type],
				    stype, &self->texture->vn_coltype ) ) );
#if 0
	else if( ( self->texture->type == TEX_MUSGRAVE &&
		   EXPP_map_getShortVal( tex_stype_map
					 [TEX_DISTNOISE], stype,
					 &self->texture->noisebasis ) ) );
#endif
	else if( ( self->texture->type == TEX_ENVMAP &&
	      EXPP_map_getShortVal( tex_stype_map[self->texture->type],
				    stype, &self->texture->env->stype ) ) );
	else if( !EXPP_map_getShortVal
		 ( tex_stype_map[self->texture->type], stype,
		   &self->texture->stype ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "invalid texture stype" );

	Py_RETURN_NONE;
}

static PyObject *Texture_oldsetDistMetric( BPy_Texture * self, PyObject * args )
{
/* NOTE: leave as-is: don't use setterWrapper */
	char *dist = NULL;

	if( !PyArg_ParseTuple( args, "s", &dist ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );
	/* can we really trust texture->type? */
	if( self->texture->type == TEX_VORONOI &&
	    !EXPP_map_getShortVal( tex_stype_map[self->texture->type + 2],
				   dist, &self->texture->vn_distm ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "invalid dist metric type" );

	Py_RETURN_NONE;
}

static PyObject *Texture_oldsetImageFlags( BPy_Texture * self, PyObject * args )
{
	unsigned int i, flag = 0;

	/* check that we're passed a tuple of no more than 3 args*/

	if( !PyTuple_Check( args ) )
		return EXPP_ReturnPyObjError ( PyExc_AttributeError,
					"expected tuple of string arguments" );

	/* check each argument for type, find its value */

	for( i = PyTuple_Size( args ); i-- ; ) {
		short thisflag;
		char * name = PyString_AsString( PyTuple_GET_ITEM( args, i ) );
		if( !name )
			return EXPP_ReturnPyObjError ( PyExc_AttributeError,
					"expected string argument" );

		if( !EXPP_map_getShortVal( tex_imageflag_map, name, &thisflag ) )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
						      "unknown Texture image flag name" );

		flag |= thisflag;
	}

	self->texture->imaflag = (short)flag;

	Py_RETURN_NONE;
}

static PyObject *Texture_getColorband( BPy_Texture * self)
{
	return EXPP_PyList_fromColorband( self->texture->coba );
}

int Texture_setColorband( BPy_Texture * self, PyObject * value)
{
	return EXPP_Colorband_fromPyList( &self->texture->coba, value );
}

static PyObject *Texture_evaluate( BPy_Texture * self, PyObject * value )
{
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float vec[4];
	/* int rgbnor; dont use now */
	
	if (VectorObject_Check(value)) {
		if(((VectorObject *)value)->size < 3)
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
					"expects a 3D vector object or a tuple of 3 numbers");
		
		/* rgbnor = .. we don't need this now */
		multitex_ext(self->texture, ((VectorObject *)value)->vec, NULL, NULL, 1, &texres);
	} else {
		float vec_in[3];
		if (!PyTuple_Check(value) || PyTuple_Size(value) < 3)
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
					"expects a 3D vector object or a tuple of 3 numbers");
		
		vec_in[0] = PyFloat_AsDouble(PyTuple_GET_ITEM(value, 0));
		vec_in[1] = PyFloat_AsDouble(PyTuple_GET_ITEM(value, 1));
		vec_in[2] = PyFloat_AsDouble(PyTuple_GET_ITEM(value, 2));
		if (PyErr_Occurred())
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
					"expects a 3D vector object or a tuple of 3 numbers");
		
		multitex_ext(self->texture, vec_in, NULL, NULL, 1, &texres);
	}
	vec[0] = texres.tr;
	vec[1] = texres.tg;
	vec[2] = texres.tb;
	vec[3] = texres.tin;
	
	return newVectorObject(vec, 4, Py_NEW);
}

static PyObject *Texture_copy( BPy_Texture * self )
{
	Tex *tex = copy_texture(self->texture );
	tex->id.us = 0;
	return Texture_CreatePyObject(tex);
}
