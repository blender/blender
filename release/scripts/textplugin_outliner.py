#!BPY
"""
Name: 'Code Outline | Ctrl T'
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
except ImportError:
	OK = False
else:
	OK = True

def make_menu(items, eventoffs):
	n = len(items)
	if n < 20:
		return [(items[i], i+1+eventoffs) for i in range(len(items))]
	
	letters = []
	check = 'abcdefghijklmnopqrstuvwxyz_' # Names cannot start 0-9
	for c in check:
		for item in items:
			if item[0].lower() == c:
				letters.append(c)
				break
	
	entries = {}
	i = 0
	for item in items:
		i += 1
		c = item[0].lower()
		entries.setdefault(c, []).append((item, i+eventoffs))
	
	subs = []
	for c in letters:
		subs.append((c, entries[c]))
	
	return subs

def find_word(txt, word):
	i = 0
	txt.reset()
	while True:
		try:
			line = txt.readline()
		except StopIteration:
			break
		c = line.find(word)
		if c != -1:
			txt.setCursorPos(i, c)
			break
		i += 1

def main():
	txt = bpy.data.texts.active
	if not txt:
		return
	
	# Identify word under cursor
	if get_context(txt) == CTX_NORMAL:
		line, c = current_line(txt)
		start = c-1
		end = c
		while start >= 0:
			if not line[start].lower() in 'abcdefghijklmnopqrstuvwxyz0123456789_':
				break
			start -= 1
		while end < len(line):
			if not line[end].lower() in 'abcdefghijklmnopqrstuvwxyz0123456789_':
				break
			end += 1
		word = line[start+1:end]
		if word in KEYWORDS:
			word = None
	else:
		word = None
	
	script = get_cached_descriptor(txt)
	items = []
	desc = None
	
	tmp = script.classes.keys()
	tmp.sort(cmp = suggest_cmp)
	class_menu = make_menu(tmp, len(items))
	class_menu_length = len(tmp)
	items.extend(tmp)
	
	tmp = script.defs.keys()
	tmp.sort(cmp = suggest_cmp)
	defs_menu = make_menu(tmp, len(items))
	defs_menu_length = len(tmp)
	items.extend(tmp)
	
	tmp = script.vars.keys()
	tmp.sort(cmp = suggest_cmp)
	vars_menu = make_menu(tmp, len(items))
	vars_menu_length = len(tmp)
	items.extend(tmp)
	
	menu = [('Script %t', 0),
			('Classes', class_menu),
			('Functions', defs_menu),
			('Variables', vars_menu)]
	if word:
		menu.extend([None, ('Locate', [(word, -10)])])
	
	i = Draw.PupTreeMenu(menu)
	if i == -1:
		return
	
	# Chosen to search for word under cursor
	if i == -10:
		if script.classes.has_key(word):
			desc = script.classes[word]
		elif script.defs.has_key(word):
			desc = script.defs[word]
		elif script.vars.has_key(word):
			desc = script.vars[word]
		else:
			find_word(txt, word)
			return
	else:
		i -= 1
		if i < class_menu_length:
			desc = script.classes[items[i]]
		elif i < class_menu_length + defs_menu_length:
			desc = script.defs[items[i]]
		elif i < class_menu_length + defs_menu_length + vars_menu_length:
			desc = script.vars[items[i]]
	
	if desc:
		txt.setCursorPos(desc.lineno-1, 0)

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
