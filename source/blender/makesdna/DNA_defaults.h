
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

/** \file
 * \ingroup DNA
 *
 * \see dna_defaults.c for details on how to use this system.
 */

#pragma once

#include "BLI_utildefines.h"

#include "dna_type_offsets.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const void *DNA_default_table[SDNA_TYPE_MAX];

char *_DNA_struct_default_alloc_impl(const char *data_src, size_t size, const char *alloc_str);

/**
 * Wrap with macro that casts correctly.
 */
#define DNA_struct_default_get(struct_name) \
  (const struct_name *)DNA_default_table[SDNA_TYPE_FROM_STRUCT(struct_name)]

#define DNA_struct_default_alloc(struct_name) \
  (struct_name *)_DNA_struct_default_alloc_impl( \
      DNA_default_table[SDNA_TYPE_FROM_STRUCT(struct_name)], sizeof(struct_name), __func__)

#ifdef __cplusplus
}
#endif
