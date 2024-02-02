/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <cstdio>

#include "BLI_sys_types.h"

/**
 * Compute MD5 message digest for 'len' bytes beginning at 'buffer'.
 * The result is always in little endian byte order,
 * so that a byte-wise output yields to the wanted ASCII representation of the message digest.
 */
void *BLI_hash_md5_buffer(const char *buffer, size_t len, void *resblock);

/**
 * Compute MD5 message digest for bytes read from 'stream'.
 * The resulting message digest number will be written into the 16 bytes beginning at 'resblock'.
 * \return Non-zero if an error occurred.
 */
int BLI_hash_md5_stream(FILE *stream, void *resblock);

char *BLI_hash_md5_to_hexdigest(const void *resblock, char r_hex_digest[33]);
