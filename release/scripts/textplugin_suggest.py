#!BPY
"""
Name: 'Suggest All | Ctrl Space'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Ctrl+Space'
Tooltip: 'Performs suggestions based on the context of the cursor'
"""

# Only run if we have the required modules
try:
	import bpy
	from BPyTextPlugin import *
except ImportError:
	OK = False
else:
	OK = True

def check_membersuggest(line, c):
	pos = line.rfind('.', 0, c)
	if pos == -1:
		return False
	for s in line[pos+1:c]:
		if not s.isalnum() and s != '_':
			return False
	return True

def check_imports(line, c):
	pos = line.rfind('import ', 0, c)
	if pos > -1:
		for s in line[pos+7:c]:
			if not s.isalnum() and s != '_':
				return False
		return True
	pos = line.rfind('from ', 0, c)
	if pos > -1:
		for s in line[pos+5:c]:
			if not s.isalnum() and s != '_':
				return False
		return True
	return False

def main():
	txt = bpy.data.texts.active
	if not txt:
		return
	
	line, c = current_line(txt)
	
	# Check we are in a normal context
	if get_context(txt) != CTX_NORMAL:
		return
	
	# Check the character preceding the cursor and execute the corresponding script
	
	if check_membersuggest(line, c):
		import textplugin_membersuggest
		textplugin_membersuggest.main()
		return
	
	elif check_imports(line, c):
		import textplugin_imports
		textplugin_imports.main()
		return
	
	# Otherwise we suggest globals, keywords, etc.
	list = []
	targets = get_targets(line, c)
	desc = get_cached_descriptor(txt)
	
	for k in KEYWORDS:
		list.append((k, 'k'))
	
	for k, v in get_builtins().items():
		list.append((k, type_char(v)))
	
	for k, v in desc.imports.items():
		list.append((k, type_char(v)))
	
	for k, v in desc.classes.items():
		list.append((k, 'f'))
	
	for k, v in desc.defs.items():
		list.append((k, 'f'))
	
	for k, v in desc.vars.items():
		list.append((k, 'v'))
	
	list.sort(cmp = suggest_cmp)
	txt.suggest(list, targets[-1])

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
