#!BPY
"""
Name: 'Member Suggest | .'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Period'
Tooltip: 'Lists members of the object preceding the cursor in the current text space'
"""

# Only run if we have the required modules
try:
	import bpy
	from BPyTextPlugin import *
except ImportError:
	OK = False
else:
	OK = True

def main():
	txt = bpy.data.texts.active
	if not txt:
		return
	
	(line, c) = current_line(txt)
	
	# Check we are in a normal context
	if get_context(txt) != CTX_NORMAL:
		return
	
	targets = get_targets(line, c)
	
	if targets[0] == '': # Check if we are looking at a constant [] {} '' etc.
		i = c - len('.'.join(targets)) - 1
		if i >= 0:
			if line[i] == '"' or line[i] == "'":
				targets[0] = 'str'
			elif line[i] == '}':
				targets[0] = 'dict'
			elif line[i] == ']': # Could be array elem x[y] or list [y]
				i = line.rfind('[', 0, i) - 1
				while i >= 0:
					if line[i].isalnum() or line[i] == '_':
						break
					elif line[i] != ' ' and line[i] != '\t':
						i = -1
						break
					i -= 1
				if i < 0: 
					targets[0] = 'list'
	
	obj = resolve_targets(txt, targets[:-1])
	if not obj:
		return
	
	items = []
	
	if isinstance(obj, VarDesc):
		obj = obj.type
		
	if isinstance(obj, Definition): # Locally defined
		if hasattr(obj, 'classes'):
			items.extend([(s, 'f') for s in obj.classes.keys()])
		if hasattr(obj, 'defs'):
			items.extend([(s, 'f') for s in obj.defs.keys()])
		if hasattr(obj, 'vars'):
			items.extend([(s, 'v') for s in obj.vars.keys()])
	
	else: # Otherwise we have an imported or builtin object
		try:
			attr = obj.__dict__.keys()
		except AttributeError:
			attr = dir(obj)
		else:
			if not attr: attr = dir(obj)
		
		for k in attr:
			try:
				v = getattr(obj, k)
			except (AttributeError, TypeError): # Some attributes are not readable
				pass
			else:
				items.append((k, type_char(v)))
	
	if items != []:
		items.sort(cmp = suggest_cmp)
		txt.suggest(items, targets[-1])

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
