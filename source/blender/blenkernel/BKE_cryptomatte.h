/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"
#include "DNA_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations. */
struct CryptomatteSession;
struct Material;
struct Object;
struct RenderResult;
struct Scene;

struct CryptomatteSession *BKE_cryptomatte_init(void);
struct CryptomatteSession *BKE_cryptomatte_init_from_render_result(
    const struct RenderResult *render_result);
struct CryptomatteSession *BKE_cryptomatte_init_from_scene(const struct Scene *scene);
struct CryptomatteSession *BKE_cryptomatte_init_from_view_layer(
    const struct ViewLayer *view_layer);
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
/**
 * Find an ID in the given main that matches the given encoded float.
 */
bool BKE_cryptomatte_find_name(const struct CryptomatteSession *session,
                               float encoded_hash,
                               char *r_name,
                               int name_maxncpy);

char *BKE_cryptomatte_entries_to_matte_id(struct NodeCryptomatte *node_storage);
void BKE_cryptomatte_matte_id_to_entries(struct NodeCryptomatte *node_storage,
                                         const char *matte_id);

void BKE_cryptomatte_store_metadata(const struct CryptomatteSession *session,
                                    struct RenderResult *render_result,
                                    const ViewLayer *view_layer);

#ifdef __cplusplus
}
#endif
