def write_some_data(context, path, use_some_setting):
	pass
	
class ExportSomeData(bpy.types.Operator):
	'''This appiers in the tooltip of the operator and in the generated docs.'''
	__idname__ = "export.some_data" # this is important since its how bpy.ops.export.some_data is constructed
	__label__ = "Export Some Data"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	# TODO, add better example props
	__props__ = [
		bpy.props.StringProperty(attr="path", name="File Path", description="File path used for exporting the PLY file", maxlen= 1024, default= ""),
		bpy.props.BoolProperty(attr="use_some_setting", name="Apply Modifiers", description="Apply Modifiers to the exported mesh", default= True),
	]
	
	def poll(self, context):
		return context.active_object != None
	
	def execute(self, context):
		if not self.is_property_set("path"):
			raise Exception("filename not set")
		
		write(self.path, context, use_setting, SOME_SETTING = self.use_some_setting)

		return ('FINISHED',)
	
	def invoke(self, context, event):
		wm = context.manager
		
		if True:
			# File selector
			wm.add_fileselect(self.__operator__) # will run self.execute()
			return ('RUNNING_MODAL',)
		else if 0:
			# Redo popup
			wm.invoke_props_popup(self.__operator__, event) # 
			return ('RUNNING_MODAL',)
		else if 0:
			return self.execute(context)


bpy.ops.add(ExportSomeData)

# Only needed if you want to add into a dynamic menu
import dynamic_menu
menu_func = lambda self, context: self.layout.itemO("export.some_data", text="Example Exporter...")
menu_item = dynamic_menu.add(bpy.types.INFO_MT_file_export, menu_func)

# Use for running this script directly
if __name__ == "__main__":
	bpy.ops.export.some_data(path="/tmp/test.ply")