"""
Basic FileHandler for importing a single file
---------------------------------------------

A file handler allows custom 'drag-and-drop' behavior to be associated with a given ``Operator``
(:class:`FileHandler.bl_import_operator`) and set of file extensions
(:class:`FileHandler.bl_file_extensions`). Control over which area of the UI accepts the
`drag-in-drop` action is specified using the :class:`FileHandler.poll_drop` method.

Similar to operators that use a file select window, operators participating in 'drag-and-drop', and
only accepting a single file, must define the following property:

.. code-block:: python

    filepath: bpy.props.StringProperty(subtype='FILE_PATH', options={'SKIP_SAVE'})

This ``filepath`` property will be set to the full path of the file dropped by the user.
"""

import bpy


class CurveTextImport(bpy.types.Operator):
    """
    Creates a text object from a text file.
    """
    bl_idname = "curve.text_import"
    bl_label = "Import a text file as text object"

    # This Operator supports processing one `.txt` file at a time. The following file-path
    # property must be defined.
    filepath: bpy.props.StringProperty(subtype='FILE_PATH', options={'SKIP_SAVE'})

    @classmethod
    def poll(cls, context):
        return (context.area and context.area.type == "VIEW_3D")

    def execute(self, context):
        # Direct calls to this Operator may use unsupported file-paths. Ensure the incoming
        # file-path is one that is supported.
        if not self.filepath or not self.filepath.endswith(".txt"):
            return {'CANCELLED'}

        # Create a Blender Text object from the contents of the provided file.
        with open(self.filepath) as file:
            text_curve = bpy.data.curves.new(type="FONT", name="Text")
            text_curve.body = ''.join(file.readlines())
            text_object = bpy.data.objects.new(name="Text", object_data=text_curve)
            bpy.context.scene.collection.objects.link(text_object)
        return {'FINISHED'}

    # By default the file handler invokes the operator with the file-path property set. If the
    # operator also supports being invoked with no file-path set, and allows the user to pick from a
    # file select window instead, the following logic can be used.
    #
    # Note: It is important to use `options={'SKIP_SAVE'}` when defining the file-path property to
    # avoid prior values from being reused on subsequent calls.

    def invoke(self, context, event):
        if self.filepath:
            return self.execute(context)
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


# Define a file handler that supports the following set of conditions:
#  - Execute the `curve.text_import` operator
#  - When `.txt` files are dropped in the 3D Viewport
class CURVE_FH_text_import(bpy.types.FileHandler):
    bl_idname = "CURVE_FH_text_import"
    bl_label = "File handler for curve text object import"
    bl_import_operator = "curve.text_import"
    bl_file_extensions = ".txt"

    @classmethod
    def poll_drop(cls, context):
        return (context.area and context.area.type == 'VIEW_3D')


bpy.utils.register_class(CurveTextImport)
bpy.utils.register_class(CURVE_FH_text_import)
