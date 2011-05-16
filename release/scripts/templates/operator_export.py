import bpy


def write_some_data(context, filepath, use_some_setting):
    print("running write_some_data...")
    f = open(filepath, 'w')
    f.write("Hello World %s" % use_some_setting)
    f.close()

    return {'FINISHED'}


# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty


class ExportSomeData(bpy.types.Operator, ExportHelper):
    '''This appears in the tooltip of the operator and in the generated docs.'''
    bl_idname = "export.some_data"  # this is important since its how bpy.ops.export.some_data is constructed
    bl_label = "Export Some Data"

    # ExportHelper mixin class uses this
    filename_ext = ".txt"

    filter_glob = StringProperty(default="*.txt", options={'HIDDEN'})

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    use_setting = BoolProperty(name="Example Boolean", description="Example Tooltip", default=True)

    type = EnumProperty(items=(('OPT_A', "First Option", "Description one"),
                               ('OPT_B', "Second Option", "Description two."),
                               ),
                        name="Example Enum",
                        description="Choose between two items",
                        default='OPT_A')

    @classmethod
    def poll(cls, context):
        return context.active_object != None

    def execute(self, context):
        return write_some_data(context, self.filepath, self.use_setting)


# Only needed if you want to add into a dynamic menu
def menu_func_export(self, context):
    self.layout.operator(ExportSomeData.bl_idname, text="Text Export Operator")


def register():
    bpy.utils.register_class(ExportSomeData)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ExportSomeData)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()

    # test call
    bpy.ops.export.some_data('INVOKE_DEFAULT')
