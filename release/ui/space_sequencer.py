
import bpy

def act_strip(context):
	try:		return context.scene.sequence_editor.active_strip
	except:	return None

# Header
class SEQUENCER_HT_header(bpy.types.Header):
	__space_type__ = "SEQUENCE_EDITOR"
	__idname__ = "SEQUENCE_HT_header"

	def draw(self, context):
		
		st = context.space_data
		layout = self.layout

		layout.template_header(context)
		
		if context.area.show_menus:
			row = layout.row(align=True)
			row.itemM(context, "SEQUENCER_MT_view")
			
			row.itemR(st, "display_mode")
			
			layout.itemS()
			
			if st.display_mode == 'SEQUENCER':
				row.itemM(context, "SEQUENCER_MT_select")
				row.itemM(context, "SEQUENCER_MT_marker")
				row.itemM(context, "SEQUENCER_MT_add")
				row.itemM(context, "SEQUENCER_MT_strip")
				layout.itemS()
				row.itemO("SEQUENCER_OT_reload")
			else:
				row.itemR(st, "display_channel") # text="Chan"

class SEQUENCER_MT_view(bpy.types.Menu):
	__space_type__ = "SEQUENCE_EDITOR"
	__label__ = "View (TODO)"
	
	def draw(self, context):
		layout = self.layout
		st = context.space_data
		
		layout.column()
		
		"""
	uiBlock *block= uiBeginBlock(C, ar, "seq_viewmenu", UI_EMBOSSP);
	short yco= 0, menuwidth=120;

	if (sseq->mainb == SEQ_DRAW_SEQUENCE) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
				 "Play Back Animation "
				 "in all Sequence Areas|Alt A", 0, yco-=20,
				 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	}
	else {
		uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL,
				 "Grease Pencil...", 0, yco-=20,
				 menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefMenuSep(block);

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
				 "Play Back Animation "
				 "in this window|Alt A", 0, yco-=20,
				 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
			 "Play Back Animation in all "
			 "3D Views and Sequence Areas|Alt Shift A",
			 0, yco-=20,
			 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

		"""
		layout.itemS()
		layout.itemO("SEQUENCER_OT_view_all")
		layout.itemO("SEQUENCER_OT_view_selected")
		layout.itemS()
		"""
	

	/* Lock Time */
	uiDefIconTextBut(block, BUTM, 1, (v2d->flag & V2D_VIEWSYNC_SCREEN_TIME)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT,
			"Lock Time to Other Windows|", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");

	/* Draw time or frames.*/
	uiDefMenuSep(block);
		"""
		
		layout.itemR(st, "draw_frames")
		
		"""
	if(!sa->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,0, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");

		"""

class SEQUENCER_MT_select(bpy.types.Menu):
	__space_type__ = "SEQUENCE_EDITOR"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout
		st = context.space_data
		
		layout.column()
		layout.item_enumO("SEQUENCER_OT_select_active_side", "side", 'LEFT', text="Strips to the Left")
		layout.item_enumO("SEQUENCER_OT_select_active_side", "side", 'RIGHT', text="Strips to the Right")
		layout.itemS()
		layout.item_enumO("SEQUENCER_OT_select_handles", "side", 'BOTH', text="Surrounding Handles")
		layout.item_enumO("SEQUENCER_OT_select_handles", "side", 'LEFT', text="Left Handle")
		layout.item_enumO("SEQUENCER_OT_select_handles", "side", 'RIGHT', text="Right Handle")
		layout.itemS()
		layout.itemO("SEQUENCER_OT_select_linked")
		layout.itemO("SEQUENCER_OT_select_all_toggle")
		layout.itemO("SEQUENCER_OT_select_invert")

class SEQUENCER_MT_marker(bpy.types.Menu):
	__space_type__ = "SEQUENCE_EDITOR"
	__label__ = "Marker (TODO)"

	def draw(self, context):
		layout = self.layout
		st = context.space_data
		
		layout.column()
		layout.itemO("SEQUENCER_OT_sound_strip_add", text="Add Marker|Ctrl Alt M")
		layout.itemO("SEQUENCER_OT_sound_strip_add", text="Duplicate Marker|Ctrl Shift D")
		layout.itemO("SEQUENCER_OT_sound_strip_add", text="Delete Marker|Shift X")
		layout.itemS()
		layout.itemO("SEQUENCER_OT_sound_strip_add", text="(Re)Name Marker|Ctrl M")
		layout.itemO("SEQUENCER_OT_sound_strip_add", text="Grab/Move Marker|Ctrl G")
		layout.itemS()
		layout.itemO("SEQUENCER_OT_sound_strip_add", text="Transform Markers") # toggle, will be rna - (sseq->flag & SEQ_MARKER_TRANS)

class SEQUENCER_MT_add(bpy.types.Menu):
	__space_type__ = "SEQUENCE_EDITOR"
	__label__ = "Add"

	def draw(self, context):
		layout = self.layout
		st = context.space_data
		
		layout.column()
		layout.itemO("SEQUENCER_OT_movie_strip_add", text="Movie")
		layout.item_booleanO("SEQUENCER_OT_movie_strip_add", "sound", True, text="Movie & Sound") # FFMPEG ONLY
		layout.itemO("SEQUENCER_OT_image_strip_add", text="Image")
		layout.itemO("SEQUENCER_OT_sound_strip_add", text="Sound (Ram)")
		layout.item_booleanO("SEQUENCER_OT_sound_strip_add", "hd", True, text="Sound (Streaming)") # FFMPEG ONLY
		
		layout.itemM(context, "SEQUENCER_MT_add_effect")


class SEQUENCER_MT_add_effect(bpy.types.Menu):
	__space_type__ = "SEQUENCE_EDITOR"
	__label__ = "Effect Strip..."

	def draw(self, context):
		layout = self.layout
		st = context.space_data
		
		self.layout.column()
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'ADD')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'SUBTRACT')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'ALPHA_OVER')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'ALPHA_UNDER')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'GAMMA_CROSS')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'MULTIPLY')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'OVER_DROP')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'PLUGIN')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'WIPE')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'GLOW')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'TRANSFORM')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'COLOR')
		self.layout.item_enumO("SEQUENCER_OT_effect_strip_add", 'type', 'SPEED')

class SEQUENCER_MT_strip(bpy.types.Menu):
	__space_type__ = "SEQUENCE_EDITOR"
	__label__ = "Strip"

	def draw(self, context):
		layout = self.layout
		st = context.space_data
		
		layout.operator_context = 'INVOKE_REGION_WIN'
		
		layout.column()
		layout.item_enumO("TFM_OT_transform", "mode", 'TRANSLATION', text="Grab/Move")
		layout.item_enumO("TFM_OT_transform", "mode", 'TIME_EXTEND', text="Grab/Extend from frame")
		#  uiItemO(layout, NULL, 0, "SEQUENCER_OT_strip_snap"); // TODO - add this operator
		layout.itemS()
		
		layout.item_enumO("SEQUENCER_OT_cut", "type", 'HARD', text="Cut (hard) at frame")
		layout.item_enumO("SEQUENCER_OT_cut", "type", 'SOFT', text="Cut (soft) at frame")
		layout.itemO("SEQUENCER_OT_images_separate")
		layout.itemS()
		
		layout.itemO("SEQUENCER_OT_duplicate_add")
		layout.itemO("SEQUENCER_OT_delete")
		
		strip = act_strip(context)
		
		if strip:
			stype = strip.type
			
			if	stype=='EFFECT':
				layout.itemS()
				layout.itemO("SEQUENCER_OT_effect_change")
				layout.itemO("SEQUENCER_OT_effect_reassign_inputs")
			elif stype=='IMAGE':
				layout.itemS()
				layout.itemO("SEQUENCER_OT_image_change")
			elif stype=='SCENE':
				layout.itemS()
				layout.itemO("SEQUENCER_OT_scene_change", text="Change Scene")
			elif stype=='MOVIE':
				layout.itemS()
				layout.itemO("SEQUENCER_OT_movie_change")
			
		layout.itemS()
		
		layout.itemO("SEQUENCER_OT_meta_make")
		layout.itemO("SEQUENCER_OT_meta_separate")
		
		#if (ed && (ed->metastack.first || (ed->act_seq && ed->act_seq->type == SEQ_META))) {
		#	uiItemS(layout);
		#	uiItemO(layout, NULL, 0, "SEQUENCER_OT_meta_toggle");
		#}
		
		layout.itemS()
		layout.itemO("SEQUENCER_OT_reload")
		layout.itemS()
		layout.itemO("SEQUENCER_OT_lock")
		layout.itemO("SEQUENCER_OT_unlock")
		layout.itemO("SEQUENCER_OT_mute")
		layout.itemO("SEQUENCER_OT_unmute")
		
		layout.item_enumO("SEQUENCER_OT_mute", property="type", value='UNSELECTED', text="Mute Deselected Strips")

		layout.itemO("SEQUENCER_OT_snap")

# Panels
class SequencerButtonsPanel(bpy.types.Panel):
	__space_type__ = "SEQUENCE_EDITOR"
	__region_type__ = "UI"

	def poll(self, context):
		return context.space_data.display_mode == 'SEQUENCER' and act_strip(context) != None
		
class SequencerButtonsPanel_Output(bpy.types.Panel):
	__space_type__ = "SEQUENCE_EDITOR"
	__region_type__ = "UI"

	def poll(self, context):
		return context.space_data.display_mode != 'SEQUENCER'

class SEQUENCER_PT_edit(SequencerButtonsPanel):
	__label__ = "Edit Strip"
	__idname__ = "SEQUENCER_PT_edit"

	def draw(self, context):
		layout = self.layout
		
		strip = act_strip(context)
		
		layout.itemR(strip, "name")
		
		layout.itemR(strip, "type")
		
		layout.itemR(strip, "blend_mode")
		
		layout.itemR(strip, "blend_opacity", text="Opacity", slider=True)
		
		split = layout.split()
		
		col = split.column()
		col.itemR(strip, "mute")
		col.itemR(strip, "lock")
		col.itemR(strip, "frame_locked")
		
		col = split.column()
		col.itemR(strip, "channel")
		col.itemR(strip, "start_frame")
		col.itemR(strip, "length")
		
		
class SEQUENCER_PT_effect(SequencerButtonsPanel):
	__label__ = "Effect Strip"
	__idname__ = "SEQUENCER_PT_effect"

	def poll(self, context):
		if context.space_data.display_mode != 'SEQUENCER':
			return False
		
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
		if context.space_data.display_mode != 'SEQUENCER':
			return False
		
		strip = act_strip(context)
		if not strip:
			return False
		
		return strip.type in ('MOVIE', 'IMAGE', 'SOUND')
	
	def draw(self, context):
		layout = self.layout
		
		strip = act_strip(context)
		
		split = layout.split(percentage=0.3)
		sub = split.column()
		sub.itemL(text="Directory:")
		sub = split.column() 
		sub.itemR(strip, "directory", text="")
		
		# Current element for the filename
		split = layout.split(percentage=0.3)
		sub = split.column()
		sub.itemL(text="File Name:")
		sub = split.column()
		
		elem = strip.getStripElem(context.scene.current_frame)
		if elem:
			sub.itemR(elem, "filename", text="") # strip.elements[0] could be a fallback
		
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
		if context.space_data.display_mode != 'SEQUENCER':
			return False
		
		strip = act_strip(context)
		if not strip:
			return False
		
		return strip.type in ('MOVIE', 'IMAGE', 'SCENE', 'META')
	
	def draw(self, context):
		layout = self.layout
		
		strip = act_strip(context)
		
		split = layout.split()
		
		col = split.column()
		col.itemR(strip, "premultiply")
		col.itemR(strip, "convert_float")
		col.itemR(strip, "de_interlace")
		col.itemR(strip, "multiply_colors")
		col.itemR(strip, "strobe")
		
		col = split.column()
		col.itemL(text="Flip:")
		col.itemR(strip, "flip_x", text="X")
		col.itemR(strip, "flip_y", text="Y")
		col.itemR(strip, "reverse_frames", text="Backwards")
		
		layout.itemR(strip, "use_color_balance")

class SEQUENCER_PT_proxy(SequencerButtonsPanel):
	__label__ = "Proxy"
	__idname__ = "SEQUENCER_PT_proxy"
	
	def poll(self, context):
		if context.space_data.display_mode != 'SEQUENCER':
			return False
		
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
		if strip.proxy: # TODO - need to add this somehow
			row.itemR(strip.proxy, "dir")
			row.itemR(strip.proxy, "file")


class SEQUENCER_PT_view(SequencerButtonsPanel_Output):
	__label__ = "View Settings"
	__idname__ = "SEQUENCER_PT_view"

	def draw(self, context):
		st = context.space_data

		layout = self.layout

		flow = layout.column_flow()
		flow.itemR(st, "draw_overexposed") # text="Zebra"
		flow.itemR(st, "draw_safe_margin")


bpy.types.register(SEQUENCER_HT_header) # header/menu classes
bpy.types.register(SEQUENCER_MT_view)
bpy.types.register(SEQUENCER_MT_select)
bpy.types.register(SEQUENCER_MT_marker)
bpy.types.register(SEQUENCER_MT_add)
bpy.types.register(SEQUENCER_MT_add_effect)
bpy.types.register(SEQUENCER_MT_strip)

bpy.types.register(SEQUENCER_PT_edit) # sequencer panels
bpy.types.register(SEQUENCER_PT_effect)
bpy.types.register(SEQUENCER_PT_input)
bpy.types.register(SEQUENCER_PT_filter)
bpy.types.register(SEQUENCER_PT_proxy)

bpy.types.register(SEQUENCER_PT_view) # view panels

