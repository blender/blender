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
from bpy.types import Header, Menu, Panel
from .properties_paint_common import UnifiedPaintPanel
from .properties_grease_pencil_common import GreasePencilDataPanel
from bpy.app.translations import contexts as i18n_contexts


class VIEW3D_HT_header(Header):
    bl_space_type = 'VIEW_3D'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        shading = view.shading
        # mode_string = context.mode
        obj = context.active_object
        overlay = view.overlay
        tool_settings = context.tool_settings

        row = layout.row(align=True)
        row.template_header()

        object_mode = 'OBJECT' if obj is None else obj.mode

        act_mode_item = bpy.types.Object.bl_rna.properties["mode"].enum_items[object_mode]
        layout.operator_menu_enum("object.mode_set", "mode", text=act_mode_item.name, icon=act_mode_item.icon)
        del act_mode_item

        layout.template_header_3D_mode()

        # Contains buttons like Mode, Pivot, Layer, Mesh Select Mode...
        shading_type = view.shading.type
        shading_item = bpy.types.View3DShading.bl_rna.properties["type"].enum_items[shading_type]

        if obj:
            # Set above:
            # object_mode = obj.mode

            # Particle edit
            if object_mode == 'PARTICLE_EDIT':
                row = layout.row()
                row.prop(tool_settings.particle_edit, "select_mode", text="", expand=True)

            # Occlude geometry
            if (
                    (((shading.type not in {'SOLID', 'TEXTURED'}) or not shading.show_xray) and
                     (object_mode == 'PARTICLE_EDIT' or (object_mode == 'EDIT' and obj.type == 'MESH'))) or
                    (object_mode in {'WEIGHT_PAINT', 'VERTEX_PAINT'})
            ):
                row = layout.row()
                row.prop(view, "use_occlude_geometry", text="")

        # Pose
        if obj and object_mode == 'POSE':
            row = layout.row(align=True)
            row.operator("pose.copy", text="", icon='COPYDOWN')
            row.operator("pose.paste", text="", icon='PASTEDOWN').flipped = False
            row.operator("pose.paste", text="", icon='PASTEFLIPDOWN').flipped = True

        # Grease Pencil
        if obj and obj.type == 'GPENCIL' and context.gpencil_data:
            gpd = context.gpencil_data

            if gpd.is_stroke_paint_mode:
                row = layout.row(align=True)
                row.popover(
                    panel="VIEW3D_PT_tools_grease_pencil_shapes",
                    text="Shapes"
                )

            if gpd.use_stroke_edit_mode or gpd.is_stroke_sculpt_mode or gpd.is_stroke_weight_mode:
                row = layout.row(align=True)
                row.prop(gpd, "use_multiedit", text="", icon="FORCE_HARMONIC")

                sub = row.row(align=True)
                sub.active = gpd.use_multiedit
                sub.popover(
                    panel="VIEW3D_PT_gpencil_multi_frame",
                    text="Multiframe"
                )

            if gpd.use_stroke_edit_mode:
                row = layout.row(align=True)
                row.operator("gpencil.copy", text="", icon='COPYDOWN')
                row.operator("gpencil.paste", text="", icon='PASTEDOWN')

                row = layout.row(align=True)
                row.prop(tool_settings.gpencil_sculpt, "use_select_mask", text="")

                row.popover(
                    panel="VIEW3D_PT_tools_grease_pencil_interpolate",
                    text="Interpolate"
                )

        VIEW3D_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        # Mode & Transform Settings
        scene = context.scene

        # Orientation
        if object_mode in {'OBJECT', 'EDIT', 'POSE', 'GPENCIL_EDIT'}:
            orientation = scene.transform_orientation
            current_orientation = scene.current_orientation

            if not current_orientation:
                trans_orientation = \
                    bpy.types.Scene.bl_rna.properties["transform_orientation"].enum_items[orientation]
                trans_icon = getattr(trans_orientation, "icon", "BLANK1")
                trans_name = getattr(trans_orientation, "name", "Orientation")
            else:
                trans_icon = 'VISIBLE_IPO_OFF'
                trans_name = getattr(current_orientation, "name", "Orientation")

            row = layout.row(align=True)
            row.popover(
                panel="VIEW3D_PT_transform_orientations",
                text=trans_name,
                icon=trans_icon,
            )

        # Snap
        show_snap = False
        if obj is None:
            show_snap = True
        else:
            if object_mode not in {'SCULPT', 'VERTEX_PAINT', 'WEIGHT_PAINT', 'TEXTURE_PAINT',
                                   'GPENCIL_PAINT', 'GPENCIL_SCULPT', 'GPENCIL_WEIGHT'}:
                show_snap = True
            else:

                from .properties_paint_common import UnifiedPaintPanel
                paint_settings = UnifiedPaintPanel.paint_settings(context)

                if paint_settings:
                    brush = paint_settings.brush
                    if brush and brush.stroke_method == 'CURVE':
                        show_snap = True

        if show_snap:
            snap_items = bpy.types.ToolSettings.bl_rna.properties['snap_elements'].enum_items
            for elem in tool_settings.snap_elements:
                # TODO: Display multiple icons.
                # (Currently only one of the enabled modes icons is displayed)
                icon = snap_items[elem].icon
                break
            else:
                icon = 'NONE'

            row = layout.row(align=True)
            row.prop(tool_settings, "use_snap", text="")

            sub = row.row(align=True)
            sub.popover(
                panel="VIEW3D_PT_snapping",
                icon=icon,
                text="",
            )

        # Proportional editing
        if obj:
            gpd = context.gpencil_data
            if gpd is not None and obj.type == 'GPENCIL':
                if gpd.use_stroke_edit_mode or gpd.is_stroke_sculpt_mode:
                    row = layout.row(align=True)
                    row.prop(tool_settings, "proportional_edit", icon_only=True)

                    sub = row.row(align=True)
                    sub.active = tool_settings.proportional_edit != 'DISABLED'
                    sub.prop(tool_settings, "proportional_edit_falloff", icon_only=True)

            elif object_mode in {'EDIT', 'PARTICLE_EDIT'}:
                row = layout.row(align=True)
                row.prop(tool_settings, "proportional_edit", icon_only=True)
                sub = row.row(align=True)
                sub.active = tool_settings.proportional_edit != 'DISABLED'
                sub.prop(tool_settings, "proportional_edit_falloff", icon_only=True)

            elif object_mode == 'OBJECT':
                row = layout.row(align=True)
                row.prop(tool_settings, "use_proportional_edit_objects", icon_only=True)
                sub = row.row(align=True)
                sub.active = tool_settings.use_proportional_edit_objects
                sub.prop(tool_settings, "proportional_edit_falloff", icon_only=True)
        else:
            if context.gpencil_data and context.gpencil_data.use_stroke_edit_mode:
                row = layout.row(align=True)
                row.prop(tool_settings, "proportional_edit", icon_only=True)
                sub = row.row(align=True)
                sub.active = tool_settings.proportional_edit != 'DISABLED'
                sub.prop(tool_settings, "proportional_edit_falloff", icon_only=True)

        # Pivot
        if object_mode in {'OBJECT', 'EDIT', 'POSE', 'GPENCIL_EDIT', 'GPENCIL_SCULPT'}:
            pivot_point = tool_settings.transform_pivot_point
            act_pivot_point = bpy.types.ToolSettings.bl_rna.properties["transform_pivot_point"].enum_items[pivot_point]
            row = layout.row(align=True)
            row.popover(
                panel="VIEW3D_PT_pivot_point",
                icon=act_pivot_point.icon,
                text="",
            )

        layout.separator_spacer()

        # Viewport Settings
        layout.popover(
            panel="VIEW3D_PT_object_type_visibility",
            icon_value=view.icon_from_show_object_viewport,
            text="",
        )

        row = layout.row(align=True)
        row.prop(overlay, "show_overlays", icon='WIRE', text="")
        sub = row.row(align=True)
        sub.active = overlay.show_overlays
        sub.popover(panel="VIEW3D_PT_overlay")

        row = layout.row(align=True)
        row.prop(shading, "type", text="", expand=True)
        sub = row.row(align=True)
        sub.enabled = shading.type != 'RENDERED'
        sub.popover(panel="VIEW3D_PT_shading")


class VIEW3D_MT_editor_menus(Menu):
    bl_space_type = 'VIEW3D_MT_editor_menus'
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        obj = context.active_object
        mode_string = context.mode
        edit_object = context.edit_object
        gp_edit = obj and obj.mode in {'GPENCIL_EDIT', 'GPENCIL_PAINT', 'GPENCIL_SCULPT', 'GPENCIL_WEIGHT'}

        layout.menu("VIEW3D_MT_view")

        # Select Menu
        if gp_edit:
            if mode_string not in {'GPENCIL_PAINT', 'GPENCIL_WEIGHT'}:
                layout.menu("VIEW3D_MT_select_gpencil")
        elif mode_string in {'PAINT_WEIGHT', 'PAINT_VERTEX', 'PAINT_TEXTURE'}:
            mesh = obj.data
            if mesh.use_paint_mask:
                layout.menu("VIEW3D_MT_select_paint_mask")
            elif mesh.use_paint_mask_vertex and mode_string in {'PAINT_WEIGHT', 'PAINT_VERTEX'}:
                layout.menu("VIEW3D_MT_select_paint_mask_vertex")
        elif mode_string != 'SCULPT':
            layout.menu("VIEW3D_MT_select_%s" % mode_string.lower())

        if gp_edit:
            pass
        elif mode_string == 'OBJECT':
            layout.menu("INFO_MT_add", text="Add")
        elif mode_string == 'EDIT_MESH':
            layout.menu("INFO_MT_mesh_add", text="Add")
        elif mode_string == 'EDIT_CURVE':
            layout.menu("INFO_MT_curve_add", text="Add")
        elif mode_string == 'EDIT_SURFACE':
            layout.menu("INFO_MT_surface_add", text="Add")
        elif mode_string == 'EDIT_METABALL':
            layout.menu("INFO_MT_metaball_add", text="Add")
        elif mode_string == 'EDIT_ARMATURE':
            layout.menu("INFO_MT_edit_armature_add", text="Add")

        if gp_edit:
            if obj and obj.mode == 'GPENCIL_PAINT':
                layout.menu("VIEW3D_MT_paint_gpencil")
            elif obj and obj.mode == 'GPENCIL_EDIT':
                layout.menu("VIEW3D_MT_edit_gpencil")
            elif obj and obj.mode == 'GPENCIL_SCULPT':
                layout.menu("VIEW3D_MT_sculpt_gpencil")
            elif obj and obj.mode == 'GPENCIL_WEIGHT':
                layout.menu("VIEW3D_MT_weight_gpencil")

        elif edit_object:
            layout.menu("VIEW3D_MT_edit_%s" % edit_object.type.lower())

            if mode_string == 'EDIT_MESH':
                layout.menu("VIEW3D_MT_edit_mesh_vertices")
                layout.menu("VIEW3D_MT_edit_mesh_edges")
                layout.menu("VIEW3D_MT_edit_mesh_faces")
                layout.menu("VIEW3D_MT_uv_map", text="UV")

        elif obj:
            if mode_string != 'PAINT_TEXTURE':
                layout.menu("VIEW3D_MT_%s" % mode_string.lower())
            if mode_string in {'SCULPT', 'PAINT_VERTEX', 'PAINT_WEIGHT', 'PAINT_TEXTURE'}:
                layout.menu("VIEW3D_MT_brush")
            if mode_string == 'SCULPT':
                layout.menu("VIEW3D_MT_hide_mask")
        else:
            layout.menu("VIEW3D_MT_object")


# ********** Menu **********


# ********** Utilities **********


class ShowHideMenu:
    bl_label = "Show/Hide"
    _operator_name = ""

    def draw(self, context):
        layout = self.layout

        layout.operator("%s.reveal" % self._operator_name, text="Show Hidden")
        layout.operator("%s.hide" % self._operator_name, text="Hide Selected").unselected = False
        layout.operator("%s.hide" % self._operator_name, text="Hide Unselected").unselected = True


# Standard transforms which apply to all cases
# NOTE: this doesn't seem to be able to be used directly
class VIEW3D_MT_transform_base(Menu):
    bl_label = "Transform"

    # TODO: get rid of the custom text strings?
    def draw(self, context):
        layout = self.layout

        layout.operator("transform.tosphere", text="To Sphere")
        layout.operator("transform.shear", text="Shear")
        layout.operator("transform.bend", text="Bend")
        layout.operator("transform.push_pull", text="Push/Pull")

        if context.mode != 'OBJECT':
            layout.operator("transform.vertex_warp", text="Warp")
            layout.operator("transform.vertex_random", text="Randomize")


# Generic transform menu - geometry types
class VIEW3D_MT_transform(VIEW3D_MT_transform_base):
    def draw(self, context):
        # base menu
        VIEW3D_MT_transform_base.draw(self, context)

        # generic...
        layout = self.layout
        layout.operator("transform.shrink_fatten", text="Shrink Fatten")

        layout.separator()

        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True


# Object-specific extensions to Transform menu
class VIEW3D_MT_transform_object(VIEW3D_MT_transform_base):
    def draw(self, context):
        layout = self.layout

        # base menu
        VIEW3D_MT_transform_base.draw(self, context)

        # object-specific option follow...
        layout.separator()

        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'
        # XXX see alignmenu() in edit.c of b2.4x to get this working
        layout.operator("transform.transform", text="Align to Transform Orientation").mode = 'ALIGN'

        layout.separator()

        layout.operator_context = 'EXEC_AREA'

        layout.operator("object.origin_set", text="Geometry to Origin").type = 'GEOMETRY_ORIGIN'
        layout.operator("object.origin_set", text="Origin to Geometry").type = 'ORIGIN_GEOMETRY'
        layout.operator("object.origin_set", text="Origin to 3D Cursor").type = 'ORIGIN_CURSOR'
        layout.operator("object.origin_set", text="Origin to Center of Mass (Surface)").type = 'ORIGIN_CENTER_OF_MASS'
        layout.operator("object.origin_set", text="Origin to Center of Mass (Volume)").type = 'ORIGIN_CENTER_OF_VOLUME'
        layout.separator()

        layout.operator("object.randomize_transform")
        layout.operator("object.align")

        # TODO: there is a strange context bug here.
        """
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("object.transform_axis_target")
        """


# Armature EditMode extensions to Transform menu
class VIEW3D_MT_transform_armature(VIEW3D_MT_transform_base):
    def draw(self, context):
        layout = self.layout

        # base menu
        VIEW3D_MT_transform_base.draw(self, context)

        # armature specific extensions follow...
        obj = context.object
        if obj.type == 'ARMATURE' and obj.mode in {'EDIT', 'POSE'}:
            if obj.data.draw_type == 'BBONE':
                layout.separator()

                layout.operator("transform.transform", text="Scale BBone").mode = 'BONE_SIZE'
            elif obj.data.draw_type == 'ENVELOPE':
                layout.separator()

                layout.operator("transform.transform", text="Scale Envelope Distance").mode = 'BONE_SIZE'
                layout.operator("transform.transform", text="Scale Radius").mode = 'BONE_ENVELOPE'

        if context.edit_object and context.edit_object.type == 'ARMATURE':
            layout.separator()

            layout.operator("armature.align")


class VIEW3D_MT_mirror(Menu):
    bl_label = "Mirror"

    def draw(self, context):
        layout = self.layout

        layout.operator("transform.mirror", text="Interactive Mirror")

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'

        props = layout.operator("transform.mirror", text="X Global")
        props.constraint_axis = (True, False, False)
        props.constraint_orientation = 'GLOBAL'
        props = layout.operator("transform.mirror", text="Y Global")
        props.constraint_axis = (False, True, False)
        props.constraint_orientation = 'GLOBAL'
        props = layout.operator("transform.mirror", text="Z Global")
        props.constraint_axis = (False, False, True)
        props.constraint_orientation = 'GLOBAL'

        if context.edit_object:
            layout.separator()

            props = layout.operator("transform.mirror", text="X Local")
            props.constraint_axis = (True, False, False)
            props.constraint_orientation = 'LOCAL'
            props = layout.operator("transform.mirror", text="Y Local")
            props.constraint_axis = (False, True, False)
            props.constraint_orientation = 'LOCAL'
            props = layout.operator("transform.mirror", text="Z Local")
            props.constraint_axis = (False, False, True)
            props.constraint_orientation = 'LOCAL'

            layout.operator("object.vertex_group_mirror")


class VIEW3D_MT_snap(Menu):
    bl_label = "Snap"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.snap_selected_to_grid", text="Selection to Grid")
        layout.operator("view3d.snap_selected_to_cursor", text="Selection to Cursor").use_offset = False
        layout.operator("view3d.snap_selected_to_cursor", text="Selection to Cursor (Keep Offset)").use_offset = True
        layout.operator("view3d.snap_selected_to_active", text="Selection to Active")

        layout.separator()

        layout.operator("view3d.snap_cursor_to_selected", text="Cursor to Selected")
        layout.operator("view3d.snap_cursor_to_center", text="Cursor to Center")
        layout.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid")
        layout.operator("view3d.snap_cursor_to_active", text="Cursor to Active")


class VIEW3D_MT_uv_map(Menu):
    bl_label = "UV Mapping"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.unwrap")

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("uv.smart_project")
        layout.operator("uv.lightmap_pack")
        layout.operator("uv.follow_active_quads")

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("uv.cube_project")
        layout.operator("uv.cylinder_project")
        layout.operator("uv.sphere_project")

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("uv.project_from_view").scale_to_bounds = False
        layout.operator("uv.project_from_view", text="Project from View (Bounds)").scale_to_bounds = True

        layout.separator()

        layout.operator("uv.reset")

        layout.separator()

        layout.operator("mesh.uvs_rotate")
        layout.operator("mesh.uvs_reverse")


class VIEW3D_MT_edit_proportional(Menu):
    bl_label = "Proportional Editing"

    def draw(self, context):
        layout = self.layout

        layout.props_enum(tool_settings, "proportional_edit")

        layout.separator()

        layout.label("Falloff:")
        layout.props_enum(tool_settings, "proportional_edit_falloff")


# ********** View menus **********


class VIEW3D_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        view = context.space_data

        layout.operator("view3d.toolshelf", icon='MENU_PANEL')
        layout.operator("view3d.properties", icon='MENU_PANEL')

        layout.separator()

        layout.operator("view3d.view_selected", text="Frame Selected").use_all_regions = False
        if view.region_quadviews:
            layout.operator("view3d.view_selected", text="Frame Selected (Quad View)").use_all_regions = True

        layout.operator("view3d.view_all", text="Frame All").center = False
        layout.operator("view3d.view_persportho", text="Perspective/Orthographic")

        layout.separator()

        layout.menu("VIEW3D_MT_view_cameras", text="Cameras")

        layout.separator()
        layout.menu("VIEW3D_MT_view_viewpoint")
        layout.menu("VIEW3D_MT_view_navigation")
        layout.menu("VIEW3D_MT_view_align")

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.menu("VIEW3D_MT_view_borders", text="View Borders")

        layout.separator()

        layout.operator("screen.animation_play", text="Play Animation")

        layout.separator()

        layout.operator("render.opengl", icon='RENDER_STILL')
        layout.operator("render.opengl", text="OpenGL Render Animation", icon='RENDER_ANIMATION').animation = True

        layout.separator()

        layout.menu("INFO_MT_area")


class VIEW3D_MT_view_cameras(Menu):
    bl_label = "Cameras"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.object_as_camera")
        layout.operator("view3d.view_camera", text="Active Camera")


class VIEW3D_MT_view_viewpoint(Menu):
    bl_label = "Viewpoint"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.view_camera", text="Camera")

        layout.separator()

        layout.operator("view3d.view_axis", text="Top").type = 'TOP'
        layout.operator("view3d.view_axis", text="Bottom").type = 'BOTTOM'

        layout.separator()

        layout.operator("view3d.view_axis", text="Front").type = 'FRONT'
        layout.operator("view3d.view_axis", text="Back").type = 'BACK'

        layout.separator()

        layout.operator("view3d.view_axis", text="Right").type = 'RIGHT'
        layout.operator("view3d.view_axis", text="Left").type = 'LEFT'


class VIEW3D_MT_view_navigation(Menu):
    bl_label = "Navigation"

    def draw(self, context):
        from math import pi
        layout = self.layout

        layout.operator_enum("view3d.view_orbit", "type")
        props = layout.operator("view3d.view_orbit", "Orbit Opposite")
        props.type = 'ORBITRIGHT'
        props.angle = pi

        layout.separator()

        layout.operator("view3d.view_roll", text="Roll Left").type = 'LEFT'
        layout.operator("view3d.view_roll", text="Roll Right").type = 'RIGHT'

        layout.separator()

        layout.operator_enum("view3d.view_pan", "type")

        layout.separator()

        layout.operator("view3d.zoom", text="Zoom In").delta = 1
        layout.operator("view3d.zoom", text="Zoom Out").delta = -1
        layout.operator("view3d.zoom_border", text="Zoom Border...")
        layout.operator("view3d.zoom_camera_1_to_1", text="Zoom Camera 1:1")

        layout.separator()

        layout.operator("view3d.fly")
        layout.operator("view3d.walk")


class VIEW3D_MT_view_align(Menu):
    bl_label = "Align View"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_view_align_selected")

        layout.separator()

        layout.operator("view3d.camera_to_view", text="Align Active Camera to View")
        layout.operator("view3d.camera_to_view_selected", text="Align Active Camera to Selected")

        layout.separator()

        layout.operator("view3d.view_all", text="Center Cursor and View All").center = True
        layout.operator("view3d.view_center_cursor")

        layout.separator()

        layout.operator("view3d.view_lock_to_active")
        layout.operator("view3d.view_lock_clear")


class VIEW3D_MT_view_align_selected(Menu):
    bl_label = "Align View to Active"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("view3d.view_axis", text="Top")
        props.align_active = True
        props.type = 'TOP'

        props = layout.operator("view3d.view_axis", text="Bottom")
        props.align_active = True
        props.type = 'BOTTOM'

        layout.separator()

        props = layout.operator("view3d.view_axis", text="Front")
        props.align_active = True
        props.type = 'FRONT'

        props = layout.operator("view3d.view_axis", text="Back")
        props.align_active = True
        props.type = 'BACK'

        layout.separator()

        props = layout.operator("view3d.view_axis", text="Right")
        props.align_active = True
        props.type = 'RIGHT'

        props = layout.operator("view3d.view_axis", text="Left")
        props.align_active = True
        props.type = 'LEFT'


class VIEW3D_MT_view_borders(Menu):
    bl_label = "View Borders"

    def draw(self, context):
        layout = self.layout
        layout.operator("view3d.clip_border", text="Clipping Border...")
        layout.operator("view3d.render_border", text="Render Border...")

        layout.separator()

        layout.operator("view3d.clear_render_border")


# ********** Select menus, suffix from context.mode **********

class VIEW3D_MT_select_object_more_less(Menu):
    bl_label = "Select More/Less"

    def draw(self, context):
        layout = self.layout

        layout = self.layout

        layout.operator("object.select_more", text="More")
        layout.operator("object.select_less", text="Less")

        layout.separator()

        props = layout.operator("object.select_hierarchy", text="Parent")
        props.extend = False
        props.direction = 'PARENT'

        props = layout.operator("object.select_hierarchy", text="Child")
        props.extend = False
        props.direction = 'CHILD'

        layout.separator()

        props = layout.operator("object.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("object.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'


class VIEW3D_MT_select_object(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.select_all", text="All").action = 'SELECT'
        layout.operator("object.select_all", text="None").action = 'DESELECT'
        layout.operator("object.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator_menu_enum("object.select_by_type", "type", text="Select All by Type...")
        layout.operator("object.select_camera", text="Select Active Camera")
        layout.operator("object.select_mirror", text="Mirror Selection")
        layout.operator("object.select_random", text="Select Random")

        layout.separator()

        layout.menu("VIEW3D_MT_select_object_more_less")

        layout.separator()

        layout.operator_menu_enum("object.select_grouped", "type", text="Select Grouped")
        layout.operator_menu_enum("object.select_linked", "type", text="Select Linked")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_pose_more_less(Menu):
    bl_label = "Select More/Less"

    def draw(self, context):
        layout = self.layout

        layout = self.layout

        props = layout.operator("pose.select_hierarchy", text="Parent")
        props.extend = False
        props.direction = 'PARENT'

        props = layout.operator("pose.select_hierarchy", text="Child")
        props.extend = False
        props.direction = 'CHILD'

        layout.separator()

        props = layout.operator("pose.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("pose.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'


class VIEW3D_MT_select_pose(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.select_all", text="All").action = 'SELECT'
        layout.operator("pose.select_all", text="None").action = 'DESELECT'
        layout.operator("pose.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("pose.select_mirror", text="Flip Active")

        layout.separator()

        layout.operator("pose.select_constraint_target", text="Constraint Target")
        layout.operator("pose.select_linked", text="Linked")

        layout.separator()

        layout.menu("VIEW3D_MT_select_pose_more_less")

        layout.separator()

        layout.operator_menu_enum("pose.select_grouped", "type", text="Grouped")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_particle(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("particle.select_all", text="All").action = 'SELECT'
        layout.operator("particle.select_all", text="None").action = 'DESELECT'
        layout.operator("particle.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("particle.select_linked")

        layout.separator()

        layout.operator("particle.select_more")
        layout.operator("particle.select_less")

        layout.separator()

        layout.operator("particle.select_random")

        layout.separator()

        layout.operator("particle.select_roots", text="Roots")
        layout.operator("particle.select_tips", text="Tips")


class VIEW3D_MT_edit_mesh_select_similar(Menu):
    bl_label = "Select Similar"

    def draw(self, context):
        layout = self.layout

        layout.operator_enum("mesh.select_similar", "type")

        layout.separator()

        layout.operator("mesh.select_similar_region", text="Face Regions")


class VIEW3D_MT_edit_mesh_select_by_trait(Menu):
    bl_label = "Select All by Trait"

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        if tool_settings.mesh_select_mode[2] is False:
            layout.operator("mesh.select_non_manifold", text="Non Manifold")
        layout.operator("mesh.select_loose", text="Loose Geometry")
        layout.operator("mesh.select_interior_faces", text="Interior Faces")
        layout.operator("mesh.select_face_by_sides", text="Faces by Sides")

        layout.separator()

        layout.operator("mesh.select_ungrouped", text="Ungrouped Verts")


class VIEW3D_MT_edit_mesh_select_more_less(Menu):
    bl_label = "Select More/Less"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.select_more", text="More")
        layout.operator("mesh.select_less", text="Less")

        layout.separator()

        layout.operator("mesh.select_next_item", text="Next Active")
        layout.operator("mesh.select_prev_item", text="Previous Active")


class VIEW3D_MT_edit_mesh_select_linked(Menu):
    bl_label = "Select Linked"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.select_linked", text="Linked")
        layout.operator("mesh.shortest_path_select", text="Shortest Path")
        layout.operator("mesh.faces_select_linked_flat", text="Linked Flat Faces")


class VIEW3D_MT_edit_mesh_select_loops(Menu):
    bl_label = "Select Loops"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.loop_multi_select", text="Edge Loops").ring = False
        layout.operator("mesh.loop_multi_select", text="Edge Rings").ring = True

        layout.separator()

        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_select_edit_mesh(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        # primitive
        layout.operator("mesh.select_all", text="All").action = 'SELECT'
        layout.operator("mesh.select_all", text="None").action = 'DESELECT'
        layout.operator("mesh.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        # numeric
        layout.operator("mesh.select_random", text="Select Random")
        layout.operator("mesh.select_nth")

        layout.separator()

        # geometric
        layout.operator("mesh.edges_select_sharp", text="Select Sharp Edges")

        layout.separator()

        # other ...
        layout.menu("VIEW3D_MT_edit_mesh_select_similar")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_select_by_trait")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_select_more_less")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_select_loops")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_select_linked")

        layout.separator()

        layout.operator("mesh.select_axis", text="Side of Active")
        layout.operator("mesh.select_mirror", text="Mirror Selection")


class VIEW3D_MT_select_edit_curve(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.select_all", text="All").action = 'SELECT'
        layout.operator("curve.select_all", text="None").action = 'DESELECT'
        layout.operator("curve.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_random")
        layout.operator("curve.select_nth")
        layout.operator("curve.select_linked", text="Select Linked")
        layout.operator("curve.select_similar", text="Select Similar")

        layout.separator()

        layout.operator("curve.de_select_first")
        layout.operator("curve.de_select_last")
        layout.operator("curve.select_next")
        layout.operator("curve.select_previous")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_select_edit_surface(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.select_all", text="All").action = 'SELECT'
        layout.operator("curve.select_all", text="None").action = 'DESELECT'
        layout.operator("curve.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_random")
        layout.operator("curve.select_nth")
        layout.operator("curve.select_linked", text="Select Linked")
        layout.operator("curve.select_similar", text="Select Similar")

        layout.separator()

        layout.operator("curve.select_row")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_select_edit_text(Menu):
    # intentional name mis-match
    # select menu for 3d-text doesn't make sense
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        layout.operator("font.text_paste", text="Paste")
        layout.operator("font.text_cut", text="Cut")
        layout.operator("font.text_copy", text="Copy")

        layout.separator()

        layout.operator("font.text_paste_from_file")

        layout.separator()

        layout.operator("font.select_all")


class VIEW3D_MT_select_edit_metaball(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("mball.select_all", text="All").action = 'SELECT'
        layout.operator("mball.select_all", text="None").action = 'DESELECT'
        layout.operator("mball.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("mball.select_random_metaelems")

        layout.separator()

        layout.operator_menu_enum("mball.select_similar", "type", text="Similar")


class VIEW3D_MT_select_edit_lattice(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("lattice.select_all", text="All").action = 'SELECT'
        layout.operator("lattice.select_all", text="None").action = 'DESELECT'
        layout.operator("lattice.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("lattice.select_mirror")
        layout.operator("lattice.select_random")

        layout.separator()

        layout.operator("lattice.select_ungrouped", text="Ungrouped Verts")


class VIEW3D_MT_select_edit_armature(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("armature.select_all", text="All").action = 'SELECT'
        layout.operator("armature.select_all", text="None").action = 'DESELECT'
        layout.operator("armature.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("armature.select_mirror", text="Mirror").extend = False

        layout.separator()

        layout.operator("armature.select_more", text="More")
        layout.operator("armature.select_less", text="Less")

        layout.separator()

        props = layout.operator("armature.select_hierarchy", text="Parent")
        props.extend = False
        props.direction = 'PARENT'

        props = layout.operator("armature.select_hierarchy", text="Child")
        props.extend = False
        props.direction = 'CHILD'

        layout.separator()

        props = layout.operator("armature.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("armature.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'

        layout.operator_menu_enum("armature.select_similar", "type", text="Similar")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_gpencil(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("gpencil.select_all", text="All").action = 'SELECT'
        layout.operator("gpencil.select_all", text="None").action = 'DESELECT'
        layout.operator("gpencil.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("gpencil.select_border")
        layout.operator("gpencil.select_circle")

        layout.separator()

        layout.operator("gpencil.select_linked", text="Linked")
        layout.operator("gpencil.select_alternate")
        layout.operator_menu_enum("gpencil.select_grouped", "type", text="Grouped")

        layout.separator()

        layout.operator("gpencil.select_first")
        layout.operator("gpencil.select_last")

        layout.separator()

        layout.operator("gpencil.select_more")
        layout.operator("gpencil.select_less")


class VIEW3D_MT_select_paint_mask(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("paint.face_select_all", text="All").action = 'SELECT'
        layout.operator("paint.face_select_all", text="None").action = 'DESELECT'
        layout.operator("paint.face_select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("paint.face_select_linked", text="Linked")


class VIEW3D_MT_select_paint_mask_vertex(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("paint.vert_select_all", text="All").action = 'SELECT'
        layout.operator("paint.vert_select_all", text="None").action = 'DESELECT'
        layout.operator("paint.vert_select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("paint.vert_select_ungrouped", text="Ungrouped Verts")


class VIEW3D_MT_angle_control(Menu):
    bl_label = "Angle Control"

    @classmethod
    def poll(cls, context):
        settings = UnifiedPaintPanel.paint_settings(context)
        if not settings:
            return False

        brush = settings.brush
        tex_slot = brush.texture_slot

        return tex_slot.has_texture_angle and tex_slot.has_texture_angle_source

    def draw(self, context):
        layout = self.layout

        settings = UnifiedPaintPanel.paint_settings(context)
        brush = settings.brush

        sculpt = (context.sculpt_object is not None)

        tex_slot = brush.texture_slot

        layout.prop(tex_slot, "use_rake", text="Rake")

        if brush.brush_capabilities.has_random_texture_angle and tex_slot.has_random_texture_angle:
            if sculpt:
                if brush.sculpt_capabilities.has_random_texture_angle:
                    layout.prop(tex_slot, "use_random", text="Random")
            else:
                layout.prop(tex_slot, "use_random", text="Random")


# XXX: INFO_MT_ names used to keep backwards compatibility (Add-ons etc. that hook into the menu)
class INFO_MT_mesh_add(Menu):
    bl_idname = "INFO_MT_mesh_add"
    bl_label = "Mesh"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.primitive_plane_add", text="Plane", icon='MESH_PLANE')
        layout.operator("mesh.primitive_cube_add", text="Cube", icon='MESH_CUBE')
        layout.operator("mesh.primitive_circle_add", text="Circle", icon='MESH_CIRCLE')
        layout.operator("mesh.primitive_uv_sphere_add", text="UV Sphere", icon='MESH_UVSPHERE')
        layout.operator("mesh.primitive_ico_sphere_add", text="Ico Sphere", icon='MESH_ICOSPHERE')
        layout.operator("mesh.primitive_cylinder_add", text="Cylinder", icon='MESH_CYLINDER')
        layout.operator("mesh.primitive_cone_add", text="Cone", icon='MESH_CONE')
        layout.operator("mesh.primitive_torus_add", text="Torus", icon='MESH_TORUS')

        layout.separator()

        layout.operator("mesh.primitive_grid_add", text="Grid", icon='MESH_GRID')
        layout.operator("mesh.primitive_monkey_add", text="Monkey", icon='MESH_MONKEY')


class INFO_MT_curve_add(Menu):
    bl_idname = "INFO_MT_curve_add"
    bl_label = "Curve"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("curve.primitive_bezier_curve_add", text="Bezier", icon='CURVE_BEZCURVE')
        layout.operator("curve.primitive_bezier_circle_add", text="Circle", icon='CURVE_BEZCIRCLE')

        layout.separator()

        layout.operator("curve.primitive_nurbs_curve_add", text="Nurbs Curve", icon='CURVE_NCURVE')
        layout.operator("curve.primitive_nurbs_circle_add", text="Nurbs Circle", icon='CURVE_NCIRCLE')
        layout.operator("curve.primitive_nurbs_path_add", text="Path", icon='CURVE_PATH')

        layout.separator()

        layout.operator("curve.draw", icon='LINE_DATA')


class INFO_MT_surface_add(Menu):
    bl_idname = "INFO_MT_surface_add"
    bl_label = "Surface"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("surface.primitive_nurbs_surface_curve_add", text="Nurbs Curve", icon='SURFACE_NCURVE')
        layout.operator("surface.primitive_nurbs_surface_circle_add", text="Nurbs Circle", icon='SURFACE_NCIRCLE')
        layout.operator("surface.primitive_nurbs_surface_surface_add", text="Nurbs Surface", icon='SURFACE_NSURFACE')
        layout.operator("surface.primitive_nurbs_surface_cylinder_add",
                        text="Nurbs Cylinder", icon='SURFACE_NCYLINDER')
        layout.operator("surface.primitive_nurbs_surface_sphere_add", text="Nurbs Sphere", icon='SURFACE_NSPHERE')
        layout.operator("surface.primitive_nurbs_surface_torus_add", text="Nurbs Torus", icon='SURFACE_NTORUS')


class INFO_MT_metaball_add(Menu):
    bl_idname = "INFO_MT_metaball_add"
    bl_label = "Metaball"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator_enum("object.metaball_add", "type")


class INFO_MT_edit_curve_add(Menu):
    bl_idname = "INFO_MT_edit_curve_add"
    bl_label = "Add"

    def draw(self, context):
        is_surf = context.active_object.type == 'SURFACE'

        layout = self.layout
        layout.operator_context = 'EXEC_REGION_WIN'

        if is_surf:
            INFO_MT_surface_add.draw(self, context)
        else:
            INFO_MT_curve_add.draw(self, context)


class INFO_MT_edit_armature_add(Menu):
    bl_idname = "INFO_MT_edit_armature_add"
    bl_label = "Armature"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("armature.bone_primitive_add", text="Single Bone", icon='BONE_DATA')


class INFO_MT_armature_add(Menu):
    bl_idname = "INFO_MT_armature_add"
    bl_label = "Armature"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("object.armature_add", text="Single Bone", icon='BONE_DATA')


class INFO_MT_light_add(Menu):
    bl_idname = "INFO_MT_light_add"
    bl_label = "Light"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator_enum("object.light_add", "type")


class INFO_MT_lightprobe_add(Menu):
    bl_idname = "INFO_MT_lightprobe_add"
    bl_label = "Light Probe"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator_enum("object.lightprobe_add", "type")


class INFO_MT_camera_add(Menu):
    bl_idname = "INFO_MT_camera_add"
    bl_label = "Camera"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("object.camera_add", text="Camera", icon='OUTLINER_OB_CAMERA')


class INFO_MT_add(Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        # note, don't use 'EXEC_SCREEN' or operators wont get the 'v3d' context.

        # Note: was EXEC_AREA, but this context does not have the 'rv3d', which prevents
        #       "align_view" to work on first call (see [#32719]).
        layout.operator_context = 'EXEC_REGION_WIN'

        # layout.operator_menu_enum("object.mesh_add", "type", text="Mesh", icon='OUTLINER_OB_MESH')
        layout.menu("INFO_MT_mesh_add", icon='OUTLINER_OB_MESH')

        # layout.operator_menu_enum("object.curve_add", "type", text="Curve", icon='OUTLINER_OB_CURVE')
        layout.menu("INFO_MT_curve_add", icon='OUTLINER_OB_CURVE')
        # layout.operator_menu_enum("object.surface_add", "type", text="Surface", icon='OUTLINER_OB_SURFACE')
        layout.menu("INFO_MT_surface_add", icon='OUTLINER_OB_SURFACE')
        layout.menu("INFO_MT_metaball_add", text="Metaball", icon='OUTLINER_OB_META')
        layout.operator("object.text_add", text="Text", icon='OUTLINER_OB_FONT')
        layout.operator_menu_enum("object.gpencil_add", "type", text="Grease Pencil", icon='OUTLINER_OB_GREASEPENCIL')
        layout.separator()

        layout.menu("INFO_MT_armature_add", icon='OUTLINER_OB_ARMATURE')
        layout.operator("object.add", text="Lattice", icon='OUTLINER_OB_LATTICE').type = 'LATTICE'
        layout.operator_menu_enum("object.empty_add", "type", text="Empty", icon='OUTLINER_OB_EMPTY')
        layout.separator()

        layout.operator("object.speaker_add", text="Speaker", icon='OUTLINER_OB_SPEAKER')
        layout.separator()

        if INFO_MT_camera_add.is_extended():
            layout.menu("INFO_MT_camera_add", icon='OUTLINER_OB_CAMERA')
        else:
            INFO_MT_camera_add.draw(self, context)

        layout.menu("INFO_MT_light_add", icon='OUTLINER_OB_LIGHT')
        layout.separator()
        layout.menu("INFO_MT_lightprobe_add", icon='OUTLINER_OB_LIGHTPROBE')
        layout.separator()

        layout.operator_menu_enum("object.effector_add", "type", text="Force Field", icon='OUTLINER_OB_FORCE_FIELD')
        layout.separator()

        has_collections = bool(bpy.data.collections)
        col = layout.column()
        col.enabled = has_collections

        if not has_collections or len(bpy.data.collections) > 10:
            col.operator_context = 'INVOKE_REGION_WIN'
            col.operator(
                "object.collection_instance_add",
                text="Collection Instance..." if has_collections else "No Collections to Instance",
                icon='OUTLINER_OB_GROUP_INSTANCE',
            )
        else:
            col.operator_menu_enum(
                "object.collection_instance_add",
                "collection",
                text="Collection Instance",
                icon='OUTLINER_OB_GROUP_INSTANCE',
            )


class VIEW3D_MT_object_relations(Menu):
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.proxy_make", text="Make Proxy...")

        layout.operator("object.make_dupli_face")

        layout.separator()

        layout.operator_menu_enum("object.make_local", "type", text="Make Local...")
        layout.menu("VIEW3D_MT_make_single_user")

        layout.separator()

        layout.operator("object.data_transfer")
        layout.operator("object.datalayout_transfer")


class VIEW3D_MT_object(Menu):
    bl_context = "objectmode"
    bl_label = "Object"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_transform_object")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_object_clear")
        layout.menu("VIEW3D_MT_object_apply")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("object.duplicate_move")
        layout.operator("object.duplicate_move_linked")
        layout.operator("object.join")

        layout.separator()

        layout.operator("view3d.copybuffer", text="Copy Objects")
        layout.operator("view3d.pastebuffer", text="Paste Objects")

        layout.separator()

        layout.menu("VIEW3D_MT_object_parent")
        layout.menu("VIEW3D_MT_object_collection")
        layout.menu("VIEW3D_MT_object_relations")
        layout.menu("VIEW3D_MT_object_constraints")
        layout.menu("VIEW3D_MT_object_track")
        layout.menu("VIEW3D_MT_make_links", text="Make Links...")

        layout.separator()

        layout.operator("object.shade_smooth")
        layout.operator("object.shade_flat")

        layout.separator()

        layout.menu("VIEW3D_MT_object_animation")
        layout.menu("VIEW3D_MT_object_rigid_body")

        layout.separator()

        layout.menu("VIEW3D_MT_object_quick_effects")

        layout.separator()

        layout.operator_menu_enum("object.convert", "target")

        layout.separator()

        layout.menu("VIEW3D_MT_object_showhide")

        layout.separator()

        layout.operator("object.delete", text="Delete...").use_global = False


class VIEW3D_MT_object_animation(Menu):
    bl_label = "Animation"

    def draw(self, context):
        layout = self.layout

        layout.operator("anim.keyframe_insert_menu", text="Insert Keyframe...")
        layout.operator("anim.keyframe_delete_v3d", text="Delete Keyframes...")
        layout.operator("anim.keyframe_clear_v3d", text="Clear Keyframes...")
        layout.operator("anim.keying_set_active_set", text="Change Keying Set...")

        layout.separator()

        layout.operator("nla.bake", text="Bake Action...")


class VIEW3D_MT_object_rigid_body(Menu):
    bl_label = "Rigid Body"

    def draw(self, context):
        layout = self.layout

        layout.operator("rigidbody.objects_add", text="Add Active").type = 'ACTIVE'
        layout.operator("rigidbody.objects_add", text="Add Passive").type = 'PASSIVE'

        layout.separator()

        layout.operator("rigidbody.objects_remove", text="Remove")

        layout.separator()

        layout.operator("rigidbody.shape_change", text="Change Shape")
        layout.operator("rigidbody.mass_calculate", text="Calculate Mass")
        layout.operator("rigidbody.object_settings_copy", text="Copy from Active")
        layout.operator("object.visual_transform_apply", text="Apply Transformation")
        layout.operator("rigidbody.bake_to_keyframes", text="Bake To Keyframes")

        layout.separator()

        layout.operator("rigidbody.connect", text="Connect")


class VIEW3D_MT_object_clear(Menu):
    bl_label = "Clear"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.location_clear", text="Location").clear_delta = False
        layout.operator("object.rotation_clear", text="Rotation").clear_delta = False
        layout.operator("object.scale_clear", text="Scale").clear_delta = False

        layout.separator()

        layout.operator("object.origin_clear", text="Origin")


class VIEW3D_MT_object_specials(Menu):
    bl_label = "Object Context Menu"

    @classmethod
    def poll(cls, context):
        # add more special types
        return context.object

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        obj = context.object

        layout.operator("view3d.copybuffer", text="Copy Objects", icon='COPYDOWN')
        layout.operator("view3d.pastebuffer", text="Paste Objects", icon='PASTEDOWN')

        layout.separator()

        layout.operator("object.duplicate_move")
        layout.operator("object.duplicate_move_linked")

        layout.separator()

        layout.menu("VIEW3D_MT_snap")
        layout.menu("VIEW3D_MT_object_parent")
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("object.move_to_collection")

        layout.separator()

        layout.operator("anim.keyframe_insert_menu", text="Insert Keyframe...")

        layout.separator()

        layout.operator("object.delete", text="Delete...").use_global = False

        if obj.type == 'MESH':

            layout.separator()

            layout.operator("object.shade_smooth", text="Smooth Shading")
            layout.operator("object.shade_flat", text="Flat Shading")

            layout.separator()

            layout.operator("object.origin_set")
            layout.operator("object.join")
            layout.operator_menu_enum("object.convert", "target")

        if obj.type == 'CAMERA':
            layout.operator_context = 'INVOKE_REGION_WIN'

            if obj.data.type == 'PERSP':
                props = layout.operator("wm.context_modal_mouse", text="Camera Lens Angle")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.lens"
                props.input_scale = 0.1
                if obj.data.lens_unit == 'MILLIMETERS':
                    props.header_text = "Camera Lens Angle: %.1fmm"
                else:
                    props.header_text = "Camera Lens Angle: %.1f\u00B0"

            else:
                props = layout.operator("wm.context_modal_mouse", text="Camera Lens Scale")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.ortho_scale"
                props.input_scale = 0.01
                props.header_text = "Camera Lens Scale: %.3f"

            if not obj.data.dof_object:
                view = context.space_data
                if view and view.camera == obj and view.region_3d.view_perspective == 'CAMERA':
                    props = layout.operator("ui.eyedropper_depth", text="DOF Distance (Pick)")
                else:
                    props = layout.operator("wm.context_modal_mouse", text="DOF Distance")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.dof_distance"
                    props.input_scale = 0.02
                    props.header_text = "DOF Distance: %.3f"
                del view

        if obj.type in {'CURVE', 'FONT'}:
            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Extrude Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "data.extrude"
            props.input_scale = 0.01
            props.header_text = "Extrude Size: %.3f"

            props = layout.operator("wm.context_modal_mouse", text="Width Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "data.offset"
            props.input_scale = 0.01
            props.header_text = "Width Size: %.3f"

        if obj.type == 'EMPTY':
            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Empty Draw Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "empty_draw_size"
            props.input_scale = 0.01
            props.header_text = "Empty Draw Size: %.3f"

        if obj.type == 'LIGHT':
            light = obj.data

            layout.operator_context = 'INVOKE_REGION_WIN'

            emission_node = None
            if light.node_tree:
                for node in light.node_tree.nodes:
                    if getattr(node, "type", None) == 'EMISSION':
                        emission_node = node
                        break

            if emission_node is not None:
                props = layout.operator("wm.context_modal_mouse", text="Strength")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.node_tree" \
                                       ".nodes[\"" + emission_node.name + "\"]" \
                                       ".inputs[\"Strength\"].default_value"
                props.header_text = "Light Strength: %.3f"
                props.input_scale = 0.1

            if light.type == 'AREA':
                props = layout.operator("wm.context_modal_mouse", text="Size X")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.size"
                props.header_text = "Light Size X: %.3f"

                if light.shape in {'RECTANGLE', 'ELLIPSE'}:
                    props = layout.operator("wm.context_modal_mouse", text="Size Y")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.size_y"
                    props.header_text = "Light Size Y: %.3f"

            elif light.type in {'SPOT', 'POINT', 'SUN'}:
                props = layout.operator("wm.context_modal_mouse", text="Size")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.shadow_soft_size"
                props.header_text = "Light Size: %.3f"

            if light.type == 'SPOT':
                layout.separator()
                props = layout.operator("wm.context_modal_mouse", text="Spot Size")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.spot_size"
                props.input_scale = 0.01
                props.header_text = "Spot Size: %.2f"

                props = layout.operator("wm.context_modal_mouse", text="Spot Blend")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.spot_blend"
                props.input_scale = -0.01
                props.header_text = "Spot Blend: %.2f"


class VIEW3D_MT_object_shading(Menu):
    # XXX, this menu is a place to store shading operator in object mode
    bl_label = "Shading"

    def draw(self, context):
        layout = self.layout
        layout.operator("object.shade_smooth", text="Smooth")
        layout.operator("object.shade_flat", text="Flat")


class VIEW3D_MT_object_apply(Menu):
    bl_label = "Apply"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("object.transform_apply", text="Location", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = True, False, False

        props = layout.operator("object.transform_apply", text="Rotation", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, True, False

        props = layout.operator("object.transform_apply", text="Scale", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, False, True
        props = layout.operator("object.transform_apply", text="Rotation & Scale", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, True, True

        layout.separator()

        layout.operator(
            "object.transforms_to_deltas",
            text="Location to Deltas",
            text_ctxt=i18n_contexts.default,
        ).mode = 'LOC'
        layout.operator(
            "object.transforms_to_deltas",
            text="Rotation to Deltas",
            text_ctxt=i18n_contexts.default,
        ).mode = 'ROT'
        layout.operator(
            "object.transforms_to_deltas",
            text="Scale to Deltas",
            text_ctxt=i18n_contexts.default,
        ).mode = 'SCALE'

        layout.operator(
            "object.transforms_to_deltas",
            text="All Transforms to Deltas",
            text_ctxt=i18n_contexts.default,
        ).mode = 'ALL'
        layout.operator("object.anim_transforms_to_deltas")

        layout.separator()

        layout.operator(
            "object.visual_transform_apply",
            text="Visual Transform",
            text_ctxt=i18n_contexts.default,
        )
        layout.operator(
            "object.convert",
            text="Visual Geometry to Mesh",
            text_ctxt=i18n_contexts.default,
        ).target = 'MESH'
        layout.operator("object.duplicates_make_real")


class VIEW3D_MT_object_parent(Menu):
    bl_label = "Parent"

    def draw(self, context):
        layout = self.layout

        layout.operator_enum("object.parent_set", "type")

        layout.separator()

        layout.operator_enum("object.parent_clear", "type")


class VIEW3D_MT_object_track(Menu):
    bl_label = "Track"

    def draw(self, context):
        layout = self.layout

        layout.operator_enum("object.track_set", "type")

        layout.separator()

        layout.operator_enum("object.track_clear", "type")


class VIEW3D_MT_object_collection(Menu):
    bl_label = "Collection"

    def draw(self, context):
        layout = self.layout

        layout.operator("collection.create")
        # layout.operator_menu_enum("collection.objects_remove", "collection")  # BUGGY
        layout.operator("collection.objects_remove")
        layout.operator("collection.objects_remove_all")

        layout.separator()

        layout.operator("collection.objects_add_active")
        layout.operator("collection.objects_remove_active")


class VIEW3D_MT_object_constraints(Menu):
    bl_label = "Constraints"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.constraint_add_with_targets")
        layout.operator("object.constraints_copy")

        layout.separator()

        layout.operator("object.constraints_clear")


class VIEW3D_MT_object_quick_effects(Menu):
    bl_label = "Quick Effects"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.quick_fur")
        layout.operator("object.quick_explode")
        layout.operator("object.quick_smoke")
        layout.operator("object.quick_fluid")


class VIEW3D_MT_object_showhide(Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.hide_view_clear", text="Show Hidden")

        layout.separator()

        layout.operator("object.hide_view_set", text="Hide Selected").unselected = False
        layout.operator("object.hide_view_set", text="Hide Unselected").unselected = True


class VIEW3D_MT_make_single_user(Menu):
    bl_label = "Make Single User"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("object.make_single_user", text="Object")
        props.object = True
        props.obdata = props.material = props.animation = False

        props = layout.operator("object.make_single_user", text="Object & Data")
        props.object = props.obdata = True
        props.material = props.animation = False

        props = layout.operator("object.make_single_user", text="Object & Data & Materials")
        props.object = props.obdata = props.material = True
        props.animation = False

        props = layout.operator("object.make_single_user", text="Materials")
        props.material = True
        props.object = props.obdata = props.animation = False

        props = layout.operator("object.make_single_user", text="Object Animation")
        props.animation = True
        props.object = props.obdata = props.material = False


class VIEW3D_MT_make_links(Menu):
    bl_label = "Make Links"

    def draw(self, context):
        layout = self.layout
        operator_context_default = layout.operator_context

        if len(bpy.data.scenes) > 10:
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("object.make_links_scene", text="Objects to Scene...", icon='OUTLINER_OB_EMPTY')
        else:
            layout.operator_context = 'EXEC_REGION_WIN'
            layout.operator_menu_enum("object.make_links_scene", "scene", text="Objects to Scene")

        layout.separator()

        layout.operator_context = operator_context_default

        layout.operator_enum("object.make_links_data", "type")  # inline

        layout.operator("object.join_uvs")  # stupid place to add this!


class VIEW3D_MT_brush(Menu):
    bl_label = "Brush"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        settings = UnifiedPaintPanel.paint_settings(context)
        brush = getattr(settings, "brush", None)

        ups = tool_settings.unified_paint_settings
        layout.prop(ups, "use_unified_size", text="Unified Size")
        layout.prop(ups, "use_unified_strength", text="Unified Strength")
        if context.image_paint_object or context.vertex_paint_object:
            layout.prop(ups, "use_unified_color", text="Unified Color")
        layout.separator()

        # skip if no active brush
        if not brush:
            layout.label(text="No Brushes currently available", icon='INFO')
            return

        # brush paint modes
        layout.menu("VIEW3D_MT_brush_paint_modes")

        # brush tool
        if context.sculpt_object:
            layout.operator("brush.reset")
            layout.prop_menu_enum(brush, "sculpt_tool")
        elif context.image_paint_object:
            layout.prop_menu_enum(brush, "image_tool")
        elif context.vertex_paint_object or context.weight_paint_object:
            layout.prop_menu_enum(brush, "vertex_tool")

        # TODO: still missing a lot of brush options here

        # sculpt options
        if context.sculpt_object:

            sculpt_tool = brush.sculpt_tool

            layout.separator()
            layout.operator_menu_enum("brush.curve_preset", "shape", text="Curve Preset")
            layout.separator()

            if sculpt_tool != 'GRAB':
                layout.prop_menu_enum(brush, "stroke_method")

                if sculpt_tool in {'DRAW', 'PINCH', 'INFLATE', 'LAYER', 'CLAY'}:
                    layout.prop_menu_enum(brush, "direction")

                if sculpt_tool == 'LAYER':
                    layout.prop(brush, "use_persistent")
                    layout.operator("sculpt.set_persistent_base")


class VIEW3D_MT_brush_paint_modes(Menu):
    bl_label = "Enabled Modes"

    def draw(self, context):
        layout = self.layout

        settings = UnifiedPaintPanel.paint_settings(context)
        brush = settings.brush

        layout.prop(brush, "use_paint_sculpt", text="Sculpt")
        layout.prop(brush, "use_paint_vertex", text="Vertex Paint")
        layout.prop(brush, "use_paint_weight", text="Weight Paint")
        layout.prop(brush, "use_paint_image", text="Texture Paint")


class VIEW3D_MT_paint_vertex(Menu):
    bl_label = "Paint"

    def draw(self, context):
        layout = self.layout

        layout.operator("paint.vertex_color_set")
        layout.operator("paint.vertex_color_smooth")
        layout.operator("paint.vertex_color_dirt")
        layout.operator("paint.vertex_color_from_weight")

        layout.separator()

        layout.operator("paint.vertex_color_invert", text="Invert")
        layout.operator("paint.vertex_color_levels", text="Levels")
        layout.operator("paint.vertex_color_hsv", text="Hue Saturation Value")
        layout.operator("paint.vertex_color_brightness_contrast", text="Bright/Contrast")


class VIEW3D_MT_hook(Menu):
    bl_label = "Hooks"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.hook_add_newob")
        layout.operator("object.hook_add_selob").use_bone = False
        layout.operator("object.hook_add_selob", text="Hook to Selected Object Bone").use_bone = True

        if [mod.type == 'HOOK' for mod in context.active_object.modifiers]:
            layout.separator()

            layout.operator_menu_enum("object.hook_assign", "modifier")
            layout.operator_menu_enum("object.hook_remove", "modifier")

            layout.separator()

            layout.operator_menu_enum("object.hook_select", "modifier")
            layout.operator_menu_enum("object.hook_reset", "modifier")
            layout.operator_menu_enum("object.hook_recenter", "modifier")


class VIEW3D_MT_vertex_group(Menu):
    bl_label = "Vertex Groups"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.vertex_group_assign_new")

        ob = context.active_object
        if ob.mode == 'EDIT' or (ob.mode == 'WEIGHT_PAINT' and ob.type == 'MESH' and ob.data.use_paint_mask_vertex):
            if ob.vertex_groups.active:
                layout.separator()

                layout.operator("object.vertex_group_assign", text="Assign to Active Group")
                layout.operator(
                    "object.vertex_group_remove_from",
                    text="Remove from Active Group",
                ).use_all_groups = False
                layout.operator("object.vertex_group_remove_from", text="Remove from All").use_all_groups = True

        if ob.vertex_groups.active:
            layout.separator()

            layout.operator_menu_enum("object.vertex_group_set_active", "group", text="Set Active Group")
            layout.operator("object.vertex_group_remove", text="Remove Active Group").all = False
            layout.operator("object.vertex_group_remove", text="Remove All Groups").all = True


class VIEW3D_MT_paint_weight(Menu):
    bl_label = "Weights"

    @staticmethod
    def draw_generic(layout, is_editmode=False):

        if not is_editmode:

            layout.operator("paint.weight_from_bones", text="Assign Automatic From Bones").type = 'AUTOMATIC'
            layout.operator("paint.weight_from_bones", text="Assign From Bone Envelopes").type = 'ENVELOPES'

            layout.separator()

        layout.operator("object.vertex_group_normalize_all", text="Normalize All")
        layout.operator("object.vertex_group_normalize", text="Normalize")

        layout.separator()

        layout.operator("object.vertex_group_mirror", text="Mirror")
        layout.operator("object.vertex_group_invert", text="Invert")
        layout.operator("object.vertex_group_clean", text="Clean")

        layout.separator()

        layout.operator("object.vertex_group_quantize", text="Quantize")
        layout.operator("object.vertex_group_levels", text="Levels")
        layout.operator("object.vertex_group_smooth", text="Smooth")

        if not is_editmode:
            props = layout.operator("object.data_transfer", text="Transfer Weights")
            props.use_reverse_transfer = True
            props.data_type = 'VGROUP_WEIGHTS'

        layout.operator("object.vertex_group_limit_total", text="Limit Total")
        layout.operator("object.vertex_group_fix", text="Fix Deforms")

        if not is_editmode:
            layout.separator()

            layout.operator("paint.weight_set")

    def draw(self, context):
        self.draw_generic(self.layout, is_editmode=False)


class VIEW3D_MT_sculpt(Menu):
    bl_label = "Sculpt"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt

        layout.prop(sculpt, "use_symmetry_x")
        layout.prop(sculpt, "use_symmetry_y")
        layout.prop(sculpt, "use_symmetry_z")

        layout.separator()

        layout.prop(sculpt, "lock_x")
        layout.prop(sculpt, "lock_y")
        layout.prop(sculpt, "lock_z")

        layout.separator()

        layout.prop(sculpt, "use_threaded", text="Threaded Sculpt")
        layout.prop(sculpt, "show_low_resolution")
        layout.prop(sculpt, "show_brush")
        layout.prop(sculpt, "use_deform_only")
        layout.prop(sculpt, "show_diffuse_color")
        layout.prop(sculpt, "show_mask")


class VIEW3D_MT_hide_mask(Menu):
    bl_label = "Hide/Mask"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("paint.hide_show", text="Show All")
        props.action = 'SHOW'
        props.area = 'ALL'

        props = layout.operator("paint.hide_show", text="Hide Bounding Box")
        props.action = 'HIDE'
        props.area = 'INSIDE'

        props = layout.operator("paint.hide_show", text="Show Bounding Box")
        props.action = 'SHOW'
        props.area = 'INSIDE'

        props = layout.operator("paint.hide_show", text="Hide Masked")
        props.area = 'MASKED'
        props.action = 'HIDE'

        layout.separator()

        props = layout.operator("paint.mask_flood_fill", text="Invert Mask")
        props.mode = 'INVERT'

        props = layout.operator("paint.mask_flood_fill", text="Fill Mask")
        props.mode = 'VALUE'
        props.value = 1

        props = layout.operator("paint.mask_flood_fill", text="Clear Mask")
        props.mode = 'VALUE'
        props.value = 0

        props = layout.operator("view3d.select_border", text="Box Mask")
        props = layout.operator("paint.mask_lasso_gesture", text="Lasso Mask")


class VIEW3D_MT_particle(Menu):
    bl_label = "Particle"

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings

        particle_edit = tool_settings.particle_edit

        layout.operator("particle.mirror")

        layout.operator("particle.remove_doubles")

        layout.separator()

        if particle_edit.select_mode == 'POINT':
            layout.operator("particle.subdivide")

        layout.operator("particle.unify_length")
        layout.operator("particle.rekey")
        layout.operator("particle.weight_set")

        layout.separator()

        layout.menu("VIEW3D_MT_particle_showhide")

        layout.separator()

        layout.operator("particle.delete")


class VIEW3D_MT_particle_specials(Menu):
    bl_label = "Particle Context Menu"

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings

        particle_edit = tool_settings.particle_edit

        layout.operator("particle.rekey")

        layout.separator()

        layout.operator("particle.delete")

        layout.separator()

        layout.operator("particle.remove_doubles")
        layout.operator("particle.unify_length")

        if particle_edit.select_mode == 'POINT':
            layout.operator("particle.subdivide")

        layout.operator("particle.weight_set")

        layout.separator()

        layout.operator("particle.mirror")

        if particle_edit.select_mode == 'POINT':
            layout.separator()

            layout.operator("particle.select_all", text="All").action = 'SELECT'
            layout.operator("particle.select_all", text="None").action = 'DESELECT'
            layout.operator("particle.select_all", text="Invert").action = 'INVERT'

            layout.separator()

            layout.operator("particle.select_roots")
            layout.operator("particle.select_tips")

            layout.separator()

            layout.operator("particle.select_random")

            layout.separator()

            layout.operator("particle.select_more")
            layout.operator("particle.select_less")

            layout.separator()

            layout.operator("particle.select_linked")


class VIEW3D_MT_particle_showhide(ShowHideMenu, Menu):
    _operator_name = "particle"


class VIEW3D_MT_pose(Menu):
    bl_label = "Pose"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_transform_armature")

        layout.menu("VIEW3D_MT_pose_transform")
        layout.menu("VIEW3D_MT_pose_apply")

        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.menu("VIEW3D_MT_object_animation")

        layout.separator()

        layout.menu("VIEW3D_MT_pose_slide")
        layout.menu("VIEW3D_MT_pose_propagate")

        layout.separator()

        layout.operator("pose.copy")
        layout.operator("pose.paste").flipped = False
        layout.operator("pose.paste", text="Paste X-Flipped Pose").flipped = True

        layout.separator()

        layout.menu("VIEW3D_MT_pose_library")
        layout.menu("VIEW3D_MT_pose_motion")
        layout.menu("VIEW3D_MT_pose_group")

        layout.separator()

        layout.menu("VIEW3D_MT_object_parent")
        layout.menu("VIEW3D_MT_pose_ik")
        layout.menu("VIEW3D_MT_pose_constraints")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        layout.operator("pose.autoside_names", text="AutoName Left/Right").axis = 'XAXIS'
        layout.operator("pose.autoside_names", text="AutoName Front/Back").axis = 'YAXIS'
        layout.operator("pose.autoside_names", text="AutoName Top/Bottom").axis = 'ZAXIS'

        layout.operator("pose.flip_names")

        layout.operator("pose.quaternions_flip")

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("armature.armature_layers", text="Change Armature Layers...")
        layout.operator("pose.bone_layers", text="Change Bone Layers...")

        layout.separator()

        layout.menu("VIEW3D_MT_pose_showhide")
        layout.menu("VIEW3D_MT_bone_options_toggle", text="Bone Settings")


class VIEW3D_MT_pose_transform(Menu):
    bl_label = "Clear Transform"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.transforms_clear", text="All")

        layout.separator()

        layout.operator("pose.loc_clear", text="Location")
        layout.operator("pose.rot_clear", text="Rotation")
        layout.operator("pose.scale_clear", text="Scale")

        layout.separator()

        layout.operator("pose.user_transforms_clear", text="Reset Unkeyed")


class VIEW3D_MT_pose_slide(Menu):
    bl_label = "In-Betweens"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.push")
        layout.operator("pose.relax")
        layout.operator("pose.breakdown")


class VIEW3D_MT_pose_propagate(Menu):
    bl_label = "Propagate"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.propagate").mode = 'WHILE_HELD'

        layout.separator()

        layout.operator("pose.propagate", text="To Next Keyframe").mode = 'NEXT_KEY'
        layout.operator("pose.propagate", text="To Last Keyframe (Make Cyclic)").mode = 'LAST_KEY'

        layout.separator()

        layout.operator("pose.propagate", text="On Selected Keyframes").mode = 'SELECTED_KEYS'

        layout.separator()

        layout.operator("pose.propagate", text="On Selected Markers").mode = 'SELECTED_MARKERS'


class VIEW3D_MT_pose_library(Menu):
    bl_label = "Pose Library"

    def draw(self, context):
        layout = self.layout

        layout.operator("poselib.browse_interactive", text="Browse Poses...")

        layout.separator()

        layout.operator("poselib.pose_add", text="Add Pose...")
        layout.operator("poselib.pose_rename", text="Rename Pose...")
        layout.operator("poselib.pose_remove", text="Remove Pose...")


class VIEW3D_MT_pose_motion(Menu):
    bl_label = "Motion Paths"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.paths_calculate", text="Calculate")
        layout.operator("pose.paths_clear", text="Clear")


class VIEW3D_MT_pose_group(Menu):
    bl_label = "Bone Groups"

    def draw(self, context):
        layout = self.layout

        pose = context.active_object.pose

        layout.operator_context = 'EXEC_AREA'
        layout.operator("pose.group_assign", text="Assign to New Group").type = 0

        if pose.bone_groups:
            active_group = pose.bone_groups.active_index + 1
            layout.operator("pose.group_assign", text="Assign to Group").type = active_group

            layout.separator()

            # layout.operator_context = 'INVOKE_AREA'
            layout.operator("pose.group_unassign")
            layout.operator("pose.group_remove")


class VIEW3D_MT_pose_ik(Menu):
    bl_label = "Inverse Kinematics"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.ik_add")
        layout.operator("pose.ik_clear")


class VIEW3D_MT_pose_constraints(Menu):
    bl_label = "Constraints"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.constraint_add_with_targets", text="Add (With Targets)...")
        layout.operator("pose.constraints_copy")
        layout.operator("pose.constraints_clear")


class VIEW3D_MT_pose_showhide(ShowHideMenu, Menu):
    _operator_name = "pose"


class VIEW3D_MT_pose_apply(Menu):
    bl_label = "Apply"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.armature_apply")
        layout.operator("pose.visual_transform_apply")


class VIEW3D_MT_pose_specials(Menu):
    bl_label = "Pose Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator("anim.keyframe_insert_menu", text="Insert Keyframe...")

        layout.separator()

        layout.operator("pose.copy")
        layout.operator("pose.paste").flipped = False
        layout.operator("pose.paste", text="Paste X-Flipped Pose").flipped = True

        layout.separator()

        layout.operator("pose.select_constraint_target")

        layout.separator()

        layout.operator("pose.paths_calculate", text="Calculate")
        layout.operator("pose.paths_clear", text="Clear")

        layout.separator()

        layout.operator("pose.paths_calculate")
        layout.operator("pose.paths_clear")

        layout.separator()

        layout.operator("pose.hide").unselected = False
        layout.operator("pose.reveal")

        layout.separator()

        layout.operator("pose.user_transforms_clear")


class BoneOptions:
    def draw(self, context):
        layout = self.layout

        options = [
            "show_wire",
            "use_deform",
            "use_envelope_multiply",
            "use_inherit_rotation",
            "use_inherit_scale",
        ]

        if context.mode == 'EDIT_ARMATURE':
            bone_props = bpy.types.EditBone.bl_rna.properties
            data_path_iter = "selected_bones"
            opt_suffix = ""
            options.append("lock")
        else:  # pose-mode
            bone_props = bpy.types.Bone.bl_rna.properties
            data_path_iter = "selected_pose_bones"
            opt_suffix = "bone."

        for opt in options:
            props = layout.operator("wm.context_collection_boolean_set", text=bone_props[opt].name,
                                    text_ctxt=i18n_contexts.default)
            props.data_path_iter = data_path_iter
            props.data_path_item = opt_suffix + opt
            props.type = self.type


class VIEW3D_MT_bone_options_toggle(Menu, BoneOptions):
    bl_label = "Toggle Bone Options"
    type = 'TOGGLE'


class VIEW3D_MT_bone_options_enable(Menu, BoneOptions):
    bl_label = "Enable Bone Options"
    type = 'ENABLE'


class VIEW3D_MT_bone_options_disable(Menu, BoneOptions):
    bl_label = "Disable Bone Options"
    type = 'DISABLE'


# ********** Edit Menus, suffix from ob.type **********


class VIEW3D_MT_edit_mesh(Menu):
    bl_label = "Mesh"

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings

        with_bullet = bpy.app.build_options.bullet

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("mesh.duplicate_move", text="Duplicate")
        layout.menu("VIEW3D_MT_edit_mesh_extrude")
        layout.operator("mesh.split")
        layout.operator("mesh.bisect")

        if with_bullet:
            layout.operator("mesh.convex_hull")

        layout.separator()

        layout.operator("mesh.symmetrize")
        layout.operator("mesh.symmetry_snap")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_normals")
        layout.menu("VIEW3D_MT_edit_mesh_shading")
        layout.menu("VIEW3D_MT_edit_mesh_weights")
        layout.operator_menu_enum("mesh.sort_elements", "type", text="Sort Elements...")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_showhide")
        layout.operator_menu_enum("mesh.separate", "type")
        layout.menu("VIEW3D_MT_edit_mesh_clean")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_delete")


class VIEW3D_MT_edit_mesh_specials(Menu):
    bl_label = "Mesh Context Menu"

    def draw(self, context):
        layout = self.layout

        select_mode = context.tool_settings.mesh_select_mode

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.subdivide", text="Subdivide")

        layout.separator()

        layout.operator("mesh.duplicate_move", text="Duplicate")

        # Vertex Select Commands
        if select_mode[0]:
            layout.separator()

            layout.operator("mesh.edge_face_add", text="New Edge/Face from Vertices")
            layout.operator("mesh.vert_connect_path", text="Connect Vertex Path")
            layout.operator("mesh.vert_connect", text="Connect Vertex Pairs")

            layout.separator()

            layout.operator("mesh.vertices_smooth", text="Smooth")
            layout.operator("mesh.vertices_smooth_laplacian", text="Smooth Laplacian")

            layout.separator()
            layout.operator("mesh.merge", text="Merge Vertices...")
            layout.operator("mesh.remove_doubles", text="Remove Double Vertices")
            layout.operator("mesh.dissolve_verts")
            layout.operator("mesh.delete", text="Delete Vertices").type = 'VERT'

        # Edge Select Commands
        if select_mode[1]:
            layout.separator()

            layout.operator("mesh.bridge_edge_loops", text="Bridge Edge Loops")

            layout.separator()

            layout.operator("mesh.dissolve_edges")
            layout.operator("mesh.delete", text="Delete Edges").type = 'EDGE'

        # Face Select Commands
        if select_mode[2]:
            layout.separator()

            layout.operator("mesh.faces_shade_smooth")
            layout.operator("mesh.faces_shade_flat")

            layout.separator()

            layout.operator("mesh.bridge_edge_loops", text="Bridge Faces")

            layout.separator()

            layout.operator("mesh.poke")

            layout.separator()

            props = layout.operator("mesh.quads_convert_to_tris")
            props.quad_method = props.ngon_method = 'BEAUTY'
            layout.operator("mesh.tris_convert_to_quads")

            layout.separator()

            layout.menu("VIEW3D_MT_uv_map", text="UV Unwrap Faces...")

            layout.separator()

            layout.operator("mesh.dissolve_faces")
            layout.operator("mesh.delete", text="Delete Faces").type = 'FACE'

        # General Mesh Commands

        layout.separator()

        layout.menu("VIEW3D_MT_snap", text="Snap...")
        layout.operator("transform.mirror", text="Mirror")
        layout.operator("mesh.symmetrize")
        layout.operator("mesh.symmetry_snap")

        layout.separator()

        layout.operator("mesh.hide", text="Hide").unselected = False
        layout.operator("mesh.reveal", text="Reveal")


class VIEW3D_MT_edit_mesh_select_mode(Menu):
    bl_label = "Mesh Select Mode"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.select_mode", text="Vertex", icon='VERTEXSEL').type = 'VERT'
        layout.operator("mesh.select_mode", text="Edge", icon='EDGESEL').type = 'EDGE'
        layout.operator("mesh.select_mode", text="Face", icon='FACESEL').type = 'FACE'


class VIEW3D_MT_edit_mesh_extrude(Menu):
    bl_label = "Extrude"

    _extrude_funcs = {
        'VERT': lambda layout:
            layout.operator("mesh.extrude_vertices_move", text="Extrude Vertices"),
        'EDGE': lambda layout:
            layout.operator("mesh.extrude_edges_move", text="Extrude Edges"),
        'REGION': lambda layout:
            layout.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Faces"),
        'REGION_VERT_NORMAL': lambda layout:
            layout.operator("view3d.edit_mesh_extrude_move_shrink_fatten", text="Extrude Faces Along Normals"),
        'FACE': lambda layout:
            layout.operator("mesh.extrude_faces_move", text="Extrude Individual Faces"),
    }

    @staticmethod
    def extrude_options(context):
        tool_settings = context.tool_settings
        select_mode = tool_settings.mesh_select_mode
        mesh = context.object.data

        menu = []
        if mesh.total_face_sel:
            menu += ['REGION', 'REGION_VERT_NORMAL', 'FACE']
        if mesh.total_edge_sel and (select_mode[0] or select_mode[1]):
            menu += ['EDGE']
        if mesh.total_vert_sel and select_mode[0]:
            menu += ['VERT']

        # should never get here
        return menu

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        for menu_id in self.extrude_options(context):
            self._extrude_funcs[menu_id](layout)


class VIEW3D_MT_edit_mesh_vertices(Menu):
    bl_label = "Vertex"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.extrude_vertices_move", text="Extrude Vertices"),
        layout.operator("mesh.bevel", text="Bevel Vertices").vertex_only = True

        layout.separator()

        layout.operator("mesh.edge_face_add", text="New Edge/Face from Vertices")
        layout.operator("mesh.vert_connect_path", text="Connect Vertex Path")
        layout.operator("mesh.vert_connect", text="Connect Vertex Pairs")

        layout.separator()

        props = layout.operator("mesh.rip_move", text="Rip Vertices")
        props.MESH_OT_rip.use_fill = False
        props = layout.operator("mesh.rip_move", text="Rip Vertices and Fill")
        props.MESH_OT_rip.use_fill = True
        layout.operator("mesh.rip_edge_move", text="Rip Vertices and Extend")

        layout.separator()

        layout.operator("transform.vert_slide", text="Slide Vertices")
        layout.operator("mesh.vertices_smooth", text="Smooth Vertices")

        layout.separator()

        layout.operator("mesh.blend_from_shape")
        layout.operator("mesh.shape_propagate_to_all", text="Propagate to Shapes")

        layout.separator()

        layout.operator("mesh.merge", text="Merge Vertices")
        layout.operator("mesh.remove_doubles", text="Remove Double Vertices")

        layout.separator()

        layout.menu("VIEW3D_MT_vertex_group")
        layout.menu("VIEW3D_MT_hook")

        layout.separator()

        layout.operator("object.vertex_parent_set")


class VIEW3D_MT_edit_mesh_edges_data(Menu):
    bl_label = "Edge Data"

    def draw(self, context):
        layout = self.layout

        with_freestyle = bpy.app.build_options.freestyle

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("transform.edge_crease")
        layout.operator("transform.edge_bevelweight")

        layout.separator()

        layout.operator("mesh.mark_seam").clear = False
        layout.operator("mesh.mark_seam", text="Clear Seam").clear = True

        layout.separator()

        layout.operator("mesh.mark_sharp")
        layout.operator("mesh.mark_sharp", text="Clear Sharp").clear = True

        layout.operator("mesh.mark_sharp", text="Mark Sharp from Vertices").use_verts = True
        props = layout.operator("mesh.mark_sharp", text="Clear Sharp from Vertices")
        props.use_verts = True
        props.clear = True

        if with_freestyle:
            layout.separator()

            layout.operator("mesh.mark_freestyle_edge").clear = False
            layout.operator("mesh.mark_freestyle_edge", text="Clear Freestyle Edge").clear = True


class VIEW3D_MT_edit_mesh_edges(Menu):
    bl_label = "Edge"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.extrude_edges_move", text="Extrude Edges"),
        layout.operator("mesh.bevel", text="Bevel Edges").vertex_only = False
        layout.operator("mesh.bridge_edge_loops")

        layout.separator()

        layout.operator("mesh.subdivide")
        layout.operator("mesh.subdivide_edgering")
        layout.operator("mesh.unsubdivide")

        layout.separator()

        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").use_ccw = False
        layout.operator("mesh.edge_rotate", text="Rotate Edge CCW").use_ccw = True

        layout.separator()

        layout.operator("transform.edge_slide")
        layout.operator("mesh.edge_split")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_edges_data")


class VIEW3D_MT_edit_mesh_faces_data(Menu):
    bl_label = "Face Data"

    def draw(self, context):
        layout = self.layout

        with_freestyle = bpy.app.build_options.freestyle

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.colors_rotate")
        layout.operator("mesh.colors_reverse")

        layout.separator()

        layout.operator("mesh.uvs_rotate")
        layout.operator("mesh.uvs_reverse")

        layout.separator()

        if with_freestyle:
            layout.operator("mesh.mark_freestyle_face").clear = False
            layout.operator("mesh.mark_freestyle_face", text="Clear Freestyle Face").clear = True


class VIEW3D_MT_edit_mesh_faces(Menu):
    bl_label = "Face"
    bl_idname = "VIEW3D_MT_edit_mesh_faces"

    def draw(self, context):
        layout = self.layout

        with_freestyle = bpy.app.build_options.freestyle

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Faces"),
        layout.operator("view3d.edit_mesh_extrude_move_shrink_fatten", text="Extrude Faces Along Normals"),
        layout.operator("mesh.extrude_faces_move", text="Extrude Individual Faces"),

        layout.separator()

        layout.operator("mesh.inset")
        layout.operator("mesh.poke")
        props = layout.operator("mesh.quads_convert_to_tris")
        props.quad_method = props.ngon_method = 'BEAUTY'
        layout.operator("mesh.tris_convert_to_quads")
        layout.operator("mesh.solidify", text="Solidify Faces")
        layout.operator("mesh.wireframe")

        layout.separator()

        layout.operator("mesh.fill")
        layout.operator("mesh.fill_grid")
        layout.operator("mesh.beautify_fill")

        layout.separator()

        layout.operator("mesh.intersect")
        layout.operator("mesh.intersect_boolean")

        layout.separator()

        layout.operator("mesh.face_split_by_edges")

        layout.separator()

        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_faces_data")


class VIEW3D_MT_edit_mesh_normals(Menu):
    bl_label = "Normals"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.normals_make_consistent", text="Recalculate Outside").inside = False
        layout.operator("mesh.normals_make_consistent", text="Recalculate Inside").inside = True

        layout.separator()

        layout.operator("mesh.flip_normals")
        layout.operator("mesh.set_normals_from_faces", text="Set From Faces")

        layout.operator("transform.rotate_normal", text="Rotate Normal")
        layout.operator("mesh.point_normals", text="Point normals to target")

        layout.operator("mesh.merge_normals", text="Merge")
        layout.operator("mesh.split_normals", text="Split")

        layout.operator("mesh.average_normals", text="Average Normals")

        layout.label(text="Normal Vector")

        layout.operator("mesh.normals_tools", text="Copy").mode = 'COPY'
        layout.operator("mesh.normals_tools", text="Paste").mode = 'PASTE'

        layout.operator("mesh.normals_tools", text="Multiply").mode = 'MULTIPLY'
        layout.operator("mesh.normals_tools", text="Add").mode = 'ADD'

        layout.operator("mesh.normals_tools", text="Reset").mode = 'RESET'

        layout.operator("mesh.smoothen_normals", text="Smoothen")

        layout.label(text="Face Strength")
        layout.operator("mesh.mod_weighted_strength", text="Face select", icon = "FACESEL").set = False
        layout.operator("mesh.mod_weighted_strength", text="Set Strength", icon = "ZOOMIN").set = True


class VIEW3D_MT_edit_mesh_shading(Menu):
    bl_label = "Shading"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.faces_shade_smooth", text="Smooth Faces")
        layout.operator("mesh.faces_shade_flat", text="Flat Faces")

        layout.separator()

        layout.operator("mesh.mark_sharp", text="Smooth Edges").clear = True
        layout.operator("mesh.mark_sharp", text="Sharp Edges")

        layout.separator()

        props = layout.operator("mesh.mark_sharp", text="Smooth Vertices")
        props.use_verts = True
        props.clear = True

        layout.operator("mesh.mark_sharp", text="Sharp Vertices").use_verts = True


class VIEW3D_MT_edit_mesh_weights(Menu):
    bl_label = "Weights"

    def draw(self, context):
        VIEW3D_MT_paint_weight.draw_generic(self.layout, is_editmode=True)


class VIEW3D_MT_edit_mesh_clean(Menu):
    bl_label = "Clean Up"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.delete_loose")

        layout.separator()

        layout.operator("mesh.decimate")
        layout.operator("mesh.dissolve_degenerate")
        layout.operator("mesh.dissolve_limited")
        layout.operator("mesh.face_make_planar")

        layout.separator()

        layout.operator("mesh.vert_connect_nonplanar")
        layout.operator("mesh.vert_connect_concave")
        layout.operator("mesh.remove_doubles")
        layout.operator("mesh.fill_holes")


class VIEW3D_MT_edit_mesh_delete(Menu):
    bl_label = "Delete"

    def draw(self, context):
        layout = self.layout

        layout.operator_enum("mesh.delete", "type")

        layout.separator()

        layout.operator("mesh.dissolve_verts")
        layout.operator("mesh.dissolve_edges")
        layout.operator("mesh.dissolve_faces")

        layout.separator()

        layout.operator("mesh.dissolve_limited")

        layout.separator()

        layout.operator("mesh.edge_collapse")
        layout.operator("mesh.delete_edgeloop", text="Edge Loops")


class VIEW3D_MT_edit_mesh_showhide(ShowHideMenu, Menu):
    _operator_name = "mesh"


class VIEW3D_MT_edit_gpencil_delete(Menu):
    bl_label = "Delete"

    def draw(self, context):
        layout = self.layout

        layout.operator_enum("gpencil.delete", "type")

        layout.separator()

        layout.operator_enum("gpencil.dissolve", "type")

        layout.separator()

        layout.operator("gpencil.active_frames_delete_all")

        layout.separator()

        layout.operator("gpencil.frame_clean_fill", text="Clean Boundary Strokes").mode = 'ACTIVE'
        layout.operator("gpencil.frame_clean_fill", text="Clean Boundary Strokes all Frames").mode = 'ALL'


# Edit Curve
# draw_curve is used by VIEW3D_MT_edit_curve and VIEW3D_MT_edit_surface


def draw_curve(self, context):
    layout = self.layout

    layout.menu("VIEW3D_MT_transform")
    layout.menu("VIEW3D_MT_mirror")
    layout.menu("VIEW3D_MT_snap")

    layout.separator()

    layout.operator("curve.extrude_move")
    layout.operator("curve.spin")
    layout.operator("curve.duplicate_move")

    layout.separator()

    layout.operator("curve.split")
    layout.operator("curve.separate")
    layout.operator("curve.make_segment")
    layout.operator("curve.cyclic_toggle")

    layout.separator()

    layout.menu("VIEW3D_MT_edit_curve_ctrlpoints")
    layout.menu("VIEW3D_MT_edit_curve_segments")

    layout.separator()

    layout.menu("VIEW3D_MT_edit_curve_showhide")
    layout.menu("VIEW3D_MT_edit_curve_clean")
    layout.menu("VIEW3D_MT_edit_curve_delete")


class VIEW3D_MT_edit_curve(Menu):
    bl_label = "Curve"

    draw = draw_curve


class VIEW3D_MT_edit_curve_ctrlpoints(Menu):
    bl_label = "Control Points"

    def draw(self, context):
        layout = self.layout

        edit_object = context.edit_object

        if edit_object.type == 'CURVE':
            layout.operator("transform.tilt")
            layout.operator("curve.tilt_clear")

            layout.separator()

            layout.operator_menu_enum("curve.handle_type_set", "type")
            layout.operator("curve.normals_make_consistent")

            layout.separator()

        layout.menu("VIEW3D_MT_hook")

        layout.separator()

        layout.operator("object.vertex_parent_set")


class VIEW3D_MT_edit_curve_segments(Menu):
    bl_label = "Segments"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.subdivide")
        layout.operator("curve.switch_direction")


class VIEW3D_MT_edit_curve_clean(Menu):
    bl_label = "Clean Up"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.decimate")


class VIEW3D_MT_edit_curve_specials(Menu):
    bl_label = "Curve Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.subdivide")
        layout.operator("curve.switch_direction")
        layout.operator("curve.spline_weight_set")
        layout.operator("curve.radius_set")

        layout.separator()

        layout.operator("curve.smooth")
        layout.operator("curve.smooth_weight")
        layout.operator("curve.smooth_radius")
        layout.operator("curve.smooth_tilt")


class VIEW3D_MT_edit_curve_delete(Menu):
    bl_label = "Delete"

    def draw(self, context):
        layout = self.layout

        layout.operator_enum("curve.delete", "type")

        layout.separator()

        layout.operator("curve.dissolve_verts")


class VIEW3D_MT_edit_curve_showhide(ShowHideMenu, Menu):
    _operator_name = "curve"


class VIEW3D_MT_edit_surface(Menu):
    bl_label = "Surface"

    draw = draw_curve


class VIEW3D_MT_edit_font(Menu):
    bl_label = "Text"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_edit_text_chars")

        layout.separator()

        layout.operator("font.style_toggle", text="Toggle Bold").style = 'BOLD'
        layout.operator("font.style_toggle", text="Toggle Italic").style = 'ITALIC'

        layout.separator()

        layout.operator("font.style_toggle", text="Toggle Underline").style = 'UNDERLINE'
        layout.operator("font.style_toggle", text="Toggle Small Caps").style = 'SMALL_CAPS'


class VIEW3D_MT_edit_text_chars(Menu):
    bl_label = "Special Characters"

    def draw(self, context):
        layout = self.layout

        layout.operator("font.text_insert", text="Copyright").text = "\u00A9"
        layout.operator("font.text_insert", text="Registered Trademark").text = "\u00AE"

        layout.separator()

        layout.operator("font.text_insert", text="Degree Sign").text = "\u00B0"
        layout.operator("font.text_insert", text="Multiplication Sign").text = "\u00D7"
        layout.operator("font.text_insert", text="Circle").text = "\u008A"

        layout.separator()

        layout.operator("font.text_insert", text="Superscript 1").text = "\u00B9"
        layout.operator("font.text_insert", text="Superscript 2").text = "\u00B2"
        layout.operator("font.text_insert", text="Superscript 3").text = "\u00B3"

        layout.separator()

        layout.operator("font.text_insert", text="Double >>").text = "\u00BB"
        layout.operator("font.text_insert", text="Double <<").text = "\u00AB"
        layout.operator("font.text_insert", text="Promillage").text = "\u2030"

        layout.separator()

        layout.operator("font.text_insert", text="Dutch Florin").text = "\u00A4"
        layout.operator("font.text_insert", text="British Pound").text = "\u00A3"
        layout.operator("font.text_insert", text="Japanese Yen").text = "\u00A5"

        layout.separator()

        layout.operator("font.text_insert", text="German S").text = "\u00DF"
        layout.operator("font.text_insert", text="Spanish Question Mark").text = "\u00BF"
        layout.operator("font.text_insert", text="Spanish Exclamation Mark").text = "\u00A1"


class VIEW3D_MT_edit_meta(Menu):
    bl_label = "Metaball"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("mball.duplicate_metaelems")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_meta_showhide")
        layout.operator("mball.delete_metaelems", text="Delete...")


class VIEW3D_MT_edit_meta_showhide(Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        layout = self.layout

        layout.operator("mball.reveal_metaelems", text="Show Hidden")
        layout.operator("mball.hide_metaelems", text="Hide Selected").unselected = False
        layout.operator("mball.hide_metaelems", text="Hide Unselected").unselected = True


class VIEW3D_MT_edit_lattice(Menu):
    bl_label = "Lattice"

    def draw(self, context):
        layout = self.layout

        layout.separator()

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")
        layout.operator_menu_enum("lattice.flip", "axis")

        layout.separator()

        layout.operator("lattice.make_regular")

        layout.separator()

        layout.operator("object.vertex_parent_set")


class VIEW3D_MT_edit_armature(Menu):
    bl_label = "Armature"

    def draw(self, context):
        layout = self.layout

        edit_object = context.edit_object
        arm = edit_object.data

        layout.menu("VIEW3D_MT_transform_armature")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")
        layout.menu("VIEW3D_MT_edit_armature_roll")

        layout.separator()

        layout.operator("armature.extrude_move")

        if arm.use_mirror_x:
            layout.operator("armature.extrude_forked")

        layout.operator("armature.duplicate_move")
        layout.operator("armature.merge")
        layout.operator("armature.fill")
        layout.operator("armature.split")
        layout.operator("armature.separate")

        layout.separator()

        layout.operator("armature.subdivide", text="Subdivide")
        layout.operator("armature.switch_direction", text="Switch Direction")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        layout.operator("armature.symmetrize")
        layout.operator("armature.autoside_names", text="AutoName Left/Right").type = 'XAXIS'
        layout.operator("armature.autoside_names", text="AutoName Front/Back").type = 'YAXIS'
        layout.operator("armature.autoside_names", text="AutoName Top/Bottom").type = 'ZAXIS'
        layout.operator("armature.flip_names")

        layout.separator()

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("armature.armature_layers")
        layout.operator("armature.bone_layers")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_armature_parent")

        layout.separator()

        layout.menu("VIEW3D_MT_bone_options_toggle", text="Bone Settings")

        layout.separator()

        layout.operator("armature.delete")


class VIEW3D_MT_armature_specials(Menu):
    bl_label = "Armature Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("armature.subdivide", text="Subdivide")
        layout.operator("armature.switch_direction", text="Switch Direction")

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("armature.autoside_names", text="AutoName Left/Right").type = 'XAXIS'
        layout.operator("armature.autoside_names", text="AutoName Front/Back").type = 'YAXIS'
        layout.operator("armature.autoside_names", text="AutoName Top/Bottom").type = 'ZAXIS'
        layout.operator("armature.flip_names", text="Flip Names")
        layout.operator("armature.symmetrize")


class VIEW3D_MT_edit_armature_parent(Menu):
    bl_label = "Parent"

    def draw(self, context):
        layout = self.layout

        layout.operator("armature.parent_set", text="Make")
        layout.operator("armature.parent_clear", text="Clear")


class VIEW3D_MT_edit_armature_roll(Menu):
    bl_label = "Bone Roll"

    def draw(self, context):
        layout = self.layout

        layout.operator_menu_enum("armature.calculate_roll", "type")

        layout.separator()

        layout.operator("transform.transform", text="Set Roll").mode = 'BONE_ROLL'
        layout.operator("armature.roll_clear")


class VIEW3D_MT_edit_armature_delete(Menu):
    bl_label = "Delete"

    def draw(self, context):
        layout = self.layout

        layout.operator("armature.delete", text="Delete Bones")

        layout.separator()

        layout.operator("armature.dissolve", text="Dissolve")


# ********** Grease Pencil Stroke menus **********
class VIEW3D_MT_gpencil_simplify(Menu):
    bl_label = "Simplify"

    def draw(self, context):
        layout = self.layout
        layout.operator("gpencil.stroke_simplify_fixed", text="Fixed")
        layout.operator("gpencil.stroke_simplify", text="Adaptative")


class VIEW3D_MT_paint_gpencil(Menu):
    bl_label = "Strokes"

    def draw(self, context):

        layout = self.layout

        layout.menu("VIEW3D_MT_gpencil_animation")
        layout.menu("VIEW3D_MT_edit_gpencil_interpolate")

        layout.separator()

        layout.operator("gpencil.delete", text="Delete Frame").type = 'FRAME'
        layout.operator("gpencil.active_frames_delete_all")


class VIEW3D_MT_assign_material(Menu):
    bl_label = "Assign Material"

    def draw(self, context):
        layout = self.layout
        ob = context.active_object

        for slot in ob.material_slots:
            mat = slot.material
            layout.operator("gpencil.stroke_change_color", text=mat.name).material = mat.name


class VIEW3D_MT_edit_gpencil(Menu):
    bl_label = "Strokes"

    def draw(self, context):
        tool_settings = context.tool_settings

        layout = self.layout

        layout.menu("VIEW3D_MT_edit_gpencil_transform")

        layout.separator()
        layout.menu("GPENCIL_MT_snap")

        layout.separator()

        layout.menu("VIEW3D_MT_gpencil_animation")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_gpencil_interpolate")

        layout.separator()

        layout.operator("gpencil.duplicate_move", text="Duplicate")
        layout.operator("gpencil.stroke_subdivide", text="Subdivide")
        layout.menu("VIEW3D_MT_gpencil_simplify")

        layout.separator()

        layout.operator_menu_enum("gpencil.stroke_separate", "mode", text="Separate...")
        layout.operator("gpencil.stroke_split", text="Split")
        layout.operator_menu_enum("gpencil.stroke_join", "type", text="Join...")
        layout.operator("gpencil.stroke_flip", text="Flip Direction")

        layout.separator()

        layout.operator("gpencil.copy", text="Copy")
        layout.operator("gpencil.paste", text="Paste").type = 'COPY'
        layout.operator("gpencil.paste", text="Paste & Merge").type = 'MERGE'

        layout.separator()

        layout.operator_menu_enum("gpencil.move_to_layer", "layer", text="Move to Layer")
        layout.menu("VIEW3D_MT_assign_material")
        layout.operator_menu_enum("gpencil.stroke_arrange", "direction", text="Arrange Strokes...")

        layout.separator()

        layout.operator_menu_enum("gpencil.convert", "type", text="Convert to Geometry...")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_gpencil_delete")
        layout.operator("gpencil.stroke_cyclical_set", text="Toggle Cyclic").type = 'TOGGLE'

        layout.separator()

        layout.menu("GPENCIL_MT_cleanup")


class VIEW3D_MT_sculpt_gpencil(Menu):
    bl_label = "Strokes"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_edit_gpencil_transform")

        layout.separator()
        layout.menu("GPENCIL_MT_snap")

        layout.separator()

        layout.operator("gpencil.duplicate_move", text="Duplicate")
        layout.operator("gpencil.stroke_subdivide", text="Subdivide")
        layout.menu("VIEW3D_MT_gpencil_simplify")

        layout.separator()

        layout.operator_menu_enum("gpencil.stroke_separate", "mode", text="Separate...")
        layout.operator("gpencil.stroke_split", text="Split")
        layout.operator_menu_enum("gpencil.stroke_join", "type", text="Join...")
        layout.operator("gpencil.stroke_flip", text="Flip Direction")

        layout.separator()

        layout.operator("gpencil.copy", text="Copy")
        layout.operator("gpencil.paste", text="Paste").type = 'COPY'
        layout.operator("gpencil.paste", text="Paste & Merge").type = 'MERGE'

        layout.separator()

        layout.operator_menu_enum("gpencil.move_to_layer", "layer", text="Move to Layer")
        layout.menu("VIEW3D_MT_assign_material")
        layout.operator_menu_enum("gpencil.stroke_arrange", "direction", text="Arrange Strokes...")

        layout.separator()

        layout.operator_menu_enum("gpencil.convert", "type", text="Convert to Geometry...")


class VIEW3D_MT_weight_gpencil(Menu):
    bl_label = "Weights"

    def draw(self, context):
        layout = self.layout

        layout.operator("gpencil.vertex_group_invert", text="Invert")
        layout.operator("gpencil.vertex_group_smooth", text="Smooth")


class VIEW3D_MT_gpencil_animation(Menu):
    bl_label = "Animation"

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob and ob.type == 'GPENCIL' and ob.mode != 'OBJECT'

    @staticmethod
    def draw(self, context):
        layout = self.layout

        layout.operator("gpencil.blank_frame_add")
        layout.operator("gpencil.active_frames_delete_all", text="Delete Frame(s)")

        layout.separator()
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame")
        layout.operator("gpencil.frame_duplicate", text="Duplicate All Layers").mode = 'ALL'


class VIEW3D_MT_edit_gpencil_transform(Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize", text="Scale")

        layout.separator()

        layout.operator("transform.bend", text="Bend")
        layout.operator("transform.shear", text="Shear")
        layout.operator("transform.tosphere", text="To Sphere")
        layout.operator("transform.transform", text="Shrink Fatten").mode = 'GPENCIL_SHRINKFATTEN'

        layout.separator()

        layout.operator("gpencil.reproject")


class VIEW3D_MT_edit_gpencil_interpolate(Menu):
    bl_label = "Interpolate"

    def draw(self, context):
        layout = self.layout

        layout.operator("gpencil.interpolate", text="Interpolate")
        layout.operator("gpencil.interpolate_sequence", text="Sequence")


class VIEW3D_MT_object_mode_pie(Menu):
    bl_label = "Mode"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator_enum("OBJECT_OT_mode_set", "mode")


class VIEW3D_MT_view_pie(Menu):
    bl_label = "View"
    bl_idname = "VIEW3D_MT_view_pie"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator_enum("VIEW3D_OT_view_axis", "type")
        pie.operator("view3d.view_camera", text="View Camera", icon='CAMERA_DATA')
        pie.operator("view3d.view_selected", text="View Selected", icon='ZOOM_SELECTED')


# ********** Panel **********


class VIEW3D_PT_view3d_properties(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=True)
        col = flow.column()

        subcol = col.column()
        subcol.active = bool(view.region_3d.view_perspective != 'CAMERA' or view.region_quadviews)
        subcol.prop(view, "lens", text="Focal Length")

        subcol = col.column(align=True)
        subcol.prop(view, "clip_start", text="Clip Start")
        subcol.prop(view, "clip_end", text="End")

        subcol.separator()

        col = flow.column()

        subcol = col.column()
        subcol.enabled = not view.lock_camera_and_layers
        subcol.prop(view, "camera", text="Local Camera")

        subcol = col.column(align=True)
        subcol.prop(view, "use_render_border")
        subcol.active = view.region_3d.view_perspective != 'CAMERA'


class VIEW3D_PT_view3d_camera_lock(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Camera Lock"
    bl_parent_id = "VIEW3D_PT_view3d_properties"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        view = context.space_data

        col = layout.column(align=True)
        subcol = col.column()
        subcol.active = bool(view.region_3d.view_perspective != 'CAMERA' or view.region_quadviews)

        subcol.prop(view, "lock_object")
        lock_object = view.lock_object
        if lock_object:
            if lock_object.type == 'ARMATURE':
                subcol.prop_search(
                    view, "lock_bone", lock_object.data,
                    "edit_bones" if lock_object.mode == 'EDIT'
                    else "bones",
                    text=""
                )
        else:
            subcol.prop(view, "lock_cursor", text="Lock to 3D Cursor")

        col.prop(view, "lock_camera")


class VIEW3D_PT_view3d_cursor(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "3D Cursor"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        view = context.space_data

        layout.column().prop(view, "cursor_location", text="Location")


class VIEW3D_PT_object_type_visibility(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "View Object Types"
    bl_ui_units_x = 6

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        view = context.space_data

        col = layout.column()

        attr_object_types = (
            # Geometry
            "mesh",
            "curve",
            "surf",
            "meta",
            "font",
            None,
            # Other
            "armature",
            "lattice",
            "empty",
            "grease_pencil",
            "camera",
            "light",
            "light_probe",
            "speaker",
        )

        for attr in attr_object_types:
            if attr is None:
                col.separator()
                continue

            attr_v = "show_object_viewport_" f"{attr:s}"
            attr_s = "show_object_select_" f"{attr:s}"

            icon_s = 'RESTRICT_SELECT_OFF' if getattr(view, attr_s) else 'RESTRICT_SELECT_ON'

            row = col.row(align=True)
            row.prop(view, attr_v)
            row.active = getattr(view, attr_v)
            row.prop(view, attr_s, text="", icon=icon_s, emboss=False)


class VIEW3D_PT_shading(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Shading"
    bl_ui_units_x = 11

    @classmethod
    def get_shading(cls, context):
        # Get settings from 3D viewport or OpenGL render engine
        view = context.space_data
        if view.type == 'VIEW_3D':
            return view.shading
        else:
            return context.scene.display.shading

    def draw(self, context):
        pass


class VIEW3D_PT_shading_lighting(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Lighting"
    bl_parent_id = 'VIEW3D_PT_shading'

    def draw(self, context):
        layout = self.layout
        shading = VIEW3D_PT_shading.get_shading(context)

        col = layout.column()
        split = col.split(0.9)

        if shading.type == 'SOLID':
            split.row().prop(shading, "light", expand=True)
            col = split.column()

            split = layout.split(0.9)
            col = split.column()
            sub = col.row()
            sub.scale_y = 0.6  # smaller matcap/hdri preview

            if shading.light == 'STUDIO':
                sub.template_icon_view(shading, "studio_light", scale=3)

                if shading.selected_studio_light.orientation == 'WORLD':
                    col.prop(shading, "studiolight_rotate_z", text="Rotation")

                col = split.column()
                col.operator('wm.studiolight_userpref_show', emboss=False, text="", icon='PREFERENCES')

            elif shading.light == 'MATCAP':
                sub.template_icon_view(shading, "studio_light", scale=3)

                col = split.column()
                col.operator('wm.studiolight_userpref_show', emboss=False, text="", icon='PREFERENCES')
                col.operator('VIEW3D_OT_toggle_matcap_flip', emboss=False, text="", icon='ARROW_LEFTRIGHT')

        elif shading.type == 'MATERIAL':
            col.prop(shading, "use_scene_lights")
            col.prop(shading, "use_scene_world")

            if not shading.use_scene_world:
                col = layout.column()
                split = col.split(0.9)

                col = split.column()
                sub = col.row()
                sub.scale_y = 0.6
                sub.template_icon_view(shading, "studio_light", scale=3)

                col = split.column()
                col.operator('wm.studiolight_userpref_show', emboss=False, text="", icon='PREFERENCES')

                if shading.selected_studio_light.orientation == 'WORLD':
                    split = layout.split(0.9)
                    col = split.column()
                    col.prop(shading, "studiolight_rotate_z", text="Rotation")
                    col.prop(shading, "studiolight_background_alpha")
                    col = split.column()  # to align properly with above


class VIEW3D_PT_shading_color(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Color"
    bl_parent_id = 'VIEW3D_PT_shading'

    @classmethod
    def poll(cls, context):
        shading = VIEW3D_PT_shading.get_shading(context)
        return shading.type == 'SOLID'

    def _draw_color_type(self, context):
        layout = self.layout
        shading = VIEW3D_PT_shading.get_shading(context)

        layout.row().prop(shading, 'color_type', expand=True)
        if shading.color_type == 'SINGLE':
            layout.row().prop(shading, 'single_color', text="")

    def _draw_background_color(self, context):
        layout = self.layout
        shading = VIEW3D_PT_shading.get_shading(context)

        layout.row().label("Background")
        layout.row().prop(shading, 'background_type', expand=True)
        if shading.background_type == 'VIEWPORT':
            layout.row().prop(shading, "background_color", text="")

    def draw(self, context):
        self._draw_color_type(context)
        self._draw_background_color(context)


class VIEW3D_PT_shading_options(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Options"
    bl_parent_id = 'VIEW3D_PT_shading'

    @classmethod
    def poll(cls, context):
        shading = VIEW3D_PT_shading.get_shading(context)
        return shading.type == 'SOLID'

    def draw(self, context):
        layout = self.layout

        shading = VIEW3D_PT_shading.get_shading(context)

        col = layout.column()

        is_xray = shading.show_xray
        is_shadows = shading.show_shadows

        icon_x = 'CHECKBOX_HLT' if is_xray else 'CHECKBOX_DEHLT'
        row = col.row()
        row.prop(shading, "show_xray", text="")
        sub = row.row()
        sub.active = is_xray
        sub.prop(shading, "xray_alpha", text="X-Ray")

        icon_s = 'CHECKBOX_HLT' if is_shadows else 'CHECKBOX_DEHLT'
        row = col.row()
        row.prop(shading, "show_shadows", text="")
        row.active = not is_xray
        sub = row.row(align=True)
        sub.active = is_shadows
        sub.prop(shading, "shadow_intensity", text="Shadow")
        sub.popover(
            panel="VIEW3D_PT_shading_options_shadow",
            icon='SCRIPTWIN',
            text=""
        )

        col = layout.column()
        row = col.row()
        row.active = not is_xray
        row.prop(shading, "show_cavity")

        if shading.show_cavity:
            sub = col.row(align=True)
            sub.active = not shading.show_xray and shading.show_cavity
            sub.prop(shading, "cavity_ridge_factor")
            sub.prop(shading, "cavity_valley_factor")
            sub.popover(
                panel="VIEW3D_PT_shading_options_ssao",
                icon='SCRIPTWIN',
                text=""
            )

        row = layout.split()
        row.prop(shading, "show_object_outline")
        sub = row.row()
        sub.active = shading.show_object_outline
        sub.prop(shading, "object_outline_color", text="")

        col = layout.column()
        if not shading.light == 'MATCAP':
            col.prop(shading, "show_specular_highlight")


class VIEW3D_PT_shading_options_shadow(Panel):
    bl_label = "Shadow Settings"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'

    @classmethod
    def poll(cls, context):
        return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene

        col = layout.column()
        col.prop(scene.display, "light_direction")
        col.prop(scene.display, "shadow_shift")


class VIEW3D_PT_shading_options_ssao(Panel):
    bl_label = "SSAO Settings"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'

    @classmethod
    def poll(cls, context):
        return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene

        col = layout.column(align=True)
        col.prop(scene.display, "matcap_ssao_samples")
        col.prop(scene.display, "matcap_ssao_distance")
        col.prop(scene.display, "matcap_ssao_attenuation")


class VIEW3D_PT_overlay(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Overlays"
    bl_ui_units_x = 13

    def draw(self, context):
        pass


class VIEW3D_PT_overlay_gizmo(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Gizmo"

    def draw_header(self, context):
        view = context.space_data
        self.layout.prop(view, "show_gizmo", text="")

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        row = col.row(align=True)
        row.active = view.show_gizmo
        row.prop(view, "show_gizmo_navigate", text="Navigate", toggle=True)
        row.prop(view, "show_gizmo_context", text="Active Object", toggle=True)
        row.prop(view, "show_gizmo_tool", text="Active Tools", toggle=True)


class VIEW3D_PT_overlay_guides(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Guides"

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        shading = view.shading
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        split = col.split()
        sub = split.column()
        sub.prop(overlay, "show_floor", text="Grid")

        if overlay.show_floor:
            sub = col.column(align=True)
            sub.active = bool(overlay.show_floor or view.region_quadviews or not view.region_3d.is_perspective)
            subsub = sub.row(align=True)
            subsub.active = overlay.show_floor
            subsub.prop(overlay, "grid_scale", text="Scale")
            subsub.prop(overlay, "grid_subdivisions", text="Subdivisions")

        sub = split.column()
        row = sub.row()
        row.label(text="Axes")

        subrow = row.row(align=True)
        subrow.prop(overlay, "show_axis_x", text="X", toggle=True)
        subrow.prop(overlay, "show_axis_y", text="Y", toggle=True)
        subrow.prop(overlay, "show_axis_z", text="Z", toggle=True)

        split = col.split()
        sub = split.column()
        sub.prop(overlay, "show_text", text="Text Info")
        sub = split.column()
        sub.prop(overlay, "show_cursor", text="3D Cursor")

        if shading.type == 'MATERIAL':
            col.prop(overlay, "show_look_dev")

        col.prop(overlay, "show_annotation", text="Annotations")


class VIEW3D_PT_overlay_object(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Objects"

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        shading = view.shading
        display_all = overlay.show_overlays

        col = layout.column(align=True)
        col.active = display_all

        split = col.split()

        sub = split.column(align=True)
        sub.prop(overlay, "show_ornaments", text="Ornaments")
        sub.prop(overlay, "show_relationship_lines")
        sub.prop(overlay, "show_outline_selected")

        sub = split.column(align=True)
        sub.prop(overlay, "show_bones", text="Bones")
        sub.prop(overlay, "show_motion_paths")
        sub.prop(overlay, "show_object_origins", text="Origins")
        subsub = sub.column()
        subsub.active = overlay.show_object_origins
        subsub.prop(overlay, "show_object_origins_all", text="Origins (All)")


class VIEW3D_PT_overlay_geometry(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Geometry"

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        row = col.row()
        row.prop(overlay, "show_wireframes", text="")
        sub = row.row()
        sub.active = overlay.show_wireframes
        sub.prop(overlay, "wireframe_threshold", text="Wireframe")

        col = layout.column(align=True)
        col.active = display_all

        split = col.split()
        sub = split.column(align=True)
        sub.prop(overlay, "show_backface_culling")

        sub = split.column(align=True)
        #sub.prop(overlay, "show_onion_skins")
        sub.prop(overlay, "show_face_orientation")


class VIEW3D_PT_overlay_motion_tracking(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Motion Tracking"

    def draw_header(self, context):
        view = context.space_data
        self.layout.prop(view, "show_reconstruction", text="")

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        if view.show_reconstruction:
            split = col.split()

            sub = split.column(align=True)
            sub.active = view.show_reconstruction
            sub.prop(view, "show_camera_path", text="Camera Path")

            sub = split.column()
            sub.prop(view, "show_bundle_names", text="Marker Names")

            col = layout.column()
            col.label(text="Tracks:")
            row = col.row(align=True)
            row.prop(view, "tracks_draw_type", text="")
            row.prop(view, "tracks_draw_size", text="Size")


class VIEW3D_PT_overlay_edit_mesh(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Mesh Edit Mode"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays
        data = context.active_object.data
        with_freestyle = bpy.app.build_options.freestyle

        col = layout.column()
        col.active = display_all

        split = col.split()

        sub = split.column()
        sub.prop(data, "show_edges", text="Edges")
        sub = split.column()
        sub.prop(data, "show_faces", text="Faces")

        row = col.row(align=True)
        row.prop(data, "show_edge_crease", text="Creases", toggle=True)
        row.prop(data, "show_edge_sharp", text="Sharp", text_ctxt=i18n_contexts.plural, toggle=True)
        row.prop(data, "show_edge_bevel_weight", text="Bevel", toggle=True)

        if not with_freestyle:
            row.prop(data, "show_edge_seams", text="Seams", toggle=True)


class VIEW3D_PT_overlay_edit_mesh_shading(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay_edit_mesh'
    bl_label = "Shading"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        tool_settings = context.tool_settings
        display_all = overlay.show_overlays
        data = context.active_object.data
        statvis = tool_settings.statvis

        col = layout.column()
        col.active = display_all

        col.prop(overlay, "show_occlude_wire")

        col.prop(overlay, "show_weight", text="Vertex Group Weights")
        if overlay.show_weight:
            row = col.split()
            row.label(text="Zero Weights")
            row.prop(tool_settings, "vertex_group_user", text="")

        col.prop(data, "show_statvis", text="Mesh Analysis")
        if data.show_statvis:
            col = col.column()

            sub = col.split()
            sub.active = data.show_statvis
            sub.label(text="Type")
            sub.prop(statvis, "type", text="")

            statvis_type = statvis.type
            if statvis_type == 'OVERHANG':
                row = col.row(align=True)
                row.prop(statvis, "overhang_min", text="Minimum")
                row.prop(statvis, "overhang_max", text="Maximum")
                col.row().prop(statvis, "overhang_axis", expand=True)
            elif statvis_type == 'THICKNESS':
                row = col.row(align=True)
                row.prop(statvis, "thickness_min", text="Minimum")
                row.prop(statvis, "thickness_max", text="Maximum")
                col.prop(statvis, "thickness_samples")
            elif statvis_type == 'INTERSECT':
                pass
            elif statvis_type == 'DISTORT':
                row = col.row(align=True)
                row.prop(statvis, "distort_min", text="Minimum")
                row.prop(statvis, "distort_max", text="Maximum")
            elif statvis_type == 'SHARP':
                row = col.row(align=True)
                row.prop(statvis, "sharp_min", text="Minimum")
                row.prop(statvis, "sharp_max", text="Maximum")


class VIEW3D_PT_overlay_edit_mesh_measurement(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay_edit_mesh'
    bl_label = "Measurement"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays
        data = context.active_object.data

        col = layout.column()
        col.active = display_all

        split = col.split()

        sub = split.column()
        sub.prop(data, "show_extra_edge_length", text="Edge Length")
        sub.prop(data, "show_extra_edge_angle", text="Edge Angle")

        sub = split.column()
        sub.prop(data, "show_extra_face_area", text="Face Area")
        sub.prop(data, "show_extra_face_angle", text="Face Angle")


class VIEW3D_PT_overlay_edit_mesh_normals(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay_edit_mesh'
    bl_label = "Normals"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        row = col.row(align=True)
        row.prop(overlay, "show_vertex_normals", text="", icon='VERTEXSEL')
        row.prop(overlay, "show_split_normals", text="", icon='LOOPSEL')
        row.prop(overlay, "show_face_normals", text="", icon='FACESEL')

        sub = row.row(align=True)
        sub.active = overlay.show_vertex_normals or overlay.show_face_normals or overlay.show_split_normals
        sub.prop(overlay, "normals_length", text="Size")


class VIEW3D_PT_overlay_edit_mesh_freestyle(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Freestyle"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH' and bpy.app.build_options.freestyle

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays
        data = context.active_object.data

        col = layout.column()
        col.active = display_all

        row = col.row()
        row.prop(data, "show_freestyle_edge_marks", text="Edge Marks")
        row.prop(data, "show_freestyle_face_marks", text="Face Marks")
        row.prop(data, "show_edge_seams", text="Seams")


class VIEW3D_PT_overlay_edit_mesh_developer(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay_edit_mesh'
    bl_label = "Developer"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH' and context.user_preferences.view.show_developer_ui

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays
        data = context.active_object.data

        col = layout.column()
        col.active = display_all

        col.prop(data, "show_extra_indices", text="Indices")


class VIEW3D_PT_overlay_edit_curve(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Curve Edit Mode"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_CURVE'

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        data = context.active_object.data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        row = col.row()
        row.prop(data, "show_handles", text="Handles")
        row.prop(data, "show_normal_face", text="Normals")


class VIEW3D_PT_overlay_sculpt(Panel):
    bl_space_type = 'VIEW_3D'
    bl_context = ".sculpt_mode"
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Sculpt"

    @classmethod
    def poll(cls, context):
        return (
            context.mode == 'SCULPT' and
            (context.sculpt_object and context.tool_settings.sculpt)
        )

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt

        layout.prop(sculpt, "show_diffuse_color")
        layout.prop(sculpt, "show_mask")


class VIEW3D_PT_overlay_pose(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Pose Mode"

    @classmethod
    def poll(cls, context):
        return context.mode == 'POSE'

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all
        col.prop(overlay, "show_transparent_bones")

        row = col.row()
        row.prop(overlay, "show_bone_select", text="")
        sub = row.row()
        sub.active = display_all and overlay.show_bone_select
        sub.prop(overlay, "bone_select_alpha", text="Fade Geometry")


class VIEW3D_PT_overlay_edit_armature(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Armature Edit Mode"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE'

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all
        col.prop(overlay, "show_transparent_bones")


class VIEW3D_PT_overlay_paint(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = ""

    @classmethod
    def poll(cls, context):
        return context.mode in {'PAINT_WEIGHT', 'PAINT_VERTEX', 'PAINT_TEXTURE'}

    def draw_header(self, context):
        layout = self.layout
        layout.label(text={
            'PAINT_TEXTURE': "Texture Paint",
            'PAINT_VERTEX': "Vertex Paint",
            'PAINT_WEIGHT': "Weight Paint",
        }[context.mode])

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        col.prop(overlay, {
            'PAINT_TEXTURE': "texture_paint_mode_opacity",
            'PAINT_VERTEX': "vertex_paint_mode_opacity",
            'PAINT_WEIGHT': "weight_paint_mode_opacity",
        }[context.mode], text="Opacity")

        if context.mode in {'PAINT_WEIGHT', 'PAINT_VERTEX'}:
            col.prop(overlay, "show_paint_wire")


class VIEW3D_PT_pivot_point(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Pivot Point"
    bl_ui_units_x = 8

    def draw(self, context):
        toolsettings = context.tool_settings
        obj = context.active_object
        mode = context.mode

        layout = self.layout
        col = layout.column()
        col.label("Pivot Point")
        col.prop(toolsettings, "transform_pivot_point", expand=True)

        if (obj is None) or (mode in {'OBJECT', 'POSE', 'WEIGHT_PAINT'}):
            col.separator()

            col.prop(
                toolsettings,
                "use_transform_pivot_point_align",
                text="Center Points Only",
            )


class VIEW3D_PT_snapping(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Snapping"

    def draw(self, context):
        toolsettings = context.tool_settings
        snap_elements = toolsettings.snap_elements
        obj = context.active_object
        mode = context.mode
        object_mode = 'OBJECT' if obj is None else obj.mode

        layout = self.layout
        col = layout.column()
        col.label("Snapping")
        col.prop(toolsettings, "snap_elements", expand=True)

        col.separator()
        if 'INCREMENT' in snap_elements:
            col.prop(toolsettings, "use_snap_grid_absolute")

        if snap_elements != {'INCREMENT'}:
            col.label("Target")
            row = col.row(align=True)
            row.prop(toolsettings, "snap_target", expand=True)

            if obj:
                if object_mode == 'EDIT':
                    col.prop(toolsettings, "use_snap_self")
                if object_mode in {'OBJECT', 'POSE', 'EDIT'}:
                    col.prop(toolsettings, "use_snap_align_rotation", text="Align Rotation")

            if 'FACE' in snap_elements:
                col.prop(toolsettings, "use_snap_project", text="Project Elements")

            if 'VOLUME' in snap_elements:
                col.prop(toolsettings, "use_snap_peel_object")


class VIEW3D_PT_transform_orientations(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Transform Orientations"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout
        layout.label("Transform Orientations")

        scene = context.scene
        orientation = scene.current_orientation

        row = layout.row()
        col = row.column()
        col.prop(scene, "transform_orientation", expand=True)
        row.operator("transform.create_orientation", text="", icon='ZOOMIN', emboss=False).use = True

        if orientation:
            row = layout.row(align=False)
            row.prop(orientation, "name", text="")
            row.operator("transform.delete_orientation", text="", icon='X', emboss=False)


class VIEW3D_PT_overlay_gpencil_options(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = ""

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'GPENCIL'

    def draw_header(self, context):
        layout = self.layout
        layout.label(text={
            'GPENCIL_PAINT': "Draw Grease Pencil",
            'GPENCIL_EDIT': "Edit Grease Pencil",
            'GPENCIL_SCULPT': "Sculpt Grease Pencil",
            'GPENCIL_WEIGHT': "Weight Grease Pencil",
            'OBJECT': "Grease Pencil",
        }[context.mode])

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay

        layout.prop(overlay, "use_gpencil_onion_skin", text="Onion Skin")

        col = layout.column()
        row = col.row()
        row.prop(overlay, "use_gpencil_paper", text="")
        sub = row.row()
        sub.active = overlay.use_gpencil_paper
        sub.prop(overlay, "gpencil_paper_opacity", text="Fade 3D Objects", slider=True)

        col = layout.column()
        row = col.row()
        row.prop(overlay, "use_gpencil_grid", text="")
        sub = row.row()
        sub.active = overlay.use_gpencil_grid
        sub.prop(overlay, "gpencil_grid_opacity", text="Canvas Grid", slider=True)

        if overlay.use_gpencil_grid:
            row = layout.row(align=True)
            row.prop(overlay, "gpencil_grid_scale")
            col = row.column()
            col.prop(overlay, "gpencil_grid_lines", text="Subdivisions")
            col.prop(overlay, "gpencil_grid_axis")

        if context.object.mode in {'GPENCIL_EDIT', 'GPENCIL_SCULPT', 'GPENCIL_WEIGHT'}:
            layout.prop(overlay, "use_gpencil_edit_lines", text="Edit Lines")
            layout.prop(overlay, "use_gpencil_multiedit_line_only", text="Show Edit Lines only in multiframe")
            layout.prop(overlay, "vertex_opacity", text="Vertex Opacity", slider=True)


class VIEW3D_PT_quad_view(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Quad View"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return view.region_quadviews

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        region = view.region_quadviews[2]
        col = layout.column()
        col.prop(region, "lock_rotation")
        row = col.row()
        row.enabled = region.lock_rotation
        row.prop(region, "show_sync_view")
        row = col.row()
        row.enabled = region.lock_rotation and region.show_sync_view
        row.prop(region, "use_box_clip")


# Annotation properties
class VIEW3D_PT_grease_pencil(GreasePencilDataPanel, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'

    # NOTE: this is just a wrapper around the generic GP Panel


class VIEW3D_PT_view3d_stereo(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Stereoscopy"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        scene = context.scene

        multiview = scene.render.use_multiview
        return multiview

    def draw(self, context):
        layout = self.layout
        view = context.space_data

        basic_stereo = context.scene.render.views_format == 'STEREO_3D'

        col = layout.column()
        col.row().prop(view, "stereo_3d_camera", expand=True)

        col.label(text="Display:")
        row = col.row()
        row.active = basic_stereo
        row.prop(view, "show_stereo_3d_cameras")
        row = col.row()
        row.active = basic_stereo
        split = row.split()
        split.prop(view, "show_stereo_3d_convergence_plane")
        split = row.split()
        split.prop(view, "stereo_3d_convergence_plane_alpha", text="Alpha")
        split.active = view.show_stereo_3d_convergence_plane
        row = col.row()
        split = row.split()
        split.prop(view, "show_stereo_3d_volume")
        split = row.split()
        split.prop(view, "stereo_3d_volume_alpha", text="Alpha")


class VIEW3D_PT_context_properties(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Properties"
    bl_options = {'DEFAULT_CLOSED'}

    def _active_context_member(context):
        obj = context.object
        if obj:
            object_mode = obj.mode
            if object_mode == 'POSE':
                return "active_pose_bone"
            elif object_mode == 'EDIT' and obj.type == 'ARMATURE':
                return "active_bone"
            else:
                return "object"

        return ""

    @classmethod
    def poll(cls, context):
        import rna_prop_ui
        member = cls._active_context_member(context)

        if member:
            context_member, member = rna_prop_ui.rna_idprop_context_value(context, member, object)
            return context_member and rna_prop_ui.rna_idprop_has_properties(context_member)

        return False

    def draw(self, context):
        import rna_prop_ui
        member = VIEW3D_PT_context_properties._active_context_member(context)

        if member:
            # Draw with no edit button
            rna_prop_ui.draw(self.layout, context, member, object, False)


# Grease Pencil Object - Multiframe falloff tools
class VIEW3D_PT_gpencil_multi_frame(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Multi Frame"

    @staticmethod
    def draw(self, context):
        gpd = context.gpencil_data
        settings = context.tool_settings.gpencil_sculpt

        layout = self.layout
        col = layout.column(align=True)
        col.prop(settings, "use_multiframe_falloff")

        # Falloff curve
        if gpd.use_multiedit and settings.use_multiframe_falloff:
            layout.template_curve_mapping(settings, "multiframe_falloff_curve", brush=True)


class VIEW3D_MT_gpencil_edit_specials(Menu):
    bl_label = "Grease Pencil Specials"

    def draw(self, context):
        layout = self.layout
        is_3d_view = context.space_data.type == 'VIEW_3D'

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.menu("VIEW3D_MT_assign_material")
        layout.separator()

        layout.operator("gpencil.stroke_subdivide", text="Subdivide")
        layout.operator("gpencil.stroke_simplify_fixed", text="Simplify")
        layout.operator("gpencil.stroke_simplify", text="Simplify Adaptative")

        layout.separator()
        layout.menu("GPENCIL_MT_separate", text="Separate")

        layout.separator()
        layout.operator("gpencil.stroke_split", text="Split")

        layout.separator()

        layout.operator("gpencil.stroke_join", text="Join").type = 'JOIN'
        layout.operator("gpencil.stroke_join", text="Join & Copy").type = 'JOINCOPY'
        layout.operator("gpencil.stroke_flip", text="Flip Direction")

        layout.separator()
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame")
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame All Layers").mode = 'ALL'

        if is_3d_view:
            layout.separator()
            layout.menu("GPENCIL_MT_cleanup")


class VIEW3D_MT_gpencil_sculpt_specials(Menu):
    bl_label = "Grease Pencil Specials"

    def draw(self, context):
        layout = self.layout
        is_3d_view = context.space_data.type == 'VIEW_3D'

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.menu("VIEW3D_MT_assign_material")
        layout.separator()

        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame")
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame All Layers").mode = 'ALL'

        layout.separator()

        layout.operator("gpencil.stroke_subdivide", text="Subdivide")
        layout.operator("gpencil.stroke_simplify_fixed", text="Simplify")
        layout.operator("gpencil.stroke_simplify", text="Simplify Adaptative")


classes = (
    VIEW3D_HT_header,
    VIEW3D_MT_editor_menus,
    VIEW3D_MT_transform,
    VIEW3D_MT_transform_base,
    VIEW3D_MT_transform_object,
    VIEW3D_MT_transform_armature,
    VIEW3D_MT_mirror,
    VIEW3D_MT_snap,
    VIEW3D_MT_uv_map,
    VIEW3D_MT_edit_proportional,
    VIEW3D_MT_view,
    VIEW3D_MT_view_cameras,
    VIEW3D_MT_view_navigation,
    VIEW3D_MT_view_align,
    VIEW3D_MT_view_align_selected,
    VIEW3D_MT_view_viewpoint,
    VIEW3D_MT_view_borders,
    VIEW3D_MT_select_object,
    VIEW3D_MT_select_object_more_less,
    VIEW3D_MT_select_pose,
    VIEW3D_MT_select_pose_more_less,
    VIEW3D_MT_select_particle,
    VIEW3D_MT_edit_mesh,
    VIEW3D_MT_edit_mesh_select_similar,
    VIEW3D_MT_edit_mesh_select_by_trait,
    VIEW3D_MT_edit_mesh_select_more_less,
    VIEW3D_MT_select_edit_mesh,
    VIEW3D_MT_select_edit_curve,
    VIEW3D_MT_select_edit_surface,
    VIEW3D_MT_select_edit_text,
    VIEW3D_MT_select_edit_metaball,
    VIEW3D_MT_select_edit_lattice,
    VIEW3D_MT_select_edit_armature,
    VIEW3D_MT_select_gpencil,
    VIEW3D_MT_select_paint_mask,
    VIEW3D_MT_select_paint_mask_vertex,
    VIEW3D_MT_angle_control,
    INFO_MT_mesh_add,
    INFO_MT_curve_add,
    INFO_MT_surface_add,
    INFO_MT_metaball_add,
    INFO_MT_edit_curve_add,
    INFO_MT_edit_armature_add,
    INFO_MT_armature_add,
    INFO_MT_light_add,
    INFO_MT_lightprobe_add,
    INFO_MT_camera_add,
    INFO_MT_add,
    VIEW3D_MT_object,
    VIEW3D_MT_object_animation,
    VIEW3D_MT_object_rigid_body,
    VIEW3D_MT_object_clear,
    VIEW3D_MT_object_specials,
    VIEW3D_MT_object_shading,
    VIEW3D_MT_object_apply,
    VIEW3D_MT_object_relations,
    VIEW3D_MT_object_parent,
    VIEW3D_MT_object_track,
    VIEW3D_MT_object_collection,
    VIEW3D_MT_object_constraints,
    VIEW3D_MT_object_quick_effects,
    VIEW3D_MT_object_showhide,
    VIEW3D_MT_make_single_user,
    VIEW3D_MT_make_links,
    VIEW3D_MT_brush,
    VIEW3D_MT_brush_paint_modes,
    VIEW3D_MT_paint_vertex,
    VIEW3D_MT_hook,
    VIEW3D_MT_vertex_group,
    VIEW3D_MT_paint_weight,
    VIEW3D_MT_sculpt,
    VIEW3D_MT_hide_mask,
    VIEW3D_MT_particle,
    VIEW3D_MT_particle_specials,
    VIEW3D_MT_particle_showhide,
    VIEW3D_MT_pose,
    VIEW3D_MT_pose_transform,
    VIEW3D_MT_pose_slide,
    VIEW3D_MT_pose_propagate,
    VIEW3D_MT_pose_library,
    VIEW3D_MT_pose_motion,
    VIEW3D_MT_pose_group,
    VIEW3D_MT_pose_ik,
    VIEW3D_MT_pose_constraints,
    VIEW3D_MT_pose_showhide,
    VIEW3D_MT_pose_apply,
    VIEW3D_MT_pose_specials,
    VIEW3D_MT_bone_options_toggle,
    VIEW3D_MT_bone_options_enable,
    VIEW3D_MT_bone_options_disable,
    VIEW3D_MT_edit_mesh_specials,
    VIEW3D_MT_edit_mesh_select_mode,
    VIEW3D_MT_edit_mesh_select_linked,
    VIEW3D_MT_edit_mesh_select_loops,
    VIEW3D_MT_edit_mesh_extrude,
    VIEW3D_MT_edit_mesh_vertices,
    VIEW3D_MT_edit_mesh_edges,
    VIEW3D_MT_edit_mesh_edges_data,
    VIEW3D_MT_edit_mesh_faces,
    VIEW3D_MT_edit_mesh_faces_data,
    VIEW3D_MT_edit_mesh_normals,
    VIEW3D_MT_edit_mesh_shading,
    VIEW3D_MT_edit_mesh_weights,
    VIEW3D_MT_edit_mesh_clean,
    VIEW3D_MT_edit_mesh_delete,
    VIEW3D_MT_edit_mesh_showhide,
    VIEW3D_MT_paint_gpencil,
    VIEW3D_MT_assign_material,
    VIEW3D_MT_edit_gpencil,
    VIEW3D_MT_edit_gpencil_delete,
    VIEW3D_MT_sculpt_gpencil,
    VIEW3D_MT_weight_gpencil,
    VIEW3D_MT_gpencil_animation,
    VIEW3D_MT_gpencil_simplify,
    VIEW3D_MT_edit_curve,
    VIEW3D_MT_edit_curve_ctrlpoints,
    VIEW3D_MT_edit_curve_segments,
    VIEW3D_MT_edit_curve_clean,
    VIEW3D_MT_edit_curve_specials,
    VIEW3D_MT_edit_curve_delete,
    VIEW3D_MT_edit_curve_showhide,
    VIEW3D_MT_edit_surface,
    VIEW3D_MT_edit_font,
    VIEW3D_MT_edit_text_chars,
    VIEW3D_MT_edit_meta,
    VIEW3D_MT_edit_meta_showhide,
    VIEW3D_MT_edit_lattice,
    VIEW3D_MT_edit_armature,
    VIEW3D_MT_armature_specials,
    VIEW3D_MT_edit_armature_parent,
    VIEW3D_MT_edit_armature_roll,
    VIEW3D_MT_edit_armature_delete,
    VIEW3D_MT_edit_gpencil_transform,
    VIEW3D_MT_edit_gpencil_interpolate,
    VIEW3D_MT_object_mode_pie,
    VIEW3D_MT_view_pie,
    VIEW3D_PT_view3d_properties,
    VIEW3D_PT_view3d_camera_lock,
    VIEW3D_PT_view3d_cursor,
    VIEW3D_PT_object_type_visibility,
    VIEW3D_PT_grease_pencil,
    VIEW3D_PT_gpencil_multi_frame,
    VIEW3D_MT_gpencil_edit_specials,
    VIEW3D_MT_gpencil_sculpt_specials,
    VIEW3D_PT_quad_view,
    VIEW3D_PT_view3d_stereo,
    VIEW3D_PT_shading,
    VIEW3D_PT_shading_lighting,
    VIEW3D_PT_shading_color,
    VIEW3D_PT_shading_options,
    VIEW3D_PT_shading_options_shadow,
    VIEW3D_PT_shading_options_ssao,
    VIEW3D_PT_overlay,
    VIEW3D_PT_overlay_gizmo,
    VIEW3D_PT_overlay_guides,
    VIEW3D_PT_overlay_object,
    VIEW3D_PT_overlay_geometry,
    VIEW3D_PT_overlay_motion_tracking,
    VIEW3D_PT_overlay_edit_mesh,
    VIEW3D_PT_overlay_edit_mesh_shading,
    VIEW3D_PT_overlay_edit_mesh_measurement,
    VIEW3D_PT_overlay_edit_mesh_normals,
    VIEW3D_PT_overlay_edit_mesh_freestyle,
    VIEW3D_PT_overlay_edit_mesh_developer,
    VIEW3D_PT_overlay_edit_curve,
    VIEW3D_PT_overlay_edit_armature,
    VIEW3D_PT_overlay_pose,
    VIEW3D_PT_overlay_paint,
    VIEW3D_PT_overlay_sculpt,
    VIEW3D_PT_pivot_point,
    VIEW3D_PT_snapping,
    VIEW3D_PT_transform_orientations,
    VIEW3D_PT_overlay_gpencil_options,
    VIEW3D_PT_context_properties,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
