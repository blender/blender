/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

[[node]]
void boolean_math_and(bool a, bool b, bool &result)
{
  result = (a && b);
}

[[node]]
void boolean_math_or(bool a, bool b, bool &result)
{
  result = (a || b);
}

[[node]]
void boolean_math_not(bool a, bool &result)
{
  result = !a;
}

[[node]]
void boolean_math_nand(bool a, bool b, bool &result)
{
  result = !(a && b);
}

[[node]]
void boolean_math_nor(bool a, bool b, bool &result)
{
  result = !(a || b);
}

[[node]]
void boolean_math_xnor(bool a, bool b, bool &result)
{
  result = (a == b);
}

[[node]]
void boolean_math_xor(bool a, bool b, bool &result)
{
  result = (a != b);
}

[[node]]
void boolean_math_imply(bool a, bool b, bool &result)
{
  result = (!a || b);
}

[[node]]
void boolean_math_nimply(bool a, bool b, bool &result)
{
  result = (a && !b);
}
