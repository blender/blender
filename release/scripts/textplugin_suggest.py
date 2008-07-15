#!BPY
"""
Name: 'Suggest All'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Ctrl+Space'
Tooltip: 'Performs suggestions based on the context of the cursor'
"""

# Only run if we have the required modules
try:
	import bpy
	from BPyTextPlugin import *
	OK = True
except:
	OK = False

def main():
	txt = bpy.data.texts.active
	(line, c) = current_line(txt)
	
	# Check we are in a normal context
	if get_context(line, c) != NORMAL:
		return
	
	# Check that which precedes the cursor and perform the following:
	# Period(.)				- Run textplugin_membersuggest.py
	# 'import' or 'from'	- Run textplugin_imports.py
	# Other                 - Continue this script (global suggest)
	pre = get_targets(line, c)
	
	count = len(pre)
	
	if count > 1: # Period found
		import textplugin_membersuggest
		textplugin_membersuggest.main()
		return
	# Look for 'import' or 'from'
	elif line.rfind('import ', 0, c) == c-7 or line.rfind('from ', 0, c) == c-5:
		import textplugin_imports
		textplugin_imports.main()
		return
	
	list = []
	
	for k in KEYWORDS:
		list.append((k, 'k'))
	
	for k, v in get_builtins().items():
		list.append((k, type_char(v)))
	
	for k, v in get_imports(txt).items():
		list.append((k, type_char(v)))
	
	for k, v in get_defs(txt).items():
		list.append((k, 'f'))
	
	list.sort(cmp = suggest_cmp)
	txt.suggest(list, pre[-1])

if OK:
	main()
