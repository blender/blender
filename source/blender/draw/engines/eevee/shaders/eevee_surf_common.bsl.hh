/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_defines.hh"
#include "eevee_shadow_shared.hh"

namespace eevee {

struct GeomShadow {
  [[storage(SHADOW_RENDER_VIEW_BUF_SLOT,
            read)]] const ShadowRenderView (&render_view_buf)[SHADOW_VIEW_MAX];
};

}  // namespace eevee
