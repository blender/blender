
import bpy

class INFO_HT_header(bpy.types.Header):
	__space_type__ = "USER_PREFERENCES"
	__idname__ = "INFO_HT_header"

	def draw(self, context):
		st = context.space_data
		layout = self.layout
		
		layout.template_header()

		if context.area.show_menus:
			row = layout.row()
			row.itemM("INFO_MT_file")
			row.itemM("INFO_MT_add")
			row.itemM("INFO_MT_timeline")
			row.itemM("INFO_MT_game")
			row.itemM("INFO_MT_render")
			row.itemM("INFO_MT_help")

		layout.template_ID(context.window, "screen") #, new="SCREEN_OT_new", open="SCREEN_OT_unlink")
		layout.template_ID(context.screen, "scene") #, new="SCENE_OT_new", unlink="SCENE_OT_unlink")

		layout.itemS()

		layout.template_operator_search()
		layout.template_running_jobs()
			
class INFO_MT_file(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "File"

	def draw(self, context):
		layout = self.layout

		layout.operator_context = "EXEC_AREA"
		layout.itemO("WM_OT_read_homefile")
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("WM_OT_open_mainfile")

		layout.itemS()

		layout.operator_context = "EXEC_AREA"
		layout.itemO("WM_OT_save_mainfile")
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("WM_OT_save_as_mainfile")

		layout.itemS()

		layout.itemM("INFO_MT_file_external_data")

class INFO_MT_file_external_data(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "External Data"

	def draw(self, context):
		layout = self.layout

		layout.itemO("FILE_OT_pack_all", text="Pack into .blend file")
		layout.itemO("FILE_OT_unpack_all", text="Unpack into Files...")

		layout.itemS()

		layout.itemO("FILE_OT_make_paths_relative")
		layout.itemO("FILE_OT_make_paths_absolute")
		layout.itemO("FILE_OT_report_missing_files")
		layout.itemO("FILE_OT_find_missing_files")

class INFO_MT_add(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Add"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_MT_timeline(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Timeline"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_MT_game(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Game"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_MT_render(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Render"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_MT_help(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Help"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_PT_tabs(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__no_header__ = True

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences

		layout.itemR(userpref, "active_section")

class INFO_PT_view(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "View"

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'VIEW_CONTROLS')

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
		view = userpref.view

		layout.itemR(view, "show_view_name")

bpy.types.register(INFO_HT_header)
bpy.types.register(INFO_MT_file)
bpy.types.register(INFO_MT_file_external_data)
bpy.types.register(INFO_MT_add)
bpy.types.register(INFO_MT_timeline)
bpy.types.register(INFO_MT_game)
bpy.types.register(INFO_MT_render)
bpy.types.register(INFO_MT_help)
bpy.types.register(INFO_PT_tabs)
bpy.types.register(INFO_PT_view)

