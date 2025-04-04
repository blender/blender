/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup intern_uri
 */

/**
 * \brief Encodes a string into URL format by converting special characters into percent-encoded
 * sequences.
 *
 * This function iterates over the provided C-string and replaces non-alphanumeric characters
 * (except for '-', '_', '.', '~') with their hexadecimal representations prefixed with '%'.
 * Spaces are converted into '+', following the conventions of URL encoding for forms
 * (application/x-www-form-urlencoded).
 *
 * \param str: The input C-string to be URL-encoded.
 * \param dst: The output buffer where the URL-encoded string will be stored.
 * \param dst_size: The size of the output buffer `dst`.
 * \return: `true` if encoding was successful, or `false` if the output buffer was insufficient.
 */
bool url_encode(const char *str, char *dst, size_t dst_size);
