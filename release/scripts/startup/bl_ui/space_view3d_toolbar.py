# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Menu, Panel, UIList
from bl_ui.properties_grease_pencil_common import GreasePencilPanel
from bl_ui.properties_paint_common import (
        UnifiedPaintPanel,
        brush_texture_settings,
        brush_texpaint_common,
        brush_mask_texture_settings,
        )


class View3DPanel():
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'


# **************** standard tool clusters ******************

# Keyframing tools
def draw_keyframing_tools(context, layout):
    col = layout.column(align=True)
    col.label(text="Keyframes:")
    row = col.row(align=True)
    row.operator("anim.keyframe_insert_menu", text="Insert")
    row.operator("anim.keyframe_delete_v3d", text="Remove")


# ********** default tools for object-mode ****************


class VIEW3D_PT_tools_transform(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "objectmode"
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        col = layout.column(align=True)
        col.operator("transform.mirror", text="Mirror")


class VIEW3D_PT_tools_object(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "objectmode"
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("object.duplicate_move", text="Duplicate")
        col.operator("object.duplicate_move_linked", text="Duplicate Linked")

        col.operator("object.delete")

        obj = context.active_object
        if obj:
            obj_type = obj.type

            if obj_type in {'MESH', 'CURVE', 'SURFACE', 'ARMATURE'}:
                col = layout.column(align=True)
                col.operator("object.join")

            if obj_type in {'MESH', 'CURVE', 'SURFACE', 'ARMATURE', 'FONT', 'LATTICE'}:
                col = layout.column(align=True)
                col.operator_menu_enum("object.origin_set", "type", text="Set Origin")

            if obj_type in {'MESH', 'CURVE', 'SURFACE'}:
                col = layout.column(align=True)
                col.label(text="Shading:")
                row = col.row(align=True)
                row.operator("object.shade_smooth", text="Smooth")
                row.operator("object.shade_flat", text="Flat")


class VIEW3D_PT_tools_add_object(View3DPanel, Panel):
    bl_category = "Create"
    bl_context = "objectmode"
    bl_label = "Add Primitive"

    @staticmethod
    def draw_add_mesh(layout, label=False):
        if label:
            layout.label(text="Primitives:")
        layout.operator("mesh.primitive_plane_add", text="Plane", icon='MESH_PLANE')
        layout.operator("mesh.primitive_cube_add", text="Cube", icon='MESH_CUBE')
        layout.operator("mesh.primitive_circle_add", text="Circle", icon='MESH_CIRCLE')
        layout.operator("mesh.primitive_uv_sphere_add", text="UV Sphere", icon='MESH_UVSPHERE')
        layout.operator("mesh.primitive_ico_sphere_add", text="Ico Sphere", icon='MESH_ICOSPHERE')
        layout.operator("mesh.primitive_cylinder_add", text="Cylinder", icon='MESH_CYLINDER')
        layout.operator("mesh.primitive_cone_add", text="Cone", icon='MESH_CONE')
        layout.operator("mesh.primitive_torus_add", text="Torus", icon='MESH_TORUS')

        if label:
            layout.label(text="Special:")
        else:
            layout.separator()
        layout.operator("mesh.primitive_grid_add", text="Grid", icon='MESH_GRID')
        layout.operator("mesh.primitive_monkey_add", text="Monkey", icon='MESH_MONKEY')

    @staticmethod
    def draw_add_curve(layout, label=False):
        if label:
            layout.label(text="Bezier:")
        layout.operator("curve.primitive_bezier_curve_add", text="Bezier", icon='CURVE_BEZCURVE')
        layout.operator("curve.primitive_bezier_circle_add", text="Circle", icon='CURVE_BEZCIRCLE')

        if label:
            layout.label(text="Nurbs:")
        else:
            layout.separator()
        layout.operator("curve.primitive_nurbs_curve_add", text="Nurbs Curve", icon='CURVE_NCURVE')
        layout.operator("curve.primitive_nurbs_circle_add", text="Nurbs Circle", icon='CURVE_NCIRCLE')
        layout.operator("curve.primitive_nurbs_path_add", text="Path", icon='CURVE_PATH')

    @staticmethod
    def draw_add_surface(layout):
        layout.operator("surface.primitive_nurbs_surface_curve_add", text="Nurbs Curve", icon='SURFACE_NCURVE')
        layout.operator("surface.primitive_nurbs_surface_circle_add", text="Nurbs Circle", icon='SURFACE_NCIRCLE')
        layout.operator("surface.primitive_nurbs_surface_surface_add", text="Nurbs Surface", icon='SURFACE_NSURFACE')
        layout.operator("surface.primitive_nurbs_surface_cylinder_add", text="Nurbs Cylinder", icon='SURFACE_NCYLINDER')
        layout.operator("surface.primitive_nurbs_surface_sphere_add", text="Nurbs Sphere", icon='SURFACE_NSPHERE')
        layout.operator("surface.primitive_nurbs_surface_torus_add", text="Nurbs Torus", icon='SURFACE_NTORUS')

    @staticmethod
    def draw_add_mball(layout):
        layout.operator_enum("object.metaball_add", "type")

    @staticmethod
    def draw_add_lamp(layout):
        layout.operator_enum("object.lamp_add", "type")

    @staticmethod
    def draw_add_other(layout):
        layout.operator("object.text_add", text="Text", icon='OUTLINER_OB_FONT')
        layout.operator("object.armature_add", text="Armature", icon='OUTLINER_OB_ARMATURE')
        layout.operator("object.add", text="Lattice", icon='OUTLINER_OB_LATTICE').type = 'LATTICE'
        layout.operator("object.empty_add", text="Empty", icon='OUTLINER_OB_EMPTY').type = 'PLAIN_AXES'
        layout.operator("object.speaker_add", text="Speaker", icon='OUTLINER_OB_SPEAKER')
        layout.operator("object.camera_add", text="Camera", icon='OUTLINER_OB_CAMERA')

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Mesh:")
        self.draw_add_mesh(col)

        col = layout.column(align=True)
        col.label(text="Curve:")
        self.draw_add_curve(col)

        # not used here:
        # draw_add_surface
        # draw_add_mball

        col = layout.column(align=True)
        col.label(text="Lamp:")
        self.draw_add_lamp(col)

        col = layout.column(align=True)
        col.label(text="Other:")
        self.draw_add_other(col)


class VIEW3D_PT_tools_relations(View3DPanel, Panel):
    bl_category = "Relations"
    bl_context = "objectmode"
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        col.label(text="Group:")
        col.operator("group.create", text="New Group")
        col.operator("group.objects_add_active", text="Add to Active")
        col.operator("group.objects_remove", text="Remove from Group")

        col.separator()

        col.label(text="Parent:")
        row = col.row(align=True)
        row.operator("object.parent_set", text="Set")
        row.operator("object.parent_clear", text="Clear")

        col.separator()

        col.label(text="Object Data:")
        col.operator("object.make_links_data")
        col.operator("object.make_single_user")

        col.separator()

        col.label(text="Linked Objects:")
        col.operator("object.make_local")
        col.operator("object.proxy_make")


class VIEW3D_PT_tools_animation(View3DPanel, Panel):
    bl_category = "Animation"
    bl_context = "objectmode"
    bl_label = "Animation"

    def draw(self, context):
        layout = self.layout

        draw_keyframing_tools(context, layout)

        col = layout.column(align=True)
        col.label(text="Motion Paths:")
        row = col.row(align=True)
        row.operator("object.paths_calculate", text="Calculate")
        row.operator("object.paths_clear", text="Clear")

        col.separator()

        col.label(text="Action:")
        col.operator("nla.bake", text="Bake Action")


class VIEW3D_PT_tools_rigid_body(View3DPanel, Panel):
    bl_category = "Physics"
    bl_context = "objectmode"
    bl_label = "Rigid Body Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Add/Remove:")
        row = col.row(align=True)
        row.operator("rigidbody.objects_add", text="Add Active").type = 'ACTIVE'
        row.operator("rigidbody.objects_add", text="Add Passive").type = 'PASSIVE'
        row = col.row(align=True)
        row.operator("rigidbody.objects_remove", text="Remove")

        col = layout.column(align=True)
        col.label(text="Object Tools:")
        col.operator("rigidbody.shape_change", text="Change Shape")
        col.operator("rigidbody.mass_calculate", text="Calculate Mass")
        col.operator("rigidbody.object_settings_copy", text="Copy from Active")
        col.operator("object.visual_transform_apply", text="Apply Transformation")
        col.operator("rigidbody.bake_to_keyframes", text="Bake To Keyframes")
        col.label(text="Constraints:")
        col.operator("rigidbody.connect", text="Connect")


# ********** default tools for editmode_mesh ****************

class VIEW3D_PT_tools_transform_mesh(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "mesh_edit"
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")
        col.operator("transform.shrink_fatten", text="Shrink/Fatten")
        col.operator("transform.push_pull", text="Push/Pull")


class VIEW3D_PT_tools_meshedit(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "mesh_edit"
    bl_label = "Mesh Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Deform:")
        row = col.row(align=True)
        row.operator("transform.edge_slide", text="Slide Edge")
        row.operator("transform.vert_slide", text="Vertex")
        col.operator("mesh.noise")
        col.operator("mesh.vertices_smooth")
        col.operator("object.vertex_random")

        col = layout.column(align=True)
        col.label(text="Add:")

        col.menu("VIEW3D_MT_edit_mesh_extrude")
        col.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Region")
        col.operator("view3d.edit_mesh_extrude_individual_move", text="Extrude Individual")
        col.operator("mesh.subdivide")
        col.operator("mesh.loopcut_slide")
        col.operator("mesh.duplicate_move", text="Duplicate")
        row = col.row(align=True)
        row.operator("mesh.spin")
        row.operator("mesh.screw")

        row = col.row(align=True)
        props = row.operator("mesh.knife_tool", text="Knife")
        props.use_occlude_geometry = True
        props.only_selected = False
        props = row.operator("mesh.knife_tool", text="Select")
        props.use_occlude_geometry = False
        props.only_selected = True
        col.operator("mesh.knife_project")
        col.operator("mesh.bisect")

        col = layout.column(align=True)
        col.label(text="Remove:")
        col.menu("VIEW3D_MT_edit_mesh_delete")
        col.operator_menu_enum("mesh.merge", "type")
        col.operator("mesh.remove_doubles")


class VIEW3D_PT_tools_meshweight(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "mesh_edit"
    bl_label = "Weight Tools"
    bl_options = {'DEFAULT_CLOSED'}

    # Used for Weight-Paint mode and Edit-Mode
    @staticmethod
    def draw_generic(layout):
        col = layout.column()
        col.operator("object.vertex_group_normalize_all", text="Normalize All")
        col.operator("object.vertex_group_normalize", text="Normalize")
        col.operator("object.vertex_group_mirror", text="Mirror")
        col.operator("object.vertex_group_invert", text="Invert")
        col.operator("object.vertex_group_clean", text="Clean")
        col.operator("object.vertex_group_quantize", text="Quantize")
        col.operator("object.vertex_group_levels", text="Levels")
        col.operator("object.vertex_group_blend", text="Blend")
        col.operator("object.vertex_group_limit_total", text="Limit Total")
        col.operator("object.vertex_group_fix", text="Fix Deforms")

    def draw(self, context):
        layout = self.layout
        self.draw_generic(layout)


class VIEW3D_PT_tools_add_mesh_edit(View3DPanel, Panel):
    bl_category = "Create"
    bl_context = "mesh_edit"
    bl_label = "Add Meshes"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        VIEW3D_PT_tools_add_object.draw_add_mesh(col, label=True)


class VIEW3D_PT_tools_shading(View3DPanel, Panel):
    bl_category = "Shading / UVs"
    bl_context = "mesh_edit"
    bl_label = "Shading"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Faces:")
        row = col.row(align=True)
        row.operator("mesh.faces_shade_smooth", text="Smooth")
        row.operator("mesh.faces_shade_flat", text="Flat")
        col.label(text="Edges:")
        row = col.row(align=True)
        row.operator("mesh.mark_sharp", text="Smooth").clear = True
        row.operator("mesh.mark_sharp", text="Sharp")
        col.label(text="Vertices:")
        row = col.row(align=True)
        op = row.operator("mesh.mark_sharp", text="Smooth")
        op.use_verts = True
        op.clear = True
        row.operator("mesh.mark_sharp", text="Sharp").use_verts = True

        col = layout.column(align=True)
        col.label(text="Normals:")
        col.operator("mesh.normals_make_consistent", text="Recalculate")
        col.operator("mesh.flip_normals", text="Flip Direction")


class VIEW3D_PT_tools_uvs(View3DPanel, Panel):
    bl_category = "Shading / UVs"
    bl_context = "mesh_edit"
    bl_label = "UVs"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="UV Mapping:")
        col.menu("VIEW3D_MT_uv_map", text="Unwrap")
        col.operator("mesh.mark_seam").clear = False
        col.operator("mesh.mark_seam", text="Clear Seam").clear = True


class VIEW3D_PT_tools_meshedit_options(View3DPanel, Panel):
    bl_category = "Options"
    bl_context = "mesh_edit"
    bl_label = "Mesh Options"

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout

        ob = context.active_object

        tool_settings = context.tool_settings
        mesh = ob.data

        col = layout.column(align=True)
        col.prop(mesh, "use_mirror_x")

        row = col.row(align=True)
        row.active = ob.data.use_mirror_x
        row.prop(mesh, "use_mirror_topology")

        col = layout.column(align=True)
        col.label("Edge Select Mode:")
        col.prop(tool_settings, "edge_path_mode", text="")
        col.prop(tool_settings, "edge_path_live_unwrap")
        col.label("Double Threshold:")
        col.prop(tool_settings, "double_threshold", text="")

        if mesh.show_weight:
            col.label("Show Zero Weights:")
            col.row().prop(tool_settings, "vertex_group_user", expand=True)

# ********** default tools for editmode_curve ****************


class VIEW3D_PT_tools_transform_curve(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "curve_edit"
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        col = layout.column(align=True)
        col.operator("transform.tilt", text="Tilt")
        col.operator("transform.transform", text="Shrink/Fatten").mode = 'CURVE_SHRINKFATTEN'


class VIEW3D_PT_tools_curveedit(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "curve_edit"
    bl_label = "Curve Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Curve:")
        col.operator("curve.duplicate_move", text="Duplicate")
        col.operator("curve.delete")
        col.operator("curve.cyclic_toggle")
        col.operator("curve.switch_direction")
        col.operator("curve.spline_type_set")
        col.operator("curve.radius_set")

        col = layout.column(align=True)
        col.label(text="Handles:")
        row = col.row(align=True)
        row.operator("curve.handle_type_set", text="Auto").type = 'AUTOMATIC'
        row.operator("curve.handle_type_set", text="Vector").type = 'VECTOR'
        row = col.row(align=True)
        row.operator("curve.handle_type_set", text="Align").type = 'ALIGNED'
        row.operator("curve.handle_type_set", text="Free").type = 'FREE_ALIGN'

        col = layout.column(align=True)
        col.operator("curve.normals_make_consistent")

        col = layout.column(align=True)
        col.label(text="Modeling:")
        col.operator("curve.extrude_move", text="Extrude")
        col.operator("curve.subdivide")
        col.operator("curve.smooth")
        col.operator("object.vertex_random")


class VIEW3D_PT_tools_add_curve_edit(View3DPanel, Panel):
    bl_category = "Create"
    bl_context = "curve_edit"
    bl_label = "Add Curves"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        VIEW3D_PT_tools_add_object.draw_add_curve(col, label=True)

# ********** default tools for editmode_surface ****************


class VIEW3D_PT_tools_transform_surface(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "surface_edit"
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")


class VIEW3D_PT_tools_surfaceedit(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "surface_edit"
    bl_label = "Surface Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Curve:")
        col.operator("curve.duplicate_move", text="Duplicate")
        col.operator("curve.delete")
        col.operator("curve.cyclic_toggle")
        col.operator("curve.switch_direction")

        col = layout.column(align=True)
        col.label(text="Modeling:")
        col.operator("curve.extrude", text="Extrude")
        col.operator("curve.subdivide")

        col = layout.column(align=True)
        col.label(text="Deform:")
        col.operator("object.vertex_random")


class VIEW3D_PT_tools_add_surface_edit(View3DPanel, Panel):
    bl_category = "Create"
    bl_context = "surface_edit"
    bl_label = "Add Surfaces"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        VIEW3D_PT_tools_add_object.draw_add_surface(col)


# ********** default tools for editmode_text ****************


class VIEW3D_PT_tools_textedit(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "text_edit"
    bl_label = "Text Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Set Case:")
        col.operator("font.case_set", text="To Upper").case = 'UPPER'
        col.operator("font.case_set", text="To Lower").case = 'LOWER'

        col = layout.column(align=True)
        col.label(text="Style:")
        col.operator("font.style_toggle", text="Bold").style = 'BOLD'
        col.operator("font.style_toggle", text="Italic").style = 'ITALIC'
        col.operator("font.style_toggle", text="Underline").style = 'UNDERLINE'


# ********** default tools for editmode_armature ****************


class VIEW3D_PT_tools_armatureedit_transform(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "armature_edit"
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")


class VIEW3D_PT_tools_armatureedit(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "armature_edit"
    bl_label = "Armature Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Bones:")
        col.operator("armature.bone_primitive_add", text="Add")
        col.operator("armature.duplicate_move", text="Duplicate")
        col.operator("armature.delete", text="Delete")

        col = layout.column(align=True)
        col.label(text="Modeling:")
        col.operator("armature.extrude_move")
        col.operator("armature.subdivide", text="Subdivide")

        col = layout.column(align=True)
        col.label(text="Deform:")
        col.operator("object.vertex_random")


class VIEW3D_PT_tools_armatureedit_options(View3DPanel, Panel):
    bl_category = "Options"
    bl_context = "armature_edit"
    bl_label = "Armature Options"

    def draw(self, context):
        arm = context.active_object.data

        self.layout.prop(arm, "use_mirror_x")


# ********** default tools for editmode_mball ****************


class VIEW3D_PT_tools_mballedit(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "mball_edit"
    bl_label = "Meta Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        col = layout.column(align=True)
        col.label(text="Deform:")
        col.operator("object.vertex_random")


class VIEW3D_PT_tools_add_mball_edit(View3DPanel, Panel):
    bl_category = "Create"
    bl_context = "mball_edit"
    bl_label = "Add Metaball"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        VIEW3D_PT_tools_add_object.draw_add_mball(col)


# ********** default tools for editmode_lattice ****************


class VIEW3D_PT_tools_latticeedit(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "lattice_edit"
    bl_label = "Lattice Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        col = layout.column(align=True)
        col.operator("lattice.make_regular")

        col = layout.column(align=True)
        col.label(text="Deform:")
        col.operator("object.vertex_random")


# ********** default tools for pose-mode ****************


class VIEW3D_PT_tools_posemode(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "posemode"
    bl_label = "Pose Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        col = layout.column(align=True)
        col.label(text="In-Between:")
        row = col.row(align=True)
        row.operator("pose.push", text="Push")
        row.operator("pose.relax", text="Relax")
        col.operator("pose.breakdown", text="Breakdowner")

        col = layout.column(align=True)
        col.label(text="Pose:")
        row = col.row(align=True)
        row.operator("pose.copy", text="Copy")
        row.operator("pose.paste", text="Paste")

        col = layout.column(align=True)
        col.operator("poselib.pose_add", text="Add To Library")

        draw_keyframing_tools(context, layout)

        col = layout.column(align=True)
        col.label(text="Motion Paths:")
        row = col.row(align=True)
        row.operator("pose.paths_calculate", text="Calculate")
        row.operator("pose.paths_clear", text="Clear")


class VIEW3D_PT_tools_posemode_options(View3DPanel, Panel):
    bl_category = "Options"
    bl_context = "posemode"
    bl_label = "Pose Options"

    def draw(self, context):
        arm = context.active_object.data

        self.layout.prop(arm, "use_auto_ik")

# ********** default tools for paint modes ****************


class View3DPaintPanel(UnifiedPaintPanel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'


class VIEW3D_PT_tools_brush(Panel, View3DPaintPanel):
    bl_category = "Tools"
    bl_label = "Brush"

    @classmethod
    def poll(cls, context):
        return cls.paint_settings(context)

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        settings = self.paint_settings(context)
        brush = settings.brush

        if not context.particle_edit_object:
            col = layout.split().column()
            col.template_ID_preview(settings, "brush", new="brush.add", rows=3, cols=8)

        # Particle Mode #
        if context.particle_edit_object:
            tool = settings.tool

            layout.column().prop(settings, "tool", expand=True)

            if tool != 'NONE':
                col = layout.column()
                col.prop(brush, "size", slider=True)
                if tool != 'ADD':
                    col.prop(brush, "strength", slider=True)

            if tool == 'ADD':
                col.prop(brush, "count")
                col = layout.column()
                col.prop(settings, "use_default_interpolate")
                sub = col.column(align=True)
                sub.active = settings.use_default_interpolate
                sub.prop(brush, "steps", slider=True)
                sub.prop(settings, "default_key_count", slider=True)
            elif tool == 'LENGTH':
                layout.prop(brush, "length_mode", expand=True)
            elif tool == 'PUFF':
                layout.prop(brush, "puff_mode", expand=True)
                layout.prop(brush, "use_puff_volume")

        # Sculpt Mode #

        elif context.sculpt_object and brush:
            capabilities = brush.sculpt_capabilities

            col = layout.column()

            col.separator()

            row = col.row(align=True)

            ups = toolsettings.unified_paint_settings
            if ((ups.use_unified_size and ups.use_locked_size) or
                    ((not ups.use_unified_size) and brush.use_locked_size)):
                self.prop_unified_size(row, context, brush, "use_locked_size", icon='LOCKED')
                self.prop_unified_size(row, context, brush, "unprojected_radius", slider=True, text="Radius")
            else:
                self.prop_unified_size(row, context, brush, "use_locked_size", icon='UNLOCKED')
                self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")

            self.prop_unified_size(row, context, brush, "use_pressure_size")

            # strength, use_strength_pressure, and use_strength_attenuation
            if capabilities.has_strength:
                col.separator()
                row = col.row(align=True)

                if capabilities.has_space_attenuation:
                    row.prop(brush, "use_space_attenuation", toggle=True, icon_only=True)

                self.prop_unified_strength(row, context, brush, "strength", text="Strength")
                self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            # auto_smooth_factor and use_inverse_smooth_pressure
            if capabilities.has_auto_smooth:
                col.separator()

                row = col.row(align=True)
                row.prop(brush, "auto_smooth_factor", slider=True)
                row.prop(brush, "use_inverse_smooth_pressure", toggle=True, text="")

            # normal_weight
            if capabilities.has_normal_weight:
                col.separator()
                row = col.row(align=True)
                row.prop(brush, "normal_weight", slider=True)

            # crease_pinch_factor
            if capabilities.has_pinch_factor:
                col.separator()
                row = col.row(align=True)
                row.prop(brush, "crease_pinch_factor", slider=True, text="Pinch")

            # use_original_normal and sculpt_plane
            if capabilities.has_sculpt_plane:
                col.separator()
                row = col.row(align=True)

                row.prop(brush, "use_original_normal", toggle=True, icon_only=True)

                row.prop(brush, "sculpt_plane", text="")

            if brush.sculpt_tool == 'MASK':
                col.prop(brush, "mask_tool", text="")

            # plane_offset, use_offset_pressure, use_plane_trim, plane_trim
            if capabilities.has_plane_offset:
                row = col.row(align=True)
                row.prop(brush, "plane_offset", slider=True)
                row.prop(brush, "use_offset_pressure", text="")

                col.separator()

                row = col.row()
                row.prop(brush, "use_plane_trim", text="Trim")
                row = col.row()
                row.active = brush.use_plane_trim
                row.prop(brush, "plane_trim", slider=True, text="Distance")

            # height
            if capabilities.has_height:
                row = col.row()
                row.prop(brush, "height", slider=True, text="Height")

            # use_frontface
            col.separator()
            row = col.row()
            row.prop(brush, "use_frontface", text="Front Faces Only")

            # direction
            col.separator()
            col.row().prop(brush, "direction", expand=True)

            # use_accumulate
            if capabilities.has_accumulate:
                col.separator()

                col.prop(brush, "use_accumulate")

            # use_persistent, set_persistent_base
            if capabilities.has_persistence:
                col.separator()

                ob = context.sculpt_object
                do_persistent = True

                # not supported yet for this case
                for md in ob.modifiers:
                    if md.type == 'MULTIRES':
                        do_persistent = False
                        break

                if do_persistent:
                    col.prop(brush, "use_persistent")
                    col.operator("sculpt.set_persistent_base")

        # Texture Paint Mode #

        elif context.image_paint_object and brush:
            brush_texpaint_common(self, context, layout, brush, settings, True)

        # Weight Paint Mode #
        elif context.weight_paint_object and brush:

            col = layout.column()

            row = col.row(align=True)
            self.prop_unified_weight(row, context, brush, "weight", slider=True, text="Weight")

            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            col.prop(brush, "vertex_tool", text="Blend")

            col = layout.column()
            col.prop(toolsettings, "use_auto_normalize", text="Auto Normalize")
            col.prop(toolsettings, "use_multipaint", text="Multi-Paint")

        # Vertex Paint Mode #
        elif context.vertex_paint_object and brush:
            col = layout.column()
            self.prop_unified_color_picker(col, context, brush, "color", value_slider=True)
            if settings.palette:
                col.template_palette(settings, "palette", color=True)
            self.prop_unified_color(col, context, brush, "color", text="")

            col.separator()
            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            # XXX - TODO
            # row = col.row(align=True)
            # row.prop(brush, "jitter", slider=True)
            # row.prop(brush, "use_pressure_jitter", toggle=True, text="")
            col.separator()
            col.prop(brush, "vertex_tool", text="Blend")

            col.separator()
            col.template_ID(settings, "palette", new="palette.new")


class TEXTURE_UL_texpaintslots(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        mat = data

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(item, "name", text="", emboss=False, icon_value=icon)
            if (not mat.use_nodes) and (context.scene.render.engine == 'BLENDER_RENDER'):
                mtex_index = mat.texture_paint_slots[index].index
                layout.prop(mat, "use_textures", text="", index=mtex_index)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="")

class VIEW3D_MT_tools_projectpaint_uvlayer(Menu):
    bl_label = "Clone Layer"

    def draw(self, context):
        layout = self.layout

        for i, tex in enumerate(context.active_object.data.uv_textures):
            props = layout.operator("wm.context_set_int", text=tex.name, translate=False)
            props.data_path = "active_object.data.uv_textures.active_index"
            props.value = i


class VIEW3D_PT_slots_projectpaint(View3DPanel, Panel):
    bl_context = "imagepaint"
    bl_label = "Slots"
    bl_category = "Slots"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        ob = context.active_object
        return (brush is not None and ob is not None)

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings.image_paint
        # brush = settings.brush

        ob = context.active_object
        col = layout.column()

        col.label("Painting Mode")
        col.prop(settings, "mode", text="")
        col.separator()

        if settings.mode == 'MATERIAL':
            if len(ob.material_slots) > 1:
                col.label("Materials")
                col.template_list("MATERIAL_UL_matslots", "layers",
                                  ob, "material_slots",
                                  ob, "active_material_index", rows=2)

            mat = ob.active_material
            if mat:
                col.label("Available Paint Slots")
                col.template_list("TEXTURE_UL_texpaintslots", "",
                                  mat, "texture_paint_images",
                                  mat, "paint_active_slot", rows=2)

                if (not mat.use_nodes) and (context.scene.render.engine == 'BLENDER_RENDER'):
                    row = col.row(align=True)
                    row.operator_menu_enum("paint.add_texture_paint_slot", "type")
                    row.operator("paint.delete_texture_paint_slot", text="", icon='X')

                    if mat.texture_paint_slots:
                        slot = mat.texture_paint_slots[mat.paint_active_slot]

                        col.prop(mat.texture_slots[slot.index], "blend_type")
                        col.separator()
                        col.label("UV Map")
                        col.prop_search(slot, "uv_layer", ob.data, "uv_textures", text="")

        elif settings.mode == 'IMAGE':
            mesh = ob.data
            uv_text = mesh.uv_textures.active.name if mesh.uv_textures.active else ""
            col.label("Image")
            col.template_ID(settings, "canvas")
            col.label("UV Map")
            col.menu("VIEW3D_MT_tools_projectpaint_uvlayer", text=uv_text, translate=False)

        col.separator()
        col.operator("image.save_dirty", text="Save All Images")


class VIEW3D_PT_stencil_projectpaint(View3DPanel, Panel):
    bl_context = "imagepaint"
    bl_label = "Mask"
    bl_category = "Slots"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        ob = context.active_object
        return (brush is not None and ob is not None)

    def draw_header(self, context):
        ipaint = context.tool_settings.image_paint
        self.layout.prop(ipaint, "use_stencil_layer", text="")

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint
        ob = context.active_object
        mesh = ob.data

        col = layout.column()
        col.active = ipaint.use_stencil_layer

        stencil_text = mesh.uv_texture_stencil.name if mesh.uv_texture_stencil else ""
        col.label("UV Map")
        col.menu("VIEW3D_MT_tools_projectpaint_stencil", text=stencil_text, translate=False)

        col.label("Image")
        row = col.row(align=True)
        row.template_ID(ipaint, "stencil_image")
 
        col.label("Visualization")
        row = col.row(align=True)
        row.prop(ipaint, "stencil_color", text="")
        row.prop(ipaint, "invert_stencil", text="", icon='IMAGE_ALPHA')


class VIEW3D_PT_tools_brush_overlay(Panel, View3DPaintPanel):
    bl_category = "Options"
    bl_label = "Overlay"

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and
                settings.brush and
                (context.sculpt_object or
                 context.vertex_paint_object or
                 context.weight_paint_object or
                 context.image_paint_object))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush
        tex_slot = brush.texture_slot
        tex_slot_mask = brush.mask_texture_slot

        col = layout.column()

        col.label(text="Curve:")

        row = col.row(align=True)
        if brush.use_cursor_overlay:
            row.prop(brush, "use_cursor_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
        else:
            row.prop(brush, "use_cursor_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

        sub = row.row(align=True)
        sub.prop(brush, "cursor_overlay_alpha", text="Alpha")
        sub.prop(brush, "use_cursor_overlay_override", toggle=True, text="", icon='BRUSH_DATA')

        col.active = brush.brush_capabilities.has_overlay

        if context.image_paint_object or context.sculpt_object or context.vertex_paint_object:
            col.label(text="Texture:")
            row = col.row(align=True)
            if tex_slot.map_mode != 'STENCIL':
                if brush.use_primary_overlay:
                    row.prop(brush, "use_primary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
                else:
                    row.prop(brush, "use_primary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

            sub = row.row(align=True)
            sub.prop(brush, "texture_overlay_alpha", text="Alpha")
            sub.prop(brush, "use_primary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')

        if context.image_paint_object:
            col.label(text="Mask Texture:")

            row = col.row(align=True)
            if tex_slot_mask.map_mode != 'STENCIL':
                if brush.use_secondary_overlay:
                    row.prop(brush, "use_secondary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
                else:
                    row.prop(brush, "use_secondary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

            sub = row.row(align=True)
            sub.prop(brush, "mask_overlay_alpha", text="Alpha")
            sub.prop(brush, "use_secondary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')


class VIEW3D_PT_tools_brush_texture(Panel, View3DPaintPanel):
    bl_category = "Tools"
    bl_label = "Texture"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and settings.brush and
                (context.sculpt_object or context.image_paint_object or context.vertex_paint_object))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush

        col = layout.column()

        col.template_ID_preview(brush, "texture", new="texture.new", rows=3, cols=8)

        brush_texture_settings(col, brush, context.sculpt_object)


class VIEW3D_PT_tools_mask_texture(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "imagepaint"
    bl_label = "Texture Mask"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        return (context.image_paint_object and brush)

    def draw(self, context):
        layout = self.layout

        brush = context.tool_settings.image_paint.brush

        col = layout.column()

        col.template_ID_preview(brush, "mask_texture", new="texture.new", rows=3, cols=8)

        brush_mask_texture_settings(col, brush)


class VIEW3D_PT_tools_brush_stroke(Panel, View3DPaintPanel):
    bl_category = "Tools"
    bl_label = "Stroke"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and
                settings.brush and
                (context.sculpt_object or
                 context.vertex_paint_object or
                 context.weight_paint_object or
                 context.image_paint_object))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush

        col = layout.column()

        col.label(text="Stroke Method:")

        col.prop(brush, "stroke_method", text="")

        if brush.use_anchor:
            col.separator()
            col.prop(brush, "use_edge_to_edge", "Edge To Edge")

        if brush.use_airbrush:
            col.separator()
            col.prop(brush, "rate", text="Rate", slider=True)

        if brush.use_space:
            col.separator()
            row = col.row(align=True)
            row.prop(brush, "spacing", text="Spacing")
            row.prop(brush, "use_pressure_spacing", toggle=True, text="")

        if brush.use_line or brush.use_curve:
            col.separator()
            row = col.row(align=True)
            row.prop(brush, "spacing", text="Spacing")

        if brush.use_curve:
            col.separator()
            col.template_ID(brush, "paint_curve", new="paintcurve.new")
            col.operator("paintcurve.draw")

        if context.sculpt_object:
            if brush.sculpt_capabilities.has_jitter:
                col.separator()

                row = col.row(align=True)
                row.prop(brush, "use_relative_jitter", icon_only=True)
                if brush.use_relative_jitter:
                    row.prop(brush, "jitter", slider=True)
                else:
                    row.prop(brush, "jitter_absolute")
                row.prop(brush, "use_pressure_jitter", toggle=True, text="")

            if brush.sculpt_capabilities.has_smooth_stroke:
                col = layout.column()
                col.separator()

                col.prop(brush, "use_smooth_stroke")

                sub = col.column()
                sub.active = brush.use_smooth_stroke
                sub.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
                sub.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)
        else:
            col.separator()

            row = col.row(align=True)
            row.prop(brush, "use_relative_jitter", icon_only=True)
            if brush.use_relative_jitter:
                row.prop(brush, "jitter", slider=True)
            else:
                row.prop(brush, "jitter_absolute")
            row.prop(brush, "use_pressure_jitter", toggle=True, text="")

            col = layout.column()
            col.separator()

            if brush.brush_capabilities.has_smooth_stroke:
                col.prop(brush, "use_smooth_stroke")

                sub = col.column()
                sub.active = brush.use_smooth_stroke
                sub.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
                sub.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)

        layout.prop(settings, "input_samples")


class VIEW3D_PT_tools_brush_curve(Panel, View3DPaintPanel):
    bl_category = "Tools"
    bl_label = "Curve"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and settings.brush and settings.brush.curve)

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)

        brush = settings.brush

        layout.template_curve_mapping(brush, "curve", brush=True)

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
        row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
        row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
        row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
        row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
        row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'


class VIEW3D_PT_sculpt_dyntopo(Panel, View3DPaintPanel):
    bl_category = "Tools"
    bl_label = "Dyntopo"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        sculpt = toolsettings.sculpt
        settings = self.paint_settings(context)
        brush = settings.brush

        if context.sculpt_object.use_dynamic_topology_sculpting:
            layout.operator("sculpt.dynamic_topology_toggle", icon='X', text="Disable Dyntopo")
        else:
            layout.operator("sculpt.dynamic_topology_toggle", icon='SCULPT_DYNTOPO', text="Enable Dyntopo")

        col = layout.column()
        col.active = context.sculpt_object.use_dynamic_topology_sculpting
        sub = col.column(align=True)
        sub.active = (brush and brush.sculpt_tool != 'MASK')
        if (sculpt.detail_type_method == 'CONSTANT'):
            row = sub.row(align=True)
            row.prop(sculpt, "constant_detail")
            row.operator("sculpt.sample_detail_size", text="", icon='EYEDROPPER')
        else:
            sub.prop(sculpt, "detail_size")
        sub.prop(sculpt, "detail_refine_method", text="")
        sub.prop(sculpt, "detail_type_method", text="")
        col.separator()
        col.prop(sculpt, "use_smooth_shading")
        col.operator("sculpt.optimize")
        if (sculpt.detail_type_method == 'CONSTANT'):
            col.operator("sculpt.detail_flood_fill")
        col.separator()
        col.prop(sculpt, "symmetrize_direction")
        col.operator("sculpt.symmetrize")


class VIEW3D_PT_sculpt_options(Panel, View3DPaintPanel):
    bl_category = "Options"
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw(self, context):
        layout = self.layout
        # scene = context.scene

        toolsettings = context.tool_settings
        sculpt = toolsettings.sculpt
        capabilities = sculpt.brush.sculpt_capabilities

        col = layout.column(align=True)
        col.active = capabilities.has_gravity
        col.label(text="Gravity:")
        col.prop(sculpt, "gravity", slider=True, text="Factor")
        col.prop(sculpt, "gravity_object")
        col.separator()

        layout.prop(sculpt, "use_threaded", text="Threaded Sculpt")
        layout.prop(sculpt, "show_low_resolution")
        layout.prop(sculpt, "use_deform_only")
        layout.prop(sculpt, "show_diffuse_color")

        self.unified_paint_settings(layout, context)


class VIEW3D_PT_sculpt_symmetry(Panel, View3DPaintPanel):
    bl_category = "Tools"
    bl_label = "Symmetry / Lock"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw(self, context):
        layout = self.layout

        sculpt = context.tool_settings.sculpt

        col = layout.column(align=True)
        col.label(text="Mirror:")
        row = col.row(align=True)
        row.prop(sculpt, "use_symmetry_x", text="X", toggle=True)
        row.prop(sculpt, "use_symmetry_y", text="Y", toggle=True)
        row.prop(sculpt, "use_symmetry_z", text="Z", toggle=True)

        layout.column().prop(sculpt, "radial_symmetry", text="Radial")
        layout.prop(sculpt, "use_symmetry_feather", text="Feather")

        layout.label(text="Lock:")

        row = layout.row(align=True)
        row.prop(sculpt, "lock_x", text="X", toggle=True)
        row.prop(sculpt, "lock_y", text="Y", toggle=True)
        row.prop(sculpt, "lock_z", text="Z", toggle=True)


class VIEW3D_PT_tools_brush_appearance(Panel, View3DPaintPanel):
    bl_category = "Options"
    bl_label = "Appearance"

    @classmethod
    def poll(cls, context):
        toolsettings = context.tool_settings
        return ((context.sculpt_object and toolsettings.sculpt) or
                (context.vertex_paint_object and toolsettings.vertex_paint) or
                (context.weight_paint_object and toolsettings.weight_paint) or
                (context.image_paint_object and toolsettings.image_paint))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush

        if brush is None:  # unlikely but can happen
            layout.label(text="Brush Unset")
            return

        col = layout.column()
        col.prop(settings, "show_brush")

        sub = col.column()
        sub.active = settings.show_brush

        if context.sculpt_object and context.tool_settings.sculpt:
            if brush.sculpt_capabilities.has_secondary_color:
                sub.row().prop(brush, "cursor_color_add", text="Add")
                sub.row().prop(brush, "cursor_color_subtract", text="Subtract")
            else:
                sub.prop(brush, "cursor_color_add", text="")
        else:
            sub.prop(brush, "cursor_color_add", text="")

        col.separator()

        col = col.column(align=True)
        col.prop(brush, "use_custom_icon")
        sub = col.column()
        sub.active = brush.use_custom_icon
        sub.prop(brush, "icon_filepath", text="")

# ********** default tools for weight-paint ****************


class VIEW3D_PT_tools_weightpaint(View3DPanel, Panel):
    bl_category = "Tools"
    bl_context = "weightpaint"
    bl_label = "Weight Tools"

    def draw(self, context):
        layout = self.layout
        VIEW3D_PT_tools_meshweight.draw_generic(layout)

        col = layout.column()
        col.operator("paint.weight_gradient")
        col.operator("object.vertex_group_transfer_weight", text="Transfer Weights")


class VIEW3D_PT_tools_weightpaint_options(Panel, View3DPaintPanel):
    bl_category = "Options"
    bl_context = "weightpaint"
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        wpaint = tool_settings.weight_paint

        col = layout.column()
        row = col.row()

        row.prop(wpaint, "use_normal")
        col = layout.column()
        row = col.row()
        row.prop(wpaint, "use_spray")
        row.prop(wpaint, "use_group_restrict")

        obj = context.weight_paint_object
        if obj.type == 'MESH':
            mesh = obj.data
            col.prop(mesh, "use_mirror_x")
            row = col.row()
            row.active = mesh.use_mirror_x
            row.prop(mesh, "use_mirror_topology")

        col.label("Show Zero Weights:")
        sub = col.row()
        sub.active = (not tool_settings.use_multipaint)
        sub.prop(tool_settings, "vertex_group_user", expand=True)

        self.unified_paint_settings(col, context)

# ********** default tools for vertex-paint ****************


class VIEW3D_PT_tools_vertexpaint(Panel, View3DPaintPanel):
    bl_category = "Options"
    bl_context = "vertexpaint"
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        vpaint = toolsettings.vertex_paint

        col = layout.column()
        row = col.row()
        # col.prop(vpaint, "mode", text="")
        row.prop(vpaint, "use_normal")
        col.prop(vpaint, "use_spray")

        self.unified_paint_settings(col, context)

# Commented out because the Apply button isn't an operator yet, making these settings useless
#~         col.label(text="Gamma:")
#~         col.prop(vpaint, "gamma", text="")
#~         col.label(text="Multiply:")
#~         col.prop(vpaint, "mul", text="")

# ********** default tools for texture-paint ****************


class VIEW3D_PT_tools_imagepaint_external(Panel, View3DPaintPanel):
    bl_category = "Tools"
    bl_context = "imagepaint"
    bl_label = "External"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint

        col = layout.column()
        row = col.split(align=True, percentage=0.55)
        row.operator("image.project_edit", text="Quick Edit")
        row.operator("image.project_apply", text="Apply")

        col.row().prop(ipaint, "screen_grab_size", text="")

        col.operator("paint.project_image", text="Apply Camera Image")


class VIEW3D_PT_tools_projectpaint(View3DPaintPanel, Panel):
    bl_category = "Options"
    bl_context = "imagepaint"
    bl_label = "Project Paint"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        return (brush is not None)

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint

        col = layout.column()

        col.prop(ipaint, "use_occlude")
        col.prop(ipaint, "use_backface_culling")

        row = layout.row()
        row.prop(ipaint, "use_normal_falloff")

        sub = row.row()
        sub.active = (ipaint.use_normal_falloff)
        sub.prop(ipaint, "normal_angle", text="")

        layout.prop(ipaint, "seam_bleed")
        self.unified_paint_settings(layout, context)


class VIEW3D_PT_imagepaint_options(View3DPaintPanel):
    bl_category = "Options"
    bl_label = "Options"

    @classmethod
    def poll(cls, context):
        return (context.image_paint_object and context.tool_settings.image_paint)

    def draw(self, context):
        layout = self.layout

        col = layout.column()
        self.unified_paint_settings(col, context)


class VIEW3D_MT_tools_projectpaint_stencil(Menu):
    bl_label = "Mask Layer"

    def draw(self, context):
        layout = self.layout
        for i, tex in enumerate(context.active_object.data.uv_textures):
            props = layout.operator("wm.context_set_int", text=tex.name, translate=False)
            props.data_path = "active_object.data.uv_texture_stencil_index"
            props.value = i


class VIEW3D_PT_tools_particlemode(View3DPanel, Panel):
    """Default tools for particle mode"""
    bl_context = "particlemode"
    bl_label = "Options"
    bl_category = "Tools"

    def draw(self, context):
        layout = self.layout

        pe = context.tool_settings.particle_edit
        ob = pe.object

        layout.prop(pe, "type", text="")

        ptcache = None

        if pe.type == 'PARTICLES':
            if ob.particle_systems:
                if len(ob.particle_systems) > 1:
                    layout.template_list("UI_UL_list", "particle_systems", ob, "particle_systems",
                                         ob.particle_systems, "active_index", rows=2, maxrows=3)

                ptcache = ob.particle_systems.active.point_cache
        else:
            for md in ob.modifiers:
                if md.type == pe.type:
                    ptcache = md.point_cache

        if ptcache and len(ptcache.point_caches) > 1:
            layout.template_list("UI_UL_list", "particles_point_caches", ptcache, "point_caches",
                                 ptcache.point_caches, "active_index", rows=2, maxrows=3)

        if not pe.is_editable:
            layout.label(text="Point cache must be baked")
            layout.label(text="in memory to enable editing!")

        col = layout.column(align=True)
        if pe.is_hair:
            col.active = pe.is_editable
            col.prop(pe, "use_emitter_deflect", text="Deflect emitter")
            sub = col.row(align=True)
            sub.active = pe.use_emitter_deflect
            sub.prop(pe, "emitter_distance", text="Distance")

        col = layout.column(align=True)
        col.active = pe.is_editable
        col.label(text="Keep:")
        col.prop(pe, "use_preserve_length", text="Lengths")
        col.prop(pe, "use_preserve_root", text="Root")
        if not pe.is_hair:
            col.label(text="Correct:")
            col.prop(pe, "use_auto_velocity", text="Velocity")
        col.prop(ob.data, "use_mirror_x")

        col = layout.column(align=True)
        col.active = pe.is_editable
        col.label(text="Draw:")
        col.prop(pe, "draw_step", text="Path Steps")
        if pe.is_hair:
            col.prop(pe, "show_particles", text="Children")
        else:
            if pe.type == 'PARTICLES':
                col.prop(pe, "show_particles", text="Particles")
            col.prop(pe, "use_fade_time")
            sub = col.row(align=True)
            sub.active = pe.use_fade_time
            sub.prop(pe, "fade_frames", slider=True)


# Grease Pencil tools
class VIEW3D_PT_tools_grease_pencil(GreasePencilPanel, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = "Grease Pencil"


# Note: moved here so that it's always in last position in 'Tools' panels!
class VIEW3D_PT_tools_history(View3DPanel, Panel):
    bl_category = "Tools"
    # No bl_context, we are always available!
    bl_label = "History"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        obj = context.object

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("ed.undo")
        row.operator("ed.redo")
        if obj is None or obj.mode not in {'SCULPT'}:
            # Sculpt mode does not generate an undo menu it seems...
            col.operator("ed.undo_history")

        col = layout.column(align=True)
        col.label(text="Repeat:")
        col.operator("screen.repeat_last")
        col.operator("screen.repeat_history", text="History...")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
