/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Interpolated lookup table access */

ccl_device float lookup_table_read(KernelGlobals kg, float x, int offset, int size)
{
  x = saturatef(x) * (size - 1);

  int index = min(float_to_int(x), size - 1);
  int nindex = min(index + 1, size - 1);
  float t = x - index;

  float data0 = kernel_data_fetch(lookup_table, index + offset);
  if (t == 0.0f)
    return data0;

  float data1 = kernel_data_fetch(lookup_table, nindex + offset);
  return (1.0f - t) * data0 + t * data1;
}

ccl_device float lookup_table_read_2D(
    KernelGlobals kg, float x, float y, int offset, int xsize, int ysize)
{
  y = saturatef(y) * (ysize - 1);

  int index = min(float_to_int(y), ysize - 1);
  int nindex = min(index + 1, ysize - 1);
  float t = y - index;

  float data0 = lookup_table_read(kg, x, offset + xsize * index, xsize);
  if (t == 0.0f)
    return data0;

  float data1 = lookup_table_read(kg, x, offset + xsize * nindex, xsize);
  return (1.0f - t) * data0 + t * data1;
}

ccl_device float lookup_table_read_3D(
    KernelGlobals kg, float x, float y, float z, int offset, int xsize, int ysize, int zsize)
{
  z = saturatef(z) * (zsize - 1);

  int index = min(float_to_int(z), zsize - 1);
  int nindex = min(index + 1, zsize - 1);
  float t = z - index;

  float data0 = lookup_table_read_2D(kg, x, y, offset + xsize * ysize * index, xsize, ysize);
  if (t == 0.0f)
    return data0;

  float data1 = lookup_table_read_2D(kg, x, y, offset + xsize * ysize * nindex, xsize, ysize);
  return (1.0f - t) * data0 + t * data1;
}

CCL_NAMESPACE_END
