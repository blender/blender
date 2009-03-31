#!BPY
"""
Name: 'Camera/Object Example'
Blender: 245
Group: 'ScriptTemplate'
Tooltip: 'Script template for setting the camera direction'
"""

from Blender import Window
import bpy

script_data = \
'''#!BPY
"""
Name: 'My Camera script'
Blender: 245
Group: 'Object'
Tooltip: 'Rotate the camera to center on the active object'
"""

import Blender
from Blender import Window, Scene, Draw, Mathutils

# Rotate the camera in such a way that it centers on the currently active object
def RotCamToOb(cam, ob):
	
	# Get the camera matrix
	camMat = cam.getMatrix('worldspace');
	
	# Get the location of the camera and object and make sure they're vectors
	camLoc = Mathutils.Vector(cam.loc)
	obLoc = Mathutils.Vector(ob.loc)
	
	# Get the vector (direction) from the camera to the object
	newVec =  obLoc - camLoc
	
	# Make a quaternion that points the camera along the vector
	newQuat = newVec.toTrackQuat('-z', 'y')
	
	# Convert the new quaternion to a rotation matrix (and resize it to 4x4 so it matches the other matrices)
	rotMat = newQuat.toMatrix().resize4x4()
	
	# Make a matrix with only the current location of the camera
	transMat = Mathutils.TranslationMatrix(camMat.translationPart());
	
	# Multiply the rotation and translation matrixes to make 1 matrix with all data
	newMat = rotMat * transMat
		
	# Now we make this matrix the camera matrix and voila done!
	cam.setMatrix(newMat)

#Make sure blender and the objects are in the right state and start doing stuff
def SceneCheck():

	# Show a neat waitcursor whilst the script runs
	Window.WaitCursor(1)

	# If we are in edit mode, go out of edit mode and store the status in a var
	emode = int(Window.EditMode())
	if emode: Window.EditMode(0)

	# Get the scene, the camera and the currently active object
	scn = Scene.GetCurrent()
	cam = scn.getCurrentCamera()
	ob = scn.getActiveObject()

	# Lets do some checks to make sure we have everything
	# And if we don't then call a return which stops the entire script
	if not cam:
		Draw.PupMenu('Error, no active camera, aborting.')
		return
		
	if not ob:
		Draw.PupMenu('Error, no active object, aborting.')
		return
		
	if cam == ob:
		Draw.PupMenu('Error, select an object other than the camera, aborting.')
		return
		
	# Start the main function of the script if we didn't encounter any errors
	RotCamToOb(cam, ob)

	# Update the scene
	scn.update()

	# Redraw the 3d view so we can instantly see what was changed
	Window.Redraw(Window.Types.VIEW3D)

	# If we were in edit mode when the script started, go back into edit mode
	if emode: Window.EditMode(1)

	# Remove the waitcursor
	Window.WaitCursor(0)

# Start the script
SceneCheck()

'''

new_text = bpy.data.texts.new('camobject_template.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
