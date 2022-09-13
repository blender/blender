/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "util/transform.h"

CCL_NAMESPACE_BEGIN

void transform_inverse_cpu_sse41(const Transform &tfm, Transform &itfm)
{
  itfm = transform_inverse_impl(tfm);
}

CCL_NAMESPACE_END
