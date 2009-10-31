
import bpy

class DataButtonsPanel(bpy.types.Panel):
	bl_space_type = 'PROPERTIES'
	bl_region_type = 'WINDOW'
	bl_context = "data"
	
	def poll(self, context):
		return context.lattice
	
class DATA_PT_context_lattice(DataButtonsPanel):
	bl_label = ""
	bl_show_header = False
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		lat = context.lattice
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "data")
			split.itemS()
		elif lat:
			split.template_ID(space, "pin_id")
			split.itemS()

class DATA_PT_lattice(DataButtonsPanel):
	bl_label = "Lattice"

	def draw(self, context):
		layout = self.layout
		
		lat = context.lattice

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
		row.itemO("lattice.make_regular")
		row.itemR(lat, "outside")

bpy.types.register(DATA_PT_context_lattice)
bpy.types.register(DATA_PT_lattice)
