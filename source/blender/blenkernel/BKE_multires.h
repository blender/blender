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
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#ifndef BKE_MULTIRES_H
#define BKE_MULTIRES_H

struct DerivedMesh;
struct Mesh;
struct MFace;
struct Multires;
struct MultiresModifierData;
struct ModifierData;
struct Object;
struct Scene;
struct MDisps;

void multires_mark_as_modified(struct Object *ob);

void multires_force_update(struct Object *ob);
void multires_force_render_update(struct Object *ob);
void multires_force_external_reload(struct Object *ob);

void multiresModifier_set_levels_from_disps(struct MultiresModifierData *mmd, struct Object *ob);

struct DerivedMesh *multires_dm_create_from_derived(struct MultiresModifierData*,
	int local_mmd, struct DerivedMesh*, struct Object *, int, int);

struct MultiresModifierData *find_multires_modifier_before(struct Scene *scene,
	struct ModifierData *lastmd);
struct DerivedMesh *get_multires_dm(struct Scene *scene, struct MultiresModifierData *mmd,
				struct Object *ob);
void multiresModifier_del_levels(struct MultiresModifierData *, struct Object *, int direction);
void multiresModifier_base_apply(struct MultiresModifierData *mmd, struct Object *ob);
void multiresModifier_subdivide(struct MultiresModifierData *mmd, struct Object *ob,
				int updateblock, int simple);
int multiresModifier_reshape(struct Scene *scene, struct MultiresModifierData *mmd,
				struct Object *dst, struct Object *src);
int multiresModifier_reshapeFromDM(struct Scene *scene, struct MultiresModifierData *mmd,
				struct Object *ob, struct DerivedMesh *srcdm);
int multiresModifier_reshapeFromDeformMod(struct Scene *scene, struct MultiresModifierData *mmd,
				struct Object *ob, struct ModifierData *md);

void multires_stitch_grids(struct Object *);

/* Related to the old multires */
void multires_free(struct Multires *mr);
void multires_load_old(struct Object *ob, struct Mesh *me);
void multires_load_old_250(struct Mesh *);

void multiresModifier_scale_disp(struct Scene *scene, struct Object *ob);
void multiresModifier_prepare_join(struct Scene *scene, struct Object *ob, struct Object *to_ob);

int multires_mdisp_corners(struct MDisps *s);
void multires_mdisp_smooth_bounds(struct MDisps *disps);

/* update multires data after topology changing */
void multires_topology_changed(struct Object *ob);

/**** interpolation stuff ****/
void old_mdisps_bilinear(float out[3], float (*disps)[3], int st, float u, float v);
void mdisp_rot_crn_to_face(int S, int corners, int face_side, float x, float y, float *u, float *v);
int mdisp_rot_face_to_crn(int corners, int face_side, float u, float v, float *x, float *y);
void mdisp_apply_weight(int S, int corners, int x, int y, int face_side, float crn_weight[4][2], float *u_r, float *v_r);
void mdisp_flip_disp(int S, int corners, float axis_x[2], float axis_y[2], float disp[3]);
void mdisp_join_tris(struct MDisps *dst, struct MDisps *tri1, struct MDisps *tri2);

#endif

