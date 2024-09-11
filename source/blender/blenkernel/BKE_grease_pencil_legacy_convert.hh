/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct BlendfileLinkAppendContext;
struct BlendFileReadReport;
struct GreasePencil;
struct Main;
struct LineartGpencilModifierData;
struct GreasePencilLineartModifierData;

namespace blender::bke::greasepencil::convert {

/** Main entry point to convert all legacy GPData into GreasePencil data and objects. */
void legacy_main(Main &bmain,
                 BlendfileLinkAppendContext *lapp_context,
                 BlendFileReadReport &reports);

void lineart_wrap_v3(const LineartGpencilModifierData *lmd_legacy,
                     GreasePencilLineartModifierData *lmd);
void lineart_unwrap_v3(LineartGpencilModifierData *lmd_legacy,
                       const GreasePencilLineartModifierData *lmd);

}  // namespace blender::bke::greasepencil::convert
