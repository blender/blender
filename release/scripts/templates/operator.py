
import bpy

def write_some_data(context, path, use_some_setting):
    print("running write_some_data...")
    pass

from bpy.props import *

class ExportSomeData(bpy.types.Operator):
    '''This appiers in the tooltip of the operator and in the generated docs.'''
    bl_idname = "export.some_data" # this is important since its how bpy.ops.export.some_data is constructed
    bl_label = "Export Some Data"

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.

    # TODO, add better example props
    path = StringProperty(name="File Path", description="File path used for exporting the PLY file", maxlen= 1024, default= "")
    use_setting = BoolProperty(name="Example Boolean", description="Example Tooltip", default= True)

    def poll(self, context):
        return context.active_object != None

    def execute(self, context):

        # # Bug, currently isnt working
        #if not self.is_property_set("path"):
        #    raise Exception("filename not set")

        write_some_data(self.properties.path, context, self.properties.use_setting)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager

        if True:
            # File selector
            wm.add_fileselect(self) # will run self.execute()
            return {'RUNNING_MODAL'}
        elif 0:
            # Redo popup
            return wm.invoke_props_popup(self, event) #
        elif 0:
            return self.execute(context)


bpy.types.register(ExportSomeData)

# Only needed if you want to add into a dynamic menu
import dynamic_menu
menu_func = lambda self, context: self.layout.operator("export.some_data", text="Example Exporter...")
menu_item = dynamic_menu.add(bpy.types.INFO_MT_file_export, menu_func)

# Use for running this script directly
if __name__ == "__main__":
    bpy.ops.export.some_data(path="/tmp/test.ply")
