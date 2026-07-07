/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

namespace blender {

namespace gpu {
class Texture;
}
struct GPUViewport;
struct DRWContext;

namespace draw::color_management {

void viewport_color_management_set(GPUViewport &viewport, DRWContext &draw_ctx);

}  // namespace draw::color_management

}  // namespace blender
