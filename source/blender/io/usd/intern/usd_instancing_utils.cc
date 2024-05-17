/* SPDX-FileCopyrightText: 2024 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_instancing_utils.hh"
#include "usd_hash_types.hh"

#include "BLI_map.hh"

#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/primCompositionQuery.h>
#include <pxr/usd/usd/primRange.h>

#include <string>

#include "CLG_log.h"
static CLG_LogRef LOG = { "io.usd" };

namespace usdtokens {
/* Identifier to a temporary entry in a prim's custom data dictionary which stores
 * a path to a prototype to be referenced when the prim is converted to an
 * instance. This entry is temporarily set as a step during the conversion of
 * original prototypes to instances. */
static const pxr::TfToken refpath("blender:temp:refpath", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace {

/* If the given path already exists on the given stage, return the path with
 * a numerical suffix appende to the name that ensures the path is unique. If
 * the path does not exist on the stage, it will be returned unchanged. */
pxr::SdfPath get_unique_path(pxr::UsdStageRefPtr stage, const std::string& path)
{
  std::string unique_path = path;
  int suffix = 2;
  while (stage->GetPrimAtPath(pxr::SdfPath(unique_path)).IsValid()) {
    unique_path = path + std::to_string(suffix++);
  }
  return pxr::SdfPath(unique_path);
}

} // End anonymous namespace

namespace blender::io::usd {

using SdfPathMap = Map<pxr::SdfPath, pxr::SdfPath>;

/* Returns the top-most prototype prim in the subtree containing the given
 * proto_path. */
static bool is_root_prototype(pxr::UsdStageRefPtr stage, const pxr::SdfPath& proto_path, const pxr::SdfPathSet &all_protos)
{
  if (all_protos.find(proto_path) == all_protos.end()) {
    return false;
  }

  pxr::UsdPrim prim = stage->GetPrimAtPath(proto_path);
  if (!prim) {
    return false;
  }

  prim = prim.GetParent();
  while (prim) {
    if (all_protos.find(prim.GetPath()) != all_protos.end()) {
      return false;
    }
    prim = prim.GetParent();
  }

  return true;
}

/* Convert a prototype prim to an instance referencing the path given by the 'refpath'
 * entry in the prototype's data dictionary. */
static void proto_to_instance(pxr::UsdStageRefPtr stage, const pxr::SdfPath &proto_path)
{
  BLI_assert(stage);

  pxr::UsdPrim proto_prim = stage->GetPrimAtPath(proto_path);

  if (!proto_prim) {
    CLOG_WARN(&LOG,
      "Couldn't get proto prim %s",
      proto_path.GetAsString().c_str());
    return;
  }

  pxr::VtValue refpath_value;
  if (!proto_prim.GetMetadataByDictKey(pxr::SdfFieldKeys->CustomData, usdtokens::refpath, &refpath_value)) {
    CLOG_WARN(&LOG,
      "Couldn't get refpath value from prim %s",
      proto_prim.GetPath().GetAsString().c_str());
    return;
  }

  if (!refpath_value.IsHolding<std::string>()) {
    CLOG_WARN(&LOG,
      "refpath value from prim %s is not a string",
      proto_prim.GetPath().GetAsString().c_str());
    return;
  }

  std::string refpath = refpath_value.Get<std::string>();

  if (refpath.empty()) {
    return;
  }

  /* Delete any children of the prototype. */

  /* Collect child paths. */
  pxr::SdfPathVector child_paths;
  pxr::UsdPrimSiblingRange children = proto_prim.GetFilteredChildren(pxr::Usd_PrimFlagsPredicate());
  for (const auto& child_prim : children) {
    child_paths.push_back(child_prim.GetPath());
  }

  /* Remove children from the sage. */
  for (const pxr::SdfPath& child_path : child_paths) {
    stage->RemovePrim(child_path);
  }

  /* Clear temporary metadata. */
  proto_prim.ClearMetadataByDictKey(pxr::SdfFieldKeys->CustomData, usdtokens::refpath);

  proto_prim.GetReferences().AddInternalReference(pxr::SdfPath(refpath));
  proto_prim.SetInstanceable(true);
}

void process_scene_graph_instances(pxr::UsdStageRefPtr stage)
{
  if (!stage) {
    return;
  }

  /* Collect paths to instances and prototypes. */
  pxr::SdfPathSet referenced_protos;
  pxr::SdfPathVector instances;

  pxr::UsdPrimRange range(stage->GetPseudoRoot());
  for (pxr::UsdPrim prim : range) {
    if (prim.IsInstanceable()) {
      pxr::UsdPrimCompositionQuery query = pxr::UsdPrimCompositionQuery::GetDirectReferences(prim);
      for (const auto &arc : query.GetCompositionArcs()) {
        referenced_protos.insert(arc.GetTargetPrimPath());
        instances.push_back(prim.GetPath());
      }
    }
  }

  /* Paths where prototypes will be copied into abstract prims. */
  pxr::SdfPathVector abstract_protos;

  /* Map an original prototype path to the location where
   * it was copied. */
  SdfPathMap proto_map;

  /* For each original prototype, create a placeholder abstract prim where
   * the prototype will be copied. */
  for (const pxr::SdfPath &path : referenced_protos) {
    pxr::SdfPath dst_path = get_unique_path(stage, "/Proto");
    if (!stage->CreateClassPrim(dst_path).IsValid()) {
      CLOG_WARN(&LOG,
        "Couldn't create class prim %s",
        dst_path.GetAsString().c_str());
      continue;
    }
    dst_path = dst_path.AppendChild(path.GetNameToken());
    /* Create a placeholder prim where the prototype will be copied later. */
    stage->DefinePrim(dst_path);

    /* Record where original prototype path will be copied. */
    proto_map.add(path, dst_path);
    abstract_protos.push_back(dst_path);
  }

  /* Update references to point to new prototype locations. */
  for (const pxr::SdfPath& path : instances) {
    pxr::UsdPrim prim = stage->GetPrimAtPath(path);
    if (!prim) {
      continue;
    }
    pxr::SdfReferenceVector new_refs;

    pxr::UsdPrimCompositionQuery query = pxr::UsdPrimCompositionQuery::GetDirectReferences(prim);
    for (const auto& arc : query.GetCompositionArcs()) {
      pxr::SdfPath path = arc.GetTargetPrimPath();
      pxr::SdfPath new_path = proto_map.lookup_default(path, path);
      pxr::SdfReference ref("", new_path);
      new_refs.push_back(ref);
    }

    pxr::UsdReferences refs = prim.GetReferences();
    refs.SetReferences(new_refs);
  }

  /* Update each original prototype prim with a temporary 'refpath' dictionary entry
   * storing the location where the prototype will be copied.  This infomation will
   * be used later to convert the original prototypes to instances referencing the new
   * locations. */
  for (const pxr::SdfPath& path : referenced_protos) {
    if (!proto_map.contains(path)) {
      continue;
    }
    pxr::UsdPrim prim = stage->GetPrimAtPath(path);
    if (!prim) {
      continue;
    }
    pxr::SdfPath ref_path = proto_map.lookup(path);
    prim.SetMetadataByDictKey(pxr::SdfFieldKeys->CustomData, usdtokens::refpath, pxr::VtValue(ref_path.GetAsString()));
  }

  /* Copy the original prototypes to their new locations. */
  for (const pxr::SdfPath& path : referenced_protos) {
    if (!proto_map.contains(path)) {
      continue;
    }
    pxr::UsdPrim prim = stage->GetPrimAtPath(path);
    if (!prim) {
      continue;
    }
    pxr::SdfPath ref_path = proto_map.lookup(path);
    if (!pxr::SdfCopySpec(stage->GetRootLayer(), path, stage->GetRootLayer(), ref_path)) {
      CLOG_WARN(&LOG,
        "Couldn't copy prim %s to %s",
        path.GetAsString().c_str(),
        ref_path.GetAsString().c_str());
      continue;
    }
    pxr::UsdPrim ref_prim = stage->GetPrimAtPath(ref_path);
    if (!ref_prim) {
      CLOG_WARN(&LOG,
        "Couldn't access prim %s",
        ref_path.GetAsString().c_str());
      continue;
    }
    ref_prim.ClearMetadataByDictKey(pxr::SdfFieldKeys->CustomData, usdtokens::refpath);
  }

  /* Iterate over the copied abstract prototypes and convert any nested protypes to
   * instances referencing the copied abstract prototypes. */
  for (const pxr::SdfPath& path : abstract_protos) {
    pxr::UsdPrim prim = stage->GetPrimAtPath(path);

    if (!prim) {
      CLOG_WARN(&LOG,
        "Couldn't get abstract proto prim %s",
        path.GetAsString().c_str());
      continue;
    }

    /* Use default constructed predicate because we want to include abstract
     * prims excluded by the default predicate. */
    pxr::UsdPrimRange range(prim, pxr::Usd_PrimFlagsPredicate());

    pxr::SdfPathVector nested_prototypes;

    for (auto iter = range.begin(); iter != range.end(); ++iter) {
      if (!iter->HasMetadataDictKey(pxr::SdfFieldKeys->CustomData, usdtokens::refpath)) {
        continue;
      }
      nested_prototypes.push_back(iter->GetPath());
      iter.PruneChildren();
    }

    for (const pxr::SdfPath& nested_proto_path : nested_prototypes) {
      proto_to_instance(stage, nested_proto_path);
    }
  }

  /* Finally, convert the original prototypes to instances. */

  /* Find the original set of root-level prototypes.  I.e., those prototypes that
   * don't have a prototype as an ancestor. */
  pxr::SdfPathVector root_protos;
  for (const pxr::SdfPath &path : referenced_protos) {
    if (is_root_prototype(stage, path, referenced_protos)) {
      root_protos.push_back(path);
    }
  }
  /* Convert the root prototypes to instances. */
  for (const pxr::SdfPath& path : root_protos) {
    proto_to_instance(stage, path);
  }
}

}  // namespace blender::io::usd
