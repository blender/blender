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
	
	pre = get_targets(line, c)
	
	if len(pre) <= 1:
		return
	
	imports = get_imports(txt)
	builtins = get_builtins()
	
	# Identify the root (root.sub.sub.)
	obj = None
	if pre[0] == '':
		i = c - len('.'.join(pre)) - 1
		if i >= 0:
			if line[i] == '"' or line[i] == "'":
				obj = str
			elif line[i] == '}':
				obj = dict
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
					obj = list
	elif imports.has_key(pre[0]):
		obj = imports[pre[0]]
	elif builtins.has_key(pre[0]):
		obj = builtins[pre[0]]
	else:
		desc = get_cached_descriptor(txt)
		if desc.vars.has_key(pre[0]):
			obj = desc.vars[pre[0]].type
	
	if not obj:
		return
	
	# Step through sub-attributes
	try:
		for name in pre[1:-1]:
			obj = getattr(obj, name)
	except AttributeError:
		print "Attribute not found '%s' in '%s'" % (name, '.'.join(pre))
		return
	
	try:
		attr = obj.__dict__.keys()
	except AttributeError:
		attr = dir(obj)
	else:
		if not attr:
			attr = dir(obj)
	
	items = []
	for k in attr:
		try:
			v = getattr(obj, k)
		except (AttributeError, TypeError): # Some attributes are not readable
			pass
		else:
			items.append((k, type_char(v)))
	
	if items != []:
		items.sort(cmp = suggest_cmp)
		txt.suggest(items, pre[-1])

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
