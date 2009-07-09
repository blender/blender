
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.object and context.object.type == 'TEXT' and context.curve)
		
class DATA_PT_context_text(DataButtonsPanel):
	__idname__ = "DATA_PT_context_text"
	__label__ = " "
	
	def poll(self, context):
		ob = context.object
		return (context.object and context.object.type == 'TEXT')

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		curve = context.curve
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "data")
			split.itemS()
		elif curve:
			split.template_ID(space, "pin_id")
			split.itemS()


class DATA_PT_shape_text(DataButtonsPanel):
	__idname__ = "DATA_PT_shape_text"
	__label__ = "Shape Text"
	
	def poll(self, context):
		ob = context.object
		return (context.object and context.object.type == 'TEXT')

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		curve = context.curve
		space = context.space_data

		if curve:
			layout.itemR(curve, "curve_2d")			
							
			split = layout.split()
		
			sub = split.column()
			sub.itemL(text="Caps:")
			sub.itemR(curve, "front")
			sub.itemR(curve, "back")
			
			sub.itemL(text="Textures:")
			sub.itemR(curve, "uv_orco")
			sub.itemR(curve, "auto_texspace")
			
			sub = split.column()	
			sub.itemL(text="Resolution:")
			sub.itemR(curve, "resolution_u", text="Preview U")
			sub.itemR(curve, "resolution_v", text="Preview V")
			sub.itemR(curve, "render_resolution_u", text="Render U")
			sub.itemR(curve, "render_resolution_v", text="Render V")

			sub.itemL(text="Display:")
			sub.itemR(curve, "fast")

class DATA_PT_geometry_text(DataButtonsPanel):
	__idname__ = "DATA_PT_geometry_text"
	__label__ = "Geometry"

	def draw(self, context):
		layout = self.layout
		curve = context.curve

		split = layout.split()
	
		sub = split.column()
		sub.itemL(text="Modification:")
		sub.itemR(curve, "width")
		sub.itemR(curve, "extrude")
		sub.itemR(curve, "taper_object")
		
		sub = split.column()
		sub.itemL(text="Bevel:")
		sub.itemR(curve, "bevel_depth", text="Depth")
		sub.itemR(curve, "bevel_resolution", text="Resolution")
		sub.itemR(curve, "bevel_object")

class DATA_PT_font(DataButtonsPanel):
	__idname__ = "DATA_PT_font"
	__label__ = "Font"

	def draw(self, context):
		layout = self.layout
		text = context.curve

		layout.row()
		layout.itemR(text, "font")
		
		split = layout.split()
		
		sub = split.column()	
	#	sub.itemR(text, "style")
	#	sub.itemR(text, "bold")
	#	sub.itemR(text, "italic")
	#	sub.itemR(text, "underline")
	# 	ToDo: These settings are in a sub struct (Edit Format). 
		sub.itemR(text, "text_size")		
		sub.itemR(text, "shear")

		sub = split.column()
		sub.itemR(text, "text_on_curve")
		sub.itemR(text, "family")
		sub.itemL(text="Underline:")
		sub.itemR(text, "ul_position", text="Position")
		sub.itemR(text, "ul_height", text="Height")

	#	sub.itemR(text, "edit_format")

class DATA_PT_paragraph(DataButtonsPanel):
	__idname__ = "DATA_PT_paragraph"
	__label__ = "Paragraph"

	def draw(self, context):
		layout = self.layout
		text = context.curve

		layout.itemL(text="Align:")
		layout.itemR(text, "spacemode", expand=True)

		split = layout.split()
		
		sub = split.column()	
		sub.itemL(text="Spacing:")
		sub.itemR(text, "spacing", text="Character")
		sub.itemR(text, "word_spacing", text="Word")
		sub.itemR(text, "line_dist", text="Line")

		sub = split.column()
		sub.itemL(text="Offset:")
		sub.itemR(text, "x_offset", text="X")
		sub.itemR(text, "y_offset", text="Y")
		sub.itemR(text, "wrap")

"""		
class DATA_PT_textboxes(DataButtonsPanel):
		__idname__ = "DATA_PT_textboxes"
		__label__ = "Text Boxes"

		def draw(self, context):
			layout = self.layout
			text = context.curve
"""

bpy.types.register(DATA_PT_context_text)	
bpy.types.register(DATA_PT_shape_text)	
bpy.types.register(DATA_PT_geometry_text)
bpy.types.register(DATA_PT_font)
bpy.types.register(DATA_PT_paragraph)
#bpy.types.register(DATA_PT_textboxes)
