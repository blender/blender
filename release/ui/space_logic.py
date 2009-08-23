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
		
		for prop in game.properties:
			flow = layout.row()
			flow.itemR(prop, "name", text="")
			flow.itemR(prop, "type", text="")
			flow.itemR(prop, "value", text="") # we dont care about the type. rna will display correctly
			flow.itemR(prop, "debug")

bpy.types.register(LOGIC_PT_properties)

