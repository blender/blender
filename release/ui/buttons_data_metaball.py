import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.meta_ball != None)

class DATA_PT_context_metaball(DataButtonsPanel):
	__show_header__ = False
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		mball = context.meta_ball
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "data")
			split.itemS()
		elif mball:
			split.template_ID(space, "pin_id")
			split.itemS()

class DATA_PT_metaball(DataButtonsPanel):
	__label__ = "Metaball"

	def draw(self, context):
		layout = self.layout
		
		mball = context.meta_ball
		
		split = layout.split()
		sub = split.column()
		
		sub.itemL(text="Settings:")
		sub.itemR(mball, "threshold", text="Threshold")
		sub.itemR(mball, "wire_size", text="View Resolution")
		sub.itemR(mball, "render_size", text="Render Resolution")
		
		sub.itemL(text="Update:")		
		sub.itemR(mball, "flag", expand=True)

class DATA_PT_metaball_metaelem(DataButtonsPanel):
	__label__ = "MetaElem"

	def draw(self, context):
		layout = self.layout
		
		metaelem = context.meta_ball.last_selected_element
		
		if(metaelem != None):
			split = layout.split()
			sub = split.column()
			
			sub.itemL(text="Settings:")
			sub.itemR(metaelem, "stiffness", text="Stiffness")
			sub.itemR(metaelem, "size", text="Size")
			sub.itemL(text="Type:")
			sub.itemR(metaelem, "type", expand=True)
			sub.itemR(metaelem, "negative", text="Negative")

		
bpy.types.register(DATA_PT_context_metaball)
bpy.types.register(DATA_PT_metaball)
bpy.types.register(DATA_PT_metaball_metaelem)