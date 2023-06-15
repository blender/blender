/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DNA_particle_types.h"
#include "RNA_access.h"
#include "RNA_path.h"
#include "RNA_types.h"

#include "draw_handle.hh"
#include "draw_manager.hh"
#include "draw_shader_shared.h"

/* -------------------------------------------------------------------- */
/** \name ObjectAttributes
 * \{ */

/**
 * Go through all possible source of the given object uniform attribute.
 * Returns true if the attribute was correctly filled.
 * This function mirrors lookup_instance_property in cycles/blender/blender_object.cpp
 */
bool ObjectAttribute::sync(const blender::draw::ObjectRef &ref, const GPUUniformAttr &attr)
{
  hash_code = attr.hash_code;

  /* If requesting instance data, check the parent particle system and object. */
  if (attr.use_dupli) {
    return BKE_object_dupli_find_rgba_attribute(
        ref.object, ref.dupli_object, ref.dupli_parent, attr.name, &data_x);
  }
  return BKE_object_dupli_find_rgba_attribute(ref.object, nullptr, nullptr, attr.name, &data_x);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name LayerAttributes
 * \{ */

bool LayerAttribute::sync(Scene *scene, ViewLayer *layer, const GPULayerAttr &attr)
{
  hash_code = attr.hash_code;

  return BKE_view_layer_find_rgba_attribute(scene, layer, attr.name, &data.x);
}

/** \} */
