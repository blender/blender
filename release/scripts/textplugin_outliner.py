#!BPY
"""
Name: 'Outline'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Ctrl+T'
Tooltip: 'Provides a menu for jumping to class and functions definitions.'
"""

# Only run if we have the required modules
try:
	import bpy
	from BPyTextPlugin import *
	from Blender import Draw
	OK = True
except ImportError:
	OK = False

def do_long_menu(title, items):
	n = len(items)
	if n < 20:
		return Draw.PupMenu(title+'%t|'+'|'.join(items))
	
	letters = []
	check = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ_' # Cannot start 0-9 so just letters
	for c in check:
		for item in items:
			if item[0].upper() == c:
				letters.append(c)
				break
	
	i = Draw.PupMenu(title+'%t|'+'|'.join(letters))
	if i < 1:
		return i
	
	c = letters[i-1]
	newitems = []
	
	i = 0
	for item in items:
		i += 1
		if item[0].upper() == c:
			newitems.append(item+'%x'+str(i))
	
	return Draw.PupMenu(title+'%t|'+'|'.join(newitems))

def main():
	txt = bpy.data.texts.active
	if not txt:
		return
	
	items = []
	i = Draw.PupMenu('Outliner%t|Classes|Defs|Variables')
	if i < 1: return
	
	script = get_cached_descriptor(txt)
	if i == 1:
		type = script.classes
	elif i == 2:
		type = script.defs
	elif i == 3:
		type = script.vars
	else:
		return
	items.extend(type.keys())
	items.sort(cmp = suggest_cmp)
	i = do_long_menu('Outliner', items)
	if i < 1:
		return
	
	try:
		desc = type[items[i-1]]
	except:
		desc = None
	
	if desc:
		txt.setCursorPos(desc.lineno-1, 0)

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
