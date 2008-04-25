#!BPY
"""
Name: 'Object Editing'
Blender: 243
Group: 'ScriptTemplate'
Tooltip: 'Add a new text for editing selected objects'
"""

from Blender import Window
import bpy

script_data = \
'''#!BPY
"""
Name: 'My Object Script'
Blender: 245
Group: 'Object'
Tooltip: 'Put some useful info here'
"""

# Add a licence here if you wish to re-distribute, we recommend the GPL

from Blender import Window, sys
import bpy

def my_object_util(sce):
	
	# Remove these when writing your own tool
	print 'Blend object count', len(bpy.data.objects)
	print 'Scene object count', len(sce.objects)
	
	# context means its selected, in the view layer and not hidden.
	print 'Scene context count', len(sce.objects.context)
	
	# Examples
	
	# Move context objects on the x axis
	"""
	for ob in sce.objects.context:
		ob.LocX += 1
	"""
	
	# Copy Objects, does not copy object data
	"""
	# Store the current contetx
	context = list(sce.objects.context)
	# de-select all
	sce.objects.selected = []
	
	for ob in context:
		ob_copy = ob.copy()
		sce.objects.link(ob_copy)	# the copy is not added to a scene
		ob_copy.sel = True
	"""

def main():
	
	# Gets the current scene, there can be many scenes in 1 blend file.
	sce = bpy.data.scenes.active
	
	Window.WaitCursor(1)
	t = sys.time()
	
	# Run the object editing function
	my_object_util(sce)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'My Script finished in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	
	
# This lets you can import the script without running it
if __name__ == '__main__':
	main()

'''

new_text = bpy.data.texts.new('object_template.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
