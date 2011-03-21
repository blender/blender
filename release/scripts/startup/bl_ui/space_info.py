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


class INFO_HT_header(bpy.types.Header):
    bl_space_type = 'INFO'

    def draw(self, context):
        layout = self.layout

        window = context.window
        scene = context.scene
        rd = scene.render

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("INFO_MT_file")
            sub.menu("INFO_MT_add")
            if rd.use_game_engine:
                sub.menu("INFO_MT_game")
            else:
                sub.menu("INFO_MT_render")
            sub.menu("INFO_MT_help")

        if window.screen.show_fullscreen:
            layout.operator("screen.back_to_previous", icon='SCREEN_BACK', text="Back to Previous")
            layout.separator()
        else:
            layout.template_ID(context.window, "screen", new="screen.new", unlink="screen.delete")
            layout.template_ID(context.screen, "scene", new="scene.new", unlink="scene.delete")

        layout.separator()

        if rd.has_multiple_engines:
            layout.prop(rd, "engine", text="")

        layout.separator()

        layout.template_running_jobs()

        layout.template_reports_banner()

        layout.label(text=scene.statistics())

        # XXX: this should be right-aligned to the RHS of the region
        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER', text="")

        # XXX: BEFORE RELEASE, MOVE FILE MENU OUT OF INFO!!!
        """
        sinfo = context.space_data
        row = layout.row(align=True)
        row.prop(sinfo, "show_report_debug", text="Debug")
        row.prop(sinfo, "show_report_info", text="Info")
        row.prop(sinfo, "show_report_operator", text="Operators")
        row.prop(sinfo, "show_report_warning", text="Warnings")
        row.prop(sinfo, "show_report_error", text="Errors")

        row = layout.row()
        row.enabled = sinfo.show_report_operator
        row.operator("info.report_replay")

        row.menu("INFO_MT_report")
        """


class INFO_MT_report(bpy.types.Menu):
    bl_label = "Report"

    def draw(self, context):
        layout = self.layout
        layout.column()
        layout.operator("console.select_all_toggle")
        layout.operator("console.select_border")
        layout.operator("console.report_delete")
        layout.operator("console.report_copy")


class INFO_MT_file(bpy.types.Menu):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.read_homefile", text="New", icon='NEW')
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        layout.menu("INFO_MT_file_open_recent")
        layout.operator("wm.recover_last_session", icon='RECOVER_LAST')
        layout.operator("wm.recover_auto_save", text="Recover Auto Save...")

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_mainfile", text="Save", icon='FILE_TICK').check_existing = False
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save As...")
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save Copy...").copy = True

        layout.separator()

        layout.operator("screen.userpref_show", text="User Preferences...", icon='PREFERENCES')

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.save_homefile")
        layout.operator("wm.read_factory_settings")

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.link_append", text="Link")
        props = layout.operator("wm.link_append", text="Append")
        props.link = False
        props.instance_groups = False

        layout.separator()

        layout.menu("INFO_MT_file_import")
        layout.menu("INFO_MT_file_export")

        layout.separator()

        layout.menu("INFO_MT_file_external_data")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.quit_blender", text="Quit", icon='QUIT')


class INFO_MT_file_import(bpy.types.Menu):
    bl_idname = "INFO_MT_file_import"
    bl_label = "Import"

    def draw(self, context):
        if hasattr(bpy.types, "WM_OT_collada_import"):
            self.layout.operator("wm.collada_import", text="COLLADA (.dae)")


class INFO_MT_file_export(bpy.types.Menu):
    bl_idname = "INFO_MT_file_export"
    bl_label = "Export"

    def draw(self, context):
        if hasattr(bpy.types, "WM_OT_collada_export"):
            self.layout.operator("wm.collada_export", text="COLLADA (.dae)")


class INFO_MT_file_external_data(bpy.types.Menu):
    bl_label = "External Data"

    def draw(self, context):
        layout = self.layout

        layout.operator("file.pack_all", text="Pack into .blend file")
        layout.operator("file.unpack_all", text="Unpack into Files")

        layout.separator()

        layout.operator("file.make_paths_relative")
        layout.operator("file.make_paths_absolute")
        layout.operator("file.report_missing_files")
        layout.operator("file.find_missing_files")


class INFO_MT_mesh_add(bpy.types.Menu):
    bl_idname = "INFO_MT_mesh_add"
    bl_label = "Mesh"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.primitive_plane_add", icon='MESH_PLANE', text="Plane")
        layout.operator("mesh.primitive_cube_add", icon='MESH_CUBE', text="Cube")
        layout.operator("mesh.primitive_circle_add", icon='MESH_CIRCLE', text="Circle")
        layout.operator("mesh.primitive_uv_sphere_add", icon='MESH_UVSPHERE', text="UV Sphere")
        layout.operator("mesh.primitive_ico_sphere_add", icon='MESH_ICOSPHERE', text="Icosphere")
        layout.operator("mesh.primitive_cylinder_add", icon='MESH_CYLINDER', text="Cylinder")
        layout.operator("mesh.primitive_cone_add", icon='MESH_CONE', text="Cone")
        layout.separator()
        layout.operator("mesh.primitive_grid_add", icon='MESH_GRID', text="Grid")
        layout.operator("mesh.primitive_monkey_add", icon='MESH_MONKEY', text="Monkey")
        layout.operator("mesh.primitive_torus_add", text="Torus", icon='MESH_TORUS')


class INFO_MT_curve_add(bpy.types.Menu):
    bl_idname = "INFO_MT_curve_add"
    bl_label = "Curve"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("curve.primitive_bezier_curve_add", icon='CURVE_BEZCURVE', text="Bezier")
        layout.operator("curve.primitive_bezier_circle_add", icon='CURVE_BEZCIRCLE', text="Circle")
        layout.operator("curve.primitive_nurbs_curve_add", icon='CURVE_NCURVE', text="Nurbs Curve")
        layout.operator("curve.primitive_nurbs_circle_add", icon='CURVE_NCIRCLE', text="Nurbs Circle")
        layout.operator("curve.primitive_nurbs_path_add", icon='CURVE_PATH', text="Path")


class INFO_MT_edit_curve_add(bpy.types.Menu):
    bl_idname = "INFO_MT_edit_curve_add"
    bl_label = "Add"

    def draw(self, context):
        is_surf = context.active_object.type == 'SURFACE'

        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        if is_surf:
            INFO_MT_surface_add.draw(self, context)
        else:
            INFO_MT_curve_add.draw(self, context)


class INFO_MT_surface_add(bpy.types.Menu):
    bl_idname = "INFO_MT_surface_add"
    bl_label = "Surface"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("surface.primitive_nurbs_surface_curve_add", icon='SURFACE_NCURVE', text="NURBS Curve")
        layout.operator("surface.primitive_nurbs_surface_circle_add", icon='SURFACE_NCIRCLE', text="NURBS Circle")
        layout.operator("surface.primitive_nurbs_surface_surface_add", icon='SURFACE_NSURFACE', text="NURBS Surface")
        layout.operator("surface.primitive_nurbs_surface_cylinder_add", icon='SURFACE_NCYLINDER', text="NURBS Cylinder")
        layout.operator("surface.primitive_nurbs_surface_sphere_add", icon='SURFACE_NSPHERE', text="NURBS Sphere")
        layout.operator("surface.primitive_nurbs_surface_torus_add", icon='SURFACE_NTORUS', text="NURBS Torus")


class INFO_MT_armature_add(bpy.types.Menu):
    bl_idname = "INFO_MT_armature_add"
    bl_label = "Armature"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("object.armature_add", text="Single Bone", icon='BONE_DATA')


class INFO_MT_add(bpy.types.Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_SCREEN'

        #layout.operator_menu_enum("object.mesh_add", "type", text="Mesh", icon='OUTLINER_OB_MESH')
        layout.menu("INFO_MT_mesh_add", icon='OUTLINER_OB_MESH')

        #layout.operator_menu_enum("object.curve_add", "type", text="Curve", icon='OUTLINER_OB_CURVE')
        layout.menu("INFO_MT_curve_add", icon='OUTLINER_OB_CURVE')
        #layout.operator_menu_enum("object.surface_add", "type", text="Surface", icon='OUTLINER_OB_SURFACE')
        layout.menu("INFO_MT_surface_add", icon='OUTLINER_OB_SURFACE')
        layout.operator_menu_enum("object.metaball_add", "type", text="Metaball", icon='OUTLINER_OB_META')
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("object.text_add", text="Text", icon='OUTLINER_OB_FONT')
        layout.separator()

        layout.menu("INFO_MT_armature_add", icon='OUTLINER_OB_ARMATURE')
        layout.operator("object.add", text="Lattice", icon='OUTLINER_OB_LATTICE').type = 'LATTICE'
        layout.operator("object.add", text="Empty", icon='OUTLINER_OB_EMPTY').type = 'EMPTY'
        layout.separator()

        layout.operator("object.camera_add", text="Camera", icon='OUTLINER_OB_CAMERA')
        layout.operator_context = 'EXEC_SCREEN'
        layout.operator_menu_enum("object.lamp_add", "type", text="Lamp", icon='OUTLINER_OB_LAMP')
        layout.separator()

        layout.operator_menu_enum("object.effector_add", "type", text="Force Field", icon='OUTLINER_OB_EMPTY')
        layout.separator()

        if(len(bpy.data.groups) > 10):
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("object.group_instance_add", text="Group Instance...", icon='OUTLINER_OB_EMPTY')
        else:
            layout.operator_menu_enum("object.group_instance_add", "group", text="Group Instance", icon='OUTLINER_OB_EMPTY')


class INFO_MT_game(bpy.types.Menu):
    bl_label = "Game"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        layout.operator("view3d.game_start")

        layout.separator()

        layout.prop(gs, "show_debug_properties")
        layout.prop(gs, "show_framerate_profile")
        layout.prop(gs, "show_physics_visualization")
        layout.prop(gs, "use_deprecation_warnings")
        layout.prop(gs, "use_animation_record")
        layout.separator()
        layout.prop(gs, "use_auto_start")


class INFO_MT_render(bpy.types.Menu):
    bl_label = "Render"

    def draw(self, context):
        layout = self.layout

        layout.operator("render.render", text="Render Image", icon='RENDER_STILL')
        layout.operator("render.render", text="Render Animation", icon='RENDER_ANIMATION').animation = True

        layout.separator()

        layout.operator("render.opengl", text="OpenGL Render Image")
        layout.operator("render.opengl", text="OpenGL Render Animation").animation = True

        layout.separator()

        layout.operator("render.view_show")
        layout.operator("render.play_rendered_anim")


class INFO_MT_help(bpy.types.Menu):
    bl_label = "Help"

    def draw(self, context):
        import sys

        layout = self.layout

        layout.operator("wm.url_open", text="Manual", icon='HELP').url = 'http://wiki.blender.org/index.php/Doc:Manual'
        layout.operator("wm.url_open", text="Release Log", icon='URL').url = 'http://www.blender.org/development/release-logs/blender-256-beta/'

        layout.separator()

        layout.operator("wm.url_open", text="Blender Website", icon='URL').url = 'http://www.blender.org/'
        layout.operator("wm.url_open", text="Blender e-Shop", icon='URL').url = 'http://www.blender.org/e-shop'
        layout.operator("wm.url_open", text="Developer Community", icon='URL').url = 'http://www.blender.org/community/get-involved/'
        layout.operator("wm.url_open", text="User Community", icon='URL').url = 'http://www.blender.org/community/user-community/'
        layout.separator()
        layout.operator("wm.url_open", text="Report a Bug", icon='URL').url = 'http://projects.blender.org/tracker/?atid=498&group_id=9&func=browse'
        layout.separator()
        layout.operator("wm.url_open", text="Python API Reference", icon='URL').url = "http://www.blender.org/documentation/blender_python_api_%s/contents.html" % "_".join(str(v) for v in bpy.app.version)
        layout.operator("help.operator_cheat_sheet", icon='TEXT')
        layout.operator("wm.sysinfo", icon='TEXT')
        layout.separator()
        if sys.platform[:3] == "win":
            layout.operator("wm.toggle_console", icon='CONSOLE')
            layout.separator()
        layout.operator("anim.update_data_paths", text="FCurve/Driver 2.54 fix", icon='HELP')
        layout.separator()
        layout.operator("wm.splash", icon='BLENDER')


# Help operators


class HELP_OT_operator_cheat_sheet(bpy.types.Operator):
    bl_idname = "help.operator_cheat_sheet"
    bl_label = "Operator Cheat Sheet"

    def execute(self, context):
        op_strings = []
        tot = 0
        for op_module_name in dir(bpy.ops):
            op_module = getattr(bpy.ops, op_module_name)
            for op_submodule_name in dir(op_module):
                op = getattr(op_module, op_submodule_name)
                text = repr(op)
                if text.split("\n")[-1].startswith('bpy.ops.'):
                    op_strings.append(text)
                    tot += 1

            op_strings.append('')

        textblock = bpy.data.texts.new("OperatorList.txt")
        textblock.write('# %d Operators\n\n' % tot)
        textblock.write('\n'.join(op_strings))
        self.report({'INFO'}, "See OperatorList.txt textblock")
        return {'FINISHED'}
