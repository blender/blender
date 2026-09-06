/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BLI_resource_scope.hh"

#include "RNA_define.hh"

namespace blender {

struct BlenderRNA;
struct StructRNA;
namespace nodes {

/**
 * Contains a runtime registration of RNA types generated based on the node group's interface for
 * a particular context. These RNA types are not registered in the global list, but owned by the
 * node group so that RNA access to interface properties works as long as the node group exists.
 */
struct GeneratedTreeSrnaData {
  ResourceScope scope;
  StructRNA *properties_struct;
  BlenderRNA *generated_rna;
  GeneratedTreeSrnaData()
  {
    generated_rna = RNA_create_runtime();
  }
  ~GeneratedTreeSrnaData()
  {
    RNA_free(generated_rna);
  }
};

}  // namespace nodes
}  // namespace blender
