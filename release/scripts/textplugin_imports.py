#!BPY
"""
Name: 'Import Complete|Space'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Space'
Tooltip: 'Lists modules when import or from is typed'
"""

# Only run if we have the required modules
try:
	import bpy, sys
	from BPyTextPlugin import *
except ImportError:
	OK = False
else:
	OK = True

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
		# Check instead for straight 'import xxxx'
		pos2 = line.rfind('import ', 0, c)
		if pos2 != -1:
			pos2 += 7
			for i in range(pos2, c):
				if line[i]==',' or (line[i]==' ' and line[i-1]==','):
					pos2 = i+1
				elif not line[i].isalnum() and line[i] != '_':
					return
			items = [(m, 'm') for m in get_modules()]
			items.sort(cmp = suggest_cmp)
			txt.suggest(items, line[pos2:c].strip())
		return
	
	# Found 'from xxxxx' before cursor
	immediate = True
	pos += 5
	for i in range(pos, c):
		if not line[i].isalnum() and line[i] != '_' and line[i] != '.':
			immediate = False
			break
	
	# Immediate 'from' followed by at most a module name
	if immediate:
		items = [(m, 'm') for m in get_modules()]
		items.sort(cmp = suggest_cmp)
		txt.suggest(items, line[pos:c])
		return
	
	# Found 'from' earlier, suggest import if not already there
	pos2 = line.rfind('import ', pos, c)
	
	# No 'import' found after 'from' so suggest it
	if pos2 == -1:
		txt.suggest([('import', 'k')], '')
		return
		
	# Immediate 'import' before cursor and after 'from...'
	for i in range(pos2+7, c):
		if line[i]==',' or (line[i]==' ' and line[i-1]==','):
			pass
		elif not line[i].isalnum() and line[i] != '_':
			return
	between = line[pos:pos2-1].strip()
	try:
		mod = get_module(between)
	except ImportError:
		return
	
	items = [('*', 'k')]
	for (k,v) in mod.__dict__.items():
		items.append((k, type_char(v)))
	items.sort(cmp = suggest_cmp)
	txt.suggest(items, '')

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
