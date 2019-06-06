import mathutils
import math

# a new rotation 90 degrees about the Y axis
quat_a = mathutils.Quaternion((0.7071068, 0.0, 0.7071068, 0.0))

# passing values to Quaternion's directly can be confusing so axis, angle
# is supported for initializing too
quat_b = mathutils.Quaternion((0.0, 1.0, 0.0), math.radians(90.0))

print("Check quaternions match", quat_a == quat_b)

# like matrices, quaternions can be multiplied to accumulate rotational values
quat_a = mathutils.Quaternion((0.0, 1.0, 0.0), math.radians(90.0))
quat_b = mathutils.Quaternion((0.0, 0.0, 1.0), math.radians(45.0))
quat_out = quat_a @ quat_b

# print the quat, euler degrees for mere mortals and (axis, angle)
print("Final Rotation:")
print(quat_out)
print("%.2f, %.2f, %.2f" % tuple(math.degrees(a) for a in quat_out.to_euler()))
print("(%.2f, %.2f, %.2f), %.2f" % (quat_out.axis[:] +
                                    (math.degrees(quat_out.angle), )))

# multiple rotations can be interpolated using the exponential map
quat_c = mathutils.Quaternion((1.0, 0.0, 0.0), math.radians(15.0))
exp_avg = (quat_a.to_exponential_map() +
           quat_b.to_exponential_map() +
           quat_c.to_exponential_map()) / 3.0
quat_avg = mathutils.Quaternion(exp_avg)
print("Average rotation:")
print(quat_avg)
