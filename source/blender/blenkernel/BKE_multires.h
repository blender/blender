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
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#ifndef __BKE_MULTIRES_H__
#define __BKE_MULTIRES_H__

/** \file BKE_multires.h
 *  \ingroup bke
 */

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
struct MultiresModifierData *get_multires_modifier(struct Scene *scene, struct Object *ob, int use_first);
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

/*switch mdisp data in dm between tangent and object space*/
enum {
	MULTIRES_SPACE_TANGENT,
	MULTIRES_SPACE_OBJECT,
	MULTIRES_SPACE_ABSOLUTE,
};
void multires_set_space(struct DerivedMesh *dm, struct Object *ob, int from, int to);

/* Related to the old multires */
void multires_free(struct Multires *mr);
void multires_load_old(struct Object *ob, struct Mesh *me);
void multires_load_old_250(struct Mesh *);

void multiresModifier_scale_disp(struct Scene *scene, struct Object *ob);
void multiresModifier_prepare_join(struct Scene *scene, struct Object *ob, struct Object *to_ob);

int multires_mdisp_corners(struct MDisps *s);

/* update multires data after topology changing */
void multires_topology_changed(struct Scene *scene, struct Object *ob);

/**** interpolation stuff ****/
void old_mdisps_bilinear(float out[3], float (*disps)[3], const int st, float u, float v);
int mdisp_rot_face_to_crn(const int corners, const int face_side, const float u, const float v, float *x, float *y);

#endif // __BKE_MULTIRES_H__

