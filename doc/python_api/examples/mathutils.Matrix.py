import mathutils
import math

# Create a location matrix.
mat_loc = mathutils.Matrix.Translation((2.0, 3.0, 4.0))

# Create an identity matrix.
mat_sca = mathutils.Matrix.Scale(0.5, 4, (0.0, 0.0, 1.0))

# Create a rotation matrix.
mat_rot = mathutils.Matrix.Rotation(math.radians(45.0), 4, 'X')

# Combine transformations.
mat_out = mat_loc @ mat_rot @ mat_sca
print(mat_out)

# Extract components back out of the matrix as two vectors and a quaternion.
loc, rot, sca = mat_out.decompose()
print(loc, rot, sca)

# Recombine extracted components.
mat_out2 = mathutils.Matrix.LocRotScale(loc, rot, sca)
print(mat_out2)

# It can also be useful to access components of a matrix directly.
mat = mathutils.Matrix()
mat[0][0], mat[1][0], mat[2][0] = 0.0, 1.0, 2.0

mat[0][0:3] = 0.0, 1.0, 2.0

# Each item in a matrix is a vector so vector utility functions can be used.
mat[0].xyz = 0.0, 1.0, 2.0
