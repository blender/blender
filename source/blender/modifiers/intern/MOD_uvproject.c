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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2005 by the Blender Foundation.
* All rights reserved.
*
* Contributor(s): Daniel Dunbar
*                 Ton Roosendaal,
*                 Ben Batt,
*                 Brecht Van Lommel,
*                 Campbell Barton
*
* ***** END GPL LICENSE BLOCK *****
*
*/

#include "stddef.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"

#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_uvproject.h"

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_fluidsim.h"


#include "BKE_action.h"
#include "BKE_bmesh.h"
#include "BKE_cloth.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "CCGSubSurf.h"

#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"


/* UVProject */
/* UV Project modifier: Generates UVs projected from an object
*/

static void initData(ModifierData *md)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;
	int i;

	for(i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; ++i)
		umd->projectors[i] = NULL;
	umd->image = NULL;
	umd->flags = 0;
	umd->num_projectors = 1;
	umd->aspectx = umd->aspecty = 1.0f;
	umd->scalex = umd->scaley = 1.0f;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;
	UVProjectModifierData *tumd = (UVProjectModifierData*) target;
	int i;

	for(i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; ++i)
		tumd->projectors[i] = umd->projectors[i];
	tumd->image = umd->image;
	tumd->flags = umd->flags;
	tumd->num_projectors = umd->num_projectors;
	tumd->aspectx = umd->aspectx;
	tumd->aspecty = umd->aspecty;
	tumd->scalex = umd->scalex;
	tumd->scaley = umd->scaley;
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	CustomDataMask dataMask = 0;

	/* ask for UV coordinates */
	dataMask |= (1 << CD_MTFACE);

	return dataMask;
}

static void foreachObjectLink(ModifierData *md, Object *ob,
		ObjectWalkFunc walk, void *userData)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;
	int i;

	for(i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; ++i)
		walk(userData, ob, &umd->projectors[i]);
}

static void foreachIDLink(ModifierData *md, Object *ob,
						IDWalkFunc walk, void *userData)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;

	walk(userData, ob, (ID **)&umd->image);

	foreachObjectLink(md, ob, (ObjectWalkFunc)walk,
						userData);
}

static void updateDepgraph(ModifierData *md,
						 DagForest *forest, Scene *scene, Object *ob, DagNode *obNode)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;
	int i;

	for(i = 0; i < umd->num_projectors; ++i) {
		if(umd->projectors[i]) {
			DagNode *curNode = dag_get_node(forest, umd->projectors[i]);

			dag_add_relation(forest, curNode, obNode,
					 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "UV Project Modifier");
		}
	}
}

typedef struct Projector {
	Object *ob;				/* object this projector is derived from */
	float projmat[4][4];	/* projection matrix */ 
	float normal[3];		/* projector normal in world space */
	void *uci;				/* optional uv-project info (panorama projection) */
} Projector;

static DerivedMesh *uvprojectModifier_do(UVProjectModifierData *umd,
					 Object *ob, DerivedMesh *dm)
{
	float (*coords)[3], (*co)[3];
	MTFace *tface;
	int i, numVerts, numFaces;
	Image *image = umd->image;
	MFace *mface, *mf;
	int override_image = ((umd->flags & MOD_UVPROJECT_OVERRIDEIMAGE) != 0);
	Projector projectors[MOD_UVPROJECT_MAXPROJECTORS];
	int num_projectors = 0;
	float aspect;
	char uvname[32];
	float aspx= umd->aspectx ? umd->aspectx : 1.0f;
	float aspy= umd->aspecty ? umd->aspecty : 1.0f;
	float scax= umd->scalex ? umd->scalex : 1.0f;
	float scay= umd->scaley ? umd->scaley : 1.0f;
	int free_uci= 0;
	
	aspect = aspx / aspy;

	for(i = 0; i < umd->num_projectors; ++i)
		if(umd->projectors[i])
			projectors[num_projectors++].ob = umd->projectors[i];

	if(num_projectors == 0) return dm;

	/* make sure there are UV layers available */

	if(!CustomData_has_layer(&dm->faceData, CD_MTFACE)) return dm;

	/* make sure we're using an existing layer */
	validate_layer_name(&dm->faceData, CD_MTFACE, umd->uvlayer_name, uvname);

	/* calculate a projection matrix and normal for each projector */
	for(i = 0; i < num_projectors; ++i) {
		float tmpmat[4][4];
		float offsetmat[4][4];
		Camera *cam = NULL;
		/* calculate projection matrix */
		invert_m4_m4(projectors[i].projmat, projectors[i].ob->obmat);

		projectors[i].uci= NULL;

		if(projectors[i].ob->type == OB_CAMERA) {
			cam = (Camera *)projectors[i].ob->data;

			if(cam->flag & CAM_PANORAMA) {
				projectors[i].uci= project_camera_info(projectors[i].ob, NULL, aspx, aspy);
				free_uci= 1;
			}
			else if(cam->type == CAM_PERSP) {
				float perspmat[4][4];
				float xmax; 
				float xmin;
				float ymax;
				float ymin;
				float pixsize = cam->clipsta * 32.0 / cam->lens;

				if(aspect > 1.0f) {
					xmax = 0.5f * pixsize;
					ymax = xmax / aspect;
				} else {
					ymax = 0.5f * pixsize;
					xmax = ymax * aspect; 
				}
				xmin = -xmax;
				ymin = -ymax;

				perspective_m4( perspmat,xmin, xmax, ymin, ymax, cam->clipsta, cam->clipend);
				mul_m4_m4m4(tmpmat, projectors[i].projmat, perspmat);
			} else if(cam->type == CAM_ORTHO) {
				float orthomat[4][4];
				float xmax; 
				float xmin;
				float ymax;
				float ymin;

				if(aspect > 1.0f) {
					xmax = 0.5f * cam->ortho_scale; 
					ymax = xmax / aspect;
				} else {
					ymax = 0.5f * cam->ortho_scale;
					xmax = ymax * aspect; 
				}
				xmin = -xmax;
				ymin = -ymax;

				orthographic_m4( orthomat,xmin, xmax, ymin, ymax, cam->clipsta, cam->clipend);
				mul_m4_m4m4(tmpmat, projectors[i].projmat, orthomat);
			}
		} else {
			copy_m4_m4(tmpmat, projectors[i].projmat);
		}

		unit_m4(offsetmat);
		mul_mat3_m4_fl(offsetmat, 0.5);
		offsetmat[3][0] = offsetmat[3][1] = offsetmat[3][2] = 0.5;
		
		if (cam) {
			if (aspx == aspy) { 
				offsetmat[3][0] -= cam->shiftx;
				offsetmat[3][1] -= cam->shifty;
			} else if (aspx < aspy)  {
				offsetmat[3][0] -=(cam->shiftx * aspy/aspx);
				offsetmat[3][1] -= cam->shifty;
			} else {
				offsetmat[3][0] -= cam->shiftx;
				offsetmat[3][1] -=(cam->shifty * aspx/aspy);
			}
		}
		
		mul_m4_m4m4(projectors[i].projmat, tmpmat, offsetmat);

		/* calculate worldspace projector normal (for best projector test) */
		projectors[i].normal[0] = 0;
		projectors[i].normal[1] = 0;
		projectors[i].normal[2] = 1;
		mul_mat3_m4_v3(projectors[i].ob->obmat, projectors[i].normal);
	}

	/* make sure we are not modifying the original UV layer */
	tface = CustomData_duplicate_referenced_layer_named(&dm->faceData,
			CD_MTFACE, uvname);

	
	numVerts = dm->getNumVerts(dm);

	coords = MEM_callocN(sizeof(*coords) * numVerts,
				 "uvprojectModifier_do coords");
	dm->getVertCos(dm, coords);

	/* convert coords to world space */
	for(i = 0, co = coords; i < numVerts; ++i, ++co)
		mul_m4_v3(ob->obmat, *co);
	
	/* if only one projector, project coords to UVs */
	if(num_projectors == 1 && projectors[0].uci==NULL)
		for(i = 0, co = coords; i < numVerts; ++i, ++co)
			mul_project_m4_v4(projectors[0].projmat, *co);

	mface = dm->getFaceArray(dm);
	numFaces = dm->getNumFaces(dm);

	/* apply coords as UVs, and apply image if tfaces are new */
	for(i = 0, mf = mface; i < numFaces; ++i, ++mf, ++tface) {
		if(override_image || !image || tface->tpage == image) {
				if(num_projectors == 1) {
					if(projectors[0].uci) {
						project_from_camera(tface->uv[0], coords[mf->v1], projectors[0].uci);
						project_from_camera(tface->uv[1], coords[mf->v2], projectors[0].uci);
						project_from_camera(tface->uv[2], coords[mf->v3], projectors[0].uci);
						if(mf->v3)
							project_from_camera(tface->uv[3], coords[mf->v4], projectors[0].uci);
						
						if(scax != 1.0f) {
							tface->uv[0][0] = ((tface->uv[0][0] - 0.5f) * scax) + 0.5f;
							tface->uv[1][0] = ((tface->uv[1][0] - 0.5f) * scax) + 0.5f;
							tface->uv[2][0] = ((tface->uv[2][0] - 0.5f) * scax) + 0.5f;
							if(mf->v3)
								tface->uv[3][0] = ((tface->uv[3][0] - 0.5f) * scax) + 0.5f;
						}
						
						if(scay != 1.0f) {
							tface->uv[0][1] = ((tface->uv[0][1] - 0.5f) * scay) + 0.5f;
							tface->uv[1][1] = ((tface->uv[1][1] - 0.5f) * scay) + 0.5f;
							tface->uv[2][1] = ((tface->uv[2][1] - 0.5f) * scay) + 0.5f;
							if(mf->v3)
								tface->uv[3][1] = ((tface->uv[3][1] - 0.5f) * scay) + 0.5f;
						}
					}
					else {
						/* apply transformed coords as UVs */
						tface->uv[0][0] = coords[mf->v1][0];
						tface->uv[0][1] = coords[mf->v1][1];
						tface->uv[1][0] = coords[mf->v2][0];
						tface->uv[1][1] = coords[mf->v2][1];
						tface->uv[2][0] = coords[mf->v3][0];
						tface->uv[2][1] = coords[mf->v3][1];
						if(mf->v4) {
							tface->uv[3][0] = coords[mf->v4][0];
							tface->uv[3][1] = coords[mf->v4][1];
						}
				}
			} else {
				/* multiple projectors, select the closest to face normal
				* direction
				*/
				float co1[3], co2[3], co3[3], co4[3];
				float face_no[3];
				int j;
				Projector *best_projector;
				float best_dot;

				VECCOPY(co1, coords[mf->v1]);
				VECCOPY(co2, coords[mf->v2]);
				VECCOPY(co3, coords[mf->v3]);

				/* get the untransformed face normal */
				if(mf->v4) {
					VECCOPY(co4, coords[mf->v4]);
					normal_quad_v3( face_no,co1, co2, co3, co4);
				} else { 
					normal_tri_v3( face_no,co1, co2, co3);
				}

				/* find the projector which the face points at most directly
				* (projector normal with largest dot product is best)
				*/
				best_dot = dot_v3v3(projectors[0].normal, face_no);
				best_projector = &projectors[0];

				for(j = 1; j < num_projectors; ++j) {
					float tmp_dot = dot_v3v3(projectors[j].normal,
							face_no);
					if(tmp_dot > best_dot) {
						best_dot = tmp_dot;
						best_projector = &projectors[j];
					}
				}
				
				if(best_projector->uci) {
					project_from_camera(tface->uv[0], coords[mf->v1], best_projector->uci);
					project_from_camera(tface->uv[1], coords[mf->v2], best_projector->uci);
					project_from_camera(tface->uv[2], coords[mf->v3], best_projector->uci);
					if(mf->v3)
						project_from_camera(tface->uv[3], coords[mf->v4], best_projector->uci);
				}
				else {
					mul_project_m4_v4(best_projector->projmat, co1);
					mul_project_m4_v4(best_projector->projmat, co2);
					mul_project_m4_v4(best_projector->projmat, co3);
					if(mf->v4)
						mul_project_m4_v4(best_projector->projmat, co4);

					/* apply transformed coords as UVs */
					tface->uv[0][0] = co1[0];
					tface->uv[0][1] = co1[1];
					tface->uv[1][0] = co2[0];
					tface->uv[1][1] = co2[1];
					tface->uv[2][0] = co3[0];
					tface->uv[2][1] = co3[1];
					if(mf->v4) {
						tface->uv[3][0] = co4[0];
						tface->uv[3][1] = co4[1];
					}
				}
			}
		}

		if(override_image) {
			tface->mode = TF_TEX;
			tface->tpage = image;
		}
	}

	MEM_freeN(coords);
	
	if(free_uci) {
		int j;
		for(j = 0; j < num_projectors; ++j) {
			if(projectors[j].uci) {
				MEM_freeN(projectors[j].uci);
			}
		}
	}
	return dm;
}

static DerivedMesh *applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	UVProjectModifierData *umd = (UVProjectModifierData*) md;

	result = uvprojectModifier_do(umd, ob, derivedData);

	return result;
}

static DerivedMesh *applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_UVProject = {
	/* name */              "UVProject",
	/* structName */        "UVProjectModifierData",
	/* structSize */        sizeof(UVProjectModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
};
