/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_viewer_path.h"

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "MEM_guardedalloc.h"

#include "BLO_read_write.h"

using blender::IndexRange;
using blender::StringRef;

void BKE_viewer_path_init(ViewerPath *viewer_path)
{
  BLI_listbase_clear(&viewer_path->path);
}

void BKE_viewer_path_clear(ViewerPath *viewer_path)
{
  LISTBASE_FOREACH_MUTABLE (ViewerPathElem *, elem, &viewer_path->path) {
    BKE_viewer_path_elem_free(elem);
  }
  BLI_listbase_clear(&viewer_path->path);
}

void BKE_viewer_path_copy(ViewerPath *dst, const ViewerPath *src)
{
  BKE_viewer_path_init(dst);
  LISTBASE_FOREACH (const ViewerPathElem *, src_elem, &src->path) {
    ViewerPathElem *new_elem = BKE_viewer_path_elem_copy(src_elem);
    BLI_addtail(&dst->path, new_elem);
  }
}

bool BKE_viewer_path_equal(const ViewerPath *a, const ViewerPath *b)
{
  const ViewerPathElem *elem_a = static_cast<const ViewerPathElem *>(a->path.first);
  const ViewerPathElem *elem_b = static_cast<const ViewerPathElem *>(b->path.first);

  while (elem_a != nullptr && elem_b != nullptr) {
    if (!BKE_viewer_path_elem_equal(elem_a, elem_b)) {
      return false;
    }
    elem_a = elem_a->next;
    elem_b = elem_b->next;
  }
  if (elem_a == nullptr && elem_b == nullptr) {
    return true;
  }
  return false;
}

void BKE_viewer_path_blend_write(BlendWriter *writer, const ViewerPath *viewer_path)
{
  LISTBASE_FOREACH (ViewerPathElem *, elem, &viewer_path->path) {
    switch (ViewerPathElemType(elem->type)) {
      case VIEWER_PATH_ELEM_TYPE_ID: {
        const auto *typed_elem = reinterpret_cast<IDViewerPathElem *>(elem);
        BLO_write_struct(writer, IDViewerPathElem, typed_elem);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
        const auto *typed_elem = reinterpret_cast<ModifierViewerPathElem *>(elem);
        BLO_write_struct(writer, ModifierViewerPathElem, typed_elem);
        BLO_write_string(writer, typed_elem->modifier_name);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
        const auto *typed_elem = reinterpret_cast<GroupNodeViewerPathElem *>(elem);
        BLO_write_struct(writer, GroupNodeViewerPathElem, typed_elem);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
        const auto *typed_elem = reinterpret_cast<SimulationZoneViewerPathElem *>(elem);
        BLO_write_struct(writer, SimulationZoneViewerPathElem, typed_elem);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
        const auto *typed_elem = reinterpret_cast<ViewerNodeViewerPathElem *>(elem);
        BLO_write_struct(writer, ViewerNodeViewerPathElem, typed_elem);
        break;
      }
    }
    BLO_write_string(writer, elem->ui_name);
  }
}

void BKE_viewer_path_blend_read_data(BlendDataReader *reader, ViewerPath *viewer_path)
{
  BLO_read_list(reader, &viewer_path->path);
  LISTBASE_FOREACH (ViewerPathElem *, elem, &viewer_path->path) {
    BLO_read_data_address(reader, &elem->ui_name);
    switch (ViewerPathElemType(elem->type)) {
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE:
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE:
      case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE:
      case VIEWER_PATH_ELEM_TYPE_ID: {
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
        auto *typed_elem = reinterpret_cast<ModifierViewerPathElem *>(elem);
        BLO_read_data_address(reader, &typed_elem->modifier_name);
        break;
      }
    }
  }
}

void BKE_viewer_path_blend_read_lib(BlendLibReader *reader, ID *self_id, ViewerPath *viewer_path)
{
  LISTBASE_FOREACH (ViewerPathElem *, elem, &viewer_path->path) {
    switch (ViewerPathElemType(elem->type)) {
      case VIEWER_PATH_ELEM_TYPE_ID: {
        auto *typed_elem = reinterpret_cast<IDViewerPathElem *>(elem);
        BLO_read_id_address(reader, self_id, &typed_elem->id);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_MODIFIER:
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE:
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE:
      case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
        break;
      }
    }
  }
}

void BKE_viewer_path_foreach_id(LibraryForeachIDData *data, ViewerPath *viewer_path)
{
  LISTBASE_FOREACH (ViewerPathElem *, elem, &viewer_path->path) {
    switch (ViewerPathElemType(elem->type)) {
      case VIEWER_PATH_ELEM_TYPE_ID: {
        auto *typed_elem = reinterpret_cast<IDViewerPathElem *>(elem);
        BKE_LIB_FOREACHID_PROCESS_ID(data, typed_elem->id, IDWALK_CB_NOP);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_MODIFIER:
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE:
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE:
      case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
        break;
      }
    }
  }
}

void BKE_viewer_path_id_remap(ViewerPath *viewer_path, const IDRemapper *mappings)
{
  LISTBASE_FOREACH (ViewerPathElem *, elem, &viewer_path->path) {
    switch (ViewerPathElemType(elem->type)) {
      case VIEWER_PATH_ELEM_TYPE_ID: {
        auto *typed_elem = reinterpret_cast<IDViewerPathElem *>(elem);
        BKE_id_remapper_apply(mappings, &typed_elem->id, ID_REMAP_APPLY_DEFAULT);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_MODIFIER:
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE:
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE:
      case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
        break;
      }
    }
  }
}

template<typename T> static T *make_elem(const ViewerPathElemType type)
{
  T *elem = MEM_cnew<T>(__func__);
  elem->base.type = type;
  return elem;
}

ViewerPathElem *BKE_viewer_path_elem_new(const ViewerPathElemType type)
{
  switch (type) {
    case VIEWER_PATH_ELEM_TYPE_ID: {
      return &make_elem<IDViewerPathElem>(type)->base;
    }
    case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
      return &make_elem<ModifierViewerPathElem>(type)->base;
    }
    case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
      return &make_elem<GroupNodeViewerPathElem>(type)->base;
    }
    case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
      return &make_elem<SimulationZoneViewerPathElem>(type)->base;
    }
    case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
      return &make_elem<ViewerNodeViewerPathElem>(type)->base;
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

IDViewerPathElem *BKE_viewer_path_elem_new_id()
{
  return reinterpret_cast<IDViewerPathElem *>(BKE_viewer_path_elem_new(VIEWER_PATH_ELEM_TYPE_ID));
}

ModifierViewerPathElem *BKE_viewer_path_elem_new_modifier()
{
  return reinterpret_cast<ModifierViewerPathElem *>(
      BKE_viewer_path_elem_new(VIEWER_PATH_ELEM_TYPE_MODIFIER));
}

GroupNodeViewerPathElem *BKE_viewer_path_elem_new_group_node()
{
  return reinterpret_cast<GroupNodeViewerPathElem *>(
      BKE_viewer_path_elem_new(VIEWER_PATH_ELEM_TYPE_GROUP_NODE));
}

SimulationZoneViewerPathElem *BKE_viewer_path_elem_new_simulation_zone()
{
  return reinterpret_cast<SimulationZoneViewerPathElem *>(
      BKE_viewer_path_elem_new(VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE));
}

ViewerNodeViewerPathElem *BKE_viewer_path_elem_new_viewer_node()
{
  return reinterpret_cast<ViewerNodeViewerPathElem *>(
      BKE_viewer_path_elem_new(VIEWER_PATH_ELEM_TYPE_VIEWER_NODE));
}

ViewerPathElem *BKE_viewer_path_elem_copy(const ViewerPathElem *src)
{
  ViewerPathElem *dst = BKE_viewer_path_elem_new(ViewerPathElemType(src->type));
  if (src->ui_name) {
    dst->ui_name = BLI_strdup(src->ui_name);
  }
  switch (ViewerPathElemType(src->type)) {
    case VIEWER_PATH_ELEM_TYPE_ID: {
      const auto *old_elem = reinterpret_cast<const IDViewerPathElem *>(src);
      auto *new_elem = reinterpret_cast<IDViewerPathElem *>(dst);
      new_elem->id = old_elem->id;
      break;
    }
    case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
      const auto *old_elem = reinterpret_cast<const ModifierViewerPathElem *>(src);
      auto *new_elem = reinterpret_cast<ModifierViewerPathElem *>(dst);
      if (old_elem->modifier_name != nullptr) {
        new_elem->modifier_name = BLI_strdup(old_elem->modifier_name);
      }
      break;
    }
    case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
      const auto *old_elem = reinterpret_cast<const GroupNodeViewerPathElem *>(src);
      auto *new_elem = reinterpret_cast<GroupNodeViewerPathElem *>(dst);
      new_elem->node_id = old_elem->node_id;
      break;
    }
    case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
      const auto *old_elem = reinterpret_cast<const SimulationZoneViewerPathElem *>(src);
      auto *new_elem = reinterpret_cast<SimulationZoneViewerPathElem *>(dst);
      new_elem->sim_output_node_id = old_elem->sim_output_node_id;
      break;
    }
    case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
      const auto *old_elem = reinterpret_cast<const ViewerNodeViewerPathElem *>(src);
      auto *new_elem = reinterpret_cast<ViewerNodeViewerPathElem *>(dst);
      new_elem->node_id = old_elem->node_id;
      break;
    }
  }
  return dst;
}

bool BKE_viewer_path_elem_equal(const ViewerPathElem *a, const ViewerPathElem *b)
{
  if (a->type != b->type) {
    return false;
  }
  switch (ViewerPathElemType(a->type)) {
    case VIEWER_PATH_ELEM_TYPE_ID: {
      const auto *a_elem = reinterpret_cast<const IDViewerPathElem *>(a);
      const auto *b_elem = reinterpret_cast<const IDViewerPathElem *>(b);
      return a_elem->id == b_elem->id;
    }
    case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
      const auto *a_elem = reinterpret_cast<const ModifierViewerPathElem *>(a);
      const auto *b_elem = reinterpret_cast<const ModifierViewerPathElem *>(b);
      return StringRef(a_elem->modifier_name) == StringRef(b_elem->modifier_name);
    }
    case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
      const auto *a_elem = reinterpret_cast<const GroupNodeViewerPathElem *>(a);
      const auto *b_elem = reinterpret_cast<const GroupNodeViewerPathElem *>(b);
      return a_elem->node_id == b_elem->node_id;
    }
    case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
      const auto *a_elem = reinterpret_cast<const SimulationZoneViewerPathElem *>(a);
      const auto *b_elem = reinterpret_cast<const SimulationZoneViewerPathElem *>(b);
      return a_elem->sim_output_node_id == b_elem->sim_output_node_id;
    }
    case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
      const auto *a_elem = reinterpret_cast<const ViewerNodeViewerPathElem *>(a);
      const auto *b_elem = reinterpret_cast<const ViewerNodeViewerPathElem *>(b);
      return a_elem->node_id == b_elem->node_id;
    }
  }
  return false;
}

void BKE_viewer_path_elem_free(ViewerPathElem *elem)
{
  switch (ViewerPathElemType(elem->type)) {
    case VIEWER_PATH_ELEM_TYPE_ID:
    case VIEWER_PATH_ELEM_TYPE_GROUP_NODE:
    case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE:
    case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
      break;
    }
    case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
      auto *typed_elem = reinterpret_cast<ModifierViewerPathElem *>(elem);
      MEM_SAFE_FREE(typed_elem->modifier_name);
      break;
    }
  }
  if (elem->ui_name) {
    MEM_freeN(elem->ui_name);
  }
  MEM_freeN(elem);
}
