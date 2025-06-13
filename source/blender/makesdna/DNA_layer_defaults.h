/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Scene ViewLayer
 * \{ */

#define _DNA_DEFAULT_ViewLayerEEVEE \
  { \
    .ambient_occlusion_distance = 10.0f, \
  }

#define _DNA_DEFAULT_ViewLayer \
  { \
    .eevee = _DNA_DEFAULT_ViewLayerEEVEE, \
  }

/** \} */

/* clang-format on */
