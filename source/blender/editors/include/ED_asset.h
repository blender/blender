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
 * \ingroup editors
 */

#ifndef __ED_ASSET_H__
#define __ED_ASSET_H__

#ifdef __cplusplus
extern "C" {
#endif

bool ED_asset_mark_id(const struct bContext *C, struct ID *id);
bool ED_asset_clear_id(struct ID *id);

bool ED_asset_can_make_single_from_context(const struct bContext *C);

void ED_operatortypes_asset(void);

#ifdef __cplusplus
}
#endif

#endif /* __ED_ASSET_H__ */
