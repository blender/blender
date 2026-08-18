/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

int divide_floor(int a, int b)
{
  int d = a / b;
  int r = a % b;
  return (r != 0) ? (d - int((a < 0) != (b < 0))) : d;
}

int euclid_gcd(int a, int b)
{
  a = abs(a);
  b = abs(b);
  while (b != 0) {
    int t = b;
    b = a % b;
    a = t;
  }
  return a;
}

[[node]]
void integer_math_add(int a, int b, int c, out int result)
{
  result = a + b;
}

[[node]]
void integer_math_subtract(int a, int b, int c, out int result)
{
  result = a - b;
}

[[node]]
void integer_math_multiply(int a, int b, int c, out int result)
{
  result = a * b;
}

[[node]]
void integer_math_divide(int a, int b, int c, out int result)
{
  result = (b != 0) ? (a / b) : 0;
}

[[node]]
void integer_math_multiply_add(int a, int b, int c, out int result)
{
  result = a * b + c;
}

[[node]]
void integer_math_power(int base, int exponent, int c, out int result)
{
  result = int(pow(float(base), float(exponent)));
}

[[node]]
void integer_math_floored_modulo(int a, int b, int c, out int result)
{
  result = (b != 0) ? (((a % b) + b) % b) : 0;
}

[[node]]
void integer_math_absolute(int a, int b, int c, out int result)
{
  result = abs(a);
}

[[node]]
void integer_math_minimum(int a, int b, int c, out int result)
{
  result = min(a, b);
}

[[node]]
void integer_math_maximum(int a, int b, int c, out int result)
{
  result = max(a, b);
}

[[node]]
void integer_math_gcd(int a, int b, int c, out int result)
{
  result = euclid_gcd(a, b);
}

[[node]]
void integer_math_lcm(int a, int b, int c, out int result)
{
  int gcd = euclid_gcd(a, b);
  result = (gcd != 0) ? abs(a / gcd * b) : 0;
}

[[node]]
void integer_math_negate(int a, int b, int c, out int result)
{
  result = -a;
}

[[node]]
void integer_math_sign(int a, int b, int c, out int result)
{
  result = int(0 < a) - int(a < 0);
}

[[node]]
void integer_math_divide_floor(int a, int b, int c, out int result)
{
  result = (b != 0) ? divide_floor(a, b) : 0;
}

[[node]]
void integer_math_divide_ceil(int a, int b, int c, out int result)
{
  result = (b != 0) ? -divide_floor(a, -b) : 0;
}

[[node]]
void integer_math_divide_round(int a, int b, int c, out int result)
{
  int abs_b = abs(b);
  int sign_b = int(0 < b) - int(b < 0);
  if (a >= 0) {
    result = ((abs_b != 0) ? ((2 * a + abs_b) / (2 * abs_b)) : 0) * sign_b;
  }
  else {
    result = -((abs_b != 0) ? ((2 * -a + abs_b) / (2 * abs_b)) : 0) * sign_b;
  }
}

[[node]]
void integer_math_modulo(int a, int b, int c, out int result)
{
  result = (b != 0) ? (a % b) : 0;
}
