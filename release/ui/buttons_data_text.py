
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == 'TEXT')
		
class DATA_PT_shape_text(DataButtonsPanel):
		__idname__ = "DATA_PT_shape_text"
		__label__ = "Shape"

		def draw(self, context):
			curve = context.main.curves[0]
			layout = self.layout

			if not curve:
				return
			row = layout.row()
			row.itemR(curve, "curve_2d")			
							
			split = layout.split()
		
			sub = split.column()
			sub.itemL(text="Caps:")
			sub.itemR(curve, "front")
			sub.itemR(curve, "back")
			
			sub.itemL(text="Textures:")
			sub.itemR(curve, "uv_orco")
			sub.itemR(curve, "autotexspace")
			
			sub = split.column()	
			sub.itemL(text="Resolution:")
			sub.itemR(curve, "resolution_u", text="Preview U")
			sub.itemR(curve, "resolution_v", text="Preview V")
			sub.itemR(curve, "render_resolution_u", text="Render U")
			sub.itemR(curve, "render_resolution_v", text="Render V")

			sub.itemL(text="Display:")
			sub.itemR(curve, "fast")
			

class DATA_PT_font(DataButtonsPanel):
	__idname__ = "DATA_PT_font"
	__label__ = "Font"

	def draw(self, context):
		text = context.main.curves[0]
		layout = self.layout

		if not text:
			return
		
		layout.row()
		layout.itemR(text, "font")
		
		split = layout.split()
		
		sub = split.column()	

	#	sub.itemR(text, "style")
		sub.itemR(text, "bold")
		sub.itemR(text, "italic")
		sub.itemR(text, "underline")
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
		text = context.main.curves[0]
		layout = self.layout

		if not text:
			return
			
		row = layout.row()
		row.itemL(text="Align:")
		row = layout.row()
		row.itemR(text, "spacemode", expand=True)

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
			
class DATA_PT_textboxes(DataButtonsPanel):
		__idname__ = "DATA_PT_textboxes"
		__label__ = "Text Boxes"

		def draw(self, context):
			text = context.main.curves[0]
			layout = self.layout

			if not text:
				return
				

			
bpy.types.register(DATA_PT_shape_text)	
bpy.types.register(DATA_PT_font)
bpy.types.register(DATA_PT_paragraph)
bpy.types.register(DATA_PT_textboxes)

