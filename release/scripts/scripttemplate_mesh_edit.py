#!BPY
"""
Name: 'Mesh Editing'
Blender: 243
Group: 'ScriptTemplate'
Tooltip: 'Add a new text for editing a mesh'
"""

from Blender import Text, Window

script_data = '''
#!BPY
"""
Name: 'My Mesh Script'
Blender: 243
Group: 'Mesh'
Tooltip: 'Put some useful info here'
"""

# Add a licence here if you wish to re-distribute, we recommend the GPL

from Blender import Scene, Mesh, Window, sys
import BPyMessages

def my_mesh_util(me):
	# This function runs out of editmode with a mesh
	# error cases are alredy checked for
	
	# Remove these when writing your own tool
	print me.name
	print 'vert count', len(me.verts)
	print 'edge count', len(me.edges)
	print 'face count', len(me.faces)
	
	# Examples
	
	# Move selected verts on the x axis
	"""
	for v in me.verts:
		if v.sel:
			v.co.x += 1.0
	"""
	
	# Shrink selected faces
	"""
	for f in me.faces:
		if f.sel:
			c = f.cent
			for v in f:
				v.co = (c+v.co)/2
	"""

def main():
	
	# Gets the current scene, there can be many scenes in 1 blend file.
	sce = Scene.GetCurrent()
	
	# Get the active object, there can only ever be 1
	# and the active object is always the editmode object.
	ob_act = sce.objects.active
	
	if not ob_act or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return 
	
	
	# Saves the editmode state and go's out of 
	# editmode if its enabled, we cant make
	# changes to the mesh data while in editmode.
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(1)
	
	Window.WaitCursor(1)
	me = ob_act.getData(mesh=1)
	t = sys.time()
	
	# Run the mesh editing function
	my_mesh_util(me)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'My Script finished in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	
	
# This lets you can import the script without running it
if __name__ == '__main__':
	main()
'''

new_text = bpy.texts.new('mesh_template.py')
new_text.write(script_data)
bpy.texts.active = new_text
Window.RedrawAll()