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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

#ifndef __WCWIDTH_H__
#define __WCWIDTH_H__

#ifndef __cplusplus
#  if defined(__APPLE__) || defined(__NetBSD__)
/* The <uchar.h> standard header is missing on macOS. */
#include <stddef.h>
typedef unsigned int char32_t;
#  else
#    include <uchar.h>
#  endif
#endif

int mk_wcwidth(char32_t ucs);
int mk_wcswidth(const char32_t *pwcs, size_t n);
int mk_wcwidth_cjk(char32_t ucs);
int mk_wcswidth_cjk(const char32_t *pwcs, size_t n);

#endif
