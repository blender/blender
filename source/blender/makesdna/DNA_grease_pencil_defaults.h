/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil Struct
 * \{ */

#define _DNA_DEFAULT_GreasePencilOnionSkinningSettings \
  { \
    .opacity = 0.5f, \
    .mode = GP_ONION_SKINNING_MODE_RELATIVE, \
    .flag = (GP_ONION_SKINNING_USE_FADE | GP_ONION_SKINNING_USE_CUSTOM_COLORS), \
    .filter = GREASE_PENCIL_ONION_SKINNING_FILTER_ALL, \
    .num_frames_before = 1, \
    .num_frames_after = 1, \
    .color_before = {0.145098f, 0.419608f, 0.137255f}, \
    .color_after = {0.125490f, 0.082353f, 0.529412f},\
  }


#define _DNA_DEFAULT_GreasePencil \
  { \
    .flag = GREASE_PENCIL_ANIM_CHANNEL_EXPANDED, \
    .onion_skinning_settings = _DNA_DEFAULT_GreasePencilOnionSkinningSettings, \
  }

/** \} */

/* clang-format on */
