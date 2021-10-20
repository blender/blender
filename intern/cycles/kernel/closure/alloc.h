/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device ccl_private ShaderClosure *closure_alloc(ccl_private ShaderData *sd,
                                                    int size,
                                                    ClosureType type,
                                                    float3 weight)
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
                                                        float3 weight)
{
  kernel_assert(isfinite3_safe(weight));

  const float sample_weight = fabsf(average(weight));

  /* Use comparison this way to help dealing with non-finite weight: if the average is not finite
   * we will not allocate new closure. */
  if (sample_weight >= CLOSURE_WEIGHT_CUTOFF) {
    ccl_private ShaderClosure *sc = closure_alloc(sd, size, CLOSURE_NONE_ID, weight);
    if (sc == NULL) {
      return NULL;
    }

    sc->sample_weight = sample_weight;

    return sc;
  }

  return NULL;
}

#ifdef __OSL__
ccl_device_inline ShaderClosure *bsdf_alloc_osl(ShaderData *sd,
                                                int size,
                                                float3 weight,
                                                void *data)
{
  kernel_assert(isfinite3_safe(weight));

  const float sample_weight = fabsf(average(weight));

  /* Use comparison this way to help dealing with non-finite weight: if the average is not finite
   * we will not allocate new closure. */
  if (sample_weight >= CLOSURE_WEIGHT_CUTOFF) {
    ShaderClosure *sc = closure_alloc(sd, size, CLOSURE_NONE_ID, weight);
    if (!sc) {
      return NULL;
    }

    memcpy((void *)sc, data, size);

    sc->weight = weight;
    sc->sample_weight = sample_weight;

    return sc;
  }

  return NULL;
}
#endif

CCL_NAMESPACE_END
