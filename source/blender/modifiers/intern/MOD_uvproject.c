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

/** \file blender/modifiers/intern/MOD_uvproject.c
 *  \ingroup modifiers
 */


/* UV Project modifier: Generates UVs projected from an object */

#include "DNA_meshdata_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_uvproject.h"
#include "BLI_utildefines.h"


#include "BKE_camera.h"
#include "BKE_DerivedMesh.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"

#include "MEM_guardedalloc.h"
#include "depsgraph_private.h"

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
	BLI_strncpy(tumd->uvlayer_name, umd->uvlayer_name, sizeof(umd->uvlayer_name));
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *UNUSED(md))
{
	CustomDataMask dataMask = 0;

	/* ask for UV coordinates */
	dataMask |= CD_MASK_MTFACE;

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

static void updateDepgraph(ModifierData *md, DagForest *forest,
						struct Scene *UNUSED(scene),
						Object *UNUSED(ob),
						DagNode *obNode)
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

	/* make sure there are UV Maps available */

	if(!CustomData_has_layer(&dm->faceData, CD_MTFACE)) return dm;

	/* make sure we're using an existing layer */
	CustomData_validate_layer_name(&dm->faceData, CD_MTFACE, umd->uvlayer_name, uvname);

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
				project_camera_info_scale(projectors[i].uci, scax, scay);
				free_uci= 1;
			}
			else {
				float sensor= camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
				int sensor_fit= camera_sensor_fit(cam->sensor_fit, aspx, aspy);
				float scale= (cam->type == CAM_PERSP) ? cam->clipsta * sensor / cam->lens : cam->ortho_scale;
				float xmax, xmin, ymax, ymin;

				if(sensor_fit==CAMERA_SENSOR_FIT_HOR) {
					xmax = 0.5f * scale;
					ymax = xmax / aspect;
				}
				else {
					ymax = 0.5f * scale;
					xmax = ymax * aspect;
				}

				xmin = -xmax;
				ymin = -ymax;

				/* scale the matrix */
				xmin *= scax;
				xmax *= scax;
				ymin *= scay;
				ymax *= scay;

				if(cam->type == CAM_PERSP) {
					float perspmat[4][4];
					perspective_m4( perspmat,xmin, xmax, ymin, ymax, cam->clipsta, cam->clipend);
					mult_m4_m4m4(tmpmat, perspmat, projectors[i].projmat);
				} else { /* if(cam->type == CAM_ORTHO) */
					float orthomat[4][4];
					orthographic_m4( orthomat,xmin, xmax, ymin, ymax, cam->clipsta, cam->clipend);
					mult_m4_m4m4(tmpmat, orthomat, projectors[i].projmat);
				}
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
		
		mult_m4_m4m4(projectors[i].projmat, offsetmat, tmpmat);

		/* calculate worldspace projector normal (for best projector test) */
		projectors[i].normal[0] = 0;
		projectors[i].normal[1] = 0;
		projectors[i].normal[2] = 1;
		mul_mat3_m4_v3(projectors[i].ob->obmat, projectors[i].normal);
	}

	numFaces = dm->getNumFaces(dm);

	/* make sure we are not modifying the original UV map */
	tface = CustomData_duplicate_referenced_layer_named(&dm->faceData,
			CD_MTFACE, uvname, numFaces);

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
			mul_project_m4_v3(projectors[0].projmat, *co);

	mface = dm->getFaceArray(dm);

	/* apply coords as UVs, and apply image if tfaces are new */
	for(i = 0, mf = mface; i < numFaces; ++i, ++mf, ++tface) {
		if(override_image || !image || tface->tpage == image) {
			if(num_projectors == 1) {
				if(projectors[0].uci) {
					unsigned int fidx= mf->v4 ? 3:2;
					do {
						unsigned int vidx= *(&mf->v1 + fidx);
						project_from_camera(tface->uv[fidx], coords[vidx], projectors[0].uci);
					} while (fidx--);
				}
				else {
					/* apply transformed coords as UVs */
					unsigned int fidx= mf->v4 ? 3:2;
					do {
						unsigned int vidx= *(&mf->v1 + fidx);
						copy_v2_v2(tface->uv[fidx], coords[vidx]);
					} while (fidx--);
				}
			} else {
				/* multiple projectors, select the closest to face normal
				* direction
				*/
				float face_no[3];
				int j;
				Projector *best_projector;
				float best_dot;

				/* get the untransformed face normal */
				if(mf->v4) {
					normal_quad_v3(face_no, coords[mf->v1], coords[mf->v2], coords[mf->v3], coords[mf->v4]);
				} else { 
					normal_tri_v3(face_no, coords[mf->v1], coords[mf->v2], coords[mf->v3]);
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
					unsigned int fidx= mf->v4 ? 3:2;
					do {
						unsigned int vidx= *(&mf->v1 + fidx);
						project_from_camera(tface->uv[fidx], coords[vidx], best_projector->uci);
					} while (fidx--);
				}
				else {
					unsigned int fidx= mf->v4 ? 3:2;
					do {
						unsigned int vidx= *(&mf->v1 + fidx);
						float tco[3];

						copy_v3_v3(tco, coords[vidx]);
						mul_project_m4_v3(best_projector->projmat, tco);
						copy_v2_v2(tface->uv[fidx], tco);

					} while (fidx--);
				}
			}
		}

		if(override_image) {
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

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	DerivedMesh *result;
	UVProjectModifierData *umd = (UVProjectModifierData*) md;

	result = uvprojectModifier_do(umd, ob, derivedData);

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
						struct EditMesh *UNUSED(editData),
						DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_UVProject = {
	/* name */              "UVProject",
	/* structName */        "UVProjectModifierData",
	/* structSize */        sizeof(UVProjectModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL,
};
