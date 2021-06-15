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

/** \file
 * \ingroup blenloader
 */

#pragma once

struct ARegion;
struct ListBase;

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion *do_versions_add_region_if_not_found(struct ListBase *regionbase,
                                                    int region_type,
                                                    const char *name,
                                                    int link_after_region_type);

#ifdef __cplusplus
}
#endif
