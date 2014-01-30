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
from bpy.types import Menu, Panel
from bl_ui.properties_paint_common import (
        UnifiedPaintPanel,
        brush_texture_settings,
        brush_mask_texture_settings,
        )


class View3DPanel():
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'


# **************** standard tool clusters ******************

# History/Repeat tools
def draw_repeat_tools(context, layout):
    col = layout.column(align=True)
    col.label(text="Repeat:")
    col.operator("screen.repeat_last")
    col.operator("screen.repeat_history", text="History...")

# Keyframing tools
def draw_keyframing_tools(context, layout):
    col = layout.column(align=True)
    col.label(text="Keyframes:")
    row = col.row(align=True)
    row.operator("anim.keyframe_insert_menu", text="Insert")
    row.operator("anim.keyframe_delete_v3d", text="Remove")

# Grease Pencil tools
def draw_gpencil_tools(context, layout):
    col = layout.column(align=True)

    col.label(text="Grease Pencil:")

    row = col.row(align=True)
    row.operator("gpencil.draw", text="Draw").mode = 'DRAW'
    row.operator("gpencil.draw", text="Line").mode = 'DRAW_STRAIGHT'
    row = col.row(align=True)
    row.operator("gpencil.draw", text="Poly").mode = 'DRAW_POLY'
    row.operator("gpencil.draw", text="Erase").mode = 'ERASER'

    col.separator()
    
    col.prop(context.tool_settings, "use_grease_pencil_sessions")

    col.separator()
    
    col.label(text="Measure:")
    col.operator("view3d.ruler")

# ********** default tools for object-mode ****************

class VIEW3D_PT_tools_add_mesh(View3DPanel, Panel):
    bl_category = "Create"
    bl_context = "objectmode"
    bl_label = "Add Primitive"
    
    def draw (self, context):
        layout = self.layout
        
        col = layout.column(align=True)
        col.label(text="Mesh:")
        col.operator("mesh.primitive_plane_add", text="Plane", icon="MESH_PLANE")
        col.operator("mesh.primitive_cube_add", text="Cube", icon="MESH_CUBE")
        col.operator("mesh.primitive_circle_add", text="Circle", icon="MESH_CIRCLE")
        col.operator("mesh.primitive_uv_sphere_add", text="UV Sphere", icon="MESH_UVSPHERE")
        col.operator("mesh.primitive_ico_sphere_add", text="Ico Sphere", icon="MESH_ICOSPHERE")
        col.operator("mesh.primitive_cylinder_add", text="Cylinder", icon="MESH_CYLINDER")
        col.operator("mesh.primitive_cone_add", text="Cone", icon="MESH_CONE")
        col.operator("mesh.primitive_torus_add", text="Torus", icon="MESH_TORUS")
        col.operator("mesh.primitive_monkey_add", text="Monkey", icon="MESH_MONKEY")   
        
        col = layout.column(align=True)
        col.label(text="Curve:")
        col.operator("curve.primitive_bezier_curve_add", text="Curve", icon="CURVE_BEZCURVE")
        col.operator("curve.primitive_bezier_circle_add", text="Circle", icon="CURVE_BEZCIRCLE")
        col.operator("curve.primitive_nurbs_path_add", text="Path" , icon="CURVE_PATH")

        col.label(text="Lamp:")
        col.operator("object.lamp_add", text="Point", icon="LAMP_POINT").type='POINT'
        col.operator("object.lamp_add", text="Sun", icon="LAMP_SUN").type='SUN'
        col.operator("object.lamp_add", text="Spot", icon="LAMP_SPOT").type='SPOT'
        col.operator("object.lamp_add", text="Hemi", icon="LAMP_HEMI").type='HEMI'
        col.operator("object.lamp_add", text="Area", icon="LAMP_AREA").type='AREA'
              
        col.label(text="Other:")
        col.operator("object.text_add", text="Text", icon ="OUTLINER_OB_FONT")
        col.operator("object.armature_add",text="Armature", icon="OUTLINER_OB_ARMATURE")
        col.operator("object.add", text="Lattice", icon="OUTLINER_OB_LATTICE").type='LATTICE'
        col.operator("object.empty_add", text="Empty", icon="OUTLINER_OB_EMPTY").type='PLAIN_AXES'
        col.operator("object.camera_add", text="Camera", icon="OUTLINER_OB_CAMERA")
        

class VIEW3D_PT_tools_basic(View3DPanel, Panel):
    bl_category = "Basic"
    bl_context = "objectmode"
    bl_label = "Object Operations"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")
            
        active_object = context.active_object
        if active_object and active_object.type in {'MESH', 'CURVE', 'SURFACE'}:

            col = layout.column(align=True)
            col.operator("transform.mirror", text="Mirror")
            
            col = layout.column(align=True)
            col.operator("object.origin_set", text="Set Origin")

            col = layout.column(align=True)
            col.label(text="Operations:")
            col.operator("object.duplicate_move", text="Duplicate")
            col.operator("object.duplicate_move_linked", text="Duplicate Linked")
            
            col = layout.column(align=True)
            col.operator("object.join")
            col.operator("object.delete")

            col = layout.column(align=True)
            col.label(text="Shading:")
            row = col.row(align=True)
            row.operator("object.shade_smooth", text="Smooth")
            row.operator("object.shade_flat", text="Flat")


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

class VIEW3D_PT_tools_rigidbody(View3DPanel, Panel):
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
    bl_category = "Mesh Tools"
    bl_context = "mesh_edit"
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")
        col.operator("transform.shrink_fatten", text="Shrink/Fatten")
        col.operator("transform.push_pull", text="Push/Pull")

class VIEW3D_PT_tools_meshedit(View3DPanel, Panel):
    bl_category = "Mesh Tools"
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

        draw_repeat_tools(context, layout)


class VIEW3D_PT_tools_shading(View3DPanel, Panel):
    bl_category = "Shading / UVs"
    bl_context = "mesh_edit"
    bl_label = "Shading"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Shading:")
        row = col.row(align=True)
        row.operator("mesh.faces_shade_smooth", text="Smooth")
        row.operator("mesh.faces_shade_flat", text="Flat")

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


class VIEW3D_PT_tools_add_mesh_edit(View3DPanel, Panel):
    bl_category = "Create"
    bl_context = "mesh_edit"
    bl_label = "Add Meshes"
    
    def draw (self, context):
        layout = self.layout
        
        col = layout.column(align=True)
        col.label(text="Primitives:")
        col.operator("mesh.primitive_plane_add", text="Plane", icon="MESH_PLANE")
        col.operator("mesh.primitive_cube_add", text="Cube", icon="MESH_CUBE")
        col.operator("mesh.primitive_circle_add", text="Circle", icon="MESH_CIRCLE")
        col.operator("mesh.primitive_uv_sphere_add", text="UV Sphere", icon="MESH_UVSPHERE")
        col.operator("mesh.primitive_ico_sphere_add", text="Ico Sphere", icon="MESH_ICOSPHERE")
        col.operator("mesh.primitive_cylinder_add", text="Cylinder", icon="MESH_CYLINDER")
        col.operator("mesh.primitive_cone_add", text="Cone", icon="MESH_CONE")
        col.operator("mesh.primitive_torus_add", text="Torus", icon="MESH_TORUS")   
        
        col = layout.column(align=True)
        col.label(text="Special:")
        col.operator("mesh.primitive_grid_add", text="Grid", icon="MESH_GRID")
        col.operator("mesh.primitive_monkey_add", text="Monkey", icon="MESH_MONKEY")

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

class VIEW3D_PT_tools_add_curve_edit(View3DPanel, Panel):
    bl_category = "Create"
    bl_context = "curve_edit"
    bl_label = "Add Curves"
    
    def draw (self, context):
        layout = self.layout
        
        col = layout.column(align=True)
        
        col.label(text="Bezier:")
        col.operator("curve.primitive_bezier_curve_add", text="Bezier Curve", icon="CURVE_BEZCURVE")
        col.operator("curve.primitive_bezier_circle_add", text="Bezier Circle", icon="CURVE_BEZCIRCLE")
        
        col.label(text="Nurbs:")
        col.operator("curve.primitive_nurbs_curve_add", text="Nurbs Curve", icon="CURVE_NCURVE")
        col.operator("curve.primitive_nurbs_circle_add", text="Nurbs Circle", icon="CURVE_NCIRCLE")
        col.operator("curve.primitive_nurbs_path_add", text="Nurbs Path" , icon="CURVE_PATH")

class VIEW3D_PT_tools_curveedit(View3DPanel, Panel):
    bl_context = "curve_edit"
    bl_label = "Curve Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        col = layout.column(align=True)
        col.operator("transform.tilt", text="Tilt")
        col.operator("transform.transform", text="Scale Feather").mode = 'CURVE_SHRINKFATTEN'

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

        draw_repeat_tools(context, layout)

        draw_gpencil_tools(context, layout)

# ********** default tools for editmode_surface ****************


class VIEW3D_PT_tools_surfaceedit(View3DPanel, Panel):
    bl_context = "surface_edit"
    bl_label = "Surface Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

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

        draw_repeat_tools(context, layout)

        draw_gpencil_tools(context, layout)

# ********** default tools for editmode_text ****************


class VIEW3D_PT_tools_textedit(View3DPanel, Panel):
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

        draw_repeat_tools(context, layout)


# ********** default tools for editmode_armature ****************


class VIEW3D_PT_tools_armatureedit(View3DPanel, Panel):
    bl_context = "armature_edit"
    bl_label = "Armature Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        col = layout.column(align=True)
        col.label(text="Bones:")
        col.operator("armature.bone_primitive_add", text="Add")
        col.operator("armature.duplicate_move", text="Duplicate")
        col.operator("armature.delete", text="Delete")

        col = layout.column(align=True)
        col.label(text="Modeling:")
        col.operator("armature.extrude_move")
        col.operator("armature.subdivide", text="Subdivide")

        draw_repeat_tools(context, layout)

        draw_gpencil_tools(context, layout)


class VIEW3D_PT_tools_armatureedit_options(View3DPanel, Panel):
    bl_context = "armature_edit"
    bl_label = "Armature Options"

    def draw(self, context):
        arm = context.active_object.data

        self.layout.prop(arm, "use_mirror_x")

# ********** default tools for editmode_mball ****************


class VIEW3D_PT_tools_mballedit(View3DPanel, Panel):
    bl_context = "mball_edit"
    bl_label = "Meta Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        draw_repeat_tools(context, layout)

        draw_gpencil_tools(context, layout)

# ********** default tools for editmode_lattice ****************


class VIEW3D_PT_tools_latticeedit(View3DPanel, Panel):
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

        draw_repeat_tools(context, layout)

        draw_gpencil_tools(context, layout)


# ********** default tools for pose-mode ****************


class VIEW3D_PT_tools_posemode(View3DPanel, Panel):
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

        draw_repeat_tools(context, layout)

        draw_gpencil_tools(context, layout)


class VIEW3D_PT_tools_posemode_options(View3DPanel, Panel):
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
            col = layout.column()

            if brush.image_tool == 'DRAW' and brush.blend not in ('ERASE_ALPHA', 'ADD_ALPHA'):
                col.template_color_picker(brush, "color", value_slider=True)
                col.prop(brush, "color", text="")

            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            col.prop(brush, "blend", text="Blend")

            col = layout.column()
            col.active = (brush.blend not in {'ERASE_ALPHA', 'ADD_ALPHA'})
            col.prop(brush, "use_alpha")

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
            col.template_color_picker(brush, "color", value_slider=True)
            col.prop(brush, "color", text="")

            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            # XXX - TODO
            #row = col.row(align=True)
            #row.prop(brush, "jitter", slider=True)
            #row.prop(brush, "use_pressure_jitter", toggle=True, text="")

            col.prop(brush, "vertex_tool", text="Blend")


class VIEW3D_PT_tools_brush_overlay(Panel, View3DPaintPanel):
    bl_label = "Overlay"
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
        tex_slot = brush.texture_slot

        col = layout.column()

        col.template_ID_preview(brush, "texture", new="texture.new", rows=3, cols=8)

        brush_texture_settings(col, brush, context.sculpt_object)


class VIEW3D_PT_tools_mask_texture(View3DPanel, Panel):
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
        tex_slot_alpha = brush.mask_texture_slot

        col = layout.column()

        col.template_ID_preview(brush, "mask_texture", new="texture.new", rows=3, cols=8)

        brush_mask_texture_settings(col, brush)


class VIEW3D_PT_tools_brush_stroke(Panel, View3DPaintPanel):
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

        if context.sculpt_object:
            col.prop(brush, "sculpt_stroke_method", text="")
        else:
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
            row.active = brush.use_space
            row.prop(brush, "spacing", text="Spacing")
            row.prop(brush, "use_pressure_spacing", toggle=True, text="")

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

            col.prop(brush, "use_smooth_stroke")

            sub = col.column()
            sub.active = brush.use_smooth_stroke
            sub.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
            sub.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)

        layout.prop(settings, "input_samples")


class VIEW3D_PT_tools_brush_curve(Panel, View3DPaintPanel):
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

        row = layout.row(align=True)
        row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
        row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
        row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
        row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
        row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
        row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'


class VIEW3D_PT_sculpt_topology(Panel, View3DPaintPanel):
    bl_label = "Topology"
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
            layout.operator("sculpt.dynamic_topology_toggle", icon='X', text="Disable Dynamic")
        else:
            layout.operator("sculpt.dynamic_topology_toggle", icon='SCULPT_DYNTOPO', text="Enable Dynamic")

        col = layout.column()
        col.active = context.sculpt_object.use_dynamic_topology_sculpting
        sub = col.column(align=True)
        sub.active = brush and brush.sculpt_tool not in ('MASK')
        sub.prop(sculpt, "detail_size")
        sub.prop(sculpt, "detail_refine_method", text="")
        col.separator()
        col.prop(sculpt, "use_smooth_shading")
        col.operator("sculpt.optimize")
        col.separator()
        col.prop(sculpt, "symmetrize_direction")
        col.operator("sculpt.symmetrize")


class VIEW3D_PT_sculpt_options(Panel, View3DPaintPanel):
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        sculpt = toolsettings.sculpt
        capabilities = sculpt.brush.sculpt_capabilities

        col = layout.column(align=True)
        col.active = capabilities.has_gravity
        col.label(text="Gravity:")
        col.prop(sculpt, "gravity", slider=True, text="Factor")
        col.prop(sculpt, "gravity_object")

        layout.label(text="Lock:")
        
        row = layout.row(align=True)
        row.prop(sculpt, "lock_x", text="X", toggle=True)
        row.prop(sculpt, "lock_y", text="Y", toggle=True)
        row.prop(sculpt, "lock_z", text="Z", toggle=True)

        layout.prop(sculpt, "use_threaded", text="Threaded Sculpt")
        layout.prop(sculpt, "show_low_resolution")
        layout.prop(sculpt, "use_deform_only")
        layout.prop(sculpt, "show_diffuse_color")

        self.unified_paint_settings(layout, context)


class VIEW3D_PT_sculpt_symmetry(Panel, View3DPaintPanel):
    bl_label = "Symmetry"
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


class VIEW3D_PT_tools_brush_appearance(Panel, View3DPaintPanel):
    bl_label = "Appearance"
    bl_options = {'DEFAULT_CLOSED'}

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

        col = col.column()
        col.active = settings.show_brush

        if context.sculpt_object and context.tool_settings.sculpt:
            if brush.sculpt_capabilities.has_secondary_color:
                col.row().prop(brush, "cursor_color_add", text="Add")
                col.row().prop(brush, "cursor_color_subtract", text="Subtract")
            else:
                col.prop(brush, "cursor_color_add", text="")
        else:
            col.prop(brush, "cursor_color_add", text="")

        layout.separator()

        col = layout.column(align=True)
        col.prop(brush, "use_custom_icon")
        if brush.use_custom_icon:
            col.prop(brush, "icon_filepath", text="")

# ********** default tools for weight-paint ****************


class VIEW3D_PT_tools_weightpaint(View3DPanel, Panel):
    bl_context = "weightpaint"
    bl_label = "Weight Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column()
        col.operator("object.vertex_group_normalize_all", text="Normalize All")
        col.operator("object.vertex_group_normalize", text="Normalize")
        col.operator("object.vertex_group_mirror", text="Mirror")
        col.operator("object.vertex_group_invert", text="Invert")
        col.operator("object.vertex_group_clean", text="Clean")
        col.operator("object.vertex_group_quantize", text="Quantize")
        col.operator("object.vertex_group_levels", text="Levels")
        col.operator("object.vertex_group_blend", text="Blend")
        col.operator("object.vertex_group_transfer_weight", text="Transfer Weights")
        col.operator("object.vertex_group_limit_total", text="Limit Total")
        col.operator("object.vertex_group_fix", text="Fix Deforms")
        col.operator("paint.weight_gradient")


class VIEW3D_PT_tools_weightpaint_options(Panel, View3DPaintPanel):
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
    bl_context = "vertexpaint"
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        vpaint = toolsettings.vertex_paint

        col = layout.column()
        row = col.row()
        #col.prop(vpaint, "mode", text="")
        row.prop(vpaint, "use_normal")
        col.prop(vpaint, "use_spray")

        self.unified_paint_settings(col, context)

# Commented out because the Apply button isn't an operator yet, making these settings useless
#~         col.label(text="Gamma:")
#~         col.prop(vpaint, "gamma", text="")
#~         col.label(text="Multiply:")
#~         col.prop(vpaint, "mul", text="")

# ********** default tools for texture-paint ****************


class VIEW3D_PT_tools_projectpaint(View3DPanel, Panel):
    bl_context = "imagepaint"
    bl_label = "Project Paint"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        return (brush is not None)

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        mesh = ob.data
        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint
        settings = toolsettings.image_paint

        col = layout.column()
        col.prop(ipaint, "use_occlude")
        col.prop(ipaint, "use_backface_culling")

        row = layout.row()
        row.prop(ipaint, "use_normal_falloff")

        sub = row.row()
        sub.active = (ipaint.use_normal_falloff)
        sub.prop(ipaint, "normal_angle", text="")

        split = layout.split()

        split.prop(ipaint, "use_stencil_layer", text="Stencil")

        row = split.row()
        row.active = (ipaint.use_stencil_layer)
        stencil_text = mesh.uv_texture_stencil.name if mesh.uv_texture_stencil else ""
        row.menu("VIEW3D_MT_tools_projectpaint_stencil", text=stencil_text, translate=False)
        row.prop(ipaint, "invert_stencil", text="", icon='IMAGE_ALPHA')

        col = layout.column()
        col.active = (settings.brush.image_tool == 'CLONE')
        col.prop(ipaint, "use_clone_layer", text="Clone from UV map")
        clone_text = mesh.uv_texture_clone.name if mesh.uv_texture_clone else ""
        col.menu("VIEW3D_MT_tools_projectpaint_clone", text=clone_text, translate=False)

        layout.prop(ipaint, "seam_bleed")

        col = layout.column()
        col.label(text="External Editing:")

        row = col.split(align=True, percentage=0.55)
        row.operator("image.project_edit", text="Quick Edit")
        row.operator("image.project_apply", text="Apply")

        col.row().prop(ipaint, "screen_grab_size", text="")

        col.operator("paint.project_image", text="Apply Camera Image")
        col.operator("image.save_dirty", text="Save All Edited")


class VIEW3D_PT_imagepaint_options(View3DPaintPanel):
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.image_paint_object and context.tool_settings.image_paint)

    def draw(self, context):
        layout = self.layout

        col = layout.column()
        self.unified_paint_settings(col, context)


class VIEW3D_MT_tools_projectpaint_clone(Menu):
    bl_label = "Clone Layer"

    def draw(self, context):
        layout = self.layout

        for i, tex in enumerate(context.active_object.data.uv_textures):
            props = layout.operator("wm.context_set_int", text=tex.name, translate=False)
            props.data_path = "active_object.data.uv_texture_clone_index"
            props.value = i


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
class VIEW3D_PT_tools_greasepencil(View3DPanel, Panel):
    bl_category = "Grease Pencil"
    bl_label = "Grease Pencil"

    def draw(self, context):
        layout = self.layout
        draw_gpencil_tools(context, layout)

class VIEW3D_PT_tools_objectmode(View3DPanel, Panel):
    bl_category = "History"
    bl_context = "objectmode"
    bl_label = "History"

    def draw(self, context):
        layout = self.layout     

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("ed.undo")
        row.operator("ed.redo")
        col.operator("ed.undo_history")
        
        draw_repeat_tools(context, layout)

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
