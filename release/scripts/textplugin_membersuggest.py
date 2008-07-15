#!BPY
"""
Name: 'Member Suggest'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Period'
Tooltip: 'Lists members of the object preceding the cursor in the current text \
space'
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
	
	pre = get_targets(line, c)
	
	if len(pre) <= 1:
		return
	
	list = []
	
	imports = get_imports(txt)
	
	# Identify the root (root.sub.sub.)
	if imports.has_key(pre[0]):
		obj = imports[pre[0]]
	else:
		return
	
	# Step through sub-attributes
	try:
		for name in pre[1:-1]:
			obj = getattr(obj, name)
	except:
		print "Attribute not found '%s' in '%s'" % (name, '.'.join(pre))
		return
	
	try:
		attr = obj.__dict__.keys()
	except:
		attr = dir(obj)
	
	for k in attr:
		v = getattr(obj, k)
		if is_module(v): t = 'm'
		elif callable(v): t = 'f'
		else: t = 'v'
		list.append((k, t))
	
	if list != []:
		list.sort(cmp = suggest_cmp)
		txt.suggest(list, pre[-1])

if OK:
	main()
