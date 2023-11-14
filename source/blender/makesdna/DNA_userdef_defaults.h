/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name bUserExtensionRepo Struct
 * \{ */

#define _DNA_DEFAULT_bUserExtensionRepo \
  { \
    .name = {'\0'}, \
    .module = {'\0'}, \
    .dirpath = {'\0'}, \
    .remote_path = {'\0'}, \
    .flag = 0, \
  }

/** \} */

/* clang-format on */
