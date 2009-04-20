#!BPY
"""
Name: 'IPO Example'
Blender: 245
Group: 'ScriptTemplate'
Tooltip: 'Script template for setting the IPO'
"""

from Blender import Window
import bpy

script_data = \
'''#!BPY
"""
Name: 'My Ipo Script'
Blender: 245
Group: 'Animation'
Tooltip: 'Put some useful info here'
"""

# Add a licence here if you wish to re-distribute, we recommend the GPL

from Blender import Ipo, Mathutils, Window
import bpy, BPyMessages

def makeRandomIpo(object, firstFrame, numberOfFrames, frameStep):
	# Create an new Ipo Curve of name myIpo and type Object
	myIpo = bpy.data.ipos.new('myIpo', 'Object')
	
	# Create LocX, LocY, and LocZ Ipo curves in our new Curve Object
	# and store them so we can access them later
	myIpo_x = myIpo.addCurve('LocX')
	myIpo_y = myIpo.addCurve('LocY')
	myIpo_z = myIpo.addCurve('LocZ')
	
	# What value we want to scale our random value by
	ipoScale = 4
	
	# This Calculates the End Frame for use in an xrange() expression
	endFrame = firstFrame + (numberOfFrames * frameStep) + frameStep
	
	for frame in xrange(firstFrame, endFrame, frameStep):
		
		# Use the Mathutils Rand() function to get random numbers
		ipoValue_x = Mathutils.Rand(-1, 1) * ipoScale
		ipoValue_y = Mathutils.Rand(-1, 1) * ipoScale
		ipoValue_z = Mathutils.Rand(-1, 1) * ipoScale
		
		# Append to the Ipo curve at location frame, with the value ipoValue_x
		# Note that we should pass the append function a tuple or a BezTriple
		myIpo_x.append((frame, ipoValue_x))
	
		# Similar to above
		myIpo_y.append((frame, ipoValue_y))
		myIpo_z.append((frame, ipoValue_z))
	
	# Link our new Ipo Curve to the passed object
	object.setIpo(myIpo)
	print object
	
	
def main():
	
	# Get the active scene, since there can be multiple ones
	sce = bpy.data.scenes.active
	
	# Get the active object
	object = sce.objects.active
	
	# If there is no active object, pop up an error message
	if not object:
		BPyMessages.Error_NoActive()
		
	Window.WaitCursor(1)
	
	# Call our makeRandomIpo function
	# Pass it our object, Tell it to keys from the start frame until the end frame, at a step of 10 frames
	# between them
	
	makeRandomIpo(object, sce.render.sFrame, sce.render.eFrame, 10)
	
	Window.WaitCursor(0)

if __name__ == '__main__':
	main()

'''

new_text = bpy.data.texts.new('ipo_template.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
