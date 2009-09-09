
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "data"
	
	def poll(self, context):
		return (context.object and context.object.type == 'EMPTY')
	
class DATA_PT_empty(DataButtonsPanel):
	__label__ = "Empty"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object

		layout.itemR(ob, "empty_draw_type", text="Draw Type")
		layout.itemR(ob, "empty_draw_size", text="Draw Size")
		
bpy.types.register(DATA_PT_empty)
