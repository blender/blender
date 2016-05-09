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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_snap_object.c
 *  \ingroup edtransform
 */

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_kdopbvh.h"
#include "BLI_memarena.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_anim.h"  /* for duplis */
#include "BKE_editmesh.h"
#include "BKE_main.h"
#include "BKE_tracking.h"

#include "ED_transform.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"
#include "ED_armature.h"

#include "transform.h"

typedef struct SnapObjectData {
	enum {
		SNAP_MESH = 1,
		SNAP_EDIT_MESH,
	} type;
} SnapObjectData;

typedef struct SnapObjectData_Mesh {
	SnapObjectData sd;
	BVHTreeFromMesh *bvh_trees[2];

} SnapObjectData_Mesh;

typedef struct SnapObjectData_EditMesh {
	SnapObjectData sd;
	BVHTreeFromEditMesh *bvh_trees[2];

} SnapObjectData_EditMesh;

struct SnapObjectContext {
	Main *bmain;
	Scene *scene;
	int flag;

	/* Optional: when performing screen-space projection.
	 * otherwise this doesn't take viewport into account. */
	bool use_v3d;
	struct {
		struct View3D *v3d;
		struct ARegion *ar;
	} v3d_data;


	/* Object -> SnapObjectData map */
	struct {
		GHash *object_map;
		MemArena *mem_arena;
	} cache;

	/* Filter data, returns true to check this value */
	struct {
		struct {
			bool (*test_vert_fn)(BMVert *, void *user_data);
			bool (*test_edge_fn)(BMEdge *, void *user_data);
			bool (*test_face_fn)(BMFace *, void *user_data);
			void *user_data;
		} edit_mesh;
	} callbacks;

};


/* -------------------------------------------------------------------- */

/** \name Internal Object Snapping API
 * \{ */

static bool snapEdge(
        ARegion *ar, const float v1co[3], const short v1no[3], const float v2co[3], const short v2no[3],
        float obmat[4][4], float timat[3][3], const float mval_fl[2], float *dist_px,
        const float ray_start[3], const float ray_start_local[3], const float ray_normal_local[3], float *ray_depth,
        float r_loc[3], float r_no[3])
{
	float intersect[3] = {0, 0, 0}, ray_end[3], dvec[3];
	int result;
	bool retval = false;

	copy_v3_v3(ray_end, ray_normal_local);
	mul_v3_fl(ray_end, 2000);
	add_v3_v3v3(ray_end, ray_start_local, ray_end);

	/* dvec used but we don't care about result */
	result = isect_line_line_v3(v1co, v2co, ray_start_local, ray_end, intersect, dvec);

	if (result) {
		float edge_loc[3], vec[3];
		float mul;

		/* check for behind ray_start */
		sub_v3_v3v3(dvec, intersect, ray_start_local);

		sub_v3_v3v3(edge_loc, v1co, v2co);
		sub_v3_v3v3(vec, intersect, v2co);

		mul = dot_v3v3(vec, edge_loc) / dot_v3v3(edge_loc, edge_loc);

		if (mul > 1) {
			mul = 1;
			copy_v3_v3(intersect, v1co);
		}
		else if (mul < 0) {
			mul = 0;
			copy_v3_v3(intersect, v2co);
		}

		if (dot_v3v3(ray_normal_local, dvec) > 0) {
			float location[3];
			float new_depth;
			float screen_loc[2];
			float new_dist;

			copy_v3_v3(location, intersect);

			mul_m4_v3(obmat, location);

			new_depth = len_v3v3(location, ray_start);

			if (ED_view3d_project_float_global(ar, location, screen_loc, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
				new_dist = len_manhattan_v2v2(mval_fl, screen_loc);
			}
			else {
				new_dist = TRANSFORM_DIST_MAX_PX;
			}

			/* 10% threshold if edge is closer but a bit further
			 * this takes care of series of connected edges a bit slanted w.r.t the viewport
			 * otherwise, it would stick to the verts of the closest edge and not slide along merrily
			 * */
			if (new_dist <= *dist_px && new_depth < *ray_depth * 1.001f) {
				float n1[3], n2[3];

				*ray_depth = new_depth;
				retval = true;

				sub_v3_v3v3(edge_loc, v1co, v2co);
				sub_v3_v3v3(vec, intersect, v2co);

				mul = dot_v3v3(vec, edge_loc) / dot_v3v3(edge_loc, edge_loc);

				if (r_no) {
					normal_short_to_float_v3(n1, v1no);
					normal_short_to_float_v3(n2, v2no);
					interp_v3_v3v3(r_no, n2, n1, mul);
					mul_m3_v3(timat, r_no);
					normalize_v3(r_no);
				}

				copy_v3_v3(r_loc, location);

				*dist_px = new_dist;
			}
		}
	}

	return retval;
}

static bool snapVertex(
        ARegion *ar, const float vco[3], const float vno[3],
        float obmat[4][4], float timat[3][3], const float mval_fl[2], float *dist_px,
        const float ray_start[3], const float ray_start_local[3], const float ray_normal_local[3], float *ray_depth,
        float r_loc[3], float r_no[3])
{
	bool retval = false;
	float dvec[3];

	sub_v3_v3v3(dvec, vco, ray_start_local);

	if (dot_v3v3(ray_normal_local, dvec) > 0) {
		float location[3];
		float new_depth;
		float screen_loc[2];
		float new_dist;

		copy_v3_v3(location, vco);

		mul_m4_v3(obmat, location);

		new_depth = len_v3v3(location, ray_start);

		if (ED_view3d_project_float_global(ar, location, screen_loc, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
			new_dist = len_manhattan_v2v2(mval_fl, screen_loc);
		}
		else {
			new_dist = TRANSFORM_DIST_MAX_PX;
		}


		if (new_dist <= *dist_px && new_depth < *ray_depth) {
			*ray_depth = new_depth;
			retval = true;

			copy_v3_v3(r_loc, location);

			if (r_no) {
				copy_v3_v3(r_no, vno);
				mul_m3_v3(timat, r_no);
				normalize_v3(r_no);
			}

			*dist_px = new_dist;
		}
	}

	return retval;
}

static bool snapArmature(
        ARegion *ar, Object *ob, bArmature *arm, float obmat[4][4],
        const float mval[2], float *dist_px, const short snap_to,
        const float ray_start[3], const float ray_normal[3], float *ray_depth,
        float r_loc[3], float *UNUSED(r_no))
{
	float imat[4][4];
	float ray_start_local[3], ray_normal_local[3];
	bool retval = false;

	invert_m4_m4(imat, obmat);

	mul_v3_m4v3(ray_start_local, imat, ray_start);
	mul_v3_mat3_m4v3(ray_normal_local, imat, ray_normal);

	if (arm->edbo) {
		EditBone *eBone;

		for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
			if (eBone->layer & arm->layer) {
				/* skip hidden or moving (selected) bones */
				if ((eBone->flag & (BONE_HIDDEN_A | BONE_ROOTSEL | BONE_TIPSEL)) == 0) {
					switch (snap_to) {
						case SCE_SNAP_MODE_VERTEX:
							retval |= snapVertex(
							        ar, eBone->head, NULL, obmat, NULL, mval, dist_px,
							        ray_start, ray_start_local, ray_normal_local, ray_depth,
							        r_loc, NULL);
							retval |= snapVertex(
							        ar, eBone->tail, NULL, obmat, NULL, mval, dist_px,
							        ray_start, ray_start_local, ray_normal_local, ray_depth,
							        r_loc, NULL);
							break;
						case SCE_SNAP_MODE_EDGE:
							retval |= snapEdge(
							        ar, eBone->head, NULL, eBone->tail, NULL,
							        obmat, NULL, mval, dist_px,
							        ray_start, ray_start_local, ray_normal_local,
							        ray_depth, r_loc, NULL);
							break;
					}
				}
			}
		}
	}
	else if (ob->pose && ob->pose->chanbase.first) {
		bPoseChannel *pchan;
		Bone *bone;

		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			bone = pchan->bone;
			/* skip hidden bones */
			if (bone && !(bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
				const float *head_vec = pchan->pose_head;
				const float *tail_vec = pchan->pose_tail;

				switch (snap_to) {
					case SCE_SNAP_MODE_VERTEX:
						retval |= snapVertex(
						        ar, head_vec, NULL, obmat, NULL, mval, dist_px,
						        ray_start, ray_start_local, ray_normal_local,
						        ray_depth, r_loc, NULL);
						retval |= snapVertex(
						        ar, tail_vec, NULL, obmat, NULL, mval, dist_px,
						        ray_start, ray_start_local, ray_normal_local, ray_depth,
						        r_loc, NULL);
						break;
					case SCE_SNAP_MODE_EDGE:
						retval |= snapEdge(
						        ar, head_vec, NULL, tail_vec, NULL,
						        obmat, NULL, mval, dist_px,
						        ray_start, ray_start_local, ray_normal_local,
						        ray_depth, r_loc, NULL);
						break;
				}
			}
		}
	}

	return retval;
}

static bool snapCurve(
        ARegion *ar, Object *ob, Curve *cu, float obmat[4][4],
        const float mval[2], float *dist_px, const short snap_to,
        const float ray_start[3], const float ray_normal[3], float *ray_depth,
        float r_loc[3], float *UNUSED(r_no))
{
	float imat[4][4];
	float ray_start_local[3], ray_normal_local[3];
	bool retval = false;
	int u;

	Nurb *nu;

	/* only vertex snapping mode (eg control points and handles) supported for now) */
	if (snap_to != SCE_SNAP_MODE_VERTEX) {
		return retval;
	}

	invert_m4_m4(imat, obmat);

	copy_v3_v3(ray_start_local, ray_start);
	copy_v3_v3(ray_normal_local, ray_normal);

	mul_m4_v3(imat, ray_start_local);
	mul_mat3_m4_v3(imat, ray_normal_local);

	for (nu = (ob->mode == OB_MODE_EDIT ? cu->editnurb->nurbs.first : cu->nurb.first); nu; nu = nu->next) {
		for (u = 0; u < nu->pntsu; u++) {
			switch (snap_to) {
				case SCE_SNAP_MODE_VERTEX:
				{
					if (ob->mode == OB_MODE_EDIT) {
						if (nu->bezt) {
							/* don't snap to selected (moving) or hidden */
							if (nu->bezt[u].f2 & SELECT || nu->bezt[u].hide != 0) {
								break;
							}
							retval |= snapVertex(
							        ar, nu->bezt[u].vec[1], NULL, obmat, NULL,  mval, dist_px,
							        ray_start, ray_start_local, ray_normal_local, ray_depth,
							        r_loc, NULL);
							/* don't snap if handle is selected (moving), or if it is aligning to a moving handle */
							if (!(nu->bezt[u].f1 & SELECT) &&
							    !(nu->bezt[u].h1 & HD_ALIGN && nu->bezt[u].f3 & SELECT))
							{
								retval |= snapVertex(
								        ar, nu->bezt[u].vec[0], NULL, obmat, NULL, mval, dist_px,
								        ray_start, ray_start_local, ray_normal_local, ray_depth,
								        r_loc, NULL);
							}
							if (!(nu->bezt[u].f3 & SELECT) &&
							    !(nu->bezt[u].h2 & HD_ALIGN && nu->bezt[u].f1 & SELECT))
							{
								retval |= snapVertex(
								        ar, nu->bezt[u].vec[2], NULL, obmat, NULL, mval, dist_px,
								        ray_start, ray_start_local, ray_normal_local, ray_depth,
								        r_loc, NULL);
							}
						}
						else {
							/* don't snap to selected (moving) or hidden */
							if (nu->bp[u].f1 & SELECT || nu->bp[u].hide != 0) {
								break;
							}
							retval |= snapVertex(
							        ar, nu->bp[u].vec, NULL, obmat, NULL, mval, dist_px,
							        ray_start, ray_start_local, ray_normal_local, ray_depth,
							        r_loc, NULL);
						}
					}
					else {
						/* curve is not visible outside editmode if nurb length less than two */
						if (nu->pntsu > 1) {
							if (nu->bezt) {
								retval |= snapVertex(
								        ar, nu->bezt[u].vec[1], NULL, obmat, NULL, mval, dist_px,
								        ray_start, ray_start_local, ray_normal_local, ray_depth,
								        r_loc, NULL);
							}
							else {
								retval |= snapVertex(
								        ar, nu->bp[u].vec, NULL, obmat, NULL, mval, dist_px,
								        ray_start, ray_start_local, ray_normal_local, ray_depth,
								        r_loc, NULL);
							}
						}
					}
					break;
				}
				default:
					break;
			}
		}
	}
	return retval;
}

/* may extend later (for now just snaps to empty center) */
static bool snapEmpty(
        ARegion *ar, Object *ob, float obmat[4][4],
        const float mval[2], float *dist_px, const short snap_to,
        const float ray_start[3], const float ray_normal[3], float *ray_depth,
        float r_loc[3], float *UNUSED(r_no))
{
	float imat[4][4];
	float ray_start_local[3], ray_normal_local[3];
	bool retval = false;

	if (ob->transflag & OB_DUPLI) {
		return retval;
	}
	/* for now only vertex supported */
	if (snap_to != SCE_SNAP_MODE_VERTEX) {
		return retval;
	}

	invert_m4_m4(imat, obmat);

	mul_v3_m4v3(ray_start_local, imat, ray_start);
	mul_v3_mat3_m4v3(ray_normal_local, imat, ray_normal);

	switch (snap_to) {
		case SCE_SNAP_MODE_VERTEX:
		{
			const float zero_co[3] = {0.0f};
			retval |= snapVertex(
			        ar, zero_co, NULL, obmat, NULL, mval, dist_px,
			        ray_start, ray_start_local, ray_normal_local, ray_depth,
			        r_loc, NULL);
			break;
		}
		default:
			break;
	}

	return retval;
}

static bool snapCamera(
        ARegion *ar, Scene *scene, Object *object, float obmat[4][4],
        const float mval[2], float *dist_px, const short snap_to,
        const float ray_start[3], const float ray_normal[3], float *ray_depth,
        float r_loc[3], float *UNUSED(r_no))
{
	float orig_camera_mat[4][4], orig_camera_imat[4][4], imat[4][4];
	bool retval = false;
	MovieClip *clip = BKE_object_movieclip_get(scene, object, false);
	MovieTracking *tracking;
	float ray_start_local[3], ray_normal_local[3];

	if (clip == NULL) {
		return retval;
	}
	if (object->transflag & OB_DUPLI) {
		return retval;
	}

	tracking = &clip->tracking;

	BKE_tracking_get_camera_object_matrix(scene, object, orig_camera_mat);

	invert_m4_m4(orig_camera_imat, orig_camera_mat);
	invert_m4_m4(imat, obmat);

	switch (snap_to) {
		case SCE_SNAP_MODE_VERTEX:
		{
			MovieTrackingObject *tracking_object;

			for (tracking_object = tracking->objects.first;
			     tracking_object;
			     tracking_object = tracking_object->next)
			{
				ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
				MovieTrackingTrack *track;
				float reconstructed_camera_mat[4][4],
				      reconstructed_camera_imat[4][4];
				float (*vertex_obmat)[4];

				copy_v3_v3(ray_start_local, ray_start);
				copy_v3_v3(ray_normal_local, ray_normal);

				if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
					BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object,
					                                                  CFRA, reconstructed_camera_mat);

					invert_m4_m4(reconstructed_camera_imat, reconstructed_camera_mat);
				}

				for (track = tracksbase->first; track; track = track->next) {
					float bundle_pos[3];

					if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
						continue;
					}

					copy_v3_v3(bundle_pos, track->bundle_pos);
					if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
						mul_m4_v3(orig_camera_imat, ray_start_local);
						mul_mat3_m4_v3(orig_camera_imat, ray_normal_local);
						vertex_obmat = orig_camera_mat;
					}
					else {
						mul_m4_v3(reconstructed_camera_imat, bundle_pos);
						mul_m4_v3(imat, ray_start_local);
						mul_mat3_m4_v3(imat, ray_normal_local);
						vertex_obmat = obmat;
					}

					retval |= snapVertex(
					        ar, bundle_pos, NULL, vertex_obmat, NULL, mval, dist_px,
					        ray_start, ray_start_local, ray_normal_local, ray_depth,
					        r_loc, NULL);
				}
			}

			break;
		}
		default:
			break;
	}

	return retval;
}

static int dm_looptri_to_poly_index(DerivedMesh *dm, const MLoopTri *lt)
{
	const int *index_mp_to_orig = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	return index_mp_to_orig ? index_mp_to_orig[lt->poly] : lt->poly;
}

static bool snapDerivedMesh(
        SnapObjectContext *sctx,
        Object *ob, DerivedMesh *dm, float obmat[4][4],
        const float mval[2], float *dist_px, const short snap_to, bool do_bb,
        const float ray_start[3], const float ray_normal[3], const float ray_origin[3], float *ray_depth,
        float r_loc[3], float r_no[3], int *r_index)
{
	ARegion *ar = sctx->v3d_data.ar;
	bool retval = false;
	int totvert = dm->getNumVerts(dm);

	if (totvert > 0) {
		const bool do_ray_start_correction = (
		         ELEM(snap_to, SCE_SNAP_MODE_FACE, SCE_SNAP_MODE_VERTEX) &&
		         (sctx->use_v3d && !((RegionView3D *)sctx->v3d_data.ar->regiondata)->is_persp));
		bool need_ray_start_correction_init = do_ray_start_correction;

		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_start_local[3], ray_normal_local[3];
		float local_scale, local_depth, len_diff;

		invert_m4_m4(imat, obmat);
		transpose_m3_m4(timat, imat);

		copy_v3_v3(ray_start_local, ray_start);
		copy_v3_v3(ray_normal_local, ray_normal);

		mul_m4_v3(imat, ray_start_local);
		mul_mat3_m4_v3(imat, ray_normal_local);

		/* local scale in normal direction */
		local_scale = normalize_v3(ray_normal_local);
		local_depth = *ray_depth;
		if (local_depth != BVH_RAYCAST_DIST_MAX) {
			local_depth *= local_scale;
		}

		if (do_bb) {
			BoundBox *bb = BKE_object_boundbox_get(ob);

			if (bb) {
				BoundBox bb_temp;

				/* We cannot aford a bbox with some null dimension, which may happen in some cases...
				 * Threshold is rather high, but seems to be needed to get good behavior, see T46099. */
				bb = BKE_boundbox_ensure_minimum_dimensions(bb, &bb_temp, 1e-1f);

				/* Exact value here is arbitrary (ideally we would scale in pixel-space based on 'dist_px'),
				 * scale up so we can snap against verts & edges on the boundbox, see T46816. */
				if (ELEM(snap_to, SCE_SNAP_MODE_VERTEX, SCE_SNAP_MODE_EDGE)) {
					BKE_boundbox_scale(&bb_temp, bb, 1.0f + 1e-1f);
					bb = &bb_temp;
				}

				/* was local_depth, see: T47838 */
				len_diff = BVH_RAYCAST_DIST_MAX;

				if (!BKE_boundbox_ray_hit_check(bb, ray_start_local, ray_normal_local, &len_diff)) {
					return retval;
				}
				need_ray_start_correction_init = false;
			}
		}

		SnapObjectData_Mesh *sod = NULL;
		BVHTreeFromMesh *treedata = NULL, treedata_stack;

		if (sctx->flag & SNAP_OBJECT_USE_CACHE) {
			void **sod_p;
			if (BLI_ghash_ensure_p(sctx->cache.object_map, ob, &sod_p)) {
				sod = *sod_p;
			}
			else {
				sod = *sod_p = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
				sod->sd.type = SNAP_MESH;
			}

			int tree_index = -1;
			switch (snap_to) {
				case SCE_SNAP_MODE_FACE:
					tree_index = 1;
					break;
				case SCE_SNAP_MODE_VERTEX:
					tree_index = 0;
					break;
			}
			if (tree_index != -1) {
				if (sod->bvh_trees[tree_index] == NULL) {
					sod->bvh_trees[tree_index] = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*treedata));
				}
				treedata = sod->bvh_trees[tree_index];

				/* the tree is owned by the DM and may have been freed since we last used! */
				if (treedata && treedata->tree) {
					if (BLI_linklist_index(dm->bvhCache, treedata->tree) == -1) {
						free_bvhtree_from_mesh(treedata);

					}
				}
			}
		}
		else {
			if (ELEM(snap_to, SCE_SNAP_MODE_FACE, SCE_SNAP_MODE_VERTEX)) {
				treedata = &treedata_stack;
				memset(treedata, 0, sizeof(*treedata));
			}
		}

		if (treedata && treedata->tree == NULL) {
			switch (snap_to) {
				case SCE_SNAP_MODE_FACE:
					bvhtree_from_mesh_looptri(treedata, dm, 0.0f, 4, 6);
					break;
				case SCE_SNAP_MODE_VERTEX:
					bvhtree_from_mesh_verts(treedata, dm, 0.0f, 2, 6);
					break;
			}
		}

		if (need_ray_start_correction_init) {
			/* We *need* a reasonably valid len_diff in this case.
			 * Use BHVTree to find the closest face from ray_start_local.
			 */
			if (treedata && treedata->tree != NULL) {
				BVHTreeNearest nearest;
				nearest.index = -1;
				nearest.dist_sq = FLT_MAX;
				/* Compute and store result. */
				BLI_bvhtree_find_nearest(
				            treedata->tree, ray_start_local, &nearest, treedata->nearest_callback, treedata);
				if (nearest.index != -1) {
					len_diff = sqrtf(nearest.dist_sq);
				}
			}
		}
		/* Only use closer ray_start in case of ortho view! In perspective one, ray_start may already
		 * been *inside* boundbox, leading to snap failures (see T38409).
		 * Note also ar might be null (see T38435), in this case we assume ray_start is ok!
		 */
		if (do_ray_start_correction) {
			float ray_org_local[3];

			copy_v3_v3(ray_org_local, ray_origin);
			mul_m4_v3(imat, ray_org_local);

			/* We pass a temp ray_start, set from object's boundbox, to avoid precision issues with very far
			 * away ray_start values (as returned in case of ortho view3d), see T38358.
			 */
			len_diff -= local_scale;  /* make temp start point a bit away from bbox hit point. */
			madd_v3_v3v3fl(ray_start_local, ray_org_local, ray_normal_local,
			               len_diff - len_v3v3(ray_start_local, ray_org_local));
			local_depth -= len_diff;
		}
		else {
			len_diff = 0.0f;
		}

		switch (snap_to) {
			case SCE_SNAP_MODE_FACE:
			{
				BVHTreeRayHit hit;

				hit.index = -1;
				hit.dist = local_depth;

				if (treedata->tree &&
				    BLI_bvhtree_ray_cast(treedata->tree, ray_start_local, ray_normal_local, 0.0f,
				                         &hit, treedata->raycast_callback, treedata) != -1)
				{
					hit.dist += len_diff;
					hit.dist /= local_scale;
					if (hit.dist <= *ray_depth) {
						*ray_depth = hit.dist;
						copy_v3_v3(r_loc, hit.co);
						copy_v3_v3(r_no, hit.no);

						/* back to worldspace */
						mul_m4_v3(obmat, r_loc);
						mul_m3_v3(timat, r_no);
						normalize_v3(r_no);

						retval = true;

						if (r_index) {
							*r_index = dm_looptri_to_poly_index(dm, &treedata->looptri[hit.index]);
						}
					}
				}
				break;
			}
			case SCE_SNAP_MODE_VERTEX:
			{
				BVHTreeNearest nearest;

				nearest.index = -1;
				nearest.dist_sq = local_depth * local_depth;
				if (treedata->tree &&
				    BLI_bvhtree_find_nearest_to_ray(
				        treedata->tree, ray_start_local, ray_normal_local,
				        &nearest, NULL, NULL) != -1)
				{
					const MVert *v = &treedata->vert[nearest.index];
					float vno[3];
					normal_short_to_float_v3(vno, v->no);
					retval = snapVertex(
					             ar, v->co, vno, obmat, timat, mval, dist_px,
					             ray_start, ray_start_local, ray_normal_local, ray_depth,
					             r_loc, r_no);
				}
				break;
			}
			case SCE_SNAP_MODE_EDGE:
			{
				MVert *verts = dm->getVertArray(dm);
				MEdge *edges = dm->getEdgeArray(dm);
				int totedge = dm->getNumEdges(dm);

				for (int i = 0; i < totedge; i++) {
					MEdge *e = edges + i;
					retval |= snapEdge(
					        ar, verts[e->v1].co, verts[e->v1].no, verts[e->v2].co, verts[e->v2].no,
					        obmat, timat, mval, dist_px,
					        ray_start, ray_start_local, ray_normal_local, ray_depth,
					        r_loc, r_no);
				}

				break;
			}
		}

		if ((sctx->flag & SNAP_OBJECT_USE_CACHE) == 0) {
			if (treedata) {
				free_bvhtree_from_mesh(treedata);
			}
		}
	}

	return retval;
}


static bool snapEditMesh(
        SnapObjectContext *sctx,
        Object *ob, BMEditMesh *em, float obmat[4][4],
        const float mval[2], float *dist_px, const short snap_to,
        const float ray_start[3], const float ray_normal[3], const float ray_origin[3], float *ray_depth,
        float r_loc[3], float r_no[3], int *r_index)
{
	ARegion *ar = sctx->v3d_data.ar;
	bool retval = false;
	int totvert = em->bm->totvert;

	if (totvert > 0) {
		const bool do_ray_start_correction = (
		        ELEM(snap_to, SCE_SNAP_MODE_FACE, SCE_SNAP_MODE_VERTEX) &&
		        (sctx->use_v3d && !((RegionView3D *)sctx->v3d_data.ar->regiondata)->is_persp));

		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_start_local[3], ray_normal_local[3];
		float local_scale, local_depth, len_diff;

		invert_m4_m4(imat, obmat);
		transpose_m3_m4(timat, imat);

		copy_v3_v3(ray_start_local, ray_start);
		copy_v3_v3(ray_normal_local, ray_normal);

		mul_m4_v3(imat, ray_start_local);
		mul_mat3_m4_v3(imat, ray_normal_local);

		/* local scale in normal direction */
		local_scale = normalize_v3(ray_normal_local);
		local_depth = *ray_depth;
		if (local_depth != BVH_RAYCAST_DIST_MAX) {
			local_depth *= local_scale;
		}

		SnapObjectData_EditMesh *sod = NULL;

		BVHTreeFromEditMesh *treedata = NULL, treedata_stack;

		if (sctx->flag & SNAP_OBJECT_USE_CACHE) {
			void **sod_p;
			if (BLI_ghash_ensure_p(sctx->cache.object_map, ob, &sod_p)) {
				sod = *sod_p;
			}
			else {
				sod = *sod_p = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*sod));
				sod->sd.type = SNAP_EDIT_MESH;
			}

			int tree_index = -1;
			switch (snap_to) {
				case SCE_SNAP_MODE_FACE:
					tree_index = 1;
					break;
				case SCE_SNAP_MODE_VERTEX:
					tree_index = 0;
					break;
			}
			if (tree_index != -1) {
				if (sod->bvh_trees[tree_index] == NULL) {
					sod->bvh_trees[tree_index] = BLI_memarena_calloc(sctx->cache.mem_arena, sizeof(*treedata));
				}
				treedata = sod->bvh_trees[tree_index];
			}
		}
		else {
			if (ELEM(snap_to, SCE_SNAP_MODE_FACE, SCE_SNAP_MODE_VERTEX)) {
				treedata = &treedata_stack;
				memset(treedata, 0, sizeof(*treedata));
			}
		}

		if (treedata && treedata->tree == NULL) {
			switch (snap_to) {
				case SCE_SNAP_MODE_FACE:
				{
					BLI_bitmap *looptri_mask = NULL;
					int looptri_num_active = -1;
					if (sctx->callbacks.edit_mesh.test_face_fn) {
						looptri_mask = BLI_BITMAP_NEW(em->tottri, __func__);
						looptri_num_active = BM_iter_mesh_bitmap_from_filter_tessface(
						        em->bm, looptri_mask,
						        sctx->callbacks.edit_mesh.test_face_fn, sctx->callbacks.edit_mesh.user_data);
					}
					bvhtree_from_editmesh_looptri_ex(treedata, em, looptri_mask, looptri_num_active, 0.0f, 4, 6);
					if (looptri_mask) {
						MEM_freeN(looptri_mask);
					}
					break;
				}
				case SCE_SNAP_MODE_VERTEX:
				{
					BLI_bitmap *verts_mask = NULL;
					int verts_num_active = -1;
					if (sctx->callbacks.edit_mesh.test_vert_fn) {
						verts_mask = BLI_BITMAP_NEW(em->bm->totvert, __func__);
						verts_num_active = BM_iter_mesh_bitmap_from_filter(
						        BM_VERTS_OF_MESH, em->bm, verts_mask,
						        (bool (*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_vert_fn,
						        sctx->callbacks.edit_mesh.user_data);
					}
					bvhtree_from_editmesh_verts_ex(treedata, em, verts_mask, verts_num_active, 0.0f, 2, 6);
					if (verts_mask) {
						MEM_freeN(verts_mask);
					}
					break;
				}
			}
		}

		/* Only use closer ray_start in case of ortho view! In perspective one, ray_start may already
		 * been *inside* boundbox, leading to snap failures (see T38409).
		 * Note also ar might be null (see T38435), in this case we assume ray_start is ok!
		 */
		if (do_ray_start_correction) {
			/* We *need* a reasonably valid len_diff in this case.
			 * Use BHVTree to find the closest face from ray_start_local.
			 */
			if (treedata && treedata->tree != NULL) {
				BVHTreeNearest nearest;
				nearest.index = -1;
				nearest.dist_sq = FLT_MAX;
				/* Compute and store result. */
				BLI_bvhtree_find_nearest(
				        treedata->tree, ray_start_local, &nearest, treedata->nearest_callback, treedata);
				if (nearest.index != -1) {
					len_diff = sqrtf(nearest.dist_sq);
				}
			}
			float ray_org_local[3];

			copy_v3_v3(ray_org_local, ray_origin);
			mul_m4_v3(imat, ray_org_local);

			/* We pass a temp ray_start, set from object's boundbox, to avoid precision issues with very far
			 * away ray_start values (as returned in case of ortho view3d), see T38358.
			 */
			len_diff -= local_scale;  /* make temp start point a bit away from bbox hit point. */
			madd_v3_v3v3fl(ray_start_local, ray_org_local, ray_normal_local,
				len_diff - len_v3v3(ray_start_local, ray_org_local));
			local_depth -= len_diff;
		}
		else {
			len_diff = 0.0f;
		}

		switch (snap_to) {
			case SCE_SNAP_MODE_FACE:
			{
				BVHTreeRayHit hit;

				hit.index = -1;
				hit.dist = local_depth;

				if (treedata->tree &&
				    BLI_bvhtree_ray_cast(
				        treedata->tree, ray_start_local, ray_normal_local, 0.0f,
				        &hit, treedata->raycast_callback, treedata) != -1)
				{
					hit.dist += len_diff;
					hit.dist /= local_scale;
					if (hit.dist <= *ray_depth) {
						*ray_depth = hit.dist;
						copy_v3_v3(r_loc, hit.co);
						copy_v3_v3(r_no, hit.no);

						/* back to worldspace */
						mul_m4_v3(obmat, r_loc);
						mul_m3_v3(timat, r_no);
						normalize_v3(r_no);

						retval = true;

						if (r_index) {
							*r_index = hit.index;
						}
					}
				}
				break;
			}
			case SCE_SNAP_MODE_VERTEX:
			{
				BVHTreeNearest nearest;

				nearest.index = -1;
				nearest.dist_sq = local_depth * local_depth;
				if (treedata->tree &&
				    BLI_bvhtree_find_nearest_to_ray(
				            treedata->tree, ray_start_local, ray_normal_local,
				            &nearest, NULL, NULL) != -1)
				{
					const BMVert *v = BM_vert_at_index(em->bm, nearest.index);
					retval = snapVertex(
					        ar, v->co, v->no, obmat, timat, mval, dist_px,
					        ray_start, ray_start_local, ray_normal_local, ray_depth,
					        r_loc, r_no);
				}
				break;
			}
			case SCE_SNAP_MODE_EDGE:
			{
				BM_mesh_elem_table_ensure(em->bm, BM_EDGE);
				int totedge = em->bm->totedge;
				for (int i = 0; i < totedge; i++) {
					BMEdge *eed = BM_edge_at_index(em->bm, i);

					if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) &&
					    !BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) &&
					    !BM_elem_flag_test(eed->v2, BM_ELEM_SELECT))
					{
						short v1no[3], v2no[3];
						normal_float_to_short_v3(v1no, eed->v1->no);
						normal_float_to_short_v3(v2no, eed->v2->no);
						retval |= snapEdge(
						        ar, eed->v1->co, v1no, eed->v2->co, v2no,
						        obmat, timat, mval, dist_px,
						        ray_start, ray_start_local, ray_normal_local, ray_depth,
						        r_loc, r_no);
					}
				}

				break;
			}
		}

		if ((sctx->flag & SNAP_OBJECT_USE_CACHE) == 0) {
			if (treedata) {
				free_bvhtree_from_editmesh(treedata);
			}
		}
	}

	return retval;
}

static bool snapObject(
        SnapObjectContext *sctx,
        Object *ob, float obmat[4][4], bool use_obedit, const short snap_to,
        const float mval[2], float *dist_px,
        const float ray_start[3], const float ray_normal[3], const float ray_origin[3], float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	ARegion *ar = sctx->v3d_data.ar;
	bool retval = false;

	if (ob->type == OB_MESH) {
		BMEditMesh *em;

		if (use_obedit) {
			em = BKE_editmesh_from_object(ob);
			retval = snapEditMesh(
			        sctx, ob, em, obmat, mval, dist_px, snap_to,
			        ray_start, ray_normal, ray_origin, ray_depth,
			        r_loc, r_no, r_index);
		}
		else {
			/* in this case we want the mesh from the editmesh, avoids stale data. see: T45978.
			 * still set the 'em' to NULL, since we only want the 'dm'. */
			DerivedMesh *dm;
			em = BKE_editmesh_from_object(ob);
			if (em) {
				editbmesh_get_derived_cage_and_final(sctx->scene, ob, em, CD_MASK_BAREMESH, &dm);
			}
			else {
				dm = mesh_get_derived_final(sctx->scene, ob, CD_MASK_BAREMESH);
			}
			retval = snapDerivedMesh(
			        sctx, ob, dm, obmat, mval, dist_px, snap_to, true,
			        ray_start, ray_normal, ray_origin, ray_depth,
			        r_loc, r_no, r_index);

			dm->release(dm);
		}
	}
	else if (ob->type == OB_ARMATURE) {
		retval = snapArmature(
		        ar, ob, ob->data, obmat, mval, dist_px, snap_to,
		        ray_start, ray_normal, ray_depth,
		        r_loc, r_no);
	}
	else if (ob->type == OB_CURVE) {
		retval = snapCurve(
		        ar, ob, ob->data, obmat, mval, dist_px, snap_to,
		        ray_start, ray_normal, ray_depth,
		        r_loc, r_no);
	}
	else if (ob->type == OB_EMPTY) {
		retval = snapEmpty(
		        ar, ob, obmat, mval, dist_px, snap_to,
		        ray_start, ray_normal, ray_depth,
		        r_loc, r_no);
	}
	else if (ob->type == OB_CAMERA) {
		retval = snapCamera(
		        ar, sctx->scene, ob, obmat, mval, dist_px, snap_to,
		        ray_start, ray_normal, ray_depth,
		        r_loc, r_no);
	}

	if (retval) {
		if (r_ob) {
			*r_ob = ob;
			copy_m4_m4(r_obmat, obmat);
		}
	}

	return retval;
}

static bool snapObjectsRay(
        SnapObjectContext *sctx,
        SnapSelect snap_select, const short snap_to,
        const float mval[2], float *dist_px,
        /* special handling of active and edit objects */
        Base *base_act, Object *obedit,
        const float ray_start[3], const float ray_normal[3], const float ray_origin[3], float *ray_depth,
        /* return args */
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	Base *base;
	bool retval = false;

	if (snap_select == SNAP_ALL && obedit) {
		Object *ob = obedit;

		retval |= snapObject(
		        sctx, ob, ob->obmat, true, snap_to,
		        mval, dist_px,
		        ray_start, ray_normal, ray_origin, ray_depth,
		        r_loc, r_no, r_index, r_ob, r_obmat);
	}

	/* Need an exception for particle edit because the base is flagged with BA_HAS_RECALC_DATA
	 * which makes the loop skip it, even the derived mesh will never change
	 *
	 * To solve that problem, we do it first as an exception.
	 * */
	base = base_act;
	if (base && base->object && base->object->mode & OB_MODE_PARTICLE_EDIT) {
		Object *ob = base->object;
		retval |= snapObject(
		        sctx, ob, ob->obmat, false, snap_to,
		        mval, dist_px,
		        ray_start, ray_normal, ray_origin, ray_depth,
		        r_loc, r_no, r_index, r_ob, r_obmat);
	}

	for (base = sctx->scene->base.first; base != NULL; base = base->next) {
		if ((BASE_VISIBLE_BGMODE(sctx->v3d_data.v3d, sctx->scene, base)) &&
		    (base->flag & (BA_HAS_RECALC_OB | BA_HAS_RECALC_DATA)) == 0 &&

		    ((snap_select == SNAP_NOT_SELECTED && (base->flag & (SELECT | BA_WAS_SEL)) == 0) ||
		     (ELEM(snap_select, SNAP_ALL, SNAP_NOT_OBEDIT) && base != base_act)))
		{
			Object *ob = base->object;
			Object *ob_snap = ob;
			bool use_obedit = false;

			/* for linked objects, use the same object but a different matrix */
			if (obedit && ob->data == obedit->data) {
				use_obedit = true;
				ob_snap = obedit;
			}

			if (ob->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(sctx->bmain->eval_ctx, sctx->scene, ob);

				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					bool use_obedit_dupli = (obedit && dupli_ob->ob->data == obedit->data);
					Object *dupli_snap = (use_obedit_dupli) ? obedit : dupli_ob->ob;

					retval |= snapObject(
					        sctx, dupli_snap, dupli_ob->mat, use_obedit_dupli, snap_to,
					        mval, dist_px,
					        ray_start, ray_normal, ray_origin, ray_depth,
					        r_loc, r_no, r_index, r_ob, r_obmat);
				}

				free_object_duplilist(lb);
			}

			retval |= snapObject(
			        sctx, ob_snap, ob->obmat, use_obedit, snap_to,
			        mval, dist_px,
			        ray_start, ray_normal, ray_origin, ray_depth,
			        r_loc, r_no, r_index, r_ob, r_obmat);
		}
	}

	return retval;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Public Object Snapping API
 * \{ */

SnapObjectContext *ED_transform_snap_object_context_create(
        Main *bmain, Scene *scene, int flag)
{
	SnapObjectContext *sctx = MEM_callocN(sizeof(*sctx), __func__);

	sctx->flag = flag;

	sctx->bmain = bmain;
	sctx->scene = scene;

	return sctx;
}

SnapObjectContext *ED_transform_snap_object_context_create_view3d(
        Main *bmain, Scene *scene, int flag,
        /* extra args for view3d */
        ARegion *ar, View3D *v3d)
{
	SnapObjectContext *sctx = ED_transform_snap_object_context_create(bmain, scene, flag);

	sctx->use_v3d = true;
	sctx->v3d_data.ar = ar;
	sctx->v3d_data.v3d = v3d;

	if (sctx->flag & SNAP_OBJECT_USE_CACHE) {
		sctx->cache.object_map = BLI_ghash_ptr_new(__func__);
		sctx->cache.mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
	}

	return sctx;
}

static void snap_object_data_free(void *sod_v)
{
	switch (((SnapObjectData *)sod_v)->type) {
		case SNAP_MESH:
		{
			SnapObjectData_Mesh *sod = sod_v;
			for (int i = 0; i < ARRAY_SIZE(sod->bvh_trees); i++) {
				if (sod->bvh_trees[i]) {
					free_bvhtree_from_mesh(sod->bvh_trees[i]);
				}
			}
			break;
		}
		case SNAP_EDIT_MESH:
		{
			SnapObjectData_EditMesh *sod = sod_v;
			for (int i = 0; i < ARRAY_SIZE(sod->bvh_trees); i++) {
				if (sod->bvh_trees[i]) {
					free_bvhtree_from_editmesh(sod->bvh_trees[i]);
				}
			}
			break;
		}
	}
}

void ED_transform_snap_object_context_destroy(SnapObjectContext *sctx)
{
	if (sctx->flag & SNAP_OBJECT_USE_CACHE) {
		BLI_ghash_free(sctx->cache.object_map, NULL, snap_object_data_free);
		BLI_memarena_free(sctx->cache.mem_arena);
	}

	MEM_freeN(sctx);
}

void ED_transform_snap_object_context_set_editmesh_callbacks(
        SnapObjectContext *sctx,
        bool (*test_vert_fn)(BMVert *, void *user_data),
        bool (*test_edge_fn)(BMEdge *, void *user_data),
        bool (*test_face_fn)(BMFace *, void *user_data),
        void *user_data)
{
	sctx->callbacks.edit_mesh.test_vert_fn = test_vert_fn;
	sctx->callbacks.edit_mesh.test_edge_fn = test_edge_fn;
	sctx->callbacks.edit_mesh.test_face_fn = test_face_fn;

	sctx->callbacks.edit_mesh.user_data = user_data;
}

bool ED_transform_snap_object_project_ray_ex(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float ray_start[3], const float ray_normal[3], float *ray_depth,
        float r_loc[3], float r_no[3], int *r_index,
        Object **r_ob, float r_obmat[4][4])
{
	Base *base_act = params->use_object_active ? sctx->scene->basact : NULL;
	Object *obedit = params->use_object_edit ? sctx->scene->obedit : NULL;

	return snapObjectsRay(
	        sctx,
	        params->snap_select, params->snap_to,
	        NULL, NULL,
	        base_act, obedit,
	        ray_start, ray_normal, ray_start, ray_depth,
	        r_loc, r_no, r_index,
	        r_ob, r_obmat);
}

/**
 * Convenience function for snap ray-casting.
 *
 * Given a ray, cast it into the scene (snapping to faces).
 *
 * \return Snap success
 */
static bool transform_snap_context_project_ray_impl(
        SnapObjectContext *sctx,
        const float ray_start[3], const float ray_normal[3], float *ray_dist,
        float r_co[3], float r_no[3])
{
	bool ret;

	/* try snap edge, then face if it fails */
	ret = ED_transform_snap_object_project_ray_ex(
	        sctx,
	        &(const struct SnapObjectParams){
	            .snap_select = SNAP_ALL,
	            .snap_to = SCE_SNAP_MODE_FACE,
	            .use_object_edit = (sctx->scene->obedit != NULL),
	        },
	        ray_start, ray_normal, ray_dist,
	        r_co, r_no, NULL,
	        NULL, NULL);

	return ret;
}

bool ED_transform_snap_object_project_ray(
        SnapObjectContext *sctx,
        const float ray_origin[3], const float ray_direction[3], float *ray_dist,
        float r_co[3], float r_no[3])
{
	float ray_dist_fallback;
	if (ray_dist == NULL) {
		ray_dist_fallback = BVH_RAYCAST_DIST_MAX;
		ray_dist = &ray_dist_fallback;
	}

	float no_fallback[3];
	if (r_no == NULL) {
		r_no = no_fallback;
	}

	return transform_snap_context_project_ray_impl(
	        sctx,
	        ray_origin, ray_direction, ray_dist,
	        r_co, r_no);
}

static bool transform_snap_context_project_view3d_mixed_impl(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        bool use_depth,
        float r_co[3], float r_no[3])
{
	float ray_dist = BVH_RAYCAST_DIST_MAX;
	bool is_hit = false;

	float r_no_dummy[3];
	if (r_no == NULL) {
		r_no = r_no_dummy;
	}

	const int  elem_type[3] = {SCE_SNAP_MODE_VERTEX, SCE_SNAP_MODE_EDGE, SCE_SNAP_MODE_FACE};

	BLI_assert(params->snap_to_flag != 0);
	BLI_assert((params->snap_to_flag & ~(1 | 2 | 4)) == 0);

	struct SnapObjectParams params_temp = *params;

	for (int i = 0; i < 3; i++) {
		if ((params->snap_to_flag & (1 << i)) && (is_hit == false || use_depth)) {
			if (use_depth == false) {
				ray_dist = BVH_RAYCAST_DIST_MAX;
			}

			params_temp.snap_to = elem_type[i];

			if (ED_transform_snap_object_project_view3d(
			        sctx,
			        &params_temp,
			        mval, dist_px, &ray_dist,
			        r_co, r_no))
			{
				is_hit = true;
			}
		}
	}

	return is_hit;
}

/**
 * Convenience function for performing snapping.
 *
 * Given a 2D region value, snap to vert/edge/face.
 *
 * \param sctx: Snap context.
 * \param mval: Screenspace coordinate.
 * \param dist_px: Maximum distance to snap (in pixels).
 * \param use_depth: Snap to the closest element, use when using more than one snap type.
 * \param r_co: hit location.
 * \param r_no: hit normal (optional).
 * \return Snap success
 */
bool ED_transform_snap_object_project_view3d_mixed(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval_fl[2], float *dist_px,
        bool use_depth,
        float r_co[3], float r_no[3])
{
	return transform_snap_context_project_view3d_mixed_impl(
	        sctx,
	        params,
	        mval_fl, dist_px, use_depth,
	        r_co, r_no);
}

bool ED_transform_snap_object_project_view3d_ex(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float *ray_depth,
        float r_loc[3], float r_no[3], int *r_index)
{
	float ray_start[3], ray_normal[3], ray_orgigin[3];

	if (!ED_view3d_win_to_ray_ex(
	        sctx->v3d_data.ar, sctx->v3d_data.v3d,
	        mval, ray_orgigin, ray_normal, ray_start, true))
	{
		return false;
	}

	Base *base_act = params->use_object_active ? sctx->scene->basact : NULL;
	Object *obedit = params->use_object_edit ? sctx->scene->obedit : NULL;
	return snapObjectsRay(
	        sctx,
	        params->snap_select, params->snap_to,
	        mval, dist_px,
	        base_act, obedit,
	        ray_start, ray_normal, ray_orgigin, ray_depth,
	        r_loc, r_no, r_index, NULL, NULL);
}

bool ED_transform_snap_object_project_view3d(
        SnapObjectContext *sctx,
        const struct SnapObjectParams *params,
        const float mval[2], float *dist_px,
        float *ray_depth,
        float r_loc[3], float r_no[3])
{
	return ED_transform_snap_object_project_view3d_ex(
	        sctx,
	        params,
	        mval, dist_px,
	        ray_depth,
	        r_loc, r_no, NULL);
}

/** \} */
