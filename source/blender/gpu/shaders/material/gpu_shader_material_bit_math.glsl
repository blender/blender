/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void bit_math_and(float a, float b, out float result)
{
  result = float(int(a) & int(b));
}

[[node]]
void bit_math_or(float a, float b, out float result)
{
  result = float(int(a) | int(b));
}

[[node]]
void bit_math_xor(float a, float b, out float result)
{
  result = float(int(a) ^ int(b));
}

[[node]]
void bit_math_not(float a, out float result)
{
  result = float(~int(a));
}

[[node]]
void bit_math_shift(float a, float shift_amount, out float result)
{
  const uint value = uint(int(a));
  const int shift = int(shift_amount);

  if (shift < -31 || shift > 31) {
    result = 0.0f;
  }
  else {
    if (shift >= 0) {
      result = float(int(value << uint(shift)));
    }
    else {
      result = float(int(value >> uint(-shift)));
    }
  }
}

[[node]]
void bit_math_rotate(float a, float shift_amount, out float result)
{
  const uint value = uint(int(a));
  int shift = int(shift_amount) % 32;

  if (shift < 0) {
    shift += 32;
  }
  if (shift == 0) {
    result = float(int(value));
  }
  else {
    result = float(int((value << uint(shift)) | (value >> uint(32 - shift))));
  }
}
