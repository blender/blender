/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"

#include "multires_reshape.hh"
#include "opensubdiv_converter_capi.hh"
#include "subdiv_converter.hh"

namespace blender {

#ifdef WITH_OPENSUBDIV

static bke::subdiv::Subdiv *subdiv_for_simple_to_catmull_clark(Object *object,
                                                               MultiresModifierData *mmd)
{
  using namespace blender::bke;
  subdiv::Settings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);

  const Mesh *base_mesh = id_cast<const Mesh *>(object->data);

  OpenSubdiv_Converter converter;
  subdiv::converter_init_for_mesh(&converter, &subdiv_settings, base_mesh);

  /* Emulate the old simple subdivision by making all of the topology infinitely sharp. */
  const Array<float> edge_sharpness(int64_t(converter.edges.size()),
                                    OPENSUBDIV_SHARPNESS_INFINITE);
  const Array<float> vert_sharpness(converter.verts_num, OPENSUBDIV_SHARPNESS_INFINITE);
  converter.edge_sharpness = edge_sharpness;
  converter.vert_sharpness = vert_sharpness;

  subdiv::Subdiv *subdiv = subdiv::new_from_converter(&subdiv_settings, &converter);
  subdiv::converter_free(&converter);

  if (!subdiv::eval_begin_from_mesh(subdiv, base_mesh, subdiv::SUBDIV_EVALUATOR_TYPE_CPU)) {
    subdiv::free(subdiv);
    return nullptr;
  }

  return subdiv;
}

#endif

void multires_do_versions_simple_to_catmull_clark(Object *object, MultiresModifierData *mmd)
{
#ifdef WITH_OPENSUBDIV
  const Mesh *base_mesh = id_cast<const Mesh *>(object->data);
  if (base_mesh->corners_num == 0) {
    return;
  }

  /* Store the grids displacement in object space against the simple limit surface. */
  {
    bke::subdiv::Subdiv *subdiv = subdiv_for_simple_to_catmull_clark(object, mmd);
    MultiresReshapeContext reshape_context;
    if (!multires_reshape_context_create_from_subdiv(
            &reshape_context, object, mmd, subdiv, mmd->totlvl))
    {
      bke::subdiv::free(subdiv);
      return;
    }

    multires_reshape_store_original_grids(&reshape_context);
    multires_reshape_assign_final_coords_from_mdisps(&reshape_context);
    multires_reshape_context_free(&reshape_context);

    bke::subdiv::free(subdiv);
  }

  /* Calculate the new tangent displacement against the new Catmull-Clark limit surface. */
  {
    MultiresReshapeContext reshape_context;
    if (!multires_reshape_context_create_from_modifier(&reshape_context, object, mmd, mmd->totlvl))
    {
      return;
    }
    multires_reshape_object_grids_to_tangent_displacement(&reshape_context);
    multires_reshape_context_free(&reshape_context);
  }
#else
  UNUSED_VARS(object, mmd);
#endif
}

}  // namespace blender
