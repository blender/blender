#!BPY
"""
Name: 'Function Documentation | Ctrl I'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Ctrl+I'
Tooltip: 'Attempts to display documentation about the function preceding the cursor.'
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
	
	# Identify the name under the cursor
	llen = len(line)
	while c<llen and (line[c].isalnum() or line[c]=='_'):
		c += 1
	
	targets = get_targets(line, c)
	
	# If no name under cursor, look backward to see if we're in function parens
	if len(targets) == 0 or targets[0] == '':
		# Look backwards for first '(' without ')'
		b = 0
		found = False
		for i in range(c-1, -1, -1):
			if line[i] == ')': b += 1
			elif line[i] == '(':
				b -= 1
				if b < 0:
					found = True
					c = i
					break
		if found: targets = get_targets(line, c)
		if len(targets) == 0 or targets[0] == '':
			return
	
	obj = resolve_targets(txt, targets)
	if not obj: return
	
	if isinstance(obj, Definition): # Local definition
		txt.showDocs(obj.doc)
	elif hasattr(obj, '__doc__') and obj.__doc__:
		txt.showDocs(obj.__doc__)

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
