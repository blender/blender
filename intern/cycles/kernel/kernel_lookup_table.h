/*
 * Copyright 2011-2013 Blender Foundation
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

/* Interpolated lookup table access */

ccl_device float lookup_table_read(KernelGlobals kg, float x, int offset, int size)
{
  x = saturate(x) * (size - 1);

  int index = min(float_to_int(x), size - 1);
  int nindex = min(index + 1, size - 1);
  float t = x - index;

  float data0 = kernel_tex_fetch(__lookup_table, index + offset);
  if (t == 0.0f)
    return data0;

  float data1 = kernel_tex_fetch(__lookup_table, nindex + offset);
  return (1.0f - t) * data0 + t * data1;
}

ccl_device float lookup_table_read_2D(
    KernelGlobals kg, float x, float y, int offset, int xsize, int ysize)
{
  y = saturate(y) * (ysize - 1);

  int index = min(float_to_int(y), ysize - 1);
  int nindex = min(index + 1, ysize - 1);
  float t = y - index;

  float data0 = lookup_table_read(kg, x, offset + xsize * index, xsize);
  if (t == 0.0f)
    return data0;

  float data1 = lookup_table_read(kg, x, offset + xsize * nindex, xsize);
  return (1.0f - t) * data0 + t * data1;
}

CCL_NAMESPACE_END
