/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/dataSource.h>

namespace blender {

struct Object;

namespace io::hydra {

struct EmittedObject;
struct PopulateContext;

/** Compose a light prim data source from light parameters, transform and visibility. */
pxr::HdContainerDataSourceHandle build_light_prim_data_source(
    const pxr::HdContainerDataSourceHandle &light_params,
    const pxr::GfMatrix4d &transform,
    bool visible);

/** Emit a light Sprim for a lamp object. */
void emit_light_object(PopulateContext &ctx, const Object *object, EmittedObject &emitted);

/** Emit a per-dupli light Sprim. Lights aren't gprims and don't go
 * through the instancer prototype mechanism. */
void emit_light_dupli(PopulateContext &ctx, const Object *source, const float dupli_mat[4][4]);

}  // namespace io::hydra
}  // namespace blender
