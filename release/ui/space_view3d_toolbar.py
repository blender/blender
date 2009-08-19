
import bpy

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"

# ********** default tools for objectmode ****************

class VIEW3D_PT_tools_objectmode(View3DPanel):
	__context__ = "objectmode"
	__label__ = "Object Tools"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Transform:")
		
		col = layout.column(align=True)
		col.itemO("tfm.translate")
		col.itemO("tfm.rotate")
		col.itemO("tfm.resize", text="Scale")
		
		layout.itemL(text="Object:")
		
		col = layout.column(align=True)
		col.itemO("object.duplicate")
		col.itemO("object.delete")
		
		active_object= context.active_object
		if active_object and active_object.type == 'MESH':
			layout.itemL(text="Shading:")
		
			col = layout.column(align=True)
			col.itemO("object.shade_smooth", text="Smooth")
			col.itemO("object.shade_flat", text="Flat")
		
		layout.itemL(text="Keyframes:")
		
		col = layout.column(align=True)
		col.itemO("anim.insert_keyframe_menu", text="Insert")
		col.itemO("anim.delete_keyframe_v3d", text="Remove")

# ********** default tools for editmode_mesh ****************

class VIEW3D_PT_tools_meshedit(View3DPanel):
	__context__ = "mesh_edit"
	__label__ = "Mesh Tools"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Transform:")
		
		col = layout.column(align=True)
		col.itemO("tfm.translate")
		col.itemO("tfm.rotate")
		col.itemO("tfm.resize", text="Scale")
		
		layout.itemL(text="Mesh:")
		
		col = layout.column(align=True)
		col.itemO("mesh.duplicate")
		col.itemO("mesh.delete")
		
		layout.itemL(text="Modeling:")
		
		col = layout.column(align=True)
		col.itemO("mesh.extrude")
		col.itemO("mesh.subdivide")
		col.itemO("mesh.spin")
		col.itemO("mesh.screw")
		
		layout.itemL(text="Shading:")
		
		col = layout.column(align=True)
		col.itemO("mesh.faces_shade_smooth", text="Smooth")
		col.itemO("mesh.faces_shade_flat", text="Flat")
		
		layout.itemL(text="UV Mapping:")
		
		col = layout.column(align=True)
		col.itemO("uv.mapping_menu", text="Unwrap")
		col.itemO("mesh.uvs_rotate")
		col.itemO("mesh.uvs_mirror")

# ********** default tools for editmode_curve ****************

class VIEW3D_PT_tools_curveedit(View3DPanel):
	__context__ = "curve_edit"
	__label__ = "Curve Tools"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Transform:")
		
		col = layout.column(align=True)
		col.itemO("tfm.translate")
		col.itemO("tfm.rotate")
		col.itemO("tfm.resize", text="Scale")
		
		layout.itemL(text="Curve:")

		col = layout.column(align=True)
		col.itemO("curve.duplicate")
		col.itemO("curve.delete")
		col.itemO("curve.cyclic_toggle")
		col.itemO("curve.switch_direction")
		
		layout.itemL(text="Modeling:")

		col = layout.column(align=True)
		col.itemO("curve.extrude")
		col.itemO("curve.subdivide")

# ********** default tools for editmode_surface ****************

class VIEW3D_PT_tools_surfaceedit(View3DPanel):
	__context__ = "surface_edit"
	__label__ = "Surface Tools"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Transform:")

		col = layout.column(align=True)
		col.itemO("tfm.translate")
		col.itemO("tfm.rotate")
		col.itemO("tfm.resize", text="Scale")
		
		layout.itemL(text="Curve:")

		col = layout.column(align=True)
		col.itemO("curve.duplicate")
		col.itemO("curve.delete")
		col.itemO("curve.cyclic_toggle")
		col.itemO("curve.switch_direction")
		
		layout.itemL(text="Modeling:")

		col = layout.column(align=True)
		col.itemO("curve.extrude")
		col.itemO("curve.subdivide")

# ********** default tools for editmode_text ****************

class VIEW3D_PT_tools_textedit(View3DPanel):
	__context__ = "text_edit"
	__label__ = "Text Tools"

	def draw(self, context):
		layout = self.layout

		col = layout.column(align=True)
		col.itemO("font.text_copy", text="Copy")
		col.itemO("font.text_paste", text="Paste")
		
		col = layout.column()
		col.itemO("font.case_set")
		col.itemO("font.style_toggle")

# ********** default tools for editmode_armature ****************

class VIEW3D_PT_tools_armatureedit(View3DPanel):
	__context__ = "armature_edit"
	__label__ = "Armature Tools"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Transform:")
		
		col = layout.column(align=True)
		col.itemO("tfm.translate")
		col.itemO("tfm.rotate")
		col.itemO("tfm.resize", text="Scale")
		
		layout.itemL(text="Bones:")

		col = layout.column(align=True)
		col.itemO("armature.bone_primitive_add", text="Add")
		col.itemO("armature.duplicate", text="Duplicate")
		col.itemO("armature.delete", text="Delete")
		
		layout.itemL(text="Modeling:")
		layout.itemO("armature.extrude")

# ********** default tools for editmode_mball ****************

class VIEW3D_PT_tools_mballedit(View3DPanel):
	__context__ = "mball_edit"
	__label__ = "Meta Tools"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Transform:")
		
		col = layout.column(align=True)
		col.itemO("tfm.translate")
		col.itemO("tfm.rotate")
		col.itemO("tfm.resize", text="Scale")

# ********** default tools for editmode_lattice ****************

class VIEW3D_PT_tools_latticeedit(View3DPanel):
	__context__ = "lattice_edit"
	__label__ = "Lattice Tools"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Transform:")

		col = layout.column(align=True)
		col.itemO("tfm.translate")
		col.itemO("tfm.rotate")
		col.itemO("tfm.resize", text="Scale")

# ********** default tools for posemode ****************

class VIEW3D_PT_tools_posemode(View3DPanel):
	__context__ = "posemode"
	__label__ = "Pose Tools"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Transform:")

		col = layout.column(align=True)
		col.itemO("tfm.translate")
		col.itemO("tfm.rotate")
		col.itemO("tfm.resize", text="Scale")
		
		layout.itemL(text="Bones:")

		col = layout.column(align=True)
		col.itemO("pose.hide", text="Hide")
		col.itemO("pose.reveal", text="Reveal")
		
		layout.itemL(text="Keyframes:")
		
		col = layout.column(align=True)
		col.itemO("anim.insert_keyframe_menu", text="Insert")
		col.itemO("anim.delete_keyframe_v3d", text="Remove")
		
		layout.itemL(text="Pose:")
		
		col = layout.column(align=True)
		col.itemO("pose.copy", text="Copy")
		col.itemO("pose.paste", text="Paste")
		
		layout.itemL(text="Library:")
		
		col = layout.column(align=True)
		col.itemO("poselib.pose_add", text="Add")
		col.itemO("poselib.pose_remove", text="Remove")

# ********** default tools for paint modes ****************

class PaintPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"

	def paint_settings(self, context):
		ts = context.tool_settings

		if context.sculpt_object:
			return ts.sculpt
		elif context.vertex_paint_object:
			return ts.vertex_paint
		elif context.weight_paint_object:
			return ts.weight_paint
		elif context.texture_paint_object:
			return ts.image_paint
		elif context.particle_edit_object:
			return ts.particle_edit

		return False

class VIEW3D_PT_tools_brush(PaintPanel):
	__label__ = "Brush"

	def poll(self, context):
		return self.paint_settings(context)

	def draw(self, context):
		layout = self.layout
		
		settings = self.paint_settings(context)
		brush = settings.brush

		if not context.particle_edit_object:
			col = layout.split().column()
			row = col.row()
			row.template_list(settings, "brushes", settings, "active_brush_index", rows=2)
				
			col.template_ID(settings, "brush", new="brush.add")
                
		# Particle Mode #

		# XXX This needs a check if psys is editable.
		if context.particle_edit_object:
			# XXX Select Particle System
			layout.column().itemR(settings, "tool", expand=True)
			
			if settings.tool != 'NONE':
				col = layout.column()
				col.itemR(brush, "size", slider=True)
				col.itemR(brush, "strength", slider=True)
				
			if settings.tool == 'ADD':
				
				col = layout.column()
				col.itemR(settings, "add_interpolate")
				sub = col.column(align=True)
				sub.active = settings.add_interpolate
				sub.itemR(brush, "steps", slider=True)
				sub.itemR(settings, "add_keys", slider=True)
			elif settings.tool == 'LENGTH':
				layout.itemR(brush, "length_mode", expand=True)
			elif settings.tool == 'PUFF':
				layout.itemR(brush, "puff_mode", expand=True)

		# Sculpt Mode #

		elif context.sculpt_object and settings.brush:
			col = layout.column()
			col.itemS()
				
			row = col.row(align=True)
			row.itemR(brush, "size", slider=True)
			row.itemR(brush, "size_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
			
			if brush.sculpt_tool != 'GRAB':
				row = col.row(align=True)
				row.itemR(brush, "strength", slider=True)
				row.itemR(brush, "strength_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
			
				col = layout.column()

				if brush.sculpt_tool != 'LAYER':
					col.itemR(brush, "anchored")

				if brush.sculpt_tool in ('DRAW', 'PINCH', 'INFLATE', 'LAYER', 'CLAY'):
					col.itemR(brush, "flip_direction")

				if brush.sculpt_tool == 'LAYER':
					col.itemR(brush, "persistent")
					col.itemO("sculpt.set_persistent_base")

				col.itemR(brush, "rake")

			col.itemR(brush, "sculpt_tool")
				
		# Texture Paint Mode #
		
		elif context.texture_paint_object:
			col = layout.column(align=True)
			col.item_enumR(settings, "tool", 'DRAW')
			col.item_enumR(settings, "tool", 'SOFTEN')
			if settings.use_projection:
				col.item_enumR(settings, "tool", 'CLONE')
			else:
				col.item_enumR(settings, "tool", 'SMEAR')
				
			col = layout.column()
			col.itemR(brush, "color", text="")
				
			row = col.row(align=True)
			row.itemR(brush, "size", slider=True)
			row.itemR(brush, "size_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
			
			row = col.row(align=True)
			row.itemR(brush, "strength", slider=True)
			row.itemR(brush, "strength_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
			
			row = col.row(align=True)
			row.itemR(brush, "falloff", slider=True)
			row.itemR(brush, "falloff_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
			
			row = col.row(align=True)
			row.itemR(brush, "space", text="")
			rowsub = row.row(align=True)
			rowsub.active = brush.space
			rowsub.itemR(brush, "spacing", text="Spacing", slider=True)
			rowsub.itemR(brush, "spacing_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
			
			col = layout.column()
			col.itemR(brush, "airbrush")
			sub = col.column()
			sub.active = brush.airbrush
			sub.itemR(brush, "rate")
		
		# Weight Paint Mode #
	
		elif context.weight_paint_object:
			layout.itemR(context.tool_settings, "vertex_group_weight", text="Weight", slider=True)
			
			col = layout.column()
			row = col.row(align=True)
			row.itemR(brush, "size", slider=True)
			row.itemR(brush, "size_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
			
			row = col.row(align=True)
			row.itemR(brush, "strength", slider=True)
			row.itemR(brush, "strength_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
		
		# Vertex Paint Mode #
		
		elif context.vertex_paint_object:
			col = layout.column()
			col.itemR(brush, "color", text="")
			
			row = col.row(align=True)
			row.itemR(brush, "size", slider=True)
			row.itemR(brush, "size_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")
			
			row = col.row(align=True)
			row.itemR(brush, "strength", slider=True)
			row.itemR(brush, "strength_pressure", toggle=True, icon='ICON_BRUSH_DATA', text="")

class VIEW3D_PT_tools_brush_stroke(PaintPanel):
	__label__ = "Stroke"
	__default_closed__ = True

	def poll(self, context):
		settings = self.paint_settings(context)
		return (settings and settings.brush and context.sculpt_object)

	def draw(self, context):
		settings = self.paint_settings(context)
		brush = settings.brush
		layout = self.layout

		layout.itemR(brush, "smooth_stroke")
		col = layout.column()
		col.active = brush.smooth_stroke
		col.itemR(brush, "smooth_stroke_radius", text="Radius", slider=True)
		col.itemR(brush, "smooth_stroke_factor", text="Factor", slider=True)

		layout.itemR(brush, "space")
		col = layout.column()
		col.active = brush.space
		col.itemR(brush, "spacing", text="Distance", slider=True)


class VIEW3D_PT_tools_brush_curve(PaintPanel):
	__label__ = "Curve"
	__default_closed__ = True

	def poll(self, context):
		settings = self.paint_settings(context)
		return (settings and settings.brush and settings.brush.curve)

	def draw(self, context):
		settings = self.paint_settings(context)
		brush = settings.brush
		layout = self.layout

		layout.template_curve_mapping(brush.curve)
		layout.item_menu_enumO("brush.curve_preset", property="shape")
		
class VIEW3D_PT_sculpt_options(PaintPanel):
	__label__ = "Options"

	def poll(self, context):
		return context.sculpt_object

	def draw(self, context):
		layout = self.layout
		sculpt = context.tool_settings.sculpt

		col = layout.column()
		col.itemR(sculpt, "partial_redraw", text="Partial Refresh")
		col.itemR(sculpt, "show_brush")

		split = self.layout.split()
		
		col = split.column()
		col.itemL(text="Symmetry:")
		col.itemR(sculpt, "symmetry_x", text="X")
		col.itemR(sculpt, "symmetry_y", text="Y")
		col.itemR(sculpt, "symmetry_z", text="Z")
		
		col = split.column()
		col.itemL(text="Lock:")
		col.itemR(sculpt, "lock_x", text="X")
		col.itemR(sculpt, "lock_y", text="Y")
		col.itemR(sculpt, "lock_z", text="Z")

# ********** default tools for weightpaint ****************

class VIEW3D_PT_tools_weightpaint(View3DPanel):
	__context__ = "weightpaint"
	__label__ = "Options"

	def draw(self, context):
		layout = self.layout
		wpaint = context.tool_settings.weight_paint

		col = layout.column()
		col.itemL(text="Blend:")
		col.itemR(wpaint, "mode", text="")
		col.itemR(wpaint, "all_faces")
		col.itemR(wpaint, "normals")
		col.itemR(wpaint, "spray")
		col.itemR(wpaint, "vertex_dist", text="Distance")

# Commented out because the Apply button isn't an operator yet, making these settings useless
#		col.itemL(text="Gamma:")
#		col.itemR(wpaint, "gamma", text="")
#		col.itemL(text="Multiply:")
#		col.itemR(wpaint, "mul", text="")

# Also missing now:
# Soft, Vgroup, X-Mirror and "Clear" Operator.

# ********** default tools for vertexpaint ****************

class VIEW3D_PT_tools_vertexpaint(View3DPanel):
	__context__ = "vertexpaint"
	__label__ = "Options"

	def draw(self, context):
		layout = self.layout
		vpaint = context.tool_settings.vertex_paint

		col = layout.column()
		col.itemL(text="Blend:")
		col.itemR(vpaint, "mode", text="")
		col.itemR(vpaint, "all_faces")
		col.itemR(vpaint, "normals")
		col.itemR(vpaint, "spray")
		col.itemR(vpaint, "vertex_dist", text="Distance")
# Commented out because the Apply button isn't an operator yet, making these settings useless
#		col.itemL(text="Gamma:")
#		col.itemR(vpaint, "gamma", text="")
#		col.itemL(text="Multiply:")
#		col.itemR(vpaint, "mul", text="")


# ********** default tools for texturepaint ****************

class VIEW3D_PT_tools_texturepaint(View3DPanel):
	__context__ = "texturepaint"
	__label__ = "Options"

	def draw(self, context):
		layout = self.layout
		
		ipaint = context.tool_settings.image_paint
		settings = context.tool_settings.image_paint
		use_projection= ipaint.use_projection
		
		col = layout.column()
		col.itemR(ipaint, "use_projection")
		sub = col.column()
		sub.active = use_projection
		sub.itemR(ipaint, "use_occlude")
		sub.itemR(ipaint, "use_backface_cull")
		
		split = layout.split()
		
		col = split.column()
		col.active = (use_projection)
		col.itemR(ipaint, "use_normal_falloff")
		
		col = split.column()
		col.active = (ipaint.use_normal_falloff and use_projection)
		col.itemR(ipaint, "normal_angle", text="")
		
		
		split = layout.split(percentage=0.7)
		
		col = split.column(align=False)
		col.active = (use_projection)
		col.itemR(ipaint, "use_stencil_layer")
		
		col = split.column(align=False)
		col.active = (use_projection and ipaint.use_stencil_layer)
		col.itemR(ipaint, "invert_stencil", text="Inv")
		
		
		col = layout.column()
		sub = col.column()
		sub.active = (settings.tool == 'CLONE')
		sub.itemR(ipaint, "use_clone_layer")
		
		sub = col.column()
		sub.itemR(ipaint, "seam_bleed")
		
# ********** default tools for particle mode ****************

class VIEW3D_PT_tools_particlemode(View3DPanel):
	__context__ = "particlemode"
	__label__ = "Options"

	def draw(self, context):
		layout = self.layout
		pe = context.tool_settings.particle_edit

		col = layout.column(align=True)
		col.itemR(pe, "emitter_deflect", text="Deflect")
		sub = col.row()
		sub.active = pe.emitter_deflect
		sub.itemR(pe, "emitter_distance", text="Distance")
		
		col = layout.column(align=True)
		col.itemL(text="Keep:")
		col.itemR(pe, "keep_lengths", text="Lenghts")
		col.itemR(pe, "keep_root", text="Root")
		
		col = layout.column(align=True)
		col.itemL(text="Display:")
		col.itemR(pe, "show_time", text="Time")
		col.itemR(pe, "show_children", text="Children")

bpy.types.register(VIEW3D_PT_tools_objectmode)
bpy.types.register(VIEW3D_PT_tools_meshedit)
bpy.types.register(VIEW3D_PT_tools_curveedit)
bpy.types.register(VIEW3D_PT_tools_surfaceedit)
bpy.types.register(VIEW3D_PT_tools_textedit)
bpy.types.register(VIEW3D_PT_tools_armatureedit)
bpy.types.register(VIEW3D_PT_tools_mballedit)
bpy.types.register(VIEW3D_PT_tools_latticeedit)
bpy.types.register(VIEW3D_PT_tools_posemode)
bpy.types.register(VIEW3D_PT_tools_brush)
bpy.types.register(VIEW3D_PT_tools_brush_stroke)
bpy.types.register(VIEW3D_PT_tools_brush_curve)
bpy.types.register(VIEW3D_PT_sculpt_options)
bpy.types.register(VIEW3D_PT_tools_vertexpaint)
bpy.types.register(VIEW3D_PT_tools_weightpaint)
bpy.types.register(VIEW3D_PT_tools_texturepaint)
bpy.types.register(VIEW3D_PT_tools_particlemode)
