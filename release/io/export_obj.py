import bpy

class SCRIPT_OT_export_obj(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__label__ = 'Export OBJ'
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	__props__ = [
		bpy.props["StringProperty"](attr="filename", name="filename")
		]

	def debug(self, message):
		print("{0}: {1}".format(self.__class__.__name__, message))

	def exec(self, context):
		self.debug("exec")
		self.debug("filename = " + self.filename)

		self.debug("num selected objects: {0}".format(len(context.selected_objects)))

		ob = bpy.data.objects["Cube"]
		o = ob.data

		m = bpy.data.add_mesh("tmpmesh")
		m.copy_applied(context.scene, ob, True)

		def vert(co):
			return "{0}, {1}, {2}".format(co[0], co[1], co[2])

		print("	  orig: {0} with totverts={1}".format(vert(o.verts[0].co), len(o.verts)))
		print("applied: {0} with totverts={1}".format(vert(m.verts[0].co), len(m.verts)))

		# XXX errors are silenced for some reason
#		raise Exception("oops!")

		return ('FINISHED',)
	
	def invoke(self, context, event):
		self.debug("invoke")
#		context.add_fileselect(self.__operator__)
#		return ('RUNNING_MODAL',)
		return self.exec(context)
	
	def poll(self, context): # poll isnt working yet
		self.debug("poll")
		return True
