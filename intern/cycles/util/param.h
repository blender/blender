/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Parameter value lists from OpenImageIO are used to store custom properties
 * on various data, which can then later be used in shaders. */

#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>

#include "util/types_spherical_harmonics.h"

CCL_NAMESPACE_BEGIN

using OIIO::ParamValue;

using OIIO::TypeColor;
using OIIO::TypeDesc;
using OIIO::TypeFloat;
using OIIO::TypeFloat2;
using OIIO::TypeFloat4;
using OIIO::TypeInt;
using OIIO::TypeMatrix;
using OIIO::TypeNormal;
using OIIO::TypePoint;
using OIIO::TypeString;
using OIIO::TypeUnknown;
using OIIO::TypeVector;

static constexpr TypeDesc TypeRGBA(TypeDesc::FLOAT, TypeDesc::VEC4, TypeDesc::COLOR);
/* Consider quaternion an array of scalars to differentiate the type from TypeFloat4 and TypeRGBA.
 */
static constexpr TypeDesc TypeQuaternion(TypeDesc::FLOAT,
                                         TypeDesc::SCALAR,
                                         TypeDesc::NOSEMANTICS,
                                         4);
static_assert(TypeQuaternion != TypeFloat4);
static_assert(TypeQuaternion != TypeRGBA);
static constexpr TypeDesc TypePackedSphericalHarmonics(TypeDesc::INT8,
                                                       TypeDesc::VEC3,
                                                       TypeDesc::NOSEMANTICS,
                                                       PackedSphericalHarmonics::MAX_COEFFICIENTS);

using OIIO::ustring;
using OIIO::ustringhash;

CCL_NAMESPACE_END
