#!BPY
"""
Name: 'Template Completion | Tab'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Tab'
Tooltip: 'Completes templates based on the text preceding the cursor'
"""

# Only run if we have the required modules
try:
	import bpy
	from BPyTextPlugin import *
	from Blender import Text
except ImportError:
	OK = False
else:
	OK = True

templates = {
	'ie':
		'if ${1:cond}:\n'
		'\t${2}\n'
		'else:\n'
		'\t${3}\n',
	'iei':
		'if ${1:cond}:\n'
		'\t${2}\n'
		'elif:\n'
		'\t${3}\n'
		'else:\n'
		'\t${4}\n',
	'def':
		'def ${1:name}(${2:params}):\n'
		'\t"""(${2}) - ${3:comment}"""\n'
		'\t${4}',
	'cls':
		'class ${1:name}(${2:parent}):\n'
		'\t"""${3:docs}"""\n'
		'\t\n'
		'\tdef __init__(self, ${4:params}):\n'
		'\t\t"""Creates a new ${1}"""\n'
		'\t\t${5}',
	'class':
		'class ${1:name}(${2:parent}):\n'
		'\t"""${3:docs}"""\n'
		'\t\n'
		'\tdef __init__(self, ${4:params}):\n'
		'\t\t"""Creates a new ${1}"""\n'
		'\t\t${5}'
}

def main():
	txt = bpy.data.texts.active
	if not txt:
		return
	
	row, c = txt.getCursorPos()
	line = txt.asLines(row, row+1)[0]
	indent=0
	while indent<c and (line[indent]==' ' or line[indent]=='\t'):
		indent += 1
	
	# Check we are in a normal context
	if get_context(txt) != CTX_NORMAL:
		return
	
	targets = get_targets(line, c-1);
	if len(targets) != 1: return
	
	color = (0, 192, 32)
	
	for trigger, template in templates.items():
		if trigger != targets[0]: continue
		inserts = {}
		txt.delete(-len(trigger)-1)
		y, x = txt.getCursorPos()
		first = None
		
		# Insert template text and parse for insertion points
		count = len(template); i = 0
		while i < count:
			if i<count-1 and template[i]=='$' and template[i+1]=='{':
				i += 2
				e = template.find('}', i)
				item = template[i:e].split(':')
				if len(item)<2: item.append('')
				if not inserts.has_key(item[0]):
					inserts[item[0]] = (item[1], [(x, y)])
				else:
					inserts[item[0]][1].append((x, y))
					item[1] = inserts[item[0]][0]
				if not first: first = (item[1], x, y)
				txt.insert(item[1])
				x += len(item[1])
				i = e
			else:
				txt.insert(template[i])
				if template[i] == '\n':
					txt.insert(line[:indent])
					y += 1
					x = indent
				else:
					x += 1
			i += 1
		
		# Insert markers at insertion points
		for id, (text, points) in inserts.items():
			for x, y in points:
				txt.setCursorPos(y, x)
				txt.setSelectPos(y, x+len(text))
				txt.markSelection((hash(text)+int(id)) & 0xFFFF, color,
						Text.TMARK_TEMP | Text.TMARK_EDITALL)
		if first:
			text, x, y = first
			txt.setCursorPos(y, x)
			txt.setSelectPos(y, x+len(text))
		break
		

# Check we are running as a script and not imported as a module
if __name__ == "__main__" and OK:
	main()
