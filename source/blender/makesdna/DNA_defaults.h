/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

uint8_t *_DNA_struct_default_alloc_impl(const uint8_t *data_src,
                                        size_t size,
                                        const char *alloc_str);

/**
 * Wrap with macro that casts correctly.
 */
#define DNA_struct_default_get(struct_name) \
  (const struct_name *)DNA_default_table[SDNA_TYPE_FROM_STRUCT(struct_name)]

#define DNA_struct_default_alloc(struct_name) \
  (struct_name *)_DNA_struct_default_alloc_impl( \
      (const uint8_t *)DNA_default_table[SDNA_TYPE_FROM_STRUCT(struct_name)], \
      sizeof(struct_name), \
      __func__)

#ifdef __cplusplus
}
#endif
