/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_PARAM_H__
#define __UTIL_PARAM_H__

/* Parameter value lists from OpenImageIO are used to store custom properties
 * on various data, which can then later be used in shaders. */

#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>

CCL_NAMESPACE_BEGIN

OIIO_NAMESPACE_USING

static constexpr TypeDesc TypeFloat2(TypeDesc::FLOAT, TypeDesc::VEC2);
static constexpr TypeDesc TypeRGBA(TypeDesc::FLOAT, TypeDesc::VEC4, TypeDesc::COLOR);
static constexpr TypeDesc TypeFloatArray4(TypeDesc::FLOAT,
                                          TypeDesc::SCALAR,
                                          TypeDesc::NOSEMANTICS,
                                          4);

CCL_NAMESPACE_END

#endif /* __UTIL_PARAM_H__ */
