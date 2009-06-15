
import bpy

# temporary
ICON_LINENUMBERS_OFF = 588
ICON_WORDWRAP_OFF = 584
ICON_SYNTAX_OFF = 586
ICON_TEXT = 120
ICON_HELP = 1
ICON_SCRIPTPLUGINS = 1

class TEXT_HT_header(bpy.types.Header):
	__space_type__ = "TEXT_EDITOR"
	__idname__ = "TEXT_HT_header"

	def draw(self, context):
		st = context.space_data
		text = st.text
		layout = self.layout

		layout.template_header(context)

		if context.area.show_menus:
			row = layout.row(align=True)
			row.itemM(context, "TEXT_MT_text")
			if text:
				row.itemM(context, "TEXT_MT_edit")
				row.itemM(context, "TEXT_MT_format")

		if text and text.modified:
			row = layout.row()
			# row.color(redalert)
			row.itemO("TEXT_OT_resolve_conflict", text="", icon='ICON_HELP')

		row = layout.row(align=True)
		row.itemR(st, "line_numbers", text="", icon='ICON_LINENUMBERS_OFF')
		row.itemR(st, "word_wrap", text="", icon='ICON_WORDWRAP_OFF')
		row.itemR(st, "syntax_highlight", text="", icon='ICON_SYNTAX_OFF')
		# row.itemR(st, "do_python_plugins", text="", icon=ICON_SCRIPTPLUGINS)

		layout.template_ID(context, st, "text", new="TEXT_OT_new", open="TEXT_OT_open", unlink="TEXT_OT_unlink")

		if text:
			row = layout.row()
			if text.filename != "":
				if text.dirty:
					row.itemL(text="File: *" + text.filename + " (unsaved)")
				else:
					row.itemL(text="File: " + text.filename)
			else:
				if text.library:
					row.itemL(text="Text: External")
				else:
					row.itemL(text="Text: Internal")

class TEXT_PT_properties(bpy.types.Panel):
	__space_type__ = "TEXT_EDITOR"
	__region_type__ = "UI"
	__label__ = "Properties"

	def draw(self, context):
		st = context.space_data
		layout = self.layout

		flow = layout.column_flow()
		flow.itemR(st, "line_numbers", icon='ICON_LINENUMBERS_OFF')
		flow.itemR(st, "word_wrap", icon='ICON_WORDWRAP_OFF')
		flow.itemR(st, "syntax_highlight", icon='ICON_SYNTAX_OFF')
		flow.itemR(st, "live_edit")

		flow = layout.column_flow()
		flow.itemR(st, "font_size")
		flow.itemR(st, "tab_width")

class TEXT_PT_find(bpy.types.Panel):
	__space_type__ = "TEXT_EDITOR"
	__region_type__ = "UI"
	__label__ = "Find"

	def draw(self, context):
		st = context.space_data
		layout = self.layout

		# find
		col = layout.column(align=True)
		row = col.row()
		row.itemR(st, "find_text", text="")
		row.itemO("TEXT_OT_find_set_selected", text="", icon='ICON_TEXT')
		col.itemO("TEXT_OT_find")

		# replace
		col = layout.column(align=True)
		row = col.row()
		row.itemR(st, "replace_text", text="")
		row.itemO("TEXT_OT_replace_set_selected", text="", icon='ICON_TEXT')
		col.itemO("TEXT_OT_replace")

		# mark
		layout.itemO("TEXT_OT_mark_all")

		# settings
		row = layout.row()
		row.itemR(st, "find_wrap", text="Wrap")
		row.itemR(st, "find_all", text="All")

class TEXT_MT_text(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "Text"

	def draw(self, context):
		layout = self.layout
		st = context.space_data
		text = st.text

		layout.column()
		layout.itemO("TEXT_OT_new")
		layout.itemO("TEXT_OT_open")

		if text:
			layout.itemO("TEXT_OT_reload")

			layout.column()
			layout.itemO("TEXT_OT_save")
			layout.itemO("TEXT_OT_save_as")

			if text.filename != "":
				layout.itemO("TEXT_OT_make_internal")

			layout.column()
			layout.itemO("TEXT_OT_run_script")

			#ifndef DISABLE_PYTHON
			# XXX if(BPY_is_pyconstraint(text))
			# XXX   uiMenuItemO(head, 0, "TEXT_OT_refresh_pyconstraints");
			#endif
		
		#ifndef DISABLE_PYTHON
		# XXX layout.column()
		# XXX uiDefIconTextBlockBut(block, text_template_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Script Templates", 0, yco-=20, 120, 19, "");
		# XXX uiDefIconTextBlockBut(block, text_plugin_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Text Plugins", 0, yco-=20, 120, 19, "");
		#endif

bpy.types.register(TEXT_HT_header)
bpy.types.register(TEXT_PT_properties)
bpy.types.register(TEXT_PT_find)
bpy.types.register(TEXT_MT_text)

