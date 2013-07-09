"""
Basic Panel Example
+++++++++++++++++++
This script is a simple panel which will draw into the object properties
section.

Notice the 'CATEGORY_PT_name' :class:`Panel.bl_idname`, this is a naming
convention for panels.

.. note::

   Panel subclasses must be registered for blender to use them.
"""
import bpy


class HelloWorldPanel(bpy.types.Panel):
    bl_idname = "OBJECT_PT_hello_world"
    bl_label = "Hello World"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        self.layout.label(text="Hello World")


bpy.utils.register_class(HelloWorldPanel)
