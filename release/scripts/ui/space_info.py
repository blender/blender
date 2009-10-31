# This software is distributable under the terms of the GNU
# General Public License (GPL) v2, the text of which can be found at
# http://www.gnu.org/copyleft/gpl.html. Installing, importing or otherwise
# using this module constitutes acceptance of the terms of this License.

# <pep8 compliant>
import bpy

import dynamic_menu
# reload(dynamic_menu)


class INFO_HT_header(bpy.types.Header):
    bl_space_type = 'INFO'

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        scene = context.scene
        rd = scene.render_data

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.itemM("INFO_MT_file")
            sub.itemM("INFO_MT_add")
            if rd.use_game_engine:
                sub.itemM("INFO_MT_game")
            else:
                sub.itemM("INFO_MT_render")
            sub.itemM("INFO_MT_help")

        layout.template_ID(context.window, "screen", new="screen.new", unlink="screen.delete")
        layout.template_ID(context.screen, "scene", new="scene.new", unlink="scene.delete")

        if rd.multiple_engines:
            layout.itemR(rd, "engine", text="")

        layout.itemS()

        layout.template_operator_search()
        layout.template_running_jobs()

        layout.itemL(text=scene.statistics())

        layout.itemO("wm.window_fullscreen_toggle", icon='ICON_ARROW_LEFTRIGHT', text="")


class INFO_MT_file(bpy.types.Menu):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = "EXEC_AREA"
        layout.itemO("wm.read_homefile", text="New", icon='ICON_NEW')
        layout.operator_context = "INVOKE_AREA"
        layout.itemO("wm.open_mainfile", text="Open...", icon='ICON_FILE_FOLDER')
        layout.item_menu_enumO("wm.open_recentfile", "file", text="Open Recent")
        layout.itemO("wm.recover_last_session")
        layout.itemO("wm.recover_auto_save", text="Recover Auto Save...")

        layout.itemS()

        layout.operator_context = "EXEC_AREA"
        layout.itemO("wm.save_mainfile", text="Save", icon='ICON_FILE_TICK')
        layout.operator_context = "INVOKE_AREA"
        layout.itemO("wm.save_as_mainfile", text="Save As...")
        layout.itemO("screen.userpref_show", text="User Preferences...", icon='ICON_PREFERENCES')

        layout.itemS()
        layout.operator_context = "INVOKE_AREA"
        layout.itemO("wm.link_append", text="Link")
        layout.item_booleanO("wm.link_append", "link", False, text="Append")
        layout.itemS()

        layout.itemM("INFO_MT_file_import")
        layout.itemM("INFO_MT_file_export")

        layout.itemS()

        layout.itemM("INFO_MT_file_external_data")

        layout.itemS()

        layout.operator_context = "EXEC_AREA"
        layout.itemO("wm.exit_blender", text="Quit", icon='ICON_QUIT')

# test for expanding menus
'''
class INFO_MT_file_more(INFO_MT_file):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.itemO("wm.read_homefile", text="TESTING ")

dynamic_menu.setup(INFO_MT_file_more)
'''


class INFO_MT_file_import(dynamic_menu.DynMenu):
    bl_idname = "INFO_MT_file_import"
    bl_label = "Import"

    def draw(self, context):
        self.layout.itemO("WM_OT_collada_import", text="COLLADA (.dae)...")


class INFO_MT_file_export(dynamic_menu.DynMenu):
    bl_idname = "INFO_MT_file_export"
    bl_label = "Export"

    def draw(self, context):
        self.layout.itemO("WM_OT_collada_export", text="COLLADA (.dae)...")


class INFO_MT_file_external_data(bpy.types.Menu):
    bl_label = "External Data"

    def draw(self, context):
        layout = self.layout

        layout.itemO("file.pack_all", text="Pack into .blend file")
        layout.itemO("file.unpack_all", text="Unpack into Files...")

        layout.itemS()

        layout.itemO("file.make_paths_relative")
        layout.itemO("file.make_paths_absolute")
        layout.itemO("file.report_missing_files")
        layout.itemO("file.find_missing_files")


class INFO_MT_mesh_add(dynamic_menu.DynMenu):
    bl_idname = "INFO_MT_mesh_add"
    bl_label = "Mesh"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.itemO("mesh.primitive_plane_add", icon='ICON_MESH_PLANE', text="Plane")
        layout.itemO("mesh.primitive_cube_add", icon='ICON_MESH_CUBE', text="Cube")
        layout.itemO("mesh.primitive_circle_add", icon='ICON_MESH_CIRCLE', text="Circle")
        layout.itemO("mesh.primitive_uv_sphere_add", icon='ICON_MESH_UVSPHERE', text="UV Sphere")
        layout.itemO("mesh.primitive_ico_sphere_add", icon='ICON_MESH_ICOSPHERE', text="Icosphere")
        layout.itemO("mesh.primitive_tube_add", icon='ICON_MESH_TUBE', text="Tube")
        layout.itemO("mesh.primitive_cone_add", icon='ICON_MESH_CONE', text="Cone")
        layout.itemS()
        layout.itemO("mesh.primitive_grid_add", icon='ICON_MESH_GRID', text="Grid")
        layout.itemO("mesh.primitive_monkey_add", icon='ICON_MESH_MONKEY', text="Monkey")


class INFO_MT_add(bpy.types.Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = "EXEC_SCREEN"

        # layout.item_menu_enumO("object.mesh_add", "type", text="Mesh", icon='ICON_OUTLINER_OB_MESH')
        layout.itemM("INFO_MT_mesh_add", icon='ICON_OUTLINER_OB_MESH')

        layout.item_menu_enumO("object.curve_add", "type", text="Curve", icon='ICON_OUTLINER_OB_CURVE')
        layout.item_menu_enumO("object.surface_add", "type", text="Surface", icon='ICON_OUTLINER_OB_SURFACE')
        layout.item_menu_enumO("object.metaball_add", "type", 'META', text="Metaball", icon='ICON_OUTLINER_OB_META')
        layout.itemO("object.text_add", text="Text", icon='ICON_OUTLINER_OB_FONT')

        layout.itemS()

        layout.itemO("object.armature_add", text="Armature", icon='ICON_OUTLINER_OB_ARMATURE')
        layout.item_enumO("object.add", "type", 'LATTICE', icon='ICON_OUTLINER_OB_LATTICE')
        layout.item_enumO("object.add", "type", 'EMPTY', icon='ICON_OUTLINER_OB_EMPTY')

        layout.itemS()

        layout.item_enumO("object.add", "type", 'CAMERA', icon='ICON_OUTLINER_OB_CAMERA')
        layout.item_menu_enumO("object.lamp_add", "type", 'LAMP', text="Lamp", icon='ICON_OUTLINER_OB_LAMP')

        layout.itemS()

        layout.item_menu_enumO("object.effector_add", "type", 'EMPTY', text="Force Field", icon='ICON_OUTLINER_OB_EMPTY')

        layout.itemS()

        layout.item_menu_enumO("object.group_instance_add", "type", text="Group Instance", icon='ICON_OUTLINER_OB_EMPTY')


class INFO_MT_game(bpy.types.Menu):
    bl_label = "Game"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_data

        layout.itemO("view3d.game_start")

        layout.itemS()

        layout.itemR(gs, "show_debug_properties")
        layout.itemR(gs, "show_framerate_profile")
        layout.itemR(gs, "show_physics_visualization")
        layout.itemR(gs, "deprecation_warnings")


class INFO_MT_render(bpy.types.Menu):
    bl_label = "Render"

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data

        layout.itemO("screen.render", text="Render Image", icon='ICON_RENDER_STILL')
        layout.item_booleanO("screen.render", "animation", True, text="Render Animation", icon='ICON_RENDER_ANIMATION')

        layout.itemS()

        layout.itemO("screen.opengl_render", text="OpenGL Render Image")
        layout.item_booleanO("screen.opengl_render", "animation", True, text="OpenGL Render Animation")

        layout.itemS()

        layout.itemO("screen.render_view_show")


class INFO_MT_help(bpy.types.Menu):
    bl_label = "Help"

    def draw(self, context):
        layout = self.layout

        layout.itemO("help.manual", icon='ICON_HELP')
        layout.itemO("help.release_logs", icon='ICON_URL')

        layout.itemS()

        layout.itemO("help.blender_website", icon='ICON_URL')
        layout.itemO("help.blender_eshop", icon='ICON_URL')
        layout.itemO("help.developer_community", icon='ICON_URL')
        layout.itemO("help.user_community", icon='ICON_URL')
        layout.itemS()
        layout.itemO("help.report_bug", icon='ICON_URL')
        layout.itemS()
        layout.itemO("help.operator_cheat_sheet")

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
    _url = 'http://wiki.blender.org/index.php/Manual'


class HELP_OT_release_logs(HelpOperator):
    '''Information about the changes in this version of Blender'''
    bl_idname = "help.release_logs"
    bl_label = "Release Logs"
    _url = 'http://www.blender.org/development/release-logs/'


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
bpy.ops.add(HELP_OT_operator_cheat_sheet)
