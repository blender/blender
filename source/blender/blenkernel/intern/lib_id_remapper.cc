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
 * The Original Code is Copyright (C) 2022 by Blender Foundation.
 */

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
    source_types |= BKE_idtype_idcode_to_idfilter(GS(old_id->name));
  }

  bool contains_mappings_for_any(IDTypeFilter filter) const
  {
    return (source_types & filter) != 0;
  }

  IDRemapperApplyResult apply(ID **r_id_ptr, IDRemapperApplyOptions options) const
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
    if (*r_id_ptr == nullptr) {
      return ID_REMAP_RESULT_SOURCE_UNASSIGNED;
    }

    if (options & ID_REMAP_APPLY_UPDATE_REFCOUNT) {
      id_us_plus(*r_id_ptr);
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

void BKE_id_remapper_clear(struct IDRemapper *id_remapper)
{
  blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  remapper->clear();
}

bool BKE_id_remapper_is_empty(const struct IDRemapper *id_remapper)
{
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  return remapper->is_empty();
}

void BKE_id_remapper_add(IDRemapper *id_remapper, ID *old_id, ID *new_id)
{
  blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  remapper->add(old_id, new_id);
}

bool BKE_id_remapper_has_mapping_for(const struct IDRemapper *id_remapper, uint64_t type_filter)
{
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  return remapper->contains_mappings_for_any(type_filter);
}

IDRemapperApplyResult BKE_id_remapper_apply(const IDRemapper *id_remapper,
                                            ID **r_id_ptr,
                                            const IDRemapperApplyOptions options)
{
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  return remapper->apply(r_id_ptr, options);
}

void BKE_id_remapper_iter(const struct IDRemapper *id_remapper,
                          IDRemapperIterFunction func,
                          void *user_data)
{
  const blender::bke::id::remapper::IDRemapper *remapper = unwrap(id_remapper);
  remapper->iter(func, user_data);
}
}
