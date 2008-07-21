#!BPY
"""
Name: 'Function Documentation'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Ctrl+I'
Tooltip: 'Attempts to display documentation about the function preceding the cursor.'
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
	if not txt:
		return
	
	(line, c) = current_line(txt)
	
	# Check we are in a normal context
	if get_context(txt) != CTX_NORMAL:
		return
	
	# Look backwards for first '(' without ')'
	b = 0
	for i in range(c-1, -1, -1):
		if line[i] == ')': b += 1
		elif line[i] == '(':
			b -= 1
			if b < 0:
				c = i
				break
	
	pre = get_targets(line, c)
	
	if len(pre) == 0:
		return
	
	imports = get_imports(txt)
	builtins = get_builtins()
	
	# Identify the root (root.sub.sub.)
	if imports.has_key(pre[0]):
		obj = imports[pre[0]]
	elif builtins.has_key(pre[0]):
		obj = builtins[pre[0]]
	else:
		return
	
	# Step through sub-attributes
	try:
		for name in pre[1:]:
			obj = getattr(obj, name)
	except AttributeError:
		print "Attribute not found '%s' in '%s'" % (name, '.'.join(pre))
		return
	
	if hasattr(obj, '__doc__') and obj.__doc__:
		txt.showDocs(obj.__doc__)

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
