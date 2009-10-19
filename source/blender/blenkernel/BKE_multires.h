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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

struct DerivedMesh;
struct Mesh;
struct MFace;
struct MultiresModifierData;
struct Object;

typedef struct MultiresSubsurf {
	struct MultiresModifierData *mmd;
	struct Object *ob;
	int local_mmd;
} MultiresSubsurf;

/* MultiresDM */
struct Object *MultiresDM_get_object(struct DerivedMesh *dm);
struct Mesh *MultiresDM_get_mesh(struct DerivedMesh *dm);
struct DerivedMesh *MultiresDM_new(struct MultiresSubsurf *, struct DerivedMesh*, int, int, int);
void *MultiresDM_get_vertnorm(struct DerivedMesh *);
void *MultiresDM_get_orco(struct DerivedMesh *);
struct MVert *MultiresDM_get_subco(struct DerivedMesh *);
struct ListBase *MultiresDM_get_vert_face_map(struct DerivedMesh *);
struct ListBase *MultiresDM_get_vert_edge_map(struct DerivedMesh *);
int *MultiresDM_get_face_offsets(struct DerivedMesh *);
int MultiresDM_get_totlvl(struct DerivedMesh *);
int MultiresDM_get_lvl(struct DerivedMesh *);
void MultiresDM_set_update(struct DerivedMesh *, void (*)(struct DerivedMesh*));

/* The displacements will only be updated when
   the MultiresDM has been marked as modified */
void MultiresDM_mark_as_modified(struct DerivedMesh *);
void multires_mark_as_modified(struct Object *ob);

void multires_force_update(struct Object *ob);

struct DerivedMesh *multires_dm_create_from_derived(struct MultiresModifierData*, int local_mmd, struct DerivedMesh*,
						    struct Object *, int, int);

struct MultiresModifierData *find_multires_modifier(struct Object *ob);
int multiresModifier_switch_level(struct Object *, const int);
void multiresModifier_join(struct Object *);
void multiresModifier_del_levels(struct MultiresModifierData *, struct Object *, int direction);
void multiresModifier_subdivide(struct MultiresModifierData *mmd, struct Object *ob, int distance,
				int updateblock, int simple);
int multiresModifier_reshape(struct MultiresModifierData *mmd, struct Object *dst, struct Object *src);

/* Related to the old multires */
struct Multires;
void multires_load_old(struct DerivedMesh *, struct Multires *);
void multires_free(struct Multires*);
