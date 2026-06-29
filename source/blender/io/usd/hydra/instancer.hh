/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/usd/sdf/path.h>

#include "BLI_span.hh"

namespace blender::io::hydra {

/** Build an #HdInstancedBySchema data source binding a prototype to its instancer. */
pxr::HdContainerDataSourceHandle build_instanced_by_data_source(
    const pxr::SdfPath &instancer_path, const pxr::SdfPath &prototype_root);

/** Build the aggregate instancer prim data source. */
pxr::HdContainerDataSourceHandle build_instancer_prim_data_source(
    Span<pxr::SdfPath> prototypes,
    Span<pxr::VtIntArray> instance_indices,
    const pxr::VtMatrix4dArray &transforms);

}  // namespace blender::io::hydra
