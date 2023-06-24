/* SPDX-FileCopyrightText: 2022-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifdef __cplusplus
#  undef wl_array_for_each
/**
 * This macro causes a warning for C++ code, define our own.
 * See: https://gitlab.freedesktop.org/wayland/wayland/-/issues/34
 */
#  define WL_ARRAY_FOR_EACH(pos, array) \
    for (pos = (decltype(pos))((array)->data); \
         (const char *)pos < ((const char *)(array)->data + (array)->size); \
         (pos)++)
#endif
