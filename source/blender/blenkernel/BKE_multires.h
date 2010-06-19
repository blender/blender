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

void multires_mark_as_modified(struct Object *ob);

void multires_force_update(struct Object *ob);
void multires_force_render_update(struct Object *ob);
void multires_force_external_reload(struct Object *ob);

struct DerivedMesh *multires_dm_create_from_derived(struct MultiresModifierData*,
	int local_mmd, struct DerivedMesh*, struct Object *, int, int);

struct MultiresModifierData *find_multires_modifier(struct Scene *scene, struct Object *ob);
struct DerivedMesh *get_multires_dm(struct Scene *scene, struct MultiresModifierData *mmd,
				struct Object *ob);
void multiresModifier_join(struct Object *);
void multiresModifier_del_levels(struct MultiresModifierData *, struct Object *, int direction);
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

#endif

