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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#ifndef __BKE_IDCODE_H__
#define __BKE_IDCODE_H__

/** \file
 * \ingroup bke
 */

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *BKE_idcode_to_name(short idcode);
const char *BKE_idcode_to_name_plural(short idcode);
const char *BKE_idcode_to_translation_context(short idcode);
short BKE_idcode_from_name(const char *name);
bool BKE_idcode_is_linkable(short idcode);
bool BKE_idcode_is_valid(short idcode);

uint64_t BKE_idcode_to_idfilter(const short idcode);
short BKE_idcode_from_idfilter(const uint64_t idfilter);

int BKE_idcode_to_index(const short idcode);
short BKE_idcode_from_index(const int index);

short BKE_idcode_iter_step(int *index);

#ifdef __cplusplus
}
#endif

#endif
