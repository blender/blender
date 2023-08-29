/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device ccl_private ShaderClosure *closure_alloc(ccl_private ShaderData *sd,
                                                    int size,
                                                    ClosureType type,
                                                    Spectrum weight)
{
  kernel_assert(size <= sizeof(ShaderClosure));

  if (sd->num_closure_left == 0)
    return NULL;

  ccl_private ShaderClosure *sc = &sd->closure[sd->num_closure];

  sc->type = type;
  sc->weight = weight;

  sd->num_closure++;
  sd->num_closure_left--;

  return sc;
}

ccl_device ccl_private void *closure_alloc_extra(ccl_private ShaderData *sd, int size)
{
  /* Allocate extra space for closure that need more parameters. We allocate
   * in chunks of sizeof(ShaderClosure) starting from the end of the closure
   * array.
   *
   * This lets us keep the same fast array iteration over closures, as we
   * found linked list iteration and iteration with skipping to be slower. */
  int num_extra = ((size + sizeof(ShaderClosure) - 1) / sizeof(ShaderClosure));

  if (num_extra > sd->num_closure_left) {
    /* Remove previous closure if it was allocated. */
    sd->num_closure--;
    sd->num_closure_left++;
    return NULL;
  }

  sd->num_closure_left -= num_extra;
  return (ccl_private void *)(sd->closure + sd->num_closure + sd->num_closure_left);
}

ccl_device_inline ccl_private ShaderClosure *bsdf_alloc(ccl_private ShaderData *sd,
                                                        int size,
                                                        Spectrum weight)
{
  kernel_assert(isfinite_safe(weight));

  /* No negative weights allowed. */
  weight = max(weight, zero_float3());

  const float sample_weight = fabsf(average(weight));

  /* Use comparison this way to help dealing with non-finite weight: if the average is not finite
   * we will not allocate new closure. */
  if (sample_weight >= CLOSURE_WEIGHT_CUTOFF) {
    ccl_private ShaderClosure *sc = closure_alloc(sd, size, CLOSURE_NONE_ID, weight);
    if (!sc) {
      return NULL;
    }

    sc->sample_weight = sample_weight;

    return sc;
  }

  return NULL;
}

CCL_NAMESPACE_END
