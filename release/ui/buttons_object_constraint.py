
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "object"

	def poll(self, context):
		ob = context.active_object
		return (ob != None)
		
class DATA_PT_constraints(DataButtonsPanel):
	__idname__ = "DATA_PT_constraints"
	__label__ = "Constraints"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		row = layout.row()
		row.item_menu_enumO("OBJECT_OT_constraint_add", "type")
		row.itemL();

		for con in ob.constraints:
			box = layout.template_constraint(con)

			if box:
				if con.type == 'COPY_LOCATION':
					self.copy_location(box, con)
					
	def target_template(self, layout, con, subtargets=True):
		layout.itemR(con, "target") # XXX limiting settings for only 'curves' or some type of object
		
		if con.target and subtargets:
			if con.target.type == "ARMATURE":
				layout.itemR(con, "subtarget", text="Bone") # XXX autocomplete
				
				row = layout.row()
				row.itemL(text="Head/Tail:")
				row.itemR(con, "head_tail", text="")
			elif con.target.type in ("MESH", "LATTICE"):
				layout.itemR(con, "subtarget", text="Vertex Group") # XXX autocomplete
	
	def copy_location(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row(align=True)
		row.itemR(con, "locate_like_x", text="X", toggle=True)
		row.itemR(con, "invert_x", text="-", toggle=True)
		row.itemR(con, "locate_like_y", text="Y", toggle=True)
		row.itemR(con, "invert_y", text="-", toggle=True)
		row.itemR(con, "locate_like_z", text="Z", toggle=True)
		row.itemR(con, "invert_z", text="-", toggle=True)

		layout.itemR(con, "offset")

bpy.types.register(DATA_PT_constraints)

