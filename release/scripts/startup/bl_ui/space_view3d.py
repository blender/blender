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
from bl_ui.properties_paint_common import UnifiedPaintPanel
from bpy.app.translations import contexts as i18n_contexts


class VIEW3D_HT_header(Header):
    bl_space_type = 'VIEW_3D'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        # mode_string = context.mode
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.template_header()
        sub = row.row(align=True)

        VIEW3D_MT_editor_menus.draw_collapsible(context, layout)

        # Contains buttons like Mode, Pivot, Manipulator, Layer, Mesh Select Mode...
        row = layout
        layout.template_header_3D()

        if obj:
            mode = obj.mode
            # Particle edit
            if mode == 'PARTICLE_EDIT':
                row.prop(toolsettings.particle_edit, "select_mode", text="", expand=True)

            # Occlude geometry
            if ((view.viewport_shade not in {'BOUNDBOX', 'WIREFRAME'} and (mode == 'PARTICLE_EDIT' or (mode == 'EDIT' and obj.type == 'MESH'))) or
                    (mode == 'WEIGHT_PAINT')):
                row.prop(view, "use_occlude_geometry", text="")

            # Proportional editing
            if mode in {'EDIT', 'PARTICLE_EDIT'}:
                row = layout.row(align=True)
                row.prop(toolsettings, "proportional_edit", icon_only=True)
                if toolsettings.proportional_edit != 'DISABLED':
                    row.prop(toolsettings, "proportional_edit_falloff", icon_only=True)
            elif mode == 'OBJECT':
                row = layout.row(align=True)
                row.prop(toolsettings, "use_proportional_edit_objects", icon_only=True)
                if toolsettings.use_proportional_edit_objects:
                    row.prop(toolsettings, "proportional_edit_falloff", icon_only=True)

        # Snap
        if not obj or mode not in {'SCULPT', 'VERTEX_PAINT', 'WEIGHT_PAINT', 'TEXTURE_PAINT'}:
            snap_element = toolsettings.snap_element
            row = layout.row(align=True)
            row.prop(toolsettings, "use_snap", text="")
            row.prop(toolsettings, "snap_element", icon_only=True)
            if snap_element != 'INCREMENT':
                row.prop(toolsettings, "snap_target", text="")
                if obj:
                    if mode in {'OBJECT', 'POSE'} and snap_element != 'VOLUME':
                        row.prop(toolsettings, "use_snap_align_rotation", text="")
                    elif mode == 'EDIT':
                        row.prop(toolsettings, "use_snap_self", text="")

            if snap_element == 'VOLUME':
                row.prop(toolsettings, "use_snap_peel_object", text="")
            elif snap_element == 'FACE':
                row.prop(toolsettings, "use_snap_project", text="")

        # AutoMerge editing
        if obj:
            if (mode == 'EDIT' and obj.type == 'MESH'):
                layout.prop(toolsettings, "use_mesh_automerge", text="", icon='AUTOMERGE_ON')

        # OpenGL render
        row = layout.row(align=True)
        row.operator("render.opengl", text="", icon='RENDER_STILL')
        row.operator("render.opengl", text="", icon='RENDER_ANIMATION').animation = True

        # Pose
        if obj and mode == 'POSE':
            row = layout.row(align=True)
            row.operator("pose.copy", text="", icon='COPYDOWN')
            row.operator("pose.paste", text="", icon='PASTEDOWN')
            row.operator("pose.paste", text="", icon='PASTEFLIPDOWN').flipped = 1


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

        layout.menu("VIEW3D_MT_view")

        # Select Menu
        if mode_string in {'PAINT_WEIGHT', 'PAINT_VERTEX', 'PAINT_TEXTURE'}:
            mesh = obj.data
            if mesh.use_paint_mask:
                layout.menu("VIEW3D_MT_select_paint_mask")
            elif mesh.use_paint_mask_vertex and mode_string == 'PAINT_WEIGHT':
                layout.menu("VIEW3D_MT_select_paint_mask_vertex")
        elif mode_string != 'SCULPT':
            layout.menu("VIEW3D_MT_select_%s" % mode_string.lower())

        if mode_string == 'OBJECT':
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

        if edit_object:
            layout.menu("VIEW3D_MT_edit_%s" % edit_object.type.lower())
        elif obj:
            if mode_string not in {'PAINT_TEXTURE'}:
                layout.menu("VIEW3D_MT_%s" % mode_string.lower())
            if mode_string in {'SCULPT', 'PAINT_VERTEX', 'PAINT_WEIGHT', 'PAINT_TEXTURE'}:
                layout.menu("VIEW3D_MT_brush")
            if mode_string == 'SCULPT':
                layout.menu("VIEW3D_MT_hide_mask")
        else:
            layout.menu("VIEW3D_MT_object")


# ********** Menu **********


# ********** Utilities **********


class ShowHideMenu():
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

        layout.operator("transform.translate", text="Grab/Move")
        # TODO: sub-menu for grab per axis
        layout.operator("transform.rotate", text="Rotate")
        # TODO: sub-menu for rot per axis
        layout.operator("transform.resize", text="Scale")
        # TODO: sub-menu for scale per axis

        layout.separator()

        layout.operator("transform.tosphere", text="To Sphere")
        layout.operator("transform.shear", text="Shear")
        layout.operator("transform.bend", text="Bend")
        layout.operator("transform.push_pull", text="Push/Pull")
        layout.operator("object.vertex_warp", text="Warp")
        layout.operator("object.vertex_random", text="Randomize")


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
        layout.operator("transform.transform", text="Align to Transform Orientation").mode = 'ALIGN'  # XXX see alignmenu() in edit.c of b2.4x to get this working

        layout.separator()

        layout.operator_context = 'EXEC_AREA'

        layout.operator("object.origin_set", text="Geometry to Origin").type = 'GEOMETRY_ORIGIN'
        layout.operator("object.origin_set", text="Origin to Geometry").type = 'ORIGIN_GEOMETRY'
        layout.operator("object.origin_set", text="Origin to 3D Cursor").type = 'ORIGIN_CURSOR'
        layout.operator("object.origin_set", text="Origin to Center of Mass").type = 'ORIGIN_CENTER_OF_MASS'
        layout.separator()

        layout.operator("object.randomize_transform")
        layout.operator("object.align")

        layout.separator()

        layout.operator("object.anim_transforms_to_deltas")


# Armature EditMode extensions to Transform menu
class VIEW3D_MT_transform_armature(VIEW3D_MT_transform_base):
    def draw(self, context):
        layout = self.layout

        # base menu
        VIEW3D_MT_transform_base.draw(self, context)

        # armature specific extensions follow...
        layout.separator()

        obj = context.object
        if obj.type == 'ARMATURE' and obj.mode in {'EDIT', 'POSE'}:
            if obj.data.draw_type == 'BBONE':
                layout.operator("transform.transform", text="Scale BBone").mode = 'BONE_SIZE'
            elif obj.data.draw_type == 'ENVELOPE':
                layout.operator("transform.transform", text="Scale Envelope Distance").mode = 'BONE_SIZE'
                layout.operator("transform.transform", text="Scale Radius").mode = 'BONE_ENVELOPE'

        if context.edit_object and context.edit_object.type == 'ARMATURE':
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
        layout.operator("view3d.snap_selected_to_cursor", text="Selection to Cursor (Offset)").use_offset = True

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


# ********** View menus **********


class VIEW3D_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.properties", icon='MENU_PANEL')
        layout.operator("view3d.toolshelf", icon='MENU_PANEL')

        layout.separator()

        layout.operator("view3d.viewnumpad", text="Camera").type = 'CAMERA'
        layout.operator("view3d.viewnumpad", text="Top").type = 'TOP'
        layout.operator("view3d.viewnumpad", text="Bottom").type = 'BOTTOM'
        layout.operator("view3d.viewnumpad", text="Front").type = 'FRONT'
        layout.operator("view3d.viewnumpad", text="Back").type = 'BACK'
        layout.operator("view3d.viewnumpad", text="Right").type = 'RIGHT'
        layout.operator("view3d.viewnumpad", text="Left").type = 'LEFT'

        layout.menu("VIEW3D_MT_view_cameras", text="Cameras")

        layout.separator()

        layout.operator("view3d.view_persportho")

        layout.separator()

        layout.menu("VIEW3D_MT_view_navigation")
        layout.menu("VIEW3D_MT_view_align")

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("view3d.clip_border", text="Clipping Border...")
        layout.operator("view3d.zoom_border", text="Zoom Border...")
        layout.operator("view3d.render_border", text="Render Border...")

        layout.separator()

        layout.operator("view3d.layers", text="Show All Layers").nr = 0

        layout.separator()

        layout.operator("view3d.localview", text="View Global/Local")
        layout.operator("view3d.view_selected")
        layout.operator("view3d.view_all")

        layout.separator()

        layout.operator("screen.animation_play", text="Playback Animation")

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.region_quadview")
        layout.operator("screen.screen_full_area")


class VIEW3D_MT_view_navigation(Menu):
    bl_label = "Navigation"

    def draw(self, context):
        from math import pi
        layout = self.layout

        layout.operator_enum("view3d.view_orbit", "type")

        layout.separator()

        layout.operator("view3d.view_roll", text="Roll Left").angle = pi / -12.0
        layout.operator("view3d.view_roll", text="Roll Right").angle = pi / 12.0

        layout.separator()

        layout.operator_enum("view3d.view_pan", "type")

        layout.separator()

        layout.operator("view3d.zoom", text="Zoom In").delta = 1
        layout.operator("view3d.zoom", text="Zoom Out").delta = -1
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

        layout.operator("view3d.view_all", text="Center Cursor and View All").center = True
        layout.operator("view3d.camera_to_view", text="Align Active Camera to View")
        layout.operator("view3d.camera_to_view_selected", text="Align Active Camera to Selected")
        layout.operator("view3d.view_selected")
        layout.operator("view3d.view_center_cursor")

        layout.separator()

        layout.operator("view3d.view_lock_to_active")
        layout.operator("view3d.view_lock_clear")


class VIEW3D_MT_view_align_selected(Menu):
    bl_label = "Align View to Active"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("view3d.viewnumpad", text="Top")
        props.align_active = True
        props.type = 'TOP'

        props = layout.operator("view3d.viewnumpad", text="Bottom")
        props.align_active = True
        props.type = 'BOTTOM'

        props = layout.operator("view3d.viewnumpad", text="Front")
        props.align_active = True
        props.type = 'FRONT'

        props = layout.operator("view3d.viewnumpad", text="Back")
        props.align_active = True
        props.type = 'BACK'

        props = layout.operator("view3d.viewnumpad", text="Right")
        props.align_active = True
        props.type = 'RIGHT'

        props = layout.operator("view3d.viewnumpad", text="Left")
        props.align_active = True
        props.type = 'LEFT'


class VIEW3D_MT_view_cameras(Menu):
    bl_label = "Cameras"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.object_as_camera")
        layout.operator("view3d.viewnumpad", text="Active Camera").type = 'CAMERA'

# ********** Select menus, suffix from context.mode **********


class VIEW3D_MT_select_object(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("object.select_all").action = 'TOGGLE'
        layout.operator("object.select_all", text="Inverse").action = 'INVERT'
        layout.operator("object.select_random", text="Random")
        layout.operator("object.select_mirror", text="Mirror")
        layout.operator("object.select_by_layer", text="Select All by Layer")
        layout.operator_menu_enum("object.select_by_type", "type", text="Select All by Type...")
        layout.operator("object.select_camera", text="Select Camera")

        layout.separator()

        layout.operator_menu_enum("object.select_grouped", "type", text="Grouped")
        layout.operator_menu_enum("object.select_linked", "type", text="Linked")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_pose(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("pose.select_all").action = 'TOGGLE'
        layout.operator("pose.select_all", text="Inverse").action = 'INVERT'
        layout.operator("pose.select_mirror", text="Flip Active")
        layout.operator("pose.select_constraint_target", text="Constraint Target")
        layout.operator("pose.select_linked", text="Linked")

        layout.separator()

        layout.operator("pose.select_hierarchy", text="Parent").direction = 'PARENT'
        layout.operator("pose.select_hierarchy", text="Child").direction = 'CHILD'

        layout.separator()

        props = layout.operator("pose.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("pose.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'

        layout.separator()

        layout.operator_menu_enum("pose.select_grouped", "type", text="Grouped")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_particle(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("particle.select_all").action = 'TOGGLE'
        layout.operator("particle.select_linked")
        layout.operator("particle.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("particle.select_more")
        layout.operator("particle.select_less")

        layout.separator()

        layout.operator("particle.select_roots", text="Roots")
        layout.operator("particle.select_tips", text="Tips")


class VIEW3D_MT_select_edit_mesh(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        # primitive
        layout.operator("mesh.select_all").action = 'TOGGLE'
        layout.operator("mesh.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        # numeric
        layout.operator("mesh.select_random", text="Random")
        layout.operator("mesh.select_nth")

        layout.separator()

        # geometric
        layout.operator("mesh.edges_select_sharp", text="Sharp Edges")
        layout.operator("mesh.faces_select_linked_flat", text="Linked Flat Faces")

        layout.separator()

        # topology
        layout.operator("mesh.select_loose", text="Loose Geometry")
        if context.scene.tool_settings.mesh_select_mode[2] is False:
            layout.operator("mesh.select_non_manifold", text="Non Manifold")
        layout.operator("mesh.select_interior_faces", text="Interior Faces")
        layout.operator("mesh.select_face_by_sides")

        layout.separator()

        # other ...
        layout.operator_menu_enum("mesh.select_similar", "type", text="Similar")
        layout.operator("mesh.select_ungrouped", text="Ungrouped Verts")

        layout.separator()

        layout.operator("mesh.select_more", text="More")
        layout.operator("mesh.select_less", text="Less")

        layout.separator()

        layout.operator("mesh.select_mirror", text="Mirror")
        layout.operator("mesh.select_axis", text="Side of Active")

        layout.operator("mesh.select_linked", text="Linked")
        layout.operator("mesh.shortest_path_select", text="Shortest Path")
        layout.operator("mesh.loop_multi_select", text="Edge Loops").ring = False
        layout.operator("mesh.loop_multi_select", text="Edge Rings").ring = True

        layout.separator()

        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_select_edit_curve(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_all").action = 'TOGGLE'
        layout.operator("curve.select_all", text="Inverse").action = 'INVERT'
        layout.operator("curve.select_random")
        layout.operator("curve.select_nth")
        layout.operator("curve.select_linked", text="Select Linked")

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

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_all").action = 'TOGGLE'
        layout.operator("curve.select_all", text="Inverse").action = 'INVERT'
        layout.operator("curve.select_random")
        layout.operator("curve.select_nth")
        layout.operator("curve.select_linked", text="Select Linked")

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

        layout.operator("font.text_copy", text="Copy")
        layout.operator("font.text_cut", text="Cut")
        layout.operator("font.text_paste", text="Paste")

        layout.separator()

        layout.operator("font.text_paste_from_file")
        layout.operator("font.text_paste_from_clipboard")

        layout.separator()

        layout.operator("font.select_all")


class VIEW3D_MT_select_edit_metaball(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("mball.select_all").action = 'TOGGLE'
        layout.operator("mball.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("mball.select_random_metaelems")

        layout.separator()

        layout.operator_menu_enum("mball.select_similar", "type", text="Similar")


class VIEW3D_MT_select_edit_lattice(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("lattice.select_mirror")
        layout.operator("lattice.select_random")
        layout.operator("lattice.select_all").action = 'TOGGLE'
        layout.operator("lattice.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("lattice.select_ungrouped", text="Ungrouped Verts")


class VIEW3D_MT_select_edit_armature(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("armature.select_all").action = 'TOGGLE'
        layout.operator("armature.select_all", text="Inverse").action = 'INVERT'
        layout.operator("armature.select_mirror", text="Mirror").extend = False

        layout.separator()

        layout.operator("armature.select_more", text="More")
        layout.operator("armature.select_less", text="Less")

        layout.separator()

        layout.operator("armature.select_hierarchy", text="Parent").direction = 'PARENT'
        layout.operator("armature.select_hierarchy", text="Child").direction = 'CHILD'

        layout.separator()

        props = layout.operator("armature.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("armature.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'

        layout.operator_menu_enum("armature.select_similar", "type", text="Similar")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_paint_mask(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("paint.face_select_all").action = 'TOGGLE'
        layout.operator("paint.face_select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("paint.face_select_linked", text="Linked")


class VIEW3D_MT_select_paint_mask_vertex(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("paint.vert_select_all").action = 'TOGGLE'
        layout.operator("paint.vert_select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("paint.vert_select_ungrouped", text="Ungrouped Verts")

# ********** Add menu **********

# XXX: INFO_MT_ names used to keep backwards compatibility (Addons etc that hook into the menu)


class INFO_MT_mesh_add(Menu):
    bl_idname = "INFO_MT_mesh_add"
    bl_label = "Mesh"

    def draw(self, context):
        from .space_view3d_toolbar import VIEW3D_PT_tools_add_object

        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        VIEW3D_PT_tools_add_object.draw_add_mesh(layout)


class INFO_MT_curve_add(Menu):
    bl_idname = "INFO_MT_curve_add"
    bl_label = "Curve"

    def draw(self, context):
        from .space_view3d_toolbar import VIEW3D_PT_tools_add_object
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        VIEW3D_PT_tools_add_object.draw_add_curve(layout)


class INFO_MT_surface_add(Menu):
    bl_idname = "INFO_MT_surface_add"
    bl_label = "Surface"

    def draw(self, context):
        from .space_view3d_toolbar import VIEW3D_PT_tools_add_object
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        VIEW3D_PT_tools_add_object.draw_add_surface(layout)


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


class INFO_MT_add(Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        # note, don't use 'EXEC_SCREEN' or operators wont get the 'v3d' context.

        # Note: was EXEC_AREA, but this context does not have the 'rv3d', which prevents
        #       "align_view" to work on first call (see [#32719]).
        layout.operator_context = 'EXEC_REGION_WIN'

        #layout.operator_menu_enum("object.mesh_add", "type", text="Mesh", icon='OUTLINER_OB_MESH')
        layout.menu("INFO_MT_mesh_add", icon='OUTLINER_OB_MESH')

        #layout.operator_menu_enum("object.curve_add", "type", text="Curve", icon='OUTLINER_OB_CURVE')
        layout.menu("INFO_MT_curve_add", icon='OUTLINER_OB_CURVE')
        #layout.operator_menu_enum("object.surface_add", "type", text="Surface", icon='OUTLINER_OB_SURFACE')
        layout.menu("INFO_MT_surface_add", icon='OUTLINER_OB_SURFACE')
        layout.menu("INFO_MT_metaball_add", text="Metaball", icon='OUTLINER_OB_META')
        layout.operator("object.text_add", text="Text", icon='OUTLINER_OB_FONT')
        layout.separator()

        layout.menu("INFO_MT_armature_add", icon='OUTLINER_OB_ARMATURE')
        layout.operator("object.add", text="Lattice", icon='OUTLINER_OB_LATTICE').type = 'LATTICE'
        layout.operator_menu_enum("object.empty_add", "type", text="Empty", icon='OUTLINER_OB_EMPTY')
        layout.separator()

        layout.operator("object.speaker_add", text="Speaker", icon='OUTLINER_OB_SPEAKER')
        layout.separator()

        layout.operator("object.camera_add", text="Camera", icon='OUTLINER_OB_CAMERA')
        layout.operator_menu_enum("object.lamp_add", "type", text="Lamp", icon='OUTLINER_OB_LAMP')
        layout.separator()

        layout.operator_menu_enum("object.effector_add", "type", text="Force Field", icon='OUTLINER_OB_EMPTY')
        layout.separator()

        if len(bpy.data.groups) > 10:
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("object.group_instance_add", text="Group Instance...", icon='OUTLINER_OB_EMPTY')
        else:
            layout.operator_menu_enum("object.group_instance_add", "group", text="Group Instance", icon='OUTLINER_OB_EMPTY')


# ********** Object menu **********


class VIEW3D_MT_object(Menu):
    bl_context = "objectmode"
    bl_label = "Object"

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")
        layout.operator("ed.undo_history")

        layout.separator()

        layout.menu("VIEW3D_MT_transform_object")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_object_clear")
        layout.menu("VIEW3D_MT_object_apply")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.menu("VIEW3D_MT_object_animation")

        layout.separator()

        layout.operator("object.duplicate_move")
        layout.operator("object.duplicate_move_linked")
        layout.operator("object.delete", text="Delete...")
        layout.operator("object.proxy_make", text="Make Proxy...")
        layout.menu("VIEW3D_MT_make_links", text="Make Links...")
        layout.operator("object.make_dupli_face")
        layout.operator_menu_enum("object.make_local", "type", text="Make Local...")
        layout.menu("VIEW3D_MT_make_single_user")

        layout.separator()

        layout.menu("VIEW3D_MT_object_parent")
        layout.menu("VIEW3D_MT_object_track")
        layout.menu("VIEW3D_MT_object_group")
        layout.menu("VIEW3D_MT_object_constraints")

        layout.separator()

        layout.menu("VIEW3D_MT_object_quick_effects")

        layout.separator()

        layout.menu("VIEW3D_MT_object_game")

        layout.separator()

        layout.operator("object.join")

        layout.separator()

        layout.operator("object.move_to_layer", text="Move to Layer...")
        layout.menu("VIEW3D_MT_object_showhide")

        layout.operator_menu_enum("object.convert", "target")


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


class VIEW3D_MT_object_clear(Menu):
    bl_label = "Clear"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.location_clear", text="Location")
        layout.operator("object.rotation_clear", text="Rotation")
        layout.operator("object.scale_clear", text="Scale")
        layout.operator("object.origin_clear", text="Origin")


class VIEW3D_MT_object_specials(Menu):
    bl_label = "Specials"

    @classmethod
    def poll(cls, context):
        # add more special types
        return context.object

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        obj = context.object

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
                #layout.label(text="Test Has DOF obj");
                props = layout.operator("wm.context_modal_mouse", text="DOF Distance")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.dof_distance"
                props.input_scale = 0.02
                props.header_text = "DOF Distance: %.3f"

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

        if obj.type == 'LAMP':
            lamp = obj.data

            layout.operator_context = 'INVOKE_REGION_WIN'

            if scene.render.use_shading_nodes:
                try:
                    value = lamp.node_tree.nodes["Emission"].inputs["Strength"].default_value
                except AttributeError:
                    value = None

                if value is not None:
                    props = layout.operator("wm.context_modal_mouse", text="Strength")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.node_tree.nodes[\"Emission\"].inputs[\"Strength\"].default_value"
                    props.header_text = "Lamp Strength: %.3f"
                    props.input_scale = 0.1
                del value

                if lamp.type == 'AREA':
                    props = layout.operator("wm.context_modal_mouse", text="Size X")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.size"
                    props.header_text = "Lamp Size X: %.3f"

                    if lamp.shape == 'RECTANGLE':
                        props = layout.operator("wm.context_modal_mouse", text="Size Y")
                        props.data_path_iter = "selected_editable_objects"
                        props.data_path_item = "data.size_y"
                        props.header_text = "Lamp Size Y: %.3f"

                elif lamp.type in {'SPOT', 'POINT', 'SUN'}:
                    props = layout.operator("wm.context_modal_mouse", text="Size")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.shadow_soft_size"
                    props.header_text = "Lamp Size: %.3f"
            else:
                props = layout.operator("wm.context_modal_mouse", text="Energy")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.energy"
                props.header_text = "Lamp Energy: %.3f"

                if lamp.type in {'SPOT', 'AREA', 'POINT'}:
                    props = layout.operator("wm.context_modal_mouse", text="Falloff Distance")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.distance"
                    props.input_scale = 0.1
                    props.header_text = "Lamp Falloff Distance: %.1f"

            if lamp.type == 'SPOT':
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

                if not scene.render.use_shading_nodes:
                    props = layout.operator("wm.context_modal_mouse", text="Clip Start")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.shadow_buffer_clip_start"
                    props.input_scale = 0.05
                    props.header_text = "Clip Start: %.2f"

                    props = layout.operator("wm.context_modal_mouse", text="Clip End")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.shadow_buffer_clip_end"
                    props.input_scale = 0.05
                    props.header_text = "Clip End: %.2f"

        layout.separator()

        props = layout.operator("object.isolate_type_render")
        props = layout.operator("object.hide_render_clear_all")


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

        layout.operator("object.visual_transform_apply", text="Visual Transform", text_ctxt=i18n_contexts.default)
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


class VIEW3D_MT_object_group(Menu):
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout

        layout.operator("group.create")
        # layout.operator_menu_enum("group.objects_remove", "group")  # BUGGY
        layout.operator("group.objects_remove")
        layout.operator("group.objects_remove_all")

        layout.separator()

        layout.operator("group.objects_add_active")
        layout.operator("group.objects_remove_active")


class VIEW3D_MT_object_constraints(Menu):
    bl_label = "Constraints"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.constraint_add_with_targets")
        layout.operator("object.constraints_copy")
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
        layout.operator("object.hide_view_set", text="Hide Selected").unselected = False
        layout.operator("object.hide_view_set", text="Hide Unselected").unselected = True


class VIEW3D_MT_make_single_user(Menu):
    bl_label = "Make Single User"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("object.make_single_user", text="Object")
        props.object = True
        props.obdata = props.material = props.texture = props.animation = False

        props = layout.operator("object.make_single_user", text="Object & Data")
        props.object = props.obdata = True
        props.material = props.texture = props.animation = False

        props = layout.operator("object.make_single_user", text="Object & Data & Materials+Tex")
        props.object = props.obdata = props.material = props.texture = True
        props.animation = False

        props = layout.operator("object.make_single_user", text="Materials+Tex")
        props.material = props.texture = True
        props.object = props.obdata = props.animation = False

        props = layout.operator("object.make_single_user", text="Object Animation")
        props.animation = True
        props.object = props.obdata = props.material = props.texture = False


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
            layout.operator_menu_enum("object.make_links_scene", "scene", text="Objects to Scene...")
        layout.operator_context = operator_context_default

        layout.operator_enum("object.make_links_data", "type")  # inline

        layout.operator("object.join_uvs")  # stupid place to add this!


class VIEW3D_MT_object_game(Menu):
    bl_label = "Game"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.logic_bricks_copy", text="Copy Logic Bricks")
        layout.operator("object.game_physics_copy", text="Copy Physics Properties")

        layout.separator()

        layout.operator("object.game_property_copy", text="Replace Properties").operation = 'REPLACE'
        layout.operator("object.game_property_copy", text="Merge Properties").operation = 'MERGE'
        layout.operator_menu_enum("object.game_property_copy", "property", text="Copy Properties...")

        layout.separator()

        layout.operator("object.game_property_clear")


# ********** Brush menu **********
class VIEW3D_MT_brush(Menu):
    bl_label = "Brush"

    def draw(self, context):
        layout = self.layout

        settings = UnifiedPaintPanel.paint_settings(context)
        brush = settings.brush

        ups = context.tool_settings.unified_paint_settings
        layout.prop(ups, "use_unified_size", text="Unified Size")
        layout.prop(ups, "use_unified_strength", text="Unified Strength")
        layout.separator()

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

        # skip if no active brush
        if not brush:
            return

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

# ********** Vertex paint menu **********


class VIEW3D_MT_paint_vertex(Menu):
    bl_label = "Paint"

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

        layout.operator("paint.vertex_color_set")
        layout.operator("paint.vertex_color_smooth")
        layout.operator("paint.vertex_color_dirt")


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
                layout.operator("object.vertex_group_remove_from", text="Remove from Active Group").use_all_groups = False
                layout.operator("object.vertex_group_remove_from", text="Remove from All").use_all_groups = True
                layout.separator()

        if ob.vertex_groups.active:
            layout.operator_menu_enum("object.vertex_group_set_active", "group", text="Set Active Group")
            layout.operator("object.vertex_group_remove", text="Remove Active Group").all = False
            layout.operator("object.vertex_group_remove", text="Remove All Groups").all = True

# ********** Weight paint menu **********


class VIEW3D_MT_paint_weight(Menu):
    bl_label = "Weights"

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")
        layout.operator("ed.undo_history")

        layout.separator()

        layout.operator("paint.weight_from_bones", text="Assign Automatic From Bones").type = 'AUTOMATIC'
        layout.operator("paint.weight_from_bones", text="Assign From Bone Envelopes").type = 'ENVELOPES'

        layout.separator()

        layout.operator("object.vertex_group_normalize_all", text="Normalize All")
        layout.operator("object.vertex_group_normalize", text="Normalize")
        layout.operator("object.vertex_group_mirror", text="Mirror")
        layout.operator("object.vertex_group_invert", text="Invert")
        layout.operator("object.vertex_group_clean", text="Clean")
        layout.operator("object.vertex_group_quantize", text="Quantize")
        layout.operator("object.vertex_group_levels", text="Levels")
        layout.operator("object.vertex_group_blend", text="Blend")
        layout.operator("object.vertex_group_transfer_weight", text="Transfer Weights")
        layout.operator("object.vertex_group_limit_total", text="Limit Total")
        layout.operator("object.vertex_group_fix", text="Fix Deforms")

        layout.separator()

        layout.operator("paint.weight_set")

# ********** Sculpt menu **********


class VIEW3D_MT_sculpt(Menu):
    bl_label = "Sculpt"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        sculpt = toolsettings.sculpt

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

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


# ********** Particle menu **********


class VIEW3D_MT_particle(Menu):
    bl_label = "Particle"

    def draw(self, context):
        layout = self.layout

        particle_edit = context.tool_settings.particle_edit

        layout.operator("ed.undo")
        layout.operator("ed.redo")
        layout.operator("ed.undo_history")

        layout.separator()

        layout.operator("particle.mirror")

        layout.separator()

        layout.operator("particle.remove_doubles")
        layout.operator("particle.delete")

        if particle_edit.select_mode == 'POINT':
            layout.operator("particle.subdivide")

        layout.operator("particle.rekey")
        layout.operator("particle.weight_set")

        layout.separator()

        layout.menu("VIEW3D_MT_particle_showhide")


class VIEW3D_MT_particle_specials(Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        particle_edit = context.tool_settings.particle_edit

        layout.operator("particle.rekey")
        layout.operator("particle.delete")
        layout.operator("particle.remove_doubles")

        if particle_edit.select_mode == 'POINT':
            layout.operator("particle.subdivide")

        layout.operator("particle.weight_set")
        layout.separator()

        layout.operator("particle.mirror")

        if particle_edit.select_mode == 'POINT':
            layout.separator()
            layout.operator("particle.select_roots")
            layout.operator("particle.select_tips")

            layout.separator()

            layout.operator("particle.select_more")
            layout.operator("particle.select_less")

            layout.separator()

            layout.operator("particle.select_all").action = 'TOGGLE'
            layout.operator("particle.select_linked")
            layout.operator("particle.select_all", text="Inverse").action = 'INVERT'


class VIEW3D_MT_particle_showhide(ShowHideMenu, Menu):
    _operator_name = "particle"

# ********** Pose Menu **********


class VIEW3D_MT_pose(Menu):
    bl_label = "Pose"

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")
        layout.operator("ed.undo_history")

        layout.separator()

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
        layout.operator("pose.paste")
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

        layout.operator("pose.user_transforms_clear", text="Reset unkeyed")


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

            #layout.operator_context = 'INVOKE_AREA'
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
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("paint.weight_from_bones", text="Assign Automatic from Bones").type = 'AUTOMATIC'
        layout.operator("paint.weight_from_bones", text="Assign from Bone Envelopes").type = 'ENVELOPES'

        layout.separator()

        layout.operator("pose.select_constraint_target")
        layout.operator("pose.flip_names")
        layout.operator("pose.paths_calculate")
        layout.operator("pose.paths_clear")
        layout.operator("pose.user_transforms_clear")
        layout.operator("pose.user_transforms_clear", text="Clear User Transforms (All)").only_selected = False
        layout.operator("pose.relax")

        layout.separator()

        layout.operator_menu_enum("pose.autoside_names", "axis")


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

        toolsettings = context.tool_settings

        layout.operator("ed.undo")
        layout.operator("ed.redo")
        layout.operator("ed.undo_history")

        layout.separator()

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.menu("VIEW3D_MT_uv_map", text="UV Unwrap...")

        layout.separator()

        layout.operator("mesh.duplicate_move")
        layout.menu("VIEW3D_MT_edit_mesh_extrude")
        layout.menu("VIEW3D_MT_edit_mesh_delete")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_vertices")
        layout.menu("VIEW3D_MT_edit_mesh_edges")
        layout.menu("VIEW3D_MT_edit_mesh_faces")
        layout.menu("VIEW3D_MT_edit_mesh_normals")
        layout.menu("VIEW3D_MT_edit_mesh_clean")

        layout.separator()

        layout.operator("mesh.symmetrize")
        layout.operator("mesh.symmetry_snap")
        layout.operator("mesh.bisect")
        layout.operator_menu_enum("mesh.sort_elements", "type", text="Sort Elements...")

        layout.separator()

        layout.prop(toolsettings, "use_mesh_automerge")
        layout.prop_menu_enum(toolsettings, "proportional_edit")
        layout.prop_menu_enum(toolsettings, "proportional_edit_falloff")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_showhide")


class VIEW3D_MT_edit_mesh_specials(Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.subdivide", text="Subdivide").smoothness = 0.0
        layout.operator("mesh.subdivide", text="Subdivide Smooth").smoothness = 1.0

        layout.separator()

        layout.operator("mesh.merge", text="Merge...")
        layout.operator("mesh.remove_doubles")

        layout.separator()

        layout.operator("mesh.hide", text="Hide").unselected = False
        layout.operator("mesh.reveal", text="Reveal")
        layout.operator("mesh.select_all", text="Select Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("mesh.flip_normals")
        layout.operator("mesh.vertices_smooth", text="Smooth")
        layout.operator("mesh.vertices_smooth_laplacian", text="Laplacian Smooth")

        layout.separator()

        layout.operator("mesh.inset")
        layout.operator("mesh.bevel", text="Bevel")
        layout.operator("mesh.bridge_edge_loops")

        layout.separator()

        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")

        layout.separator()

        layout.operator("mesh.blend_from_shape")
        layout.operator("mesh.shape_propagate_to_all")
        layout.operator("mesh.shortest_path_select")
        layout.operator("mesh.sort_elements")
        layout.operator("mesh.symmetrize")
        layout.operator("mesh.symmetry_snap")


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
            layout.operator("mesh.extrude_vertices_move", text="Vertices Only"),
        'EDGE': lambda layout:
            layout.operator("mesh.extrude_edges_move", text="Edges Only"),
        'FACE': lambda layout:
            layout.operator("mesh.extrude_faces_move", text="Individual Faces"),
        'REGION': lambda layout:
            layout.operator("view3d.edit_mesh_extrude_move_normal", text="Region"),
        'REGION_VERT_NORMAL': lambda layout:
            layout.operator("view3d.edit_mesh_extrude_move_shrink_fatten", text="Region (Vertex Normals)"),
    }

    @staticmethod
    def extrude_options(context):
        mesh = context.object.data
        select_mode = context.tool_settings.mesh_select_mode

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
    bl_label = "Vertices"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.merge")
        layout.operator("mesh.rip_move")
        layout.operator("mesh.rip_move_fill")
        layout.operator("mesh.rip_edge_move")
        layout.operator("mesh.split")
        layout.operator_menu_enum("mesh.separate", "type")
        layout.operator("mesh.vert_connect", text="Connect")
        layout.operator("transform.vert_slide", text="Slide")

        layout.separator()

        op = layout.operator("mesh.mark_sharp", text="Shade Smooth")
        op.use_verts = True
        op.clear = True
        layout.operator("mesh.mark_sharp", text="Shade Sharp").use_verts = True

        layout.separator()

        layout.operator("mesh.bevel").vertex_only = True
        layout.operator("mesh.convex_hull")
        layout.operator("mesh.vertices_smooth")
        layout.operator("mesh.remove_doubles")

        layout.operator("mesh.blend_from_shape")

        layout.operator("object.vertex_group_blend")
        layout.operator("mesh.shape_propagate_to_all")

        layout.separator()

        layout.menu("VIEW3D_MT_vertex_group")
        layout.menu("VIEW3D_MT_hook")


class VIEW3D_MT_edit_mesh_edges(Menu):
    bl_label = "Edges"

    def draw(self, context):
        layout = self.layout

        with_freestyle = bpy.app.build_options.freestyle
        scene = context.scene

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.edge_face_add")
        layout.operator("mesh.subdivide")
        layout.operator("mesh.unsubdivide")

        layout.separator()

        layout.operator("transform.edge_crease")
        layout.operator("transform.edge_bevelweight")

        layout.separator()

        layout.operator("mesh.mark_seam").clear = False
        layout.operator("mesh.mark_seam", text="Clear Seam").clear = True

        layout.separator()

        layout.operator("mesh.mark_sharp")
        layout.operator("mesh.mark_sharp", text="Clear Sharp").clear = True

        layout.separator()

        if with_freestyle and not scene.render.use_shading_nodes:
            layout.operator("mesh.mark_freestyle_edge").clear = False
            layout.operator("mesh.mark_freestyle_edge", text="Clear Freestyle Edge").clear = True
            layout.separator()

        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").use_ccw = False
        layout.operator("mesh.edge_rotate", text="Rotate Edge CCW").use_ccw = True

        layout.separator()

        layout.operator("mesh.bevel").vertex_only = False
        layout.operator("mesh.edge_split")
        layout.operator("mesh.bridge_edge_loops")

        layout.separator()

        layout.operator("transform.edge_slide")
        layout.operator("mesh.loop_multi_select", text="Edge Loops").ring = False
        layout.operator("mesh.loop_multi_select", text="Edge Rings").ring = True
        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_edit_mesh_faces(Menu):
    bl_label = "Faces"
    bl_idname = "VIEW3D_MT_edit_mesh_faces"

    def draw(self, context):
        layout = self.layout

        with_freestyle = bpy.app.build_options.freestyle
        scene = context.scene

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.flip_normals")
        layout.operator("mesh.edge_face_add")
        layout.operator("mesh.fill")
        layout.operator("mesh.fill_grid")
        layout.operator("mesh.beautify_fill")
        layout.operator("mesh.inset")
        layout.operator("mesh.bevel").vertex_only = False
        layout.operator("mesh.solidify")
        layout.operator("mesh.wireframe")

        layout.separator()

        if with_freestyle and not scene.render.use_shading_nodes:
            layout.operator("mesh.mark_freestyle_face").clear = False
            layout.operator("mesh.mark_freestyle_face", text="Clear Freestyle Face").clear = True
            layout.separator()

        layout.operator("mesh.poke")
        layout.operator("mesh.quads_convert_to_tris")
        layout.operator("mesh.tris_convert_to_quads")
        layout.operator("mesh.face_split_by_edges")

        layout.separator()

        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")

        layout.separator()

        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").use_ccw = False

        layout.separator()

        layout.operator("mesh.uvs_rotate")
        layout.operator("mesh.uvs_reverse")
        layout.operator("mesh.colors_rotate")
        layout.operator("mesh.colors_reverse")


class VIEW3D_MT_edit_mesh_normals(Menu):
    bl_label = "Normals"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.normals_make_consistent", text="Recalculate Outside").inside = False
        layout.operator("mesh.normals_make_consistent", text="Recalculate Inside").inside = True

        layout.separator()

        layout.operator("mesh.flip_normals")


class VIEW3D_MT_edit_mesh_clean(Menu):
    bl_label = "Clean up"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.delete_loose")

        layout.separator()

        layout.operator("mesh.dissolve_degenerate")
        layout.operator("mesh.dissolve_limited")
        layout.operator("mesh.vert_connect_nonplanar")
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

# Edit Curve
# draw_curve is used by VIEW3D_MT_edit_curve and VIEW3D_MT_edit_surface


def draw_curve(self, context):
    layout = self.layout

    toolsettings = context.tool_settings

    layout.menu("VIEW3D_MT_transform")
    layout.menu("VIEW3D_MT_mirror")
    layout.menu("VIEW3D_MT_snap")

    layout.separator()

    layout.operator("curve.extrude_move")
    layout.operator("curve.duplicate_move")
    layout.operator("curve.split")
    layout.operator("curve.separate")
    layout.operator("curve.make_segment")
    layout.operator("curve.cyclic_toggle")
    layout.operator("curve.delete", text="Delete...")

    layout.separator()

    layout.menu("VIEW3D_MT_edit_curve_ctrlpoints")
    layout.menu("VIEW3D_MT_edit_curve_segments")

    layout.separator()

    layout.prop_menu_enum(toolsettings, "proportional_edit")
    layout.prop_menu_enum(toolsettings, "proportional_edit_falloff")

    layout.separator()

    layout.menu("VIEW3D_MT_edit_curve_showhide")


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
            layout.operator("curve.separate")

            layout.separator()

            layout.operator_menu_enum("curve.handle_type_set", "type")
            layout.operator("curve.normals_make_consistent")

            layout.separator()

        layout.menu("VIEW3D_MT_hook")


class VIEW3D_MT_edit_curve_segments(Menu):
    bl_label = "Segments"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.subdivide")
        layout.operator("curve.switch_direction")


class VIEW3D_MT_edit_curve_specials(Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.subdivide")
        layout.operator("curve.switch_direction")
        layout.operator("curve.spline_weight_set")
        layout.operator("curve.radius_set")
        layout.operator("curve.smooth")
        layout.operator("curve.smooth_weight")
        layout.operator("curve.smooth_radius")
        layout.operator("curve.smooth_tilt")


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
        layout.operator("font.style_toggle", text="Toggle Underline").style = 'UNDERLINE'
        layout.operator("font.style_toggle", text="Toggle Small Caps").style = 'SMALL_CAPS'

        layout.separator()

        layout.operator("font.insert_lorem")


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
        layout.operator("font.text_insert", text="Superscript 1").text = "\u00B9"
        layout.operator("font.text_insert", text="Superscript 2").text = "\u00B2"
        layout.operator("font.text_insert", text="Superscript 3").text = "\u00B3"
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

        toolsettings = context.tool_settings

        layout.operator("ed.undo")
        layout.operator("ed.redo")
        layout.operator("ed.undo_history")

        layout.separator()

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("mball.delete_metaelems", text="Delete...")
        layout.operator("mball.duplicate_metaelems")

        layout.separator()

        layout.prop_menu_enum(toolsettings, "proportional_edit")
        layout.prop_menu_enum(toolsettings, "proportional_edit_falloff")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_meta_showhide")


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

        toolsettings = context.tool_settings

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")
        layout.operator_menu_enum("lattice.flip", "axis")

        layout.separator()

        layout.operator("lattice.make_regular")

        layout.separator()

        layout.prop_menu_enum(toolsettings, "proportional_edit")
        layout.prop_menu_enum(toolsettings, "proportional_edit_falloff")


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
        layout.operator("armature.delete")
        layout.operator("armature.split")
        layout.operator("armature.separate")

        layout.separator()

        layout.operator("armature.subdivide", text="Subdivide")
        layout.operator("armature.switch_direction", text="Switch Direction")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
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


class VIEW3D_MT_armature_specials(Menu):
    bl_label = "Specials"

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

# ********** Panel **********


class VIEW3D_PT_view3d_properties(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "View"

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return (view)

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        col = layout.column()
        col.active = bool(view.region_3d.view_perspective != 'CAMERA' or view.region_quadviews)
        col.prop(view, "lens")
        col.label(text="Lock to Object:")
        col.prop(view, "lock_object", text="")
        lock_object = view.lock_object
        if lock_object:
            if lock_object.type == 'ARMATURE':
                col.prop_search(view, "lock_bone", lock_object.data,
                                "edit_bones" if lock_object.mode == 'EDIT'
                                else "bones",
                                text="")
        else:
            col.prop(view, "lock_cursor", text="Lock to Cursor")

        col = layout.column()
        col.prop(view, "lock_camera")

        col = layout.column(align=True)
        col.label(text="Clip:")
        col.prop(view, "clip_start", text="Start")
        col.prop(view, "clip_end", text="End")

        subcol = col.column(align=True)
        subcol.enabled = not view.lock_camera_and_layers
        subcol.label(text="Local Camera:")
        subcol.prop(view, "camera", text="")

        col = layout.column(align=True)
        col.prop(view, "use_render_border")
        col.active = view.region_3d.view_perspective != 'CAMERA'


class VIEW3D_PT_view3d_cursor(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "3D Cursor"

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return (view is not None)

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        layout.column().prop(view, "cursor_location", text="Location")


class VIEW3D_PT_view3d_name(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Item"

    @classmethod
    def poll(cls, context):
        return (context.space_data and context.active_object)

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        row = layout.row()
        row.label(text="", icon='OBJECT_DATA')
        row.prop(ob, "name", text="")

        if ob.type == 'ARMATURE' and ob.mode in {'EDIT', 'POSE'}:
            bone = context.active_bone
            if bone:
                row = layout.row()
                row.label(text="", icon='BONE_DATA')
                row.prop(bone, "name", text="")


class VIEW3D_PT_view3d_display(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Display"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return (view)

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        scene = context.scene

        col = layout.column()
        col.prop(view, "show_only_render")

        col = layout.column()
        display_all = not view.show_only_render
        col.active = display_all
        col.prop(view, "show_outline_selected")
        col.prop(view, "show_all_objects_origin")
        col.prop(view, "show_relationship_lines")

        col = layout.column()
        col.active = display_all
        split = col.split(percentage=0.55)
        split.prop(view, "show_floor", text="Grid Floor")

        row = split.row(align=True)
        row.prop(view, "show_axis_x", text="X", toggle=True)
        row.prop(view, "show_axis_y", text="Y", toggle=True)
        row.prop(view, "show_axis_z", text="Z", toggle=True)

        sub = col.column(align=True)
        sub.active = (display_all and view.show_floor)
        sub.prop(view, "grid_lines", text="Lines")
        sub.prop(view, "grid_scale", text="Scale")
        subsub = sub.column(align=True)
        subsub.active = scene.unit_settings.system == 'NONE'
        subsub.prop(view, "grid_subdivisions", text="Subdivisions")

        layout.separator()

        layout.operator("screen.region_quadview", text="Toggle Quad View")

        if view.region_quadviews:
            region = view.region_quadviews[2]
            col = layout.column()
            col.prop(region, "lock_rotation")
            row = col.row()
            row.enabled = region.lock_rotation
            row.prop(region, "show_sync_view")
            row = col.row()
            row.enabled = region.lock_rotation and region.show_sync_view
            row.prop(region, "use_box_clip")


class VIEW3D_PT_view3d_shading(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Shading"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return (view)

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        scene = context.scene
        gs = scene.game_settings
        obj = context.object

        col = layout.column()

        if not scene.render.use_shading_nodes:
            col.prop(gs, "material_mode", text="")

        if view.viewport_shade == 'SOLID':
            col.prop(view, "show_textured_solid")
            col.prop(view, "use_matcap")
            if view.use_matcap:
                col.template_icon_view(view, "matcap_icon")
        elif view.viewport_shade == 'TEXTURED':
            if scene.render.use_shading_nodes or gs.material_mode != 'GLSL':
                col.prop(view, "show_textured_shadeless")

        col.prop(view, "show_backface_culling")
        if obj and obj.mode == 'EDIT' and view.viewport_shade not in {'BOUNDBOX', 'WIREFRAME'}:
            col.prop(view, "show_occlude_wire")


class VIEW3D_PT_view3d_motion_tracking(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Motion Tracking"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return (view)

    def draw_header(self, context):
        view = context.space_data

        self.layout.prop(view, "show_reconstruction", text="")

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        col = layout.column()
        col.active = view.show_reconstruction
        col.prop(view, "show_camera_path", text="Camera Path")
        col.prop(view, "show_bundle_names", text="3D Marker Names")
        col.label(text="Track Type and Size:")
        row = col.row(align=True)
        row.prop(view, "tracks_draw_type", text="")
        row.prop(view, "tracks_draw_size", text="")


class VIEW3D_PT_view3d_meshdisplay(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Mesh Display"

    @classmethod
    def poll(cls, context):
        # The active object check is needed because of local-mode
        return (context.active_object and (context.mode == 'EDIT_MESH'))

    def draw(self, context):
        layout = self.layout
        with_freestyle = bpy.app.build_options.freestyle

        mesh = context.active_object.data
        scene = context.scene

        split = layout.split()

        col = split.column()
        col.label(text="Overlays:")
        col.prop(mesh, "show_faces", text="Faces")
        col.prop(mesh, "show_edges", text="Edges")
        col.prop(mesh, "show_edge_crease", text="Creases")
        if with_freestyle:
            col.prop(mesh, "show_edge_seams", text="Seams")

        layout.prop(mesh, "show_weight")

        col = split.column()
        col.label()
        if not with_freestyle:
            col.prop(mesh, "show_edge_seams", text="Seams")
        col.prop(mesh, "show_edge_sharp", text="Sharp", text_ctxt=i18n_contexts.plural)
        col.prop(mesh, "show_edge_bevel_weight", text="Bevel")
        if with_freestyle and not scene.render.use_shading_nodes:
            col.prop(mesh, "show_freestyle_edge_marks", text="Edge Marks")
            col.prop(mesh, "show_freestyle_face_marks", text="Face Marks")

        col = layout.column()

        col.separator()
        col.label(text="Normals:")
        row = col.row(align=True)

        row.prop(mesh, "show_normal_vertex", text="", icon='VERTEXSEL')
        row.prop(mesh, "show_normal_loop", text="", icon='VERTEXSEL')
        row.prop(mesh, "show_normal_face", text="", icon='FACESEL')

        sub = row.row(align=True)
        sub.active = mesh.show_normal_vertex or mesh.show_normal_face or mesh.show_normal_loop
        sub.prop(context.scene.tool_settings, "normal_size", text="Size")

        col.separator()
        split = layout.split()
        col = split.column()
        col.label(text="Edge Info:")
        col.prop(mesh, "show_extra_edge_length", text="Length")
        col.prop(mesh, "show_extra_edge_angle", text="Angle")
        col = split.column()
        col.label(text="Face Info:")
        col.prop(mesh, "show_extra_face_area", text="Area")
        col.prop(mesh, "show_extra_face_angle", text="Angle")
        if bpy.app.debug:
            layout.prop(mesh, "show_extra_indices")


class VIEW3D_PT_view3d_meshstatvis(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Mesh Analysis"

    @classmethod
    def poll(cls, context):
        # The active object check is needed because of local-mode
        return (context.active_object and (context.mode == 'EDIT_MESH'))

    def draw_header(self, context):
        mesh = context.active_object.data

        self.layout.prop(mesh, "show_statvis", text="")

    def draw(self, context):
        layout = self.layout

        mesh = context.active_object.data
        statvis = context.tool_settings.statvis
        layout.active = mesh.show_statvis

        layout.prop(statvis, "type")
        statvis_type = statvis.type
        if statvis_type == 'OVERHANG':
            row = layout.row(align=True)
            row.prop(statvis, "overhang_min", text="")
            row.prop(statvis, "overhang_max", text="")
            layout.prop(statvis, "overhang_axis", expand=True)
        elif statvis_type == 'THICKNESS':
            row = layout.row(align=True)
            row.prop(statvis, "thickness_min", text="")
            row.prop(statvis, "thickness_max", text="")
            layout.prop(statvis, "thickness_samples")
        elif statvis_type == 'INTERSECT':
            pass
        elif statvis_type == 'DISTORT':
            row = layout.row(align=True)
            row.prop(statvis, "distort_min", text="")
            row.prop(statvis, "distort_max", text="")
        elif statvis_type == 'SHARP':
            row = layout.row(align=True)
            row.prop(statvis, "sharp_min", text="")
            row.prop(statvis, "sharp_max", text="")


class VIEW3D_PT_view3d_curvedisplay(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Curve Display"

    @classmethod
    def poll(cls, context):
        editmesh = context.mode == 'EDIT_CURVE'
        return (editmesh)

    def draw(self, context):
        layout = self.layout

        curve = context.active_object.data

        col = layout.column()
        row = col.row()
        row.prop(curve, "show_handles", text="Handles")
        row.prop(curve, "show_normal_face", text="Normals")
        col.prop(context.scene.tool_settings, "normal_size", text="Normal Size")


class VIEW3D_PT_background_image(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Background Images"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        view = context.space_data

        self.layout.prop(view, "show_background_images", text="")

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        col = layout.column()
        col.operator("view3d.background_image_add", text="Add Image")

        for i, bg in enumerate(view.background_images):
            layout.active = view.show_background_images
            box = layout.box()
            row = box.row(align=True)
            row.prop(bg, "show_expanded", text="", emboss=False)
            if bg.source == 'IMAGE' and bg.image:
                row.prop(bg.image, "name", text="", emboss=False)
            elif bg.source == 'MOVIE_CLIP' and bg.clip:
                row.prop(bg.clip, "name", text="", emboss=False)
            else:
                row.label(text="Not Set")

            if bg.show_background_image:
                row.prop(bg, "show_background_image", text="", emboss=False, icon='RESTRICT_VIEW_OFF')
            else:
                row.prop(bg, "show_background_image", text="", emboss=False, icon='RESTRICT_VIEW_ON')

            row.operator("view3d.background_image_remove", text="", emboss=False, icon='X').index = i

            box.prop(bg, "view_axis", text="Axis")

            if bg.show_expanded:
                row = box.row()
                row.prop(bg, "source", expand=True)

                has_bg = False
                if bg.source == 'IMAGE':
                    row = box.row()
                    row.template_ID(bg, "image", open="image.open")
                    if bg.image is not None:
                        box.template_image(bg, "image", bg.image_user, compact=True)
                        has_bg = True

                elif bg.source == 'MOVIE_CLIP':
                    box.prop(bg, "use_camera_clip")

                    column = box.column()
                    column.active = not bg.use_camera_clip
                    column.template_ID(bg, "clip", open="clip.open")

                    if bg.clip:
                        column.template_movieclip(bg, "clip", compact=True)

                    if bg.use_camera_clip or bg.clip:
                        has_bg = True

                    column = box.column()
                    column.active = has_bg
                    column.prop(bg.clip_user, "proxy_render_size", text="")
                    column.prop(bg.clip_user, "use_render_undistorted")

                if has_bg:
                    col = box.column()
                    col.prop(bg, "opacity", slider=True)
                    col.row().prop(bg, "draw_depth", expand=True)

                    if bg.view_axis in {'CAMERA', 'ALL'}:
                        col.row().prop(bg, "frame_method", expand=True)

                    row = col.row(align=True)
                    row.prop(bg, "offset_x", text="X")
                    row.prop(bg, "offset_y", text="Y")

                    if bg.view_axis != 'CAMERA':
                        col.prop(bg, "size")


class VIEW3D_PT_transform_orientations(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Transform Orientations"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return (view)

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        orientation = view.current_orientation

        row = layout.row(align=True)
        row.prop(view, "transform_orientation", text="")
        row.operator("transform.create_orientation", text="", icon='ZOOMIN')

        if orientation:
            row = layout.row(align=True)
            row.prop(orientation, "name", text="")
            row.operator("transform.delete_orientation", text="", icon='X')


class VIEW3D_PT_etch_a_ton(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Skeleton Sketching"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        scene = context.space_data
        ob = context.active_object
        return scene and ob and ob.type == 'ARMATURE' and ob.mode == 'EDIT'

    def draw_header(self, context):
        layout = self.layout
        toolsettings = context.scene.tool_settings

        layout.prop(toolsettings, "use_bone_sketching", text="")

    def draw(self, context):
        layout = self.layout

        toolsettings = context.scene.tool_settings

        col = layout.column()

        col.prop(toolsettings, "use_etch_quick")
        col.prop(toolsettings, "use_etch_overdraw")

        col.separator()

        col.prop(toolsettings, "etch_convert_mode")

        if toolsettings.etch_convert_mode == 'LENGTH':
            col.prop(toolsettings, "etch_length_limit")
        elif toolsettings.etch_convert_mode == 'ADAPTIVE':
            col.prop(toolsettings, "etch_adaptive_limit")
        elif toolsettings.etch_convert_mode == 'FIXED':
            col.prop(toolsettings, "etch_subdivision_number")
        elif toolsettings.etch_convert_mode == 'RETARGET':
            col.prop(toolsettings, "etch_template")
            col.prop(toolsettings, "etch_roll_mode")

            col.separator()

            colsub = col.column(align=True)
            colsub.prop(toolsettings, "use_etch_autoname")
            sub = colsub.column(align=True)
            sub.enabled = not toolsettings.use_etch_autoname
            sub.prop(toolsettings, "etch_number")
            sub.prop(toolsettings, "etch_side")

        col.separator()

        col.operator("sketch.convert", text="Convert to Bones")
        col.operator("sketch.delete", text="Delete Strokes")


class VIEW3D_PT_context_properties(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Properties"
    bl_options = {'DEFAULT_CLOSED'}

    def _active_context_member(context):
        obj = context.object
        if obj:
            mode = obj.mode
            if mode == 'POSE':
                return "active_pose_bone"
            elif mode == 'EDIT' and obj.type == 'ARMATURE':
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


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
