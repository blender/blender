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
 * \ingroup imbdds
 */

#pragma once

#include "../../IMB_imbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

bool imb_is_a_dds(const unsigned char *mem, const size_t size);
bool imb_save_dds(struct ImBuf *ibuf, const char *name, int flags);
struct ImBuf *imb_load_dds(const unsigned char *mem,
                           size_t size,
                           int flags,
                           char colorspace[IM_MAX_SPACE]);

#ifdef __cplusplus
}
#endif
