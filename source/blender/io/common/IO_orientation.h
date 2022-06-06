/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "RNA_types.h"

typedef enum {
  IO_AXIS_X = 0,
  IO_AXIS_Y = 1,
  IO_AXIS_Z = 2,
  IO_AXIS_NEGATIVE_X = 3,
  IO_AXIS_NEGATIVE_Y = 4,
  IO_AXIS_NEGATIVE_Z = 5,
} eIOAxis;

extern const EnumPropertyItem io_transform_axis[];
