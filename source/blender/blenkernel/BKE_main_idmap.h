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
 */
#pragma once

/** \file
 * \ingroup bke
 *
 * API to generate and use a mapping from [ID type & name] to [id pointer], within a given Main
 * data-base.
 *
 * \note `BKE_main` files are for operations over the Main database itself, or generating extra
 * temp data to help working with it. Those should typically not affect the data-blocks themselves.
 *
 * \section Function Names
 *
 * - `BKE_main_idmap_` Should be used for functions in that file.
 */

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct IDNameLib_Map;
struct Main;

enum {
  MAIN_IDMAP_TYPE_NAME = 1 << 0,
  MAIN_IDMAP_TYPE_UUID = 1 << 1,
};

struct IDNameLib_Map *BKE_main_idmap_create(struct Main *bmain,
                                            const bool create_valid_ids_set,
                                            struct Main *old_bmain,
                                            const int idmap_types) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
void BKE_main_idmap_destroy(struct IDNameLib_Map *id_typemap) ATTR_NONNULL();
struct Main *BKE_main_idmap_main_get(struct IDNameLib_Map *id_typemap) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
struct ID *BKE_main_idmap_lookup_name(struct IDNameLib_Map *id_typemap,
                                      short id_type,
                                      const char *name,
                                      const struct Library *lib) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 3);
struct ID *BKE_main_idmap_lookup_id(struct IDNameLib_Map *id_typemap,
                                    const struct ID *id) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);

struct ID *BKE_main_idmap_lookup_uuid(struct IDNameLib_Map *id_typemap,
                                      const uint session_uuid) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif
