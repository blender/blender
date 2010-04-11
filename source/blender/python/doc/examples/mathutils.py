import mathutils

vec = mathutils.Vector(1.0, 2.0, 3.0)

mat_rot = mathutils.RotationMatrix(90, 4, 'X')
mat_trans = mathutils.TranslationMatrix(vec)

mat = mat_trans * mat_rot
mat.invert()

mat3 = mat.rotation_part()
quat1 = mat.to_quat()
quat2 = mat3.to_quat()

angle = quat1.difference(quat2)

print(angle)