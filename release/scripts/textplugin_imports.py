#!BPY
"""
Name: 'Import Complete'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Space'
Tooltip: 'Lists modules when import or from is typed'
"""

# Only run if we have the required modules
OK = False
try:
	import bpy, sys
	from BPyTextPlugin import *
	OK = True
except ImportError:
	pass

def main():
	txt = bpy.data.texts.active
	if not txt:
		return
	
	line, c = current_line(txt)
	
	# Check we are in a normal context
	if get_context(txt) != CTX_NORMAL:
		return
	
	pos = line.rfind('from ', 0, c)
	
	# No 'from' found
	if pos == -1:
		# Check instead for straight 'import'
		pos2 = line.rfind('import ', 0, c)
		if pos2 != -1 and (pos2 == c-7 or (pos2 < c-7 and line[c-2]==',')):
			items = [(m, 'm') for m in get_modules()]
			items.sort(cmp = suggest_cmp)
			txt.suggest(items, '')
	
	# Immediate 'from' before cursor
	elif pos == c-5:
		items = [(m, 'm') for m in get_modules()]
		items.sort(cmp = suggest_cmp)
		txt.suggest(items, '')
	
	# Found 'from' earlier
	else:
		pos2 = line.rfind('import ', pos+5, c)
		
		# No 'import' found after 'from' so suggest it
		if pos2 == -1:
			txt.suggest([('import', 'k')], '')
			
		# Immediate 'import' before cursor and after 'from...'
		elif pos2 == c-7 or line[c-2] == ',':
			between = line[pos+5:pos2-1].strip()
			try:
				mod = get_module(between)
			except ImportError:
				print 'Module not found:', between
				return
			
			items = [('*', 'k')]
			for (k,v) in mod.__dict__.items():
				items.append((k, type_char(v)))
			items.sort(cmp = suggest_cmp)
			txt.suggest(items, '')

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
