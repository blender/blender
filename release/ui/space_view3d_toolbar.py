
import bpy

# ********** default tools for objectmode ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "objectmode"

class VIEW3D_PT_tools_objectmode(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_objectmode"
	__label__ = "Object Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("object.duplicate")
		layout.row().itemO("object.delete")
		layout.row().itemO("object.mesh_add")
		layout.row().itemO("object.curve_add")
		layout.row().itemO("object.text_add")
		layout.row().itemO("object.surface_add")

# ********** default tools for editmode_mesh ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "editmode_mesh"

class VIEW3D_PT_tools_editmode_mesh(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_editmode_mesh"
	__label__ = "Mesh Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("mesh.duplicate")
		layout.row().itemO("mesh.delete")
		layout.row().itemO("mesh.spin")
		layout.row().itemO("mesh.screw")
		layout.row().itemO("mesh.primitive_plane_add")
		layout.row().itemO("mesh.primitive_cube_add")
		layout.row().itemO("mesh.primitive_circle_add")
		layout.row().itemO("mesh.primitive_cylinder_add")

# ********** default tools for editmode_curve ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "editmode_curve"

class VIEW3D_PT_tools_editmode_curve(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_editmode_curve"
	__label__ = "Curve Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("curve.duplicate")
		layout.row().itemO("curve.delete")
		layout.row().itemO("object.curve_add")
		layout.row().itemO("curve.subdivide")

# ********** default tools for editmode_surface ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "editmode_surface"

class VIEW3D_PT_tools_editmode_surface(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_editmode_surface"
	__label__ = "Surface Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("curve.duplicate")
		layout.row().itemO("curve.delete")
		layout.row().itemO("object.surface_add")
		layout.row().itemO("curve.subdivide")

# ********** default tools for editmode_text ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "editmode_text"

class VIEW3D_PT_tools_editmode_text(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_editmode_text"
	__label__ = "Text Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("font.text_copy")
		layout.row().itemO("font.text_paste")
		layout.row().itemO("font.case_set")
		layout.row().itemO("font.style_toggle")

# ********** default tools for editmode_armature ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "editmode_armature"

class VIEW3D_PT_tools_editmode_armature(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_editmode_armature"
	__label__ = "Armature Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("armature.duplicate_selected")
		layout.row().itemO("armature.bone_primitive_add")
		layout.row().itemO("armature.delete")
		layout.row().itemO("armature.parent_clear")

# ********** default tools for editmode_mball ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "editmode_mball"

class VIEW3D_PT_tools_editmode_mball(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_editmode_mball"
	__label__ = "Meta Tools"

	def draw(self, context):
		layout = self.layout

		row = layout.row()

# ********** default tools for editmode_lattice ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "editmode_lattice"

class VIEW3D_PT_tools_editmode_lattice(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_editmode_lattice"
	__label__ = "Lattice Tools"

	def draw(self, context):
		layout = self.layout

		row = layout.row()

# ********** default tools for posemode ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "posemode"

class VIEW3D_PT_tools_posemode(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_posemode"
	__label__ = "Pose Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("pose.hide")
		layout.row().itemO("pose.reveal")
		layout.row().itemO("pose.rot_clear")
		layout.row().itemO("pose.loc_clear")

# ********** default tools for sculptmode ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "sculptmode"

#class VIEW3D_PT_tools_sculptmode(View3DPanel):
#	__idname__ = "VIEW3D_PT_tools_sculptmode"
#	__label__ = "Sculpt Tools"
#
#	def draw(self, context):
#		layout = self.layout
#
#		layout.row().itemO("sculpt.radial_control")

class VIEW3D_PT_tools_brush(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__label__ = "Brush"

	def brush_src(self, context):
		ts = context.scene.tool_settings
		if context.sculpt_object:
			return ts.sculpt
		elif context.vpaint_object:
			return ts.vpaint
		elif context.wpaint_object:
			return ts.wpaint
		return False

	def poll(self, context):
		return self.brush_src(context)

	def draw(self, context):
		src = self.brush_src(context)
		brush = src.brush
		layout = self.layout

		layout.split().row().template_ID(src, "brush")

		if context.sculpt_object:
			layout.column().itemR(brush, "sculpt_tool", expand=True)

		split = layout.split()
		col = split.column()
		col.itemR(brush, "size", slider=True)
		if context.wpaint_object:
			col.itemR(context.scene.tool_settings, "vertex_group_weight", text="Weight", slider=True)
		col.itemR(brush, "strength", slider=True)



		split = layout.split()
		col = split.column()
		col.itemR(brush, "airbrush")
		col.itemR(brush, "anchored")
		col.itemR(brush, "rake")
		col.itemR(brush, "space", text="Spacing")
		colsub = col.column()
		colsub.active = brush.space
		colsub.itemR(brush, "spacing", text="")

		split = layout.split()
		split.template_curve_mapping(brush.curve)
		
class VIEW3D_PT_sculptoptions(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__label__ = "Options"

	def poll(self, context):
		return context.sculpt_object

	def draw(self, context):
		sculpt = context.scene.tool_settings.sculpt

		split = self.layout.split()
		
		col = split.column()
		col.itemL(text="Symmetry:")
		row = col.row(align=True)
		row.itemR(sculpt, "symmetry_x", text="X", toggle=True)
		row.itemR(sculpt, "symmetry_y", text="Y", toggle=True)
		row.itemR(sculpt, "symmetry_z", text="Z", toggle=True)

		split = self.layout.split()
		
		col = split.column()
		col.itemL(text="Lock Axis:")
		row = col.row(align=True)
		row.itemR(sculpt, "lock_x", text="X", toggle=True)
		row.itemR(sculpt, "lock_y", text="Y", toggle=True)
		row.itemR(sculpt, "lock_z", text="Z", toggle=True)


# ********** default tools for weightpaint ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "weightpaint"

class VIEW3D_PT_tools_weightpaint(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_weightpaint"
	__label__ = "Weight Paint Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("paint.weight_paint_radial_control")

# ********** default tools for vertexpaint ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "vertexpaint"

class VIEW3D_PT_tools_vertexpaint(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_vertexpaint"
	__label__ = "Vertex Paint Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("paint.vertex_paint_radial_control")

# ********** default tools for texturepaint ****************

class View3DPanel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "TOOLS"
	__context__ = "texturepaint"

class VIEW3D_PT_tools_texturepaint(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_texturepaint"
	__label__ = "Texture Paint Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("paint.texture_paint_radial_control")


bpy.types.register(VIEW3D_PT_tools_objectmode)
bpy.types.register(VIEW3D_PT_tools_editmode_mesh)
bpy.types.register(VIEW3D_PT_tools_editmode_curve)
bpy.types.register(VIEW3D_PT_tools_editmode_surface)
bpy.types.register(VIEW3D_PT_tools_editmode_text)
bpy.types.register(VIEW3D_PT_tools_editmode_armature)
bpy.types.register(VIEW3D_PT_tools_editmode_mball)
bpy.types.register(VIEW3D_PT_tools_editmode_lattice)
bpy.types.register(VIEW3D_PT_tools_posemode)
bpy.types.register(VIEW3D_PT_tools_brush)
bpy.types.register(VIEW3D_PT_sculptoptions)
bpy.types.register(VIEW3D_PT_tools_weightpaint)
bpy.types.register(VIEW3D_PT_tools_vertexpaint)
bpy.types.register(VIEW3D_PT_tools_texturepaint)


