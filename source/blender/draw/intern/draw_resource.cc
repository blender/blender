/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_duplilist.hh"
#include "BLI_ghash.hh"
#include "GPU_material.hh"

#include "draw_handle.hh"
#include "draw_shader_shared.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name ObjectAttributes
 * \{ */

bool ObjectAttribute::sync(const draw::ObjectRef &ref,
                           const GPUUniformAttr &attr,
                           int instance_index)
{
  /* This function mirrors `lookup_instance_property` in `cycles/blender/blender_object.cpp`. */
  hash_code = attr.hash_code;
  return ref.find_rgba_attribute(attr, instance_index, &data_x);
}

bool ObjectAttribute::sync(const draw::ObjectRef &ref,
                           const char *name,
                           bool use_dupli,
                           int instance_index)
{
  /* Mimic gpu_node_graph_add_uniform_attribute */
  hash_code = BLI_ghashutil_strhash_p(name) << 1 | (use_dupli ? 0 : 1);
  return ref.find_rgba_attribute(name, use_dupli, instance_index, &data_x);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name LayerAttributes
 * \{ */

bool LayerAttribute::sync(const Scene *scene, const ViewLayer *layer, const GPULayerAttr &attr)
{
  hash_code = attr.hash_code;

  return BKE_view_layer_find_rgba_attribute(scene, layer, attr.name, &data.x);
}

/** \} */

}  // namespace blender
