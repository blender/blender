/* SPDX-FileCopyrightText: 2025 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_instancing_utils.hh"

#include "usd.hh"
#include "usd_hash_types.hh"
#include "usd_utils.hh"

#include "BLI_map.hh"
#include "BLI_set.hh"

#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/primCompositionQuery.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/references.h>

#include <string>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace blender::io::usd {

/* We need an ordered map so we use std::map. */
using PathMap = std::map<pxr::SdfPath, pxr::SdfPath>;
using PathSet = Set<pxr::SdfPath>;

/* Map an instanceable prim path to a list of prototype prim paths. */
using ReferencesMap = Map<pxr::SdfPath, Vector<pxr::SdfPath>>;

/* Convert the given prototype prim to an instance by deleting its children and making
 * it an instanceable reference to the prim at ref_path. */
static void convert_proto_to_instance(pxr::UsdStageRefPtr stage,
                                      const pxr::SdfPath &proto_path,
                                      const pxr::SdfPath &ref_path)
{
  pxr::UsdPrim proto_prim = stage->GetPrimAtPath(proto_path);

  if (!proto_prim) {
    CLOG_ERROR(&LOG, "Couldn't find prototype prim %s", proto_path.GetAsString().c_str());
    return;
  }

  /* Collect child paths. */
  pxr::SdfPathVector child_paths;
  pxr::UsdPrimSiblingRange children = proto_prim.GetFilteredChildren(
      pxr::Usd_PrimFlagsPredicate());
  for (const auto &child_prim : children) {
    child_paths.push_back(child_prim.GetPath());
  }

  /* Remove children from the sage. */
  for (const pxr::SdfPath &child_path : child_paths) {
    stage->RemovePrim(child_path);
  }

  proto_prim.GetReferences().AddInternalReference(pxr::SdfPath(ref_path));
  proto_prim.SetInstanceable(true);
}

void process_scene_graph_instances(const USDExportParams &export_params, pxr::UsdStageRefPtr stage)
{
  if (!stage) {
    return;
  }

  /* Collect paths to instanceable references and prototypes. */
  PathSet protos;
  /* Map an instance to the prototypes it references. */
  ReferencesMap references_map;

  pxr::UsdPrimRange range(stage->GetPseudoRoot());
  for (pxr::UsdPrim prim : range) {
    if (prim.IsInstanceable()) {
      /* Get the prototypes referenced by this prim. */
      pxr::UsdPrimCompositionQuery query = pxr::UsdPrimCompositionQuery::GetDirectReferences(prim);
      Vector<pxr::SdfPath> references;
      for (const auto &arc : query.GetCompositionArcs()) {
        pxr::SdfPath target_prim_path = arc.GetTargetPrimPath();
        protos.add(target_prim_path);
        references.append(target_prim_path);
      }
      references_map.add(prim.GetPath(), references);
    }
  }

  if (protos.is_empty()) {
    /* No prototypes to move. */
    return;
  }

  /* Map an original prototype path to the location where it will be copied. */
  PathMap proto_to_copy_map;

  std::string protos_root_str(export_params.root_prim_path);
  protos_root_str += "/prototypes";
  pxr::SdfPath protos_root_path = get_unique_path(stage, protos_root_str);

  /* Create the abstract prim under which prototypes will be copied. */
  if (!stage->CreateClassPrim(protos_root_path)) {
    CLOG_ERROR(&LOG, "Couldn't create class prim %s.", protos_root_path.GetAsString().c_str());
    return;
  }

  /*
   * For each original prototype, create a placeholder Xform prim under the protos root
   * which will be the new location where the prototype will be copied.
   */
  for (const pxr::SdfPath &proto_path : protos) {
    pxr::SdfPath copy_path = protos_root_path;

    copy_path = copy_path.AppendChild(proto_path.GetNameToken());
    copy_path = get_unique_path(stage, copy_path.GetAsString());

    /* Create the placeholder prim. */
    static pxr::TfToken xform_type_tok("Xform");
    pxr::UsdPrim dest_prim = stage->DefinePrim(copy_path, xform_type_tok);
    if (!dest_prim) {
      CLOG_ERROR(&LOG,
                 "Couldn't create destination prim %s for copying prototype %s",
                 copy_path.GetAsString().c_str(),
                 proto_path.GetAsString().c_str());
      continue;
    }

    /* Record where original prototype path will be copied. */
    proto_to_copy_map.insert(std::make_pair(proto_path, dest_prim.GetPath()));
  }

  /* Update all references to point to new prototype locations. */
  for (const auto item : references_map.items()) {
    pxr::SdfPath inst_path = item.key;
    pxr::UsdPrim inst_prim = stage->GetPrimAtPath(item.key);
    if (!inst_prim) {
      CLOG_ERROR(&LOG, "Couldn't get prim for instance %s.", inst_path.GetAsString().c_str());
      continue;
    }

    /* Updated references pointing to new prototype locations. */
    Vector<pxr::SdfPath> new_ref_targets;
    const Vector<pxr::SdfPath> &ref_targets = item.value;
    for (const pxr::SdfPath &target_path : ref_targets) {
      PathMap::const_iterator iter = proto_to_copy_map.find(target_path);
      if (iter != proto_to_copy_map.end()) {
        new_ref_targets.append(iter->second);
      }
    }

    /* Replace existing references with the updated ones. */
    if (!new_ref_targets.is_empty()) {
      pxr::UsdReferences refs = inst_prim.GetReferences();
      refs.ClearReferences();
      for (const pxr::SdfPath &target : new_ref_targets) {
        refs.AddInternalReference(target);
      }
    }
  }

  /*
   * Copy the original prototypes to their new locations and update
   * the original prototype roots to be references to the new locations.
   * Since prototypes may be nested, we must copy the most nested prototypes
   * first by iterating backwards through the sorted prototype map.
   */
  for (PathMap::reverse_iterator riter = proto_to_copy_map.rbegin();
       riter != proto_to_copy_map.rend();
       ++riter)
  {
    const pxr::SdfPath &src_path = riter->first;
    const pxr::SdfPath &dst_path = riter->second;
    if (!pxr::SdfCopySpec(
            stage->GetRootLayer(), riter->first, stage->GetRootLayer(), riter->second))
    {
      CLOG_WARN(&LOG,
                "Couldn't copy prim %s to %s",
                src_path.GetAsString().c_str(),
                dst_path.GetAsString().c_str());
      continue;
    }

    convert_proto_to_instance(stage, src_path, dst_path);
  }
}

}  // namespace blender::io::usd
