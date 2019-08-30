void math_add(float a, float b, out float result)
{
  result = a + b;
}

void math_subtract(float a, float b, out float result)
{
  result = a - b;
}

void math_multiply(float a, float b, out float result)
{
  result = a * b;
}

void math_divide(float a, float b, out float result)
{
  result = safe_divide(a, b);
}

void math_power(float a, float b, out float result)
{
  if (a >= 0.0) {
    result = compatible_pow(a, b);
  }
  else {
    float fraction = mod(abs(b), 1.0);
    if (fraction > 0.999 || fraction < 0.001) {
      result = compatible_pow(a, floor(b + 0.5));
    }
    else {
      result = 0.0;
    }
  }
}

void math_logarithm(float a, float b, out float result)
{
  result = (a > 0.0 && b > 0.0) ? log2(a) / log2(b) : 0.0;
}

void math_sqrt(float a, float b, out float result)
{
  result = (a > 0.0) ? sqrt(a) : 0.0;
}

void math_absolute(float a, float b, out float result)
{
  result = abs(a);
}

void math_minimum(float a, float b, out float result)
{
  result = min(a, b);
}

void math_maximum(float a, float b, out float result)
{
  result = max(a, b);
}

void math_less_than(float a, float b, out float result)
{
  result = (a < b) ? 1.0 : 0.0;
}

void math_greater_than(float a, float b, out float result)
{
  result = (a > b) ? 1.0 : 0.0;
}

void math_round(float a, float b, out float result)
{
  result = floor(a + 0.5);
}

void math_floor(float a, float b, out float result)
{
  result = floor(a);
}

void math_ceil(float a, float b, out float result)
{
  result = ceil(a);
}

void math_fraction(float a, float b, out float result)
{
  result = a - floor(a);
}

void math_modulo(float a, float b, out float result)
{
  result = c_mod(a, b);
}

void math_sine(float a, float b, out float result)
{
  result = sin(a);
}

void math_cosine(float a, float b, out float result)
{
  result = cos(a);
}

void math_tangent(float a, float b, out float result)
{
  result = tan(a);
}

void math_arcsine(float a, float b, out float result)
{
  result = (a <= 1.0 && a >= -1.0) ? asin(a) : 0.0;
}

void math_arccosine(float a, float b, out float result)
{
  result = (a <= 1.0 && a >= -1.0) ? acos(a) : 0.0;
}

void math_arctangent(float a, float b, out float result)
{
  result = atan(a);
}

void math_arctan2(float a, float b, out float result)
{
  result = atan(a, b);
}
