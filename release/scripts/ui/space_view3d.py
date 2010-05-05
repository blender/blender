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


class VIEW3D_HT_header(bpy.types.Header):
    bl_space_type = 'VIEW_3D'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row()
        row.template_header()

        sub = row.row(align=True)

        # Menus
        if context.area.show_menus:

            sub.menu("VIEW3D_MT_view")

            # Select Menu
            if mode_string not in ('EDIT_TEXT', 'SCULPT', 'PAINT_WEIGHT', 'PAINT_VERTEX', 'PAINT_TEXTURE'):
                sub.menu("VIEW3D_MT_select_%s" % mode_string.lower())

            if edit_object:
                sub.menu("VIEW3D_MT_edit_%s" % edit_object.type.lower())
            elif obj:
                if mode_string not in ('PAINT_TEXTURE'):
                    sub.menu("VIEW3D_MT_%s" % mode_string.lower())
            else:
                sub.menu("VIEW3D_MT_object")

        row.template_header_3D()

        # do in C for now since these buttons cant be both toggle AND exclusive.
        '''
        if obj and obj.mode == 'EDIT' and obj.type == 'MESH':
            row_sub = row.row(align=True)
            row_sub.prop(toolsettings, "mesh_selection_mode", text="", index=0, icon='VERTEXSEL')
            row_sub.prop(toolsettings, "mesh_selection_mode", text="", index=1, icon='EDGESEL')
            row_sub.prop(toolsettings, "mesh_selection_mode", text="", index=2, icon='FACESEL')
        '''

        if obj:
            # Particle edit
            if obj.mode == 'PARTICLE_EDIT':
                row.prop(toolsettings.particle_edit, "selection_mode", text="", expand=True, toggle=True)

            # Occlude geometry
            if view.viewport_shading in ('SOLID', 'SHADED', 'TEXTURED') and (obj.mode == 'PARTICLE_EDIT' or (obj.mode == 'EDIT' and obj.type == 'MESH')):
                row.prop(view, "occlude_geometry", text="")

            # Proportional editing
            if obj.mode in ('OBJECT', 'EDIT', 'PARTICLE_EDIT'):
                row = layout.row(align=True)
                row.prop(toolsettings, "proportional_editing", text="", icon_only=True)
                if toolsettings.proportional_editing != 'DISABLED':
                    row.prop(toolsettings, "proportional_editing_falloff", text="", icon_only=True)

        # Snap
        row = layout.row(align=True)
        row.prop(toolsettings, "snap", text="")
        row.prop(toolsettings, "snap_element", text="", icon_only=True)
        if toolsettings.snap_element != 'INCREMENT':
            row.prop(toolsettings, "snap_target", text="")
            if obj and obj.mode == 'OBJECT':
                row.prop(toolsettings, "snap_align_rotation", text="")
        if toolsettings.snap_element == 'VOLUME':
            row.prop(toolsettings, "snap_peel_object", text="")
        elif toolsettings.snap_element == 'FACE':
            row.prop(toolsettings, "snap_project", text="")

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


class VIEW3D_MT_showhide(bpy.types.Menu):
    bl_label = "Show/Hide"
    _operator_name = ""

    def draw(self, context):
        layout = self.layout

        layout.operator("%s.reveal" % self._operator_name, text="Show Hidden")
        layout.operator("%s.hide" % self._operator_name, text="Hide Selected")
        layout.operator("%s.hide" % self._operator_name, text="Hide Unselected").unselected = True


class VIEW3D_MT_transform(bpy.types.Menu):
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
        if context.edit_object and context.edit_object.type == 'ARMATURE':
            layout.operator("armature.align")
        else:
            layout.operator_context = 'EXEC_REGION_WIN'
            layout.operator("transform.transform", text="Align to Transform Orientation").mode = 'ALIGN' # XXX see alignmenu() in edit.c of b2.4x to get this working

        layout.separator()

        layout.operator_context = 'EXEC_AREA'

        layout.operator("object.origin_set", text="Geometry to Origin").type = 'GEOMETRY_ORIGIN'
        layout.operator("object.origin_set", text="Origin to Geometry").type = 'ORIGIN_GEOMETRY'
        layout.operator("object.origin_set", text="Origin to 3D Cursor").type = 'ORIGIN_CURSOR'


class VIEW3D_MT_mirror(bpy.types.Menu):
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


class VIEW3D_MT_snap(bpy.types.Menu):
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


class VIEW3D_MT_uv_map(bpy.types.Menu):
    bl_label = "UV Mapping"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.unwrap")
        layout.operator("uv.cube_project")
        layout.operator("uv.cylinder_project")
        layout.operator("uv.sphere_project")
        layout.operator("uv.project_from_view")
        layout.operator("uv.project_from_view", text="Project from View (Bounds)").scale_to_bounds = True

        layout.separator()

        layout.operator("uv.reset")

# ********** View menus **********


class VIEW3D_MT_view(bpy.types.Menu):
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


class VIEW3D_MT_view_navigation(bpy.types.Menu):
    bl_label = "Navigation"

    def draw(self, context):
        layout = self.layout

        layout.operator_enums("view3d.view_orbit", "type")

        layout.separator()

        layout.operator_enums("view3d.view_pan", "type")

        layout.separator()

        layout.operator("view3d.zoom", text="Zoom In").delta = 1
        layout.operator("view3d.zoom", text="Zoom Out").delta = -1

        layout.separator()

        layout.operator("view3d.fly")


class VIEW3D_MT_view_align(bpy.types.Menu):
    bl_label = "Align View"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_view_align_selected")

        layout.separator()

        layout.operator("view3d.view_all", text="Center Cursor and View All").center = True
        layout.operator("view3d.camera_to_view", text="Align Active Camera to View")
        layout.operator("view3d.view_selected")
        layout.operator("view3d.view_center_cursor")


class VIEW3D_MT_view_align_selected(bpy.types.Menu):
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


class VIEW3D_MT_view_cameras(bpy.types.Menu):
    bl_label = "Cameras"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.object_as_camera")
        layout.operator("view3d.viewnumpad", text="Active Camera").type = 'CAMERA'

# ********** Select menus, suffix from context.mode **********


class VIEW3D_MT_select_object(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("object.select_all", text="Select/Deselect All")
        layout.operator("object.select_inverse", text="Inverse")
        layout.operator("object.select_random", text="Random")
        layout.operator("object.select_mirror", text="Mirror")
        layout.operator("object.select_by_layer", text="Select All by Layer")
        layout.operator_menu_enum("object.select_by_type", "type", text="Select All by Type...")
        layout.operator("object.select_camera", text="Select Camera")

        layout.separator()

        layout.operator_menu_enum("object.select_grouped", "type", text="Grouped")
        layout.operator_menu_enum("object.select_linked", "type", text="Linked")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_pose(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("pose.select_all", text="Select/Deselect All")
        layout.operator("pose.select_inverse", text="Inverse")
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


class VIEW3D_MT_select_particle(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("particle.select_all", text="Select/Deselect All")
        layout.operator("particle.select_linked")
        layout.operator("particle.select_inverse")

        layout.separator()

        layout.operator("particle.select_more")
        layout.operator("particle.select_less")

        layout.separator()

        layout.operator("particle.select_roots", text="Roots")
        layout.operator("particle.select_tips", text="Tips")


class VIEW3D_MT_select_edit_mesh(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("mesh.select_all", text="Select/Deselect All")
        layout.operator("mesh.select_inverse", text="Inverse")

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
        if context.scene.tool_settings.mesh_selection_mode[2] == False:
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
        layout.operator("mesh.loop_multi_select", text="Edge Loop")
        layout.operator("mesh.loop_multi_select", text="Edge Ring").ring = True

        layout.separator()

        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_select_edit_curve(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_all", text="Select/Deselect All")
        layout.operator("curve.select_inverse")
        layout.operator("curve.select_random")
        layout.operator("curve.select_every_nth")

        layout.separator()

        layout.operator("curve.de_select_first")
        layout.operator("curve.de_select_last")
        layout.operator("curve.select_next")
        layout.operator("curve.select_previous")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_select_edit_surface(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_all", text="Select/Deselect All")
        layout.operator("curve.select_inverse")
        layout.operator("curve.select_random")
        layout.operator("curve.select_every_nth")

        layout.separator()

        layout.operator("curve.select_row")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_select_edit_metaball(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("mball.select_deselect_all_metaelems")
        layout.operator("mball.select_inverse_metaelems")

        layout.separator()

        layout.operator("mball.select_random_metaelems")


class VIEW3D_MT_select_edit_lattice(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("lattice.select_all", text="Select/Deselect All")


class VIEW3D_MT_select_edit_armature(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")


        layout.separator()

        layout.operator("armature.select_all", text="Select/Deselect All")
        layout.operator("armature.select_inverse", text="Inverse")

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


class VIEW3D_MT_select_face(bpy.types.Menu):# XXX no matching enum
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        # TODO
        # see view3d_select_faceselmenu

# ********** Object menu **********


class VIEW3D_MT_object(bpy.types.Menu):
    bl_context = "objectmode"
    bl_label = "Object"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_object_clear")
        layout.menu("VIEW3D_MT_object_apply")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("anim.keyframe_insert_menu", text="Insert Keyframe...")
        layout.operator("anim.keyframe_delete_v3d", text="Delete Keyframe...")
        layout.operator("anim.keying_set_active_set", text="Change Keying Set...")

        layout.separator()

        layout.operator("object.duplicate_move")
        layout.operator("object.duplicate_move_linked")
        layout.operator("object.delete", text="Delete...")
        layout.operator("object.proxy_make", text="Make Proxy...")
        layout.menu("VIEW3D_MT_make_links", text="Make Links...")
        layout.operator("object.make_dupli_face", text="Make Dupliface...")
        layout.operator_menu_enum("object.make_local", "type", text="Make Local...")
        layout.menu("VIEW3D_MT_make_single_user")

        layout.separator()

        layout.menu("VIEW3D_MT_object_parent")
        layout.menu("VIEW3D_MT_object_track")
        layout.menu("VIEW3D_MT_object_group")
        layout.menu("VIEW3D_MT_object_constraints")

        layout.separator()

        layout.operator("object.join_uvs")
        layout.operator("object.join")

        layout.separator()

        layout.operator("object.move_to_layer", text="Move to Layer...")
        layout.menu("VIEW3D_MT_object_showhide")

        layout.operator_menu_enum("object.convert", "target")


class VIEW3D_MT_object_clear(bpy.types.Menu):
    bl_label = "Clear"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.location_clear", text="Location")
        layout.operator("object.rotation_clear", text="Rotation")
        layout.operator("object.scale_clear", text="Scale")
        layout.operator("object.origin_clear", text="Origin")


class VIEW3D_MT_object_specials(bpy.types.Menu):
    bl_label = "Specials"

    def poll(self, context):
        # add more special types
        obj = context.object
        return bool(obj and obj.type == 'LAMP')

    def draw(self, context):
        layout = self.layout

        obj = context.object
        if obj and obj.type == 'LAMP':
            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Spot Size")
            props.path_iter = "selected_editable_objects"
            props.path_item = "data.spot_size"
            props.input_scale = 0.01

            props = layout.operator("wm.context_modal_mouse", text="Distance")
            props.path_iter = "selected_editable_objects"
            props.path_item = "data.distance"
            props.input_scale = 0.1

            props = layout.operator("wm.context_modal_mouse", text="Clip Start")
            props.path_iter = "selected_editable_objects"
            props.path_item = "data.shadow_buffer_clip_start"
            props.input_scale = 0.05

            props = layout.operator("wm.context_modal_mouse", text="Clip End")
            props.path_iter = "selected_editable_objects"
            props.path_item = "data.shadow_buffer_clip_end"
            props.input_scale = 0.05


class VIEW3D_MT_object_apply(bpy.types.Menu):
    bl_label = "Apply"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.location_apply", text="Location")
        layout.operator("object.rotation_apply", text="Rotation")
        layout.operator("object.scale_apply", text="Scale")
        layout.separator()
        layout.operator("object.visual_transform_apply", text="Visual Transform")
        layout.operator("object.duplicates_make_real")


class VIEW3D_MT_object_parent(bpy.types.Menu):
    bl_label = "Parent"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.parent_set", text="Set")
        layout.operator("object.parent_clear", text="Clear")


class VIEW3D_MT_object_track(bpy.types.Menu):
    bl_label = "Track"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.track_set", text="Set")
        layout.operator("object.track_clear", text="Clear")


class VIEW3D_MT_object_group(bpy.types.Menu):
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout

        layout.operator("group.create")
        layout.operator("group.objects_remove")

        layout.separator()

        layout.operator("group.objects_add_active")
        layout.operator("group.objects_remove_active")


class VIEW3D_MT_object_constraints(bpy.types.Menu):
    bl_label = "Constraints"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.constraint_add_with_targets")
        layout.operator("object.constraints_clear")


class VIEW3D_MT_object_showhide(bpy.types.Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.restrictview_clear", text="Show Hidden")
        layout.operator("object.restrictview_set", text="Hide Selected")
        layout.operator("object.restrictview_set", text="Hide Unselected").unselected = True


class VIEW3D_MT_make_single_user(bpy.types.Menu):
    bl_label = "Make Single User"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("object.make_single_user", text="Object")
        props.object = True

        props = layout.operator("object.make_single_user", text="Object & ObData")
        props.object = props.obdata = True

        props = layout.operator("object.make_single_user", text="Object & ObData & Materials+Tex")
        props.object = props.obdata = props.material = props.texture = True

        props = layout.operator("object.make_single_user", text="Materials+Tex")
        props.material = props.texture = True

        props = layout.operator("object.make_single_user", text="Animation")
        props.animation = True


class VIEW3D_MT_make_links(bpy.types.Menu):
    bl_label = "Make Links"

    def draw(self, context):
        layout = self.layout

        layout.operator_menu_enum("object.make_links_scene", "type", text="Objects to Scene...")
        layout.operator_menu_enum("marker.make_links_scene", "type", text="Markers to Scene...")
        layout.operator_enums("object.make_links_data", "type") # inline


# ********** Vertex paint menu **********


class VIEW3D_MT_paint_vertex(bpy.types.Menu):
    bl_label = "Paint"

    def draw(self, context):
        layout = self.layout

        layout.operator("paint.vertex_color_set")
        layout.operator("paint.vertex_color_dirt")


class VIEW3D_MT_hook(bpy.types.Menu):
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


class VIEW3D_MT_vertex_group(bpy.types.Menu):
    bl_label = "Vertex Groups"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.vertex_group_assign", text="Assign to New Group").new = True

        ob = context.active_object
        if ob.mode == 'EDIT':
            if ob.vertex_groups and ob.active_vertex_group:
                layout.separator()
                layout.operator("object.vertex_group_assign", text="Assign to Active Group")
                layout.operator("object.vertex_group_remove_from", text="Remove from Active Group")
                layout.operator("object.vertex_group_remove_from", text="Remove from All").all = True
                layout.separator()

        if ob.vertex_groups and ob.active_vertex_group:
            layout.operator_menu_enum("object.vertex_group_set_active", "group", text="Set Active Group")
            layout.operator("object.vertex_group_remove", text="Remove Active Group")
            layout.operator("object.vertex_group_remove", text="Remove All Groups").all = True

# ********** Weight paint menu **********


class VIEW3D_MT_paint_weight(bpy.types.Menu):
    bl_label = "Weights"

    def draw(self, context):
        layout = self.layout

        layout.operator("paint.weight_from_bones", text="Assign Automatic From Bones").type = 'AUTOMATIC'
        layout.operator("paint.weight_from_bones", text="Assign From Bone Envelopes").type = 'ENVELOPES'

        layout.separator()

        layout.operator("object.vertex_group_normalize_all", text="Normalize All")
        layout.operator("object.vertex_group_normalize", text="Normalize")
        layout.operator("object.vertex_group_invert", text="Invert")
        layout.operator("object.vertex_group_clean", text="Clean")
        layout.operator("object.vertex_group_levels", text="Levels")

# ********** Sculpt menu **********


class VIEW3D_MT_sculpt(bpy.types.Menu):
    bl_label = "Sculpt"

    def draw(self, context):
        layout = self.layout

        sculpt = context.tool_settings.sculpt
        brush = context.tool_settings.sculpt.brush

        layout.prop(sculpt, "symmetry_x")
        layout.prop(sculpt, "symmetry_y")
        layout.prop(sculpt, "symmetry_z")
        layout.separator()
        layout.prop(sculpt, "lock_x")
        layout.prop(sculpt, "lock_y")
        layout.prop(sculpt, "lock_z")
        layout.separator()
        layout.operator_menu_enum("brush.curve_preset", "shape")
        layout.separator()

        sculpt_tool = brush.sculpt_tool

        if sculpt_tool != 'GRAB':
            layout.prop(brush, "use_airbrush")

            if sculpt_tool != 'LAYER':
                layout.prop(brush, "use_anchor")

            if sculpt_tool in ('DRAW', 'PINCH', 'INFLATE', 'LAYER', 'CLAY'):
                layout.prop(brush, "flip_direction")

            if sculpt_tool == 'LAYER':
                layout.prop(brush, "use_persistent")
                layout.operator("sculpt.set_persistent_base")

# ********** Particle menu **********


class VIEW3D_MT_particle(bpy.types.Menu):
    bl_label = "Particle"

    def draw(self, context):
        layout = self.layout

        particle_edit = context.tool_settings.particle_edit

        layout.operator("particle.mirror")

        layout.separator()

        layout.operator("particle.remove_doubles")
        layout.operator("particle.delete")

        if particle_edit.selection_mode == 'POINT':
            layout.operator("particle.subdivide")

        layout.operator("particle.rekey")
        layout.operator("particle.weight_set")

        layout.separator()

        layout.menu("VIEW3D_MT_particle_showhide")


class VIEW3D_MT_particle_specials(bpy.types.Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout
        particle_edit = context.tool_settings.particle_edit

        layout.operator("particle.rekey")

        layout.separator()
        if particle_edit.selection_mode == 'POINT':
            layout.operator("particle.subdivide")
            layout.operator("particle.select_roots")
            layout.operator("particle.select_tips")

        layout.operator("particle.remove_doubles")


class VIEW3D_MT_particle_showhide(VIEW3D_MT_showhide):
    _operator_name = "particle"

# ********** Pose Menu **********


class VIEW3D_MT_pose(bpy.types.Menu):
    bl_label = "Pose"

    def draw(self, context):
        layout = self.layout

        arm = context.active_object.data

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_snap")
        if arm.drawtype in ('BBONE', 'ENVELOPE'):
            layout.operator("transform.transform", text="Scale Envelope Distance").mode = 'BONESIZE'

        layout.menu("VIEW3D_MT_pose_transform")

        layout.separator()

        layout.operator("anim.keyframe_insert_menu", text="Insert Keyframe...")
        layout.operator("anim.keyframe_delete_v3d", text="Delete Keyframe...")
        layout.operator("anim.keying_set_active_set", text="Change Keying Set...")

        layout.separator()

        layout.operator("pose.relax")

        layout.separator()

        layout.menu("VIEW3D_MT_pose_apply")

        layout.separator()

        layout.operator("pose.copy")
        layout.operator("pose.paste")
        layout.operator("pose.paste", text="Paste X-Flipped Pose").flipped = True

        layout.separator()

        layout.menu("VIEW3D_MT_pose_pose")
        layout.menu("VIEW3D_MT_pose_motion")
        layout.menu("VIEW3D_MT_pose_group")

        layout.separator()

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
        layout.operator_menu_enum("pose.flags_set", 'mode', text="Bone Settings")


class VIEW3D_MT_pose_transform(bpy.types.Menu):
    bl_label = "Clear Transform"

    def draw(self, context):
        layout = self.layout

        layout.label(text="User Transform")

        layout.operator("pose.loc_clear", text="Location")
        layout.operator("pose.rot_clear", text="Rotation")
        layout.operator("pose.scale_clear", text="Scale")

        layout.label(text="Origin")


class VIEW3D_MT_pose_pose(bpy.types.Menu):
    bl_label = "Pose Library"

    def draw(self, context):
        layout = self.layout

        layout.operator("poselib.browse_interactive", text="Browse Poses...")

        layout.separator()

        layout.operator("poselib.pose_add", text="Add Pose...")
        layout.operator("poselib.pose_rename", text="Rename Pose...")
        layout.operator("poselib.pose_remove", text="Remove Pose...")


class VIEW3D_MT_pose_motion(bpy.types.Menu):
    bl_label = "Motion Paths"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.paths_calculate", text="Calculate")
        layout.operator("pose.paths_clear", text="Clear")


class VIEW3D_MT_pose_group(bpy.types.Menu):
    bl_label = "Bone Groups"

    def draw(self, context):
        layout = self.layout
        layout.operator("pose.group_add")
        layout.operator("pose.group_remove")

        layout.separator()

        layout.operator("pose.group_assign")
        layout.operator("pose.group_unassign")


class VIEW3D_MT_pose_ik(bpy.types.Menu):
    bl_label = "Inverse Kinematics"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.ik_add")
        layout.operator("pose.ik_clear")


class VIEW3D_MT_pose_constraints(bpy.types.Menu):
    bl_label = "Constraints"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.constraint_add_with_targets", text="Add (With Targets)...")
        layout.operator("pose.constraints_clear")


class VIEW3D_MT_pose_showhide(VIEW3D_MT_showhide):
    _operator_name = "pose"


class VIEW3D_MT_pose_apply(bpy.types.Menu):
    bl_label = "Apply"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.armature_apply")
        layout.operator("pose.visual_transform_apply")


# ********** Edit Menus, suffix from ob.type **********


class VIEW3D_MT_edit_mesh(bpy.types.Menu):
    bl_label = "Mesh"

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings

        layout.operator("ed.undo")
        layout.operator("ed.redo")

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

        layout.prop(settings, "automerge_editing")
        layout.prop_menu_enum(settings, "proportional_editing")
        layout.prop_menu_enum(settings, "proportional_editing_falloff")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_showhide")


class VIEW3D_MT_edit_mesh_specials(bpy.types.Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.subdivide", text="Subdivide")
        layout.operator("mesh.subdivide", text="Subdivide Smooth").smoothness = 1.0
        layout.operator("mesh.merge", text="Merge...")
        layout.operator("mesh.remove_doubles")
        layout.operator("mesh.hide", text="Hide")
        layout.operator("mesh.reveal", text="Reveal")
        layout.operator("mesh.select_inverse")
        layout.operator("mesh.flip_normals")
        layout.operator("mesh.vertices_smooth", text="Smooth")
        # layout.operator("mesh.bevel", text="Bevel")
        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")
        layout.operator("mesh.blend_from_shape")
        layout.operator("mesh.shape_propagate_to_all")
        layout.operator("mesh.select_vertex_path")


class VIEW3D_MT_edit_mesh_selection_mode(bpy.types.Menu):
    bl_label = "Mesh Select Mode"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        prop = layout.operator("wm.context_set_value", text="Vertex", icon='VERTEXSEL')
        prop.value = "(True, False, False)"
        prop.path = "tool_settings.mesh_selection_mode"

        prop = layout.operator("wm.context_set_value", text="Edge", icon='EDGESEL')
        prop.value = "(False, True, False)"
        prop.path = "tool_settings.mesh_selection_mode"

        prop = layout.operator("wm.context_set_value", text="Face", icon='FACESEL')
        prop.value = "(False, False, True)"
        prop.path = "tool_settings.mesh_selection_mode"


class VIEW3D_MT_edit_mesh_extrude(bpy.types.Menu):
    bl_label = "Extrude"

    @staticmethod
    def extrude_options(context):
        mesh = context.object.data
        selection_mode = context.tool_settings.mesh_selection_mode

        totface = mesh.total_face_sel
        totedge = mesh.total_edge_sel
        totvert = mesh.total_vert_sel

        # the following is dependent on selection modes
        # we don't really want that
#        if selection_mode[0]: # vert
#            if totvert == 0:
#                return ()
#            elif totvert == 1:
#                return (3,)
#            elif totedge == 0:
#                return (3,)
#            elif totface == 0:
#                return (2, 3)
#            elif totface == 1:
#                return (0, 2, 3)
#            else:
#                return (0, 1, 2, 3)
#        elif selection_mode[1]: # edge
#            if totedge == 0:
#                return ()
#            elif totedge == 1:
#                return (2,)
#            elif totface == 0:
#                return (2,)
#            elif totface == 1:
#                return (0, 2)
#            else:
#                return (0, 1, 2)
#        elif selection_mode[2]: # face
#            if totface == 0:
#                return ()
#            elif totface == 1:
#                return (0,)
#            else:
#                return (0, 1)

        if totvert == 0:
            return ()
        elif totedge == 0:
            return (0, 3)
        elif totface == 0:
            return (0, 2, 3)
        else:
            return (0, 1, 2, 3)

        # should never get here
        return ()

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        def region_menu():
            layout.operator("view3d.edit_mesh_extrude_move_normal", text="Region")

        def face_menu():
            layout.operator("mesh.extrude_faces_move", text="Individual Faces")

        def edge_menu():
            layout.operator("mesh.extrude_edges_move", text="Edges Only")

        def vert_menu():
            layout.operator("mesh.extrude_vertices_move", text="Vertices Only")

        menu_funcs = region_menu, face_menu, edge_menu, vert_menu

        for i in self.extrude_options(context):
            func = menu_funcs[i]
            func()


class VIEW3D_OT_edit_mesh_extrude_individual_move(bpy.types.Operator):
    "Extrude individual elements and move"
    bl_label = "Extrude Individual and Move"
    bl_idname = "view3d.edit_mesh_extrude_individual_move"

    def execute(self, context):
        mesh = context.object.data
        selection_mode = context.tool_settings.mesh_selection_mode

        totface = mesh.total_face_sel
        totedge = mesh.total_edge_sel
        totvert = mesh.total_vert_sel

        if selection_mode[2] and totface == 1:
            return bpy.ops.mesh.extrude_region_move('INVOKE_REGION_WIN', TRANSFORM_OT_translate={"constraint_orientation": "NORMAL", "constraint_axis": [False, False, True]})
        elif selection_mode[2] and totface > 1:
            return bpy.ops.mesh.extrude_faces_move('INVOKE_REGION_WIN')
        elif selection_mode[1] and totedge >= 1:
            return bpy.ops.mesh.extrude_edges_move('INVOKE_REGION_WIN')
        else:
            return bpy.ops.mesh.extrude_vertices_move('INVOKE_REGION_WIN')

    def invoke(self, context, event):
        return self.execute(context)


class VIEW3D_OT_edit_mesh_extrude_move(bpy.types.Operator):
    "Extrude and move along normals"
    bl_label = "Extrude and Move on Normals"
    bl_idname = "view3d.edit_mesh_extrude_move_normal"

    def execute(self, context):
        mesh = context.object.data

        totface = mesh.total_face_sel
        totedge = mesh.total_edge_sel
        totvert = mesh.total_vert_sel

        if totface >= 1:
            return bpy.ops.mesh.extrude_region_move('INVOKE_REGION_WIN', TRANSFORM_OT_translate={"constraint_orientation": "NORMAL", "constraint_axis": [False, False, True]})
        elif totedge == 1:
            return bpy.ops.mesh.extrude_region_move('INVOKE_REGION_WIN', TRANSFORM_OT_translate={"constraint_orientation": "NORMAL", "constraint_axis": [True, True, False]})
        else:
            return bpy.ops.mesh.extrude_region_move('INVOKE_REGION_WIN')

    def invoke(self, context, event):
        return self.execute(context)


class VIEW3D_MT_edit_mesh_vertices(bpy.types.Menu):
    bl_label = "Vertices"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.merge")
        layout.operator("mesh.rip")
        layout.operator("mesh.split")
        layout.operator("mesh.separate")

        layout.separator()

        layout.operator("mesh.vertices_smooth")
        layout.operator("mesh.remove_doubles")

        layout.operator("mesh.select_vertex_path")

        layout.operator("mesh.blend_from_shape")

        layout.operator("object.vertex_group_blend")
        layout.operator("mesh.shape_propagate_to_all")

        layout.separator()

        layout.menu("VIEW3D_MT_vertex_group")
        layout.menu("VIEW3D_MT_hook")


class VIEW3D_MT_edit_mesh_edges(bpy.types.Menu):
    bl_label = "Edges"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.edge_face_add")
        layout.operator("mesh.subdivide")

        layout.separator()

        layout.operator("mesh.mark_seam")
        layout.operator("mesh.mark_seam", text="Clear Seam").clear = True

        layout.separator()

        layout.operator("mesh.mark_sharp")
        layout.operator("mesh.mark_sharp", text="Clear Sharp").clear = True

        layout.separator()

        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").direction = 'CW'
        layout.operator("mesh.edge_rotate", text="Rotate Edge CCW").direction = 'CCW'

        layout.separator()

        layout.operator("TRANSFORM_OT_edge_slide")
        layout.operator("TRANSFORM_OT_edge_crease")
        layout.operator("mesh.loop_multi_select", text="Edge Loop")

        # uiItemO(layout, "Loopcut", 0, "mesh.loop_cut"); // CutEdgeloop(em, 1);
        # uiItemO(layout, "Edge Slide", 0, "mesh.edge_slide"); // EdgeSlide(em, 0,0.0);

        layout.operator("mesh.loop_multi_select", text="Edge Ring").ring = True

        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_edit_mesh_faces(bpy.types.Menu):
    bl_label = "Faces"
    bl_idname = "VIEW3D_MT_edit_mesh_faces"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        # layout.operator("mesh.bevel")
        # layout.operator("mesh.bevel")
        layout.operator("mesh.edge_face_add")
        layout.operator("mesh.fill")
        layout.operator("mesh.beautify_fill")
        layout.operator("mesh.solidify")

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


class VIEW3D_MT_edit_mesh_normals(bpy.types.Menu):
    bl_label = "Normals"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.normals_make_consistent", text="Recalculate Outside")
        layout.operator("mesh.normals_make_consistent", text="Recalculate Inside").inside = True

        layout.separator()

        layout.operator("mesh.flip_normals")


class VIEW3D_MT_edit_mesh_showhide(VIEW3D_MT_showhide):
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

    layout.prop_menu_enum(settings, "proportional_editing")
    layout.prop_menu_enum(settings, "proportional_editing_falloff")

    layout.separator()

    layout.menu("VIEW3D_MT_edit_curve_showhide")


class VIEW3D_MT_edit_curve(bpy.types.Menu):
    bl_label = "Curve"

    draw = draw_curve


class VIEW3D_MT_edit_curve_ctrlpoints(bpy.types.Menu):
    bl_label = "Control Points"

    def draw(self, context):
        layout = self.layout

        edit_object = context.edit_object

        if edit_object.type == 'CURVE':
            layout.operator("transform.transform").mode = 'TILT'
            layout.operator("curve.tilt_clear")
            layout.operator("curve.separate")

            layout.separator()

            layout.operator_menu_enum("curve.handle_type_set", "type")

            layout.separator()

            layout.menu("VIEW3D_MT_hook")


class VIEW3D_MT_edit_curve_segments(bpy.types.Menu):
    bl_label = "Segments"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.subdivide")
        layout.operator("curve.switch_direction")


class VIEW3D_MT_edit_curve_specials(bpy.types.Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("curve.subdivide")
        layout.operator("curve.switch_direction")
        layout.operator("curve.spline_weight_set")
        layout.operator("curve.radius_set")
        layout.operator("curve.smooth")
        layout.operator("curve.smooth_radius")


class VIEW3D_MT_edit_curve_showhide(VIEW3D_MT_showhide):
    _operator_name = "curve"


class VIEW3D_MT_edit_surface(bpy.types.Menu):
    bl_label = "Surface"

    draw = draw_curve


class VIEW3D_MT_edit_text(bpy.types.Menu):
    bl_label = "Text"

    def draw(self, context):
        layout = self.layout

        layout.operator("font.file_paste")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_text_chars")


class VIEW3D_MT_edit_text_chars(bpy.types.Menu):
    bl_label = "Special Characters"

    def draw(self, context):
        layout = self.layout

        layout.operator("font.text_insert", text="Copyright|Alt C").text = b'\xC2\xA9'.decode()
        layout.operator("font.text_insert", text="Registered Trademark|Alt R").text = b'\xC2\xAE'.decode()

        layout.separator()

        layout.operator("font.text_insert", text="Degree Sign|Alt G").text = b'\xC2\xB0'.decode()
        layout.operator("font.text_insert", text="Multiplication Sign|Alt x").text = b'\xC3\x97'.decode()
        layout.operator("font.text_insert", text="Circle|Alt .").text = b'\xC2\x8A'.decode()
        layout.operator("font.text_insert", text="Superscript 1|Alt 1").text = b'\xC2\xB9'.decode()
        layout.operator("font.text_insert", text="Superscript 2|Alt 2").text = b'\xC2\xB2'.decode()
        layout.operator("font.text_insert", text="Superscript 3|Alt 3").text = b'\xC2\xB3'.decode()
        layout.operator("font.text_insert", text="Double >>|Alt >").text = b'\xC2\xBB'.decode()
        layout.operator("font.text_insert", text="Double <<|Alt <").text = b'\xC2\xAB'.decode()
        layout.operator("font.text_insert", text="Promillage|Alt %").text = b'\xE2\x80\xB0'.decode()

        layout.separator()

        layout.operator("font.text_insert", text="Dutch Florin|Alt F").text = b'\xC2\xA4'.decode()
        layout.operator("font.text_insert", text="British Pound|Alt L").text = b'\xC2\xA3'.decode()
        layout.operator("font.text_insert", text="Japanese Yen|Alt Y").text = b'\xC2\xA5'.decode()

        layout.separator()

        layout.operator("font.text_insert", text="German S|Alt S").text = b'\xC3\x9F'.decode()
        layout.operator("font.text_insert", text="Spanish Question Mark|Alt ?").text = b'\xC2\xBF'.decode()
        layout.operator("font.text_insert", text="Spanish Exclamation Mark|Alt !").text = b'\xC2\xA1'.decode()


class VIEW3D_MT_edit_meta(bpy.types.Menu):
    bl_label = "Metaball"

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("mball.delete_metaelems", text="Delete...")
        layout.operator("mball.duplicate_metaelems")

        layout.separator()

        layout.prop_menu_enum(settings, "proportional_editing")
        layout.prop_menu_enum(settings, "proportional_editing_falloff")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_meta_showhide")


class VIEW3D_MT_edit_meta_showhide(bpy.types.Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        layout = self.layout

        layout.operator("mball.reveal_metaelems", text="Show Hidden")
        layout.operator("mball.hide_metaelems", text="Hide Selected")
        layout.operator("mball.hide_metaelems", text="Hide Unselected").unselected = True


class VIEW3D_MT_edit_lattice(bpy.types.Menu):
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

        layout.prop_menu_enum(settings, "proportional_editing")
        layout.prop_menu_enum(settings, "proportional_editing_falloff")


class VIEW3D_MT_edit_armature(bpy.types.Menu):
    bl_label = "Armature"

    def draw(self, context):
        layout = self.layout

        edit_object = context.edit_object
        arm = edit_object.data

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")
        layout.menu("VIEW3D_MT_edit_armature_roll")

        if arm.drawtype == 'ENVELOPE':
            layout.operator("transform.transform", text="Scale Envelope Distance").mode = 'BONESIZE'
        else:
            layout.operator("transform.transform", text="Scale B-Bone Width").mode = 'BONESIZE'

        layout.separator()

        layout.operator("armature.extrude_move")

        if arm.x_axis_mirror:
            layout.operator("armature.extrude_forked")

        layout.operator("armature.duplicate_move")
        layout.operator("armature.merge")
        layout.operator("armature.fill")
        layout.operator("armature.delete")
        layout.operator("armature.separate")

        layout.separator()

        layout.operator("armature.subdivide_multi", text="Subdivide")
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

        layout.operator_menu_enum("armature.flags_set", "mode", text="Bone Settings")


class VIEW3D_MT_armature_specials(bpy.types.Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("armature.subdivide_multi", text="Subdivide")
        layout.operator("armature.switch_direction", text="Switch Direction")

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("armature.autoside_names", text="AutoName Left/Right").type = 'XAXIS'
        layout.operator("armature.autoside_names", text="AutoName Front/Back").type = 'YAXIS'
        layout.operator("armature.autoside_names", text="AutoName Top/Bottom").type = 'ZAXIS'
        layout.operator("armature.flip_names", text="Flip Names")


class VIEW3D_MT_edit_armature_parent(bpy.types.Menu):
    bl_label = "Parent"

    def draw(self, context):
        layout = self.layout

        layout.operator("armature.parent_set", text="Make")
        layout.operator("armature.parent_clear", text="Clear")


class VIEW3D_MT_edit_armature_roll(bpy.types.Menu):
    bl_label = "Bone Roll"

    def draw(self, context):
        layout = self.layout

        layout.operator("armature.calculate_roll", text="Clear Roll (Z-Axis Up)").type = 'GLOBALUP'
        layout.operator("armature.calculate_roll", text="Roll to Cursor").type = 'CURSOR'

        layout.separator()

        layout.operator("transform.transform", text="Set Roll").mode = 'BONE_ROLL'

# ********** Panel **********


class VIEW3D_PT_view3d_properties(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "View"

    def poll(self, context):
        view = context.space_data
        return (view)

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        scene = context.scene

        col = layout.column()
        col.prop(view, "lens")
        col.label(text="Lock to Object:")
        col.prop(view, "lock_object", text="")
        if view.lock_object and view.lock_object.type == 'ARMATURE':
            col.prop_object(view, "lock_bone", view.lock_object.data, "bones", text="")

        col = layout.column(align=True)
        col.label(text="Clip:")
        col.prop(view, "clip_start", text="Start")
        col.prop(view, "clip_end", text="End")

        subcol = col.column()
        subcol.enabled = not view.lock_camera_and_layers
        subcol.label(text="Local Camera:")
        subcol.prop(view, "camera", text="")

        layout.column().prop(view, "cursor_location")


class VIEW3D_PT_view3d_name(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Item"

    def poll(self, context):
        return (context.space_data and context.active_object)

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        row = layout.row()
        row.label(text="", icon='OBJECT_DATA')
        row.prop(ob, "name", text="")

        if ob.type == 'ARMATURE' and ob.mode in ('EDIT', 'POSE'):
            bone = context.active_bone
            if bone:
                row = layout.row()
                row.label(text="", icon='BONE_DATA')
                row.prop(bone, "name", text="")


class VIEW3D_PT_view3d_display(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Display"
    bl_default_closed = True

    def poll(self, context):
        view = context.space_data
        return (view)

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        gs = context.scene.game_data
        ob = context.object

        col = layout.column()
        col.prop(view, "display_render_override")

        col = layout.column()
        display_all = not view.display_render_override
        col.active = display_all
        col.prop(view, "outline_selected")
        col.prop(view, "all_object_origins")
        col.prop(view, "relationship_lines")
        if ob and ob.type == 'MESH':
            mesh = ob.data
            col.prop(mesh, "all_edges")

        col = layout.column()
        col.active = display_all
        split = col.split(percentage=0.55)
        split.prop(view, "display_floor", text="Grid Floor")

        row = split.row(align=True)
        row.prop(view, "display_x_axis", text="X", toggle=True)
        row.prop(view, "display_y_axis", text="Y", toggle=True)
        row.prop(view, "display_z_axis", text="Z", toggle=True)

        sub = col.column(align=True)
        sub.active = (display_all and view.display_floor)
        sub.prop(view, "grid_lines", text="Lines")
        sub.prop(view, "grid_spacing", text="Spacing")
        sub.prop(view, "grid_subdivisions", text="Subdivisions")

        col = layout.column()
        col.label(text="Shading:")
        col.prop(gs, "material_mode", text="")
        col.prop(view, "textured_solid")

        layout.separator()

        region = view.region_quadview

        layout.operator("screen.region_quadview", text="Toggle Quad View")

        if region:
            col = layout.column()
            col.prop(region, "lock_rotation")
            row = col.row()
            row.enabled = region.lock_rotation
            row.prop(region, "box_preview")
            row = col.row()
            row.enabled = region.lock_rotation and region.box_preview
            row.prop(region, "box_clip")


class VIEW3D_PT_view3d_meshdisplay(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Mesh Display"

    def poll(self, context):
        # The active object check is needed because of localmode
        return (context.active_object and (context.mode == 'EDIT_MESH'))

    def draw(self, context):
        layout = self.layout

        mesh = context.active_object.data

        col = layout.column()
        col.label(text="Overlays:")
        col.prop(mesh, "draw_edges", text="Edges")
        col.prop(mesh, "draw_faces", text="Faces")
        col.prop(mesh, "draw_creases", text="Creases")
        col.prop(mesh, "draw_bevel_weights", text="Bevel Weights")
        col.prop(mesh, "draw_seams", text="Seams")
        col.prop(mesh, "draw_sharp", text="Sharp")

        col.separator()
        col.label(text="Normals:")
        col.prop(mesh, "draw_normals", text="Face")
        col.prop(mesh, "draw_vertex_normals", text="Vertex")
        col.prop(context.scene.tool_settings, "normal_size", text="Normal Size")

        col.separator()
        col.label(text="Numerics:")
        col.prop(mesh, "draw_edge_lenght")
        col.prop(mesh, "draw_edge_angle")
        col.prop(mesh, "draw_face_area")


class VIEW3D_PT_view3d_curvedisplay(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Curve Display"

    def poll(self, context):
        editmesh = context.mode == 'EDIT_CURVE'
        return (editmesh)

    def draw(self, context):
        layout = self.layout

        curve = context.active_object.data

        col = layout.column()
        col.label(text="Overlays:")
        col.prop(curve, "draw_handles", text="Handles")
        col.prop(curve, "draw_normals", text="Normals")
        col.prop(context.scene.tool_settings, "normal_size", text="Normal Size")


class VIEW3D_PT_background_image(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Background Images"
    bl_default_closed = True

    def poll(self, context):
        view = context.space_data
        # bg = context.space_data.background_image
        return (view)

    def draw_header(self, context):
        layout = self.layout
        view = context.space_data

        layout.prop(view, "display_background_images", text="")

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        col = layout.column()
        col.operator("view3d.add_background_image", text="Add Image")

        for i, bg in enumerate(view.background_images):
            layout.active = view.display_background_images
            box = layout.box()
            row = box.row(align=True)
            row.prop(bg, "show_expanded", text="", no_bg=True)
            row.label(text=getattr(bg.image, "name", "Not Set"))
            row.operator("view3d.remove_background_image", text="", icon='X').index = i

            box.prop(bg, "view_axis", text="Axis")

            if bg.show_expanded:
                row = box.row()
                row.template_ID(bg, "image", open="image.open")
                if (bg.image):
                    box.template_image(bg, "image", bg.image_user, compact=True)

                    box.prop(bg, "transparency", slider=True)
                    box.prop(bg, "size")
                    row = box.row(align=True)
                    row.prop(bg, "offset_x", text="X")
                    row.prop(bg, "offset_y", text="Y")


class VIEW3D_PT_transform_orientations(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Transform Orientations"
    bl_default_closed = True

    def poll(self, context):
        view = context.space_data
        return (view)

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        col = layout.column()

        col.prop(view, "transform_orientation")
        col.operator("transform.create_orientation", text="Create")

        orientation = view.current_orientation

        if orientation:
            col.prop(orientation, "name")
            col.operator("transform.delete_orientation", text="Delete")


class VIEW3D_PT_etch_a_ton(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Skeleton Sketching"
    bl_default_closed = True

    def poll(self, context):
        scene = context.space_data
        ob = context.active_object
        return scene and ob and ob.type == 'ARMATURE' and ob.mode == 'EDIT'

    def draw_header(self, context):
        layout = self.layout
        toolsettings = context.scene.tool_settings

        layout.prop(toolsettings, "bone_sketching", text="")

    def draw(self, context):
        layout = self.layout
        toolsettings = context.scene.tool_settings

        col = layout.column()

        col.prop(toolsettings, "etch_quick")
        col.prop(toolsettings, "etch_overdraw")

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
            col.prop(toolsettings, "etch_autoname")
            col.prop(toolsettings, "etch_number")
            col.prop(toolsettings, "etch_side")


class VIEW3D_PT_context_properties(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Properties"
    bl_default_closed = True

    def _active_context_member(self, context):
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

    def poll(self, context):
        member = self._active_context_member(context)
        if member:
            context_member = getattr(context, member)
            return context_member and context_member.keys()

        return False

    def draw(self, context):
        import rna_prop_ui
        # reload(rna_prop_ui)
        member = self._active_context_member(context)

        if member:
            # Draw with no edit button
            rna_prop_ui.draw(self.layout, context, member, False)

classes = [
    VIEW3D_OT_edit_mesh_extrude_move, # detects constraints setup and extrude region
    VIEW3D_OT_edit_mesh_extrude_individual_move,

    VIEW3D_HT_header, # Header

    VIEW3D_MT_view, #View Menus
    VIEW3D_MT_view_navigation,
    VIEW3D_MT_view_align,
    VIEW3D_MT_view_align_selected,
    VIEW3D_MT_view_cameras,

    VIEW3D_MT_select_object, # Select Menus
    VIEW3D_MT_select_pose,
    VIEW3D_MT_select_particle,
    VIEW3D_MT_select_edit_mesh,
    VIEW3D_MT_select_edit_curve,
    VIEW3D_MT_select_edit_surface,
    VIEW3D_MT_select_edit_metaball,
    VIEW3D_MT_select_edit_lattice,
    VIEW3D_MT_select_edit_armature,
    VIEW3D_MT_select_face, # XXX todo

    VIEW3D_MT_transform, # Object/Edit Menus
    VIEW3D_MT_mirror, # Object/Edit Menus
    VIEW3D_MT_snap, # Object/Edit Menus
    VIEW3D_MT_uv_map, # Edit Menus

    VIEW3D_MT_object, # Object Menu
    VIEW3D_MT_object_specials,
    VIEW3D_MT_object_apply,
    VIEW3D_MT_object_clear,
    VIEW3D_MT_object_parent,
    VIEW3D_MT_object_track,
    VIEW3D_MT_object_group,
    VIEW3D_MT_object_constraints,
    VIEW3D_MT_object_showhide,
    VIEW3D_MT_make_single_user,
    VIEW3D_MT_make_links,

    VIEW3D_MT_hook,
    VIEW3D_MT_vertex_group,

    VIEW3D_MT_sculpt, # Sculpt Menu
    VIEW3D_MT_paint_vertex,
    VIEW3D_MT_paint_weight,

    VIEW3D_MT_particle, # Particle Menu
    VIEW3D_MT_particle_specials,
    VIEW3D_MT_particle_showhide,

    VIEW3D_MT_pose, # POSE Menu
    VIEW3D_MT_pose_transform,
    VIEW3D_MT_pose_pose,
    VIEW3D_MT_pose_motion,
    VIEW3D_MT_pose_group,
    VIEW3D_MT_pose_ik,
    VIEW3D_MT_pose_constraints,
    VIEW3D_MT_pose_showhide,
    VIEW3D_MT_pose_apply,

    VIEW3D_MT_edit_mesh,
    VIEW3D_MT_edit_mesh_specials, # Only as a menu for keybindings
    VIEW3D_MT_edit_mesh_selection_mode, # Only as a menu for keybindings
    VIEW3D_MT_edit_mesh_vertices,
    VIEW3D_MT_edit_mesh_edges,
    VIEW3D_MT_edit_mesh_faces,
    VIEW3D_MT_edit_mesh_normals,
    VIEW3D_MT_edit_mesh_showhide,
    VIEW3D_MT_edit_mesh_extrude, # use with VIEW3D_OT_edit_mesh_extrude_menu

    VIEW3D_MT_edit_curve,
    VIEW3D_MT_edit_curve_ctrlpoints,
    VIEW3D_MT_edit_curve_segments,
    VIEW3D_MT_edit_curve_specials,
    VIEW3D_MT_edit_curve_showhide,

    VIEW3D_MT_edit_surface,

    VIEW3D_MT_edit_text,
    VIEW3D_MT_edit_text_chars,

    VIEW3D_MT_edit_meta,
    VIEW3D_MT_edit_meta_showhide,

    VIEW3D_MT_edit_lattice,

    VIEW3D_MT_edit_armature,
    VIEW3D_MT_edit_armature_parent,
    VIEW3D_MT_edit_armature_roll,

    VIEW3D_MT_armature_specials, # Only as a menu for keybindings

   # Panels
    VIEW3D_PT_view3d_properties,
    VIEW3D_PT_view3d_display,
    VIEW3D_PT_view3d_name,
    VIEW3D_PT_view3d_meshdisplay,
    VIEW3D_PT_view3d_curvedisplay,
    VIEW3D_PT_background_image,
    VIEW3D_PT_transform_orientations,
    VIEW3D_PT_etch_a_ton,
    VIEW3D_PT_context_properties]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
