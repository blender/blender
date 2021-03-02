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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"
#include "DNA_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CryptomatteSession;
struct ID;
struct Main;
struct Material;
struct Object;
struct RenderResult;

struct CryptomatteSession *BKE_cryptomatte_init(void);
struct CryptomatteSession *BKE_cryptomatte_init_from_render_result(
    const struct RenderResult *render_result);
void BKE_cryptomatte_free(struct CryptomatteSession *session);
void BKE_cryptomatte_add_layer(struct CryptomatteSession *session, const char *layer_name);

uint32_t BKE_cryptomatte_hash(const char *name, int name_len);
uint32_t BKE_cryptomatte_object_hash(struct CryptomatteSession *session,
                                     const char *layer_name,
                                     const struct Object *object);
uint32_t BKE_cryptomatte_material_hash(struct CryptomatteSession *session,
                                       const char *layer_name,
                                       const struct Material *material);
uint32_t BKE_cryptomatte_asset_hash(struct CryptomatteSession *session,
                                    const char *layer_name,
                                    const struct Object *object);
float BKE_cryptomatte_hash_to_float(uint32_t cryptomatte_hash);

char *BKE_cryptomatte_entries_to_matte_id(struct NodeCryptomatte *node_storage);
void BKE_cryptomatte_matte_id_to_entries(struct NodeCryptomatte *node_storage,
                                         const char *matte_id);

void BKE_cryptomatte_store_metadata(const struct CryptomatteSession *session,
                                    struct RenderResult *render_result,
                                    const ViewLayer *view_layer);

#ifdef __cplusplus
}
#endif
