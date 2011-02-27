"""
Calling Operators
+++++++++++++++++

Provides python access to calling operators, this includes operators written in
C, Python or Macros.

Only keyword arguments can be used to pass operator properties.

Operators don't have return values as you might expect, instead they return a
set() which is made up of: {'RUNNING_MODAL', 'CANCELLED', 'FINISHED',
'PASS_THROUGH'}.
Common return values are {'FINISHED'} and {'CANCELLED'}.


Calling an operator in the wrong context will raise a RuntimeError,
there is a poll() method to avoid this problem.

Note that the operator ID (bl_idname) in this example is 'mesh.subdivide',
'bpy.ops' is just the access path for python.
"""
import bpy

# calling an operator
bpy.ops.mesh.subdivide(number_cuts=3, smoothness=0.5)


# check poll() to avoid exception.
if bpy.ops.object.mode_set.poll():
    bpy.ops.object.mode_set(mode='EDIT')
