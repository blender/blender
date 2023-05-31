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

void BKE_viewer_path_blend_write(struct BlendWriter *writer, const ViewerPath *viewer_path)
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
      case VIEWER_PATH_ELEM_TYPE_NODE: {
        const auto *typed_elem = reinterpret_cast<NodeViewerPathElem *>(elem);
        BLO_write_struct(writer, NodeViewerPathElem, typed_elem);
        BLO_write_string(writer, typed_elem->node_name);
        break;
      }
    }
  }
}

void BKE_viewer_path_blend_read_data(struct BlendDataReader *reader, ViewerPath *viewer_path)
{
  BLO_read_list(reader, &viewer_path->path);
  LISTBASE_FOREACH (ViewerPathElem *, elem, &viewer_path->path) {
    switch (ViewerPathElemType(elem->type)) {
      case VIEWER_PATH_ELEM_TYPE_ID: {
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
        auto *typed_elem = reinterpret_cast<ModifierViewerPathElem *>(elem);
        BLO_read_data_address(reader, &typed_elem->modifier_name);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_NODE: {
        auto *typed_elem = reinterpret_cast<NodeViewerPathElem *>(elem);
        BLO_read_data_address(reader, &typed_elem->node_name);
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
      case VIEWER_PATH_ELEM_TYPE_NODE: {
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
      case VIEWER_PATH_ELEM_TYPE_NODE: {
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
      case VIEWER_PATH_ELEM_TYPE_NODE: {
        break;
      }
    }
  }
}

ViewerPathElem *BKE_viewer_path_elem_new(const ViewerPathElemType type)
{
  switch (type) {
    case VIEWER_PATH_ELEM_TYPE_ID: {
      IDViewerPathElem *elem = MEM_cnew<IDViewerPathElem>(__func__);
      elem->base.type = type;
      return &elem->base;
    }
    case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
      ModifierViewerPathElem *elem = MEM_cnew<ModifierViewerPathElem>(__func__);
      elem->base.type = type;
      return &elem->base;
    }
    case VIEWER_PATH_ELEM_TYPE_NODE: {
      NodeViewerPathElem *elem = MEM_cnew<NodeViewerPathElem>(__func__);
      elem->base.type = type;
      return &elem->base;
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

NodeViewerPathElem *BKE_viewer_path_elem_new_node()
{
  return reinterpret_cast<NodeViewerPathElem *>(
      BKE_viewer_path_elem_new(VIEWER_PATH_ELEM_TYPE_NODE));
}

ViewerPathElem *BKE_viewer_path_elem_copy(const ViewerPathElem *src)
{
  ViewerPathElem *dst = BKE_viewer_path_elem_new(ViewerPathElemType(src->type));
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
    case VIEWER_PATH_ELEM_TYPE_NODE: {
      const auto *old_elem = reinterpret_cast<const NodeViewerPathElem *>(src);
      auto *new_elem = reinterpret_cast<NodeViewerPathElem *>(dst);
      new_elem->node_id = old_elem->node_id;
      if (old_elem->node_name != nullptr) {
        new_elem->node_name = BLI_strdup(old_elem->node_name);
      }
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
    case VIEWER_PATH_ELEM_TYPE_NODE: {
      const auto *a_elem = reinterpret_cast<const NodeViewerPathElem *>(a);
      const auto *b_elem = reinterpret_cast<const NodeViewerPathElem *>(b);
      return a_elem->node_id == b_elem->node_id;
    }
  }
  return false;
}

void BKE_viewer_path_elem_free(ViewerPathElem *elem)
{
  switch (ViewerPathElemType(elem->type)) {
    case VIEWER_PATH_ELEM_TYPE_ID: {
      break;
    }
    case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
      auto *typed_elem = reinterpret_cast<ModifierViewerPathElem *>(elem);
      MEM_SAFE_FREE(typed_elem->modifier_name);
      break;
    }
    case VIEWER_PATH_ELEM_TYPE_NODE: {
      auto *typed_elem = reinterpret_cast<NodeViewerPathElem *>(elem);
      MEM_SAFE_FREE(typed_elem->node_name);
      break;
    }
  }
  MEM_freeN(elem);
}
