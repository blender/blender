
import bpy

import bpy_ops # XXX - should not need to do this
del bpy_ops

class CONSOLE_HT_header(bpy.types.Header):
	__space_type__ = 'CONSOLE'

	def draw(self, context):
		sc = context.space_data
		# text = sc.text
		layout = self.layout

		row= layout.row(align=True)
		row.template_header()

		if context.area.show_menus:
			sub = row.row(align=True)

			if sc.console_type == 'REPORT':
				sub.itemM("CONSOLE_MT_report")
			else:
				sub.itemM("CONSOLE_MT_console")

		layout.itemS()
		layout.itemR(sc, "console_type", expand=True)

		if sc.console_type == 'REPORT':
			row = layout.row(align=True)
			row.itemR(sc, "show_report_debug", text="Debug")
			row.itemR(sc, "show_report_info", text="Info")
			row.itemR(sc, "show_report_operator", text="Operators")
			row.itemR(sc, "show_report_warn", text="Warnings")
			row.itemR(sc, "show_report_error", text="Errors")
			
			row = layout.row()
			row.enabled = sc.show_report_operator
			row.itemO("console.report_replay")
		else:
			row = layout.row(align=True)
			row.itemO("console.autocomplete", text="Autocomplete")

class CONSOLE_MT_console(bpy.types.Menu):
	__space_type__ = 'CONSOLE'
	__label__ = "Console"

	def draw(self, context):
		layout = self.layout
		sc = context.space_data

		layout.column()
		layout.itemO("console.clear")
		layout.itemO("console.copy")
		layout.itemO("console.paste")

class CONSOLE_MT_report(bpy.types.Menu):
	__space_type__ = 'CONSOLE'
	__label__ = "Report"

	def draw(self, context):
		layout = self.layout
		sc = context.space_data

		layout.column()
		layout.itemO("console.select_all_toggle")
		layout.itemO("console.select_border")
		layout.itemO("console.report_delete")
		layout.itemO("console.report_copy")

def add_scrollback(text, text_type):
	for l in text.split('\n'):
		bpy.ops.console.scrollback_append(text=l.replace('\t', '    '), type=text_type)

def get_console(console_id):
	'''
	helper function for console operators
	currently each text datablock gets its own console - code.InteractiveConsole()
	...which is stored in this function.
	
	console_id can be any hashable type
	'''
	import sys, code
	
	try:	consoles = get_console.consoles
	except:consoles = get_console.consoles = {}
	
	# clear all dead consoles, use text names as IDs
	# TODO, find a way to clear IDs
	'''
	for console_id in list(consoles.keys()):
		if console_id not in bpy.data.texts:
			del consoles[id]
	'''
	
	try:
		namespace, console, stdout, stderr = consoles[console_id]
	except:
		namespace = {'__builtins__':__builtins__} # locals()
		namespace['bpy'] = bpy
		
		console = code.InteractiveConsole(namespace)
		
		import io
		stdout = io.StringIO()
		stderr = io.StringIO()
	
		consoles[console_id]= namespace, console, stdout, stderr
		
	return namespace, console, stdout, stderr

class CONSOLE_OT_exec(bpy.types.Operator):
	'''Execute the current console line as a python expression.'''
	__idname__ = "console.execute"
	__label__ = "Console Execute"
	__register__ = False
	
	# Both prompts must be the same length
	PROMPT = '>>> ' 
	PROMPT_MULTI = '... '
	
	# is this working???
	'''
	def poll(self, context):
		return (context.space_data.type == 'PYTHON')
	''' # its not :|
	
	def execute(self, context):
		import sys
		
		sc = context.space_data
		
		try:
			line = sc.history[-1].line
		except:
			return ('CANCELLED',)
		
		if sc.console_type != 'PYTHON':
			return ('CANCELLED',)
		
		namespace, console, stdout, stderr = get_console(hash(context.region))
		
		# redirect output
		sys.stdout = stdout
		sys.stderr = stderr
		
		# run the console
		if not line.strip():
			line_exec = '\n' # executes a multiline statement
		else:
			line_exec = line
		
		is_multiline = console.push(line_exec)
		
		stdout.seek(0)
		stderr.seek(0)
		
		output = stdout.read()
		output_err = stderr.read()
	
		# cleanup
		sys.stdout = sys.__stdout__
		sys.stderr = sys.__stderr__
		sys.last_traceback = None
		
		# So we can reuse, clear all data
		stdout.truncate(0)
		stderr.truncate(0)
		
		bpy.ops.console.scrollback_append(text = sc.prompt+line, type='INPUT')
		
		if is_multiline:	sc.prompt = self.PROMPT_MULTI
		else:				sc.prompt = self.PROMPT
		
		# insert a new blank line
		bpy.ops.console.history_append(text="", current_character=0, remove_duplicates= True)
		
		# Insert the output into the editor
		# not quite correct because the order might have changed, but ok 99% of the time.
		if output:			add_scrollback(output, 'OUTPUT')
		if output_err:		add_scrollback(output_err, 'ERROR')
		
		
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
			
			return autocomp_prefix_ret, autocomp_members
		elif len(autocomp_members) == 1:
			if autocomp_prefix == autocomp_members[0]:
				# the variable matched the prefix exactly
				# add a '.' so you can quickly continue.
				# Could try add [] or other possible extensions rather then '.' too if we had the variable.
				return '.', []
			else:
				# finish off the of the word word
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


class CONSOLE_OT_autocomplete(bpy.types.Operator):
	'''Evaluate the namespace up until the cursor and give a list of options or complete the name if there is only one.'''
	__idname__ = "console.autocomplete"
	__label__ = "Console Autocomplete"
	__register__ = False
	
	def poll(self, context):
		return context.space_data.console_type == 'PYTHON'
	
	def execute(self, context):
		
		sc = context.space_data
		
		namespace, console, stdout, stderr = get_console(hash(context.region))
		
		current_line = sc.history[-1]
		line = current_line.line
		
		if not console:
			return ('CANCELLED',)
		
		if sc.console_type != 'PYTHON':
			return ('CANCELLED',)
		
		# fake cursor, use for autocomp func.
		bcon = {}
		bcon['cursor'] = current_line.current_character
		bcon['console'] = console
		bcon['edit_text'] = line
		bcon['namespace'] = namespace
		bcon['scrollback'] = '' # nor from the BGE console
		
		
		# This function isnt aware of the text editor or being an operator
		# just does the autocomp then copy its results back
		autocomp(bcon)
		
		# Now we need to copy back the line from blender back into the text editor.
		# This will change when we dont use the text editor anymore
		if bcon['scrollback']:
			add_scrollback(bcon['scrollback'], 'INFO')
		
		# copy back
		current_line.line = bcon['edit_text']
		current_line.current_character = bcon['cursor']
		
		context.area.tag_redraw()
		
		return ('FINISHED',)



bpy.types.register(CONSOLE_HT_header)
bpy.types.register(CONSOLE_MT_console)
bpy.types.register(CONSOLE_MT_report)

bpy.ops.add(CONSOLE_OT_exec)
bpy.ops.add(CONSOLE_OT_autocomplete)

