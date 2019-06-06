"""
Note that when keying data paths which contain nested properties this must be
done from the :class:`ID` subclass, in this case the :class:`Armature` rather
than the bone.
"""

import bpy
from bpy.props import (
    FloatProperty,
    PointerProperty,
)


# Define a nested property.
class MyPropGroup(bpy.types.PropertyGroup):
    nested: FloatProperty(name="Nested", default=0.0)


# Register it so its available for all bones.
bpy.utils.register_class(MyPropGroup)
bpy.types.Bone.my_prop = PointerProperty(
    type=MyPropGroup,
    name="MyProp",
)

# Get a bone.
obj = bpy.data.objects["Armature"]
arm = obj.data

# Set the keyframe at frame 1.
arm.bones["Bone"].my_prop.nested = 10
arm.keyframe_insert(
    data_path='bones["Bone"].my_prop.nested',
    frame=1,
    group="Nested Group",
)
