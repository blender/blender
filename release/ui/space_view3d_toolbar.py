
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

class VIEW3D_PT_tools_sculptmode(View3DPanel):
	__idname__ = "VIEW3D_PT_tools_sculptmode"
	__label__ = "Sculpt Tools"

	def draw(self, context):
		layout = self.layout

		layout.row().itemO("sculpt.radial_control")

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
bpy.types.register(VIEW3D_PT_tools_sculptmode)
bpy.types.register(VIEW3D_PT_tools_weightpaint)
bpy.types.register(VIEW3D_PT_tools_vertexpaint)
bpy.types.register(VIEW3D_PT_tools_texturepaint)


