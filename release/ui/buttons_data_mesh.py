		
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == 'MESH')

class DATA_PT_surface(DataButtonsPanel):
		__idname__ = "DATA_PT_surface"
		__label__ = "Surface"

		def draw(self, context):
			mesh = context.active_object.data
			layout = self.layout

			split = layout.split()
		
			sub = split.column()
			sub.itemR(mesh, "autosmooth")
			sub.itemR(mesh, "autosmooth_angle", text="Angle")
			sub = split.column()
			sub.itemR(mesh, "vertex_normal_flip")
			sub.itemR(mesh, "double_sided")
			
			layout.itemR(mesh, "texco_mesh")			
						
bpy.types.register(DATA_PT_surface)		