
import bpy

class INFO_HT_header(bpy.types.Header):
	__space_type__ = 'INFO'

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
			
class INFO_MT_file(bpy.types.Menu):
	__space_type__ = 'INFO'
	__label__ = "File"

	def draw(self, context):
		layout = self.layout

		layout.operator_context = "EXEC_AREA"
		layout.itemO("wm.read_homefile", text="New")
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("wm.open_mainfile", text="Open...")
		layout.item_menu_enumO("wm.open_recentfile", "file", text="Open Recent")
		layout.itemO("wm.recover_last_session")

		layout.itemS()

		layout.operator_context = "EXEC_AREA"
		layout.itemO("wm.save_mainfile", text="Save")
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("wm.save_as_mainfile", text="Save As...")
		layout.itemO("screen.userpref_show", text="User Preferences...")

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
		layout.itemO("wm.exit_blender", text="Quit")

class INFO_MT_file_import(bpy.types.Menu):
	__space_type__ = 'INFO'
	__label__ = "Import"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="Nothing yet")

class INFO_MT_file_export(bpy.types.Menu):
	__space_type__ = 'INFO'
	__label__ = "Export"

	def draw(self, context):
		layout = self.layout

		layout.itemO("export.ply", text="PLY")

class INFO_MT_file_external_data(bpy.types.Menu):
	__space_type__ = 'INFO'
	__label__ = "External Data"

	def draw(self, context):
		layout = self.layout

		layout.itemO("file.pack_all", text="Pack into .blend file")
		layout.itemO("file.unpack_all", text="Unpack into Files...")

		layout.itemS()

		layout.itemO("file.make_paths_relative")
		layout.itemO("file.make_paths_absolute")
		layout.itemO("file.report_missing_files")
		layout.itemO("file.find_missing_files")

class INFO_MT_add(bpy.types.Menu):
	__space_type__ = 'INFO'
	__label__ = "Add"

	def draw(self, context):
		layout = self.layout

		layout.operator_context = "EXEC_SCREEN"

		layout.item_menu_enumO("object.mesh_add", "type", text="Mesh", icon='ICON_OUTLINER_OB_MESH')
		layout.item_menu_enumO("object.curve_add", "type", text="Curve", icon='ICON_OUTLINER_OB_CURVE')
		layout.item_menu_enumO("object.surface_add", "type", text="Surface", icon='ICON_OUTLINER_OB_SURFACE')
		layout.item_menu_enumO("object.metaball_add", "type", 'META', icon='ICON_OUTLINER_OB_META')
		layout.itemO("object.text_add", text="Text", icon='ICON_OUTLINER_OB_FONT')

		layout.itemS()

		layout.itemO("object.armature_add", text="Armature", icon='ICON_OUTLINER_OB_ARMATURE')
		layout.item_enumO("object.add", "type", 'LATTICE', icon='ICON_OUTLINER_OB_LATTICE')
		layout.item_enumO("object.add", "type", 'EMPTY', icon='ICON_OUTLINER_OB_EMPTY')

		layout.itemS()

		layout.item_enumO("object.add", "type", 'CAMERA', icon='ICON_OUTLINER_OB_CAMERA')
		layout.item_enumO("object.add", "type", 'LAMP', icon='ICON_OUTLINER_OB_LAMP')

class INFO_MT_game(bpy.types.Menu):
	__space_type__ = 'INFO'
	__label__ = "Game"

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
	__space_type__ = 'INFO'
	__label__ = "Render"

	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data

		layout.itemO("screen.render", text="Render Image")
		layout.item_booleanO("screen.render", "animation", True, text="Render Animation")

		layout.itemS()

		layout.itemO("screen.render_view_show")

class INFO_MT_help(bpy.types.Menu):
	__space_type__ = 'INFO'
	__label__ = "Help"

	def draw(self, context):
		layout = self.layout

		layout.itemO("help.manual")
		layout.itemO("help.release_logs")

		layout.itemS()

		layout.itemO("help.blender_website")
		layout.itemO("help.blender_eshop")
		layout.itemO("help.developer_community")
		layout.itemO("help.user_community")
		layout.itemS()
		layout.itemO("help.operator_cheat_sheet")
		

bpy.types.register(INFO_HT_header)
bpy.types.register(INFO_MT_file)
bpy.types.register(INFO_MT_file_import)
bpy.types.register(INFO_MT_file_export)
bpy.types.register(INFO_MT_file_external_data)
bpy.types.register(INFO_MT_add)
bpy.types.register(INFO_MT_game)
bpy.types.register(INFO_MT_render)
bpy.types.register(INFO_MT_help)

# Help operators

import bpy_ops # XXX - should not need to do this
del bpy_ops

class HelpOperator(bpy.types.Operator):
	def execute(self, context):
		try: import webbrowser
		except: webbrowser = None

		if webbrowser:
			webbrowser.open(self.__URL__)
		else:
			raise Exception("Operator requires a full Python installation")

		return ('FINISHED',)

class HELP_OT_manual(HelpOperator):
	__idname__ = "help.manual"
	__label__ = "Manual"
	__URL__ = 'http://wiki.blender.org/index.php/Manual'

class HELP_OT_release_logs(HelpOperator):
	__idname__ = "help.release_logs"
	__label__ = "Release Logs"
	__URL__ = 'http://www.blender.org/development/release-logs/'

class HELP_OT_blender_website(HelpOperator):
	__idname__ = "help.blender_website"
	__label__ = "Blender Website"
	__URL__ = 'http://www.blender.org/'

class HELP_OT_blender_eshop(HelpOperator):
	__idname__ = "help.blender_eshop"
	__label__ = "Blender e-Shop"
	__URL__ = 'http://www.blender3d.org/e-shop'

class HELP_OT_developer_community(HelpOperator):
	__idname__ = "help.developer_community"
	__label__ = "Developer Community"
	__URL__ = 'http://www.blender.org/community/get-involved/'

class HELP_OT_user_community(HelpOperator):
	__idname__ = "help.user_community"
	__label__ = "User Community"
	__URL__ = 'http://www.blender.org/community/user-community/'

class HELP_OT_operator_cheat_sheet(bpy.types.Operator):
	__idname__ = "help.operator_cheat_sheet"
	__label__ = "Operator Cheet Sheet (new textblock)"
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
bpy.ops.add(HELP_OT_operator_cheat_sheet)

