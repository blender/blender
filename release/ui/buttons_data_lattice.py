
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
		lat = context.active_object.data
		layout = self.layout

		row = layout.row()
		row.itemR(lat, "points_u")
		row.itemR(lat, "interpolation_type_u", expand=True)
		
		row = layout.row()
		row.itemR(lat, "points_v")
		row.itemR(lat, "interpolation_type_v", expand=True)
		
		row = layout.row()
		row.itemR(lat, "points_w")
		row.itemR(lat, "interpolation_type_w", expand=True)
		
		row = layout.row()
		row.itemR(lat, "outside")
		row.itemR(lat, "shape_keys")

bpy.types.register(DATA_PT_lattice)