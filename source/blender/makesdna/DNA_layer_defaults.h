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
\
    .flag = VIEW_LAYER_RENDER | VIEW_LAYER_FREESTYLE, \
\
    /* Pure rendering pipeline settings. */ \
    .layflag = SCE_LAY_FLAG_DEFAULT, \
    .passflag = SCE_PASS_COMBINED, \
    .pass_alpha_threshold = 0.5f, \
    .cryptomatte_levels = 6, \
    .cryptomatte_flag = VIEW_LAYER_CRYPTOMATTE_ACCURATE, \
  }

/** \} */

/* clang-format on */
