/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void boolean_math_and(bool a, bool b, out bool result)
{
  result = (a && b);
}

[[node]]
void boolean_math_or(bool a, bool b, out bool result)
{
  result = (a || b);
}

[[node]]
void boolean_math_not(bool a, out bool result)
{
  result = !a;
}

[[node]]
void boolean_math_nand(bool a, bool b, out bool result)
{
  result = !(a && b);
}

[[node]]
void boolean_math_nor(bool a, bool b, out bool result)
{
  result = !(a || b);
}

[[node]]
void boolean_math_xnor(bool a, bool b, out bool result)
{
  result = (a == b);
}

[[node]]
void boolean_math_xor(bool a, bool b, out bool result)
{
  result = (a != b);
}

[[node]]
void boolean_math_imply(bool a, bool b, out bool result)
{
  result = (!a || b);
}

[[node]]
void boolean_math_nimply(bool a, bool b, out bool result)
{
  result = (a && !b);
}
