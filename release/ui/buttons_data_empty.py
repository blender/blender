
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == 'EMPTY')
	
class DATA_PT_empty(DataButtonsPanel):
	__idname__ = "DATA_PT_empty"
	__label__ = "Empty"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return
			
		layout.column()
		layout.itemR(ob, "empty_draw_type")
		layout.itemR(ob, "empty_draw_size")
		
bpy.types.register(DATA_PT_empty)