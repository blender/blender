import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.meta_ball)

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
		
		col = layout.column()
		
		col.itemL(text="Settings:")
		col.itemR(mball, "threshold", text="Threshold")
		col.itemL(text="Resolution:")
		col = layout.column(align=True)
		col.itemR(mball, "wire_size", text="View")
		col.itemR(mball, "render_size", text="Render")
				
		layout.itemR(mball, "flag")

class DATA_PT_metaball_element(DataButtonsPanel):
	__label__ = "Meta Element"
	
	def poll(self, context):
		return (context.meta_ball and context.meta_ball.last_selected_element)

	def draw(self, context):
		layout = self.layout
		
		metaelem = context.meta_ball.last_selected_element
		
		col = layout.column()
			
		col.itemL(text="Settings:")
		col.itemR(metaelem, "stiffness", text="Stiffness")
		col.itemR(metaelem, "size", text="Size")
		col.itemL(text="Type:")
		col.row().itemR(metaelem, "type", expand=True)
		col.itemR(metaelem, "negative", text="Negative")
	
bpy.types.register(DATA_PT_context_metaball)
bpy.types.register(DATA_PT_metaball)
bpy.types.register(DATA_PT_metaball_element)