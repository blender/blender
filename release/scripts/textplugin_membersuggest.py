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
except ImportError:
	OK = False

def main():
	txt = bpy.data.texts.active
	(line, c) = current_line(txt)
	
	# Check we are in a normal context
	if get_context(txt) != NORMAL:
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
	except AttributeError:
		print "Attribute not found '%s' in '%s'" % (name, '.'.join(pre))
		return
	
	try:
		attr = obj.__dict__.keys()
	except AttributeError:
		attr = dir(obj)
	
	for k in attr:
		try:
			v = getattr(obj, k)
			list.append((k, type_char(v)))
		except (AttributeError, TypeError): # Some attributes are not readable
			pass
	
	if list != []:
		list.sort(cmp = suggest_cmp)
		txt.suggest(list, pre[-1])

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
