/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA    02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Alex Mole
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <BKE_texture.h>
#include <BKE_utildefines.h>

#include "MTex.h"
#include "Texture.h"
#include "Image.h"
#include "constant.h"
#include "gen_utils.h"
#include "modules.h"


/*****************************************************************************/
/* Blender.Texture constants                                                 */
/*****************************************************************************/
#define EXPP_TEX_TYPE_NONE                  0
#define EXPP_TEX_TYPE_CLOUDS                TEX_CLOUDS
#define EXPP_TEX_TYPE_WOOD                  TEX_WOOD
#define EXPP_TEX_TYPE_MARBLE                TEX_MARBLE
#define EXPP_TEX_TYPE_MAGIC                 TEX_MAGIC
#define EXPP_TEX_TYPE_BLEND                 TEX_BLEND
#define EXPP_TEX_TYPE_STUCCI                TEX_STUCCI
#define EXPP_TEX_TYPE_NOISE                 TEX_NOISE
#define EXPP_TEX_TYPE_IMAGE                 TEX_IMAGE
#define EXPP_TEX_TYPE_PLUGIN                TEX_PLUGIN
#define EXPP_TEX_TYPE_ENVMAP                TEX_ENVMAP

#define EXPP_TEX_TYPE_MIN                   EXPP_TEX_TYPE_NONE
#define EXPP_TEX_TYPE_MAX                   EXPP_TEX_TYPE_ENVMAP

/* i can't find these defined anywhere- they're just taken from looking at   */
/* the button creation code in source/blender/src/buttons_shading.c          */
#define EXPP_TEX_STYPE_CLD_DEFAULT          0
#define EXPP_TEX_STYPE_CLD_COLOR            1
#define EXPP_TEX_STYPE_WOD_BANDS            0
#define EXPP_TEX_STYPE_WOD_RINGS            1
#define EXPP_TEX_STYPE_WOD_BANDNOISE        2
#define EXPP_TEX_STYPE_WOD_RINGNOISE        3
#define EXPP_TEX_STYPE_MAG_DEFAULT          0
#define EXPP_TEX_STYPE_MBL_SOFT             0
#define EXPP_TEX_STYPE_MBL_SHARP            1
#define EXPP_TEX_STYPE_MBL_SHARPER          2
#define EXPP_TEX_STYPE_BLN_LIN              0
#define EXPP_TEX_STYPE_BLN_QUAD             1
#define EXPP_TEX_STYPE_BLN_EASE             2
#define EXPP_TEX_STYPE_BLN_DIAG             3
#define EXPP_TEX_STYPE_BLN_SPHERE           4
#define EXPP_TEX_STYPE_BLN_HALO             5
#define EXPP_TEX_STYPE_STC_PLASTIC          0
#define EXPP_TEX_STYPE_STC_WALLIN           1
#define EXPP_TEX_STYPE_STC_WALLOUT          2
#define EXPP_TEX_STYPE_NSE_DEFAULT          0
#define EXPP_TEX_STYPE_IMG_DEFAULT          0
#define EXPP_TEX_STYPE_PLG_DEFAULT          0
#define EXPP_TEX_STYPE_ENV_STATIC           0
#define EXPP_TEX_STYPE_ENV_ANIM             1
#define EXPP_TEX_STYPE_ENV_LOAD             2

#define EXPP_TEX_FLAG_COLORBAND             TEX_COLORBAND
#define EXPP_TEX_FLAG_FLIPBLEND             TEX_FLIPBLEND
#define EXPP_TEX_FLAG_NEGALPHA              TEX_NEGALPHA

#define EXPP_TEX_IMAGEFLAG_INTERPOL         TEX_INTERPOL
#define EXPP_TEX_IMAGEFLAG_USEALPHA         TEX_USEALPHA
#define EXPP_TEX_IMAGEFLAG_MIPMAP           TEX_MIPMAP
#define EXPP_TEX_IMAGEFLAG_FIELDS           TEX_FIELDS
#define EXPP_TEX_IMAGEFLAG_ROT90            TEX_IMAROT
#define EXPP_TEX_IMAGEFLAG_CALCALPHA        TEX_CALCALPHA
#define EXPP_TEX_IMAGEFLAG_CYCLIC           TEX_ANIMCYCLIC
#define EXPP_TEX_IMAGEFLAG_MOVIE            TEX_ANIM5
#define EXPP_TEX_IMAGEFLAG_STFIELD          TEX_STD_FIELD
#define EXPP_TEX_IMAGEFLAG_ANTI             TEX_ANTIALI

#define EXPP_TEX_EXTEND_EXTEND              TEX_EXTEND
#define EXPP_TEX_EXTEND_CLIP                TEX_CLIP
#define EXPP_TEX_EXTEND_REPEAT              TEX_REPEAT
#define EXPP_TEX_EXTEND_CLIPCUBE            TEX_CLIPCUBE

#define EXPP_TEX_EXTEND_MIN                 EXPP_TEX_EXTEND_EXTEND
#define EXPP_TEX_EXTEND_MAX                 EXPP_TEX_EXTEND_CLIPCUBE

#define EXPP_TEX_TEXCO_ORCO                 TEXCO_ORCO
#define EXPP_TEX_TEXCO_REFL                 TEXCO_REFL
#define EXPP_TEX_TEXCO_NOR                  TEXCO_NORM
#define EXPP_TEX_TEXCO_GLOB                 TEXCO_GLOB
#define EXPP_TEX_TEXCO_UV                   TEXCO_UV
#define EXPP_TEX_TEXCO_OBJECT               TEXCO_OBJECT
#define EXPP_TEX_TEXCO_WIN                  TEXCO_WINDOW
#define EXPP_TEX_TEXCO_VIEW                 TEXCO_VIEW
#define EXPP_TEX_TEXCO_STICK                TEXCO_STICKY

#define EXPP_TEX_MAPTO_COL                  MAP_COL
#define EXPP_TEX_MAPTO_NOR                  MAP_NORM
#define EXPP_TEX_MAPTO_CSP                  MAP_COLSPEC
#define EXPP_TEX_MAPTO_CMIR                 MAP_COLMIR
#define EXPP_TEX_MAPTO_REF                  MAP_REF
#define EXPP_TEX_MAPTO_SPEC                 MAP_SPEC
#define EXPP_TEX_MAPTO_HARD                 MAP_HAR
#define EXPP_TEX_MAPTO_ALPHA                MAP_ALPHA
#define EXPP_TEX_MAPTO_EMIT                 MAP_EMIT

/****************************************************************************/
/* Texture String->Int maps                                                 */
/****************************************************************************/

static const EXPP_map_pair tex_type_map[] = {
    { "None",   EXPP_TEX_TYPE_NONE },
    { "Clouds", EXPP_TEX_TYPE_CLOUDS },
    { "Wood",   EXPP_TEX_TYPE_WOOD },
    { "Marble", EXPP_TEX_TYPE_MARBLE },
    { "Magic",  EXPP_TEX_TYPE_MAGIC },
    { "Blend",  EXPP_TEX_TYPE_BLEND },
    { "Stucci", EXPP_TEX_TYPE_STUCCI },
    { "Noise",  EXPP_TEX_TYPE_NOISE },
    { "Image",  EXPP_TEX_TYPE_IMAGE },
    { "Plugin", EXPP_TEX_TYPE_PLUGIN },
    { "EnvMap", EXPP_TEX_TYPE_ENVMAP },
    { NULL, 0 }
};

static const EXPP_map_pair tex_flag_map[] = {
    /* we don't support this yet! */
/*    { "ColorBand",  EXPP_TEX_FLAG_COLORBAND },  */
    { "FlipBlend",  EXPP_TEX_FLAG_FLIPBLEND },
    { "NegAlpha",   EXPP_TEX_FLAG_NEGALPHA },
    { NULL, 0 }
};

static const EXPP_map_pair tex_imageflag_map[] = {
    { "InterPol",   EXPP_TEX_IMAGEFLAG_INTERPOL },
    { "UseAlpha",   EXPP_TEX_IMAGEFLAG_USEALPHA },
    { "MipMap",     EXPP_TEX_IMAGEFLAG_MIPMAP },
    { "Fields",     EXPP_TEX_IMAGEFLAG_FIELDS },
    { "Rot90",      EXPP_TEX_IMAGEFLAG_ROT90 },
    { "CalcAlpha",  EXPP_TEX_IMAGEFLAG_CALCALPHA },
    { "Cyclic",     EXPP_TEX_IMAGEFLAG_CYCLIC },
    { "Movie",      EXPP_TEX_IMAGEFLAG_MOVIE },
    { "StField",    EXPP_TEX_IMAGEFLAG_STFIELD },
    { "Anti",       EXPP_TEX_IMAGEFLAG_ANTI },
    { NULL, 0 }
};

static const EXPP_map_pair tex_extend_map[] = {
    { "Extend",     EXPP_TEX_EXTEND_EXTEND },
    { "Clip",       EXPP_TEX_EXTEND_CLIP },
    { "ClipCube",   EXPP_TEX_EXTEND_CLIPCUBE },
    { "Repeat",     EXPP_TEX_EXTEND_REPEAT },
    { NULL, 0 }
};

/* array of maps for stype */
static const EXPP_map_pair tex_stype_default_map[] = { 
    { "Default", 0 }, 
    { NULL, 0 } 
};
static const EXPP_map_pair tex_stype_clouds_map[] = {
    { "Default",        0 }, 
    { "CloudDefault",   EXPP_TEX_STYPE_CLD_DEFAULT },
    { "CloudColor",     EXPP_TEX_STYPE_CLD_COLOR }, 
    { NULL, 0 }
};
static const EXPP_map_pair tex_stype_wood_map[] = {
    { "Default",        0 }, 
    { "WoodBands",      EXPP_TEX_STYPE_WOD_BANDS },
    { "WoodRings",      EXPP_TEX_STYPE_WOD_RINGS },
    { "WoodBandNoise",  EXPP_TEX_STYPE_WOD_BANDNOISE },
    { "WoodRingNoise",  EXPP_TEX_STYPE_WOD_RINGNOISE },
    { NULL, 0 }
};
static const EXPP_map_pair tex_stype_marble_map[] = {
    { "Default",        0 }, 
    { "MarbleSoft",     EXPP_TEX_STYPE_MBL_SOFT },
    { "MarbleSharp",    EXPP_TEX_STYPE_MBL_SHARP },
    { "MarbleSharper",  EXPP_TEX_STYPE_MBL_SHARPER },
    { NULL , 0 }
};
static const EXPP_map_pair tex_stype_blend_map[] = {
    { "Default",        0 }, 
    { "BlendLin",       EXPP_TEX_STYPE_BLN_LIN },
    { "BlendQuad",      EXPP_TEX_STYPE_BLN_QUAD },
    { "BlendEase",      EXPP_TEX_STYPE_BLN_EASE },
    { "BlendDiag",      EXPP_TEX_STYPE_BLN_DIAG },
    { "BlendSphere",    EXPP_TEX_STYPE_BLN_SPHERE },
    { "BlendHalo",      EXPP_TEX_STYPE_BLN_HALO },
    { NULL , 0 }
};
static const EXPP_map_pair tex_stype_stucci_map[] = {
    { "Default",        0 }, 
    { "StucciPlastic",  EXPP_TEX_STYPE_STC_PLASTIC },
    { "StucciWallIn",   EXPP_TEX_STYPE_STC_WALLIN },
    { "StucciWallOut",  EXPP_TEX_STYPE_STC_WALLOUT },
    { NULL , 0 }
};
static const EXPP_map_pair tex_stype_envmap_map[] = {
    { "Default",        0 }, 
    { "EnvmapStatic",   EXPP_TEX_STYPE_ENV_STATIC },
    { "EnvmapAnim",     EXPP_TEX_STYPE_ENV_ANIM },
    { "EnvmapLoad",     EXPP_TEX_STYPE_ENV_LOAD },
    { NULL , 0 }
};

static const EXPP_map_pair *tex_stype_map[] = {
    tex_stype_default_map,    /* none */
    tex_stype_clouds_map,
    tex_stype_wood_map,
    tex_stype_marble_map,
    tex_stype_default_map,    /* magic */
    tex_stype_blend_map,
    tex_stype_stucci_map,
    tex_stype_default_map,    /* noise */
    tex_stype_default_map,    /* image */
    tex_stype_default_map,    /* plugin */
    tex_stype_envmap_map
};


/*****************************************************************************/
/* Python API function prototypes for the Texture module.                    */
/*****************************************************************************/
static PyObject *M_Texture_New (PyObject *self, PyObject *args,
                               PyObject *keywords);
static PyObject *M_Texture_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Texture.__doc__                                                   */
/*****************************************************************************/
static char M_Texture_doc[] =
"The Blender Texture module\n\
\n\
This module provides access to **Texture** objects in Blender\n";

static char M_Texture_New_doc[] =
"Texture.New (name = 'Tex'):\n\
        Return a new Texture object with the given type and name.";

static char M_Texture_Get_doc[] =
"Texture.Get (name = None):\n\
        Return the texture with the given 'name', None if not found, or\n\
        Return a list with all texture objects in the current scene,\n\
        if no argument was given.";

/*****************************************************************************/
/* Python method structure definition for Blender.Texture module:            */
/*****************************************************************************/
struct PyMethodDef M_Texture_methods[] = {
  {"New", (PyCFunction) M_Texture_New,  METH_VARARGS|METH_KEYWORDS, 
                                                        M_Texture_New_doc},
  {"Get",   M_Texture_Get,  METH_VARARGS,               M_Texture_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Texture methods declarations:                                   */
/*****************************************************************************/
#define GETFUNC(name)   static PyObject *Texture_##name(BPy_Texture *self)
#define SETFUNC(name)   static PyObject *Texture_##name(BPy_Texture *self,   \
                                                        PyObject *args)

GETFUNC (getExtend);
GETFUNC (getImage);
GETFUNC (getName);
GETFUNC (getType);
GETFUNC (getSType);
SETFUNC (setAnimFrames);
SETFUNC (setAnimLength);
SETFUNC (setAnimMontage);
SETFUNC (setAnimOffset);
SETFUNC (setAnimStart);
SETFUNC (setBrightness);
SETFUNC (setContrast);
SETFUNC (setCrop);
SETFUNC (setExtend);
SETFUNC (setIntExtend);     /* special case used for ".extend = ..." */
SETFUNC (setFieldsPerImage);
SETFUNC (setFilterSize);
SETFUNC (setFlags);
SETFUNC (setIntFlags);      /* special case used for ".flags = ..." */
SETFUNC (setImage);
SETFUNC (setImageFlags);
SETFUNC (setIntImageFlags); /* special case used for ".imageFlags = ..." */
SETFUNC (setName);
SETFUNC (setNoiseDepth);
SETFUNC (setNoiseSize);
SETFUNC (setNoiseType);
SETFUNC (setRepeat);
SETFUNC (setRGBCol);
SETFUNC (setSType);
SETFUNC (setIntSType);      /* special case used for ".stype = ..." */
SETFUNC (setType);
SETFUNC (setIntType);       /* special case used for ".type = ..." */
SETFUNC (setTurbulence);

/*****************************************************************************/
/* Python BPy_Texture methods table:                                          */
/*****************************************************************************/
static PyMethodDef BPy_Texture_methods[] = {
 /* name, method, flags, doc */
  {"getExtend", (PyCFunction)Texture_getExtend, METH_NOARGS,
                            "() - Return Texture extend mode"},
  {"getImage", (PyCFunction)Texture_getImage, METH_NOARGS,
                            "() - Return Texture Image"},
  {"getName", (PyCFunction)Texture_getName, METH_NOARGS,
                            "() - Return Texture name"},
  {"getSType", (PyCFunction)Texture_getSType, METH_NOARGS,
                            "() - Return Texture stype as string"},
  {"getType", (PyCFunction)Texture_getType, METH_NOARGS,
                            "() - Return Texture type as string"},
  {"setExtend", (PyCFunction)Texture_setExtend, METH_VARARGS,
                            "(s) - Set Texture extend mode"},
  {"setFlags", (PyCFunction)Texture_setFlags, METH_VARARGS,
                            "(f1,f2,f3) - Set Texture flags"},
  {"setImage", (PyCFunction)Texture_setImage, METH_VARARGS,
                            "(Blender Image) - Set Texture Image"},
  {"setImageFlags", (PyCFunction)Texture_setImageFlags, METH_VARARGS,
                            "(s,s,s,s,...) - Set Texture image flags"},
  {"setName", (PyCFunction)Texture_setName, METH_VARARGS,
                            "(s) - Set Texture name"},
  {"setSType", (PyCFunction)Texture_setSType, METH_VARARGS,
                            "(s) - Set Texture stype"},
  {"setType", (PyCFunction)Texture_setType, METH_VARARGS,
                            "(s) - Set Texture type"},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Texture_Type callback function prototypes:                          */
/*****************************************************************************/
static void Texture_dealloc (BPy_Texture *self);
static int Texture_setAttr (BPy_Texture *self, char *name, PyObject *v);
static int Texture_compare (BPy_Texture *a, BPy_Texture *b);
static PyObject *Texture_getAttr (BPy_Texture *self, char *name);
static PyObject *Texture_repr (BPy_Texture *self);


/*****************************************************************************/
/* Python Texture_Type structure definition:                                 */
/*****************************************************************************/
PyTypeObject Texture_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "Blender Texture",              /* tp_name */
    sizeof (BPy_Texture),           /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */
    (destructor)Texture_dealloc,    /* tp_dealloc */
    0,                              /* tp_print */
    (getattrfunc)Texture_getAttr,   /* tp_getattr */
    (setattrfunc)Texture_setAttr,   /* tp_setattr */
    (cmpfunc)Texture_compare,       /* tp_compare */
    (reprfunc)Texture_repr,         /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_as_hash */
    0,0,0,0,0,0,
    0,                              /* tp_doc */ 
    0,0,0,0,0,0,
    BPy_Texture_methods,            /* tp_methods */
    0,                              /* tp_members */
};

static PyObject *M_Texture_New(PyObject *self, PyObject *args, PyObject *kwords)
{
    char        *name_str = "Tex";
    static char *kwlist[] = {"name_str", NULL};
    PyObject    *pytex;     /* for Texture object wrapper in Python */
    Tex         *bltex;     /* for actual Tex we create in Blender */

    /* Parse the arguments passed in by the Python interpreter */
    if (!PyArg_ParseTupleAndKeywords(args, kwords, "|s", kwlist, &name_str))
        return EXPP_ReturnPyObjError(PyExc_AttributeError,
                        "expected zero, one or two strings as arguments");

    bltex = add_texture(name_str);  /* first create the texture in Blender */

    if (bltex)              /* now create the wrapper obj in Python */
        pytex = Texture_CreatePyObject(bltex);
    else
        return EXPP_ReturnPyObjError(PyExc_RuntimeError,
                                    "couldn't create Texture in Blender");

    /* let's return user count to zero, because add_texture() incref'd it */
    bltex->id.us = 0;

    if (pytex == NULL)
        return EXPP_ReturnPyObjError (PyExc_MemoryError,
                                            "couldn't create Tex PyObject");

    return pytex;
}

static PyObject *M_Texture_Get(PyObject *self, PyObject *args)
{
    char    * name = NULL;
    Tex     * tex_iter;

    if (!PyArg_ParseTuple(args, "|s", &name))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                               "expected string argument (or nothing)");

    tex_iter = G.main->tex.first;

    if (name) { /* (name) - Search for texture by name */
        
        PyObject *wanted_tex = NULL;

        while (tex_iter) {            
            if (STREQ(name, tex_iter->id.name+2)) {
                wanted_tex = Texture_CreatePyObject (tex_iter);
                break;
            }

            tex_iter = tex_iter->id.next;
        }

        if (!wanted_tex) { /* Requested texture doesn't exist */
            char error_msg[64];
            PyOS_snprintf(error_msg, sizeof(error_msg),
                                            "Texture \"%s\" not found", name);
            return EXPP_ReturnPyObjError (PyExc_NameError, error_msg);
        }

        return wanted_tex;
    }

    else { /* () - return a list of wrappers for all textures in the scene */
        int index = 0;
        PyObject *tex_pylist, *pyobj;

        tex_pylist = PyList_New (BLI_countlist (&(G.main->tex)));
        if (!tex_pylist)
            return EXPP_ReturnPyObjError(PyExc_MemoryError,
                                            "couldn't create PyList");

        while (tex_iter) {
            pyobj = Texture_CreatePyObject(tex_iter);
            if (!pyobj)
                return EXPP_ReturnPyObjError(PyExc_MemoryError,
                                    "couldn't create Texture PyObject");

            PyList_SET_ITEM(tex_pylist, index, pyobj);

            tex_iter = tex_iter->id.next;
            index++;
        }

        return tex_pylist;
    }
}


#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(d, #name, PyInt_FromLong(EXPP_TEX_TYPE_##name))
    
static PyObject *M_Texture_TypesDict (void)
{
    PyObject *Types = M_constant_New();
    if (Types) {
        BPy_constant *d = (BPy_constant*) Types;
        
        EXPP_ADDCONST (NONE);
        EXPP_ADDCONST (CLOUDS);
        EXPP_ADDCONST (WOOD);
        EXPP_ADDCONST (MARBLE);
        EXPP_ADDCONST (MAGIC);
        EXPP_ADDCONST (BLEND);
        EXPP_ADDCONST (STUCCI);
        EXPP_ADDCONST (NOISE);
        EXPP_ADDCONST (IMAGE);
        EXPP_ADDCONST (PLUGIN);
        EXPP_ADDCONST (ENVMAP);
    }
    return Types;
}

#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(d, #name, PyInt_FromLong(EXPP_TEX_STYPE_##name))
    
static PyObject *M_Texture_STypesDict (void)
{
    PyObject *STypes = M_constant_New();
    if (STypes) {
        BPy_constant *d = (BPy_constant*) STypes;

        EXPP_ADDCONST(CLD_DEFAULT);
        EXPP_ADDCONST(CLD_COLOR);
        EXPP_ADDCONST(WOD_BANDS);
        EXPP_ADDCONST(WOD_RINGS);
        EXPP_ADDCONST(WOD_BANDNOISE);
        EXPP_ADDCONST(WOD_RINGNOISE);
        EXPP_ADDCONST(MAG_DEFAULT);
        EXPP_ADDCONST(MBL_SOFT);
        EXPP_ADDCONST(MBL_SHARP);
        EXPP_ADDCONST(MBL_SHARPER);
        EXPP_ADDCONST(BLN_LIN);
        EXPP_ADDCONST(BLN_QUAD);
        EXPP_ADDCONST(BLN_EASE);
        EXPP_ADDCONST(BLN_DIAG);
        EXPP_ADDCONST(BLN_SPHERE);
        EXPP_ADDCONST(BLN_HALO);
        EXPP_ADDCONST(STC_PLASTIC);
        EXPP_ADDCONST(STC_WALLIN);
        EXPP_ADDCONST(STC_WALLOUT);
        EXPP_ADDCONST(NSE_DEFAULT);
        EXPP_ADDCONST(IMG_DEFAULT);
        EXPP_ADDCONST(PLG_DEFAULT);
        EXPP_ADDCONST(ENV_STATIC);
        EXPP_ADDCONST(ENV_ANIM);
        EXPP_ADDCONST(ENV_LOAD);
    }
    return STypes;
}

#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(d, #name, PyInt_FromLong(EXPP_TEX_TEXCO_##name))
    
static PyObject *M_Texture_TexCoDict (void)
{
    PyObject *TexCo = M_constant_New();
    if (TexCo) {
        BPy_constant *d = (BPy_constant*) TexCo;

        EXPP_ADDCONST(ORCO);
        EXPP_ADDCONST(REFL);
        EXPP_ADDCONST(NOR);
        EXPP_ADDCONST(GLOB);
        EXPP_ADDCONST(UV);
        EXPP_ADDCONST(OBJECT);
        EXPP_ADDCONST(WIN);
        EXPP_ADDCONST(VIEW);
        EXPP_ADDCONST(STICK);
    }
    return TexCo;
}


#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(d, #name, PyInt_FromLong(EXPP_TEX_MAPTO_##name))
    
static PyObject *M_Texture_MapToDict (void)
{
    PyObject *MapTo = M_constant_New();
    if (MapTo) {
        BPy_constant *d = (BPy_constant*) MapTo;

        EXPP_ADDCONST(COL);
        EXPP_ADDCONST(NOR);
        EXPP_ADDCONST(CSP);
        EXPP_ADDCONST(CMIR);
        EXPP_ADDCONST(REF);
        EXPP_ADDCONST(SPEC);
        EXPP_ADDCONST(HARD);
        EXPP_ADDCONST(ALPHA);
        EXPP_ADDCONST(EMIT);
    }
    return MapTo;
}


#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(d, #name, PyInt_FromLong(EXPP_TEX_FLAG_##name))
    
static PyObject *M_Texture_FlagsDict (void)
{
    PyObject *Flags = M_constant_New();
    if (Flags) {
        BPy_constant *d = (BPy_constant*) Flags;

        EXPP_ADDCONST(COLORBAND);
        EXPP_ADDCONST(FLIPBLEND);
        EXPP_ADDCONST(NEGALPHA);
    }
    return Flags;
}


#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(d, #name, PyInt_FromLong(EXPP_TEX_EXTEND_##name))
    
static PyObject *M_Texture_ExtendModesDict (void)
{
    PyObject *ExtendModes = M_constant_New();
    if (ExtendModes) {
        BPy_constant *d = (BPy_constant*) ExtendModes;

        EXPP_ADDCONST(EXTEND);
        EXPP_ADDCONST(CLIP);
        EXPP_ADDCONST(CLIPCUBE);
        EXPP_ADDCONST(REPEAT);
    }
    return ExtendModes;
}


#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(d, #name, PyInt_FromLong(EXPP_TEX_IMAGEFLAG_##name))
    
static PyObject *M_Texture_ImageFlagsDict (void)
{
    PyObject *ImageFlags = M_constant_New();
    if (ImageFlags) {
        BPy_constant *d = (BPy_constant*) ImageFlags;

        EXPP_ADDCONST(INTERPOL);
        EXPP_ADDCONST(USEALPHA);
        EXPP_ADDCONST(MIPMAP);
        EXPP_ADDCONST(FIELDS);
        EXPP_ADDCONST(ROT90);
        EXPP_ADDCONST(CALCALPHA);
        EXPP_ADDCONST(STFIELD);
        EXPP_ADDCONST(MOVIE);
        EXPP_ADDCONST(CYCLIC);
    }
    return ImageFlags;
}


PyObject *Texture_Init (void)
{
    PyObject *submodule;
    PyObject  *dict;
    
    /* constants */
    PyObject *Types = M_Texture_TypesDict();
    PyObject *STypes = M_Texture_STypesDict();
    PyObject *TexCo = M_Texture_TexCoDict();
    PyObject *MapTo = M_Texture_MapToDict();
    PyObject *Flags = M_Texture_FlagsDict();
    PyObject *ExtendModes = M_Texture_ExtendModesDict();
    PyObject *ImageFlags = M_Texture_ImageFlagsDict();
        
    Texture_Type.ob_type = &PyType_Type;

    submodule = Py_InitModule3("Blender.Texture", 
                                        M_Texture_methods, M_Texture_doc);

    if (Types)
        PyModule_AddObject(submodule, "Types", Types);
    if (STypes)
        PyModule_AddObject(submodule, "STypes", STypes);
    if (TexCo)
        PyModule_AddObject(submodule, "TexCo", TexCo);
    if (MapTo)
        PyModule_AddObject(submodule, "MapTo", MapTo);
    if (Flags)
        PyModule_AddObject(submodule, "Flags", Flags);
    if (ExtendModes)
        PyModule_AddObject(submodule, "ExtendModes", ExtendModes);
    if (ImageFlags)
        PyModule_AddObject(submodule, "ImageFlags", ImageFlags);
    
    /* Add the MTex submodule to this module */
    dict = PyModule_GetDict (submodule);
    PyDict_SetItemString (dict, "MTex", MTex_Init());
  
    return submodule;
}

PyObject *Texture_CreatePyObject (Tex *tex)
{
    BPy_Texture *pytex;

    pytex = (BPy_Texture *) PyObject_NEW (BPy_Texture, &Texture_Type);
    if (!pytex)
        return EXPP_ReturnPyObjError (PyExc_MemoryError,
                                  "couldn't create BPy_Texture PyObject");

    pytex->texture = tex;
    return (PyObject *) pytex;
}

Tex *Texture_FromPyObject (PyObject *pyobj)
{
    return ((BPy_Texture *)pyobj)->texture;
}


int Texture_CheckPyObject (PyObject *pyobj)
{
    return (pyobj->ob_type == &Texture_Type);
}


/*****************************************************************************/
/* Python BPy_Texture methods:                                               */
/*****************************************************************************/

static PyObject *Texture_getExtend(BPy_Texture *self)
{
    PyObject *attr = NULL;
    const char *extend = NULL;

    if (EXPP_map_getStrVal (tex_extend_map, self->texture->extend, &extend))
        attr = PyString_FromString (extend);
    
    if (!attr) 
        return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                    "invalid internal extend mode");
    
    return attr;
}

static PyObject *Texture_getImage(BPy_Texture *self)
{
    /* we need this to be an IMAGE texture, and we must have an image */
    if ((self->texture->type != TEX_IMAGE) || !self->texture->ima)
    {
        Py_INCREF (Py_None);
        return Py_None;
    }
        
    return Image_CreatePyObject (self->texture->ima);
}


static PyObject *Texture_getName(BPy_Texture *self)
{
    PyObject *attr = PyString_FromString(self->texture->id.name+2);
    if (!attr) 
        return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                    "couldn't get Texture.name attribute");
    
    return attr;
}

static PyObject *Texture_getSType(BPy_Texture *self)
{
    PyObject *attr = NULL;
    const char *stype = NULL;

    if (EXPP_map_getStrVal (tex_stype_map[self->texture->type], 
                                            self->texture->stype, &stype))
        attr = PyString_FromString (stype);
    
    if (!attr) 
        return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                    "invalid texture stype internally");
    
    return attr;
}

static PyObject *Texture_getType(BPy_Texture *self)
{
    PyObject *attr = NULL;
    const char *type = NULL;

    if (EXPP_map_getStrVal (tex_type_map, self->texture->type, &type))
        attr = PyString_FromString (type);
    
    if (!attr) 
        return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                    "invalid texture type internally");
    
    return attr;
}


static PyObject *Texture_setAnimFrames(BPy_Texture *self, PyObject *args)
{
    int frames;
    if (!PyArg_ParseTuple(args, "i", &frames))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected an int");

    if (frames < 0)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                        "frames cannot be negative");

    self->texture->frames = frames;
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setAnimLength(BPy_Texture *self, PyObject *args)
{
    int length;
    if (!PyArg_ParseTuple(args, "i", &length))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected an int");

    if (length < 0)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                        "length cannot be negative");

    self->texture->len = length;
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setAnimMontage(BPy_Texture *self, PyObject *args)
{
    int fradur[4][2];
    int i, j;
    if (!PyArg_ParseTuple(args, "((ii)(ii)(ii)(ii))", 
                                        &fradur[0][0], &fradur[0][1],
                                        &fradur[1][0], &fradur[1][1],
                                        &fradur[2][0], &fradur[2][1],
                                        &fradur[3][0], &fradur[3][1]))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected a tuple of tuples");

    for (i=0; i<4; ++i)
        for (j=0; j<2; ++j)
            if (fradur[i][j] < 0)
                return EXPP_ReturnPyObjError (PyExc_ValueError,
                                        "values must be greater than zero");

    for (i=0; i<4; ++i)
        for (j=0; j<2; ++j)
            self->texture->fradur[i][j] = fradur[i][j];
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setAnimOffset(BPy_Texture *self, PyObject *args)
{
    int offset;
    if (!PyArg_ParseTuple(args, "i", &offset))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected an int");

    self->texture->offset = offset;
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setAnimStart(BPy_Texture *self, PyObject *args)
{
    int sfra;
    if (!PyArg_ParseTuple(args, "i", &sfra))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected an int");

    if (sfra < 1)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                        "start must be greater than zero");

    self->texture->sfra = sfra;
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setBrightness(BPy_Texture *self, PyObject *args)
{
    float bright;
    if (!PyArg_ParseTuple(args, "f", &bright))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected a float");

    if (bright<0 || bright>2)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                        "brightness must be in range [0,2]");

    self->texture->bright = bright;
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setContrast(BPy_Texture *self, PyObject *args)
{
    float contrast;
    if (!PyArg_ParseTuple(args, "f", &contrast))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected a float");

    if (contrast<0 || contrast>2)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                        "contrast must be in range [0,2]");

    self->texture->contrast = contrast;
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setCrop(BPy_Texture *self, PyObject *args)
{
    float crop[4];
    int i;
    if (!PyArg_ParseTuple(args, "(ffff)", 
                            &crop[0], &crop[1], &crop[2], &crop[3]))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected tuple of 4 floats");
    
    for (i=0; i<4; ++i)
        if (crop[i]<-10 || crop[i]>10)
            return EXPP_ReturnPyObjError (PyExc_ValueError,
                                        "values must be in range [-10,10]");

    self->texture->cropxmin = crop[0];
    self->texture->cropymin = crop[1];
    self->texture->cropxmax = crop[2];
    self->texture->cropymax = crop[3];
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setExtend(BPy_Texture *self, PyObject *args)
{
    char *extend = NULL;
    if (!PyArg_ParseTuple(args, "s", &extend))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected string argument");

    if (!EXPP_map_getShortVal (tex_extend_map, extend, &self->texture->extend))
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                           "invalid extend mode");    

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setIntExtend(BPy_Texture *self, PyObject *args)
{
    int extend = 0;
    if (!PyArg_ParseTuple(args, "i", &extend))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected int argument");
    
    if (extend<EXPP_TEX_EXTEND_MIN || extend>EXPP_TEX_EXTEND_MAX)
        return EXPP_ReturnPyObjError (PyExc_ValueError, 
                                            "invalid extend mode");
    
    self->texture->extend = extend;
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setFieldsPerImage(BPy_Texture *self, PyObject *args)
{
    int fie_ima;
    if (!PyArg_ParseTuple(args, "i", &fie_ima))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected an int");

    if (fie_ima<1 || fie_ima>200)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                     "value must be in range [1,200]");

    self->texture->fie_ima = fie_ima;
    
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *Texture_setFilterSize(BPy_Texture *self, PyObject *args)
{
    float size;
    if (!PyArg_ParseTuple(args, "f", &size))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected a float");

    if (size<0.1 || size>25)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                           "filter size must be in range [0.1,25]");

    self->texture->filtersize = size;
    
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *Texture_setFlags(BPy_Texture *self, PyObject *args)
{
    char *sf[3] = { NULL, NULL, NULL };
    int i;
    short flags = 0;
    short thisflag;
    if (!PyArg_ParseTuple(args, "|sss", &sf[0], &sf[1], &sf[2]))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected 0-3 string arguments");

    for (i=0; i<3; ++i)
    {
        if (!sf[i]) break;

        if (!EXPP_map_getShortVal(tex_flag_map, sf[i], &thisflag))
            return EXPP_ReturnPyObjError (PyExc_ValueError,
                                            "invalid texture flag name");

        flags |= thisflag;
    }

    self->texture->flag = flags;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setIntFlags(BPy_Texture *self, PyObject *args)
{
    int flags = 0;
    if (!PyArg_ParseTuple(args, "i", &flags))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected int argument");
    
    self->texture->flag = flags;
    
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *Texture_setImage(BPy_Texture *self, PyObject *args)
{
    PyObject *pyimg;
    Image *blimg = NULL;

    if (!PyArg_ParseTuple(args, "O!", &Image_Type, &pyimg))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected an Image");
    blimg = Image_FromPyObject (pyimg);

    if (self->texture->ima) {
        self->texture->ima->id.us--;
    }

    self->texture->ima = blimg;
    id_us_plus(&blimg->id);

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *Texture_setImageFlags(BPy_Texture *self, PyObject *args)
{
    char *sf[9] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    int i;
    short flags = 0;
    short thisflag;
    if (!PyArg_ParseTuple(args, "|sssssssss", &sf[0], &sf[1], &sf[2], &sf[3],
                                    &sf[4], &sf[5], &sf[6], &sf[7], &sf[8]))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected 0-9 string arguments");

    for (i=0; i<9; ++i)
    {
        if (!sf[i]) break;

        if (!EXPP_map_getShortVal(tex_imageflag_map, sf[i], &thisflag))
            return EXPP_ReturnPyObjError (PyExc_ValueError,
                                            "invalid texture image flag name");

        flags |= thisflag;
    }

    /* MIPMAP and FIELDS can't be used together */
    if ((flags & EXPP_TEX_IMAGEFLAG_MIPMAP) &&
            (flags & EXPP_TEX_IMAGEFLAG_FIELDS))
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                  "image flags MIPMAP and FIELDS cannot be used together");
    
    self->texture->imaflag = flags;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setIntImageFlags(BPy_Texture *self, PyObject *args)
{
    int flags = 0;
    if (!PyArg_ParseTuple(args, "i", &flags))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected int argument");
    
    /* MIPMAP and FIELDS can't be used together */
    if ((flags & EXPP_TEX_IMAGEFLAG_MIPMAP) &&
            (flags & EXPP_TEX_IMAGEFLAG_FIELDS))
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                  "image flags MIPMAP and FIELDS cannot be used together");
    
    self->texture->imaflag = flags;
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setName(BPy_Texture *self, PyObject *args)
{
    char *name;
    char buf[21];

    if (!PyArg_ParseTuple(args, "s", &name))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected string argument");

    PyOS_snprintf(buf, sizeof(buf), "%s", name);
    rename_id(&self->texture->id, buf);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setNoiseDepth(BPy_Texture *self, PyObject *args)
{
    int depth;
    if (!PyArg_ParseTuple(args, "i", &depth))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected an int");

    if (depth<0 || depth>6)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                     "value must be in range [0,6]");

    self->texture->noisedepth = depth;
    
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *Texture_setNoiseSize(BPy_Texture *self, PyObject *args)
{
    float size;
    if (!PyArg_ParseTuple(args, "f", &size))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected a float");

    if (size<0 || size>2)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                           "noise size must be in range [0,2]");

    self->texture->noisesize = size;
    
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *Texture_setNoiseType(BPy_Texture *self, PyObject *args)
{
    char *type;

    if (!PyArg_ParseTuple(args, "s", &type))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected string argument");

    if (STREQ(type, "soft"))
        self->texture->noisetype = TEX_NOISESOFT;
    else if (STREQ(type, "hard"))
        self->texture->noisetype = TEX_NOISEPERL;
    
    else
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                    "noise type must be 'soft' or 'hard'");

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setRepeat(BPy_Texture *self, PyObject *args)
{
    int repeat[2];
    int i;
    if (!PyArg_ParseTuple(args, "(ii)", &repeat[0], &repeat[1]))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected tuple of 2 ints");
    
    for (i=0; i<2; ++i)
        if (repeat[i]<1 || repeat[i]>512)
            return EXPP_ReturnPyObjError (PyExc_ValueError,
                                    "values must be in range [1,512]");

    self->texture->xrepeat = repeat[0];
    self->texture->yrepeat = repeat[1];
    
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *Texture_setRGBCol(BPy_Texture *self, PyObject *args)
{
    float rgb[3];
    int i;
    if (!PyArg_ParseTuple(args, "(fff)", &rgb[0], &rgb[1], &rgb[2]))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected tuple of 3 floats");
    
    for (i=0; i<3; ++i)
        if (rgb[i]<0 || rgb[i]>2)
            return EXPP_ReturnPyObjError (PyExc_ValueError,
                                    "values must be in range [0,2]");

    self->texture->rfac = rgb[0];
    self->texture->gfac = rgb[1];
    self->texture->bfac = rgb[2];
    
    Py_INCREF (Py_None);
    return Py_None;
}


static PyObject *Texture_setSType(BPy_Texture *self, PyObject *args)
{
    char *stype = NULL;
    if (!PyArg_ParseTuple(args, "s", &stype))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected string argument");

    /* can we really trust texture->type? */
    if (!EXPP_map_getShortVal (tex_stype_map[self->texture->type], 
                                            stype, &self->texture->stype))
        return EXPP_ReturnPyObjError (PyExc_ValueError, 
                                                "invalid texture stype");

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setIntSType(BPy_Texture *self, PyObject *args)
{
    int stype = 0;
    const char *dummy = NULL;
    if (!PyArg_ParseTuple(args, "i", &stype))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected int argument");
    
    /* use the stype map to find out if this is a valid stype for this type *
     * note that this will allow CLD_COLOR when type is ENVMAP. there's not *
     * much that we can do about this though.                               */
    if (!EXPP_map_getStrVal (tex_stype_map[self->texture->type], stype, &dummy))
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                            "invalid stype (for this type)");
    
    self->texture->stype = stype;
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setTurbulence(BPy_Texture *self, PyObject *args)
{
    float turb;
    if (!PyArg_ParseTuple(args, "f", &turb))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                        "expected a float");

    if (turb<0 || turb>200)
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                           "turbulence must be in range [0,200]");

    self->texture->turbul = turb;
    
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *Texture_setType(BPy_Texture *self, PyObject *args)
{
    char *type = NULL;
    if (!PyArg_ParseTuple(args, "s", &type))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected string argument");

    if (!EXPP_map_getShortVal (tex_type_map, type, &self->texture->type))
        return EXPP_ReturnPyObjError (PyExc_ValueError,
                                           "invalid texture type");    

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Texture_setIntType(BPy_Texture *self, PyObject *args)
{
    int type = 0;
    if (!PyArg_ParseTuple(args, "i", &type))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected int argument");
    
    if (type<EXPP_TEX_TYPE_MIN || type>EXPP_TEX_TYPE_MAX)
        return EXPP_ReturnPyObjError (PyExc_ValueError, 
                                            "invalid type number");
    
    self->texture->type = type;
    
    Py_INCREF(Py_None);
    return Py_None;
}

static void Texture_dealloc (BPy_Texture *self)
{
    PyObject_DEL (self);
}

static PyObject *Texture_getAttr (BPy_Texture *self, char *name)
{
    PyObject *attr = Py_None;
    Tex *tex = self->texture;

    if (STREQ(name, "animFrames"))
        attr = PyInt_FromLong (tex->frames);
    else if (STREQ(name, "animLength"))
        attr = PyInt_FromLong (tex->len);
    else if (STREQ(name, "animMontage"))
        attr = Py_BuildValue("((i,i),(i,i),(i,i),(i,i))", 
                            tex->fradur[0][0], tex->fradur[0][1],
                            tex->fradur[1][0], tex->fradur[1][1],
                            tex->fradur[2][0], tex->fradur[2][1],
                            tex->fradur[3][0], tex->fradur[3][1] );
    else if (STREQ(name, "animOffset"))
        attr = PyInt_FromLong (tex->offset);
    else if (STREQ(name, "animStart"))
        attr = PyInt_FromLong (tex->sfra);
    else if (STREQ(name, "brightness"))
        attr = PyFloat_FromDouble (tex->bright);
    else if (STREQ(name, "contrast"))
        attr = PyFloat_FromDouble (tex->contrast);
    else if (STREQ(name, "crop"))
        attr = Py_BuildValue("(f,f,f,f)", tex->cropxmin, tex->cropymin, 
                                          tex->cropxmax, tex->cropymax);
    else if (STREQ(name, "extend"))
        attr = PyInt_FromLong (tex->extend);
    else if (STREQ(name, "fieldsPerImage"))
        attr = PyInt_FromLong (tex->fie_ima);
    else if (STREQ(name, "filterSize"))
        attr = PyFloat_FromDouble (tex->filtersize);
    else if (STREQ(name, "flags"))
        attr = PyInt_FromLong (tex->flag);
    else if (STREQ(name, "image"))
        attr = Texture_getImage (self);
    else if (STREQ(name, "imageFlags"))
        attr = PyInt_FromLong (tex->imaflag);
    else if (STREQ(name, "name"))
        attr = PyString_FromString(tex->id.name+2);
    else if (STREQ(name, "noiseDepth"))
        attr = PyInt_FromLong (tex->noisedepth);
    else if (STREQ(name, "noiseSize"))
        attr = PyFloat_FromDouble (tex->noisesize);
    else if (STREQ(name, "noiseType"))
    {
        if (tex->noisetype == TEX_NOISESOFT)
            attr = PyString_FromString ("soft");
        else
            attr = PyString_FromString ("hard");
    }
    else if (STREQ(name, "repeat"))
        attr = Py_BuildValue ("(i,i)", tex->xrepeat, tex->yrepeat);
    else if (STREQ(name, "rgbCol"))
        attr = Py_BuildValue ("(f,f,f)", tex->rfac, tex->gfac, tex->gfac);
    else if (STREQ(name, "stype"))
        attr = PyInt_FromLong (tex->stype);
    else if (STREQ(name, "turbulence"))
        attr = PyFloat_FromDouble (tex->turbul);
    else if (STREQ(name, "type"))
        attr = PyInt_FromLong (tex->type);

    
    else if (STREQ(name, "__members__"))
        attr = Py_BuildValue("[s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s]", 
                "animFrames", "animLength", "animMontage", "animOffset",
                "animStart", "brightness", "contrast", "crop", "extend",
                "fieldsPerImage", "filterSize", "flags", "image", 
                "imageFlags", "name", "noiseDepth", "noiseSize", "noiseType",
                "repeat", "rgbCol", "stype", "turbulence", "type");

    if (!attr)
        return EXPP_ReturnPyObjError (PyExc_MemoryError,
                                            "couldn't create PyObject");

    if (attr != Py_None) 
        return attr; /* member attribute found, return it */

    /* not an attribute, search the methods table */
    return Py_FindMethod(BPy_Texture_methods, (PyObject *)self, name);
}


static int Texture_setAttr (BPy_Texture *self, char *name, PyObject *value)
{
    PyObject *valtuple; 
    PyObject *error = NULL;

    /* Put "value" in a tuple, because we want to pass it to functions  *
     * that only accept PyTuples.                                       */
    valtuple = Py_BuildValue("(O)", value);
    if (!valtuple)
        return EXPP_ReturnIntError(PyExc_MemoryError,
                                "Texture_setAttr: couldn't create PyTuple");

    if (STREQ(name, "animFrames"))
        error = Texture_setAnimFrames (self, valtuple);
    else if (STREQ(name, "animLength"))
        error = Texture_setAnimLength(self, valtuple);
    else if (STREQ(name, "animMontage"))
        error = Texture_setAnimMontage(self, valtuple);
    else if (STREQ(name, "animOffset"))
        error = Texture_setAnimOffset(self, valtuple);
    else if (STREQ(name, "animStart"))
        error = Texture_setAnimStart(self, valtuple);
    else if (STREQ(name, "brightness"))
        error = Texture_setBrightness(self, valtuple);
    else if (STREQ(name, "contrast"))
        error = Texture_setContrast(self, valtuple);
    else if (STREQ(name, "crop"))
        error = Texture_setCrop(self, valtuple);
    else if (STREQ(name, "extend"))
        error = Texture_setIntExtend(self, valtuple);
    else if (STREQ(name, "fieldsPerImage"))
        error = Texture_setFieldsPerImage(self, valtuple);
    else if (STREQ(name, "filterSize"))
        error = Texture_setFilterSize(self, valtuple);
    else if (STREQ(name, "flags"))
        error = Texture_setIntFlags(self, valtuple);
    else if (STREQ(name, "image"))
        error = Texture_setImage (self, valtuple);
    else if (STREQ(name, "imageFlags"))
        error = Texture_setIntImageFlags(self, valtuple);
    else if (STREQ(name, "name"))
        error = Texture_setName(self, valtuple);
    else if (STREQ(name, "noiseDepth"))
        error = Texture_setNoiseDepth(self, valtuple);
    else if (STREQ(name, "noiseSize"))
        error = Texture_setNoiseSize(self, valtuple);
    else if (STREQ(name, "noiseType"))
        error = Texture_setNoiseType(self, valtuple);
    else if (STREQ(name, "repeat"))
        error = Texture_setRepeat(self, valtuple);
    else if (STREQ(name, "rgbCol"))
        error = Texture_setRGBCol(self, valtuple);
    else if (STREQ(name, "stype"))
        error = Texture_setIntSType(self, valtuple);
    else if (STREQ(name, "turbulence"))
        error = Texture_setTurbulence(self, valtuple);
    else if (STREQ(name, "type"))
        error = Texture_setIntType(self, valtuple);
    
    else { 
        /* Error */
        Py_DECREF(valtuple);
        return EXPP_ReturnIntError (PyExc_KeyError, "attribute not found");
    }

    Py_DECREF (valtuple);

    if (error != Py_None) 
        return -1;

    /* Py_None was INCREF'd by the set*() function, so we need to DECREF it */
    Py_DECREF (Py_None);
    
    return 0;
}

static int Texture_compare (BPy_Texture *a, BPy_Texture *b)
{
    return (a->texture == b->texture) ? 0 : -1;
}

static PyObject *Texture_repr (BPy_Texture *self)
{
    return PyString_FromFormat("[Texture \"%s\"]", self->texture->id.name+2);
}

