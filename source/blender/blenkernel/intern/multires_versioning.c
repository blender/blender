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
 * The Original Code is Copyright (C) 2020 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_subdiv.h"
#include "BKE_subdiv_eval.h"

#include "multires_reshape.h"
#include "opensubdiv_converter_capi.h"
#include "subdiv_converter.h"

static float simple_to_catmull_clark_get_edge_sharpness(
    const OpenSubdiv_Converter *UNUSED(converter), int UNUSED(manifold_edge_index))
{
  return 10.0f;
}

static bool simple_to_catmull_clark_is_infinite_sharp_vertex(
    const OpenSubdiv_Converter *UNUSED(converter), int UNUSED(manifold_vertex_index))
{
  return true;
}

static Subdiv *subdiv_for_simple_to_catmull_clark(Object *object, MultiresModifierData *mmd)
{
  SubdivSettings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);

  Mesh *base_mesh = object->data;

  OpenSubdiv_Converter converter;
  BKE_subdiv_converter_init_for_mesh(&converter, &subdiv_settings, base_mesh);
  converter.getEdgeSharpness = simple_to_catmull_clark_get_edge_sharpness;
  converter.isInfiniteSharpVertex = simple_to_catmull_clark_is_infinite_sharp_vertex;

  Subdiv *subdiv = BKE_subdiv_new_from_converter(&subdiv_settings, &converter);
  BKE_subdiv_converter_free(&converter);

  if (!BKE_subdiv_eval_begin_from_mesh(subdiv, base_mesh, NULL)) {
    BKE_subdiv_free(subdiv);
    return NULL;
  }

  return subdiv;
}

void multires_do_versions_simple_to_catmull_clark(Object *object, MultiresModifierData *mmd)
{
  const Mesh *base_mesh = object->data;
  if (base_mesh->totloop == 0) {
    return;
  }

  /* Store the grids displacement in object space against the simple limit surface. */
  {
    Subdiv *subdiv = subdiv_for_simple_to_catmull_clark(object, mmd);
    MultiresReshapeContext reshape_context;
    if (!multires_reshape_context_create_from_subdiv(
            &reshape_context, object, mmd, subdiv, mmd->totlvl)) {
      BKE_subdiv_free(subdiv);
      return;
    }

    multires_reshape_store_original_grids(&reshape_context);
    multires_reshape_assign_final_coords_from_mdisps(&reshape_context);
    multires_reshape_context_free(&reshape_context);

    BKE_subdiv_free(subdiv);
  }

  /* Calculate the new tangent displacement against the new Catmull-Clark limit surface. */
  {
    MultiresReshapeContext reshape_context;
    if (!multires_reshape_context_create_from_modifier(
            &reshape_context, object, mmd, mmd->totlvl)) {
      return;
    }
    multires_reshape_object_grids_to_tangent_displacement(&reshape_context);
    multires_reshape_context_free(&reshape_context);
  }
}
