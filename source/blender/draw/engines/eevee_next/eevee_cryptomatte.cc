/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "BKE_cryptomatte.hh"

#include "GPU_material.h"

#include "eevee_cryptomatte.hh"
#include "eevee_instance.hh"
#include "eevee_renderbuffers.hh"

namespace blender::eevee {

void Cryptomatte::begin_sync()
{
  const eViewLayerEEVEEPassType enabled_passes = static_cast<eViewLayerEEVEEPassType>(
      inst_.film.enabled_passes_get() &
      (EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT | EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET |
       EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET));

  session_.reset();
  object_layer_ = nullptr;
  asset_layer_ = nullptr;
  material_layer_ = nullptr;

  if (enabled_passes && !inst_.is_viewport()) {
    session_.reset(BKE_cryptomatte_init_from_view_layer(inst_.view_layer));

    for (const std::string &layer_name :
         bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session_)) {
      StringRef layer_name_ref = layer_name;
      bke::cryptomatte::CryptomatteLayer *layer = bke::cryptomatte::BKE_cryptomatte_layer_get(
          *session_, layer_name);
      if (layer_name_ref.endswith(RE_PASSNAME_CRYPTOMATTE_OBJECT)) {
        object_layer_ = layer;
      }
      else if (layer_name_ref.endswith(RE_PASSNAME_CRYPTOMATTE_ASSET)) {
        asset_layer_ = layer;
      }
      else if (layer_name_ref.endswith(RE_PASSNAME_CRYPTOMATTE_MATERIAL)) {
        material_layer_ = layer;
      }
    }
  }

  if (!(enabled_passes &
        (EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT | EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET)))
  {
    /* Ensure dummy buffer for API validation. */
    cryptomatte_object_buf.resize(16);
  }
}

void Cryptomatte::sync_object(Object *ob, ResourceHandle res_handle)
{
  const eViewLayerEEVEEPassType enabled_passes = inst_.film.enabled_passes_get();
  if (!(enabled_passes &
        (EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT | EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET)))
  {
    return;
  }

  uint32_t resource_id = res_handle.resource_index();
  float2 object_hashes(0.0f, 0.0f);

  if (enabled_passes & EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT) {
    object_hashes[0] = register_id(EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT, ob->id);
  }

  if (enabled_passes & EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET) {
    Object *asset = ob;
    while (asset->parent) {
      asset = asset->parent;
    }
    object_hashes[1] = register_id(EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET, asset->id);
  }
  cryptomatte_object_buf.get_or_resize(resource_id) = object_hashes;
}

void Cryptomatte::sync_material(const ::Material *material)
{
  /* Material crypto hashes are generated during shader codegen stage. We only need to register
   * them to store inside the metadata. */
  if (material_layer_ && material) {
    material_layer_->add_ID(material->id);
  }
}

void Cryptomatte::end_sync()
{
  cryptomatte_object_buf.push_update();

  object_layer_ = nullptr;
  asset_layer_ = nullptr;
  material_layer_ = nullptr;
}

float Cryptomatte::register_id(const eViewLayerEEVEEPassType layer, const ID &id) const
{
  BLI_assert(ELEM(layer,
                  EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT,
                  EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET,
                  EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL));

  uint32_t cryptomatte_hash = 0;
  if (session_) {
    if (layer == EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT) {
      BLI_assert(object_layer_);
      cryptomatte_hash = object_layer_->add_ID(id);
    }
    else if (layer == EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET) {
      BLI_assert(asset_layer_);
      cryptomatte_hash = asset_layer_->add_ID(id);
    }
    else if (layer == EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL) {
      BLI_assert(material_layer_);
      cryptomatte_hash = material_layer_->add_ID(id);
    }
  }
  else {
    const char *name = &id.name[2];
    const int name_len = BLI_strnlen(name, MAX_NAME - 2);
    cryptomatte_hash = BKE_cryptomatte_hash(name, name_len);
  }

  return BKE_cryptomatte_hash_to_float(cryptomatte_hash);
}

void Cryptomatte::store_metadata(RenderResult *render_result)
{
  if (session_) {
    BKE_cryptomatte_store_metadata(&*session_, render_result, inst_.view_layer);
  }
}

}  // namespace blender::eevee
