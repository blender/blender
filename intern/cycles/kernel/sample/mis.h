/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "util/defines.h"

CCL_NAMESPACE_BEGIN

/* Multiple importance sampling utilities. */

ccl_device float balance_heuristic(const float a, const float b)
{
  return (a) / (a + b);
}

ccl_device float balance_heuristic_3(const float a, const float b, float c)
{
  return (a) / (a + b + c);
}

ccl_device float power_heuristic(const float a, const float b)
{
  return (a * a) / (a * a + b * b);
}

ccl_device float power_heuristic_3(const float a, const float b, float c)
{
  return (a * a) / (a * a + b * b + c * c);
}

ccl_device float max_heuristic(const float a, const float b)
{
  return (a > b) ? 1.0f : 0.0f;
}

CCL_NAMESPACE_END
