/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

/**
 * Decode URL (i.e. converts `file:///a%20b/test` to `file:///a b/test`)
 *
 * \param buf_dst: Buffer for decoded URL.
 * \param buf_dst_maxlen: Size of output buffer.
 * \param buf_src: Input encoded buffer to be decoded.
 */
void GHOST_URL_decode(char *buf_dst, int buf_dst_size, const char *buf_src);
/**
 * A version of #GHOST_URL_decode that allocates the string & returns it.
 *
 * \param buf_src: Input encoded buffer to be decoded.
 * \return The decoded output buffer.
 */
char *GHOST_URL_decode_alloc(const char *buf_src);
