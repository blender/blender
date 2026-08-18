/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void bit_math_and(int a, int b, out int result)
{
  result = a & b;
}

[[node]]
void bit_math_or(int a, int b, out int result)
{
  result = a | b;
}

[[node]]
void bit_math_xor(int a, int b, out int result)
{
  result = a ^ b;
}

[[node]]
void bit_math_not(int a, out int result)
{
  result = ~a;
}

[[node]]
void bit_math_shift(int a, int shift_amount, out int result)
{
  const uint value = uint(a);
  const int shift = shift_amount;

  if (shift < -31 || shift > 31) {
    result = 0;
  }
  else {
    if (shift >= 0) {
      result = int(value << uint(shift));
    }
    else {
      result = int(value >> uint(-shift));
    }
  }
}

[[node]]
void bit_math_rotate(int a, int shift_amount, out int result)
{
  const uint value = uint(a);
  int shift = shift_amount % 32;

  if (shift < 0) {
    shift += 32;
  }
  if (shift == 0) {
    result = int(value);
  }
  else {
    result = int((value << uint(shift)) | (value >> uint(32 - shift)));
  }
}
