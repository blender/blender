/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <OSL/oslconfig.h>

CCL_NAMESPACE_BEGIN

#if OSL_LIBRARY_VERSION_CODE >= 11302
typedef OSL::ustringhash OSLUStringHash;
typedef OSL::ustringrep OSLUStringRep;

static inline OSL::ustring to_ustring(OSLUStringHash h)
{
  return OSL::ustring::from_hash(h.hash());
}

#else
typedef OSL::ustring OSLUStringHash;
typedef OSL::ustring OSLUStringRep;

static inline OSL::ustring to_ustring(OSLUStringHash h)
{
  return h;
}
#endif

CCL_NAMESPACE_END
