import Mathutils

# zero length vector
vec = Mathutils.Vector(0, 0, 1)

# unit length vector
vec_a = vec.copy().normalize()

vec_b = Mathutils.Vector(0, 1, 2)

vec2d = Mathutils.Vector(1, 2)
vec3d = Mathutils.Vector([1, 0, 0])
vec4d = vec_a.copy().resize4D()

# other mathutuls types
quat = Mathutils.Quaternion()
matrix = Mathutils.Matrix()

# Comparison operators can be done on Vector classes:

# greater and less then test vector length.
vec_a > vec_b
vec_a >= vec_b
vec_a < vec_b
vec_a <= vec_b

# ==, != test vector values e.g. 1,2,3 != 3,2,1 even if they are the same length
vec_a == vec_b
vec_a != vec_b


# Math can be performed on Vector classes
vec_a + vec_b
vec_a - vec_b
vec_a * vec_b
vec_a * 10.0
vec_a * matrix
vec_a * vec_b
vec_a * quat
-vec_a


# You can access a vector object like a sequence
x = vec_a[0]
len(vec)
vec_a[:] = vec_b
vec2d[:] = vec3d[:2]


# Vectors support 'swizzle' operations
# See http://en.wikipedia.org/wiki/Swizzling_(computer_graphics)
vec.xyz = vec.zyx
vec.xy = vec.zw
vec.xxy = vec.wzz
vec.yzyz = vec.yxyx
