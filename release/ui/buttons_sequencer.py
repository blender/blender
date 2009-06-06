
import bpy

def act_strip(context):
	try:		return context.scene.sequence_editor.active_strip
	except:	return None


class SequencerButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "sequencer"

	def poll(self, context):
		return act_strip(context) != None

class SEQUENCER_PT_edit(SequencerButtonsPanel):
	__label__ = "Edit Strip"
	__idname__ = "SEQUENCER_PT_edit"

	def draw(self, context):
		layout = self.layout
		
		strip = act_strip(context)
		
		row = layout.row()
		row.itemR(strip, "name")
		
		row = layout.row()
		row.itemR(strip, "blend_mode")
		
		row = layout.row()
		row.itemR(strip, "blend_opacity")
		
		row = layout.row()
		row.itemR(strip, "mute")
		row.itemR(strip, "lock")
		row.itemR(strip, "frame_locked")
		
		row = layout.row()
		row.itemR(strip, "channel")
		row.itemR(strip, "start_frame")
		
		row = layout.row()
		row.itemR(strip, "length")
		row.itemR(strip, "type")
		
class SEQUENCER_PT_effect(SequencerButtonsPanel):
	__label__ = "Effect Strip"
	__idname__ = "SEQUENCER_PT_effect"

	def poll(self, context):
		strip = act_strip(context)
		if not strip:
			return False
		
		# This is a crummy way to detect effects
		return strip.type in ('REPLACE', 'CROSS', 'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER', 'GAMMA_ACROSS', 'MULTIPLY', 'OVER_DROP', 'PLUGIN', 'WIPE', 'GLOW', 'COLOR', 'SPEED')
		
	def draw(self, context):
		layout = self.layout
		
		strip = act_strip(context)
		
		if strip.type == 'COLOR':
			row = layout.row()
			row.itemR(strip, "color")
		# More Todo - maybe give each its own panel?

class SEQUENCER_PT_input(SequencerButtonsPanel):
	__label__ = "Strip Input"
	__idname__ = "SEQUENCER_PT_input"
	
	def poll(self, context):
		strip = act_strip(context)
		if not strip:
			return False
		
		return strip.type in ('MOVIE', 'IMAGE', 'SCENE', 'META')
	
	def draw(self, context):
		layout = self.layout
		
		strip = act_strip(context)
		
		row = layout.row()
		row.itemR(strip, "directory")
		
		# TODO - get current element!
		row = layout.row()
		row.itemR(strip.elements[0], "filename")
		
		
class SEQUENCER_PT_filter(SequencerButtonsPanel):
	__label__ = "Filter"
	__idname__ = "SEQUENCER_PT_filter"
	
	def poll(self, context):
		strip = act_strip(context)
		if not strip:
			return False
		
		return strip.type in ('MOVIE', 'IMAGE', 'SCENE', 'META')
	
	def draw(self, context):
		layout = self.layout
		
		strip = act_strip(context)
		
		row = layout.row()
		row.itemR(strip, "premultiply")
		row.itemR(strip, "convert_float")
		row.itemR(strip, "de_interlace")
		
		row = layout.row()
		row.itemR(strip, "flip_x")
		row.itemR(strip, "flip_y")
		row.itemR(strip, "reverse_frames")
		
		row = layout.row()
		row.itemR(strip, "multiply_colors")
		row.itemR(strip, "strobe")
		
		row = layout.row()
		row.itemR(strip, "use_color_balance")
			
	
class SEQUENCER_PT_proxy(SequencerButtonsPanel):
	__label__ = "Proxy"
	__idname__ = "SEQUENCER_PT_proxy"
	
	def poll(self, context):
		strip = act_strip(context)
		if not strip:
			return False
		
		return strip.type in ('MOVIE', 'IMAGE', 'SCENE', 'META')
	
	def draw_header(self, context):
		strip = act_strip(context)

		layout = self.layout
		
		layout.itemR(strip, "use_proxy", text="")

	def draw(self, context):
		strip = act_strip(context)
		
		layout = self.layout
		
		row = layout.row()
		row.itemR(strip, "proxy_custom_directory")
		# row.itemR(strip.proxy, "dir") # TODO
	
bpy.types.register(SEQUENCER_PT_edit)
bpy.types.register(SEQUENCER_PT_effect)
bpy.types.register(SEQUENCER_PT_input)
bpy.types.register(SEQUENCER_PT_filter)
bpy.types.register(SEQUENCER_PT_proxy)
