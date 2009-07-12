
import bpy

class TEXT_HT_header(bpy.types.Header):
	__space_type__ = "TEXT_EDITOR"
	__idname__ = "TEXT_HT_header"

	def draw(self, context):
		st = context.space_data
		text = st.text
		layout = self.layout

		layout.template_header()

		if context.area.show_menus:
			row = layout.row()
			row.itemM("TEXT_MT_text")
			if text:
				row.itemM("TEXT_MT_edit")
				row.itemM("TEXT_MT_format")

		if text and text.modified:
			row = layout.row()
			# row.color(redalert)
			row.itemO("TEXT_OT_resolve_conflict", text="", icon='ICON_HELP')

		row = layout.row(align=True)
		row.itemR(st, "line_numbers", text="")
		row.itemR(st, "word_wrap", text="")
		row.itemR(st, "syntax_highlight", text="")

		layout.template_ID(st, "text", new="TEXT_OT_new", unlink="TEXT_OT_unlink")

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
		flow.itemR(st, "line_numbers")
		flow.itemR(st, "word_wrap")
		flow.itemR(st, "syntax_highlight")
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

class TEXT_MT_edit_view(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout

		layout.item_enumO("TEXT_OT_move", "type", "FILE_TOP", text="Top of File")
		layout.item_enumO("TEXT_OT_move", "type", "FILE_BOTTOM", text="Bottom of File")

class TEXT_MT_edit_select(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("TEXT_OT_select_all")
		layout.itemO("TEXT_OT_select_line")

class TEXT_MT_edit_markers(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "Markers"

	def draw(self, context):
		layout = self.layout

		layout.itemO("TEXT_OT_markers_clear")
		layout.itemO("TEXT_OT_next_marker")
		layout.itemO("TEXT_OT_previous_marker")

class TEXT_MT_format(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "Format"

	def draw(self, context):
		layout = self.layout

		layout.itemO("TEXT_OT_indent")
		layout.itemO("TEXT_OT_unindent")

		layout.itemS()

		layout.itemO("TEXT_OT_comment")
		layout.itemO("TEXT_OT_uncomment")

		layout.itemS()

		layout.item_menu_enumO("TEXT_OT_convert_whitespace", "type")

class TEXT_MT_edit_to3d(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "Text To 3D Object"

	def draw(self, context):
		layout = self.layout

		layout.item_booleanO("TEXT_OT_to_3d_object", "split_lines", False, text="One Object");
		layout.item_booleanO("TEXT_OT_to_3d_object", "split_lines", True, text="One Object Per Line");

class TEXT_MT_edit(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "Edit"

	def poll(self, context):
		st = context.space_data
		return st.text != None

	def draw(self, context):
		layout = self.layout

		layout.itemO("ED_OT_undo")
		layout.itemO("ED_OT_redo")

		layout.itemS()

		layout.itemO("TEXT_OT_cut")
		layout.itemO("TEXT_OT_copy")
		layout.itemO("TEXT_OT_paste")

		layout.itemS()

		layout.itemM("TEXT_MT_edit_view")
		layout.itemM("TEXT_MT_edit_select")
		layout.itemM("TEXT_MT_edit_markers")

		layout.itemS()

		layout.itemO("TEXT_OT_jump")
		layout.itemO("TEXT_OT_properties")

		layout.itemS()

		layout.itemM("TEXT_MT_edit_to3d")


class TEXT_OT_line_console(bpy.types.Operator):
	'''
	Operator documentatuon text, will be used for the operator tooltip and python docs.
	'''
	__label__ = "Console Execute"
	
	# Each text block gets its own console info.
	console = {}
	
	# Both prompts must be the same length
	PROMPT = '>>> ' 
	PROMPT_MULTI = '... '
	
	def execute(self, context):
		import sys
		
		# clear all dead consoles, use text names as IDs
		for id in list(self.__class__.console.keys()):
			if id not in bpy.data.texts:
				del self.__class__.console[id]
		
		# print("Selected: " + context.active_object.name)
		st = context.space_data
		text = st.text
		
		if not text:
			return ('CANCELLED',)
		
		line = st.text.current_line.line
		id = text.name
		
		try:
			namespace, console, stdout = self.__class__.console[id]
		except:
			import code, io
			
			namespace = locals().update({'bpy':bpy})
			console = code.InteractiveConsole(namespace)
			
			if sys.version.startswith('2'): # Py2.x support
				stdout = io.BytesIO()
			else:
				stdout = io.StringIO()
			
			
			
			self.__class__.console[id]= namespace, console, stdout
			del code, io
		
		# redirect output
		sys.stdout = stdout
		sys.stderr = stdout
		
		# run the console
		if not line.strip():
			line = '\n' # executes a multiline statement
		
		if line.startswith(self.__class__.PROMPT_MULTI) or line.startswith(self.__class__.PROMPT):
			line = line[len(self.__class__.PROMPT):]
			was_prefix = True
		else:
			was_prefix = False
		
		
		is_multiline = console.push(line)
		
		stdout.seek(0)
		output = stdout.read()
	
		# cleanup
		sys.stdout = sys.__stdout__
		sys.stderr = sys.__stderr__
		sys.last_traceback = None
		
		# So we can reuse, clear all data
		stdout.truncate(0)
		
		if is_multiline:
			prefix = self.__class__.PROMPT_MULTI
		else:
			prefix = self.__class__.PROMPT
		
		# Kindof odd, add the prefix if we didnt have one. makes it easier to re-read.
		if not was_prefix:
			bpy.ops.TEXT_OT_move(type='LINE_BEGIN')
			bpy.ops.TEXT_OT_insert(text = prefix)
		
		bpy.ops.TEXT_OT_move(type='LINE_END')
		
		# Insert the output into the editor
		bpy.ops.TEXT_OT_insert(text= '\n' + output + prefix)
		
		return ('FINISHED',)
	
bpy.types.register(TEXT_HT_header)
bpy.types.register(TEXT_PT_properties)
bpy.types.register(TEXT_PT_find)
bpy.types.register(TEXT_MT_text)
bpy.types.register(TEXT_MT_format)
bpy.types.register(TEXT_MT_edit)
bpy.types.register(TEXT_MT_edit_view)
bpy.types.register(TEXT_MT_edit_select)
bpy.types.register(TEXT_MT_edit_markers)
bpy.types.register(TEXT_MT_edit_to3d)

bpy.ops.add(TEXT_OT_line_console)

