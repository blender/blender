#!BPY
"""
Name: 'Text Plugin'
Blender: 246
Group: 'ScriptTemplate'
Tooltip: 'Add a new text for writing a text plugin'
"""

from Blender import Window
import bpy

script_data = \
'''#!BPY
"""
Name: 'My Plugin Script'
Blender: 246
Group: 'TextPlugin'
Shortcut: 'Ctrl+Alt+U'
Tooltip: 'Put some useful info here'
"""

# Add a licence here if you wish to re-distribute, we recommend the GPL

from Blender import Window, sys
import BPyTextPlugin, bpy

def my_script_util(txt):
	# This function prints out statistical information about a script
	
	desc = BPyTextPlugin.get_cached_descriptor(txt)
	print '---------------------------------------'
	print 'Script Name:', desc.name
	print 'Classes:', len(desc.classes)
	print '  ', desc.classes.keys()
	print 'Functions:', len(desc.defs)
	print '  ', desc.defs.keys()
	print 'Variables:', len(desc.vars)
	print '  ', desc.vars.keys()

def main():
	
	# Gets the active text object, there can be many in one blend file.
	txt = bpy.data.texts.active
	
	# Silently return if the script has been run with no active text
	if not txt:
		return 
	
	# Text plug-ins should run quickly so we time it here
	Window.WaitCursor(1)
	t = sys.time()
	
	# Run our utility function
	my_script_util(txt)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'Plugin script finished in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	

# This lets you import the script without running it
if __name__ == '__main__':
	main()
'''

new_text = bpy.data.texts.new('textplugin_template.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
