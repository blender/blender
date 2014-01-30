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
 * Contributor(s): Ben Batt
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_util.h
 *  \ingroup modifiers
 */


#ifndef __MOD_UTIL_H__
#define __MOD_UTIL_H__

/* so modifier types match their defines */
#include "MOD_modifiertypes.h"

struct CustomData;
struct DerivedMesh;
struct MDeformVert;
struct ModifierData;
struct Object;
struct Scene;
struct Tex;
struct TexResult;

void modifier_init_texture(struct Scene *scene, struct Tex *texture);
void get_texture_coords(struct MappingInfoModifierData *dmd, struct Object *ob, struct DerivedMesh *dm,
                        float (*co)[3], float (*texco)[3], int numVerts);
void modifier_vgroup_cache(struct ModifierData *md, float (*vertexCos)[3]);
struct DerivedMesh *get_cddm(struct Object *ob, struct BMEditMesh *em, struct DerivedMesh *dm,
                             float (*vertexCos)[3], bool use_normals);
struct DerivedMesh *get_dm(struct Object *ob, struct BMEditMesh *em, struct DerivedMesh *dm,
                           float (*vertexCos)[3], bool use_normals, bool use_orco);
struct DerivedMesh *get_dm_for_modifier(struct Object *ob, ModifierApplyFlag flag);
void modifier_get_vgroup(struct Object *ob, struct DerivedMesh *dm,
                         const char *name, struct MDeformVert **dvert, int *defgrp_index);

/* XXX workaround for non-threadsafe context in OpenNL (T38403)
 * OpenNL uses global pointer for "current context", which causes
 * conflict when multiple modifiers get evaluated in threaded depgraph.
 * This is just a stupid hack to prevent assert failure / crash,
 * otherwise we'd have to modify OpenNL on a large scale.
 * OpenNL should be replaced eventually, there are other options (eigen, ceres).
 * - lukas_t
 */
#ifdef WITH_OPENNL
#define OPENNL_THREADING_HACK
#endif

#ifdef OPENNL_THREADING_HACK
void modifier_opennl_lock(void);
void modifier_opennl_unlock(void);
#endif

#endif /* __MOD_UTIL_H__ */
