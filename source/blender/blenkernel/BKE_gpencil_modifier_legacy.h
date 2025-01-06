/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_gpencil_modifier_types.h" /* Needed for all enum type definitions. */

#include "BKE_lib_query.hh" /* For LibraryForeachIDCallbackFlag enum. */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct GpencilModifierData;
struct ID;
struct ListBase;
struct Object;
/* NOTE: bake_modifier() called from UI:
 * needs to create new data-blocks, hence the need for this. */

typedef void (*GreasePencilIDWalkFunc)(void *user_data,
                                       struct Object *ob,
                                       struct ID **idpoin,
                                       LibraryForeachIDCallbackFlag cb_flag);

/**
 * Free grease pencil modifier data
 * \param md: Modifier data.
 * \param flag: Flags.
 */
void BKE_gpencil_modifier_free_ex(struct GpencilModifierData *md, int flag);
/**
 * Free grease pencil modifier data
 * \param md: Modifier data.
 */
void BKE_gpencil_modifier_free(struct GpencilModifierData *md);

/**
 * Link grease pencil modifier related IDs.
 * \param ob: Grease pencil object.
 * \param walk: Walk option.
 * \param user_data: User data.
 */
void BKE_gpencil_modifiers_foreach_ID_link(struct Object *ob,
                                           GreasePencilIDWalkFunc walk,
                                           void *user_data);

void BKE_gpencil_modifier_blend_read_data(struct BlendDataReader *reader,
                                          struct ListBase *lb,
                                          struct Object *ob);

#ifdef __cplusplus
}
#endif
