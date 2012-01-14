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


class VIEW3D_HT_header(Header):
    bl_space_type = 'VIEW_3D'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.template_header()

        # Menus
        if context.area.show_menus:
            sub = row.row(align=True)

            sub.menu("VIEW3D_MT_view")

            # Select Menu
            if mode_string not in {'EDIT_TEXT', 'SCULPT', 'PAINT_WEIGHT', 'PAINT_VERTEX', 'PAINT_TEXTURE'}:
                sub.menu("VIEW3D_MT_select_%s" % mode_string.lower())

            if edit_object:
                sub.menu("VIEW3D_MT_edit_%s" % edit_object.type.lower())
            elif obj:
                if mode_string not in {'PAINT_TEXTURE'}:
                    sub.menu("VIEW3D_MT_%s" % mode_string.lower())
            else:
                sub.menu("VIEW3D_MT_object")

        # Contains buttons like Mode, Pivot, Manipulator, Layer, Mesh Select Mode...
        row = layout
        layout.template_header_3D()

        if obj:
            # Particle edit
            if obj.mode == 'PARTICLE_EDIT':
                row.prop(toolsettings.particle_edit, "select_mode", text="", expand=True)

            # Occlude geometry
            if view.viewport_shade in {'SOLID', 'SHADED', 'TEXTURED'} and (obj.mode == 'PARTICLE_EDIT' or (obj.mode == 'EDIT' and obj.type == 'MESH')):
                row.prop(view, "use_occlude_geometry", text="")

            # Proportional editing
            if obj.mode in {'EDIT', 'PARTICLE_EDIT'}:
                row = layout.row(align=True)
                row.prop(toolsettings, "proportional_edit", text="", icon_only=True)
                if toolsettings.proportional_edit != 'DISABLED':
                    row.prop(toolsettings, "proportional_edit_falloff", text="", icon_only=True)
            elif obj.mode == 'OBJECT':
                row = layout.row(align=True)
                row.prop(toolsettings, "use_proportional_edit_objects", text="", icon_only=True)
                if toolsettings.use_proportional_edit_objects:
                    row.prop(toolsettings, "proportional_edit_falloff", text="", icon_only=True)

        # Snap
        snap_element = toolsettings.snap_element
        row = layout.row(align=True)
        row.prop(toolsettings, "use_snap", text="")
        row.prop(toolsettings, "snap_element", text="", icon_only=True)
        if snap_element != 'INCREMENT':
            row.prop(toolsettings, "snap_target", text="")
            if obj:
                if obj.mode == 'OBJECT':
                    row.prop(toolsettings, "use_snap_align_rotation", text="")
                elif obj.mode == 'EDIT':
                    row.prop(toolsettings, "use_snap_self", text="")

        if snap_element == 'VOLUME':
            row.prop(toolsettings, "use_snap_peel_object", text="")
        elif snap_element == 'FACE':
            row.prop(toolsettings, "use_snap_project", text="")

        # OpenGL render
        row = layout.row(align=True)
        row.operator("render.opengl", text="", icon='RENDER_STILL')
        props = row.operator("render.opengl", text="", icon='RENDER_ANIMATION')
        props.animation = True

        # Pose
        if obj and obj.mode == 'POSE':
            row = layout.row(align=True)
            row.operator("pose.copy", text="", icon='COPYDOWN')
            row.operator("pose.paste", text="", icon='PASTEDOWN')
            props = row.operator("pose.paste", text="", icon='PASTEFLIPDOWN')
            props.flipped = 1


# ********** Menu **********

# ********** Utilities **********


class ShowHideMenu():
    bl_label = "Show/Hide"
    _operator_name = ""

    def draw(self, context):
        layout = self.layout

        layout.operator("%s.reveal" % self._operator_name, text="Show Hidden")
        layout.operator("%s.hide" % self._operator_name, text="Hide Selected")
        layout.operator("%s.hide" % self._operator_name, text="Hide Unselected").unselected = True


class VIEW3D_MT_transform(Menu):
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
        layout.operator("transform.warp", text="Warp")
        layout.operator("transform.push_pull", text="Push/Pull")

        layout.separator()

        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True

        layout.separator()

        obj = context.object
        if obj.type == 'ARMATURE' and obj.mode in {'EDIT', 'POSE'} and obj.data.draw_type in {'BBONE', 'ENVELOPE'}:
            layout.operator("transform.transform", text="Scale Envelope/BBone").mode = 'BONE_SIZE'

        if context.edit_object and context.edit_object.type == 'ARMATURE':
            layout.operator("armature.align")
        else:
            layout.operator_context = 'EXEC_REGION_WIN'
            layout.operator("transform.transform", text="Align to Transform Orientation").mode = 'ALIGN'  # XXX see alignmenu() in edit.c of b2.4x to get this working

        layout.separator()

        layout.operator_context = 'EXEC_AREA'

        layout.operator("object.origin_set", text="Geometry to Origin").type = 'GEOMETRY_ORIGIN'
        layout.operator("object.origin_set", text="Origin to Geometry").type = 'ORIGIN_GEOMETRY'
        layout.operator("object.origin_set", text="Origin to 3D Cursor").type = 'ORIGIN_CURSOR'

        layout.separator()

        layout.operator("object.randomize_transform")
        layout.operator("object.align")

        layout.separator()

        layout.operator("object.anim_transforms_to_deltas")


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
        layout.operator("view3d.snap_selected_to_cursor", text="Selection to Cursor")

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

        layout.operator_context = 'EXEC_REGION_WIN'
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
        layout = self.layout

        layout.operator_enum("view3d.view_orbit", "type")

        layout.separator()

        layout.operator_enum("view3d.view_pan", "type")

        layout.separator()

        layout.operator("view3d.zoom", text="Zoom In").delta = 1
        layout.operator("view3d.zoom", text="Zoom Out").delta = -1
        layout.operator("view3d.zoom_camera_1_to_1", text="Zoom Camera 1:1")

        layout.separator()

        layout.operator("view3d.fly")


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


class VIEW3D_MT_view_align_selected(Menu):
    bl_label = "Align View to Selected"

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

        layout.operator("object.select_all", text="Select/Deselect All").action = 'TOGGLE'
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

        layout.separator()

        layout.operator("pose.select_all", text="Select/Deselect All").action = 'TOGGLE'
        layout.operator("pose.select_all", text="Inverse").action = 'INVERT'
        layout.operator("pose.select_flip_active", text="Flip Active")
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

        layout.operator("particle.select_all", text="Select/Deselect All").action = 'TOGGLE'
        layout.operator("particle.select_linked")
        layout.operator("particle.select_all").action = 'INVERT'

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

        layout.operator("mesh.select_all", text="Select/Deselect All").action = 'TOGGLE'
        layout.operator("mesh.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("mesh.select_random", text="Random")
        layout.operator("mesh.select_nth", text="Every N Number of Verts")
        layout.operator("mesh.edges_select_sharp", text="Sharp Edges")
        layout.operator("mesh.faces_select_linked_flat", text="Linked Flat Faces")
        layout.operator("mesh.faces_select_interior", text="Interior Faces")
        layout.operator("mesh.select_axis", text="Side of Active")

        layout.separator()

        layout.operator("mesh.select_by_number_vertices", text="Triangles").type = 'TRIANGLES'
        layout.operator("mesh.select_by_number_vertices", text="Quads").type = 'QUADS'
        if context.scene.tool_settings.mesh_select_mode[2] == False:
            layout.operator("mesh.select_non_manifold", text="Non Manifold")
        layout.operator("mesh.select_by_number_vertices", text="Loose Verts/Edges").type = 'OTHER'
        layout.operator("mesh.select_similar", text="Similar")

        layout.separator()

        layout.operator("mesh.select_less", text="Less")
        layout.operator("mesh.select_more", text="More")

        layout.separator()

        layout.operator("mesh.select_mirror", text="Mirror")

        layout.operator("mesh.select_linked", text="Linked")
        layout.operator("mesh.select_vertex_path", text="Vertex Path")
        layout.operator("mesh.loop_multi_select", text="Edge Loop").ring = False
        layout.operator("mesh.loop_multi_select", text="Edge Ring").ring = True

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

        layout.operator("curve.select_all", text="Select/Deselect All").action = 'TOGGLE'
        layout.operator("curve.select_all", text="Inverse").action = 'INVERT'
        layout.operator("curve.select_random")
        layout.operator("curve.select_nth", text="Every Nth Number of Points")

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

        layout.operator("curve.select_all", text="Select/Deselect All").action = 'TOGGLE'
        layout.operator("curve.select_all", text="Inverse").action = 'INVERT'
        layout.operator("curve.select_random")
        layout.operator("curve.select_nth", text="Every Nth Number of Points")

        layout.separator()

        layout.operator("curve.select_row")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_select_edit_metaball(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("mball.select_all").action = 'TOGGLE'
        layout.operator("mball.select_all").action = 'INVERT'

        layout.separator()

        layout.operator("mball.select_random_metaelems")


class VIEW3D_MT_select_edit_lattice(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("lattice.select_all", text="Select/Deselect All")


class VIEW3D_MT_select_edit_armature(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("armature.select_all", text="Select/Deselect All").action = 'TOGGLE'
        layout.operator("armature.select_all", text="Inverse").action = 'INVERT'

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

        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_face(Menu):  # XXX no matching enum
    bl_label = "Select"

    def draw(self, context):
        # layout = self.layout

        # TODO
        # see view3d_select_faceselmenu
        pass

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

        layout.menu("VIEW3D_MT_transform")
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
        layout.operator("anim.keyframe_delete_v3d", text="Delete Keyframe...")
        layout.operator("anim.keying_set_active_set", text="Change Keying Set...")


class VIEW3D_MT_object_clear(Menu):
    bl_label = "Clear"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.location_clear", text="Location")
        layout.operator("object.rotation_clear", text="Rotation")
        layout.operator("object.scale_clear", text="Scale")
        layout ("object.origin_clear", text="Origin")


class VIEW3D_MT_object_specials(Menu):
    bl_label = "Specials"

    @classmethod
    def poll(cls, context):
        # add more special types
        return context.object

    def draw(self, context):
        layout = self.layout

        obj = context.object
        if obj.type == 'CAMERA':
            layout.operator_context = 'INVOKE_REGION_WIN'

            if obj.data.type == 'PERSP':
                props = layout.operator("wm.context_modal_mouse", text="Camera Lens Angle")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.lens"
                props.input_scale = 0.1
            else:
                props = layout.operator("wm.context_modal_mouse", text="Camera Lens Scale")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.ortho_scale"
                props.input_scale = 0.01

            if not obj.data.dof_object:
                #layout.label(text="Test Has DOF obj");
                props = layout.operator("wm.context_modal_mouse", text="DOF Distance")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.dof_distance"
                props.input_scale = 0.02

        if obj.type in {'CURVE', 'FONT'}:
            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Extrude Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "data.extrude"
            props.input_scale = 0.01

            props = layout.operator("wm.context_modal_mouse", text="Width Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "data.offset"
            props.input_scale = 0.01

        if obj.type == 'EMPTY':
            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Empty Draw Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "empty_draw_size"
            props.input_scale = 0.01

        if obj.type == 'LAMP':
            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Energy")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "data.energy"

            if obj.data.type in {'SPOT', 'AREA', 'POINT'}:
                props = layout.operator("wm.context_modal_mouse", text="Falloff Distance")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.distance"
                props.input_scale = 0.1

            if obj.data.type == 'SPOT':
                layout.separator()
                props = layout.operator("wm.context_modal_mouse", text="Spot Size")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.spot_size"
                props.input_scale = 0.01

                props = layout.operator("wm.context_modal_mouse", text="Spot Blend")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.spot_blend"
                props.input_scale = -0.01

                props = layout.operator("wm.context_modal_mouse", text="Clip Start")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.shadow_buffer_clip_start"
                props.input_scale = 0.05

                props = layout.operator("wm.context_modal_mouse", text="Clip End")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.shadow_buffer_clip_end"
                props.input_scale = 0.05

        layout.separator()

        props = layout.operator("object.isolate_type_render")
        props = layout.operator("object.hide_render_clear_all")


class VIEW3D_MT_object_apply(Menu):
    bl_label = "Apply"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("object.transform_apply", text="Location")
        props.location, props.rotation, props.scale = True, False, False

        props = layout.operator("object.transform_apply", text="Rotation")
        props.location, props.rotation, props.scale = False, True, False

        props = layout.operator("object.transform_apply", text="Scale")
        props.location, props.rotation, props.scale = False, False, True
        props = layout.operator("object.transform_apply", text="Rotation & Scale")
        props.location, props.rotation, props.scale = False, True, True

        layout.separator()

        layout.operator("object.visual_transform_apply", text="Visual Transform")
        layout.operator("object.duplicates_make_real")


class VIEW3D_MT_object_parent(Menu):
    bl_label = "Parent"

    def draw(self, context):
        layout = self.layout

        layout.operator_menu_enum("object.parent_set", "type", text="Set")
        layout.operator_menu_enum("object.parent_clear", "type", text="Clear")


class VIEW3D_MT_object_track(Menu):
    bl_label = "Track"

    def draw(self, context):
        layout = self.layout

        layout.operator_menu_enum("object.track_set", "type", text="Set")
        layout.operator_menu_enum("object.track_clear", "type", text="Clear")


class VIEW3D_MT_object_group(Menu):
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout

        layout.operator("group.create")
        layout.operator("group.objects_remove")

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

        props = layout.operator("object.make_single_user", text="Object & Data")
        props.object = props.obdata = True

        props = layout.operator("object.make_single_user", text="Object & Data & Materials+Tex")
        props.object = props.obdata = props.material = props.texture = True

        props = layout.operator("object.make_single_user", text="Materials+Tex")
        props.material = props.texture = True

        props = layout.operator("object.make_single_user", text="Object Animation")
        props.animation = True


class VIEW3D_MT_make_links(Menu):
    bl_label = "Make Links"

    def draw(self, context):
        layout = self.layout

        if(len(bpy.data.scenes) > 10):
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("object.make_links_scene", text="Objects to Scene...", icon='OUTLINER_OB_EMPTY')
        else:
            layout.operator_menu_enum("object.make_links_scene", "scene", text="Objects to Scene...")

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


# ********** Vertex paint menu **********


class VIEW3D_MT_paint_vertex(Menu):
    bl_label = "Paint"

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

        layout.operator("paint.vertex_color_set")
        layout.operator("paint.vertex_color_dirt")


class VIEW3D_MT_hook(Menu):
    bl_label = "Hooks"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.hook_add_newob")
        layout.operator("object.hook_add_selob")

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
        layout.operator("object.vertex_group_assign", text="Assign to New Group").new = True

        ob = context.active_object
        if ob.mode == 'EDIT' or (ob.mode == 'WEIGHT_PAINT' and ob.type == 'MESH' and ob.data.use_paint_mask_vertex):
            if ob.vertex_groups.active:
                layout.separator()
                layout.operator("object.vertex_group_assign", text="Assign to Active Group")
                layout.operator("object.vertex_group_remove_from", text="Remove from Active Group").all = False
                layout.operator("object.vertex_group_remove_from", text="Remove from All").all = True
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
        layout.operator("object.vertex_group_levels", text="Levels")
        layout.operator("object.vertex_group_fix", text="Fix Deforms")

        layout.separator()

        layout.operator("paint.weight_set")

# ********** Sculpt menu **********


class VIEW3D_MT_sculpt(Menu):
    bl_label = "Sculpt"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt
        brush = tool_settings.sculpt.brush

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
        layout.operator_menu_enum("brush.curve_preset", "shape")
        layout.separator()

        if brush is not None:  # unlikely but can happen
            sculpt_tool = brush.sculpt_tool

            if sculpt_tool != 'GRAB':
                layout.prop_menu_enum(brush, "stroke_method")

                if sculpt_tool in {'DRAW', 'PINCH', 'INFLATE', 'LAYER', 'CLAY'}:
                    layout.prop_menu_enum(brush, "direction")

                if sculpt_tool == 'LAYER':
                    layout.prop(brush, "use_persistent")
                    layout.operator("sculpt.set_persistent_base")

        layout.separator()
        layout.prop(sculpt, "use_threaded", text="Threaded Sculpt")
        layout.prop(sculpt, "show_brush")

        # TODO, make available from paint menu!
        layout.prop(tool_settings, "sculpt_paint_use_unified_size", text="Unify Size")
        layout.prop(tool_settings, "sculpt_paint_use_unified_strength", text="Unify Strength")

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

        layout.separator()
        if particle_edit.select_mode == 'POINT':
            layout.operator("particle.subdivide")
            layout.operator("particle.select_roots")
            layout.operator("particle.select_tips")

        layout.operator("particle.remove_doubles")


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

        layout.menu("VIEW3D_MT_transform")

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
        layout.operator("pose.armature_layers", text="Change Armature Layers...")
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
        layout.operator("pose.group_add")
        layout.operator("pose.group_remove")

        layout.separator()

        layout.operator("pose.group_assign")
        layout.operator("pose.group_unassign")


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
            props = layout.operator("wm.context_collection_boolean_set", text=bone_props[opt].name)
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

        settings = context.tool_settings

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

        layout.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Region")
        layout.operator("view3d.edit_mesh_extrude_individual_move", text="Extrude Individual")
        layout.operator("mesh.duplicate_move")
        layout.operator("mesh.delete", text="Delete...")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_vertices")
        layout.menu("VIEW3D_MT_edit_mesh_edges")
        layout.menu("VIEW3D_MT_edit_mesh_faces")
        layout.menu("VIEW3D_MT_edit_mesh_normals")

        layout.separator()

        layout.prop(settings, "use_mesh_automerge")
        layout.prop_menu_enum(settings, "proportional_edit")
        layout.prop_menu_enum(settings, "proportional_edit_falloff")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_showhide")


class VIEW3D_MT_edit_mesh_specials(Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.subdivide", text="Subdivide").smoothness = 0.0
        layout.operator("mesh.subdivide", text="Subdivide Smooth").smoothness = 1.0
        layout.operator("mesh.merge", text="Merge...")
        layout.operator("mesh.remove_doubles")
        layout.operator("mesh.hide", text="Hide")
        layout.operator("mesh.reveal", text="Reveal")
        layout.operator("mesh.select_all").action = 'INVERT'
        layout.operator("mesh.flip_normals")
        layout.operator("mesh.vertices_smooth", text="Smooth")
        # layout.operator("mesh.bevel", text="Bevel")
        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")
        layout.operator("mesh.blend_from_shape")
        layout.operator("mesh.shape_propagate_to_all")
        layout.operator("mesh.select_vertex_path")


class VIEW3D_MT_edit_mesh_select_mode(Menu):
    bl_label = "Mesh Select Mode"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        props = layout.operator("wm.context_set_value", text="Vertex", icon='VERTEXSEL')
        props.value = "(True, False, False)"
        props.data_path = "tool_settings.mesh_select_mode"

        props = layout.operator("wm.context_set_value", text="Edge", icon='EDGESEL')
        props.value = "(False, True, False)"
        props.data_path = "tool_settings.mesh_select_mode"

        props = layout.operator("wm.context_set_value", text="Face", icon='FACESEL')
        props.value = "(False, False, True)"
        props.data_path = "tool_settings.mesh_select_mode"


class VIEW3D_MT_edit_mesh_extrude(Menu):
    bl_label = "Extrude"

    _extrude_funcs = {
        'VERT': lambda layout: layout.operator("mesh.extrude_vertices_move", text="Vertices Only"),
        'EDGE': lambda layout: layout.operator("mesh.extrude_edges_move", text="Edges Only"),
        'FACE': lambda layout: layout.operator("mesh.extrude_faces_move", text="Individual Faces"),
        'REGION': lambda layout: layout.operator("view3d.edit_mesh_extrude_move_normal", text="Region"),
    }

    @staticmethod
    def extrude_options(context):
        mesh = context.object.data
        select_mode = context.tool_settings.mesh_select_mode

        menu = []
        if mesh.total_face_sel:
            menu += ['REGION', 'FACE']
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
        layout.operator("mesh.split")
        layout.operator("mesh.separate")

        layout.separator()

        layout.operator("mesh.vertices_smooth")
        layout.operator("mesh.remove_doubles")
        layout.operator("mesh.vertices_sort")
        layout.operator("mesh.vertices_randomize")

        layout.operator("mesh.select_vertex_path")

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
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.edge_face_add")
        layout.operator("mesh.subdivide")

        layout.separator()

        layout.operator("mesh.mark_seam").clear = False
        layout.operator("mesh.mark_seam", text="Clear Seam").clear = True

        layout.separator()

        layout.operator("mesh.mark_sharp").clear = False
        layout.operator("mesh.mark_sharp", text="Clear Sharp").clear = True

        layout.separator()

        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").direction = 'CW'
        layout.operator("mesh.edge_rotate", text="Rotate Edge CCW").direction = 'CCW'

        layout.separator()

        layout.operator("TRANSFORM_OT_edge_slide")
        layout.operator("TRANSFORM_OT_edge_crease")
        layout.operator("mesh.loop_multi_select", text="Edge Loop").ring = False

        # uiItemO(layout, "Loopcut", 0, "mesh.loop_cut"); // CutEdgeloop(em, 1);
        # uiItemO(layout, "Edge Slide", 0, "mesh.edge_slide"); // EdgeSlide(em, 0,0.0);

        layout.operator("mesh.loop_multi_select", text="Edge Ring").ring = True

        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_edit_mesh_faces(Menu):
    bl_label = "Faces"
    bl_idname = "VIEW3D_MT_edit_mesh_faces"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.flip_normals")
        # layout.operator("mesh.bevel")
        # layout.operator("mesh.bevel")
        layout.operator("mesh.edge_face_add")
        layout.operator("mesh.fill")
        layout.operator("mesh.beautify_fill")
        layout.operator("mesh.solidify")
        layout.operator("mesh.sort_faces")

        layout.separator()

        layout.operator("mesh.fgon_make")
        layout.operator("mesh.fgon_clear")

        layout.separator()

        layout.operator("mesh.quads_convert_to_tris")
        layout.operator("mesh.tris_convert_to_quads")
        layout.operator("mesh.edge_flip")

        layout.separator()

        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")

        layout.separator()

        # uiItemO(layout, NULL, 0, "mesh.face_mode"); // mesh_set_face_flags(em, 1);
        # uiItemBooleanO(layout, NULL, 0, "mesh.face_mode", "clear", 1); // mesh_set_face_flags(em, 0);

        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").direction = 'CW'

        layout.separator()

        layout.operator_menu_enum("mesh.uvs_rotate", "direction")
        layout.operator_menu_enum("mesh.uvs_mirror", "axis")
        layout.operator_menu_enum("mesh.colors_rotate", "direction")
        layout.operator_menu_enum("mesh.colors_mirror", "axis")


class VIEW3D_MT_edit_mesh_normals(Menu):
    bl_label = "Normals"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.normals_make_consistent", text="Recalculate Outside").inside = False
        layout.operator("mesh.normals_make_consistent", text="Recalculate Inside").inside = True

        layout.separator()

        layout.operator("mesh.flip_normals")


class VIEW3D_MT_edit_mesh_showhide(ShowHideMenu, Menu):
    _operator_name = "mesh"

# Edit Curve
# draw_curve is used by VIEW3D_MT_edit_curve and VIEW3D_MT_edit_surface


def draw_curve(self, context):
    layout = self.layout

    settings = context.tool_settings

    layout.menu("VIEW3D_MT_transform")
    layout.menu("VIEW3D_MT_mirror")
    layout.menu("VIEW3D_MT_snap")

    layout.separator()

    layout.operator("curve.extrude")
    layout.operator("curve.duplicate")
    layout.operator("curve.separate")
    layout.operator("curve.make_segment")
    layout.operator("curve.cyclic_toggle")
    layout.operator("curve.delete", text="Delete...")

    layout.separator()

    layout.menu("VIEW3D_MT_edit_curve_ctrlpoints")
    layout.menu("VIEW3D_MT_edit_curve_segments")

    layout.separator()

    layout.prop_menu_enum(settings, "proportional_edit")
    layout.prop_menu_enum(settings, "proportional_edit_falloff")

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
        layout.operator("curve.smooth_radius")


class VIEW3D_MT_edit_curve_showhide(ShowHideMenu, Menu):
    _operator_name = "curve"


class VIEW3D_MT_edit_surface(Menu):
    bl_label = "Surface"

    draw = draw_curve


class VIEW3D_MT_edit_font(Menu):
    bl_label = "Text"

    def draw(self, context):
        layout = self.layout

        layout.operator("font.file_paste")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_text_chars")

        layout.separator()

        layout.operator("font.style_toggle", text="Toggle Bold").style = 'BOLD'
        layout.operator("font.style_toggle", text="Toggle Italic").style = 'ITALIC'
        layout.operator("font.style_toggle", text="Toggle Underline").style = 'UNDERLINE'
        layout.operator("font.style_toggle", text="Toggle Small Caps").style = 'SMALL_CAPS'


class VIEW3D_MT_edit_text_chars(Menu):
    bl_label = "Special Characters"

    def draw(self, context):
        layout = self.layout

        layout.operator("font.text_insert", text="Copyright|Alt C").text = "\u00A9"
        layout.operator("font.text_insert", text="Registered Trademark|Alt R").text = "\u00AE"

        layout.separator()

        layout.operator("font.text_insert", text="Degree Sign|Alt G").text = "\u00B0"
        layout.operator("font.text_insert", text="Multiplication Sign|Alt x").text = "\u00D7"
        layout.operator("font.text_insert", text="Circle|Alt .").text = "\u008A"
        layout.operator("font.text_insert", text="Superscript 1|Alt 1").text = "\u00B9"
        layout.operator("font.text_insert", text="Superscript 2|Alt 2").text = "\u00B2"
        layout.operator("font.text_insert", text="Superscript 3|Alt 3").text = "\u00B3"
        layout.operator("font.text_insert", text="Double >>|Alt >").text = "\u00BB"
        layout.operator("font.text_insert", text="Double <<|Alt <").text = "\u00AB"
        layout.operator("font.text_insert", text="Promillage|Alt %").text = "\u2030"

        layout.separator()

        layout.operator("font.text_insert", text="Dutch Florin|Alt F").text = "\u00A4"
        layout.operator("font.text_insert", text="British Pound|Alt L").text = "\u00A3"
        layout.operator("font.text_insert", text="Japanese Yen|Alt Y").text = "\u00A5"

        layout.separator()

        layout.operator("font.text_insert", text="German S|Alt S").text = "\u00DF"
        layout.operator("font.text_insert", text="Spanish Question Mark|Alt ?").text = "\u00BF"
        layout.operator("font.text_insert", text="Spanish Exclamation Mark|Alt !").text = "\u00A1"


class VIEW3D_MT_edit_meta(Menu):
    bl_label = "Metaball"

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings

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

        layout.prop_menu_enum(settings, "proportional_edit")
        layout.prop_menu_enum(settings, "proportional_edit_falloff")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_meta_showhide")


class VIEW3D_MT_edit_meta_showhide(Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        layout = self.layout

        layout.operator("mball.reveal_metaelems", text="Show Hidden")
        layout.operator("mball.hide_metaelems", text="Hide Selected")
        layout.operator("mball.hide_metaelems", text="Hide Unselected").unselected = True


class VIEW3D_MT_edit_lattice(Menu):
    bl_label = "Lattice"

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("lattice.make_regular")

        layout.separator()

        layout.prop_menu_enum(settings, "proportional_edit")
        layout.prop_menu_enum(settings, "proportional_edit_falloff")


class VIEW3D_MT_edit_armature(Menu):
    bl_label = "Armature"

    def draw(self, context):
        layout = self.layout

        edit_object = context.edit_object
        arm = edit_object.data

        layout.menu("VIEW3D_MT_transform")
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
        col.active = view.region_3d.view_perspective != 'CAMERA'
        col.prop(view, "lens")
        col.label(text="Lock to Object:")
        col.prop(view, "lock_object", text="")
        lock_object = view.lock_object
        if lock_object:
            if lock_object.type == 'ARMATURE':
                col.prop_search(view, "lock_bone", lock_object.data, "edit_bones" if lock_object.mode == 'EDIT' else "bones", text="")
        else:
            col.prop(view, "lock_cursor", text="Lock to Cursor")

        col = layout.column()
        col.prop(view, "lock_camera")

        col = layout.column(align=True)
        col.label(text="Clip:")
        col.prop(view, "clip_start", text="Start")
        col.prop(view, "clip_end", text="End")

        subcol = col.column()
        subcol.enabled = not view.lock_camera_and_layers
        subcol.label(text="Local Camera:")
        subcol.prop(view, "camera", text="")


class VIEW3D_PT_view3d_cursor(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "3D Cursor"

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return (view)

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
        gs = scene.game_settings
        ob = context.object

        col = layout.column()
        col.prop(view, "show_only_render")

        col = layout.column()
        display_all = not view.show_only_render
        col.active = display_all
        col.prop(view, "show_outline_selected")
        col.prop(view, "show_all_objects_origin")
        col.prop(view, "show_relationship_lines")
        if ob and ob.type == 'MESH':
            mesh = ob.data
            col.prop(mesh, "show_all_edges")

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

        if not scene.render.use_shading_nodes:
            col = layout.column()
            col.label(text="Shading:")
            col.prop(gs, "material_mode", text="")
            col.prop(view, "show_textured_solid")

        layout.separator()

        region = view.region_quadview

        layout.operator("screen.region_quadview", text="Toggle Quad View")

        if region:
            col = layout.column()
            col.prop(region, "lock_rotation")
            row = col.row()
            row.enabled = region.lock_rotation
            row.prop(region, "show_sync_view")
            row = col.row()
            row.enabled = region.lock_rotation and region.show_sync_view
            row.prop(region, "use_box_clip")


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
        col.prop(view, "show_bundle_names")
        col.prop(view, "show_camera_path")
        col.label(text="Tracks:")
        col.prop(view, "tracks_draw_type", text="")
        col.prop(view, "tracks_draw_size", text="Size")


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

        mesh = context.active_object.data

        col = layout.column()
        col.label(text="Overlays:")
        col.prop(mesh, "show_edges", text="Edges")
        col.prop(mesh, "show_faces", text="Faces")
        col.prop(mesh, "show_edge_crease", text="Creases")
        col.prop(mesh, "show_edge_bevel_weight", text="Bevel Weights")
        col.prop(mesh, "show_edge_seams", text="Seams")
        col.prop(mesh, "show_edge_sharp", text="Sharp")

        col.separator()
        col.label(text="Normals:")
        col.prop(mesh, "show_normal_face", text="Face")
        col.prop(mesh, "show_normal_vertex", text="Vertex")
        col.prop(context.scene.tool_settings, "normal_size", text="Normal Size")

        col.separator()
        col.label(text="Numerics:")
        col.prop(mesh, "show_extra_edge_length")
        col.prop(mesh, "show_extra_face_angle")
        col.prop(mesh, "show_extra_face_area")
        if bpy.app.debug:
            col.prop(mesh, "show_extra_indices")


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
        col.label(text="Overlays:")
        col.prop(curve, "show_handles", text="Handles")
        col.prop(curve, "show_normal_face", text="Normals")
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
                    if (bg.image):
                        box.template_image(bg, "image", bg.image_user, compact=True)
                        has_bg = True

                elif bg.source == 'MOVIE_CLIP':
                    box.prop(bg, 'use_camera_clip')

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
                    box.prop(bg, "opacity", slider=True)
                    if bg.view_axis != 'CAMERA':
                        box.prop(bg, "size")
                        row = box.row(align=True)
                        row.prop(bg, "offset_x", text="X")
                        row.prop(bg, "offset_y", text="Y")


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
            row.operator("transform.delete_orientation", text="", icon="X")


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
            col.prop(toolsettings, "use_etch_autoname")
            col.prop(toolsettings, "etch_number")
            col.prop(toolsettings, "etch_side")

        col.operator("sketch.convert", text="Convert")


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
        member = cls._active_context_member(context)
        if member:
            context_member = getattr(context, member)
            return context_member and context_member.keys()

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

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
