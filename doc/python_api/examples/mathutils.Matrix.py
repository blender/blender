import mathutils
import math

# create a location matrix
mat_loc = mathutils.Matrix.Translation((2.0, 3.0, 4.0))

# create an identitiy matrix
mat_sca = mathutils.Matrix.Scale(0.5, 4, (0.0, 0.0, 1.0))

# create a rotation matrix
mat_rot = mathutils.Matrix.Rotation(math.radians(45.0), 4, 'X')

# combine transformations
mat_out = mat_loc @ mat_rot @ mat_sca
print(mat_out)

# extract components back out of the matrix
loc, rot, sca = mat_out.decompose()
print(loc, rot, sca)

# it can also be useful to access components of a matrix directly
mat = mathutils.Matrix()
mat[0][0], mat[1][0], mat[2][0] = 0.0, 1.0, 2.0

mat[0][0:3] = 0.0, 1.0, 2.0

# each item in a matrix is a vector so vector utility functions can be used
mat[0].xyz = 0.0, 1.0, 2.0
