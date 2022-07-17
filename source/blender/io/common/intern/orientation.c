/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "RNA_types.h"

#include "IO_orientation.h"

const EnumPropertyItem io_transform_axis[] = {
    {IO_AXIS_X, "X", 0, "X", "Positive X axis"},
    {IO_AXIS_Y, "Y", 0, "Y", "Positive Y axis"},
    {IO_AXIS_Z, "Z", 0, "Z", "Positive Z axis"},
    {IO_AXIS_NEGATIVE_X, "NEGATIVE_X", 0, "-X", "Negative X axis"},
    {IO_AXIS_NEGATIVE_Y, "NEGATIVE_Y", 0, "-Y", "Negative Y axis"},
    {IO_AXIS_NEGATIVE_Z, "NEGATIVE_Z", 0, "-Z", "Negative Z axis"},
    {0, NULL, 0, NULL, NULL}};
