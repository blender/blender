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
#ifndef __BKE_LIBRARY_IDMAP_H__
#define __BKE_LIBRARY_IDMAP_H__

/** \file \ingroup bke
 */

#include "BLI_compiler_attrs.h"

struct ID;
struct IDNameLib_Map;
struct Main;

struct IDNameLib_Map *BKE_main_idmap_create(
        struct Main *bmain)
        ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void BKE_main_idmap_destroy(
        struct IDNameLib_Map *id_typemap)
        ATTR_NONNULL();
struct Main *BKE_main_idmap_main_get(
        struct IDNameLib_Map *id_typemap)
        ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
struct ID *BKE_main_idmap_lookup(
        struct IDNameLib_Map *id_typemap,
        short id_type, const char *name, const struct Library *lib)
        ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 3);
struct ID *BKE_main_idmap_lookup_id(
        struct IDNameLib_Map *id_typemap, const struct ID *id)
        ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);

#endif  /* __BKE_LIBRARY_IDMAP_H__ */
