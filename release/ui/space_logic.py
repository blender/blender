import bpy

class LOGIC_PT_properties(bpy.types.Panel):
	__space_type__ = 'LOGIC_EDITOR'
	__region_type__ = 'UI'
	__label__ = "Properties"

	def poll(self, context):
		ob = context.active_object
		return ob and ob.game

	def draw(self, context):
		layout = self.layout
		ob = context.active_object
		game = ob.game
		
		for i, prop in enumerate(game.properties):
			flow = layout.row(align=True)
			flow.itemR(prop, "name", text="")
			flow.itemR(prop, "type", text="")
			flow.itemR(prop, "value", text="") # we dont care about the type. rna will display correctly
			flow.itemR(prop, "debug", text="", toggle=True, icon='ICON_INFO')
			flow.item_intO("object.game_property_remove", "index", i, text="", icon='ICON_X')
		
		flow = layout.row()
		flow.itemO("object.game_property_new")
			
			

bpy.types.register(LOGIC_PT_properties)
