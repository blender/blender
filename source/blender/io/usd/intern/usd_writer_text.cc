/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_text.hh"
#include "usd_exporter_context.hh"

#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "DNA_mesh_types.h"

namespace blender::io::usd {

USDTextWriter::USDTextWriter(const USDExporterContext &ctx) : USDGenericMeshWriter(ctx) {}

Mesh *USDTextWriter::get_export_mesh(Object *object_eval, bool &r_needsfree)
{
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(object_eval);
  if (mesh_eval != nullptr) {
    /* Mesh_eval only exists when generative modifiers are in use. */
    r_needsfree = false;
    return mesh_eval;
  }
  r_needsfree = true;
  return BKE_mesh_new_from_object(usd_export_context_.depsgraph, object_eval, false, false, true);
}

void USDTextWriter::free_export_mesh(Mesh *mesh)
{
  BKE_id_free(nullptr, mesh);
}

}  // namespace blender::io::usd
