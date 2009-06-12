import bpy

class SCRIPT_OT_export_obj(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__label__ = 'Export OBJ'
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	__props__ = [
#		FloatProperty(attr="setting_1", name="Example 1", default=10.0, min=0, max=10, description="Add info here"),
#		StringProperty(attr="filename")
		]

	def debug(self, message):
		print("{0}: {1}".format(self.__class__.__name__, message))

	def exec(self, context):
#		print(self.setting_1)
		self.debug("exec")
		return 'FINISHED'
	
	def invoke(self, context, event):
		self.debug("invoke")
		return self.exec(context)
	
	def poll(self, context): # poll isnt working yet
		self.debug("poll")
		return True
