"""
Operator Example
++++++++++++++++

A common use of custom properties is for python based :class:`Operator`
classes. Test this code by running it in the text editor, or by clicking the
button in the 3D Viewport's Tools panel. The latter will show the properties
in the Redo panel and allow you to change them.
"""
import bpy


class OBJECT_OT_property_example(bpy.types.Operator):
    bl_idname = "object.property_example"
    bl_label = "Property Example"
    bl_options = {'REGISTER', 'UNDO'}

    my_float: bpy.props.FloatProperty(name="Some Floating Point")
    my_bool: bpy.props.BoolProperty(name="Toggle Option")
    my_string: bpy.props.StringProperty(name="String Value")

    def execute(self, context):
        self.report(
            {'INFO'}, 'F: %.2f  B: %s  S: %r' %
            (self.my_float, self.my_bool, self.my_string)
        )
        print('My float:', self.my_float)
        print('My bool:', self.my_bool)
        print('My string:', self.my_string)
        return {'FINISHED'}


class OBJECT_PT_property_example(bpy.types.Panel):
    bl_idname = "object_PT_property_example"
    bl_label = "Property Example"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Tool"

    def draw(self, context):
        # You can set the property values that should be used when the user
        # presses the button in the UI.
        props = self.layout.operator('object.property_example')
        props.my_bool = True
        props.my_string = "Shouldn't that be 47?"

        # You can set properties dynamically:
        if context.object:
            props.my_float = context.object.location.x
        else:
            props.my_float = 327


bpy.utils.register_class(OBJECT_OT_property_example)
bpy.utils.register_class(OBJECT_PT_property_example)

# Demo call. Be sure to also test in the 3D Viewport.
bpy.ops.object.property_example(
    my_float=47,
    my_bool=True,
    my_string="Shouldn't that be 327?",
)
