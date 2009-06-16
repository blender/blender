import bpy

def write_obj(filepath, scene, ob):
	out = open(filepath, 'w')

	# create a temporary mesh
	mesh = bpy.data.add_mesh("tmpmesh")

	# copy data with modifiers applied
	mesh.copy_applied(scene, ob)

	# for vert in mesh.verts:
	# ^ iterating that way doesn't work atm for some reason

	for i in range(len(mesh.verts)):
		vert = mesh.verts[i]
		out.write('v {0} {1} {2}\n'.format(vert.co[0], vert.co[1], vert.co[2]))
	
	for i in range(len(mesh.faces)):
		face = mesh.faces[i]
		out.write('f')

		# but this works
		for index in face.verts:
			out.write(' {0}'.format(index + 1))
		out.write('\n')

	# TODO: delete mesh here

	out.close()
	
class SCRIPT_OT_export_obj(bpy.types.Operator):
	'''A very basic OBJ exporter, writes only active object's mesh.'''

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

		act = context.active_object

		if act.type == 'MESH':
			write_obj(self.filename, context.scene, act)
		else:
			self.debug("Active object is not a MESH.")

		# XXX errors are silenced for some reason
#		raise Exception("oops!")

		return ('FINISHED',)
	
	def invoke(self, context, event):
		self.debug("invoke")
		context.add_fileselect(self.__operator__)
		return ('RUNNING_MODAL',)
	
	def poll(self, context): # poll isnt working yet
		self.debug("poll")
		return True
