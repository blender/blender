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
from bl_ui.properties_paint_common import UnifiedPaintPanel


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
    row = col.row()
    row.operator("anim.keyframe_insert_menu", text="Insert")
    row.operator("anim.keyframe_delete_v3d", text="Remove")


# Grease Pencil tools
def draw_gpencil_tools(context, layout):
    col = layout.column(align=True)

    col.label(text="Grease Pencil:")

    row = col.row()
    row.operator("gpencil.draw", text="Draw").mode = 'DRAW'
    row.operator("gpencil.draw", text="Line").mode = 'DRAW_STRAIGHT'

    row = col.row()
    row.operator("gpencil.draw", text="Poly").mode = 'DRAW_POLY'
    row.operator("gpencil.draw", text="Erase").mode = 'ERASER'

    row = col.row()
    row.prop(context.tool_settings, "use_grease_pencil_sessions")


# ********** default tools for object-mode ****************

class VIEW3D_PT_tools_objectmode(View3DPanel, Panel):
    bl_context = "objectmode"
    bl_label = "Object Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")

        col = layout.column(align=True)
        col.operator("object.origin_set", text="Origin")

        col = layout.column(align=True)
        col.label(text="Object:")
        col.operator("object.duplicate_move")
        col.operator("object.delete")
        col.operator("object.join")

        active_object = context.active_object
        if active_object and active_object.type in {'MESH', 'CURVE', 'SURFACE'}:

            col = layout.column(align=True)
            col.label(text="Shading:")
            row = col.row(align=True)
            row.operator("object.shade_smooth", text="Smooth")
            row.operator("object.shade_flat", text="Flat")

        draw_keyframing_tools(context, layout)

        col = layout.column(align=True)
        col.label(text="Motion Paths:")
        col.operator("object.paths_calculate", text="Calculate Paths")
        col.operator("object.paths_clear", text="Clear Paths")

        draw_repeat_tools(context, layout)

        draw_gpencil_tools(context, layout)

# ********** default tools for editmode_mesh ****************


class VIEW3D_PT_tools_meshedit(View3DPanel, Panel):
    bl_context = "mesh_edit"
    bl_label = "Mesh Tools"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Transform:")
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")
        col.operator("transform.shrink_fatten", text="Shrink/Fatten")
        col.operator("transform.push_pull", text="Push/Pull")

        col = layout.column(align=True)
        col.label(text="Deform:")
        col.operator("transform.edge_slide")
        col.operator("mesh.noise")
        col.operator("mesh.vertices_smooth")

        col = layout.column(align=True)
        col.label(text="Add:")
        col.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Region")
        col.operator("view3d.edit_mesh_extrude_individual_move", text="Extrude Individual")
        col.operator("mesh.subdivide")
        col.operator("mesh.loopcut_slide")
        col.operator("mesh.duplicate_move", text="Duplicate")
        col.operator("mesh.spin")
        col.operator("mesh.screw")

        row = col.row(align=True)
        props = row.operator("mesh.knife_tool", text="Knife")
        props.use_occlude_geometry = True
        props.only_selected = False
        props = row.operator("mesh.knife_tool", text="Select")
        props.use_occlude_geometry = False
        props.only_selected = True

        col = layout.column(align=True)
        col.label(text="Remove:")
        col.menu("VIEW3D_MT_edit_mesh_delete")
        col.operator("mesh.merge")
        col.operator("mesh.remove_doubles")

        col = layout.column(align=True)
        col.label(text="Normals:")
        col.operator("mesh.normals_make_consistent", text="Recalculate")
        col.operator("mesh.flip_normals", text="Flip Direction")

        col = layout.column(align=True)
        col.label(text="UV Mapping:")
        col.operator("wm.call_menu", text="Unwrap").name = "VIEW3D_MT_uv_map"
        col.operator("mesh.mark_seam").clear = False
        col.operator("mesh.mark_seam", text="Clear Seam").clear = True

        col = layout.column(align=True)
        col.label(text="Shading:")
        row = col.row(align=True)
        row.operator("mesh.faces_shade_smooth", text="Smooth")
        row.operator("mesh.faces_shade_flat", text="Flat")

        draw_repeat_tools(context, layout)

        draw_gpencil_tools(context, layout)


class VIEW3D_PT_tools_meshedit_options(View3DPanel, Panel):
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
        col.active = tool_settings.proportional_edit == 'DISABLED'
        col.prop(mesh, "use_mirror_x")

        row = col.row()
        row.active = ob.data.use_mirror_x
        row.prop(mesh, "use_mirror_topology")

        col = layout.column(align=True)
        col.label("Edge Select Mode:")
        col.prop(tool_settings, "edge_path_mode", text="")
        col.prop(tool_settings, "edge_path_live_unwrap")
        col.label("Double Threshold:")
        col.prop(tool_settings, "double_threshold", text="")

# ********** default tools for editmode_curve ****************


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
        col.operator("transform.transform", text="Shrink/Fatten").mode = 'CURVE_SHRINKFATTEN'

        col = layout.column(align=True)
        col.label(text="Curve:")
        col.operator("curve.duplicate_move", text="Duplicate")
        col.operator("curve.delete")
        col.operator("curve.cyclic_toggle")
        col.operator("curve.switch_direction")
        col.operator("curve.spline_type_set")

        col = layout.column(align=True)
        col.label(text="Handles:")
        row = col.row()
        row.operator("curve.handle_type_set", text="Auto").type = 'AUTOMATIC'
        row.operator("curve.handle_type_set", text="Vector").type = 'VECTOR'
        row = col.row()
        row.operator("curve.handle_type_set", text="Align").type = 'ALIGNED'
        row.operator("curve.handle_type_set", text="Free").type = 'FREE_ALIGN'

        col = layout.column(align=True)
        col.label(text="Modeling:")
        col.operator("curve.extrude_move", text="Extrude")
        col.operator("curve.subdivide")

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
        col.label(text="Text Edit:")
        col.operator("font.text_copy", text="Copy")
        col.operator("font.text_cut", text="Cut")
        col.operator("font.text_paste", text="Paste")

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
        row = col.row()
        row.operator("pose.push", text="Push")
        row.operator("pose.relax", text="Relax")
        col.operator("pose.breakdown", text="Breakdowner")

        col = layout.column(align=True)
        col.label(text="Pose:")
        row = col.row()
        row.operator("pose.copy", text="Copy")
        row.operator("pose.paste", text="Paste")

        col = layout.column(align=True)
        col.operator("poselib.pose_add", text="Add To Library")

        draw_keyframing_tools(context, layout)

        col = layout.column(align=True)
        col.label(text="Motion Paths:")
        col.operator("pose.paths_calculate", text="Calculate Paths")
        col.operator("pose.paths_clear", text="Clear Paths")

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
                    if brush.use_space_attenuation:
                        row.prop(brush, "use_space_attenuation", toggle=True, text="", icon='LOCKED')
                    else:
                        row.prop(brush, "use_space_attenuation", toggle=True, text="", icon='UNLOCKED')

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
                row = col.row(align=True)
                col.separator()

                if brush.use_original_normal:
                    row.prop(brush, "use_original_normal", toggle=True, text="", icon='LOCKED')
                else:
                    row.prop(brush, "use_original_normal", toggle=True, text="", icon='UNLOCKED')

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

                if do_persistent:
                    col.prop(brush, "use_persistent")
                    col.operator("sculpt.set_persistent_base")

        # Texture Paint Mode #

        elif context.image_paint_object and brush:
            col = layout.column()
            col.template_color_wheel(brush, "color", value_slider=True)
            col.prop(brush, "color", text="")

            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            row = col.row(align=True)
            row.prop(brush, "jitter", slider=True)
            row.prop(brush, "use_pressure_jitter", toggle=True, text="")

            col.prop(brush, "blend", text="Blend")

            col = layout.column()
            col.active = (brush.blend not in {'ERASE_ALPHA', 'ADD_ALPHA'})
            col.prop(brush, "use_alpha")

        # Weight Paint Mode #
        elif context.weight_paint_object and brush:
            layout.prop(toolsettings, "use_auto_normalize", text="Auto Normalize")
            layout.prop(toolsettings, "use_multipaint", text="Multi-Paint")

            col = layout.column()

            row = col.row(align=True)
            self.prop_unified_weight(row, context, brush, "weight", slider=True, text="Weight")

            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            row = col.row(align=True)
            row.prop(brush, "jitter", slider=True)
            row.prop(brush, "use_pressure_jitter", toggle=True, text="")

        # Vertex Paint Mode #
        elif context.vertex_paint_object and brush:
            col = layout.column()
            col.template_color_wheel(brush, "color", value_slider=True)
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


class VIEW3D_PT_tools_brush_texture(Panel, View3DPaintPanel):
    bl_label = "Texture"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and settings.brush and (context.sculpt_object or
                             context.image_paint_object))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush
        tex_slot = brush.texture_slot

        col = layout.column()

        col.template_ID_preview(brush, "texture", new="texture.new", rows=3, cols=8)
        if brush.use_paint_image:
            col.prop(brush, "use_fixed_texture")

        if context.sculpt_object:
            #XXX duplicated from properties_texture.py

            col.label(text="Brush Mapping:")
            col.row().prop(tex_slot, "map_mode", expand=True)

            col.separator()

            col = layout.column()
            col.active = tex_slot.map_mode in {'FIXED'}
            col.label(text="Angle:")
            if brush.sculpt_capabilities.has_random_texture_angle:
                col.prop(brush, "texture_angle_source_random", text="")
            else:
                col.prop(brush, "texture_angle_source_no_random", text="")

            #row = col.row(align=True)
            #row.label(text="Angle:")
            #row.active = tex_slot.map_mode in {'FIXED', 'TILED'}

            #row = col.row(align=True)

            #col = row.column()
            #col.active = tex_slot.map_mode in {'FIXED'}
            #col.prop(brush, "use_rake", toggle=True, icon='PARTICLEMODE', text="")

            col = layout.column()
            col.active = tex_slot.map_mode in {'FIXED', 'TILED'}
            col.prop(tex_slot, "angle", text="")

            split = layout.split()
            split.prop(tex_slot, "offset")
            split.prop(tex_slot, "scale")

            col = layout.column(align=True)
            col.label(text="Sample Bias:")
            col.prop(brush, "texture_sample_bias", slider=True, text="")

            col = layout.column(align=True)
            col.active = tex_slot.map_mode in {'FIXED', 'TILED'}
            col.label(text="Overlay:")

            row = col.row()
            if brush.use_texture_overlay:
                row.prop(brush, "use_texture_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
            else:
                row.prop(brush, "use_texture_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')
            sub = row.row()
            sub.active = tex_slot.map_mode in {'FIXED', 'TILED'} and brush.use_texture_overlay
            sub.prop(brush, "texture_overlay_alpha", text="Alpha")


class VIEW3D_PT_tools_brush_stroke(Panel, View3DPaintPanel):
    bl_label = "Stroke"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and settings.brush and (context.sculpt_object or
                             context.vertex_paint_object or
                             context.weight_paint_object or
                             context.image_paint_object))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush
        image_paint = context.image_paint_object

        col = layout.column()

        if context.sculpt_object:
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
                row = col.row()
                row.active = brush.use_space
                row.prop(brush, "spacing", text="Spacing")

            if brush.sculpt_capabilities.has_smooth_stroke:
                col = layout.column()
                col.separator()

                col.prop(brush, "use_smooth_stroke")

                sub = col.column()
                sub.active = brush.use_smooth_stroke
                sub.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
                sub.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)

            if brush.sculpt_capabilities.has_jitter:
                col.separator()

                row = col.row(align=True)
                row.prop(brush, "jitter", slider=True)
                row.prop(brush, "use_pressure_jitter", toggle=True, text="")

        else:
            col.prop(brush, "use_airbrush")

            row = col.row()
            row.active = brush.use_airbrush and (not brush.use_space) and (not brush.use_anchor)
            row.prop(brush, "rate", slider=True)

            col.separator()

            if not image_paint:
                col.prop(brush, "use_smooth_stroke")

                col = layout.column()
                col.active = brush.use_smooth_stroke
                col.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
                col.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)

            col.separator()

            col = layout.column()
            col.active = brush.sculpt_capabilities.has_spacing
            col.prop(brush, "use_space")

            row = col.row()
            row.active = brush.use_space
            row.prop(brush, "spacing", text="Spacing")


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

        layout.label(text="Lock:")
        row = layout.row(align=True)
        row.prop(sculpt, "lock_x", text="X", toggle=True)
        row.prop(sculpt, "lock_y", text="Y", toggle=True)
        row.prop(sculpt, "lock_z", text="Z", toggle=True)

        layout.prop(sculpt, "use_threaded", text="Threaded Sculpt")
        layout.prop(sculpt, "show_low_resolution")
        layout.prop(sculpt, "show_brush")
        layout.prop(sculpt, "use_deform_only")

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
        row = col.row()
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

        if context.sculpt_object and context.tool_settings.sculpt:
            if brush.sculpt_capabilities.has_secondary_color:
                col.prop(brush, "cursor_color_add", text="Add Color")
                col.prop(brush, "cursor_color_subtract", text="Subtract Color")
            else:
                col.prop(brush, "cursor_color_add", text="Color")
        else:
            col.prop(brush, "cursor_color_add", text="Color")

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

        ob = context.active_object

        col = layout.column()
        col.active = ob.vertex_groups.active is not None
        col.operator("object.vertex_group_normalize_all", text="Normalize All")
        col.operator("object.vertex_group_normalize", text="Normalize")
        col.operator("object.vertex_group_mirror", text="Mirror")
        col.operator("object.vertex_group_invert", text="Invert")
        col.operator("object.vertex_group_clean", text="Clean")
        col.operator("object.vertex_group_levels", text="Levels")
        col.operator("object.vertex_group_blend", text="Blend")
        col.operator("object.vertex_group_fix", text="Fix Deforms")


class VIEW3D_PT_tools_weightpaint_options(Panel, View3DPaintPanel):
    bl_context = "weightpaint"
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        wpaint = tool_settings.weight_paint

        col = layout.column()

        col.prop(wpaint, "use_normal")
        col.prop(wpaint, "use_spray")
        col.prop(wpaint, "use_group_restrict")

        obj = context.weight_paint_object
        if obj.type == 'MESH':
            mesh = obj.data
            col.prop(mesh, "use_mirror_x")
            col.prop(mesh, "use_mirror_topology")

        self.unified_paint_settings(col, context)

# Commented out because the Apply button isn't an operator yet, making these settings useless
#~         col.label(text="Gamma:")
#~         col.prop(wpaint, "gamma", text="")
#~         col.label(text="Multiply:")
#~         col.prop(wpaint, "mul", text="")

# Also missing now:
# Soft, Vertex-Group, X-Mirror and "Clear" Operator.

# ********** default tools for vertex-paint ****************


class VIEW3D_PT_tools_vertexpaint(Panel, View3DPaintPanel):
    bl_context = "vertexpaint"
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        vpaint = toolsettings.vertex_paint

        col = layout.column()
        #col.prop(vpaint, "mode", text="")
        col.prop(vpaint, "use_all_faces")
        col.prop(vpaint, "use_normal")
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
        return (brush and brush.image_tool != 'SOFTEN')

    def draw_header(self, context):
        ipaint = context.tool_settings.image_paint

        self.layout.prop(ipaint, "use_projection", text="")

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        mesh = ob.data
        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint
        settings = toolsettings.image_paint
        use_projection = ipaint.use_projection

        col = layout.column()
        col.active = use_projection
        col.prop(ipaint, "use_occlude")
        col.prop(ipaint, "use_backface_culling")

        row = layout.row()
        row.active = (use_projection)
        row.prop(ipaint, "use_normal_falloff")

        sub = row.row()
        sub.active = (ipaint.use_normal_falloff)
        sub.prop(ipaint, "normal_angle", text="")

        split = layout.split()

        split.active = (use_projection)
        split.prop(ipaint, "use_stencil_layer", text="Stencil")

        row = split.row()
        row.active = (ipaint.use_stencil_layer)
        stencil_text = mesh.uv_texture_stencil.name if mesh.uv_texture_stencil else ""
        row.menu("VIEW3D_MT_tools_projectpaint_stencil", text=stencil_text)
        row.prop(ipaint, "invert_stencil", text="", icon='IMAGE_ALPHA')

        row = layout.row()
        row.active = (settings.brush.image_tool == 'CLONE')
        row.prop(ipaint, "use_clone_layer", text="Clone")
        clone_text = mesh.uv_texture_clone.name if mesh.uv_texture_clone else ""
        row.menu("VIEW3D_MT_tools_projectpaint_clone", text=clone_text)

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
            props = layout.operator("wm.context_set_int", text=tex.name)
            props.data_path = "active_object.data.uv_texture_clone_index"
            props.value = i


class VIEW3D_MT_tools_projectpaint_stencil(Menu):
    bl_label = "Mask Layer"

    def draw(self, context):
        layout = self.layout
        for i, tex in enumerate(context.active_object.data.uv_textures):
            props = layout.operator("wm.context_set_int", text=tex.name)
            props.data_path = "active_object.data.uv_texture_stencil_index"
            props.value = i


class VIEW3D_PT_tools_particlemode(View3DPanel, Panel):
    '''default tools for particle mode'''
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
                    layout.template_list(ob, "particle_systems", ob.particle_systems, "active_index", rows=2, maxrows=3)

                ptcache = ob.particle_systems.active.point_cache
        else:
            for md in ob.modifiers:
                if md.type == pe.type:
                    ptcache = md.point_cache

        if ptcache and len(ptcache.point_caches) > 1:
            layout.template_list(ptcache, "point_caches", ptcache.point_caches, "active_index", rows=2, maxrows=3)

        if not pe.is_editable:
            layout.label(text="Point cache must be baked")
            layout.label(text="in memory to enable editing!")

        col = layout.column(align=True)
        if pe.is_hair:
            col.active = pe.is_editable
            col.prop(pe, "use_emitter_deflect", text="Deflect emitter")
            sub = col.row()
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
            sub = col.row()
            sub.active = pe.use_fade_time
            sub.prop(pe, "fade_frames", slider=True)

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
