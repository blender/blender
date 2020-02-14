/* Float Math */

float safe_divide(float a, float b)
{
  return (b != 0.0) ? a / b : 0.0;
}

/* fmod function compatible with OSL using nvidia reference example. */
float compatible_fmod(float a, float b)
{
  float c = (b != 0.0) ? fract(abs(a / b)) * abs(b) : 0.0;
  return (a < 0.0) ? -c : c;
}

float compatible_pow(float x, float y)
{
  if (y == 0.0) { /* x^0 -> 1, including 0^0 */
    return 1.0;
  }

  /* glsl pow doesn't accept negative x */
  if (x < 0.0) {
    if (mod(-y, 2.0) == 0.0) {
      return pow(-x, y);
    }
    else {
      return -pow(-x, y);
    }
  }
  else if (x == 0.0) {
    return 0.0;
  }

  return pow(x, y);
}

float wrap(float a, float b, float c)
{
  float range = b - c;
  return (range != 0.0) ? a - (range * floor((a - c) / range)) : c;
}

vec3 wrap(vec3 a, vec3 b, vec3 c)
{
  return vec3(wrap(a.x, b.x, c.x), wrap(a.y, b.y, c.y), wrap(a.z, b.z, c.z));
}

float hypot(float x, float y)
{
  return sqrt(x * x + y * y);
}

int floor_to_int(float x)
{
  return int(floor(x));
}

int quick_floor(float x)
{
  return int(x) - ((x < 0) ? 1 : 0);
}

/* Vector Math */

vec2 safe_divide(vec2 a, vec2 b)
{
  return vec2(safe_divide(a.x, b.x), safe_divide(a.y, b.y));
}

vec3 safe_divide(vec3 a, vec3 b)
{
  return vec3(safe_divide(a.x, b.x), safe_divide(a.y, b.y), safe_divide(a.z, b.z));
}

vec4 safe_divide(vec4 a, vec4 b)
{
  return vec4(
      safe_divide(a.x, b.x), safe_divide(a.y, b.y), safe_divide(a.z, b.z), safe_divide(a.w, b.w));
}

vec2 safe_divide(vec2 a, float b)
{
  return (b != 0.0) ? a / b : vec2(0.0);
}

vec3 safe_divide(vec3 a, float b)
{
  return (b != 0.0) ? a / b : vec3(0.0);
}

vec4 safe_divide(vec4 a, float b)
{
  return (b != 0.0) ? a / b : vec4(0.0);
}

vec3 compatible_fmod(vec3 a, vec3 b)
{
  return vec3(compatible_fmod(a.x, b.x), compatible_fmod(a.y, b.y), compatible_fmod(a.z, b.z));
}

void invert_z(vec3 v, out vec3 outv)
{
  v.z = -v.z;
  outv = v;
}

void vector_normalize(vec3 normal, out vec3 outnormal)
{
  outnormal = normalize(normal);
}

/* Matirx Math */

mat3 euler_to_mat3(vec3 euler)
{
  float cx = cos(euler.x);
  float cy = cos(euler.y);
  float cz = cos(euler.z);
  float sx = sin(euler.x);
  float sy = sin(euler.y);
  float sz = sin(euler.z);

  mat3 mat;
  mat[0][0] = cy * cz;
  mat[0][1] = cy * sz;
  mat[0][2] = -sy;

  mat[1][0] = sy * sx * cz - cx * sz;
  mat[1][1] = sy * sx * sz + cx * cz;
  mat[1][2] = cy * sx;

  mat[2][0] = sy * cx * cz + sx * sz;
  mat[2][1] = sy * cx * sz - sx * cz;
  mat[2][2] = cy * cx;
  return mat;
}

void direction_transform_m4v3(vec3 vin, mat4 mat, out vec3 vout)
{
  vout = (mat * vec4(vin, 0.0)).xyz;
}

void normal_transform_transposed_m4v3(vec3 vin, mat4 mat, out vec3 vout)
{
  vout = transpose(mat3(mat)) * vin;
}

void point_transform_m4v3(vec3 vin, mat4 mat, out vec3 vout)
{
  vout = (mat * vec4(vin, 1.0)).xyz;
}
