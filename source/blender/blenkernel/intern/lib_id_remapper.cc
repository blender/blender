/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_ID.h"

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"

#include "MEM_guardedalloc.h"

namespace blender::bke::id {

void IDRemapper::add(ID *old_id, ID *new_id)
{
  BLI_assert(old_id != nullptr);
  BLI_assert(new_id == nullptr || this->allow_idtype_mismatch ||
             (GS(old_id->name) == GS(new_id->name)));
  BLI_assert(BKE_idtype_idcode_to_idfilter(GS(old_id->name)) != 0);

  mappings_.add(old_id, new_id);
  source_types_ |= BKE_idtype_idcode_to_idfilter(GS(old_id->name));
}

void IDRemapper::add_overwrite(ID *old_id, ID *new_id)
{
  BLI_assert(old_id != nullptr);
  BLI_assert(new_id == nullptr || this->allow_idtype_mismatch ||
             (GS(old_id->name) == GS(new_id->name)));
  BLI_assert(BKE_idtype_idcode_to_idfilter(GS(old_id->name)) != 0);

  mappings_.add_overwrite(old_id, new_id);
  source_types_ |= BKE_idtype_idcode_to_idfilter(GS(old_id->name));
}

IDRemapperApplyResult IDRemapper::get_mapping_result(ID *id,
                                                     IDRemapperApplyOptions options,
                                                     const ID *id_self) const
{
  if (!mappings_.contains(id)) {
    return ID_REMAP_RESULT_SOURCE_UNAVAILABLE;
  }
  const ID *new_id = mappings_.lookup(id);
  if ((options & ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF) != 0 && id_self == new_id) {
    new_id = nullptr;
  }
  if (new_id == nullptr) {
    return ID_REMAP_RESULT_SOURCE_UNASSIGNED;
  }

  return ID_REMAP_RESULT_SOURCE_REMAPPED;
}

IDRemapperApplyResult IDRemapper::apply(ID **r_id_ptr,
                                        IDRemapperApplyOptions options,
                                        ID *id_self) const
{
  BLI_assert(r_id_ptr != nullptr);
  BLI_assert_msg(
      ((options & ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF) == 0 || id_self != nullptr),
      "ID_REMAP_APPLY_WHEN_REMAPPING_TO_SELF requires a non-null `id_self` parameter.");

  if (*r_id_ptr == nullptr) {
    return ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE;
  }

  if (!mappings_.contains(*r_id_ptr)) {
    return ID_REMAP_RESULT_SOURCE_UNAVAILABLE;
  }

  if (options & ID_REMAP_APPLY_UPDATE_REFCOUNT) {
    id_us_min(*r_id_ptr);
  }

  *r_id_ptr = mappings_.lookup(*r_id_ptr);
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

const StringRefNull IDRemapper::result_to_string(const IDRemapperApplyResult result)
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
  }
  BLI_assert_unreachable();
  return "";
}

void IDRemapper::print(void) const
{
  auto print_cb = [](ID *old_id, ID *new_id, void * /*user_data*/) {
    if (old_id != nullptr && new_id != nullptr) {
      printf("Remap %s(%p) to %s(%p)\n", old_id->name, old_id, new_id->name, new_id);
    }
    if (old_id != nullptr && new_id == nullptr) {
      printf("Unassign %s(%p)\n", old_id->name, old_id);
    }
  };
  this->iter(print_cb, nullptr);
}

}  // namespace blender::bke::id
