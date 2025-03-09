/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Parameter value lists from OpenImageIO are used to store custom properties
 * on various data, which can then later be used in shaders. */

#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>

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
static constexpr TypeDesc TypeFloatArray4(TypeDesc::FLOAT,
                                          TypeDesc::SCALAR,
                                          TypeDesc::NOSEMANTICS,
                                          4);

using OIIO::ustring;
using OIIO::ustringhash;

CCL_NAMESPACE_END
