/*
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
 */

/** \file
 * \ingroup modifiers
 */

#ifndef __MOD_UTIL_H__
#define __MOD_UTIL_H__

/* so modifier types match their defines */
#include "MOD_modifiertypes.h"

#include "DEG_depsgraph_build.h"

struct MDeformVert;
struct Mesh;
struct ModifierData;
struct ModifierEvalContext;
struct Object;

void MOD_init_texture(struct MappingInfoModifierData *dmd, const struct ModifierEvalContext *ctx);
void MOD_get_texture_coords(struct MappingInfoModifierData *dmd,
                            const struct ModifierEvalContext *ctx,
                            struct Object *ob,
                            struct Mesh *mesh,
                            float (*cos)[3],
                            float (*r_texco)[3]);

void MOD_previous_vcos_store(struct ModifierData *md, const float (*vertexCos)[3]);

struct Mesh *MOD_deform_mesh_eval_get(struct Object *ob,
                                      struct BMEditMesh *em,
                                      struct Mesh *mesh,
                                      const float (*vertexCos)[3],
                                      const int num_verts,
                                      const bool use_normals,
                                      const bool use_orco);

void MOD_get_vgroup(struct Object *ob,
                    struct Mesh *mesh,
                    const char *name,
                    struct MDeformVert **dvert,
                    int *defgrp_index);

void MOD_depsgraph_update_object_bone_relation(struct DepsNodeHandle *node,
                                               struct Object *object,
                                               const char *bonename,
                                               const char *description);
#endif /* __MOD_UTIL_H__ */
