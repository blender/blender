"""
Update Example
++++++++++++++

It can be useful to perform an action when a property is changed and can be
used to update other properties or synchronize with external data.

All properties define update functions except for CollectionProperty.

.. warning::

   Remember that these callbacks may be executed in threaded context.

.. warning::

   If the property belongs to an Operator, the update callback's first
   parameter will be an OperatorProperties instance, rather than an instance
   of the operator itself. This means you can't access other internal functions
   of the operator, only its other properties.

"""

import bpy


def update_func(self, context):
    print("my test function", self)


bpy.types.Scene.testprop = bpy.props.FloatProperty(update=update_func)

bpy.context.scene.testprop = 11.0

# >>> my test function <bpy_struct, Scene("Scene")>
