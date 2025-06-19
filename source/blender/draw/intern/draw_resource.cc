/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_duplilist.hh"
#include "GPU_material.hh"

#include "draw_handle.hh"
#include "draw_shader_shared.hh"

/* -------------------------------------------------------------------- */
/** \name ObjectAttributes
 * \{ */

bool ObjectAttribute::sync(const blender::draw::ObjectRef &ref, const GPUUniformAttr &attr)
{
  /* This function mirrors `lookup_instance_property` in `cycles/blender/blender_object.cpp`. */
  hash_code = attr.hash_code;
  return ref.find_rgba_attribute(attr, &data_x);
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
