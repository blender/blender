/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_ID.h"

#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_remap.h"

#include "MEM_guardedalloc.h"

#include "BLI_map.hh"

using IDTypeFilter = uint64_t;

namespace blender::bke::id::remapper {
struct IDRemapper {
 private:
  Map<ID *, ID *> mappings;
  IDTypeFilter source_types = 0;

 public:
  void clear()
  {
    mappings.clear();
    source_types = 0;
  }

  bool is_empty() const
  {
    return mappings.is_empty();
  }

  void add(ID *old_id, ID *new_id)
  {
    BLI_assert(old_id != nullptr);
    BLI_assert(new_id == nullptr || (GS(old_id->name) == GS(new_id->name)));
    mappings.add(old_id, new_id);
    BLI_assert(BKE_idtype_idcode_to_idfilter(GS(old_id->name)) != 0);
    source_types |= BKE_idtype_idcode_to_idfilter(GS(old_id->name));
  }

  void add_overwrite(ID *old_id, ID *new_id)
  {
    BLI_assert(old_id != nullptr);
    BLI_assert(new_id == nullptr || (GS(old_id->name) == GS(new_id->name)));
    mappings.add_overwrite(old_id, new_id);
    BLI_assert(BKE_idtype_idcode_to_idfilter(GS(old_id->name)) != 0);
    source_types |= BKE_idtype_idcode_to_idfilter(GS(old_id->name));
  }

  bool contains_mappings_for_any(IDTypeFilter filter) const
  {
    return (source_types & filter) != 0;
  }

  IDRemapperApplyResult get_mapping_result(ID *id,
                                           IDRemapperApplyOptions options,
                                           const ID *id_self) const
  {
    if (!mappings.contains(id)) {
      return ID_REMAP_RESULT_SOURCE_UNAVAILABLE;
    }
    const ID *new_id = mappings.lookup(id);
    if ((options & ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF) != 0 && id_self == new_id) {
      new_id = nullptr;
    }
    if (new_id == nullptr) {
      return ID_REMAP_RESULT_SOURCE_UNASSIGNED;
    }

    return ID_REMAP_RESULT_SOURCE_REMAPPED;
  }

  IDRemapperApplyResult apply(ID **r_id_ptr, IDRemapperApplyOptions options, ID *id_self) const
  {
    BLI_assert(r_id_ptr != nullptr);
    if (*r_id_ptr == nullptr) {
      return ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE;
    }

    if (!mappings.contains(*r_id_ptr)) {
      return ID_REMAP_RESULT_SOURCE_UNAVAILABLE;
    }

    if (options & ID_REMAP_APPLY_UPDATE_REFCOUNT) {
      id_us_min(*r_id_ptr);
    }

    *r_id_ptr = mappings.lookup(*r_id_ptr);
    if (options & ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF && *r_id_ptr == id_self) {
      *r_id_ptr = nullptr;
    }
    if (*r_id_ptr == nullptr) {
      return ID_REMAP_RESULT_SOURCE_UNASSIGNED;
    }

    if (options & ID_REMAP_APPLY_UPDATE_REFCOUNT) {
      /* Do not handle LIB_TAG_INDIRECT/LIB_TAG_EXTERN here. */
      id_us_plus_no_lib(*r_id_ptr);
    }

    if (options & ID_REMAP_APPLY_ENSURE_REAL) {
      id_us_ensure_real(*r_id_ptr);
    }
    return ID_REMAP_RESULT_SOURCE_REMAPPED;
  }

  void iter(IDRemapperIterFunction func, void *user_data) const
  {
    for (auto item : mappings.items()) {
      func(item.key, item.value, user_data);
    }
  }
};

}  // namespace blender::bke::id::remapper

/** \brief wrap CPP IDRemapper to a C handle. */
static IDRemapper *wrap(blender::bke::id::remapper::IDRemapper *remapper)
{
  return static_cast<IDRemapper *>(static_cast<void *>(remapper));
}

/** \brief wrap C handle to a CPP IDRemapper. */
static blender::bke::id::remapper::IDRemapper *unwrap(IDRemapper *remapper)
{
  return static_cast<blender::bke::id::remapper::IDRemapper *>(static_cast<void *>(remapper));
}

/** \brief wrap C handle to a CPP IDRemapper. */
static const blender::bke::id::remapper::IDRemapper *unwrap(const IDRemapper *remapper)
{
  return static_cast<const blender::bke::id::remapper::IDRemapper *>(
      static_cast<const void *>(remapper));
}

extern "C" {

IDRemapper *BKE_id_remapper_create(void)
{
  blender::bke::id::remapper::IDRemapper *remapper =
      MEM_new<blender::bke::id::remapper::IDRemapper>(__func__);
  return wrap(remapper);
}

void BKE_id_remapper_free(IDRemapper *id_remapper)
{
  blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  MEM_delete<blender::bke::id::remapper::IDRemapper>(remapper);
}

void BKE_id_remapper_clear(IDRemapper *id_remapper)
{
  blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  remapper->clear();
}

bool BKE_id_remapper_is_empty(const IDRemapper *id_remapper)
{
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  return remapper->is_empty();
}

void BKE_id_remapper_add(IDRemapper *id_remapper, ID *old_id, ID *new_id)
{
  blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  remapper->add(old_id, new_id);
}

void BKE_id_remapper_add_overwrite(IDRemapper *id_remapper, ID *old_id, ID *new_id)
{
  blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  remapper->add_overwrite(old_id, new_id);
}

bool BKE_id_remapper_has_mapping_for(const IDRemapper *id_remapper, uint64_t type_filter)
{
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  return remapper->contains_mappings_for_any(type_filter);
}

IDRemapperApplyResult BKE_id_remapper_get_mapping_result(const IDRemapper *id_remapper,
                                                         ID *id,
                                                         IDRemapperApplyOptions options,
                                                         const ID *id_self)
{
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  return remapper->get_mapping_result(id, options, id_self);
}

IDRemapperApplyResult BKE_id_remapper_apply_ex(const IDRemapper *id_remapper,
                                               ID **r_id_ptr,
                                               const IDRemapperApplyOptions options,
                                               ID *id_self)
{
  BLI_assert_msg((options & ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF) == 0 ||
                     id_self != nullptr,
                 "ID_REMAP_APPLY_WHEN_REMAPPING_TO_SELF requires id_self parameter.");
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  return remapper->apply(r_id_ptr, options, id_self);
}

IDRemapperApplyResult BKE_id_remapper_apply(const IDRemapper *id_remapper,
                                            ID **r_id_ptr,
                                            const IDRemapperApplyOptions options)
{
  BLI_assert_msg((options & ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF) == 0,
                 "ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF requires id_self parameter. Use "
                 "`BKE_id_remapper_apply_ex`.");
  return BKE_id_remapper_apply_ex(id_remapper, r_id_ptr, options, nullptr);
}

void BKE_id_remapper_iter(const IDRemapper *id_remapper,
                          IDRemapperIterFunction func,
                          void *user_data)
{
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  remapper->iter(func, user_data);
}

const char *BKE_id_remapper_result_string(const IDRemapperApplyResult result)
{
  switch (result) {
    case ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE:
      return "not_mappable";
    case ID_REMAP_RESULT_SOURCE_UNAVAILABLE:
      return "unavailable";
    case ID_REMAP_RESULT_SOURCE_UNASSIGNED:
      return "unassigned";
    case ID_REMAP_RESULT_SOURCE_REMAPPED:
      return "remapped";
    default:
      BLI_assert_unreachable();
  }
  return "";
}

static void id_remapper_print_item_cb(ID *old_id, ID *new_id, void * /*user_data*/)
{
  if (old_id != nullptr && new_id != nullptr) {
    printf("Remap %s(%p) to %s(%p)\n", old_id->name, old_id, new_id->name, new_id);
  }
  if (old_id != nullptr && new_id == nullptr) {
    printf("Unassign %s(%p)\n", old_id->name, old_id);
  }
}

void BKE_id_remapper_print(const IDRemapper *id_remapper)
{
  BKE_id_remapper_iter(id_remapper, id_remapper_print_item_cb, nullptr);
}
}
