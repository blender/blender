"""
Calling a File Selector
+++++++++++++++++++++++
This example shows how an operator can use the file selector.

Notice the invoke function calls a window manager method and returns
RUNNING_MODAL, this means the file selector stays open and the operator does not
exit immediately after invoke finishes.

The file selector runs the operator, calling :class:`Operator.execute` when the
user confirms.

The :class:`Operator.poll` function is optional, used to check if the operator
can run.
"""
import bpy


class ExportSomeData(bpy.types.Operator):
    """Test exporter which just writes hello world"""
    bl_idname = "export.some_data"
    bl_label = "Export Some Data"

    filepath = bpy.props.StringProperty(subtype="FILE_PATH")

    @classmethod
    def poll(cls, context):
        return context.object is not None

    def execute(self, context):
        file = open(self.filepath, 'w')
        file.write("Hello World " + context.object.name)
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


# Only needed if you want to add into a dynamic menu
def menu_func(self, context):
    self.layout.operator_context = 'INVOKE_DEFAULT'
    self.layout.operator(ExportSomeData.bl_idname, text="Text Export Operator")

# Register and add to the file selector
bpy.utils.register_class(ExportSomeData)
bpy.types.INFO_MT_file_export.append(menu_func)


# test call
bpy.ops.export.some_data('INVOKE_DEFAULT')
