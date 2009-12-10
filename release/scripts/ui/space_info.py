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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

import dynamic_menu
# reload(dynamic_menu)


class INFO_HT_header(bpy.types.Header):
    bl_space_type = 'INFO'

    def draw(self, context):
        layout = self.layout

        window = context.window
        scene = context.scene
        rd = scene.render_data

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

        if window.screen.fullscreen:
            layout.operator("screen.back_to_previous", icon='SCREEN_BACK', text="Back to Previous")
            layout.separator()
        else:
            layout.template_ID(context.window, "screen", new="screen.new", unlink="screen.delete")
        
        layout.template_ID(context.screen, "scene", new="scene.new", unlink="scene.delete")

        layout.separator()

        if rd.multiple_engines:
            layout.prop(rd, "engine", text="")

        layout.separator()

        layout.template_operator_search()
        layout.template_running_jobs()

        layout.label(text=scene.statistics())

        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER', text="")


class INFO_MT_file(bpy.types.Menu):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.read_homefile", text="New", icon='NEW')
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        layout.operator_menu_enum("wm.open_recentfile", "file", text="Open Recent")
        layout.operator("wm.recover_last_session")
        layout.operator("wm.recover_auto_save", text="Recover Auto Save...")

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_mainfile", text="Save", icon='FILE_TICK')
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save As...")

        layout.separator()

        layout.operator("screen.userpref_show", text="User Preferences...", icon='PREFERENCES')
        layout.operator("wm.read_homefile", text="Load Factory Settings").factory = True

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.link_append", text="Link")
        layout.operator("wm.link_append", text="Append").link = False

        layout.separator()

        layout.menu("INFO_MT_file_import")
        layout.menu("INFO_MT_file_export")

        layout.separator()

        layout.menu("INFO_MT_file_external_data")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.exit_blender", text="Quit", icon='QUIT')

# test for expanding menus
'''
class INFO_MT_file_more(INFO_MT_file):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator("wm.read_homefile", text="TESTING ")

dynamic_menu.setup(INFO_MT_file_more)
'''


class INFO_MT_file_import(dynamic_menu.DynMenu):
    bl_idname = "INFO_MT_file_import"
    bl_label = "Import"

    def draw(self, context):
        if "collada_import" in dir(bpy.ops.wm):
            self.layout.operator("wm.collada_import", text="COLLADA (.dae)...")


class INFO_MT_file_export(dynamic_menu.DynMenu):
    bl_idname = "INFO_MT_file_export"
    bl_label = "Export"

    def draw(self, context):
        if "collada_export" in dir(bpy.ops.wm):
            self.layout.operator("wm.collada_export", text="COLLADA (.dae)...")


class INFO_MT_file_external_data(bpy.types.Menu):
    bl_label = "External Data"

    def draw(self, context):
        layout = self.layout

        layout.operator("file.pack_all", text="Pack into .blend file")
        layout.operator("file.unpack_all", text="Unpack into Files...")

        layout.separator()

        layout.operator("file.make_paths_relative")
        layout.operator("file.make_paths_absolute")
        layout.operator("file.report_missing_files")
        layout.operator("file.find_missing_files")


class INFO_MT_mesh_add(dynamic_menu.DynMenu):
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
        layout.operator("mesh.primitive_tube_add", icon='MESH_TUBE', text="Tube")
        layout.operator("mesh.primitive_cone_add", icon='MESH_CONE', text="Cone")
        layout.separator()
        layout.operator("mesh.primitive_grid_add", icon='MESH_GRID', text="Grid")
        layout.operator("mesh.primitive_monkey_add", icon='MESH_MONKEY', text="Monkey")


class INFO_MT_add(bpy.types.Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_SCREEN'

        #layout.operator_menu_enum("object.mesh_add", "type", text="Mesh", icon='OUTLINER_OB_MESH')
        layout.menu("INFO_MT_mesh_add", icon='OUTLINER_OB_MESH')

        layout.operator_menu_enum("object.curve_add", "type", text="Curve", icon='OUTLINER_OB_CURVE')
        layout.operator_menu_enum("object.surface_add", "type", text="Surface", icon='OUTLINER_OB_SURFACE')
        layout.operator_menu_enum("object.metaball_add", "type", 'META', text="Metaball", icon='OUTLINER_OB_META')
        layout.operator("object.text_add", text="Text", icon='OUTLINER_OB_FONT')

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("object.armature_add", text="Armature", icon='OUTLINER_OB_ARMATURE')
        layout.operator("object.add", text="Lattice", icon='OUTLINER_OB_LATTICE').type = 'LATTICE'
        layout.operator("object.add", text="Empty", icon='OUTLINER_OB_EMPTY').type = 'EMPTY'

        layout.separator()

        layout.operator("object.add", text="Camera", icon='OUTLINER_OB_CAMERA').type = 'CAMERA'

        layout.operator_context = 'EXEC_SCREEN'

        layout.operator_menu_enum("object.lamp_add", "type", 'LAMP', text="Lamp", icon='OUTLINER_OB_LAMP')

        layout.separator()

        layout.operator_menu_enum("object.effector_add", "type", 'EMPTY', text="Force Field", icon='OUTLINER_OB_EMPTY')

        layout.separator()

        layout.operator_menu_enum("object.group_instance_add", "type", text="Group Instance", icon='OUTLINER_OB_EMPTY')


class INFO_MT_game(bpy.types.Menu):
    bl_label = "Game"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_data

        layout.operator("view3d.game_start")

        layout.separator()

        layout.prop(gs, "show_debug_properties")
        layout.prop(gs, "show_framerate_profile")
        layout.prop(gs, "show_physics_visualization")
        layout.prop(gs, "deprecation_warnings")


class INFO_MT_render(bpy.types.Menu):
    bl_label = "Render"

    def draw(self, context):
        layout = self.layout

        # rd = context.scene.render_data

        layout.operator("screen.render", text="Render Image", icon='RENDER_STILL')
        layout.operator("screen.render", text="Render Animation", icon='RENDER_ANIMATION').animation = True

        layout.separator()

        layout.operator("screen.opengl_render", text="OpenGL Render Image")
        layout.operator("screen.opengl_render", text="OpenGL Render Animation").animation = True

        layout.separator()

        layout.operator("screen.render_view_show")


class INFO_MT_help(bpy.types.Menu):
    bl_label = "Help"

    def draw(self, context):
        layout = self.layout

        layout.operator("help.manual", icon='HELP')
        layout.operator("help.release_logs", icon='URL')

        layout.separator()

        layout.operator("help.blender_website", icon='URL')
        layout.operator("help.blender_eshop", icon='URL')
        layout.operator("help.developer_community", icon='URL')
        layout.operator("help.user_community", icon='URL')
        layout.separator()
        layout.operator("help.report_bug", icon='URL')
        layout.separator()
        layout.operator("help.python_api", icon='URL')
        layout.operator("help.operator_cheat_sheet")

bpy.types.register(INFO_HT_header)
bpy.types.register(INFO_MT_file)
bpy.types.register(INFO_MT_file_import)
bpy.types.register(INFO_MT_file_export)
bpy.types.register(INFO_MT_file_external_data)
bpy.types.register(INFO_MT_add)
bpy.types.register(INFO_MT_mesh_add)
bpy.types.register(INFO_MT_game)
bpy.types.register(INFO_MT_render)
bpy.types.register(INFO_MT_help)

# Help operators


class HelpOperator(bpy.types.Operator):

    def execute(self, context):
        import webbrowser
        webbrowser.open(self._url)
        return ('FINISHED',)


class HELP_OT_manual(HelpOperator):
    '''The Blender Wiki manual'''
    bl_idname = "help.manual"
    bl_label = "Manual"
    _url = 'http://wiki.blender.org/index.php/Doc:Manual'


class HELP_OT_release_logs(HelpOperator):
    '''Information about the changes in this version of Blender'''
    bl_idname = "help.release_logs"
    bl_label = "Release Log"
    _url = 'http://www.blender.org/development/release-logs/blender-250/'


class HELP_OT_blender_website(HelpOperator):
    '''The official Blender website'''
    bl_idname = "help.blender_website"
    bl_label = "Blender Website"
    _url = 'http://www.blender.org/'


class HELP_OT_blender_eshop(HelpOperator):
    '''Buy official Blender resources and merchandise online'''
    bl_idname = "help.blender_eshop"
    bl_label = "Blender e-Shop"
    _url = 'http://www.blender3d.org/e-shop'


class HELP_OT_developer_community(HelpOperator):
    '''Get involved with Blender development'''
    bl_idname = "help.developer_community"
    bl_label = "Developer Community"
    _url = 'http://www.blender.org/community/get-involved/'


class HELP_OT_user_community(HelpOperator):
    '''Get involved with other Blender users'''
    bl_idname = "help.user_community"
    bl_label = "User Community"
    _url = 'http://www.blender.org/community/user-community/'


class HELP_OT_report_bug(HelpOperator):
    '''Report a bug in the Blender bug tracker'''
    bl_idname = "help.report_bug"
    bl_label = "Report a Bug"
    _url = 'http://projects.blender.org/tracker/?atid=498&group_id=9&func=browse'


class HELP_OT_python_api(HelpOperator):
    '''Reference for operator and data Python API'''
    bl_idname = "help.python_api"
    bl_label = "Python API Reference"
    _url = 'http://www.blender.org/documentation/250PythonDoc/'


class HELP_OT_operator_cheat_sheet(bpy.types.Operator):
    bl_idname = "help.operator_cheat_sheet"
    bl_label = "Operator Cheat Sheet (new textblock)"

    def execute(self, context):
        op_strings = []
        tot = 0
        for op_module_name in dir(bpy.ops):
            op_module = getattr(bpy.ops, op_module_name)
            for op_submodule_name in dir(op_module):
                op = getattr(op_module, op_submodule_name)
                text = repr(op)
                if text.startswith('bpy.ops.'):
                    op_strings.append(text)
                    tot += 1

            op_strings.append('')

        bpy.ops.text.new() # XXX - assumes new text is always at the end!
        textblock = bpy.data.texts[-1]
        textblock.write('# %d Operators\n\n' % tot)
        textblock.write('\n'.join(op_strings))
        textblock.name = "OperatorList.txt"
        print("See OperatorList.txt textblock")
        return ('FINISHED',)

bpy.ops.add(HELP_OT_manual)
bpy.ops.add(HELP_OT_release_logs)
bpy.ops.add(HELP_OT_blender_website)
bpy.ops.add(HELP_OT_blender_eshop)
bpy.ops.add(HELP_OT_developer_community)
bpy.ops.add(HELP_OT_user_community)
bpy.ops.add(HELP_OT_report_bug)
bpy.ops.add(HELP_OT_python_api)
bpy.ops.add(HELP_OT_operator_cheat_sheet)
