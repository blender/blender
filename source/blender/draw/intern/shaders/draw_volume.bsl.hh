/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_shader_shared.hh"

namespace draw {

struct Volume {
  [[uniform(DRW_VOLUME_UBO_SLOT), frequency(BATCH)]] VolumeInfos &drw_volume;
};

}  // namespace draw
