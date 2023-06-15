/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"
#include "scene/attribute.h"

#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/types.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

void ApplyPrimvars(CCL_NS::AttributeSet &attributes,
                   const CCL_NS::ustring &name,
                   PXR_NS::VtValue value,
                   CCL_NS::AttributeElement elem,
                   CCL_NS::AttributeStandard std);

HDCYCLES_NAMESPACE_CLOSE_SCOPE
