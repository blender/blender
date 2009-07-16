
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


def get_console(text):
	'''
	helper function for console operators
	currently each text datablock gets its own console - code.InteractiveConsole()
	...which is stored in this function.
	'''
	import sys, code, io
	
	try:	consoles = get_console.consoles
	except:consoles = get_console.consoles = {}
	
	# clear all dead consoles, use text names as IDs
	for id in list(consoles.keys()):
		if id not in bpy.data.texts:
			del consoles[id]
	
	if not text:
		return None, None, None
		
	id = text.name
	
	try:
		namespace, console, stdout = consoles[id]
	except:
		namespace = locals()
		namespace['bpy'] = bpy
		
		console = code.InteractiveConsole(namespace)
		
		if sys.version.startswith('2'):	stdout = io.BytesIO()  # Py2x support
		else:								stdout = io.StringIO()
	
		consoles[id]= namespace, console, stdout
	
	return namespace, console, stdout

class TEXT_OT_console_exec(bpy.types.Operator):
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
		
		st = context.space_data
		text = st.text
		
		if not text:
			return ('CANCELLED',)
		
		namespace, console, stdout = get_console(text)
		
		line = text.current_line.line
		
		# redirect output
		sys.stdout = stdout
		sys.stderr = stdout
		
		# run the console
		if not line.strip():
			line = '\n' # executes a multiline statement
		
		if line.startswith(self.PROMPT_MULTI) or line.startswith(self.PROMPT):
			line = line[len(self.PROMPT):]
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
			prefix = self.PROMPT_MULTI
		else:
			prefix = self.PROMPT
		
		# Kindof odd, add the prefix if we didnt have one. makes it easier to re-read.
		if not was_prefix:
			bpy.ops.TEXT_OT_move(type='LINE_BEGIN')
			bpy.ops.TEXT_OT_insert(text = prefix)
		
		bpy.ops.TEXT_OT_move(type='LINE_END')
		
		# Insert the output into the editor
		bpy.ops.TEXT_OT_insert(text= '\n' + output + prefix)
		
		return ('FINISHED',)


def autocomp(bcon):
	'''
	This function has been taken from a BGE console autocomp I wrote a while ago
	the dictionaty bcon is not needed but it means I can copy and paste from the old func
	which works ok for now.
	
	could be moved into its own module.
	'''
	
	
	def is_delimiter(ch):
		'''
		For skipping words
		'''
		if ch == '_':
			return False
		if ch.isalnum():
			return False
		
		return True
	
	def is_delimiter_autocomp(ch):
		'''
		When autocompleteing will earch back and 
		'''
		if ch in '._[] "\'':
			return False
		if ch.isalnum():
			return False
		
		return True

	
	def do_autocomp(autocomp_prefix, autocomp_members):
		'''
		return text to insert and a list of options
		'''
		autocomp_members = [v for v in autocomp_members if v.startswith(autocomp_prefix)]
		
		print("AUTO: '%s'" % autocomp_prefix)
		print("MEMBERS: '%s'" % str(autocomp_members))
		
		if not autocomp_prefix:
			return '', autocomp_members
		elif len(autocomp_members) > 1:
			# find a common string between all members after the prefix 
			# 'ge' [getA, getB, getC] --> 'get'
			
			# get the shortest member
			min_len = min([len(v) for v in autocomp_members])
			
			autocomp_prefix_ret = ''
			
			for i in range(len(autocomp_prefix), min_len):
				char_soup = set()
				for v in autocomp_members:
					char_soup.add(v[i])
				
				if len(char_soup) > 1:
					break
				else:
					autocomp_prefix_ret += char_soup.pop()
				
			print(autocomp_prefix_ret)
			return autocomp_prefix_ret, autocomp_members
		elif len(autocomp_members) == 1:
			return autocomp_members[0][len(autocomp_prefix):], []
		else:
			return '', []
	

	def BCon_PrevChar(bcon):
		cursor = bcon['cursor']-1
		if cursor<0:
			return None
			
		try:
			return bcon['edit_text'][cursor]
		except:
			return None
		
		
	def BCon_NextChar(bcon):
		try:
			return bcon['edit_text'][bcon['cursor']]
		except:
			return None
	
	def BCon_cursorLeft(bcon):
		bcon['cursor'] -= 1
		if bcon['cursor'] < 0:
			bcon['cursor'] = 0

	def BCon_cursorRight(bcon):
			bcon['cursor'] += 1
			if bcon['cursor'] > len(bcon['edit_text']):
				bcon['cursor'] = len(bcon['edit_text'])
	
	def BCon_AddScrollback(bcon, text):
		
		bcon['scrollback'] = bcon['scrollback'] + text
		
	
	def BCon_cursorInsertChar(bcon, ch):
		if bcon['cursor']==0:
			bcon['edit_text'] = ch + bcon['edit_text']
		elif bcon['cursor']==len(bcon['edit_text']):
			bcon['edit_text'] = bcon['edit_text'] + ch
		else:
			bcon['edit_text'] = bcon['edit_text'][:bcon['cursor']] + ch + bcon['edit_text'][bcon['cursor']:]
			
		bcon['cursor'] 
		if bcon['cursor'] > len(bcon['edit_text']):
			bcon['cursor'] = len(bcon['edit_text'])
		BCon_cursorRight(bcon)
	
	
	TEMP_NAME = '___tempname___'
	
	cursor_orig = bcon['cursor']
	
	ch = BCon_PrevChar(bcon)
	while ch != None and (not is_delimiter(ch)):
		ch = BCon_PrevChar(bcon)
		BCon_cursorLeft(bcon)
	
	if ch != None:
		BCon_cursorRight(bcon)
	
	#print (cursor_orig, bcon['cursor'])
	
	cursor_base = bcon['cursor']
	
	autocomp_prefix = bcon['edit_text'][cursor_base:cursor_orig]
	
	print("PREFIX:'%s'" % autocomp_prefix)
	
	# Get the previous word
	if BCon_PrevChar(bcon)=='.':
		BCon_cursorLeft(bcon)
		ch = BCon_PrevChar(bcon)
		while ch != None and is_delimiter_autocomp(ch)==False:
			ch = BCon_PrevChar(bcon)
			BCon_cursorLeft(bcon)
		
		cursor_new = bcon['cursor']
		
		if ch != None:
			cursor_new+=1
		
		pytxt = bcon['edit_text'][cursor_new:cursor_base-1].strip()
		print("AUTOCOMP EVAL: '%s'" % pytxt)
		#try:
		if pytxt:
			bcon['console'].runsource(TEMP_NAME + '=' + pytxt, '<input>', 'single')
			# print val
		else: ##except:
			val = None
		
		try:
			val = bcon['namespace'][TEMP_NAME]
			del bcon['namespace'][TEMP_NAME]
		except:
			val = None
		
		if val:
			autocomp_members = dir(val)
			
			autocomp_prefix_ret, autocomp_members = do_autocomp(autocomp_prefix, autocomp_members)
			
			bcon['cursor'] = cursor_orig
			for v in autocomp_prefix_ret:
				BCon_cursorInsertChar(bcon, v)
			cursor_orig = bcon['cursor']
			
			if autocomp_members:
				BCon_AddScrollback(bcon, ', '.join(autocomp_members))
		
		del val
		
	else:
		# Autocomp global namespace
		autocomp_members = bcon['namespace'].keys()
		
		if autocomp_prefix:
			autocomp_members = [v for v in autocomp_members if v.startswith(autocomp_prefix)]
		
		autocomp_prefix_ret, autocomp_members = do_autocomp(autocomp_prefix, autocomp_members)
		
		bcon['cursor'] = cursor_orig
		for v in autocomp_prefix_ret:
			BCon_cursorInsertChar(bcon, v)
		cursor_orig = bcon['cursor']
		
		if autocomp_members:
			BCon_AddScrollback(bcon, ', '.join(autocomp_members))
	
	bcon['cursor'] = cursor_orig


class TEXT_OT_console_autocomplete(bpy.types.Operator):
	'''
	Operator documentatuon text, will be used for the operator tooltip and python docs.
	'''
	__label__ = "Console Autocomplete"
	
	def execute(self, context):
		
		st = context.space_data
		text = st.text
		
		namespace, console, stdout = get_console(text)
		
		line = text.current_line.line
		
		if not console:
			return ('CANCELLED',)
		
		
		# fake cursor, use for autocomp func.
		bcon = {}
		bcon['cursor'] = text.current_character
		bcon['console'] = console
		bcon['edit_text'] = line
		bcon['namespace'] = namespace
		bcon['scrollback'] = '' # nor from the BGE console
		
		
		# This function isnt aware of the text editor or being an operator
		# just does the autocomp then copy its results back
		autocomp(bcon)
		
		# Now we need to copy back the line from blender back into the text editor.
		# This will change when we dont use the text editor anymore
		
		# clear the line
		bpy.ops.TEXT_OT_move(type='LINE_END')
		bpy.ops.TEXT_OT_move_select(type = 'LINE_BEGIN')
		bpy.ops.TEXT_OT_delete(type = 'PREVIOUS_CHARACTER')
		
		if bcon['scrollback']:
			bpy.ops.TEXT_OT_move_select(type = 'LINE_BEGIN')
			bpy.ops.TEXT_OT_insert(text = bcon['scrollback'].strip() + '\n')
			bpy.ops.TEXT_OT_move_select(type='LINE_BEGIN')
		
		bpy.ops.TEXT_OT_insert(text = bcon['edit_text'])
		
		# Read only
		if 0:
			text.current_character = bcon['cursor']
		else:
			bpy.ops.TEXT_OT_move(type = 'LINE_BEGIN')
			
			for i in range(bcon['cursor']):
				bpy.ops.TEXT_OT_move(type='NEXT_CHARACTER')
			
		
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

bpy.ops.add(TEXT_OT_console_exec)
bpy.ops.add(TEXT_OT_console_autocomplete)

