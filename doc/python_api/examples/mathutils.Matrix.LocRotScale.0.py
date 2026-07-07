# Compute local object transformation matrix:

import bpy
import mathutils

obj = bpy.context.object
if obj.rotation_mode == 'QUATERNION':
    matrix = mathutils.Matrix.LocRotScale(obj.location, obj.rotation_quaternion, obj.scale)
else:
    matrix = mathutils.Matrix.LocRotScale(obj.location, obj.rotation_euler, obj.scale)
