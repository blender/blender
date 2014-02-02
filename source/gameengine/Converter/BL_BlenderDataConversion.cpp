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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Convert blender data to ketsji
 */

/** \file gameengine/Converter/BL_BlenderDataConversion.cpp
 *  \ingroup bgeconv
 */

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include <math.h>
#include <vector>
#include <algorithm>

#include "BL_BlenderDataConversion.h"
#include "KX_BlenderScalarInterpolator.h"

#include "RAS_IPolygonMaterial.h"

// Expressions
#include "ListValue.h"
#include "IntValue.h"
// Collision & Fuzzics LTD

#include "PHY_Pro.h"


#include "KX_Scene.h"
#include "KX_GameObject.h"
#include "RAS_FramingManager.h"
#include "RAS_MeshObject.h"

#include "KX_ConvertActuators.h"
#include "KX_ConvertControllers.h"
#include "KX_ConvertSensors.h"

#include "SCA_LogicManager.h"
#include "SCA_EventManager.h"
#include "SCA_TimeEventManager.h"
#include "KX_Light.h"
#include "KX_Camera.h"
#include "KX_EmptyObject.h"
#include "KX_FontObject.h"
#include "MT_Point3.h"
#include "MT_Transform.h"
#include "MT_MinMax.h"
#include "SCA_IInputDevice.h"
#include "RAS_TexMatrix.h"
#include "RAS_ICanvas.h"
#include "RAS_MaterialBucket.h"
//#include "KX_BlenderPolyMaterial.h"
#include "RAS_Polygon.h"
#include "RAS_TexVert.h"
#include "RAS_BucketManager.h"
#include "BL_Material.h"
#include "KX_BlenderMaterial.h"
#include "BL_Texture.h"

#include "DNA_action_types.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BL_ModifierDeformer.h"
#include "BL_ShapeDeformer.h"
#include "BL_SkinDeformer.h"
#include "BL_MeshDeformer.h"
#include "KX_SoftBodyDeformer.h"
//#include "BL_ArmatureController.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BlenderWorldInfo.h"

#include "KX_KetsjiEngine.h"
#include "KX_BlenderSceneConverter.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* This list includes only data type definitions */
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_group_types.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_property_types.h"
#include "DNA_text_types.h"
#include "DNA_sensor_types.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_sound_types.h"
#include "DNA_key_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_force.h"

#include "MEM_guardedalloc.h"

#include "BKE_key.h"
#include "BKE_mesh.h"
#include "MT_Point3.h"

#include "BLI_math.h"

extern "C" {
#include "BKE_scene.h"
#include "BKE_customdata.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_material.h" /* give_current_material */
#include "BKE_image.h"
#include "IMB_imbuf_types.h"

extern Material defmaterial;	/* material.c */
}

/* end of blender include block */

#include "KX_BlenderInputDevice.h"
#include "KX_ConvertProperties.h"
#include "KX_HashedPtr.h"


#include "KX_ScalarInterpolator.h"

#include "KX_IpoConvert.h"
#include "BL_System.h"

#include "SG_Node.h"
#include "SG_BBox.h"
#include "SG_Tree.h"

#include "KX_ConvertPhysicsObject.h"
#ifdef WITH_BULLET
#include "CcdPhysicsEnvironment.h"
#include "CcdGraphicController.h"
#endif
#include "KX_MotionState.h"

// This file defines relationships between parents and children
// in the game engine.

#include "KX_SG_NodeRelationships.h"
#include "KX_SG_BoneParentNodeRelationship.h"

#include "BL_ArmatureObject.h"
#include "BL_DeformableGameObject.h"

#include "KX_NavMeshObject.h"
#include "KX_ObstacleSimulation.h"

#ifdef __cplusplus
extern "C" {
#endif
//XXX void update_for_newframe();
//void BKE_scene_update_for_newframe(struct Scene *sce, unsigned int lay);
//void do_all_data_ipos(void);
#ifdef __cplusplus
}
#endif

#include "BLI_threads.h"

static bool default_light_mode = 0;

static std::map<int, SCA_IInputDevice::KX_EnumInputs> create_translate_table()
{
	std::map<int, SCA_IInputDevice::KX_EnumInputs> m;
		
	/* The reverse table. In order to not confuse ourselves, we      */
	/* immediately convert all events that come in to KX codes.      */
	m[LEFTMOUSE			] =	SCA_IInputDevice::KX_LEFTMOUSE;
	m[MIDDLEMOUSE		] =	SCA_IInputDevice::KX_MIDDLEMOUSE;
	m[RIGHTMOUSE		] =	SCA_IInputDevice::KX_RIGHTMOUSE;
	m[WHEELUPMOUSE		] =	SCA_IInputDevice::KX_WHEELUPMOUSE;
	m[WHEELDOWNMOUSE	] =	SCA_IInputDevice::KX_WHEELDOWNMOUSE;
	m[MOUSEX			] = SCA_IInputDevice::KX_MOUSEX;
	m[MOUSEY			] =	SCA_IInputDevice::KX_MOUSEY;
		
	// TIMERS                                                                                                  
		
	m[TIMER0			] = SCA_IInputDevice::KX_TIMER0;                  
	m[TIMER1			] = SCA_IInputDevice::KX_TIMER1;                  
	m[TIMER2			] = SCA_IInputDevice::KX_TIMER2;                  
		
	// SYSTEM                                                                                                  
		
#if 0
	/* **** XXX **** */
	m[KEYBD				] = SCA_IInputDevice::KX_KEYBD;                  
	m[RAWKEYBD			] = SCA_IInputDevice::KX_RAWKEYBD;                  
	m[REDRAW			] = SCA_IInputDevice::KX_REDRAW;                  
	m[INPUTCHANGE		] = SCA_IInputDevice::KX_INPUTCHANGE;                  
	m[QFULL				] = SCA_IInputDevice::KX_QFULL;                  
	m[WINFREEZE			] = SCA_IInputDevice::KX_WINFREEZE;                  
	m[WINTHAW			] = SCA_IInputDevice::KX_WINTHAW;                  
	m[WINCLOSE			] = SCA_IInputDevice::KX_WINCLOSE;                  
	m[WINQUIT			] = SCA_IInputDevice::KX_WINQUIT;                  
	m[Q_FIRSTTIME		] = SCA_IInputDevice::KX_Q_FIRSTTIME;                  
	/* **** XXX **** */
#endif
		
	// standard keyboard                                                                                       
		
	m[AKEY				] = SCA_IInputDevice::KX_AKEY;                  
	m[BKEY				] = SCA_IInputDevice::KX_BKEY;                  
	m[CKEY				] = SCA_IInputDevice::KX_CKEY;                  
	m[DKEY				] = SCA_IInputDevice::KX_DKEY;                  
	m[EKEY				] = SCA_IInputDevice::KX_EKEY;                  
	m[FKEY				] = SCA_IInputDevice::KX_FKEY;                  
	m[GKEY				] = SCA_IInputDevice::KX_GKEY;                  

//XXX clean up
#ifdef WIN32
#define HKEY	'h'
#endif
	m[HKEY				] = SCA_IInputDevice::KX_HKEY;                  
//XXX clean up
#ifdef WIN32
#undef HKEY
#endif

	m[IKEY				] = SCA_IInputDevice::KX_IKEY;                  
	m[JKEY				] = SCA_IInputDevice::KX_JKEY;                  
	m[KKEY				] = SCA_IInputDevice::KX_KKEY;                  
	m[LKEY				] = SCA_IInputDevice::KX_LKEY;                  
	m[MKEY				] = SCA_IInputDevice::KX_MKEY;                  
	m[NKEY				] = SCA_IInputDevice::KX_NKEY;                  
	m[OKEY				] = SCA_IInputDevice::KX_OKEY;                  
	m[PKEY				] = SCA_IInputDevice::KX_PKEY;                  
	m[QKEY				] = SCA_IInputDevice::KX_QKEY;                  
	m[RKEY				] = SCA_IInputDevice::KX_RKEY;                  
	m[SKEY				] = SCA_IInputDevice::KX_SKEY;                  
	m[TKEY				] = SCA_IInputDevice::KX_TKEY;                  
	m[UKEY				] = SCA_IInputDevice::KX_UKEY;                  
	m[VKEY				] = SCA_IInputDevice::KX_VKEY;                  
	m[WKEY				] = SCA_IInputDevice::KX_WKEY;                  
	m[XKEY				] = SCA_IInputDevice::KX_XKEY;                  
	m[YKEY				] = SCA_IInputDevice::KX_YKEY;                  
	m[ZKEY				] = SCA_IInputDevice::KX_ZKEY;                  
		
	m[ZEROKEY			] = SCA_IInputDevice::KX_ZEROKEY;                  
	m[ONEKEY			] = SCA_IInputDevice::KX_ONEKEY;                  
	m[TWOKEY			] = SCA_IInputDevice::KX_TWOKEY;                  
	m[THREEKEY			] = SCA_IInputDevice::KX_THREEKEY;                  
	m[FOURKEY			] = SCA_IInputDevice::KX_FOURKEY;                  
	m[FIVEKEY			] = SCA_IInputDevice::KX_FIVEKEY;                  
	m[SIXKEY			] = SCA_IInputDevice::KX_SIXKEY;                  
	m[SEVENKEY			] = SCA_IInputDevice::KX_SEVENKEY;                  
	m[EIGHTKEY			] = SCA_IInputDevice::KX_EIGHTKEY;                  
	m[NINEKEY			] = SCA_IInputDevice::KX_NINEKEY;                  
		
	m[CAPSLOCKKEY		] = SCA_IInputDevice::KX_CAPSLOCKKEY;                  
		
	m[LEFTCTRLKEY		] = SCA_IInputDevice::KX_LEFTCTRLKEY;                  
	m[LEFTALTKEY		] = SCA_IInputDevice::KX_LEFTALTKEY;                  
	m[RIGHTALTKEY		] = SCA_IInputDevice::KX_RIGHTALTKEY;                  
	m[RIGHTCTRLKEY		] = SCA_IInputDevice::KX_RIGHTCTRLKEY;                  
	m[RIGHTSHIFTKEY		] = SCA_IInputDevice::KX_RIGHTSHIFTKEY;                  
	m[LEFTSHIFTKEY		] = SCA_IInputDevice::KX_LEFTSHIFTKEY;                  
		
	m[ESCKEY			] = SCA_IInputDevice::KX_ESCKEY;                  
	m[TABKEY			] = SCA_IInputDevice::KX_TABKEY;                  
	m[RETKEY			] = SCA_IInputDevice::KX_RETKEY;                  
	m[SPACEKEY			] = SCA_IInputDevice::KX_SPACEKEY;                  
	m[LINEFEEDKEY		] = SCA_IInputDevice::KX_LINEFEEDKEY;                  
	m[BACKSPACEKEY		] = SCA_IInputDevice::KX_BACKSPACEKEY;                  
	m[DELKEY			] = SCA_IInputDevice::KX_DELKEY;                  
	m[SEMICOLONKEY		] = SCA_IInputDevice::KX_SEMICOLONKEY;                  
	m[PERIODKEY			] = SCA_IInputDevice::KX_PERIODKEY;                  
	m[COMMAKEY			] = SCA_IInputDevice::KX_COMMAKEY;                  
	m[QUOTEKEY			] = SCA_IInputDevice::KX_QUOTEKEY;                  
	m[ACCENTGRAVEKEY	] = SCA_IInputDevice::KX_ACCENTGRAVEKEY;                  
	m[MINUSKEY			] = SCA_IInputDevice::KX_MINUSKEY;                  
	m[SLASHKEY			] = SCA_IInputDevice::KX_SLASHKEY;                  
	m[BACKSLASHKEY		] = SCA_IInputDevice::KX_BACKSLASHKEY;                  
	m[EQUALKEY			] = SCA_IInputDevice::KX_EQUALKEY;                  
	m[LEFTBRACKETKEY	] = SCA_IInputDevice::KX_LEFTBRACKETKEY;                  
	m[RIGHTBRACKETKEY	] = SCA_IInputDevice::KX_RIGHTBRACKETKEY;                  
		
	m[LEFTARROWKEY		] = SCA_IInputDevice::KX_LEFTARROWKEY;                  
	m[DOWNARROWKEY		] = SCA_IInputDevice::KX_DOWNARROWKEY;                  
	m[RIGHTARROWKEY		] = SCA_IInputDevice::KX_RIGHTARROWKEY;                  
	m[UPARROWKEY		] = SCA_IInputDevice::KX_UPARROWKEY;                  
		
	m[PAD2				] = SCA_IInputDevice::KX_PAD2;                  
	m[PAD4				] = SCA_IInputDevice::KX_PAD4;                  
	m[PAD6				] = SCA_IInputDevice::KX_PAD6;                  
	m[PAD8				] = SCA_IInputDevice::KX_PAD8;                  
		
	m[PAD1				] = SCA_IInputDevice::KX_PAD1;                  
	m[PAD3				] = SCA_IInputDevice::KX_PAD3;                  
	m[PAD5				] = SCA_IInputDevice::KX_PAD5;                  
	m[PAD7				] = SCA_IInputDevice::KX_PAD7;                  
	m[PAD9				] = SCA_IInputDevice::KX_PAD9;                  
		
	m[PADPERIOD			] = SCA_IInputDevice::KX_PADPERIOD;                  
	m[PADSLASHKEY		] = SCA_IInputDevice::KX_PADSLASHKEY;                  
	m[PADASTERKEY		] = SCA_IInputDevice::KX_PADASTERKEY;                  
		
	m[PAD0				] = SCA_IInputDevice::KX_PAD0;                  
	m[PADMINUS			] = SCA_IInputDevice::KX_PADMINUS;                  
	m[PADENTER			] = SCA_IInputDevice::KX_PADENTER;                  
	m[PADPLUSKEY		] = SCA_IInputDevice::KX_PADPLUSKEY;                  
		
		
	m[F1KEY				] = SCA_IInputDevice::KX_F1KEY;                  
	m[F2KEY				] = SCA_IInputDevice::KX_F2KEY;                  
	m[F3KEY				] = SCA_IInputDevice::KX_F3KEY;                  
	m[F4KEY				] = SCA_IInputDevice::KX_F4KEY;                  
	m[F5KEY				] = SCA_IInputDevice::KX_F5KEY;                  
	m[F6KEY				] = SCA_IInputDevice::KX_F6KEY;                  
	m[F7KEY				] = SCA_IInputDevice::KX_F7KEY;                  
	m[F8KEY				] = SCA_IInputDevice::KX_F8KEY;                  
	m[F9KEY				] = SCA_IInputDevice::KX_F9KEY;                  
	m[F10KEY			] = SCA_IInputDevice::KX_F10KEY;                  
	m[F11KEY			] = SCA_IInputDevice::KX_F11KEY;                  
	m[F12KEY			] = SCA_IInputDevice::KX_F12KEY;
	m[F13KEY			] = SCA_IInputDevice::KX_F13KEY;
	m[F14KEY			] = SCA_IInputDevice::KX_F14KEY;
	m[F15KEY			] = SCA_IInputDevice::KX_F15KEY;
	m[F16KEY			] = SCA_IInputDevice::KX_F16KEY;
	m[F17KEY			] = SCA_IInputDevice::KX_F17KEY;
	m[F18KEY			] = SCA_IInputDevice::KX_F18KEY;
	m[F19KEY			] = SCA_IInputDevice::KX_F19KEY;

	m[OSKEY				] = SCA_IInputDevice::KX_OSKEY;

	m[PAUSEKEY			] = SCA_IInputDevice::KX_PAUSEKEY;                  
	m[INSERTKEY			] = SCA_IInputDevice::KX_INSERTKEY;                  
	m[HOMEKEY			] = SCA_IInputDevice::KX_HOMEKEY;                  
	m[PAGEUPKEY			] = SCA_IInputDevice::KX_PAGEUPKEY;                  
	m[PAGEDOWNKEY		] = SCA_IInputDevice::KX_PAGEDOWNKEY;                  
	m[ENDKEY			] = SCA_IInputDevice::KX_ENDKEY;

	return m;
}

static std::map<int, SCA_IInputDevice::KX_EnumInputs> gReverseKeyTranslateTable = create_translate_table();

static unsigned int KX_rgbaint2uint_new(unsigned int icol)
{
	union
	{
		unsigned int integer;
		unsigned char cp[4];
	} out_color, in_color;
	
	in_color.integer = icol;
	out_color.cp[0] = in_color.cp[3]; // red
	out_color.cp[1] = in_color.cp[2]; // green
	out_color.cp[2] = in_color.cp[1]; // blue
	out_color.cp[3] = in_color.cp[0]; // alpha
	
	return out_color.integer;
}

/* Now the real converting starts... */
static unsigned int KX_Mcol2uint_new(MCol col)
{
	/* color has to be converted without endian sensitivity. So no shifting! */
	union
	{
		MCol col;
		unsigned int integer;
		unsigned char cp[4];
	} out_color, in_color;

	in_color.col = col;
	out_color.cp[0] = in_color.cp[3]; // red
	out_color.cp[1] = in_color.cp[2]; // green
	out_color.cp[2] = in_color.cp[1]; // blue
	out_color.cp[3] = in_color.cp[0]; // alpha
	
	return out_color.integer;
}

static void SetDefaultLightMode(Scene* scene)
{
	default_light_mode = false;
	Scene *sce_iter;
	Base *base;

	for (SETLOOPER(scene, sce_iter, base))
	{
		if (base->object->type == OB_LAMP)
		{
			default_light_mode = true;
			return;
		}
	}
}


static bool GetMaterialUseVColor(Material *ma, const bool glslmat)
{
	if (ma) {
		/* glsl uses vertex colors, otherwise use material setting
		 * defmaterial doesn't have VERTEXCOLP as default [#34505] */
		return (glslmat || ma == &defmaterial || (ma->mode & MA_VERTEXCOLP) != 0);
	}
	else {
		/* no material, use vertex colors */
		return true;
	}
}

// --
static void GetRGB(
        const bool use_vcol,
        MFace* mface,
        MCol* mmcol,
        Material *mat,
        unsigned int c[4])
{
	unsigned int color = 0xFFFFFFFFL;
	if (use_vcol == true) {
		if (mmcol) {
			c[0] = KX_Mcol2uint_new(mmcol[0]);
			c[1] = KX_Mcol2uint_new(mmcol[1]);
			c[2] = KX_Mcol2uint_new(mmcol[2]);
			if (mface->v4)
				c[3] = KX_Mcol2uint_new(mmcol[3]);
		}
		else { // backup white
			c[0] = KX_rgbaint2uint_new(color);
			c[1] = KX_rgbaint2uint_new(color);
			c[2] = KX_rgbaint2uint_new(color);
			if (mface->v4)
				c[3] = KX_rgbaint2uint_new( color );
		}
	}
	else {
		/* material rgba */
		if (mat) {
			union {
				unsigned char cp[4];
				unsigned int integer;
			} col_converter;
			col_converter.cp[3] = (unsigned char) (mat->r     * 255.0f);
			col_converter.cp[2] = (unsigned char) (mat->g     * 255.0f);
			col_converter.cp[1] = (unsigned char) (mat->b     * 255.0f);
			col_converter.cp[0] = (unsigned char) (mat->alpha * 255.0f);
			color = col_converter.integer;
		}
		c[0] = KX_rgbaint2uint_new(color);
		c[1] = KX_rgbaint2uint_new(color);
		c[2] = KX_rgbaint2uint_new(color);
		if (mface->v4) {
			c[3] = KX_rgbaint2uint_new(color);
		}
	}

#if 0  /* white, unused */
	{
		c[0] = KX_rgbaint2uint_new(color);
		c[1] = KX_rgbaint2uint_new(color);
		c[2] = KX_rgbaint2uint_new(color);
		if (mface->v4)
			c[3] = KX_rgbaint2uint_new(color);
	}
#endif
}

typedef struct MTF_localLayer {
	MTFace *face;
	const char *name;
} MTF_localLayer;

static void GetUVs(BL_Material *material, MTF_localLayer *layers, MFace *mface, MTFace *tface, MT_Point2 uvs[4][MAXTEX])
{
	int unit = 0;
	if (tface)
	{
			
		uvs[0][0].setValue(tface->uv[0]);
		uvs[1][0].setValue(tface->uv[1]);
		uvs[2][0].setValue(tface->uv[2]);

		if (mface->v4) 
			uvs[3][0].setValue(tface->uv[3]);
	}
	else
	{
		uvs[0][0] = uvs[1][0] = uvs[2][0] = uvs[3][0] = MT_Point2(0.f, 0.f);
	}
	
	vector<STR_String> found_layers;

	for (int vind = 0; vind<MAXTEX; vind++)
	{
		BL_Mapping &map = material->mapping[vind];

		if (!(map.mapping & USEUV)) continue;

		if (std::find(found_layers.begin(), found_layers.end(), map.uvCoName) != found_layers.end())
			continue;

		//If no UVSet is specified, try grabbing one from the UV/Image editor
		if (map.uvCoName.IsEmpty() && tface)
		{			
			uvs[0][unit].setValue(tface->uv[0]);
			uvs[1][unit].setValue(tface->uv[1]);
			uvs[2][unit].setValue(tface->uv[2]);

			if (mface->v4) 
				uvs[3][unit].setValue(tface->uv[3]);

			++unit;
			continue;
		}


		for (int lay=0; lay<MAX_MTFACE; lay++)
		{
			MTF_localLayer& layer = layers[lay];
			if (layer.face == 0) break;

			if (map.uvCoName.IsEmpty() || strcmp(map.uvCoName.ReadPtr(), layer.name)==0)
			{
				uvs[0][unit].setValue(layer.face->uv[0]);
				uvs[1][unit].setValue(layer.face->uv[1]);
				uvs[2][unit].setValue(layer.face->uv[2]);

				if (mface->v4) 
					uvs[3][unit].setValue(layer.face->uv[3]);
				else
					uvs[3][unit].setValue(0.0f, 0.0f);

				++unit;
				found_layers.push_back(map.uvCoName);
				break;
			}
		}
	}
}

// ------------------------------------
static bool ConvertMaterial(
	BL_Material *material,
	Material *mat, 
	MTFace* tface,  
	const char *tfaceName,
	MFace* mface, 
	MCol* mmcol,
	bool glslmat)
{
	material->Initialize();
	int texalpha = 0;
	const bool validmat  = (mat != NULL);
	const bool validface = (tface != NULL);
	const bool use_vcol  = GetMaterialUseVColor(mat, glslmat);
	
	material->IdMode = DEFAULT_BLENDER;
	material->glslmat = (validmat) ? glslmat: false;
	material->materialindex = mface->mat_nr;

	// --------------------------------
	if (validmat) {

		// use lighting?
		material->ras_mode |= ( mat->mode & MA_SHLESS )?0:USE_LIGHT;
		material->ras_mode |= ( mat->game.flag & GEMAT_BACKCULL )?0:TWOSIDED;

		// cast shadows?
		material->ras_mode |= ( mat->mode & MA_SHADBUF )?CAST_SHADOW:0;
		MTex *mttmp = 0;
		int valid_index = 0;
		
		/* In Multitexture use the face texture if and only if
		 * it is set in the buttons
		 * In GLSL is not working yet :/ 3.2011 */
		bool facetex = false;
		if (validface && mat->mode &MA_FACETEXTURE) 
			facetex = true;
	
		// foreach MTex
		for (int i=0; i<MAXTEX; i++) {
			// use face tex

			if (i==0 && facetex ) {
				facetex = false;
				Image*tmp = (Image*)(tface->tpage);

				if (tmp) {
					material->img[i] = tmp;
					material->texname[i] = material->img[i]->id.name;
					material->flag[i] |= MIPMAP;

					material->flag[i] |= ( mat->game.alpha_blend & GEMAT_ALPHA_SORT )?USEALPHA:0;
					material->flag[i] |= ( mat->game.alpha_blend & GEMAT_ALPHA )?USEALPHA:0;
					material->flag[i] |= ( mat->game.alpha_blend & GEMAT_ADD )?CALCALPHA:0;

					if (material->img[i]->flag & IMA_REFLECT) {
						material->mapping[i].mapping |= USEREFL;
					}
					else {
						mttmp = getMTexFromMaterial(mat, i);
						if (mttmp && (mttmp->texco & TEXCO_UV)) {
							/* string may be "" but thats detected as empty after */
							material->mapping[i].uvCoName = mttmp->uvname;
						}
						material->mapping[i].mapping |= USEUV;
					}

					valid_index++;
				}
				else {
					material->img[i] = 0;
					material->texname[i] = "";
				}
				continue;
			}

			mttmp = getMTexFromMaterial(mat, i);
			if (mttmp) {
				if (mttmp->tex) {
					if ( mttmp->tex->type == TEX_IMAGE ) {
						material->mtexname[i] = mttmp->tex->id.name;
						material->img[i] = mttmp->tex->ima;
						if ( material->img[i] ) {

							material->texname[i] = material->img[i]->id.name;
							material->flag[i] |= ( mttmp->tex->imaflag &TEX_MIPMAP )?MIPMAP:0;
							// -----------------------
							if (material->img[i] && (material->img[i]->flag & IMA_IGNORE_ALPHA) == 0)
								material->flag[i]	|= USEALPHA;
							// -----------------------
							if ( mttmp->tex->imaflag &TEX_CALCALPHA ) {
								material->flag[i]	|= CALCALPHA;
							}
							else if (mttmp->tex->flag &TEX_NEGALPHA) {
								material->flag[i]	|= USENEGALPHA;
							}

							material->color_blend[i] = mttmp->colfac;
							material->flag[i] |= ( mttmp->mapto  & MAP_ALPHA		)?TEXALPHA:0;
							material->flag[i] |= ( mttmp->texflag& MTEX_NEGATIVE	)?TEXNEG:0;

							if (!glslmat && (material->flag[i] & TEXALPHA))
								texalpha = 1;
						}
					}
					else if (mttmp->tex->type == TEX_ENVMAP) {
						if ( mttmp->tex->env->stype == ENV_LOAD ) {
					
							material->mtexname[i]     = mttmp->tex->id.name;
							EnvMap *env = mttmp->tex->env;
							env->ima = mttmp->tex->ima;
							material->cubemap[i] = env;

							if (material->cubemap[i])
							{
								if (!material->cubemap[i]->cube[0])
									BL_Texture::SplitEnvMap(material->cubemap[i]);

								material->texname[i] = material->cubemap[i]->ima->id.name;
								material->mapping[i].mapping |= USEENV;
							}
						}
					}
#if 0				/* this flag isn't used anymore */
					material->flag[i] |= (BKE_animdata_from_id(mat->id) != NULL) ? HASIPO : 0;
#endif
					/// --------------------------------
					// mapping methods
					if (mat->septex & (1 << i)) {
						// If this texture slot isn't in use, set it to disabled to prevent multi-uv problems
						material->mapping[i].mapping = DISABLE;
					} else {
						material->mapping[i].mapping |= ( mttmp->texco  & TEXCO_REFL	)?USEREFL:0;

						if (mttmp->texco & TEXCO_OBJECT) {
							material->mapping[i].mapping |= USEOBJ;
							if (mttmp->object)
								material->mapping[i].objconame = mttmp->object->id.name;
						}
						else if (mttmp->texco &TEXCO_REFL)
							material->mapping[i].mapping |= USEREFL;
						else if (mttmp->texco &(TEXCO_ORCO|TEXCO_GLOB))
							material->mapping[i].mapping |= USEORCO;
						else if (mttmp->texco & TEXCO_UV) {
							/* string may be "" but thats detected as empty after */
							material->mapping[i].uvCoName = mttmp->uvname;
							material->mapping[i].mapping |= USEUV;
						}
						else if (mttmp->texco &TEXCO_NORM)
							material->mapping[i].mapping |= USENORM;
						else if (mttmp->texco &TEXCO_TANGENT)
							material->mapping[i].mapping |= USETANG;
						else
							material->mapping[i].mapping |= DISABLE;

						material->mapping[i].scale[0] = mttmp->size[0];
						material->mapping[i].scale[1] = mttmp->size[1];
						material->mapping[i].scale[2] = mttmp->size[2];
						material->mapping[i].offsets[0] = mttmp->ofs[0];
						material->mapping[i].offsets[1] = mttmp->ofs[1];
						material->mapping[i].offsets[2] = mttmp->ofs[2];

						material->mapping[i].projplane[0] = mttmp->projx;
						material->mapping[i].projplane[1] = mttmp->projy;
						material->mapping[i].projplane[2] = mttmp->projz;
					}
					/// --------------------------------
					
					switch (mttmp->blendtype) {
					case MTEX_BLEND:
						material->blend_mode[i] = BLEND_MIX;
						break;
					case MTEX_MUL:
						material->blend_mode[i] = BLEND_MUL;
						break;
					case MTEX_ADD:
						material->blend_mode[i] = BLEND_ADD;
						break;
					case MTEX_SUB:
						material->blend_mode[i] = BLEND_SUB;
						break;
					case MTEX_SCREEN:
						material->blend_mode[i] = BLEND_SCR;
						break;
					}
					valid_index++;
				}
			}
		}

		// above one tex the switches here
		// are not used
		switch (valid_index) {
		case 0:
			material->IdMode = DEFAULT_BLENDER;
			break;
		case 1:
			material->IdMode = ONETEX;
			break;
		default:
			material->IdMode = GREATERTHAN2;
			break;
		}
		material->SetUsers(mat->id.us);

		material->num_enabled = valid_index;

		material->speccolor[0]	= mat->specr;
		material->speccolor[1]	= mat->specg;
		material->speccolor[2]	= mat->specb;
		material->hard			= (float)mat->har/4.0f;
		material->matcolor[0]	= mat->r;
		material->matcolor[1]	= mat->g;
		material->matcolor[2]	= mat->b;
		material->matcolor[3]	= mat->alpha;
		material->alpha			= mat->alpha;
		material->emit			= mat->emit;
		material->spec_f		= mat->spec;
		material->ref			= mat->ref;
		material->amb			= mat->amb;

		material->ras_mode |= (mat->material_type == MA_TYPE_WIRE)? WIRE: 0;
	}
	else { // No Material
		int valid = 0;

		// check for tface tex to fallback on
		if ( validface ) {
			material->img[0] = (Image*)(tface->tpage);
			// ------------------------
			if (material->img[0]) {
				material->texname[0] = material->img[0]->id.name;
				material->mapping[0].mapping |= ( (material->img[0]->flag & IMA_REFLECT)!=0 )?USEREFL:0;

				/* see if depth of the image is 32bits */
				if (BKE_image_has_alpha(material->img[0])) {
					material->flag[0] |= USEALPHA;
					material->alphablend = GEMAT_ALPHA;
				}
				else
					material->alphablend = GEMAT_SOLID;

				valid++;
			}
		}
		else
			material->alphablend = GEMAT_SOLID;

		material->SetUsers(-1);
		material->num_enabled	= valid;
		material->IdMode		= TEXFACE;
		material->speccolor[0]	= 1.f;
		material->speccolor[1]	= 1.f;
		material->speccolor[2]	= 1.f;
		material->hard			= 35.f;
		material->matcolor[0]	= 0.5f;
		material->matcolor[1]	= 0.5f;
		material->matcolor[2]	= 0.5f;
		material->spec_f		= 0.5f;
		material->ref			= 0.8f;

		// No material - old default TexFace properties
		material->ras_mode |= USE_LIGHT;
	}

	/* No material, what to do? let's see what is in the UV and set the material accordingly
	 * light and visible is always on */
	if ( validface ) {
		material->tile	= tface->tile;
	}
	else {
		// nothing at all
		material->alphablend	= GEMAT_SOLID;
		material->tile		= 0;
	}

	if (validmat && validface) {
		material->alphablend = mat->game.alpha_blend;
	}

	// with ztransp enabled, enforce alpha blending mode
	if (validmat && (mat->mode & MA_TRANSP) && (mat->mode & MA_ZTRANSP) && (material->alphablend == GEMAT_SOLID))
		material->alphablend = GEMAT_ALPHA;

	// always zsort alpha + add
	if ((ELEM3(material->alphablend, GEMAT_ALPHA, GEMAT_ALPHA_SORT, GEMAT_ADD) || texalpha) && (material->alphablend != GEMAT_CLIP )) {
		material->ras_mode |= ALPHA;
		material->ras_mode |= (mat && (mat->game.alpha_blend & GEMAT_ALPHA_SORT))? ZSORT: 0;
	}

	// XXX The RGB values here were meant to be temporary storage for the conversion process,
	// but fonts now make use of them too, so we leave them in for now.
	unsigned int rgb[4];
	GetRGB(use_vcol, mface, mmcol, mat, rgb);

	// swap the material color, so MCol on bitmap font works
	if (validmat && (use_vcol == false) && (mat->game.flag & GEMAT_TEXT))
	{
		rgb[0] = KX_rgbaint2uint_new(rgb[0]);
		rgb[1] = KX_rgbaint2uint_new(rgb[1]);
		rgb[2] = KX_rgbaint2uint_new(rgb[2]);
		rgb[3] = KX_rgbaint2uint_new(rgb[3]);
	}

	if (validmat)
		material->matname	=(mat->id.name);

	if (tface) {
		material->tface		= *tface;
	}
	else {
		memset(&material->tface, 0, sizeof(material->tface));
	}
	material->material	= mat;
	return true;
}

static RAS_MaterialBucket *material_from_mesh(Material *ma, MFace *mface, MTFace *tface, MCol *mcol, MTF_localLayer *layers, int lightlayer, unsigned int *rgb, MT_Point2 uvs[4][RAS_TexVert::MAX_UNIT], const char *tfaceName, KX_Scene* scene, KX_BlenderSceneConverter *converter)
{
	RAS_IPolyMaterial* polymat = converter->FindCachedPolyMaterial(scene, ma);
	BL_Material* bl_mat = converter->FindCachedBlenderMaterial(scene, ma);
	KX_BlenderMaterial* kx_blmat = NULL;

	/* first is the BL_Material */
	if (!bl_mat)
	{
		bl_mat = new BL_Material();

		ConvertMaterial(bl_mat, ma, tface, tfaceName, mface, mcol,
			converter->GetGLSLMaterials());

		if (ma && (ma->mode & MA_FACETEXTURE) == 0)
			converter->CacheBlenderMaterial(scene, ma, bl_mat);
	}

	const bool use_vcol = GetMaterialUseVColor(ma, bl_mat->glslmat);
	GetRGB(use_vcol, mface, mcol, ma, rgb);

	GetUVs(bl_mat, layers, mface, tface, uvs);

	/* then the KX_BlenderMaterial */
	if (polymat == NULL)
	{
		kx_blmat = new KX_BlenderMaterial();

		kx_blmat->Initialize(scene, bl_mat, (ma?&ma->game:NULL), lightlayer);
		polymat = static_cast<RAS_IPolyMaterial*>(kx_blmat);
		if (ma && (ma->mode & MA_FACETEXTURE) == 0)
			converter->CachePolyMaterial(scene, ma, polymat);
	}
	
	// see if a bucket was reused or a new one was created
	// this way only one KX_BlenderMaterial object has to exist per bucket
	bool bucketCreated; 
	RAS_MaterialBucket* bucket = scene->FindBucket(polymat, bucketCreated);
	if (bucketCreated) {
		// this is needed to free up memory afterwards
		converter->RegisterPolyMaterial(polymat);
		converter->RegisterBlenderMaterial(bl_mat);
	}

	return bucket;
}

/* blenderobj can be NULL, make sure its checked for */
RAS_MeshObject* BL_ConvertMesh(Mesh* mesh, Object* blenderobj, KX_Scene* scene, KX_BlenderSceneConverter *converter, bool libloading)
{
	RAS_MeshObject *meshobj;
	int lightlayer = blenderobj ? blenderobj->lay:(1<<20)-1; // all layers if no object.

	// Without checking names, we get some reuse we don't want that can cause
	// problems with material LoDs.
	if (blenderobj && ((meshobj = converter->FindGameMesh(mesh/*, ob->lay*/)) != NULL)) {
		const char *bge_name = meshobj->GetName().ReadPtr();
		const char *blender_name = ((ID *)blenderobj->data)->name + 2;
		if (STREQ(bge_name, blender_name)) {
			return meshobj;
		}
	}

	// Get DerivedMesh data
	DerivedMesh *dm = CDDM_from_mesh(mesh);
	DM_ensure_tessface(dm);

	MVert *mvert = dm->getVertArray(dm);
	int totvert = dm->getNumVerts(dm);

	MFace *mface = dm->getTessFaceArray(dm);
	MTFace *tface = static_cast<MTFace*>(dm->getTessFaceDataArray(dm, CD_MTFACE));
	MCol *mcol = static_cast<MCol*>(dm->getTessFaceDataArray(dm, CD_MCOL));
	float (*tangent)[4] = NULL;
	int totface = dm->getNumTessFaces(dm);
	const char *tfaceName = "";

	if (tface) {
		DM_add_tangent_layer(dm);
		tangent = (float(*)[4])dm->getTessFaceDataArray(dm, CD_TANGENT);
	}

	meshobj = new RAS_MeshObject(mesh);

	// Extract avaiable layers
	MTF_localLayer *layers =  new MTF_localLayer[MAX_MTFACE];
	for (int lay=0; lay<MAX_MTFACE; lay++) {
		layers[lay].face = 0;
		layers[lay].name = "";
	}

	int validLayers = 0;
	for (int i=0; i<dm->faceData.totlayer; i++)
	{
		if (dm->faceData.layers[i].type == CD_MTFACE)
		{
			if (validLayers >= MAX_MTFACE) {
				printf("%s: corrupted mesh %s - too many CD_MTFACE layers\n", __func__, mesh->id.name);
				break;
			}

			layers[validLayers].face = (MTFace*)(dm->faceData.layers[i].data);
			layers[validLayers].name = dm->faceData.layers[i].name;
			if (tface == layers[validLayers].face)
				tfaceName = layers[validLayers].name;
			validLayers++;
		}
	}

	meshobj->SetName(mesh->id.name + 2);
	meshobj->m_sharedvertex_map.resize(totvert);

	Material* ma = 0;
	bool collider = true;
	MT_Point2 uvs[4][RAS_TexVert::MAX_UNIT];
	unsigned int rgb[4] = {0};

	MT_Point3 pt[4];
	MT_Vector3 no[4];
	MT_Vector4 tan[4];

	/* ugh, if there is a less annoying way to do this please use that.
	 * since these are converted from floats to floats, theres no real
	 * advantage to use MT_ types - campbell */
	for (unsigned int i = 0; i < 4; i++) {
		const float zero_vec[4] = {0.0f};
		pt[i].setValue(zero_vec);
		no[i].setValue(zero_vec);
		tan[i].setValue(zero_vec);
	}

	/* we need to manually initialize the uvs (MoTo doesn't do that) [#34550] */
	for (unsigned int i = 0; i < RAS_TexVert::MAX_UNIT; i++) {
		uvs[0][i] = uvs[1][i] = uvs[2][i] = uvs[3][i] = MT_Point2(0.f, 0.f);
	}

	for (int f=0;f<totface;f++,mface++)
	{
		/* get coordinates, normals and tangents */
		pt[0].setValue(mvert[mface->v1].co);
		pt[1].setValue(mvert[mface->v2].co);
		pt[2].setValue(mvert[mface->v3].co);
		if (mface->v4) pt[3].setValue(mvert[mface->v4].co);

		if (mface->flag & ME_SMOOTH) {
			float n0[3], n1[3], n2[3], n3[3];

			normal_short_to_float_v3(n0, mvert[mface->v1].no);
			normal_short_to_float_v3(n1, mvert[mface->v2].no);
			normal_short_to_float_v3(n2, mvert[mface->v3].no);
			no[0] = n0;
			no[1] = n1;
			no[2] = n2;

			if (mface->v4) {
				normal_short_to_float_v3(n3, mvert[mface->v4].no);
				no[3] = n3;
			}
		}
		else {
			float fno[3];

			if (mface->v4)
				normal_quad_v3(fno,mvert[mface->v1].co, mvert[mface->v2].co, mvert[mface->v3].co, mvert[mface->v4].co);
			else
				normal_tri_v3(fno,mvert[mface->v1].co, mvert[mface->v2].co, mvert[mface->v3].co);

			no[0] = no[1] = no[2] = no[3] = MT_Vector3(fno);
		}

		if (tangent) {
			tan[0] = tangent[f*4 + 0];
			tan[1] = tangent[f*4 + 1];
			tan[2] = tangent[f*4 + 2];

			if (mface->v4)
				tan[3] = tangent[f*4 + 3];
		}
		if (blenderobj)
			ma = give_current_material(blenderobj, mface->mat_nr+1);
		else
			ma = mesh->mat ? mesh->mat[mface->mat_nr]:NULL;

		/* ckeck for texface since texface _only_ is used as a fallback */
		if (ma == NULL && tface == NULL) {
			ma= &defmaterial;
		}

		{
			bool visible = true;
			bool twoside = false;

			RAS_MaterialBucket* bucket = material_from_mesh(ma, mface, tface, mcol, layers, lightlayer, rgb, uvs, tfaceName, scene, converter);

			// set render flags
			if (ma)
			{
				visible = ((ma->game.flag & GEMAT_INVISIBLE)==0);
				twoside = ((ma->game.flag  & GEMAT_BACKCULL)==0);
				collider = ((ma->game.flag & GEMAT_NOPHYSICS)==0);
			}
			else {
				visible = true;
				twoside = false;
				collider = true;
			}

			/* mark face as flat, so vertices are split */
			bool flat = (mface->flag & ME_SMOOTH) == 0;
				
			int nverts = (mface->v4)? 4: 3;

			RAS_Polygon *poly = meshobj->AddPolygon(bucket, nverts);

			poly->SetVisible(visible);
			poly->SetCollider(collider);
			poly->SetTwoside(twoside);
			//poly->SetEdgeCode(mface->edcode);

			meshobj->AddVertex(poly,0,pt[0],uvs[0],tan[0],rgb[0],no[0],flat,mface->v1);
			meshobj->AddVertex(poly,1,pt[1],uvs[1],tan[1],rgb[1],no[1],flat,mface->v2);
			meshobj->AddVertex(poly,2,pt[2],uvs[2],tan[2],rgb[2],no[2],flat,mface->v3);

			if (nverts==4)
				meshobj->AddVertex(poly,3,pt[3],uvs[3],tan[3],rgb[3],no[3],flat,mface->v4);
		}

		if (tface) 
			tface++;
		if (mcol)
			mcol+=4;

		for (int lay=0; lay<MAX_MTFACE; lay++)
		{
			MTF_localLayer &layer = layers[lay];
			if (layer.face == 0) break;

			layer.face++;
		}
	}
	// keep meshobj->m_sharedvertex_map for reinstance phys mesh.
	// 2.49a and before it did: meshobj->m_sharedvertex_map.clear();
	// but this didnt save much ram. - Campbell
	meshobj->EndConversion();

	// pre calculate texture generation
	// However, we want to delay this if we're libloading so we can make sure we have the right scene.
	if (!libloading) {
		for (list<RAS_MeshMaterial>::iterator mit = meshobj->GetFirstMaterial();
			mit != meshobj->GetLastMaterial(); ++ mit) {
			mit->m_bucket->GetPolyMaterial()->OnConstruction();
		}
	}

	if (layers)
		delete []layers;
	
	dm->release(dm);

	converter->RegisterGameMesh(meshobj, mesh);
	return meshobj;
}

	
	
static PHY_MaterialProps *CreateMaterialFromBlenderObject(struct Object* blenderobject)
{
	PHY_MaterialProps *materialProps = new PHY_MaterialProps;
	
	MT_assert(materialProps && "Create physics material properties failed");
		
	Material* blendermat = give_current_material(blenderobject, 1);
		
	if (blendermat)
	{
		MT_assert(0.0f <= blendermat->reflect && blendermat->reflect <= 1.0f);
	
		materialProps->m_restitution = blendermat->reflect;
		materialProps->m_friction = blendermat->friction;
		materialProps->m_fh_spring = blendermat->fh;
		materialProps->m_fh_damping = blendermat->xyfrict;
		materialProps->m_fh_distance = blendermat->fhdist;
		materialProps->m_fh_normal = (blendermat->dynamode & MA_FH_NOR) != 0;
	}
	else {
		//give some defaults
		materialProps->m_restitution = 0.f;
		materialProps->m_friction = 0.5;
		materialProps->m_fh_spring = 0.f;
		materialProps->m_fh_damping = 0.f;
		materialProps->m_fh_distance = 0.f;
		materialProps->m_fh_normal = false;

	}
	
	return materialProps;
}

static PHY_ShapeProps *CreateShapePropsFromBlenderObject(struct Object* blenderobject)
{
	PHY_ShapeProps *shapeProps = new PHY_ShapeProps;
	
	MT_assert(shapeProps);
		
	shapeProps->m_mass = blenderobject->mass;
	
//  This needs to be fixed in blender. For now, we use:
	
// in Blender, inertia stands for the size value which is equivalent to
// the sphere radius
	shapeProps->m_inertia = blenderobject->formfactor;
	
	MT_assert(0.0f <= blenderobject->damping && blenderobject->damping <= 1.0f);
	MT_assert(0.0f <= blenderobject->rdamping && blenderobject->rdamping <= 1.0f);
	
	shapeProps->m_lin_drag = 1.0f - blenderobject->damping;
	shapeProps->m_ang_drag = 1.0f - blenderobject->rdamping;
	
	shapeProps->m_friction_scaling[0] = blenderobject->anisotropicFriction[0]; 
	shapeProps->m_friction_scaling[1] = blenderobject->anisotropicFriction[1];
	shapeProps->m_friction_scaling[2] = blenderobject->anisotropicFriction[2];
	shapeProps->m_do_anisotropic = ((blenderobject->gameflag & OB_ANISOTROPIC_FRICTION) != 0);
	
	shapeProps->m_do_fh     = (blenderobject->gameflag & OB_DO_FH) != 0; 
	shapeProps->m_do_rot_fh = (blenderobject->gameflag & OB_ROT_FH) != 0;
	
//	velocity clamping XXX
	shapeProps->m_clamp_vel_min = blenderobject->min_vel;
	shapeProps->m_clamp_vel_max = blenderobject->max_vel;
	
//  Character physics properties
	shapeProps->m_step_height = blenderobject->step_height;
	shapeProps->m_jump_speed = blenderobject->jump_speed;
	shapeProps->m_fall_speed = blenderobject->fall_speed;
	
	return shapeProps;
}

	
	
	
		
//////////////////////////////////////////////////////////
	


static float my_boundbox_mesh(Mesh *me, float *loc, float *size)
{
	MVert *mvert;
	BoundBox *bb;
	float min[3], max[3];
	float mloc[3], msize[3];
	float radius_sq=0.0f, vert_radius_sq, *co;
	int a;
	
	if (me->bb==0) {
		me->bb = BKE_boundbox_alloc_unit();
	}
	bb= me->bb;
	
	INIT_MINMAX(min, max);

	if (!loc) loc= mloc;
	if (!size) size= msize;
	
	mvert= me->mvert;
	for (a = 0; a<me->totvert; a++, mvert++) {
		co = mvert->co;
		
		/* bounds */
		minmax_v3v3_v3(min, max, co);
		
		/* radius */

		vert_radius_sq = len_squared_v3(co);
		if (vert_radius_sq > radius_sq)
			radius_sq = vert_radius_sq;
	}
		
	if (me->totvert) {
		loc[0] = (min[0] + max[0]) / 2.0f;
		loc[1] = (min[1] + max[1]) / 2.0f;
		loc[2] = (min[2] + max[2]) / 2.0f;
		
		size[0] = (max[0] - min[0]) / 2.0f;
		size[1] = (max[1] - min[1]) / 2.0f;
		size[2] = (max[2] - min[2]) / 2.0f;
	}
	else {
		loc[0] = loc[1] = loc[2] = 0.0f;
		size[0] = size[1] = size[2] = 0.0f;
	}
		
	bb->vec[0][0] = bb->vec[1][0] = bb->vec[2][0] = bb->vec[3][0] = loc[0]-size[0];
	bb->vec[4][0] = bb->vec[5][0] = bb->vec[6][0] = bb->vec[7][0] = loc[0]+size[0];
		
	bb->vec[0][1] = bb->vec[1][1] = bb->vec[4][1] = bb->vec[5][1] = loc[1]-size[1];
	bb->vec[2][1] = bb->vec[3][1] = bb->vec[6][1] = bb->vec[7][1] = loc[1]+size[1];

	bb->vec[0][2] = bb->vec[3][2] = bb->vec[4][2] = bb->vec[7][2] = loc[2]-size[2];
	bb->vec[1][2] = bb->vec[2][2] = bb->vec[5][2] = bb->vec[6][2] = loc[2]+size[2];

	return sqrtf_signed(radius_sq);
}


static void my_tex_space_mesh(Mesh *me)
{
	KeyBlock *kb;
	float *fp, loc[3], size[3], min[3], max[3];
	int a;

	my_boundbox_mesh(me, loc, size);
	
	if (me->texflag & ME_AUTOSPACE) {
		if (me->key) {
			kb= me->key->refkey;
			if (kb) {

				INIT_MINMAX(min, max);

				fp= (float *)kb->data;
				for (a=0; a<kb->totelem; a++, fp += 3) {
					minmax_v3v3_v3(min, max, fp);
				}
				if (kb->totelem) {
					loc[0] = (min[0]+max[0])/2.0f; loc[1] = (min[1]+max[1])/2.0f; loc[2] = (min[2]+max[2])/2.0f;
					size[0] = (max[0]-min[0])/2.0f; size[1] = (max[1]-min[1])/2.0f; size[2] = (max[2]-min[2])/2.0f;
				}
				else {
					loc[0] = loc[1] = loc[2] = 0.0;
					size[0] = size[1] = size[2] = 0.0;
				}
				
			}
		}

		copy_v3_v3(me->loc, loc);
		copy_v3_v3(me->size, size);
		me->rot[0] = me->rot[1] = me->rot[2] = 0.0f;

		if (me->size[0] == 0.0f) me->size[0] = 1.0f;
		else if (me->size[0] > 0.0f && me->size[0]< 0.00001f) me->size[0] = 0.00001f;
		else if (me->size[0] < 0.0f && me->size[0]> -0.00001f) me->size[0] = -0.00001f;

		if (me->size[1] == 0.0f) me->size[1] = 1.0f;
		else if (me->size[1] > 0.0f && me->size[1]< 0.00001f) me->size[1] = 0.00001f;
		else if (me->size[1] < 0.0f && me->size[1]> -0.00001f) me->size[1] = -0.00001f;

		if (me->size[2] == 0.0f) me->size[2] = 1.0f;
		else if (me->size[2] > 0.0f && me->size[2]< 0.00001f) me->size[2] = 0.00001f;
		else if (me->size[2] < 0.0f && me->size[2]> -0.00001f) me->size[2] = -0.00001f;
	}
	
}

static void my_get_local_bounds(Object *ob, DerivedMesh *dm, float *center, float *size)
{
	BoundBox *bb= NULL;
	/* uses boundbox, function used by Ketsji */
	switch (ob->type)
	{
		case OB_MESH:
			if (dm)
			{
				float min_r[3], max_r[3];
				INIT_MINMAX(min_r, max_r);
				dm->getMinMax(dm, min_r, max_r);
				size[0] = 0.5f * fabsf(max_r[0] - min_r[0]);
				size[1] = 0.5f * fabsf(max_r[1] - min_r[1]);
				size[2] = 0.5f * fabsf(max_r[2] - min_r[2]);
					
				center[0] = 0.5f * (max_r[0] + min_r[0]);
				center[1] = 0.5f * (max_r[1] + min_r[1]);
				center[2] = 0.5f * (max_r[2] + min_r[2]);
				return;
			} else
			{
				bb= ( (Mesh *)ob->data )->bb;
				if (bb==0) 
				{
					my_tex_space_mesh((struct Mesh *)ob->data);
					bb= ( (Mesh *)ob->data )->bb;
				}
			}
			break;
		case OB_CURVE:
		case OB_SURF:
			center[0] = center[1] = center[2] = 0.0;
			size[0]  = size[1]=size[2]=0.0;
			break;
		case OB_FONT:
			center[0] = center[1] = center[2] = 0.0;
			size[0]  = size[1]=size[2]=1.0;
			break;
		case OB_MBALL:
			bb= ob->bb;
			break;
	}
	
	if (bb==NULL) 
	{
		center[0] = center[1] = center[2] = 0.0;
		size[0] = size[1] = size[2] = 1.0;
	}
	else 
	{
		size[0] = 0.5f * fabsf(bb->vec[0][0] - bb->vec[4][0]);
		size[1] = 0.5f * fabsf(bb->vec[0][1] - bb->vec[2][1]);
		size[2] = 0.5f * fabsf(bb->vec[0][2] - bb->vec[1][2]);

		center[0] = 0.5f * (bb->vec[0][0] + bb->vec[4][0]);
		center[1] = 0.5f * (bb->vec[0][1] + bb->vec[2][1]);
		center[2] = 0.5f * (bb->vec[0][2] + bb->vec[1][2]);
	}
}
	



//////////////////////////////////////////////////////


static void BL_CreateGraphicObjectNew(KX_GameObject* gameobj,
                                      const MT_Point3& localAabbMin,
                                      const MT_Point3& localAabbMax,
                                      KX_Scene* kxscene,
                                      bool isActive,
                                      e_PhysicsEngine physics_engine)
{
	if (gameobj->GetMeshCount() > 0)
	{
		switch (physics_engine)
		{
#ifdef WITH_BULLET
		case UseBullet:
			{
				CcdPhysicsEnvironment* env = (CcdPhysicsEnvironment*)kxscene->GetPhysicsEnvironment();
				assert(env);
				PHY_IMotionState* motionstate = new KX_MotionState(gameobj->GetSGNode());
				CcdGraphicController* ctrl = new CcdGraphicController(env, motionstate);
				gameobj->SetGraphicController(ctrl);
				ctrl->SetNewClientInfo(gameobj->getClientInfo());
				ctrl->SetLocalAabb(localAabbMin, localAabbMax);
				if (isActive) {
					// add first, this will create the proxy handle, only if the object is visible
					if (gameobj->GetVisible())
						env->AddCcdGraphicController(ctrl);
					// update the mesh if there is a deformer, this will also update the bounding box for modifiers
					RAS_Deformer* deformer = gameobj->GetDeformer();
					if (deformer)
						deformer->UpdateBuckets();
				}
			}
			break;
#endif
		default:
			break;
		}
	}
}

static void BL_CreatePhysicsObjectNew(KX_GameObject* gameobj,
                                      struct Object* blenderobject,
                                      RAS_MeshObject* meshobj,
                                      KX_Scene* kxscene,
                                      int activeLayerBitInfo,
                                      e_PhysicsEngine	physics_engine,
                                      KX_BlenderSceneConverter *converter,
                                      bool processCompoundChildren
                                      )

{
	//SYS_SystemHandle syshandle = SYS_GetSystem(); /*unused*/
	//int userigidbody = SYS_GetCommandLineInt(syshandle,"norigidbody",0);
	//bool bRigidBody = (userigidbody == 0);

	// object has physics representation?
	if (!(blenderobject->gameflag & OB_COLLISION)) {
		// Respond to all collisions so that Near sensors work on No Collision
		// objects.
		gameobj->SetUserCollisionGroup(0xff);
		gameobj->SetUserCollisionMask(0xff);
		return;
	}

	gameobj->SetUserCollisionGroup(blenderobject->col_group);
	gameobj->SetUserCollisionMask(blenderobject->col_mask);

	// get Root Parent of blenderobject
	struct Object* parent= blenderobject->parent;
	while (parent && parent->parent) {
		parent= parent->parent;
	}

	bool isCompoundChild = false;
	bool hasCompoundChildren = !parent && (blenderobject->gameflag & OB_CHILD);

	/* When the parent is not OB_DYNAMIC and has no OB_COLLISION then it gets no bullet controller
	 * and cant be apart of the parents compound shape */
	if (parent && (parent->gameflag & (OB_DYNAMIC | OB_COLLISION))) {
		
		if ((parent->gameflag & OB_CHILD) != 0 && (blenderobject->gameflag & OB_CHILD))
		{
			isCompoundChild = true;
		} 
	}
	if (processCompoundChildren != isCompoundChild)
		return;


	PHY_ShapeProps* shapeprops =
			CreateShapePropsFromBlenderObject(blenderobject);

	
	PHY_MaterialProps* smmaterial = 
		CreateMaterialFromBlenderObject(blenderobject);
					
	KX_ObjectProperties objprop;
	objprop.m_lockXaxis = (blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_X_AXIS) !=0;
	objprop.m_lockYaxis = (blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Y_AXIS) !=0;
	objprop.m_lockZaxis = (blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Z_AXIS) !=0;
	objprop.m_lockXRotaxis = (blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_X_ROT_AXIS) !=0;
	objprop.m_lockYRotaxis = (blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Y_ROT_AXIS) !=0;
	objprop.m_lockZRotaxis = (blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Z_ROT_AXIS) !=0;

	objprop.m_isCompoundChild = isCompoundChild;
	objprop.m_hasCompoundChildren = hasCompoundChildren;
	objprop.m_margin = blenderobject->margin;
	
	// ACTOR is now a separate feature
	objprop.m_isactor = (blenderobject->gameflag & OB_ACTOR)!=0;
	objprop.m_dyna = (blenderobject->gameflag & OB_DYNAMIC) != 0;
	objprop.m_softbody = (blenderobject->gameflag & OB_SOFT_BODY) != 0;
	objprop.m_angular_rigidbody = (blenderobject->gameflag & OB_RIGID_BODY) != 0;
	objprop.m_character = (blenderobject->gameflag & OB_CHARACTER) != 0;
	objprop.m_record_animation = (blenderobject->gameflag & OB_RECORD_ANIMATION) != 0;
	
	///contact processing threshold is only for rigid bodies and static geometry, not 'dynamic'
	if (objprop.m_angular_rigidbody || !objprop.m_dyna )
	{
		objprop.m_contactProcessingThreshold = blenderobject->m_contactProcessingThreshold;
	} else
	{
		objprop.m_contactProcessingThreshold = 0.f;
	}

	objprop.m_sensor = (blenderobject->gameflag & OB_SENSOR) != 0;
	
	if (objprop.m_softbody)
	{
		///for game soft bodies
		if (blenderobject->bsoft)
		{
			objprop.m_gamesoftFlag = blenderobject->bsoft->flag;
					///////////////////
			objprop.m_soft_linStiff = blenderobject->bsoft->linStiff;
			objprop.m_soft_angStiff = blenderobject->bsoft->angStiff;		/* angular stiffness 0..1 */
			objprop.m_soft_volume= blenderobject->bsoft->volume;			/* volume preservation 0..1 */

			objprop.m_soft_viterations= blenderobject->bsoft->viterations;		/* Velocities solver iterations */
			objprop.m_soft_piterations= blenderobject->bsoft->piterations;		/* Positions solver iterations */
			objprop.m_soft_diterations= blenderobject->bsoft->diterations;		/* Drift solver iterations */
			objprop.m_soft_citerations= blenderobject->bsoft->citerations;		/* Cluster solver iterations */

			objprop.m_soft_kSRHR_CL= blenderobject->bsoft->kSRHR_CL;		/* Soft vs rigid hardness [0,1] (cluster only) */
			objprop.m_soft_kSKHR_CL= blenderobject->bsoft->kSKHR_CL;		/* Soft vs kinetic hardness [0,1] (cluster only) */
			objprop.m_soft_kSSHR_CL= blenderobject->bsoft->kSSHR_CL;		/* Soft vs soft hardness [0,1] (cluster only) */
			objprop.m_soft_kSR_SPLT_CL= blenderobject->bsoft->kSR_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */

			objprop.m_soft_kSK_SPLT_CL= blenderobject->bsoft->kSK_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
			objprop.m_soft_kSS_SPLT_CL= blenderobject->bsoft->kSS_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
			objprop.m_soft_kVCF= blenderobject->bsoft->kVCF;			/* Velocities correction factor (Baumgarte) */
			objprop.m_soft_kDP= blenderobject->bsoft->kDP;			/* Damping coefficient [0,1] */

			objprop.m_soft_kDG= blenderobject->bsoft->kDG;			/* Drag coefficient [0,+inf] */
			objprop.m_soft_kLF= blenderobject->bsoft->kLF;			/* Lift coefficient [0,+inf] */
			objprop.m_soft_kPR= blenderobject->bsoft->kPR;			/* Pressure coefficient [-inf,+inf] */
			objprop.m_soft_kVC= blenderobject->bsoft->kVC;			/* Volume conversation coefficient [0,+inf] */

			objprop.m_soft_kDF= blenderobject->bsoft->kDF;			/* Dynamic friction coefficient [0,1] */
			objprop.m_soft_kMT= blenderobject->bsoft->kMT;			/* Pose matching coefficient [0,1] */
			objprop.m_soft_kCHR= blenderobject->bsoft->kCHR;			/* Rigid contacts hardness [0,1] */
			objprop.m_soft_kKHR= blenderobject->bsoft->kKHR;			/* Kinetic contacts hardness [0,1] */

			objprop.m_soft_kSHR= blenderobject->bsoft->kSHR;			/* Soft contacts hardness [0,1] */
			objprop.m_soft_kAHR= blenderobject->bsoft->kAHR;			/* Anchors hardness [0,1] */
			objprop.m_soft_collisionflags= blenderobject->bsoft->collisionflags;	/* Vertex/Face or Signed Distance Field(SDF) or Clusters, Soft versus Soft or Rigid */
			objprop.m_soft_numclusteriterations= blenderobject->bsoft->numclusteriterations;	/* number of iterations to refine collision clusters*/
			//objprop.m_soft_welding = blenderobject->bsoft->welding;		/* welding */
			/* disable welding: it doesn't bring any additional stability and it breaks the relation between soft body collision shape and graphic mesh */
			objprop.m_soft_welding = 0.f;
			objprop.m_margin = blenderobject->bsoft->margin;
			objprop.m_contactProcessingThreshold = 0.f;
		} else
		{
			objprop.m_gamesoftFlag = OB_BSB_BENDING_CONSTRAINTS | OB_BSB_SHAPE_MATCHING | OB_BSB_AERO_VPOINT;
			
			objprop.m_soft_linStiff = 0.5;
			objprop.m_soft_angStiff = 1.f;		/* angular stiffness 0..1 */
			objprop.m_soft_volume= 1.f;			/* volume preservation 0..1 */


			objprop.m_soft_viterations= 0;
			objprop.m_soft_piterations= 1;
			objprop.m_soft_diterations= 0;
			objprop.m_soft_citerations= 4;

			objprop.m_soft_kSRHR_CL= 0.1f;
			objprop.m_soft_kSKHR_CL= 1.f;
			objprop.m_soft_kSSHR_CL= 0.5;
			objprop.m_soft_kSR_SPLT_CL= 0.5f;

			objprop.m_soft_kSK_SPLT_CL= 0.5f;
			objprop.m_soft_kSS_SPLT_CL= 0.5f;
			objprop.m_soft_kVCF=  1;
			objprop.m_soft_kDP= 0;

			objprop.m_soft_kDG= 0;
			objprop.m_soft_kLF= 0;
			objprop.m_soft_kPR= 0;
			objprop.m_soft_kVC= 0;

			objprop.m_soft_kDF= 0.2f;
			objprop.m_soft_kMT= 0.05f;
			objprop.m_soft_kCHR= 1.0f;
			objprop.m_soft_kKHR= 0.1f;

			objprop.m_soft_kSHR= 1.f;
			objprop.m_soft_kAHR= 0.7f;
			objprop.m_soft_collisionflags= OB_BSB_COL_SDF_RS + OB_BSB_COL_VF_SS;
			objprop.m_soft_numclusteriterations= 16;
			objprop.m_soft_welding = 0.f;
			objprop.m_margin = 0.f;
			objprop.m_contactProcessingThreshold = 0.f;
		}
	}

	objprop.m_ghost = (blenderobject->gameflag & OB_GHOST) != 0;
	objprop.m_disableSleeping = (blenderobject->gameflag & OB_COLLISION_RESPONSE) != 0;//abuse the OB_COLLISION_RESPONSE flag
	//mmm, for now, taks this for the size of the dynamicobject
	// Blender uses inertia for radius of dynamic object
	objprop.m_radius = blenderobject->inertia;
	objprop.m_in_active_layer = (blenderobject->lay & activeLayerBitInfo) != 0;
	objprop.m_dynamic_parent=NULL;
	objprop.m_isdeformable = ((blenderobject->gameflag2 & 2)) != 0;
	objprop.m_boundclass = objprop.m_dyna?KX_BOUNDSPHERE:KX_BOUNDMESH;
	
	if ((blenderobject->gameflag & OB_SOFT_BODY) && !(blenderobject->gameflag & OB_BOUNDS))
	{
		objprop.m_boundclass = KX_BOUNDMESH;
	}

	if ((blenderobject->gameflag & OB_CHARACTER) && !(blenderobject->gameflag & OB_BOUNDS))
	{
		objprop.m_boundclass = KX_BOUNDSPHERE;
	}

	KX_BoxBounds bb;
	DerivedMesh* dm = NULL;
	if (gameobj->GetDeformer())
		dm = gameobj->GetDeformer()->GetPhysicsMesh();
	my_get_local_bounds(blenderobject,dm,objprop.m_boundobject.box.m_center,bb.m_extends);
	if (blenderobject->gameflag & OB_BOUNDS)
	{
		switch (blenderobject->collision_boundtype)
		{
			case OB_BOUND_BOX:
				objprop.m_boundclass = KX_BOUNDBOX;
				//mmm, has to be divided by 2 to be proper extends
				objprop.m_boundobject.box.m_extends[0]=2.f*bb.m_extends[0];
				objprop.m_boundobject.box.m_extends[1]=2.f*bb.m_extends[1];
				objprop.m_boundobject.box.m_extends[2]=2.f*bb.m_extends[2];
				break;
			case OB_BOUND_CONVEX_HULL:
				if (blenderobject->type == OB_MESH)
				{
					objprop.m_boundclass = KX_BOUNDPOLYTOPE;
					break;
				}
				// Object is not a mesh... fall through OB_BOUND_TRIANGLE_MESH to
				// OB_BOUND_SPHERE
			case OB_BOUND_TRIANGLE_MESH:
				if (blenderobject->type == OB_MESH)
				{
					objprop.m_boundclass = KX_BOUNDMESH;
					break;
				}
				// Object is not a mesh... can't use polyhedron.
				// Fall through and become a sphere.
			case OB_BOUND_SPHERE:
			{
				objprop.m_boundclass = KX_BOUNDSPHERE;
				objprop.m_boundobject.c.m_radius = MT_max(bb.m_extends[0], MT_max(bb.m_extends[1], bb.m_extends[2]));
				break;
			}
			case OB_BOUND_CYLINDER:
			{
				objprop.m_boundclass = KX_BOUNDCYLINDER;
				objprop.m_boundobject.c.m_radius = MT_max(bb.m_extends[0], bb.m_extends[1]);
				objprop.m_boundobject.c.m_height = 2.f*bb.m_extends[2];
				break;
			}
			case OB_BOUND_CONE:
			{
				objprop.m_boundclass = KX_BOUNDCONE;
				objprop.m_boundobject.c.m_radius = MT_max(bb.m_extends[0], bb.m_extends[1]);
				objprop.m_boundobject.c.m_height = 2.f*bb.m_extends[2];
				break;
			}
			case OB_BOUND_CAPSULE:
			{
				objprop.m_boundclass = KX_BOUNDCAPSULE;
				objprop.m_boundobject.c.m_radius = MT_max(bb.m_extends[0], bb.m_extends[1]);
				objprop.m_boundobject.c.m_height = 2.f*(bb.m_extends[2]-objprop.m_boundobject.c.m_radius);
				if (objprop.m_boundobject.c.m_height < 0.f)
					objprop.m_boundobject.c.m_height = 0.f;
				break;
			}
		}
	}

	
	if (parent/* && (parent->gameflag & OB_DYNAMIC)*/) {
		// parented object cannot be dynamic
		KX_GameObject *parentgameobject = converter->FindGameObject(parent);
		objprop.m_dynamic_parent = parentgameobject;
		//cannot be dynamic:
		objprop.m_dyna = false;
		objprop.m_softbody = false;
		shapeprops->m_mass = 0.f;
	}

	
	objprop.m_concave = (blenderobject->collision_boundtype == OB_BOUND_TRIANGLE_MESH);
	
	switch (physics_engine)
	{
#ifdef WITH_BULLET
		case UseBullet:
			KX_ConvertBulletObject(gameobj, meshobj, dm, kxscene, shapeprops, smmaterial, &objprop);
			break;

#endif
		case UseNone:
		default:
			break;
	}
	delete shapeprops;
	delete smmaterial;
	if (dm) {
		dm->needsFree = 1;
		dm->release(dm);
	}
}





static KX_LightObject *gamelight_from_blamp(Object *ob, Lamp *la, unsigned int layerflag, KX_Scene *kxscene, RAS_IRasterizer *rasterizer, KX_BlenderSceneConverter *converter)
{
	RAS_LightObject lightobj;
	KX_LightObject *gamelight;
	
	lightobj.m_att1 = la->att1;
	lightobj.m_att2 = (la->mode & LA_QUAD) ? la->att2 : 0.0f;
	lightobj.m_red = la->r;
	lightobj.m_green = la->g;
	lightobj.m_blue = la->b;
	lightobj.m_distance = la->dist;
	lightobj.m_energy = la->energy;
	lightobj.m_layer = layerflag;
	lightobj.m_spotblend = la->spotblend;
	lightobj.m_spotsize = la->spotsize;
	
	lightobj.m_nodiffuse = (la->mode & LA_NO_DIFF) != 0;
	lightobj.m_nospecular = (la->mode & LA_NO_SPEC) != 0;
	
	bool glslmat = converter->GetGLSLMaterials();

	// in GLSL NEGATIVE LAMP is handled inside the lamp update function
	if (glslmat==0) {
		if (la->mode & LA_NEG)
		{
			lightobj.m_red = -lightobj.m_red;
			lightobj.m_green = -lightobj.m_green;
			lightobj.m_blue = -lightobj.m_blue;
		}
	}
		
	if (la->type==LA_SUN) {
		lightobj.m_type = RAS_LightObject::LIGHT_SUN;
	} else if (la->type==LA_SPOT) {
		lightobj.m_type = RAS_LightObject::LIGHT_SPOT;
	} else {
		lightobj.m_type = RAS_LightObject::LIGHT_NORMAL;
	}

	gamelight = new KX_LightObject(kxscene, KX_Scene::m_callbacks, rasterizer,
		lightobj, glslmat);
	
	return gamelight;
}

static KX_Camera *gamecamera_from_bcamera(Object *ob, KX_Scene *kxscene, KX_BlenderSceneConverter *converter)
{
	Camera* ca = static_cast<Camera*>(ob->data);
	RAS_CameraData camdata(ca->lens, ca->ortho_scale, ca->sensor_x, ca->sensor_y, ca->sensor_fit, ca->clipsta, ca->clipend, ca->type == CAM_PERSP, ca->YF_dofdist);
	KX_Camera *gamecamera;
	
	gamecamera= new KX_Camera(kxscene, KX_Scene::m_callbacks, camdata);
	gamecamera->SetName(ca->id.name + 2);
	
	return gamecamera;
}

static KX_GameObject *gameobject_from_blenderobject(
								Object *ob, 
								KX_Scene *kxscene, 
								RAS_IRasterizer *rendertools,
								KX_BlenderSceneConverter *converter,
								bool libloading) 
{
	KX_GameObject *gameobj = NULL;
	Scene *blenderscene = kxscene->GetBlenderScene();
	
	switch (ob->type) {
	case OB_LAMP:
	{
		KX_LightObject* gamelight = gamelight_from_blamp(ob, static_cast<Lamp*>(ob->data), ob->lay, kxscene, rendertools, converter);
		gameobj = gamelight;
		
		if (blenderscene->lay & ob->lay)
		{
			gamelight->AddRef();
			kxscene->GetLightList()->Add(gamelight);
		}

		break;
	}
	
	case OB_CAMERA:
	{
		KX_Camera* gamecamera = gamecamera_from_bcamera(ob, kxscene, converter);
		gameobj = gamecamera;
		
		//don't add a reference: the camera list in kxscene->m_cameras is not released at the end
		//gamecamera->AddRef();
		kxscene->AddCamera(gamecamera);
		
		break;
	}
	
	case OB_MESH:
	{
		Mesh* mesh = static_cast<Mesh*>(ob->data);
		float center[3], extents[3];
		float radius = my_boundbox_mesh((Mesh*) ob->data, center, extents);
		RAS_MeshObject* meshobj = BL_ConvertMesh(mesh,ob,kxscene,converter, libloading);
		
		// needed for python scripting
		kxscene->GetLogicManager()->RegisterMeshName(meshobj->GetName(),meshobj);

		if (ob->gameflag & OB_NAVMESH)
		{
			gameobj = new KX_NavMeshObject(kxscene,KX_Scene::m_callbacks);
			gameobj->AddMesh(meshobj);
			break;
		}

		gameobj = new BL_DeformableGameObject(ob,kxscene,KX_Scene::m_callbacks);
	
		// set transformation
		gameobj->AddMesh(meshobj);

		// gather levels of detail
		if (BLI_countlist(&ob->lodlevels) > 1) {
			LodLevel *lod = ((LodLevel*)ob->lodlevels.first)->next;
			Mesh* lodmesh = mesh;
			Object* lodmatob = ob;
			gameobj->AddLodMesh(meshobj);
			for (; lod; lod = lod->next) {
				if (!lod->source || lod->source->type != OB_MESH) continue;
				if (lod->flags & OB_LOD_USE_MESH) {
					lodmesh = static_cast<Mesh*>(lod->source->data);
				}
				if (lod->flags & OB_LOD_USE_MAT) {
					lodmatob = lod->source;
				}
				gameobj->AddLodMesh(BL_ConvertMesh(lodmesh, lodmatob, kxscene, converter, libloading));
			}
		}
	
		// for all objects: check whether they want to
		// respond to updates
		bool ignoreActivityCulling =  
			((ob->gameflag2 & OB_NEVER_DO_ACTIVITY_CULLING)!=0);
		gameobj->SetIgnoreActivityCulling(ignoreActivityCulling);
		gameobj->SetOccluder((ob->gameflag & OB_OCCLUDER) != 0, false);

		// we only want obcolor used if there is a material in the mesh
		// that requires it
		Material *mat= NULL;
		bool bUseObjectColor=false;
		
		for (int i=0;i<mesh->totcol;i++) {
			mat=mesh->mat[i];
			if (!mat) break;
			if ((mat->shade_flag & MA_OBCOLOR)) {
				bUseObjectColor = true;
				break;
			}
		}
		if (bUseObjectColor)
			gameobj->SetObjectColor(ob->col);
	
		// two options exists for deform: shape keys and armature
		// only support relative shape key
		bool bHasShapeKey = mesh->key != NULL && mesh->key->type==KEY_RELATIVE;
		bool bHasDvert = mesh->dvert != NULL && ob->defbase.first;
		bool bHasArmature = (BL_ModifierDeformer::HasArmatureDeformer(ob) && ob->parent && ob->parent->type == OB_ARMATURE && bHasDvert);
		bool bHasModifier = BL_ModifierDeformer::HasCompatibleDeformer(ob);
#ifdef WITH_BULLET
		bool bHasSoftBody = (!ob->parent && (ob->gameflag & OB_SOFT_BODY));
#endif
		if (bHasModifier) {
			BL_ModifierDeformer *dcont = new BL_ModifierDeformer((BL_DeformableGameObject *)gameobj,
																kxscene->GetBlenderScene(), ob,	meshobj);
			((BL_DeformableGameObject*)gameobj)->SetDeformer(dcont);
			if (bHasShapeKey && bHasArmature)
				dcont->LoadShapeDrivers(ob->parent);
		} else if (bHasShapeKey) {
			// not that we can have shape keys without dvert! 
			BL_ShapeDeformer *dcont = new BL_ShapeDeformer((BL_DeformableGameObject*)gameobj, 
															ob, meshobj);
			((BL_DeformableGameObject*)gameobj)->SetDeformer(dcont);
			if (bHasArmature)
				dcont->LoadShapeDrivers(ob->parent);
		} else if (bHasArmature) {
			BL_SkinDeformer *dcont = new BL_SkinDeformer((BL_DeformableGameObject*)gameobj,
															ob, meshobj);
			((BL_DeformableGameObject*)gameobj)->SetDeformer(dcont);
		} else if (bHasDvert) {
			// this case correspond to a mesh that can potentially deform but not with the
			// object to which it is attached for the moment. A skin mesh was created in
			// BL_ConvertMesh() so must create a deformer too!
			BL_MeshDeformer *dcont = new BL_MeshDeformer((BL_DeformableGameObject*)gameobj,
														  ob, meshobj);
			((BL_DeformableGameObject*)gameobj)->SetDeformer(dcont);
#ifdef WITH_BULLET
		} else if (bHasSoftBody) {
			KX_SoftBodyDeformer *dcont = new KX_SoftBodyDeformer(meshobj, (BL_DeformableGameObject*)gameobj);
			((BL_DeformableGameObject*)gameobj)->SetDeformer(dcont);
#endif
		}
		
		MT_Point3 min = MT_Point3(center) - MT_Vector3(extents);
		MT_Point3 max = MT_Point3(center) + MT_Vector3(extents);
		SG_BBox bbox = SG_BBox(min, max);
		gameobj->GetSGNode()->SetBBox(bbox);
		gameobj->GetSGNode()->SetRadius(radius);
	
		break;
	}
	
	case OB_ARMATURE:
	{
		bArmature *arm = (bArmature*)ob->data;
		gameobj = new BL_ArmatureObject(
			kxscene,
			KX_Scene::m_callbacks,
			ob,
			kxscene->GetBlenderScene(), // handle
			arm->gevertdeformer
		);
		/* Get the current pose from the armature object and apply it as the rest pose */
		break;
	}
	
	case OB_EMPTY:
	{
		gameobj = new KX_EmptyObject(kxscene,KX_Scene::m_callbacks);
		// set transformation
		break;
	}

	case OB_FONT:
	{
		bool do_color_management = !(blenderscene->gm.flag & GAME_GLSL_NO_COLOR_MANAGEMENT);
		/* font objects have no bounding box */
		gameobj = new KX_FontObject(kxscene,KX_Scene::m_callbacks, rendertools, ob, do_color_management);

		/* add to the list only the visible fonts */
		if ((ob->lay & kxscene->GetBlenderScene()->lay) != 0)
			kxscene->AddFont(static_cast<KX_FontObject*>(gameobj));
		break;
	}

	}
	if (gameobj) 
	{
		gameobj->SetLayer(ob->lay);
		gameobj->SetBlenderObject(ob);
		/* set the visibility state based on the objects render option in the outliner */
		if (ob->restrictflag & OB_RESTRICT_RENDER) gameobj->SetVisible(0, 0);
	}
	return gameobj;
}

struct parentChildLink {
	struct Object* m_blenderchild;
	SG_Node* m_gamechildnode;
};

#include "DNA_constraint_types.h"
//XXX #include "BIF_editconstraint.h"

static bPoseChannel *get_active_posechannel2(Object *ob)
{
	bArmature *arm= (bArmature*)ob->data;
	bPoseChannel *pchan;
	
	/* find active */
	for (pchan= (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (pchan->bone && (pchan->bone == arm->act_bone) && (pchan->bone->layer & arm->layer))
			return pchan;
	}
	
	return NULL;
}

static ListBase *get_active_constraints2(Object *ob)
{
	if (!ob)
		return NULL;

  // XXX - shouldnt we care about the pose data and not the mode???
	if (ob->mode & OB_MODE_POSE) { 
		bPoseChannel *pchan;

		pchan = get_active_posechannel2(ob);
		if (pchan)
			return &pchan->constraints;
	}
	else 
		return &ob->constraints;

	return NULL;
}

static void UNUSED_FUNCTION(RBJconstraints)(Object *ob)//not used
{
	ListBase *conlist;
	bConstraint *curcon;

	conlist = get_active_constraints2(ob);

	if (conlist) {
		for (curcon = (bConstraint *)conlist->first; curcon; curcon = (bConstraint *)curcon->next) {

			printf("%i\n",curcon->type);
		}


	}
}

#include "PHY_IPhysicsEnvironment.h"
#include "PHY_DynamicTypes.h"


static KX_GameObject* getGameOb(STR_String busc,CListValue* sumolist)
{

	for (int j=0;j<sumolist->GetCount();j++)
	{
		KX_GameObject* gameobje = (KX_GameObject*) sumolist->GetValue(j);
		if (gameobje->GetName()==busc)
			return gameobje;
	}
	
	return 0;

}

/* helper for BL_ConvertBlenderObjects, avoids code duplication
 * note: all var names match args are passed from the caller */
static void bl_ConvertBlenderObject_Single(
        KX_BlenderSceneConverter *converter,
        Scene *blenderscene, Object *blenderobject,
        vector<MT_Vector3> &inivel, vector<MT_Vector3> &iniang,
        vector<parentChildLink> &vec_parent_child,
        CListValue* logicbrick_conversionlist,
        CListValue* objectlist, CListValue* inactivelist, CListValue*	sumolist,
        KX_Scene* kxscene, KX_GameObject* gameobj,
        SCA_LogicManager* logicmgr, SCA_TimeEventManager* timemgr,
        bool isInActiveLayer
        )
{
	MT_Point3 posPrev;
	MT_Matrix3x3 angor;
	if (converter->addInitFromFrame) blenderscene->r.cfra=blenderscene->r.sfra;

	MT_Point3 pos(
		blenderobject->loc[0]+blenderobject->dloc[0],
		blenderobject->loc[1]+blenderobject->dloc[1],
		blenderobject->loc[2]+blenderobject->dloc[2]
	);

	MT_Matrix3x3 rotation;
	float rotmat[3][3];
	BKE_object_rot_to_mat3(blenderobject, rotmat, FALSE);
	rotation.setValue3x3((float*)rotmat);

	MT_Vector3 scale(blenderobject->size);

	if (converter->addInitFromFrame) {//rcruiz
		blenderscene->r.cfra=blenderscene->r.sfra-1;
		//XXX update_for_newframe();
		MT_Vector3 tmp=pos-MT_Point3(blenderobject->loc[0]+blenderobject->dloc[0],
		                             blenderobject->loc[1]+blenderobject->dloc[1],
		                             blenderobject->loc[2]+blenderobject->dloc[2]
		                             );

		float rotmatPrev[3][3];
		BKE_object_rot_to_mat3(blenderobject, rotmatPrev, FALSE);

		float eulxyz[3], eulxyzPrev[3];
		mat3_to_eul(eulxyz, rotmat);
		mat3_to_eul(eulxyzPrev, rotmatPrev);

		double fps = (double) blenderscene->r.frs_sec/
		        (double) blenderscene->r.frs_sec_base;

		tmp.scale(fps, fps, fps);
		inivel.push_back(tmp);
		tmp[0]=eulxyz[0]-eulxyzPrev[0];
		tmp[1]=eulxyz[1]-eulxyzPrev[1];
		tmp[2]=eulxyz[2]-eulxyzPrev[2];
		tmp.scale(fps, fps, fps);
		iniang.push_back(tmp);
		blenderscene->r.cfra=blenderscene->r.sfra;
		//XXX update_for_newframe();
	}

	gameobj->NodeSetLocalPosition(pos);
	gameobj->NodeSetLocalOrientation(rotation);
	gameobj->NodeSetLocalScale(scale);
	gameobj->NodeUpdateGS(0);

	sumolist->Add(gameobj->AddRef());

	BL_ConvertProperties(blenderobject,gameobj,timemgr,kxscene,isInActiveLayer);

	gameobj->SetName(blenderobject->id.name + 2);

	// update children/parent hierarchy
	if ((blenderobject->parent != 0)&&(!converter->addInitFromFrame))
	{
		// blender has an additional 'parentinverse' offset in each object
		SG_Callbacks callback(NULL,NULL,NULL,KX_Scene::KX_ScenegraphUpdateFunc,KX_Scene::KX_ScenegraphRescheduleFunc);
		SG_Node* parentinversenode = new SG_Node(NULL,kxscene,callback);

		// define a normal parent relationship for this node.
		KX_NormalParentRelation * parent_relation = KX_NormalParentRelation::New();
		parentinversenode->SetParentRelation(parent_relation);

		parentChildLink pclink;
		pclink.m_blenderchild = blenderobject;
		pclink.m_gamechildnode = parentinversenode;
		vec_parent_child.push_back(pclink);

		float* fl = (float*) blenderobject->parentinv;
		MT_Transform parinvtrans(fl);
		parentinversenode->SetLocalPosition(parinvtrans.getOrigin());
		// problem here: the parent inverse transform combines scaling and rotation
		// in the basis but the scenegraph needs separate rotation and scaling.
		// This is not important for OpenGL (it uses 4x4 matrix) but it is important
		// for the physic engine that needs a separate scaling
		//parentinversenode->SetLocalOrientation(parinvtrans.getBasis());

		// Extract the rotation and the scaling from the basis
		MT_Matrix3x3 ori(parinvtrans.getBasis());
		MT_Vector3 x(ori.getColumn(0));
		MT_Vector3 y(ori.getColumn(1));
		MT_Vector3 z(ori.getColumn(2));
		MT_Vector3 parscale(x.length(), y.length(), z.length());
		if (!MT_fuzzyZero(parscale[0]))
			x /= parscale[0];
		if (!MT_fuzzyZero(parscale[1]))
			y /= parscale[1];
		if (!MT_fuzzyZero(parscale[2]))
			z /= parscale[2];
		ori.setColumn(0, x);
		ori.setColumn(1, y);
		ori.setColumn(2, z);
		parentinversenode->SetLocalOrientation(ori);
		parentinversenode->SetLocalScale(parscale);

		parentinversenode->AddChild(gameobj->GetSGNode());
	}

	// needed for python scripting
	logicmgr->RegisterGameObjectName(gameobj->GetName(),gameobj);

	// needed for group duplication
	logicmgr->RegisterGameObj(blenderobject, gameobj);
	for (int i = 0; i < gameobj->GetMeshCount(); i++)
		logicmgr->RegisterGameMeshName(gameobj->GetMesh(i)->GetName(), blenderobject);

	converter->RegisterGameObject(gameobj, blenderobject);
	// this was put in rapidly, needs to be looked at more closely
	// only draw/use objects in active 'blender' layers

	logicbrick_conversionlist->Add(gameobj->AddRef());

	if (converter->addInitFromFrame) {
		posPrev=gameobj->NodeGetWorldPosition();
		angor=gameobj->NodeGetWorldOrientation();
	}
	if (isInActiveLayer)
	{
		objectlist->Add(gameobj->AddRef());
		//tf.Add(gameobj->GetSGNode());

		gameobj->NodeUpdateGS(0);
		gameobj->AddMeshUser();

	}
	else
	{
		//we must store this object otherwise it will be deleted
		//at the end of this function if it is not a root object
		inactivelist->Add(gameobj->AddRef());
	}

	if (converter->addInitFromFrame) {
		gameobj->NodeSetLocalPosition(posPrev);
		gameobj->NodeSetLocalOrientation(angor);
	}
}


// convert blender objects into ketsji gameobjects
void BL_ConvertBlenderObjects(struct Main* maggie,
							  KX_Scene* kxscene,
							  KX_KetsjiEngine* ketsjiEngine,
							  e_PhysicsEngine	physics_engine,
							  RAS_IRasterizer* rendertools,
							  RAS_ICanvas* canvas,
							  KX_BlenderSceneConverter* converter,
							  bool alwaysUseExpandFraming,
							  bool libloading
							  )
{

#define BL_CONVERTBLENDEROBJECT_SINGLE                                 \
	bl_ConvertBlenderObject_Single(converter,                          \
	                               blenderscene, blenderobject,        \
	                               inivel, iniang,                     \
	                               vec_parent_child,                   \
	                               logicbrick_conversionlist,          \
	                               objectlist, inactivelist, sumolist, \
	                               kxscene, gameobj,                   \
	                               logicmgr, timemgr,                  \
	                               isInActiveLayer                     \
	                               )



	Scene *blenderscene = kxscene->GetBlenderScene();
	// for SETLOOPER
	Scene *sce_iter;
	Base *base;

	// Get the frame settings of the canvas.
	// Get the aspect ratio of the canvas as designed by the user.

	RAS_FrameSettings::RAS_FrameType frame_type;
	int aspect_width;
	int aspect_height;
	vector<MT_Vector3> inivel,iniang;
	set<Group*> grouplist;	// list of groups to be converted
	set<Object*> allblobj;	// all objects converted
	set<Object*> groupobj;	// objects from groups (never in active layer)

	// This is bad, but we use this to make sure the first time this is called
	// is not in a separate thread.
	BL_Texture::GetMaxUnits();

	if (alwaysUseExpandFraming) {
		frame_type = RAS_FrameSettings::e_frame_extend;
		aspect_width = canvas->GetWidth();
		aspect_height = canvas->GetHeight();
	} else {
		if (blenderscene->gm.framing.type == SCE_GAMEFRAMING_BARS) {
			frame_type = RAS_FrameSettings::e_frame_bars;
		} else if (blenderscene->gm.framing.type == SCE_GAMEFRAMING_EXTEND) {
			frame_type = RAS_FrameSettings::e_frame_extend;
		} else {
			frame_type = RAS_FrameSettings::e_frame_scale;
		}
		
		aspect_width  = (int)(blenderscene->r.xsch * blenderscene->r.xasp);
		aspect_height = (int)(blenderscene->r.ysch * blenderscene->r.yasp);
	}
	
	RAS_FrameSettings frame_settings(
		frame_type,
		blenderscene->gm.framing.col[0],
		blenderscene->gm.framing.col[1],
		blenderscene->gm.framing.col[2],
		aspect_width,
		aspect_height
	);
	kxscene->SetFramingType(frame_settings);

	kxscene->SetGravity(MT_Vector3(0,0, -blenderscene->gm.gravity));
	
	/* set activity culling parameters */
	kxscene->SetActivityCulling( (blenderscene->gm.mode & WO_ACTIVITY_CULLING) != 0);
	kxscene->SetActivityCullingRadius(blenderscene->gm.activityBoxRadius);
	kxscene->SetDbvtCulling((blenderscene->gm.mode & WO_DBVT_CULLING) != 0);
	
	// no occlusion culling by default
	kxscene->SetDbvtOcclusionRes(0);

	int activeLayerBitInfo = blenderscene->lay;
	
	// list of all object converted, active and inactive
	CListValue*	sumolist = new CListValue();
	
	vector<parentChildLink> vec_parent_child;
	
	CListValue* objectlist = kxscene->GetObjectList();
	CListValue* inactivelist = kxscene->GetInactiveList();
	CListValue* parentlist = kxscene->GetRootParentList();
	
	SCA_LogicManager* logicmgr = kxscene->GetLogicManager();
	SCA_TimeEventManager* timemgr = kxscene->GetTimeEventManager();
	
	CListValue* logicbrick_conversionlist = new CListValue();
	
	//SG_TreeFactory tf;
	
	// Convert actions to actionmap
	bAction *curAct;
	for (curAct = (bAction*)maggie->action.first; curAct; curAct=(bAction*)curAct->id.next)
	{
		logicmgr->RegisterActionName(curAct->id.name + 2, curAct);
	}

	SetDefaultLightMode(blenderscene);
	// Let's support scene set.
	// Beware of name conflict in linked data, it will not crash but will create confusion
	// in Python scripting and in certain actuators (replace mesh). Linked scene *should* have
	// no conflicting name for Object, Object data and Action.
	for (SETLOOPER(blenderscene, sce_iter, base))
	{
		Object* blenderobject = base->object;
		allblobj.insert(blenderobject);

		KX_GameObject* gameobj = gameobject_from_blenderobject(
										base->object, 
										kxscene, 
										rendertools, 
										converter,
										libloading);
										
		bool isInActiveLayer = (blenderobject->lay & activeLayerBitInfo) !=0;
		bool addobj=true;
		
		if (converter->addInitFromFrame)
			if (!isInActiveLayer)
				addobj=false;

		if (gameobj)
		{
			if (addobj)
			{	/* macro calls object conversion funcs */
				BL_CONVERTBLENDEROBJECT_SINGLE;

				if (gameobj->IsDupliGroup()) {
					grouplist.insert(blenderobject->dup_group);
				}
			}

			/* Note about memory leak issues:
			 * When a CValue derived class is created, m_refcount is initialized to 1
			 * so the class must be released after being used to make sure that it won't
			 * hang in memory. If the object needs to be stored for a long time,
			 * use AddRef() so that this Release() does not free the object.
			 * Make sure that for any AddRef() there is a Release()!!!!
			 * Do the same for any object derived from CValue, CExpression and NG_NetworkMessage
			 */
			gameobj->Release();
		}
	}

	if (!grouplist.empty())
	{
		// now convert the group referenced by dupli group object
		// keep track of all groups already converted
		set<Group*> allgrouplist = grouplist;
		set<Group*> tempglist;
		// recurse
		while (!grouplist.empty())
		{
			set<Group*>::iterator git;
			tempglist.clear();
			tempglist.swap(grouplist);
			for (git=tempglist.begin(); git!=tempglist.end(); git++)
			{
				Group* group = *git;
				GroupObject* go;
				for (go=(GroupObject*)group->gobject.first; go; go=(GroupObject*)go->next)
				{
					Object* blenderobject = go->ob;
					if (converter->FindGameObject(blenderobject) == NULL)
					{
						allblobj.insert(blenderobject);
						groupobj.insert(blenderobject);
						KX_GameObject* gameobj = gameobject_from_blenderobject(
														blenderobject, 
														kxscene, 
														rendertools, 
														converter,
														libloading);
										
						// this code is copied from above except that
						// object from groups are never in active layer
						bool isInActiveLayer = false;
						bool addobj=true;
						
						if (converter->addInitFromFrame)
							if (!isInActiveLayer)
								addobj=false;

						if (gameobj)
						{
							if (addobj)
							{	/* macro calls object conversion funcs */
								BL_CONVERTBLENDEROBJECT_SINGLE;
							}

							if (gameobj->IsDupliGroup())
							{
								if (allgrouplist.insert(blenderobject->dup_group).second)
								{
									grouplist.insert(blenderobject->dup_group);
								}
							}


							/* see comment above re: mem leaks */
							gameobj->Release();
						}
					}
				}
			}
		}
	}

	// non-camera objects not supported as camera currently
	if (blenderscene->camera && blenderscene->camera->type == OB_CAMERA) {
		KX_Camera *gamecamera= (KX_Camera*) converter->FindGameObject(blenderscene->camera);
		
		if (gamecamera)
			kxscene->SetActiveCamera(gamecamera);
	}

	//	Set up armatures
	set<Object*>::iterator oit;
	for (oit=allblobj.begin(); oit!=allblobj.end(); oit++)
	{
		Object* blenderobj = *oit;
		if (blenderobj->type==OB_MESH) {
			Mesh *me = (Mesh*)blenderobj->data;
	
			if (me->dvert) {
				BL_DeformableGameObject *obj = (BL_DeformableGameObject*)converter->FindGameObject(blenderobj);

				if (obj && BL_ModifierDeformer::HasArmatureDeformer(blenderobj) && blenderobj->parent && blenderobj->parent->type==OB_ARMATURE) {
					KX_GameObject *par = converter->FindGameObject(blenderobj->parent);
					if (par && obj->GetDeformer())
						((BL_SkinDeformer*)obj->GetDeformer())->SetArmature((BL_ArmatureObject*) par);
				}
			}
		}
	}
	
	// create hierarchy information
	int i;
	vector<parentChildLink>::iterator pcit;
	
	for (pcit = vec_parent_child.begin();!(pcit==vec_parent_child.end());++pcit)
	{
	
		struct Object* blenderchild = pcit->m_blenderchild;
		struct Object* blenderparent = blenderchild->parent;
		KX_GameObject* parentobj = converter->FindGameObject(blenderparent);
		KX_GameObject* childobj = converter->FindGameObject(blenderchild);

		assert(childobj);

		if (!parentobj || objectlist->SearchValue(childobj) != objectlist->SearchValue(parentobj))
		{
			// special case: the parent and child object are not in the same layer. 
			// This weird situation is used in Apricot for test purposes.
			// Resolve it by not converting the child
			childobj->GetSGNode()->DisconnectFromParent();
			delete pcit->m_gamechildnode;
			// Now destroy the child object but also all its descendent that may already be linked
			// Remove the child reference in the local list!
			// Note: there may be descendents already if the children of the child were processed
			//       by this loop before the child. In that case, we must remove the children also
			CListValue* childrenlist = childobj->GetChildrenRecursive();
			childrenlist->Add(childobj->AddRef());
			for ( i=0;i<childrenlist->GetCount();i++)
			{
				KX_GameObject* obj = static_cast<KX_GameObject*>(childrenlist->GetValue(i));
				if (sumolist->RemoveValue(obj))
					obj->Release();
				if (logicbrick_conversionlist->RemoveValue(obj))
					obj->Release();
			}
			childrenlist->Release();
			
			// now destroy recursively
			converter->UnregisterGameObject(childobj); // removing objects during conversion make sure this runs too
			kxscene->RemoveObject(childobj);
			
			continue;
		}

		switch (blenderchild->partype)
		{
			case PARVERT1:
			{
				// creat a new vertex parent relationship for this node.
				KX_VertexParentRelation * vertex_parent_relation = KX_VertexParentRelation::New();
				pcit->m_gamechildnode->SetParentRelation(vertex_parent_relation);
				break;
			}
			case PARSLOW:
			{
				// creat a new slow parent relationship for this node.
				KX_SlowParentRelation * slow_parent_relation = KX_SlowParentRelation::New(blenderchild->sf);
				pcit->m_gamechildnode->SetParentRelation(slow_parent_relation);
				break;
			}
			case PARBONE:
			{
				// parent this to a bone
				Bone *parent_bone = BKE_armature_find_bone_name(BKE_armature_from_object(blenderchild->parent),
				                                                blenderchild->parsubstr);

				if (parent_bone) {
					KX_BoneParentRelation *bone_parent_relation = KX_BoneParentRelation::New(parent_bone);
					pcit->m_gamechildnode->SetParentRelation(bone_parent_relation);
				}
			
				break;
			}
			case PARSKEL: // skinned - ignore
				break;
			case PAROBJECT:
			case PARCURVE:
			case PARKEY:
			case PARVERT3:
			default:
				// unhandled
				break;
		}
	
		parentobj->	GetSGNode()->AddChild(pcit->m_gamechildnode);
	}
	vec_parent_child.clear();
	
	// find 'root' parents (object that has not parents in SceneGraph)
	for (i=0;i<sumolist->GetCount();++i)
	{
		KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
		if (gameobj->GetSGNode()->GetSGParent() == 0)
		{
			parentlist->Add(gameobj->AddRef());
			gameobj->NodeUpdateGS(0);
		}
	}

	// create graphic controller for culling
	if (kxscene->GetDbvtCulling())
	{
		bool occlusion = false;
		for (i=0; i<sumolist->GetCount();i++)
		{
			KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
			if (gameobj->GetMeshCount() > 0) 
			{
				MT_Point3 box[2];
				gameobj->GetSGNode()->BBox().getmm(box, MT_Transform::Identity());
				// box[0] is the min, box[1] is the max
				bool isactive = objectlist->SearchValue(gameobj);
				BL_CreateGraphicObjectNew(gameobj,box[0],box[1],kxscene,isactive,physics_engine);
				if (gameobj->GetOccluder())
					occlusion = true;
			}
		}
		if (occlusion)
			kxscene->SetDbvtOcclusionRes(blenderscene->gm.occlusionRes);
	}
	if (blenderscene->world)
		kxscene->GetPhysicsEnvironment()->SetNumTimeSubSteps(blenderscene->gm.physubstep);

	// now that the scenegraph is complete, let's instantiate the deformers.
	// We need that to create reusable derived mesh and physic shapes
	for (i=0;i<sumolist->GetCount();++i)
	{
		KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
		if (gameobj->GetDeformer())
			gameobj->GetDeformer()->UpdateBuckets();
	}

	// Set up armature constraints
	for (i=0;i<sumolist->GetCount();++i)
	{
		KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
		if (gameobj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE)
			((BL_ArmatureObject*)gameobj)->LoadConstraints(converter);
	}

	bool processCompoundChildren = false;

	// create physics information
	for (i=0;i<sumolist->GetCount();i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
		struct Object* blenderobject = gameobj->GetBlenderObject();
		int nummeshes = gameobj->GetMeshCount();
		RAS_MeshObject* meshobj = 0;
		if (nummeshes > 0)
		{
			meshobj = gameobj->GetMesh(0);
		}
		int layerMask = (groupobj.find(blenderobject) == groupobj.end()) ? activeLayerBitInfo : 0;
		BL_CreatePhysicsObjectNew(gameobj,blenderobject,meshobj,kxscene,layerMask,physics_engine,converter,processCompoundChildren);
	}

	processCompoundChildren = true;
	// create physics information
	for (i=0;i<sumolist->GetCount();i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
		struct Object* blenderobject = gameobj->GetBlenderObject();
		int nummeshes = gameobj->GetMeshCount();
		RAS_MeshObject* meshobj = 0;
		if (nummeshes > 0)
		{
			meshobj = gameobj->GetMesh(0);
		}
		int layerMask = (groupobj.find(blenderobject) == groupobj.end()) ? activeLayerBitInfo : 0;
		BL_CreatePhysicsObjectNew(gameobj,blenderobject,meshobj,kxscene,layerMask,physics_engine,converter,processCompoundChildren);
	}
	
	//set ini linearVel and int angularVel //rcruiz
	if (converter->addInitFromFrame)
		for (i=0;i<sumolist->GetCount();i++)
		{
			KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
			if (gameobj->IsDynamic()) {
				gameobj->setLinearVelocity(inivel[i],false);
				gameobj->setAngularVelocity(iniang[i],false);
			}
		
		
		}

		// create physics joints
	for (i=0;i<sumolist->GetCount();i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
		struct Object* blenderobject = gameobj->GetBlenderObject();
		ListBase *conlist;
		bConstraint *curcon;
		conlist = get_active_constraints2(blenderobject);

		if ((gameobj->GetLayer()&activeLayerBitInfo)==0)
			continue;

		if (conlist) {
			for (curcon = (bConstraint *)conlist->first; curcon; curcon = (bConstraint *)curcon->next) {
				if (curcon->type==CONSTRAINT_TYPE_RIGIDBODYJOINT) {

					bRigidBodyJointConstraint *dat=(bRigidBodyJointConstraint *)curcon->data;

					if (!dat->child && !(curcon->flag & CONSTRAINT_OFF)) {

						PHY_IPhysicsController* physctr2 = 0;

						if (dat->tar)
						{
							KX_GameObject *gotar=getGameOb(dat->tar->id.name+2,sumolist);
							if (gotar && ((gotar->GetLayer()&activeLayerBitInfo)!=0) && gotar->GetPhysicsController())
								physctr2 = gotar->GetPhysicsController();
						}

						if (gameobj->GetPhysicsController())
						{
							PHY_IPhysicsController* physctrl = gameobj->GetPhysicsController();
							//we need to pass a full constraint frame, not just axis

							//localConstraintFrameBasis
							MT_Matrix3x3 localCFrame(MT_Vector3(dat->axX,dat->axY,dat->axZ));
							MT_Vector3 axis0 = localCFrame.getColumn(0);
							MT_Vector3 axis1 = localCFrame.getColumn(1);
							MT_Vector3 axis2 = localCFrame.getColumn(2);
								
							int constraintId = kxscene->GetPhysicsEnvironment()->CreateConstraint(physctrl,physctr2,(PHY_ConstraintType)dat->type,(float)dat->pivX,
								(float)dat->pivY,(float)dat->pivZ,
								(float)axis0.x(),(float)axis0.y(),(float)axis0.z(),
								(float)axis1.x(),(float)axis1.y(),(float)axis1.z(),
								(float)axis2.x(),(float)axis2.y(),(float)axis2.z(),dat->flag);
							if (constraintId)
							{
								//if it is a generic 6DOF constraint, set all the limits accordingly
								if (dat->type == PHY_GENERIC_6DOF_CONSTRAINT)
								{
									int dof;
									int dofbit=1;
									for (dof=0;dof<6;dof++)
									{
										if (dat->flag & dofbit)
										{
											kxscene->GetPhysicsEnvironment()->SetConstraintParam(constraintId,dof,dat->minLimit[dof],dat->maxLimit[dof]);
										} else
										{
											//minLimit > maxLimit means free(disabled limit) for this degree of freedom
											kxscene->GetPhysicsEnvironment()->SetConstraintParam(constraintId,dof,1,-1);
										}
										dofbit<<=1;
									}
								}
								else if (dat->type == PHY_CONE_TWIST_CONSTRAINT)
								{
									int dof;
									int dofbit = 1<<3; // bitflag use_angular_limit_x
									
									for (dof=3;dof<6;dof++)
									{
										if (dat->flag & dofbit)
										{
											kxscene->GetPhysicsEnvironment()->SetConstraintParam(constraintId,dof,dat->minLimit[dof],dat->maxLimit[dof]);
										}
										else
										{
											//maxLimit < 0 means free(disabled limit) for this degree of freedom
											kxscene->GetPhysicsEnvironment()->SetConstraintParam(constraintId,dof,1,-1);
										}
										dofbit<<=1;
									}
								}
								else if (dat->type == PHY_LINEHINGE_CONSTRAINT)
								{
									int dof = 3; // dof for angular x
									int dofbit = 1<<3; // bitflag use_angular_limit_x
									
									if (dat->flag & dofbit)
									{
										kxscene->GetPhysicsEnvironment()->SetConstraintParam(constraintId,dof,
												dat->minLimit[dof],dat->maxLimit[dof]);
									} else
									{
										//minLimit > maxLimit means free(disabled limit) for this degree of freedom
										kxscene->GetPhysicsEnvironment()->SetConstraintParam(constraintId,dof,1,-1);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	sumolist->Release();

	// convert world
	KX_WorldInfo* worldinfo = new BlenderWorldInfo(blenderscene, blenderscene->world);
	converter->RegisterWorldInfo(worldinfo);
	kxscene->SetWorldInfo(worldinfo);

	//create object representations for obstacle simulation
	KX_ObstacleSimulation* obssimulation = kxscene->GetObstacleSimulation();
	if (obssimulation)
	{
		for ( i=0;i<objectlist->GetCount();i++)
		{
			KX_GameObject* gameobj = static_cast<KX_GameObject*>(objectlist->GetValue(i));
			struct Object* blenderobject = gameobj->GetBlenderObject();
			if (blenderobject->gameflag & OB_HASOBSTACLE)
			{
				obssimulation->AddObstacleForObj(gameobj);
			}
		}
	}

	//process navigation mesh objects
	for ( i=0; i<objectlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(objectlist->GetValue(i));
		struct Object* blenderobject = gameobj->GetBlenderObject();
		if (blenderobject->type==OB_MESH && (blenderobject->gameflag & OB_NAVMESH))
		{
			KX_NavMeshObject* navmesh = static_cast<KX_NavMeshObject*>(gameobj);
			navmesh->SetVisible(0, true);
			navmesh->BuildNavMesh();
			if (obssimulation)
				obssimulation->AddObstaclesForNavMesh(navmesh);
		}
	}
	for ( i=0; i<inactivelist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(inactivelist->GetValue(i));
		struct Object* blenderobject = gameobj->GetBlenderObject();
		if (blenderobject->type==OB_MESH && (blenderobject->gameflag & OB_NAVMESH))
		{
			KX_NavMeshObject* navmesh = static_cast<KX_NavMeshObject*>(gameobj);
			navmesh->SetVisible(0, true);
		}
	}

#define CONVERT_LOGIC
#ifdef CONVERT_LOGIC
	// convert logic bricks, sensors, controllers and actuators
	for (i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = gameobj->GetBlenderObject();
		int layerMask = (groupobj.find(blenderobj) == groupobj.end()) ? activeLayerBitInfo : 0;
		bool isInActiveLayer = (blenderobj->lay & layerMask)!=0;
		BL_ConvertActuators(maggie->name, blenderobj,gameobj,logicmgr,kxscene,ketsjiEngine,layerMask,isInActiveLayer,converter);
	}
	for ( i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = gameobj->GetBlenderObject();
		int layerMask = (groupobj.find(blenderobj) == groupobj.end()) ? activeLayerBitInfo : 0;
		bool isInActiveLayer = (blenderobj->lay & layerMask)!=0;
		BL_ConvertControllers(blenderobj,gameobj,logicmgr, layerMask,isInActiveLayer,converter);
	}
	for ( i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = gameobj->GetBlenderObject();
		int layerMask = (groupobj.find(blenderobj) == groupobj.end()) ? activeLayerBitInfo : 0;
		bool isInActiveLayer = (blenderobj->lay & layerMask)!=0;
		BL_ConvertSensors(blenderobj,gameobj,logicmgr,kxscene,ketsjiEngine,layerMask,isInActiveLayer,canvas,converter);
		// set the init state to all objects
		gameobj->SetInitState((blenderobj->init_state)?blenderobj->init_state:blenderobj->state);
	}
	// apply the initial state to controllers, only on the active objects as this registers the sensors
	for ( i=0;i<objectlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(objectlist->GetValue(i));
		gameobj->ResetState();
	}

#endif //CONVERT_LOGIC

	logicbrick_conversionlist->Release();
	
	// Calculate the scene btree -
	// too slow - commented out.
	//kxscene->SetNodeTree(tf.MakeTree());

	// instantiate dupli group, we will loop trough the object
	// that are in active layers. Note that duplicating group
	// has the effect of adding objects at the end of objectlist.
	// Only loop through the first part of the list.
	int objcount = objectlist->GetCount();
	for (i=0;i<objcount;i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*) objectlist->GetValue(i);
		if (gameobj->IsDupliGroup())
		{
			kxscene->DupliGroupRecurse(gameobj, 0);
		}
	}

	KX_Camera *activecam = kxscene->GetActiveCamera();
	MT_Scalar distance = (activecam)? activecam->GetCameraFar() - activecam->GetCameraNear(): 100.0f;
	RAS_BucketManager *bucketmanager = kxscene->GetBucketManager();
	bucketmanager->OptimizeBuckets(distance);
}

SCA_IInputDevice::KX_EnumInputs ConvertKeyCode(int key_code)
{
	return gReverseKeyTranslateTable[key_code];
}
