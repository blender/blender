
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
		
		layout.itemR(strip, "name")
		
		layout.itemR(strip, "blend_mode")
		
		layout.itemR(strip, "blend_opacity")
		
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
		
		return strip.type in ('COLOR', 'WIPE', 'GLOW', 'SPEED', 'TRANSFORM')

	def draw(self, context):
		layout = self.layout
		
		strip = act_strip(context)
		
		if strip.type == 'COLOR':
			layout.itemR(strip, "color")
			
		elif strip.type == 'WIPE':
			row = layout.row()
			row.itemL(text="Transition Type:")
			row.itemL(text="Direction:")
			
			row = layout.row()
			row.itemR(strip, "transition_type", text="")
			row.itemR(strip, "direction", text="")
			
			row = layout.row()
			row.itemR(strip, "blur_width")
			if strip.transition_type in ('SINGLE', 'DOUBLE'):
				row.itemR(strip, "angle")
				
		elif strip.type == 'GLOW':
			flow = layout.column_flow()
			flow.itemR(strip, "threshold")
			flow.itemR(strip, "clamp")
			flow.itemR(strip, "boost_factor")
			flow.itemR(strip, "blur_distance")
			
			row = layout.row()
			row.itemR(strip, "quality", slider=True)
			row.itemR(strip, "only_boost")
			
		elif strip.type == 'SPEED':
			layout.itemR(strip, "global_speed")
			
			flow = layout.column_flow()
			flow.itemR(strip, "curve_velocity")
			flow.itemR(strip, "curve_compress_y")
			flow.itemR(strip, "frame_blending")
			
		elif strip.type == 'TRANSFORM':
			row = layout.row()
			row.itemL(text="Interpolation:")
			row.itemL(text="Translation Unit:")
			
			row = layout.row()
			row.itemR(strip, "interpolation", text="")
			row.itemR(strip, "translation_unit", text="")
			
			split = layout.split()
			
			col = split.column()
			sub = col.column(align=True) 
			sub.itemL(text="Position X:")
			sub.itemR(strip, "translate_start_x", text="Start")
			sub.itemR(strip, "translate_end_x", text="End")
			
			sub = col.column(align=True) 
			sub.itemL(text="Scale X:")
			sub.itemR(strip, "scale_start_x", text="Start")
			sub.itemR(strip, "scale_end_x", text="End")
			
			sub = col.column(align=True) 
			sub.itemL(text="Rotation:")
			sub.itemR(strip, "rotation_start", text="Start")
			sub.itemR(strip, "rotation_end", text="End")
			
			col = split.column()
			sub = col.column(align=True) 
			sub.itemL(text="Position Y:")
			sub.itemR(strip, "translate_start_y", text="Start")
			sub.itemR(strip, "translate_end_y", text="End")
			
			sub = col.column(align=True) 
			sub.itemL(text="Scale Y:")
			sub.itemR(strip, "scale_start_y", text="Start")
			sub.itemR(strip, "scale_end_y", text="End")

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
		
		layout.itemR(strip, "directory")
		
		# TODO - get current element!
		layout.itemR(strip.elements[0], "filename")
		
		"""
		layout.itemR(strip, "use_crop")
		
		flow = layout.column_flow()
		flow.active = strip.use_crop
		flow.itemR(strip, "top")
		flow.itemR(strip, "left")
		flow.itemR(strip, "bottom")
		flow.itemR(strip, "right")
		"""

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
		
		layout.itemR(strip, "use_color_balance")

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
