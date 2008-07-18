/**
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Convert blender data to ketsji
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable : 4786)
#endif

#include <math.h>

#include "BL_BlenderDataConversion.h"
#include "KX_BlenderGL.h"
#include "KX_BlenderScalarInterpolator.h"

#include "RAS_IPolygonMaterial.h"
#include "KX_PolygonMaterial.h"

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
#include "RAS_IRenderTools.h"
#include "BL_Material.h"
#include "KX_BlenderMaterial.h"
#include "BL_Texture.h"

#include "DNA_action_types.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BL_SkinMeshObject.h"
#include "BL_ShapeDeformer.h"
#include "BL_SkinDeformer.h"
#include "BL_MeshDeformer.h"
//#include "BL_ArmatureController.h"

#include "BlenderWorldInfo.h"

#include "KX_KetsjiEngine.h"
#include "KX_BlenderSceneConverter.h"

#include"SND_Scene.h"
#include "SND_SoundListener.h"

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

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "MT_Point3.h"

extern "C" {
	#include "BKE_customdata.h"
}

#include "BKE_material.h" /* give_current_material */
/* end of blender include block */

#include "KX_BlenderInputDevice.h"
#include "KX_ConvertProperties.h"
#include "KX_HashedPtr.h"


#include "KX_ScalarInterpolator.h"

#include "KX_IpoConvert.h"
#include "SYS_System.h"

#include "SG_Node.h"
#include "SG_BBox.h"
#include "SG_Tree.h"

// defines USE_ODE to choose physics engine
#include "KX_ConvertPhysicsObject.h"


// This file defines relationships between parents and children
// in the game engine.

#include "KX_SG_NodeRelationships.h"
#include "KX_SG_BoneParentNodeRelationship.h"

#include "BL_ArmatureObject.h"
#include "BL_DeformableGameObject.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "BSE_headerbuttons.h"
void update_for_newframe();
//void scene_update_for_newframe(struct Scene *sce, unsigned int lay);
//#include "BKE_ipo.h"
//void do_all_data_ipos(void);
#ifdef __cplusplus
}
#endif

static int default_face_mode = TF_DYNAMIC;

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

static void SetDefaultFaceType(Scene* scene)
{
	default_face_mode = TF_DYNAMIC;
	Scene *sce;
	Base *base;

	for(SETLOOPER(scene,base))
	{
		if (base->object->type == OB_LAMP)
		{
			default_face_mode = TF_DYNAMIC|TF_LIGHT;
			return;
		}
	}
}


// --
static void GetRGB(short type,
	MFace* mface,
	MCol* mmcol,
	Material *mat,
	unsigned int &c0, 
	unsigned int &c1, 
	unsigned int &c2, 
	unsigned int &c3)
{
	unsigned int color = 0xFFFFFFFFL;
	switch(type)
	{
		case 0:	// vertex colors
		{
			if(mmcol) {
				c0 = KX_Mcol2uint_new(mmcol[0]);
				c1 = KX_Mcol2uint_new(mmcol[1]);
				c2 = KX_Mcol2uint_new(mmcol[2]);
				if (mface->v4)
					c3 = KX_Mcol2uint_new(mmcol[3]);
			}else // backup white
			{
				c0 = KX_rgbaint2uint_new(color);
				c1 = KX_rgbaint2uint_new(color);
				c2 = KX_rgbaint2uint_new(color);	
				if (mface->v4)
					c3 = KX_rgbaint2uint_new( color );
			}
		} break;
		
	
		case 1: // material rgba
		{
			if (mat) {
				union {
					unsigned char cp[4];
					unsigned int integer;
				} col_converter;
				col_converter.cp[3] = (unsigned char) (mat->r*255.0);
				col_converter.cp[2] = (unsigned char) (mat->g*255.0);
				col_converter.cp[1] = (unsigned char) (mat->b*255.0);
				col_converter.cp[0] = (unsigned char) (mat->alpha*255.0);
				color = col_converter.integer;
			}
			c0 = KX_rgbaint2uint_new(color);
			c1 = KX_rgbaint2uint_new(color);
			c2 = KX_rgbaint2uint_new(color);	
			if (mface->v4)
				c3 = KX_rgbaint2uint_new(color);
		} break;
		
		default: // white
		{
			c0 = KX_rgbaint2uint_new(color);
			c1 = KX_rgbaint2uint_new(color);
			c2 = KX_rgbaint2uint_new(color);	
			if (mface->v4)
				c3 = KX_rgbaint2uint_new(color);
		} break;
	}
}

typedef struct MTF_localLayer
{
	MTFace *face;
	char *name;
}MTF_localLayer;

// ------------------------------------
BL_Material* ConvertMaterial(
	Mesh* mesh, 
	Material *mat, 
	MTFace* tface,  
	const char *tfaceName,
	MFace* mface, 
	MCol* mmcol, 
	int lightlayer, 
	Object* blenderobj,
	MTF_localLayer *layers,
	bool glslmat)
{
	//this needs some type of manager
	BL_Material *material = new BL_Material();

	int numchan =	-1;
	bool validmat	= (mat!=0);
	bool validface	= (mesh->mtface && tface);
	
	short type = 0;
	if( validmat )
		type = 1; // material color 
	
	material->IdMode = DEFAULT_BLENDER;

	// --------------------------------
	if(validmat) {

		// use vertex colors by explicitly setting
		if(mat->mode &MA_VERTEXCOLP || glslmat)
			type = 0;

		// use lighting?
		material->ras_mode |= ( mat->mode & MA_SHLESS )?0:USE_LIGHT;
		MTex *mttmp = 0;
		numchan = getNumTexChannels(mat);
		int valid_index = 0;
		
		// use the face texture if
		// 1) it is set in the buttons
		// 2) we have a face texture and a material but no valid texture in slot 1
		bool facetex = false;
		if(validface && mat->mode &MA_FACETEXTURE) 
			facetex = true;
		if(validface && !mat->mtex[0])
			facetex = true;
		if(validface && mat->mtex[0]) {
			MTex *tmp = mat->mtex[0];
			if(!tmp->tex || tmp->tex && !tmp->tex->ima )
				facetex = true;
		}
		numchan = numchan>MAXTEX?MAXTEX:numchan;
	
		// foreach MTex
		for(int i=0; i<numchan; i++) {
			// use face tex

			if(i==0 && facetex ) {
				Image*tmp = (Image*)(tface->tpage);
				if(tmp) {
					material->img[i] = tmp;
					material->texname[i] = material->img[i]->id.name;
					material->flag[i] |= ( tface->transp  &TF_ALPHA	)?USEALPHA:0;
					material->flag[i] |= ( tface->transp  &TF_ADD	)?CALCALPHA:0;
					material->ras_mode|= ( tface->transp  &(TF_ADD | TF_ALPHA))?TRANSP:0;
					if(material->img[i]->flag & IMA_REFLECT)
						material->mapping[i].mapping |= USEREFL;
					else
					{
						mttmp = getImageFromMaterial( mat, i );
						if(mttmp && mttmp->texco &TEXCO_UV)
						{
							STR_String uvName = mttmp->uvname;

							if (!uvName.IsEmpty())
								material->mapping[i].uvCoName = mttmp->uvname;
							else
								material->mapping[i].uvCoName = "";
						}
						material->mapping[i].mapping |= USEUV;
					}

					if(material->ras_mode & USE_LIGHT)
						material->ras_mode &= ~USE_LIGHT;
					if(tface->mode & TF_LIGHT)
						material->ras_mode |= USE_LIGHT;

					valid_index++;
				}
				else {
					material->img[i] = 0;
					material->texname[i] = "";
				}
				continue;
			}

			mttmp = getImageFromMaterial( mat, i );
			if( mttmp ) {
				if( mttmp->tex ) {
					if( mttmp->tex->type == TEX_IMAGE ) {
						material->mtexname[i] = mttmp->tex->id.name;
						material->img[i] = mttmp->tex->ima;
						if( material->img[i] ) {

							material->texname[i] = material->img[i]->id.name;
							material->flag[i] |= ( mttmp->tex->imaflag &TEX_MIPMAP )?MIPMAP:0;
							// -----------------------
							if( mttmp->tex->imaflag &TEX_USEALPHA ) {
								material->flag[i]	|= USEALPHA;
							}
							// -----------------------
							else if( mttmp->tex->imaflag &TEX_CALCALPHA ) {
								material->flag[i]	|= CALCALPHA;
							}
							else if(mttmp->tex->flag &TEX_NEGALPHA) {
								material->flag[i]	|= USENEGALPHA;
							}

							material->color_blend[i] = mttmp->colfac;
							material->flag[i] |= ( mttmp->mapto  & MAP_ALPHA		)?TEXALPHA:0;
							material->flag[i] |= ( mttmp->texflag& MTEX_NEGATIVE	)?TEXNEG:0;

						}
					}
					else if(mttmp->tex->type == TEX_ENVMAP) {
						if( mttmp->tex->env->stype == ENV_LOAD ) {
					
							material->mtexname[i]     = mttmp->tex->id.name;
							EnvMap *env = mttmp->tex->env;
							env->ima = mttmp->tex->ima;
							material->cubemap[i] = env;

							if (material->cubemap[i])
							{
								if (!material->cubemap[i]->cube[0])
									BL_Texture::SplitEnvMap(material->cubemap[i]);

								material->texname[i]= material->cubemap[i]->ima->id.name;
								material->mapping[i].mapping |= USEENV;
							}
						}
					}
					material->flag[i] |= (mat->ipo!=0)?HASIPO:0;
					/// --------------------------------
					// mapping methods
					material->mapping[i].mapping |= ( mttmp->texco  & TEXCO_REFL	)?USEREFL:0;
					
					if(mttmp->texco & TEXCO_OBJECT) {
						material->mapping[i].mapping |= USEOBJ;
						if(mttmp->object)
							material->mapping[i].objconame = mttmp->object->id.name;
					}
					else if(mttmp->texco &TEXCO_REFL)
						material->mapping[i].mapping |= USEREFL;
					else if(mttmp->texco &(TEXCO_ORCO|TEXCO_GLOB))
						material->mapping[i].mapping |= USEORCO;
					else if(mttmp->texco &TEXCO_UV)
					{
						STR_String uvName = mttmp->uvname;

						if (!uvName.IsEmpty())
							material->mapping[i].uvCoName = mttmp->uvname;
						else
							material->mapping[i].uvCoName = "";
						material->mapping[i].mapping |= USEUV;
					}
					else if(mttmp->texco &TEXCO_NORM)
						material->mapping[i].mapping |= USENORM;
					else if(mttmp->texco &TEXCO_TANGENT)
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
					/// --------------------------------
					
					switch( mttmp->blendtype ) {
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
		switch(valid_index) {
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

		// set alpha testing without z-sorting
		if( ( validface && (!(tface->transp &~ TF_CLIP))) && mat->mode & MA_ZTRA) {
			// sets the RAS_IPolyMaterial::m_flag |RAS_FORCEALPHA
			// this is so we don't have the overhead of the z-sorting code
			material->ras_mode|=ALPHA_TEST;
		}
		else{
			// use regular z-sorting
			material->ras_mode |= ((mat->mode & MA_ZTRA) != 0)?ZSORT:0;
		}
		material->ras_mode |= ((mat->mode & MA_WIRE) != 0)?WIRE:0;
	}
	else {
		int valid = 0;

		// check for tface tex to fallback on
		if( validface ){

			// no light bugfix
			if(tface->mode) material->ras_mode |= USE_LIGHT;

			material->img[0] = (Image*)(tface->tpage);
			// ------------------------
			if(material->img[0]) {
				material->texname[0] = material->img[0]->id.name;
				material->mapping[0].mapping |= ( (material->img[0]->flag & IMA_REFLECT)!=0 )?USEREFL:0;
				material->flag[0] |= ( tface->transp  &TF_ALPHA	)?USEALPHA:0;
				material->flag[0] |= ( tface->transp  &TF_ADD	)?CALCALPHA:0;
				material->ras_mode|= ( tface->transp  & (TF_ADD|TF_ALPHA))?TRANSP:0;
				valid++;
			}
		}
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
	}
	MT_Point2 uv[4];
	MT_Point2 uv2[4];
	const char *uvName = "", *uv2Name = "";

	uv[0]= uv[1]= uv[2]= uv[3]= MT_Point2(0.0f, 0.0f);
	uv2[0]= uv2[1]= uv2[2]= uv2[3]= MT_Point2(0.0f, 0.0f);

	if( validface ) {

		material->ras_mode |= !( 
			(mface->flag & ME_HIDE)	||
			(tface->mode & TF_INVISIBLE)
			)?POLY_VIS:0;

		material->ras_mode |= ( (tface->mode & TF_DYNAMIC)!= 0 )?COLLIDER:0;
		material->transp = tface->transp;
		
		if(tface->transp&~TF_CLIP)
			material->ras_mode |= TRANSP;

		material->tile	= tface->tile;
		material->mode	= tface->mode;
			
		uv[0]	= MT_Point2(tface->uv[0]);
		uv[1]	= MT_Point2(tface->uv[1]);
		uv[2]	= MT_Point2(tface->uv[2]);

		if (mface->v4) 
			uv[3]	= MT_Point2(tface->uv[3]);

		uvName = tfaceName;
	} 
	else {
		// nothing at all
		material->ras_mode |= (COLLIDER|POLY_VIS| (validmat?0:USE_LIGHT));
		material->mode		= default_face_mode;	
		material->transp	= TF_SOLID;
		material->tile		= 0;
	}



	// get uv sets
	if(validmat) 
	{
		bool isFirstSet = true;

		// only two sets implemented, but any of the eight 
		// sets can make up the two layers
		for (int vind = 0; vind<material->num_enabled; vind++)
		{
			BL_Mapping &map = material->mapping[vind];
			if (map.uvCoName.IsEmpty())
				isFirstSet = false;
			else
			{
				for (int lay=0; lay<MAX_MTFACE; lay++)
				{
					MTF_localLayer& layer = layers[lay];
					if (layer.face == 0) break;

					if (strcmp(map.uvCoName.ReadPtr(), layer.name)==0)
					{
						MT_Point2 uvSet[4];

						uvSet[0]	= MT_Point2(layer.face->uv[0]);
						uvSet[1]	= MT_Point2(layer.face->uv[1]);
						uvSet[2]	= MT_Point2(layer.face->uv[2]);

						if (mface->v4) 
							uvSet[3]	= MT_Point2(layer.face->uv[3]);
						else
							uvSet[3]	= MT_Point2(0.0f, 0.0f);

						if (isFirstSet)
						{
							uv[0] = uvSet[0]; uv[1] = uvSet[1];
							uv[2] = uvSet[2]; uv[3] = uvSet[3];
							isFirstSet = false;
							uvName = layer.name;
						}
						else
						{
							uv2[0] = uvSet[0]; uv2[1] = uvSet[1];
							uv2[2] = uvSet[2]; uv2[3] = uvSet[3];
							map.mapping |= USECUSTOMUV;
							uv2Name = layer.name;
						}
					}
				}
			}
		}
	}

	unsigned int rgb[4];
	GetRGB(type,mface,mmcol,mat,rgb[0],rgb[1],rgb[2], rgb[3]);

	// swap the material color, so MCol on TF_BMFONT works
	if (validmat && type==1 && (tface && tface->mode & TF_BMFONT))
	{
		rgb[0] = KX_rgbaint2uint_new(rgb[0]);
		rgb[1] = KX_rgbaint2uint_new(rgb[1]);
		rgb[2] = KX_rgbaint2uint_new(rgb[2]);
		rgb[3] = KX_rgbaint2uint_new(rgb[3]);
	}

	material->SetConversionRGB(rgb);
	material->SetConversionUV(uvName, uv);
	material->SetConversionUV2(uv2Name, uv2);

	material->ras_mode |= (mface->v4==0)?TRIANGLE:0;
	if(validmat)
		material->matname	=(mat->id.name);

	material->tface		= tface;
	material->material	= mat;
	return material;
}


static void BL_ComputeTriTangentSpace(const MT_Vector3 &v1, const MT_Vector3 &v2, const MT_Vector3 &v3, 
	const MT_Vector2 &uv1, const MT_Vector2 &uv2, const MT_Vector2 &uv3, 
	MFace* mface, MT_Vector3 *tan1, MT_Vector3 *tan2)
{
		MT_Vector3 dx1(v2 - v1), dx2(v3 - v1);
		MT_Vector2 duv1(uv2 - uv1), duv2(uv3 - uv1);
		
		MT_Scalar r = 1.0 / (duv1.x() * duv2.y() - duv2.x() * duv1.y());
		duv1 *= r;
		duv2 *= r;
		MT_Vector3 sdir(duv2.y() * dx1 - duv1.y() * dx2);
		MT_Vector3 tdir(duv1.x() * dx2 - duv2.x() * dx1);
		
		tan1[mface->v1] += sdir;
		tan1[mface->v2] += sdir;
		tan1[mface->v3] += sdir;
		
		tan2[mface->v1] += tdir;
		tan2[mface->v2] += tdir;
		tan2[mface->v3] += tdir;
}

static MT_Vector4*  BL_ComputeMeshTangentSpace(Mesh* mesh)
{
	MFace* mface = static_cast<MFace*>(mesh->mface);
	MTFace* tface = static_cast<MTFace*>(mesh->mtface);

	MT_Vector3 *tan1 = new MT_Vector3[mesh->totvert];
	MT_Vector3 *tan2 = new MT_Vector3[mesh->totvert];
	
	int v;
	for (v = 0; v < mesh->totvert; v++)
	{
		tan1[v] = MT_Vector3(0.0, 0.0, 0.0);
		tan2[v] = MT_Vector3(0.0, 0.0, 0.0);
	}
	
	for (int p = 0; p < mesh->totface; p++, mface++, tface++)
	{
		MT_Vector3 	v1(mesh->mvert[mface->v1].co),
				v2(mesh->mvert[mface->v2].co),
				v3(mesh->mvert[mface->v3].co);
				
		MT_Vector2	uv1(tface->uv[0]),
				uv2(tface->uv[1]),
				uv3(tface->uv[2]);
				
		BL_ComputeTriTangentSpace(v1, v2, v3, uv1, uv2, uv3, mface, tan1, tan2);
		if (mface->v4)
		{
			MT_Vector3 v4(mesh->mvert[mface->v4].co);
			MT_Vector2 uv4(tface->uv[3]);
			
			BL_ComputeTriTangentSpace(v1, v3, v4, uv1, uv3, uv4, mface, tan1, tan2);
		}
	}
	
	MT_Vector4 *tangent = new MT_Vector4[mesh->totvert];
	for (v = 0; v < mesh->totvert; v++)
	{
		const MT_Vector3 no(mesh->mvert[v].no[0]/32767.0, 
					mesh->mvert[v].no[1]/32767.0, 
					mesh->mvert[v].no[2]/32767.0);
		// Gram-Schmidt orthogonalize
		MT_Vector3 t(tan1[v] - no.cross(no.cross(tan1[v])));
		if (!MT_fuzzyZero(t))
			t /= t.length();

		tangent[v].x() = t.x();
		tangent[v].y() = t.y();
		tangent[v].z() = t.z();
		// Calculate handedness
		tangent[v].w() = no.dot(tan1[v].cross(tan2[v])) < 0.0 ? -1.0 : 1.0;
	}
	
	delete [] tan1;
	delete [] tan2;
	
	return tangent;
}

RAS_MeshObject* BL_ConvertMesh(Mesh* mesh, Object* blenderobj, RAS_IRenderTools* rendertools, KX_Scene* scene, KX_BlenderSceneConverter *converter)
{
	RAS_MeshObject *meshobj;
	bool	skinMesh = false;
	
	int lightlayer = blenderobj->lay;
	
	MFace* mface = static_cast<MFace*>(mesh->mface);
	MTFace* tface = static_cast<MTFace*>(mesh->mtface);
	const char *tfaceName = "";
	MCol* mmcol = mesh->mcol;
	MT_assert(mface || mesh->totface == 0);


	// Determine if we need to make a skinned mesh
	if (mesh->dvert || mesh->key){
		meshobj = new BL_SkinMeshObject(mesh, lightlayer);
		skinMesh = true;
	}
	else {
		meshobj = new RAS_MeshObject(mesh, lightlayer);
	}
	MT_Vector4 *tangent = 0;
	if (tface)
		tangent = BL_ComputeMeshTangentSpace(mesh);
	

	// Extract avaiable layers
	MTF_localLayer *layers =  new MTF_localLayer[MAX_MTFACE];
	for (int lay=0; lay<MAX_MTFACE; lay++)
	{
		layers[lay].face = 0;
		layers[lay].name = "";
	}


	int validLayers = 0;
	for (int i=0; i<mesh->fdata.totlayer; i++)
	{
		if (mesh->fdata.layers[i].type == CD_MTFACE)
		{
			assert(validLayers <= 8);

			layers[validLayers].face = (MTFace*)mesh->fdata.layers[i].data;;
			layers[validLayers].name = mesh->fdata.layers[i].name;
			if(tface == layers[validLayers].face)
				tfaceName = layers[validLayers].name;
			validLayers++;
		}
	}

	meshobj->SetName(mesh->id.name);
	meshobj->m_xyz_index_to_vertex_index_mapping.resize(mesh->totvert);
	for (int f=0;f<mesh->totface;f++,mface++)
	{
		
		bool collider = true;
		
		// only add valid polygons
		if (mface->v3)
		{
			MT_Point2 uv0(0.0,0.0),uv1(0.0,0.0),uv2(0.0,0.0),uv3(0.0,0.0);
			MT_Point2 uv20(0.0,0.0),uv21(0.0,0.0),uv22(0.0,0.0),uv23(0.0,0.0);
			// rgb3 is set from the adjoint face in a square
			unsigned int rgb0,rgb1,rgb2,rgb3 = 0;
			MT_Vector3	no0(mesh->mvert[mface->v1].no[0], mesh->mvert[mface->v1].no[1], mesh->mvert[mface->v1].no[2]),
					no1(mesh->mvert[mface->v2].no[0], mesh->mvert[mface->v2].no[1], mesh->mvert[mface->v2].no[2]),
					no2(mesh->mvert[mface->v3].no[0], mesh->mvert[mface->v3].no[1], mesh->mvert[mface->v3].no[2]),
					no3(0.0, 0.0, 0.0);
			MT_Point3	pt0(mesh->mvert[mface->v1].co),
					pt1(mesh->mvert[mface->v2].co),
					pt2(mesh->mvert[mface->v3].co),
					pt3(0.0, 0.0, 0.0);
			MT_Vector4	tan0(0.0, 0.0, 0.0, 0.0),
					tan1(0.0, 0.0, 0.0, 0.0),
					tan2(0.0, 0.0, 0.0, 0.0),
					tan3(0.0, 0.0, 0.0, 0.0);

			no0 /= 32767.0;
			no1 /= 32767.0;
			no2 /= 32767.0;
			if (mface->v4)
			{
				pt3 = MT_Point3(mesh->mvert[mface->v4].co);
				no3 = MT_Vector3(mesh->mvert[mface->v4].no[0], mesh->mvert[mface->v4].no[1], mesh->mvert[mface->v4].no[2]);
				no3 /= 32767.0;
			}
	
			if(!(mface->flag & ME_SMOOTH))
			{
				MT_Vector3 norm = ((pt1-pt0).cross(pt2-pt0)).safe_normalized();
				norm[0] = ((int) (10*norm[0]))/10.0;
				norm[1] = ((int) (10*norm[1]))/10.0;
				norm[2] = ((int) (10*norm[2]))/10.0;
				no0=no1=no2=no3= norm;
	
			}
		
			{
				Material* ma = 0;
				bool polyvisible = true;
				RAS_IPolyMaterial* polymat = NULL;
				BL_Material *bl_mat = NULL;

				if(converter->GetMaterials()) 
				{	
					if(mesh->totcol > 1)
						ma = mesh->mat[mface->mat_nr];
					else 
						ma = give_current_material(blenderobj, 1);

					bl_mat = ConvertMaterial(mesh, ma, tface, tfaceName, mface, mmcol, lightlayer, blenderobj, layers, converter->GetGLSLMaterials());
					// set the index were dealing with
					bl_mat->material_index =  (int)mface->mat_nr;

					polyvisible = ((bl_mat->ras_mode & POLY_VIS)!=0);
					collider = ((bl_mat->ras_mode & COLLIDER)!=0);
					
					polymat = new KX_BlenderMaterial(scene, bl_mat, skinMesh, lightlayer, blenderobj );
					
					unsigned int rgb[4];
					bl_mat->GetConversionRGB(rgb);
					rgb0 = rgb[0]; rgb1 = rgb[1];
					rgb2 = rgb[2]; rgb3 = rgb[3];
					MT_Point2 uv[4];
					bl_mat->GetConversionUV(uv);
					uv0 = uv[0]; uv1 = uv[1];
					uv2 = uv[2]; uv3 = uv[3];

					bl_mat->GetConversionUV2(uv);
					uv20 = uv[0]; uv21 = uv[1];
					uv22 = uv[2]; uv23 = uv[3];

					if(tangent){
						tan0 = tangent[mface->v1];
						tan1 = tangent[mface->v2];
						tan2 = tangent[mface->v3];
						if (mface->v4)
							tan3 = tangent[mface->v4];
					}
				}
				else
				{
					ma = give_current_material(blenderobj, 1);

					Image* bima = ((mesh->mtface && tface) ? (Image*) tface->tpage : NULL);
		
					STR_String imastr = 
						((mesh->mtface && tface) ? 
						(bima? (bima)->id.name : "" ) : "" );
			
					char transp=0;
					short mode=0, tile=0;
					int	tilexrep=4,tileyrep = 4;
					
					if (bima)
					{
						tilexrep = bima->xrep;
						tileyrep = bima->yrep;
				
					}

					if (mesh->mtface && tface)
					{
						// Use texface colors if available
						//TF_DYNAMIC means the polygon is a collision face
						collider = ((tface->mode & TF_DYNAMIC) != 0);
						transp = tface->transp &~ TF_CLIP;
						tile = tface->tile;
						mode = tface->mode;
						
						polyvisible = !((mface->flag & ME_HIDE)||(tface->mode & TF_INVISIBLE));
						
						uv0 = MT_Point2(tface->uv[0]);
						uv1 = MT_Point2(tface->uv[1]);
						uv2 = MT_Point2(tface->uv[2]);
		
						if (mface->v4)
							uv3 = MT_Point2(tface->uv[3]);
					} 
					else
					{
						// no texfaces, set COLLSION true and everything else FALSE
						
						mode = default_face_mode;	
						transp = TF_SOLID;
						tile = 0;
					}

					if (mmcol)
					{
						// Use vertex colors
						rgb0 = KX_Mcol2uint_new(mmcol[0]);
						rgb1 = KX_Mcol2uint_new(mmcol[1]);
						rgb2 = KX_Mcol2uint_new(mmcol[2]);
						
						if (mface->v4)
							rgb3 = KX_Mcol2uint_new(mmcol[3]);
					}
					else {
						// no vertex colors: take from material if we have one,
						// otherwise set to white
						unsigned int color = 0xFFFFFFFFL;

						if (ma)
						{
							union
							{
								unsigned char cp[4];
								unsigned int integer;
							} col_converter;
							
							col_converter.cp[3] = (unsigned char) (ma->r*255.0);
							col_converter.cp[2] = (unsigned char) (ma->g*255.0);
							col_converter.cp[1] = (unsigned char) (ma->b*255.0);
							col_converter.cp[0] = (unsigned char) (ma->alpha*255.0);
							
							color = col_converter.integer;
						}
	
						rgb0 = KX_rgbaint2uint_new(color);
						rgb1 = KX_rgbaint2uint_new(color);
						rgb2 = KX_rgbaint2uint_new(color);	
						
						if (mface->v4)
							rgb3 = KX_rgbaint2uint_new(color);
					}
					
					bool istriangle = (mface->v4==0);
					bool zsort = ma?(ma->mode & MA_ZTRA) != 0:false;
					
					polymat = new KX_PolygonMaterial(imastr, ma,
						tile, tilexrep, tileyrep, 
						mode, transp, zsort, lightlayer, istriangle, blenderobj, tface, (unsigned int*)mmcol);
		
					if (ma)
					{
						polymat->m_specular = MT_Vector3(ma->specr, ma->specg, ma->specb)*ma->spec;
						polymat->m_shininess = (float)ma->har/4.0; // 0 < ma->har <= 512
						polymat->m_diffuse = MT_Vector3(ma->r, ma->g, ma->b)*(ma->emit + ma->ref);

					} else
					{
						polymat->m_specular = MT_Vector3(0.0f,0.0f,0.0f);
						polymat->m_shininess = 35.0;
					}
				}
	
				// see if a bucket was reused or a new one was created
				// this way only one KX_BlenderMaterial object has to exist per bucket
				bool bucketCreated; 
				RAS_MaterialBucket* bucket = scene->FindBucket(polymat, bucketCreated);
				if (bucketCreated) {
					// this is needed to free up memory afterwards
					converter->RegisterPolyMaterial(polymat);
					if(converter->GetMaterials()) {
						converter->RegisterBlenderMaterial(bl_mat);
					}
				} else {
					// delete the material objects since they are no longer needed
					// from now on, use the polygon material from the material bucket
					delete polymat;
					if(converter->GetMaterials()) {
						delete bl_mat;
					}
					polymat = bucket->GetPolyMaterial();
				}
							 
				int nverts = mface->v4?4:3;
				int vtxarray = meshobj->FindVertexArray(nverts,polymat);
				RAS_Polygon* poly = new RAS_Polygon(bucket,polyvisible,nverts,vtxarray);

				bool flat;

				if (skinMesh) {
					/* If the face is set to solid, all fnors are the same */
					if (mface->flag & ME_SMOOTH)
						flat = false;
					else
						flat = true;
				}
				else
					flat = false;

				poly->SetVertex(0,meshobj->FindOrAddVertex(vtxarray,pt0,uv0,uv20,tan0,rgb0,no0,flat,polymat,mface->v1));
				poly->SetVertex(1,meshobj->FindOrAddVertex(vtxarray,pt1,uv1,uv21,tan1,rgb1,no1,flat,polymat,mface->v2));
				poly->SetVertex(2,meshobj->FindOrAddVertex(vtxarray,pt2,uv2,uv22,tan2,rgb2,no2,flat,polymat,mface->v3));
				if (nverts==4)
					poly->SetVertex(3,meshobj->FindOrAddVertex(vtxarray,pt3,uv3,uv23,tan3,rgb3,no3,flat,polymat,mface->v4));

				meshobj->AddPolygon(poly);
				if (poly->IsCollider())
				{
					RAS_TriangleIndex idx;
					idx.m_index[0] = mface->v1;
					idx.m_index[1] = mface->v2;
					idx.m_index[2] = mface->v3;
					idx.m_collider = collider;
					meshobj->m_triangle_indices.push_back(idx);
					if (nverts==4)
					{
					idx.m_index[0] = mface->v1;
					idx.m_index[1] = mface->v3;
					idx.m_index[2] = mface->v4;
					idx.m_collider = collider;
					meshobj->m_triangle_indices.push_back(idx);
					}
				}
				
//				poly->SetVisibleWireframeEdges(mface->edcode);
				poly->SetCollider(collider);
			}
		}
		if (tface) 
			tface++;
		if (mmcol)
			mmcol+=4;

		for (int lay=0; lay<MAX_MTFACE; lay++)
		{
			MTF_localLayer &layer = layers[lay];
			if (layer.face == 0) break;

			layer.face++;
		}
	}
	meshobj->m_xyz_index_to_vertex_index_mapping.clear();
	meshobj->UpdateMaterialList();

	// pre calculate texture generation
	for(RAS_MaterialBucket::Set::iterator mit = meshobj->GetFirstMaterial();
		mit != meshobj->GetLastMaterial(); ++ mit) {
		(*mit)->GetPolyMaterial()->OnConstruction();
	}

	if(tangent)
		delete [] tangent;

	if (layers)
		delete []layers;

	return meshobj;
}

	
	
static PHY_MaterialProps *CreateMaterialFromBlenderObject(struct Object* blenderobject,
												  KX_Scene *kxscene)
{
	PHY_MaterialProps *materialProps = new PHY_MaterialProps;
	
	MT_assert(materialProps && "Create physics material properties failed");
		
	Material* blendermat = give_current_material(blenderobject, 0);
		
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

static PHY_ShapeProps *CreateShapePropsFromBlenderObject(struct Object* blenderobject,
												 KX_Scene *kxscene)
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
	
	shapeProps->m_lin_drag = 1.0 - blenderobject->damping;
	shapeProps->m_ang_drag = 1.0 - blenderobject->rdamping;
	
	shapeProps->m_friction_scaling[0] = blenderobject->anisotropicFriction[0]; 
	shapeProps->m_friction_scaling[1] = blenderobject->anisotropicFriction[1];
	shapeProps->m_friction_scaling[2] = blenderobject->anisotropicFriction[2];
	shapeProps->m_do_anisotropic = ((blenderobject->gameflag & OB_ANISOTROPIC_FRICTION) != 0);
	
	shapeProps->m_do_fh     = (blenderobject->gameflag & OB_DO_FH) != 0; 
	shapeProps->m_do_rot_fh = (blenderobject->gameflag & OB_ROT_FH) != 0;
	
	return shapeProps;
}

	
	
	
		
//////////////////////////////////////////////////////////
	


static float my_boundbox_mesh(Mesh *me, float *loc, float *size)
{
	MVert *mvert;
	BoundBox *bb;
	MT_Point3 min, max;
	float mloc[3], msize[3];
	int a;
	
	if(me->bb==0) me->bb= (struct BoundBox *)MEM_callocN(sizeof(BoundBox), "boundbox");
	bb= me->bb;
	
	INIT_MINMAX(min, max);

	if (!loc) loc= mloc;
	if (!size) size= msize;
	
	mvert= me->mvert;
	for(a=0; a<me->totvert; a++, mvert++) {
		DO_MINMAX(mvert->co, min, max);
	}
		
	if(me->totvert) {
		loc[0]= (min[0]+max[0])/2.0;
		loc[1]= (min[1]+max[1])/2.0;
		loc[2]= (min[2]+max[2])/2.0;
		
		size[0]= (max[0]-min[0])/2.0;
		size[1]= (max[1]-min[1])/2.0;
		size[2]= (max[2]-min[2])/2.0;
	}
	else {
		loc[0]= loc[1]= loc[2]= 0.0;
		size[0]= size[1]= size[2]= 0.0;
	}
		
	bb->vec[0][0]=bb->vec[1][0]=bb->vec[2][0]=bb->vec[3][0]= loc[0]-size[0];
	bb->vec[4][0]=bb->vec[5][0]=bb->vec[6][0]=bb->vec[7][0]= loc[0]+size[0];
		
	bb->vec[0][1]=bb->vec[1][1]=bb->vec[4][1]=bb->vec[5][1]= loc[1]-size[1];
	bb->vec[2][1]=bb->vec[3][1]=bb->vec[6][1]=bb->vec[7][1]= loc[1]+size[1];

	bb->vec[0][2]=bb->vec[3][2]=bb->vec[4][2]=bb->vec[7][2]= loc[2]-size[2];
	bb->vec[1][2]=bb->vec[2][2]=bb->vec[5][2]=bb->vec[6][2]= loc[2]+size[2];

	float radius = 0;
	for (a=0, mvert = me->mvert; a < me->totvert; a++, mvert++)
	{
		float vert_radius = MT_Vector3(mvert->co).length2();
		if (vert_radius > radius)
			radius = vert_radius;
	} 
	return sqrt(radius);
}
		



static void my_tex_space_mesh(Mesh *me)
		{
	KeyBlock *kb;
	float *fp, loc[3], size[3], min[3], max[3];
	int a;

	my_boundbox_mesh(me, loc, size);
	
	if(me->texflag & AUTOSPACE) {
		if(me->key) {
			kb= me->key->refkey;
			if (kb) {
	
				INIT_MINMAX(min, max);
		
				fp= (float *)kb->data;
				for(a=0; a<kb->totelem; a++, fp+=3) {	
					DO_MINMAX(fp, min, max);
				}
				if(kb->totelem) {
					loc[0]= (min[0]+max[0])/2.0; loc[1]= (min[1]+max[1])/2.0; loc[2]= (min[2]+max[2])/2.0;
					size[0]= (max[0]-min[0])/2.0; size[1]= (max[1]-min[1])/2.0; size[2]= (max[2]-min[2])/2.0;
	} 
	else {
					loc[0]= loc[1]= loc[2]= 0.0;
					size[0]= size[1]= size[2]= 0.0;
				}
				
			}
				}
	
		VECCOPY(me->loc, loc);
		VECCOPY(me->size, size);
		me->rot[0]= me->rot[1]= me->rot[2]= 0.0;
	
		if(me->size[0]==0.0) me->size[0]= 1.0;
		else if(me->size[0]>0.0 && me->size[0]<0.00001) me->size[0]= 0.00001;
		else if(me->size[0]<0.0 && me->size[0]> -0.00001) me->size[0]= -0.00001;
	
		if(me->size[1]==0.0) me->size[1]= 1.0;
		else if(me->size[1]>0.0 && me->size[1]<0.00001) me->size[1]= 0.00001;
		else if(me->size[1]<0.0 && me->size[1]> -0.00001) me->size[1]= -0.00001;
						
		if(me->size[2]==0.0) me->size[2]= 1.0;
		else if(me->size[2]>0.0 && me->size[2]<0.00001) me->size[2]= 0.00001;
		else if(me->size[2]<0.0 && me->size[2]> -0.00001) me->size[2]= -0.00001;
	}
	
}

static void my_get_local_bounds(Object *ob, float *center, float *size)
{
	BoundBox *bb= NULL;
	/* uses boundbox, function used by Ketsji */
	switch (ob->type)
	{
		case OB_MESH:
			bb= ( (Mesh *)ob->data )->bb;
			if(bb==0) 
			{
				my_tex_space_mesh((struct Mesh *)ob->data);
				bb= ( (Mesh *)ob->data )->bb;
			}
			break;
		case OB_CURVE:
		case OB_SURF:
		case OB_FONT:
			center[0]= center[1]= center[2]= 0.0;
			size[0]  = size[1]=size[2]=0.0;
			break;
		case OB_MBALL:
			bb= ob->bb;
			break;
	}
	
	if(bb==NULL) 
	{
		center[0]= center[1]= center[2]= 0.0;
		size[0] = size[1]=size[2]=1.0;
	}
	else 
	{
		size[0]= 0.5*fabs(bb->vec[0][0] - bb->vec[4][0]);
		size[1]= 0.5*fabs(bb->vec[0][1] - bb->vec[2][1]);
		size[2]= 0.5*fabs(bb->vec[0][2] - bb->vec[1][2]);
					
		center[0]= 0.5*(bb->vec[0][0] + bb->vec[4][0]);
		center[1]= 0.5*(bb->vec[0][1] + bb->vec[2][1]);
		center[2]= 0.5*(bb->vec[0][2] + bb->vec[1][2]);
	}
}
	



//////////////////////////////////////////////////////





void BL_CreatePhysicsObjectNew(KX_GameObject* gameobj,
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

	// get Root Parent of blenderobject
	struct Object* parent= blenderobject->parent;
	while(parent && parent->parent) {
		parent= parent->parent;
	}

	bool isCompoundChild = false;

	if (parent && (parent->gameflag & OB_DYNAMIC)) {
		
		if ((parent->gameflag & OB_CHILD) != 0)
		{
			isCompoundChild = true;
		} 
	}
	if (processCompoundChildren != isCompoundChild)
		return;


	PHY_ShapeProps* shapeprops =
			CreateShapePropsFromBlenderObject(blenderobject, 
			kxscene);

	
	PHY_MaterialProps* smmaterial = 
		CreateMaterialFromBlenderObject(blenderobject, kxscene);
					
	KX_ObjectProperties objprop;

	objprop.m_isCompoundChild = isCompoundChild;
	objprop.m_hasCompoundChildren = (blenderobject->gameflag & OB_CHILD) != 0;

	if ((objprop.m_isactor = (blenderobject->gameflag & OB_ACTOR)!=0))
	{
		objprop.m_dyna = (blenderobject->gameflag & OB_DYNAMIC) != 0;
		objprop.m_angular_rigidbody = (blenderobject->gameflag & OB_RIGID_BODY) != 0;
		objprop.m_ghost = (blenderobject->gameflag & OB_GHOST) != 0;
		objprop.m_disableSleeping = (blenderobject->gameflag & OB_COLLISION_RESPONSE) != 0;//abuse the OB_COLLISION_RESPONSE flag
	} else {
		objprop.m_dyna = false;
		objprop.m_angular_rigidbody = false;
		objprop.m_ghost = false;
		objprop.m_disableSleeping = false;
	}
	//mmm, for now, taks this for the size of the dynamicobject
	// Blender uses inertia for radius of dynamic object
	objprop.m_radius = blenderobject->inertia;
	objprop.m_in_active_layer = (blenderobject->lay & activeLayerBitInfo) != 0;
	objprop.m_dynamic_parent=NULL;
	objprop.m_isdeformable = ((blenderobject->gameflag2 & 2)) != 0;
	objprop.m_boundclass = objprop.m_dyna?KX_BOUNDSPHERE:KX_BOUNDMESH;
	KX_BoxBounds bb;
	my_get_local_bounds(blenderobject,objprop.m_boundobject.box.m_center,bb.m_extends);
	if (blenderobject->gameflag & OB_BOUNDS)
	{
		switch (blenderobject->boundtype)
		{
			case OB_BOUND_BOX:
				objprop.m_boundclass = KX_BOUNDBOX;
				//mmm, has to be divided by 2 to be proper extends
				objprop.m_boundobject.box.m_extends[0]=2.f*bb.m_extends[0];
				objprop.m_boundobject.box.m_extends[1]=2.f*bb.m_extends[1];
				objprop.m_boundobject.box.m_extends[2]=2.f*bb.m_extends[2];
				break;
			case OB_BOUND_POLYT:
				if (blenderobject->type == OB_MESH)
				{
					objprop.m_boundclass = KX_BOUNDPOLYTOPE;
					break;
				}
				// Object is not a mesh... fall through OB_BOUND_POLYH to 
				// OB_BOUND_SPHERE
			case OB_BOUND_POLYH:
				if (blenderobject->type == OB_MESH)
				{
					objprop.m_boundclass = KX_BOUNDMESH;
					break;
				}
				// Object is not a mesh... can't use polyheder. 
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
		}
	}

	
	if (parent && (parent->gameflag & OB_DYNAMIC)) {
		
		KX_GameObject *parentgameobject = converter->FindGameObject(parent);
		objprop.m_dynamic_parent = parentgameobject;
		//cannot be dynamic:
		objprop.m_dyna = false;
		shapeprops->m_mass = 0.f;
	}

	
	objprop.m_concave = (blenderobject->boundtype & 4) != 0;
	
	switch (physics_engine)
	{
#ifdef USE_BULLET
		case UseBullet:
			KX_ConvertBulletObject(gameobj, meshobj, kxscene, shapeprops, smmaterial, &objprop);
			break;

#endif
#ifdef USE_SUMO_SOLID
		case UseSumo:
			KX_ConvertSumoObject(gameobj, meshobj, kxscene, shapeprops, smmaterial, &objprop);
			break;
#endif
			
#ifdef USE_ODE
		case UseODE:
			KX_ConvertODEEngineObject(gameobj, meshobj, kxscene, shapeprops, smmaterial, &objprop);
			break;
#endif //USE_ODE

		case UseDynamo:
			//KX_ConvertDynamoObject(gameobj,meshobj,kxscene,shapeprops,	smmaterial,	&objprop);
			break;
			
		case UseNone:
		default:
			break;
	}
	delete shapeprops;
	delete smmaterial;
}





static KX_LightObject *gamelight_from_blamp(Object *ob, Lamp *la, unsigned int layerflag, KX_Scene *kxscene, RAS_IRenderTools *rendertools, KX_BlenderSceneConverter *converter) {
	RAS_LightObject lightobj;
	KX_LightObject *gamelight;
	
	lightobj.m_att1 = la->att1;
	lightobj.m_att2 = (la->mode & LA_QUAD)?la->att2:0.0;
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
	
	if (la->mode & LA_NEG)
	{
		lightobj.m_red = -lightobj.m_red;
		lightobj.m_green = -lightobj.m_green;
		lightobj.m_blue = -lightobj.m_blue;
	}
		
	if (la->type==LA_SUN) {
		lightobj.m_type = RAS_LightObject::LIGHT_SUN;
	} else if (la->type==LA_SPOT) {
		lightobj.m_type = RAS_LightObject::LIGHT_SPOT;
	} else {
		lightobj.m_type = RAS_LightObject::LIGHT_NORMAL;
	}

#ifdef BLENDER_GLSL
	if(converter->GetGLSLMaterials())
		GPU_lamp_from_blender(ob, la);
	
	gamelight = new KX_LightObject(kxscene, KX_Scene::m_callbacks, rendertools, lightobj, ob->gpulamp);
#else
	gamelight = new KX_LightObject(kxscene, KX_Scene::m_callbacks, rendertools, lightobj, NULL);
#endif
	BL_ConvertLampIpos(la, gamelight, converter);
	
	return gamelight;
}

static KX_Camera *gamecamera_from_bcamera(Object *ob, KX_Scene *kxscene, KX_BlenderSceneConverter *converter) {
	Camera* ca = static_cast<Camera*>(ob->data);
	RAS_CameraData camdata(ca->lens, ca->clipsta, ca->clipend, ca->type == CAM_PERSP, dof_camera(ob));
	KX_Camera *gamecamera;
	
	gamecamera= new KX_Camera(kxscene, KX_Scene::m_callbacks, camdata);
	gamecamera->SetName(ca->id.name + 2);
	
	BL_ConvertCameraIpos(ca, gamecamera, converter);
	
	return gamecamera;
}

static KX_GameObject *gameobject_from_blenderobject(
								Object *ob, 
								KX_Scene *kxscene, 
								RAS_IRenderTools *rendertools, 
								KX_BlenderSceneConverter *converter,
								Scene *blenderscene) 
{
	KX_GameObject *gameobj = NULL;
	
	switch(ob->type)
	{
	case OB_LAMP:
	{
		KX_LightObject* gamelight= gamelight_from_blamp(ob, static_cast<Lamp*>(ob->data), ob->lay, kxscene, rendertools, converter);
		gameobj = gamelight;
		
		gamelight->AddRef();
		kxscene->GetLightList()->Add(gamelight);
		
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
		RAS_MeshObject* meshobj = converter->FindGameMesh(mesh, ob->lay);
		float center[3], extents[3];
		float radius = my_boundbox_mesh((Mesh*) ob->data, center, extents);
		
		if (!meshobj) {
			meshobj = BL_ConvertMesh(mesh,ob,rendertools,kxscene,converter);
			converter->RegisterGameMesh(meshobj, mesh);
		}
		
		// needed for python scripting
		kxscene->GetLogicManager()->RegisterMeshName(meshobj->GetName(),meshobj);
	
		gameobj = new BL_DeformableGameObject(ob,kxscene,KX_Scene::m_callbacks);
	
		// set transformation
		gameobj->AddMesh(meshobj);
	
		// for all objects: check whether they want to
		// respond to updates
		bool ignoreActivityCulling =  
			((ob->gameflag2 & OB_NEVER_DO_ACTIVITY_CULLING)!=0);
		gameobj->SetIgnoreActivityCulling(ignoreActivityCulling);
	
		// two options exists for deform: shape keys and armature
		// only support relative shape key
		bool bHasShapeKey = mesh->key != NULL && mesh->key->type==KEY_RELATIVE;
		bool bHasDvert = mesh->dvert != NULL && ob->defbase.first;
		bool bHasArmature = (ob->parent && ob->parent->type == OB_ARMATURE && ob->partype==PARSKEL && bHasDvert);

		if (bHasShapeKey) {
			// not that we can have shape keys without dvert! 
			BL_ShapeDeformer *dcont = new BL_ShapeDeformer((BL_DeformableGameObject*)gameobj, 
															ob, (BL_SkinMeshObject*)meshobj);
			((BL_DeformableGameObject*)gameobj)->m_pDeformer = dcont;
			if (bHasArmature)
				dcont->LoadShapeDrivers(ob->parent);
		} else if (bHasArmature) {
			BL_SkinDeformer *dcont = new BL_SkinDeformer((BL_DeformableGameObject*)gameobj,
															ob, (BL_SkinMeshObject*)meshobj);
			((BL_DeformableGameObject*)gameobj)->m_pDeformer = dcont;
		} else if (bHasDvert) {
			// this case correspond to a mesh that can potentially deform but not with the
			// object to which it is attached for the moment. A skin mesh was created in
			// BL_ConvertMesh() so must create a deformer too!
			BL_MeshDeformer *dcont = new BL_MeshDeformer((BL_DeformableGameObject*)gameobj,
														  ob, (BL_SkinMeshObject*)meshobj);
			((BL_DeformableGameObject*)gameobj)->m_pDeformer = dcont;
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
		gameobj = new BL_ArmatureObject(
			kxscene,
			KX_Scene::m_callbacks,
			ob // handle
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
	}
	if (gameobj) 
	{
		gameobj->SetPhysicsEnvironment(kxscene->GetPhysicsEnvironment());
		gameobj->SetLayer(ob->lay);
		gameobj->SetBlenderObject(ob);
	}
	return gameobj;
}

struct parentChildLink {
	struct Object* m_blenderchild;
	SG_Node* m_gamechildnode;
};

	/**
	 * Find the specified scene by name, or the first
	 * scene if nothing matches (shouldn't happen).
	 */
static struct Scene *GetSceneForName(struct Main *maggie, const STR_String& scenename) {
	Scene *sce;

	for (sce= (Scene*) maggie->scene.first; sce; sce= (Scene*) sce->id.next)
		if (scenename == (sce->id.name+2))
			return sce;

	return (Scene*) maggie->scene.first;
}

#include "DNA_constraint_types.h"
#include "BIF_editconstraint.h"

bPoseChannel *get_active_posechannel2 (Object *ob)
{
	bArmature *arm= (bArmature*)ob->data;
	bPoseChannel *pchan;
	
	/* find active */
	for(pchan= (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone && (pchan->bone->flag & BONE_ACTIVE) && (pchan->bone->layer & arm->layer))
			return pchan;
	}
	
	return NULL;
}

ListBase *get_active_constraints2(Object *ob)
{
	if (!ob)
		return NULL;

	if (ob->flag & OB_POSEMODE) {
		bPoseChannel *pchan;

		pchan = get_active_posechannel2(ob);
		if (pchan)
			return &pchan->constraints;
	}
	else 
		return &ob->constraints;

	return NULL;
}


void RBJconstraints(Object *ob)//not used
{
	ListBase *conlist;
	bConstraint *curcon;

	conlist = get_active_constraints2(ob);

	if (conlist) {
		for (curcon = (bConstraint *)conlist->first; curcon; curcon=(bConstraint *)curcon->next) {

			printf("%i\n",curcon->type);
		}


	}
}

#include "PHY_IPhysicsEnvironment.h"
#include "KX_IPhysicsController.h"
#include "PHY_DynamicTypes.h"

KX_IPhysicsController* getPhId(CListValue* sumolist,STR_String busc){//not used

    for (int j=0;j<sumolist->GetCount();j++)
	{
	    KX_GameObject* gameobje = (KX_GameObject*) sumolist->GetValue(j);
	    if (gameobje->GetName()==busc)
            return gameobje->GetPhysicsController();
	}

	return 0;

}

KX_GameObject* getGameOb(STR_String busc,CListValue* sumolist){

    for (int j=0;j<sumolist->GetCount();j++)
	{
	    KX_GameObject* gameobje = (KX_GameObject*) sumolist->GetValue(j);
	    if (gameobje->GetName()==busc)
            return gameobje;
	}
	
	return 0;

}
#include "BLI_arithb.h"
// convert blender objects into ketsji gameobjects
void BL_ConvertBlenderObjects(struct Main* maggie,
							  const STR_String& scenename,
							  KX_Scene* kxscene,
							  KX_KetsjiEngine* ketsjiEngine,
							  e_PhysicsEngine	physics_engine,
							  PyObject* pythondictionary,
							  SCA_IInputDevice* keydev,
							  RAS_IRenderTools* rendertools,
							  RAS_ICanvas* canvas,
							  KX_BlenderSceneConverter* converter,
							  bool alwaysUseExpandFraming
							  )
{	

	Scene *blenderscene = GetSceneForName(maggie, scenename);
	// for SETLOOPER
	Scene *sce;
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

	if (alwaysUseExpandFraming) {
		frame_type = RAS_FrameSettings::e_frame_extend;
		aspect_width = canvas->GetWidth();
		aspect_height = canvas->GetHeight();
	} else {
		if (blenderscene->framing.type == SCE_GAMEFRAMING_BARS) {
			frame_type = RAS_FrameSettings::e_frame_bars;
		} else if (blenderscene->framing.type == SCE_GAMEFRAMING_EXTEND) {
			frame_type = RAS_FrameSettings::e_frame_extend;
		} else {
			frame_type = RAS_FrameSettings::e_frame_scale;
		}
		
		aspect_width = blenderscene->r.xsch;
		aspect_height = blenderscene->r.ysch;
	}
	
	RAS_FrameSettings frame_settings(
		frame_type,
		blenderscene->framing.col[0],
		blenderscene->framing.col[1],
		blenderscene->framing.col[2],
		aspect_width,
		aspect_height
	);
	kxscene->SetFramingType(frame_settings);

	kxscene->SetGravity(MT_Vector3(0,0,(blenderscene->world != NULL) ? -blenderscene->world->gravity : -9.8));
	
	/* set activity culling parameters */
	if (blenderscene->world) {
		kxscene->SetActivityCulling( (blenderscene->world->mode & WO_ACTIVITY_CULLING) != 0);
		kxscene->SetActivityCullingRadius(blenderscene->world->activityBoxRadius);
	} else {
		kxscene->SetActivityCulling(false);
	}
	
	int activeLayerBitInfo = blenderscene->lay;
	
	// templist to find Root Parents (object with no parents)
	CListValue* templist = new CListValue();
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
		logicmgr->RegisterActionName(curAct->id.name, curAct);
	}

	SetDefaultFaceType(blenderscene);
	// Let's support scene set.
	// Beware of name conflict in linked data, it will not crash but will create confusion
	// in Python scripting and in certain actuators (replace mesh). Linked scene *should* have
	// no conflicting name for Object, Object data and Action.
	for (SETLOOPER(blenderscene, base))
	{
		Object* blenderobject = base->object;
		allblobj.insert(blenderobject);

		KX_GameObject* gameobj = gameobject_from_blenderobject(
										base->object, 
										kxscene, 
										rendertools, 
										converter,
										blenderscene);
										
		bool isInActiveLayer = (blenderobject->lay & activeLayerBitInfo) !=0;
		bool addobj=true;
		
		if (converter->addInitFromFrame)
			if (!isInActiveLayer)
				addobj=false;
										
		if (gameobj&&addobj)
		{
			MT_Point3 posPrev;			
			MT_Matrix3x3 angor;			
			if (converter->addInitFromFrame) blenderscene->r.cfra=blenderscene->r.sfra;
			
			MT_Point3 pos = MT_Point3(
				blenderobject->loc[0]+blenderobject->dloc[0],
				blenderobject->loc[1]+blenderobject->dloc[1],
				blenderobject->loc[2]+blenderobject->dloc[2]
			);
			MT_Vector3 eulxyz = MT_Vector3(
				blenderobject->rot[0],
				blenderobject->rot[1],
				blenderobject->rot[2]
			);
			MT_Vector3 scale = MT_Vector3(
				blenderobject->size[0],
				blenderobject->size[1],
				blenderobject->size[2]
			);
			if (converter->addInitFromFrame){//rcruiz
				float eulxyzPrev[3];
				blenderscene->r.cfra=blenderscene->r.sfra-1;
				update_for_newframe();
				MT_Vector3 tmp=pos-MT_Point3(blenderobject->loc[0]+blenderobject->dloc[0],
											blenderobject->loc[1]+blenderobject->dloc[1],
											blenderobject->loc[2]+blenderobject->dloc[2]
									);
				eulxyzPrev[0]=blenderobject->rot[0];
				eulxyzPrev[1]=blenderobject->rot[1];
				eulxyzPrev[2]=blenderobject->rot[2];

				double fps = (double) blenderscene->r.frs_sec/
					(double) blenderscene->r.frs_sec_base;

				tmp.scale(fps, fps, fps);
				inivel.push_back(tmp);
				tmp=eulxyz-eulxyzPrev;
				tmp.scale(fps, fps, fps);
				iniang.push_back(tmp);
				blenderscene->r.cfra=blenderscene->r.sfra;
				update_for_newframe();
			}		
						
			gameobj->NodeSetLocalPosition(pos);
			gameobj->NodeSetLocalOrientation(MT_Matrix3x3(eulxyz));
			gameobj->NodeSetLocalScale(scale);
			gameobj->NodeUpdateGS(0,true);
			
			BL_ConvertIpos(blenderobject,gameobj,converter);
			// TODO: expand to multiple ipos per mesh
			Material *mat = give_current_material(blenderobject, 1);
			if(mat) BL_ConvertMaterialIpos(mat, gameobj, converter);	
	
			sumolist->Add(gameobj->AddRef());
			
			BL_ConvertProperties(blenderobject,gameobj,timemgr,kxscene,isInActiveLayer);
			
	
			gameobj->SetName(blenderobject->id.name);
	
			// templist to find Root Parents (object with no parents)
			templist->Add(gameobj->AddRef());
			
			// update children/parent hierarchy
			if ((blenderobject->parent != 0)&&(!converter->addInitFromFrame))
			{
				// blender has an additional 'parentinverse' offset in each object
				SG_Node* parentinversenode = new SG_Node(NULL,NULL,SG_Callbacks());
			
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
				parentinversenode->SetLocalOrientation(parinvtrans.getBasis());
				
				parentinversenode->AddChild(gameobj->GetSGNode());
			}
			
			// needed for python scripting
			logicmgr->RegisterGameObjectName(gameobj->GetName(),gameobj);

			// needed for dynamic object morphing
			logicmgr->RegisterGameObj(gameobj, blenderobject);
			for (int i = 0; i < gameobj->GetMeshCount(); i++)
				logicmgr->RegisterGameMeshName(gameobj->GetMesh(i)->GetName(), blenderobject);
	
			converter->RegisterGameObject(gameobj, blenderobject);	
			// this was put in rapidly, needs to be looked at more closely
			// only draw/use objects in active 'blender' layers
	
			logicbrick_conversionlist->Add(gameobj->AddRef());
			
			if (converter->addInitFromFrame){
				posPrev=gameobj->NodeGetWorldPosition();
				angor=gameobj->NodeGetWorldOrientation();
			}
			if (isInActiveLayer)
			{
				objectlist->Add(gameobj->AddRef());
				//tf.Add(gameobj->GetSGNode());
				
				gameobj->NodeUpdateGS(0,true);
				gameobj->Bucketize();
		
				if (gameobj->IsDupliGroup())
					grouplist.insert(blenderobject->dup_group);
			}
			else
			{
				//we must store this object otherwise it will be deleted 
				//at the end of this function if it is not a root object
				inactivelist->Add(gameobj->AddRef());
			}
			if (converter->addInitFromFrame){
				gameobj->NodeSetLocalPosition(posPrev);
				gameobj->NodeSetLocalOrientation(angor);
			}
						
		}
		/* Note about memory leak issues:
		   When a CValue derived class is created, m_refcount is initialized to 1
		   so the class must be released after being used to make sure that it won't 
		   hang in memory. If the object needs to be stored for a long time, 
		   use AddRef() so that this Release() does not free the object.
		   Make sure that for any AddRef() there is a Release()!!!! 
		   Do the same for any object derived from CValue, CExpression and NG_NetworkMessage
		 */
		if (gameobj)
			gameobj->Release();

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
				for(go=(GroupObject*)group->gobject.first; go; go=(GroupObject*)go->next) 
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
														blenderscene);
										
						// this code is copied from above except that
						// object from groups are never is active layer
						bool isInActiveLayer = false;
						bool addobj=true;
						
						if (converter->addInitFromFrame)
							if (!isInActiveLayer)
								addobj=false;
														
						if (gameobj&&addobj)
						{
							MT_Point3 posPrev;			
							MT_Matrix3x3 angor;			
							if (converter->addInitFromFrame) 
								blenderscene->r.cfra=blenderscene->r.sfra;
							
							MT_Point3 pos = MT_Point3(
								blenderobject->loc[0]+blenderobject->dloc[0],
								blenderobject->loc[1]+blenderobject->dloc[1],
								blenderobject->loc[2]+blenderobject->dloc[2]
							);
							MT_Vector3 eulxyz = MT_Vector3(
								blenderobject->rot[0],
								blenderobject->rot[1],
								blenderobject->rot[2]
							);
							MT_Vector3 scale = MT_Vector3(
								blenderobject->size[0],
								blenderobject->size[1],
								blenderobject->size[2]
							);
							if (converter->addInitFromFrame){//rcruiz
								float eulxyzPrev[3];
								blenderscene->r.cfra=blenderscene->r.sfra-1;
								update_for_newframe();
								MT_Vector3 tmp=pos-MT_Point3(blenderobject->loc[0]+blenderobject->dloc[0],
															blenderobject->loc[1]+blenderobject->dloc[1],
															blenderobject->loc[2]+blenderobject->dloc[2]
													);
								eulxyzPrev[0]=blenderobject->rot[0];
								eulxyzPrev[1]=blenderobject->rot[1];
								eulxyzPrev[2]=blenderobject->rot[2];

								double fps = (double) blenderscene->r.frs_sec/
									(double) blenderscene->r.frs_sec_base;

								tmp.scale(fps, fps, fps);
								inivel.push_back(tmp);
								tmp=eulxyz-eulxyzPrev;
								tmp.scale(fps, fps, fps);
								iniang.push_back(tmp);
								blenderscene->r.cfra=blenderscene->r.sfra;
								update_for_newframe();
							}		
										
							gameobj->NodeSetLocalPosition(pos);
							gameobj->NodeSetLocalOrientation(MT_Matrix3x3(eulxyz));
							gameobj->NodeSetLocalScale(scale);
							gameobj->NodeUpdateGS(0,true);
							
							BL_ConvertIpos(blenderobject,gameobj,converter);
							// TODO: expand to multiple ipos per mesh
							Material *mat = give_current_material(blenderobject, 1);
							if(mat) BL_ConvertMaterialIpos(mat, gameobj, converter);	
					
							sumolist->Add(gameobj->AddRef());
							
							BL_ConvertProperties(blenderobject,gameobj,timemgr,kxscene,isInActiveLayer);
							
					
							gameobj->SetName(blenderobject->id.name);
					
							// templist to find Root Parents (object with no parents)
							templist->Add(gameobj->AddRef());
							
							// update children/parent hierarchy
							if ((blenderobject->parent != 0)&&(!converter->addInitFromFrame))
							{
								// blender has an additional 'parentinverse' offset in each object
								SG_Node* parentinversenode = new SG_Node(NULL,NULL,SG_Callbacks());
							
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
								parentinversenode->SetLocalOrientation(parinvtrans.getBasis());
								
								parentinversenode->AddChild(gameobj->GetSGNode());
							}
							
							// needed for python scripting
							logicmgr->RegisterGameObjectName(gameobj->GetName(),gameobj);

							// needed for dynamic object morphing
							logicmgr->RegisterGameObj(gameobj, blenderobject);
							for (int i = 0; i < gameobj->GetMeshCount(); i++)
								logicmgr->RegisterGameMeshName(gameobj->GetMesh(i)->GetName(), blenderobject);
					
							converter->RegisterGameObject(gameobj, blenderobject);	
							// this was put in rapidly, needs to be looked at more closely
							// only draw/use objects in active 'blender' layers
					
							logicbrick_conversionlist->Add(gameobj->AddRef());
							
							if (converter->addInitFromFrame){
								posPrev=gameobj->NodeGetWorldPosition();
								angor=gameobj->NodeGetWorldOrientation();
							}
							if (isInActiveLayer)
							{
								objectlist->Add(gameobj->AddRef());
								//tf.Add(gameobj->GetSGNode());
								
								gameobj->NodeUpdateGS(0,true);
								gameobj->Bucketize();
						
							}
							else
							{
								//we must store this object otherwise it will be deleted 
								//at the end of this function if it is not a root object
								inactivelist->Add(gameobj->AddRef());

							}
							if (gameobj->IsDupliGroup())
							{
								// check that the group is not already converted
								if (allgrouplist.insert(blenderobject->dup_group).second)
									grouplist.insert(blenderobject->dup_group);
							}
							if (converter->addInitFromFrame){
								gameobj->NodeSetLocalPosition(posPrev);
								gameobj->NodeSetLocalOrientation(angor);
							}
										
						}
						if (gameobj)
							gameobj->Release();
					}
				}
			}
		}
	}

	if (blenderscene->camera) {
		KX_Camera *gamecamera= (KX_Camera*) converter->FindGameObject(blenderscene->camera);
		
		if(gamecamera)
			kxscene->SetActiveCamera(gamecamera);
	}

	//	Set up armatures
	set<Object*>::iterator oit;
	for(oit=allblobj.begin(); oit!=allblobj.end(); oit++)
	{
		Object* blenderobj = *oit;
		if (blenderobj->type==OB_MESH){
			Mesh *me = (Mesh*)blenderobj->data;
	
			if (me->dvert){
				KX_GameObject *obj = converter->FindGameObject(blenderobj);
	
				if (obj && blenderobj->parent && blenderobj->parent->type==OB_ARMATURE && blenderobj->partype==PARSKEL){
					KX_GameObject *par = converter->FindGameObject(blenderobj->parent);
					if (par)
						((BL_SkinDeformer*)(((BL_DeformableGameObject*)obj)->m_pDeformer))->SetArmature((BL_ArmatureObject*) par);
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
				Bone *parent_bone = get_named_bone(get_armature(blenderchild->parent), blenderchild->parsubstr);
				KX_BoneParentRelation *bone_parent_relation = KX_BoneParentRelation::New(parent_bone);
				pcit->m_gamechildnode->SetParentRelation(bone_parent_relation);
			
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
	
		struct Object* blenderparent = blenderchild->parent;
		KX_GameObject* parentobj = converter->FindGameObject(blenderparent);
		if (parentobj)
		{
			parentobj->	GetSGNode()->AddChild(pcit->m_gamechildnode);
		}
	}
	vec_parent_child.clear();
	
	// find 'root' parents (object that has not parents in SceneGraph)
	for (i=0;i<templist->GetCount();++i)
	{
		KX_GameObject* gameobj = (KX_GameObject*) templist->GetValue(i);
		if (gameobj->GetSGNode()->GetSGParent() == 0)
		{
			parentlist->Add(gameobj->AddRef());
			gameobj->NodeUpdateGS(0,true);
		}
	}
	
	bool processCompoundChildren = false;

	// create physics information
	for (i=0;i<sumolist->GetCount();i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
		struct Object* blenderobject = converter->FindBlenderObject(gameobj);
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
		struct Object* blenderobject = converter->FindBlenderObject(gameobj);
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
			if (gameobj->IsDynamic()){
				gameobj->setLinearVelocity(inivel[i],false);
				gameobj->setAngularVelocity(iniang[i],false);
			}
		
		
		}	

		// create physics joints
	for (i=0;i<sumolist->GetCount();i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*) sumolist->GetValue(i);
		struct Object* blenderobject = converter->FindBlenderObject(gameobj);
		ListBase *conlist;
		bConstraint *curcon;
		conlist = get_active_constraints2(blenderobject);

		if (conlist) {
			for (curcon = (bConstraint *)conlist->first; curcon; curcon=(bConstraint *)curcon->next) {
				if (curcon->type==CONSTRAINT_TYPE_RIGIDBODYJOINT){

					bRigidBodyJointConstraint *dat=(bRigidBodyJointConstraint *)curcon->data;

					if (!dat->child){

						PHY_IPhysicsController* physctr2 = 0;

						if (dat->tar)
						{
							KX_GameObject *gotar=getGameOb(dat->tar->id.name,sumolist);
							if (gotar && gotar->GetPhysicsController())
								physctr2 = (PHY_IPhysicsController*) gotar->GetPhysicsController()->GetUserData();
						}

						if (gameobj->GetPhysicsController())
						{
							float radsPerDeg = 6.283185307179586232f / 360.f;

							PHY_IPhysicsController* physctrl = (PHY_IPhysicsController*) gameobj->GetPhysicsController()->GetUserData();
							//we need to pass a full constraint frame, not just axis
	                            
							//localConstraintFrameBasis
							MT_Matrix3x3 localCFrame(MT_Vector3(radsPerDeg*dat->axX,radsPerDeg*dat->axY,radsPerDeg*dat->axZ));
							MT_Vector3 axis0 = localCFrame.getColumn(0);
							MT_Vector3 axis1 = localCFrame.getColumn(1);
							MT_Vector3 axis2 = localCFrame.getColumn(2);
								
							int constraintId = kxscene->GetPhysicsEnvironment()->createConstraint(physctrl,physctr2,(PHY_ConstraintType)dat->type,(float)dat->pivX,
								(float)dat->pivY,(float)dat->pivZ,
								(float)axis0.x(),(float)axis0.y(),(float)axis0.z(),
								(float)axis1.x(),(float)axis1.y(),(float)axis1.z(),
								(float)axis2.x(),(float)axis2.y(),(float)axis2.z());
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
											kxscene->GetPhysicsEnvironment()->setConstraintParam(constraintId,dof,dat->minLimit[dof],dat->maxLimit[dof]);
										} else
										{
											//minLimit > maxLimit means free(disabled limit) for this degree of freedom
											kxscene->GetPhysicsEnvironment()->setConstraintParam(constraintId,dof,1,-1);
										}
										dofbit<<=1;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	templist->Release();
	sumolist->Release();	

	int executePriority=0; /* incremented by converter routines */
	
	// convert global sound stuff

	/* XXX, glob is the very very wrong place for this
	 * to be, re-enable once the listener has been moved into
	 * the scene. */
#if 1
	SND_Scene* soundscene = kxscene->GetSoundScene();
	SND_SoundListener* listener = soundscene->GetListener();
	if (listener && G.listener)
	{
		listener->SetDopplerFactor(G.listener->dopplerfactor);
		listener->SetDopplerVelocity(G.listener->dopplervelocity);
		listener->SetGain(G.listener->gain);
	}
#endif

	// convert world
	KX_WorldInfo* worldinfo = new BlenderWorldInfo(blenderscene->world);
	converter->RegisterWorldInfo(worldinfo);
	kxscene->SetWorldInfo(worldinfo);

#define CONVERT_LOGIC
#ifdef CONVERT_LOGIC
	// convert logic bricks, sensors, controllers and actuators
	for (i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = converter->FindBlenderObject(gameobj);
		int layerMask = (groupobj.find(blenderobj) == groupobj.end()) ? activeLayerBitInfo : 0;
		bool isInActiveLayer = (blenderobj->lay & layerMask)!=0;
		BL_ConvertActuators(maggie->name, blenderobj,gameobj,logicmgr,kxscene,ketsjiEngine,executePriority, layerMask,isInActiveLayer,rendertools,converter);
	}
	for ( i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = converter->FindBlenderObject(gameobj);
		int layerMask = (groupobj.find(blenderobj) == groupobj.end()) ? activeLayerBitInfo : 0;
		bool isInActiveLayer = (blenderobj->lay & layerMask)!=0;
		BL_ConvertControllers(blenderobj,gameobj,logicmgr,pythondictionary,executePriority,layerMask,isInActiveLayer,converter);
	}
	for ( i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = converter->FindBlenderObject(gameobj);
		int layerMask = (groupobj.find(blenderobj) == groupobj.end()) ? activeLayerBitInfo : 0;
		bool isInActiveLayer = (blenderobj->lay & layerMask)!=0;
		BL_ConvertSensors(blenderobj,gameobj,logicmgr,kxscene,keydev,executePriority,layerMask,isInActiveLayer,canvas,converter);
	}
	// apply the initial state to controllers
	for ( i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = converter->FindBlenderObject(gameobj);
		gameobj->SetInitState((blenderobj->init_state)?blenderobj->init_state:blenderobj->state);
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
}

