/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

/* so modifier types match their defines */
#include "MOD_modifiertypes.h"

#include "DEG_depsgraph_build.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MDeformVert;
struct Mesh;
struct ModifierData;
struct ModifierEvalContext;
struct Object;

void MOD_init_texture(struct MappingInfoModifierData *dmd, const struct ModifierEvalContext *ctx);
/**
 * \param cos: may be NULL, in which case we use directly mesh vertices' coordinates.
 */
void MOD_get_texture_coords(struct MappingInfoModifierData *dmd,
                            const struct ModifierEvalContext *ctx,
                            struct Object *ob,
                            struct Mesh *mesh,
                            float (*cos)[3],
                            float (*r_texco)[3]);

void MOD_previous_vcos_store(struct ModifierData *md, const float (*vert_coords)[3]);

/**
 * \returns a mesh if mesh == NULL, for deforming modifiers that need it.
 */
struct Mesh *MOD_deform_mesh_eval_get(struct Object *ob,
                                      struct BMEditMesh *em,
                                      struct Mesh *mesh,
                                      const float (*vertexCos)[3],
                                      int verts_num,
                                      bool use_orco);

void MOD_get_vgroup(struct Object *ob,
                    struct Mesh *mesh,
                    const char *name,
                    const struct MDeformVert **dvert,
                    int *defgrp_index);

void MOD_depsgraph_update_object_bone_relation(struct DepsNodeHandle *node,
                                               struct Object *object,
                                               const char *bonename,
                                               const char *description);

#ifdef __cplusplus
}
#endif
