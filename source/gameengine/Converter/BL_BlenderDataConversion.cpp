/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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

#include "DNA_action_types.h"
#include "BKE_main.h"
#include "BL_SkinMeshObject.h"
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


#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "MT_Point3.h"


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

#include "BL_ArmatureObject.h"
#include "BL_DeformableGameObject.h"

static unsigned int KX_rgbaint2uint_new(unsigned int icol)
{
	unsigned int temp=0;
	unsigned char *cp= (unsigned char *)&temp;
	unsigned char *src= (unsigned char *)&icol;
	cp[3]= src[0];//alpha
	cp[2]= src[1];//blue
	cp[1]= src[2];//green
	cp[0]= src[3];//red
	return temp;
}

/* Now the real converting starts... */
static unsigned int KX_Mcol2uint_new(MCol col)
{
	/* color has to be converted without endian sensitivity. So no shifting! */
	unsigned int temp=0;
	unsigned char *cp= (unsigned char *)&temp;
	unsigned char *src = (unsigned char *) &col;
	cp[0]= src[3]; // red
	cp[1]= src[2]; // green
	cp[2]= src[1]; // blue
	cp[3]= src[0]; // Alpha
	return temp;
}

RAS_MeshObject* BL_ConvertMesh(Mesh* mesh, Object* blenderobj, RAS_IRenderTools* rendertools, KX_Scene* scene, KX_BlenderSceneConverter *converter)
{
	RAS_MeshObject *meshobj;
	bool	skinMesh = false;
	
	int lightlayer = blenderobj->lay;
	
	// Determine if we need to make a skinned mesh
	if (mesh->dvert){
		meshobj = new BL_SkinMeshObject(lightlayer);
		skinMesh = true;
	}
	else {
		meshobj = new RAS_MeshObject(lightlayer);
	}
	
	meshobj->SetName(mesh->id.name);
	
	MFace* mface = static_cast<MFace*>(mesh->mface);
	TFace* tface = static_cast<TFace*>(mesh->tface);
	assert(mface);
	MCol* mmcol = mesh->mcol;
	
	meshobj->m_xyz_index_to_vertex_index_mapping.resize(mesh->totvert);
	
	for (int f=0;f<mesh->totface;f++,mface++)
	{
		
		bool collider = true;
		
		// only add valid polygons
		if (mface->v3)
		{
			
			MT_Vector3 no0(0.0,0.0,0.0),no1(0.0,0.0,0.0),no2(0.0,0.0,0.0),no3(0.0,0.0,0.0);
			MT_Point3 pt0,pt1,pt2,pt3;
			
			MT_Point2 uv0(0.0,0.0),uv1(0.0,0.0),uv2(0.0,0.0),uv3(0.0,0.0);
			// rgb3 is set from the adjoint face in a square
			unsigned int rgb0,rgb1,rgb2,rgb3 = 0;
			pt0 = MT_Point3(	mesh->mvert[mface->v1].co[0],
								mesh->mvert[mface->v1].co[1],
								mesh->mvert[mface->v1].co[2]);
			no0 = MT_Vector3(
				mesh->mvert[mface->v1].no[0]/32767.0,
				mesh->mvert[mface->v1].no[1]/32767.0,
				mesh->mvert[mface->v1].no[2]/32767.0
				);
						
			pt1 = MT_Point3(	mesh->mvert[mface->v2].co[0],
								mesh->mvert[mface->v2].co[1],
								mesh->mvert[mface->v2].co[2]);
			
			no1 = MT_Vector3(
				mesh->mvert[mface->v2].no[0]/32767.0,
				mesh->mvert[mface->v2].no[1]/32767.0,
				mesh->mvert[mface->v2].no[2]/32767.0
				);
			
			pt2 = MT_Point3(	mesh->mvert[mface->v3].co[0],
								mesh->mvert[mface->v3].co[1],
								mesh->mvert[mface->v3].co[2]);
	
			no2 = MT_Vector3(
				mesh->mvert[mface->v3].no[0]/32767.0,
				mesh->mvert[mface->v3].no[1]/32767.0,
				mesh->mvert[mface->v3].no[2]/32767.0
				);
			
			if (mface->v4)
			{
				pt3 = MT_Point3(	mesh->mvert[mface->v4].co[0],
									mesh->mvert[mface->v4].co[1],
									mesh->mvert[mface->v4].co[2]);
				no3 = MT_Vector3(
					mesh->mvert[mface->v4].no[0]/32767.0,
					mesh->mvert[mface->v4].no[1]/32767.0,
					mesh->mvert[mface->v4].no[2]/32767.0
					);
			}
	
			if((!mface->flag & ME_SMOOTH))
			{
				MT_Vector3 norm = ((pt1-pt0).cross(pt2-pt0)).safe_normalized();
				norm[0] = ((int) (10*norm[0]))/10.0;
				norm[1] = ((int) (10*norm[1]))/10.0;
				norm[2] = ((int) (10*norm[2]))/10.0;
				no0=no1=no2=no3= norm;
	
			}
		
			{
				Image* bima = ((mesh->tface && tface) ? (Image*) tface->tpage : NULL);
	
				STR_String imastr = 
					((mesh->tface && tface) ? 
					(bima? (bima)->id.name : "" ) : "" );
		
				char transp=0;
				short mode=0, tile=0;
				int	tilexrep=4,tileyrep = 4;
				
				if (bima)
				{
					tilexrep = bima->xrep;
					tileyrep = bima->yrep;
			
				}
	
				
				bool polyvisible = true;
				if (mesh->tface && tface)
				{
					// Use texface colors if available
					//TF_DYNAMIC means the polygon is a collision face
					collider = (tface->mode & TF_DYNAMIC != 0);
					transp = tface->transp;
					tile = tface->tile;
					mode = tface->mode;
					
					polyvisible = !((tface->flag & TF_HIDE)||(tface->mode & TF_INVISIBLE));
					
					uv0 = MT_Point2(tface->uv[0]);
					uv1 = MT_Point2(tface->uv[1]);
					uv2 = MT_Point2(tface->uv[2]);
					rgb0 = KX_rgbaint2uint_new(tface->col[0]);
					rgb1 = KX_rgbaint2uint_new(tface->col[1]);
					rgb2 = KX_rgbaint2uint_new(tface->col[2]);
	
					if (mface->v4)
					{
						uv3 = MT_Point2(tface->uv[3]);
						rgb3 = KX_rgbaint2uint_new(tface->col[3]);
					} else {
					}
	
	
				} else
				{
					//
					if (mmcol)
					{
						// Use vertex colours
						rgb0 = KX_Mcol2uint_new(mmcol[0]);
						rgb1 = KX_Mcol2uint_new(mmcol[1]);
						rgb2 = KX_Mcol2uint_new(mmcol[2]);
						
						
						if (mface->v4)
						{
							rgb3 = KX_Mcol2uint_new(mmcol[3]);
							
						}
					
						mmcol += 4;
					}
					else{
						// If there are no vertex colors OR texfaces,
						// Initialize face to white and set COLLSION true and everything else FALSE
						rgb0 = KX_rgbaint2uint_new(0xFFFFFFFF);
						rgb1 = KX_rgbaint2uint_new(0xFFFFFFFF);
						rgb2 = KX_rgbaint2uint_new(0xFFFFFFFF);
						
						if (mface->v4)
							rgb3 = KX_rgbaint2uint_new(0xFFFFFFFF);
	
						mode = TF_DYNAMIC;	
						transp = TF_SOLID;
						tile = 0;
					}
				}
					
				
				Material* ma = give_current_material(blenderobj, 1);
				const char* matnameptr = (ma ? ma->id.name : "");
				
	
				bool istriangle = (mface->v4==0);
				bool zsort = ma?(ma->mode & MA_ZTRA) != 0:false;
				
				RAS_IPolyMaterial* polymat = rendertools->CreateBlenderPolyMaterial(imastr, false, matnameptr,
					tile, tilexrep, tileyrep, 
					mode, transp, zsort, lightlayer, istriangle, blenderobj, tface);
	
				if (ma)
				{
					polymat->m_specular = MT_Vector3(ma->spec * ma->specr,ma->spec * ma->specg,ma->spec * ma->specb);
					polymat->m_shininess = (float)ma->har;

				} else
				{
					polymat->m_specular = MT_Vector3(0.0f,0.0f,0.0f);
					polymat->m_shininess = 35.0;
				}

			
				// this is needed to free up memory afterwards
				converter->RegisterPolyMaterial(polymat);
	
				RAS_MaterialBucket* bucket = scene->FindBucket(polymat);
							 
				int nverts = mface->v4?4:3;
				int vtxarray = meshobj->FindVertexArray(nverts,polymat);
				RAS_Polygon* poly = new RAS_Polygon(bucket,polyvisible,nverts,vtxarray);
				if (skinMesh) {
					int d1, d2, d3, d4;
					bool flat;

					/* If the face is set to solid, all fnors are the same */
					if (mface->flag & ME_SMOOTH)
						flat = false;
					else
						flat = true;
					
					d1=((BL_SkinMeshObject*)meshobj)->FindOrAddDeform(vtxarray, mface->v1, &mesh->dvert[mface->v1], polymat);
					d2=((BL_SkinMeshObject*)meshobj)->FindOrAddDeform(vtxarray, mface->v2, &mesh->dvert[mface->v2], polymat);
					d3=((BL_SkinMeshObject*)meshobj)->FindOrAddDeform(vtxarray, mface->v3, &mesh->dvert[mface->v3], polymat);
					if (nverts==4)
						d4=((BL_SkinMeshObject*)meshobj)->FindOrAddDeform(vtxarray, mface->v4, &mesh->dvert[mface->v4], polymat);
					poly->SetVertex(0,((BL_SkinMeshObject*)meshobj)->FindOrAddVertex(vtxarray,pt0,uv0,rgb0,no0,d1,flat, polymat));
					poly->SetVertex(1,((BL_SkinMeshObject*)meshobj)->FindOrAddVertex(vtxarray,pt1,uv1,rgb1,no1,d2,flat, polymat));
					poly->SetVertex(2,((BL_SkinMeshObject*)meshobj)->FindOrAddVertex(vtxarray,pt2,uv2,rgb2,no2,d3,flat, polymat));
					if (nverts==4)
						poly->SetVertex(3,((BL_SkinMeshObject*)meshobj)->FindOrAddVertex(vtxarray,pt3,uv3,rgb3,no3,d4, flat,polymat));
				}
				else
				{
					poly->SetVertex(0,meshobj->FindOrAddVertex(vtxarray,pt0,uv0,rgb0,no0,polymat,mface->v1));
					poly->SetVertex(1,meshobj->FindOrAddVertex(vtxarray,pt1,uv1,rgb1,no1,polymat,mface->v2));
					poly->SetVertex(2,meshobj->FindOrAddVertex(vtxarray,pt2,uv2,rgb2,no2,polymat,mface->v3));
					if (nverts==4)
						poly->SetVertex(3,meshobj->FindOrAddVertex(vtxarray,pt3,uv3,rgb3,no3,polymat,mface->v4));
				}
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
				
				poly->SetVisibleWireframeEdges(mface->edcode);
				poly->SetCollider(collider);
			}
		}
		if (tface) 
			tface++;
	}
	meshobj->UpdateMaterialList();
	
	return meshobj;
}

static PHY_MaterialProps g_materialProps = {
	1.0,    // restitution
	2.0,    // friction 
	0.0,    // fh spring constant
	0.0,    // fh damping
	0.0,    // fh distance
	false   // sliding material?
};

	
	
static PHY_MaterialProps *CreateMaterialFromBlenderObject(struct Object* blenderobject,
												  KX_Scene *kxscene)
{
	PHY_MaterialProps *materialProps = new PHY_MaterialProps;
	
	assert(materialProps);
		
	Material* blendermat = give_current_material(blenderobject, 0);
		
	if (blendermat)
	{
		assert(0.0f <= blendermat->reflect && blendermat->reflect <= 1.0f);
	
		materialProps->m_restitution = blendermat->reflect;
		materialProps->m_friction = blendermat->friction;
		materialProps->m_fh_spring = blendermat->fh;
		materialProps->m_fh_damping = blendermat->xyfrict;
		materialProps->m_fh_distance = blendermat->fhdist;
		materialProps->m_fh_normal = (blendermat->dynamode & MA_FH_NOR) != 0;
	}
	else {
		*materialProps = g_materialProps;
	}
	
	return materialProps;
}

static PHY_ShapeProps *CreateShapePropsFromBlenderObject(struct Object* blenderobject,
												 KX_Scene *kxscene)
{
	PHY_ShapeProps *shapeProps = new PHY_ShapeProps;
	
	assert(shapeProps);
		
	shapeProps->m_mass = blenderobject->mass;
	
//  This needs to be fixed in blender. For now, we use:
	
// in Blender, inertia stands for the size value which is equivalent to
// the sphere radius
	shapeProps->m_inertia = blenderobject->formfactor;
	
	assert(0.0f <= blenderobject->damping && blenderobject->damping <= 1.0f);
	assert(0.0f <= blenderobject->rdamping && blenderobject->rdamping <= 1.0f);
	
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
	


float my_boundbox_mesh(Mesh *me, float *loc, float *size)
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
		



void my_tex_space_mesh(Mesh *me)
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

void my_get_local_bounds(Object *ob, float *centre, float *size)
{
	BoundBox *bb= NULL;
	/* uses boundbox, function used by Ketsji */
	
	if(ob->type==OB_MESH) {
		bb= ( (Mesh *)ob->data )->bb;
		if(bb==0) {
			my_tex_space_mesh((struct Mesh *)ob->data);
			bb= ( (Mesh *)ob->data )->bb;
		}
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		centre[0]= centre[1]= centre[2]= 0.0;
		size[0]  = size[1]=size[2]=0.0;
	}
	else if(ob->type==OB_MBALL) {
		bb= ob->bb;
						}
	if(bb==NULL) {
		centre[0]= centre[1]= centre[2]= 0.0;
		size[0] = size[1]=size[2]=0.0;
					}
	else {
		size[0]= 0.5*fabs(bb->vec[0][0] - bb->vec[4][0]);
		size[1]= 0.5*fabs(bb->vec[0][1] - bb->vec[2][1]);
		size[2]= 0.5*fabs(bb->vec[0][2] - bb->vec[1][2]);
					
		centre[0]= 0.5*(bb->vec[0][0] + bb->vec[4][0]);
		centre[1]= 0.5*(bb->vec[0][1] + bb->vec[2][1]);
		centre[2]= 0.5*(bb->vec[0][2] + bb->vec[1][2]);
					}
}
	



//////////////////////////////////////////////////////





void BL_CreatePhysicsObjectNew(KX_GameObject* gameobj,
						 struct Object* blenderobject,
						 RAS_MeshObject* meshobj,
						 KX_Scene* kxscene,
						 int activeLayerBitInfo,
						 e_PhysicsEngine	physics_engine,
						 KX_BlenderSceneConverter *converter
						 )
					
{
	SYS_SystemHandle syshandle = SYS_GetSystem();
	//int userigidbody = SYS_GetCommandLineInt(syshandle,"norigidbody",0);
	//bool bRigidBody = (userigidbody == 0);

	PHY_ShapeProps* shapeprops =
			CreateShapePropsFromBlenderObject(blenderobject, 
			kxscene);

	
	PHY_MaterialProps* smmaterial = 
		CreateMaterialFromBlenderObject(blenderobject, kxscene);
					
	KX_ObjectProperties objprop;
	if ((objprop.m_isactor = (blenderobject->gameflag & OB_ACTOR)!=0))
	{
		objprop.m_dyna = (blenderobject->gameflag & OB_DYNAMIC) != 0;
		objprop.m_angular_rigidbody = (blenderobject->gameflag & OB_RIGID_BODY) != 0;
		objprop.m_ghost = (blenderobject->gameflag & OB_GHOST) != 0;
	} else {
		objprop.m_dyna = false;
		objprop.m_angular_rigidbody = false;
		objprop.m_ghost = false;
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
			case OB_BOUND_POLYH:
			{
				objprop.m_boundclass = KX_BOUNDMESH;
				break;
			}
		}
	}

	// get Root Parent of blenderobject
	struct Object* parent= blenderobject->parent;
	while(parent && parent->parent) {
		parent= parent->parent;
	}

	if (parent && (parent->gameflag & OB_DYNAMIC)) {
		
		KX_GameObject *parentgameobject = converter->FindGameObject(parent);
		objprop.m_dynamic_parent = parentgameobject;

	}

	objprop.m_concave = (blenderobject->boundtype & 4) != 0;
	
	switch (physics_engine)
	{
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

}





static KX_LightObject *gamelight_from_blamp(Lamp *la, unsigned int layerflag, KX_Scene *kxscene, RAS_IRenderTools *rendertools, KX_BlenderSceneConverter *converter) {
	RAS_LightObject lightobj;
	KX_LightObject *gamelight;
	
	lightobj.m_att1 = la->att1;
	lightobj.m_red = la->r;
	lightobj.m_green = la->g;
	lightobj.m_blue = la->b;
	lightobj.m_distance = la->dist;
	lightobj.m_energy = la->energy;
	lightobj.m_layer = layerflag;
	lightobj.m_spotblend = la->spotblend;
	lightobj.m_spotsize = la->spotsize;
	
	if (la->type==LA_SUN) {
		lightobj.m_type = RAS_LightObject::LIGHT_SUN;
	} else if (la->type==LA_SPOT) {
		lightobj.m_type = RAS_LightObject::LIGHT_SPOT;
	} else {
		lightobj.m_type = RAS_LightObject::LIGHT_NORMAL;
	}
	
	gamelight = new KX_LightObject(kxscene, KX_Scene::m_callbacks, rendertools, lightobj);
	BL_ConvertLampIpos(la, gamelight, converter);
	
	return gamelight;
}

static KX_Camera *gamecamera_from_bcamera(Camera *ca, KX_Scene *kxscene, KX_BlenderSceneConverter *converter) {
	RAS_CameraData camdata(ca->lens, ca->clipsta, ca->clipend, ca->type == CAM_PERSP);
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
		KX_LightObject* gamelight= gamelight_from_blamp(static_cast<Lamp*>(ob->data), ob->lay, kxscene, rendertools, converter);
		gameobj = gamelight;
		
		gamelight->AddRef();
		kxscene->GetLightList()->Add(gamelight);
		
		break;
	}
	
	case OB_CAMERA:
	{
		KX_Camera* gamecamera = gamecamera_from_bcamera(static_cast<Camera*>(ob->data), kxscene, converter);
		gameobj = gamecamera;
		
		gamecamera->AddRef();
		kxscene->AddCamera(gamecamera);
		
		break;
	}
	
	case OB_MESH:
	{
		Mesh* mesh = static_cast<Mesh*>(ob->data);
		RAS_MeshObject* meshobj = converter->FindGameMesh(mesh, ob->lay);
		float centre[3], extents[3];
		float radius = my_boundbox_mesh((Mesh*) ob->data, centre, extents);
		
		if (!meshobj) {
			meshobj = BL_ConvertMesh(mesh,ob,rendertools,kxscene,converter);
			converter->RegisterGameMesh(meshobj, mesh);
		}
		
		// needed for python scripting
		kxscene->GetLogicManager()->RegisterMeshName(meshobj->GetName(),meshobj);
	
		gameobj = new BL_DeformableGameObject(kxscene,KX_Scene::m_callbacks);
	
		// set transformation
		gameobj->AddMesh(meshobj);
	
		// for all objects: check whether they want to
		// respond to updates
		bool ignoreActivityCulling =  
			((ob->gameflag2 & OB_NEVER_DO_ACTIVITY_CULLING)!=0);
		gameobj->SetIgnoreActivityCulling(ignoreActivityCulling);
	
		//	If this is a skin object, make Skin Controller
		if (ob->parent && ob->parent->type == OB_ARMATURE && ob->partype==PARSKEL && ((Mesh*)ob->data)->dvert){
			BL_SkinDeformer *dcont = new BL_SkinDeformer(ob, (BL_SkinMeshObject*)meshobj);				
			((BL_DeformableGameObject*)gameobj)->m_pDeformer = dcont;
		}
		else if (((Mesh*)ob->data)->dvert){
			BL_MeshDeformer *dcont = new BL_MeshDeformer(ob, (BL_SkinMeshObject*)meshobj);
			((BL_DeformableGameObject*)gameobj)->m_pDeformer = dcont;
		}
		
		MT_Point3 min = MT_Point3(centre) - MT_Vector3(extents);
		MT_Point3 max = MT_Point3(centre) + MT_Vector3(extents);
		SG_BBox bbox = SG_BBox(min, max);
		gameobj->GetSGNode()->SetBBox(bbox);
		gameobj->GetSGNode()->SetRadius(radius);
	
		break;
	}
	
	case OB_ARMATURE:
	{
		gameobj = new BL_ArmatureObject (kxscene, KX_Scene::m_callbacks,
			(bArmature*)ob->data,
			ob->pose);
	
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

	// Get the frame settings of the canvas.
	// Get the aspect ratio of the canvas as designed by the user.

	RAS_FrameSettings::RAS_FrameType frame_type;
	int aspect_width;
	int aspect_height;
	
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
	CListValue* parentlist = kxscene->GetRootParentList();
	
	SCA_LogicManager* logicmgr = kxscene->GetLogicManager();
	SCA_TimeEventManager* timemgr = kxscene->GetTimeEventManager();
	
	CListValue* logicbrick_conversionlist = new CListValue();
	
	SG_TreeFactory tf;
	
	// Convert actions to actionmap
	bAction *curAct;
	for (curAct = (bAction*)maggie->action.first; curAct; curAct=(bAction*)curAct->id.next)
	{
		logicmgr->RegisterActionName(curAct->id.name, curAct);
	}
	
	Base *base = static_cast<Base*>(blenderscene->base.first);
	while(base)
	{
		Object* blenderobject = base->object;
		KX_GameObject* gameobj = gameobject_from_blenderobject(
										base->object, 
										kxscene, 
										rendertools, 
										converter,
										blenderscene);
											
		if (gameobj)
		{
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
			
			gameobj->NodeSetLocalPosition(pos);
			gameobj->NodeSetLocalOrientation(MT_Matrix3x3(eulxyz));
			gameobj->NodeSetLocalScale(scale);
			gameobj->NodeUpdateGS(0,true);
			
			BL_ConvertIpos(blenderobject,gameobj,converter);
	
			bool isInActiveLayer = (blenderobject->lay & activeLayerBitInfo) !=0;
	
			sumolist->Add(gameobj->AddRef());
			
			BL_ConvertProperties(blenderobject,gameobj,timemgr,kxscene,isInActiveLayer);
			
	
			gameobj->SetName(blenderobject->id.name);
	
			// templist to find Root Parents (object with no parents)
			templist->Add(gameobj->AddRef());
			
			// update children/parent hierarchy
			if (blenderobject->parent != 0)
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
	
			converter->RegisterGameObject(gameobj, blenderobject);	
			
			// this was put in rapidly, needs to be looked at more closely
			// only draw/use objects in active 'blender' layers
	
			logicbrick_conversionlist->Add(gameobj->AddRef());
			
			if (isInActiveLayer)
			{
				objectlist->Add(gameobj->AddRef());
				tf.Add(gameobj->GetSGNode());
				
				gameobj->NodeUpdateGS(0,true);
				gameobj->Bucketize();
				
			}
			
		}
			
		base = base->next;
	}

	if (blenderscene->camera) {
		KX_Camera *gamecamera= (KX_Camera*) converter->FindGameObject(blenderscene->camera);
		
		kxscene->SetActiveCamera(gamecamera);
	}

	//	Set up armatures
	for (base = static_cast<Base*>(blenderscene->base.first); base; base=base->next){
		if (base->object->type==OB_MESH){
			Mesh *me = (Mesh*)base->object->data;
	
			if (me->dvert){
				KX_GameObject *obj = converter->FindGameObject(base->object);
	
				if (base->object->parent && base->object->parent->type==OB_ARMATURE && base->object->partype==PARSKEL){
					KX_GameObject *par = converter->FindGameObject(base->object->parent);
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
		if (blenderchild->partype == PARVERT1)
		{
			// creat a new vertex parent relationship for this node.
			KX_VertexParentRelation * vertex_parent_relation = KX_VertexParentRelation::New();
			pcit->m_gamechildnode->SetParentRelation(vertex_parent_relation);
		} else 
		if (blenderchild->partype == PARSLOW) 
		{
			// creat a new slow parent relationship for this node.
			KX_SlowParentRelation * slow_parent_relation = KX_SlowParentRelation::New(blenderchild->sf);
			pcit->m_gamechildnode->SetParentRelation(slow_parent_relation);
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

		BL_CreatePhysicsObjectNew(gameobj,blenderobject,meshobj,kxscene,activeLayerBitInfo,physics_engine,converter);

	}
	
	templist->Release();
	sumolist->Release();	


	int executePriority=0; /* incremented by converter routines */
	
	// convert global sound stuff

	/* XXX, glob is the very very wrong place for this
	 * to be, re-enable once the listener has been moved into
	 * the scene. */
#if 0
	SND_Scene* soundscene = kxscene->GetSoundScene();
	SND_SoundListener* listener = soundscene->GetListener();
	if (listener && glob->listener)
	{
		listener->SetDopplerFactor(glob->listener->dopplerfactor);
		listener->SetDopplerVelocity(glob->listener->dopplervelocity);
		listener->SetGain(glob->listener->gain);
	}
#endif

	// convert world
	KX_WorldInfo* worldinfo = new BlenderWorldInfo(blenderscene->world);
	converter->RegisterWorldInfo(worldinfo);
	kxscene->SetWorldInfo(worldinfo);
	
	// convert logic bricks, sensors, controllers and actuators
	for (i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = converter->FindBlenderObject(gameobj);
		bool isInActiveLayer = (blenderobj->lay & activeLayerBitInfo)!=0;
		BL_ConvertActuators(maggie->name, blenderobj,gameobj,logicmgr,kxscene,ketsjiEngine,executePriority, activeLayerBitInfo,isInActiveLayer,rendertools,converter);
	}
	for ( i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = converter->FindBlenderObject(gameobj);
		bool isInActiveLayer = (blenderobj->lay & activeLayerBitInfo)!=0;
		BL_ConvertControllers(blenderobj,gameobj,logicmgr,pythondictionary,executePriority,activeLayerBitInfo,isInActiveLayer,converter);
	}
	for ( i=0;i<logicbrick_conversionlist->GetCount();i++)
	{
		KX_GameObject* gameobj = static_cast<KX_GameObject*>(logicbrick_conversionlist->GetValue(i));
		struct Object* blenderobj = converter->FindBlenderObject(gameobj);
		bool isInActiveLayer = (blenderobj->lay & activeLayerBitInfo)!=0;
		BL_ConvertSensors(blenderobj,gameobj,logicmgr,kxscene,keydev,executePriority,activeLayerBitInfo,isInActiveLayer,canvas,converter);
	}
	logicbrick_conversionlist->Release();
	
	// Calculate the scene btree -
	// too slow - commented out.
	//kxscene->SetNodeTree(tf.MakeTree());
}

