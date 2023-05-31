/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
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

/**
 * Generate mapping from ID type/name to ID pointer for given \a bmain.
 *
 * \note When used during undo/redo, there is no guaranty that ID pointers from UI area are not
 * pointing to freed memory (when some IDs have been deleted). To avoid crashes in those cases, one
 * can provide the 'old' (aka current) Main database as reference. #BKE_main_idmap_lookup_id will
 * then check that given ID does exist in \a old_bmain before trying to use it.
 *
 * \param create_valid_ids_set: If \a true, generate a reference to prevent freed memory accesses.
 * \param old_bmain: If not NULL, its IDs will be added the valid references set.
 */
struct IDNameLib_Map *BKE_main_idmap_create(struct Main *bmain,
                                            bool create_valid_ids_set,
                                            struct Main *old_bmain,
                                            int idmap_types) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
void BKE_main_idmap_destroy(struct IDNameLib_Map *id_map) ATTR_NONNULL();

void BKE_main_idmap_insert_id(struct IDNameLib_Map *id_map, struct ID *id) ATTR_NONNULL();
void BKE_main_idmap_remove_id(struct IDNameLib_Map *id_map, struct ID *id) ATTR_NONNULL();

struct Main *BKE_main_idmap_main_get(struct IDNameLib_Map *id_map) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

struct ID *BKE_main_idmap_lookup_name(struct IDNameLib_Map *id_map,
                                      short id_type,
                                      const char *name,
                                      const struct Library *lib) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 3);
struct ID *BKE_main_idmap_lookup_id(struct IDNameLib_Map *id_map,
                                    const struct ID *id) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);

struct ID *BKE_main_idmap_lookup_uuid(struct IDNameLib_Map *id_map,
                                      uint session_uuid) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif
