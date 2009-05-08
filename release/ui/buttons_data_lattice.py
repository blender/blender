
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == 'LATTICE')
	
class DATA_PT_lattice(DataButtonsPanel):
	__idname__ = "DATA_PT_lattice"
	__label__ = "Lattice"

	def draw(self, context):
		lat = context.main.lattices[0]
		layout = self.layout

		if not lat:
			return
		
		layout.row()
		layout.itemR(lat, "points_u")
		layout.itemR(lat, "interpolation_type_u", expand=True)
		
		layout.row()
		layout.itemR(lat, "points_v")
		layout.itemR(lat, "interpolation_type_v", expand=True)
		
		layout.row()
		layout.itemR(lat, "points_w")
		layout.itemR(lat, "interpolation_type_w", expand=True)
		
		layout.row()
		layout.itemR(lat, "outside")
		layout.itemR(lat, "shape_keys")

bpy.types.register(DATA_PT_lattice)