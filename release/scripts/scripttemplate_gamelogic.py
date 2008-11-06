#!BPY
"""
Name: 'GameLogic Example'
Blender: 245
Group: 'ScriptTemplate'
Tooltip: 'Script template with examples of how to use game logic'
"""

from Blender import Window
import bpy

script_data = \
'''
# GameLogic has been added to the global namespace no need to import

# for keyboard event comparison
# import GameKeys 

# support for Vector(), Matrix() types and advanced functions like AngleBetweenVecs(v1,v2) and RotationMatrix(...)
# import Mathutils 

# for functions like getWindowWidth(), getWindowHeight()
# import Rasterizer

def main():
	cont = GameLogic.getCurrentController()
	
	# The KX_GameObject that owns this controller.
	own = cont.getOwner()
	
	# for scripts that deal with spacial logic
	own_pos = own.getPosition() 
	
	
	# Some example functions, remove to write your own script.
	# check for a positive sensor, will run on any object without errors.
	print 'Logic info for KX_GameObject', own.getName()
	input = False
	
	for sens in cont.getSensors():
		# The sensor can be on another object, we may want to use it
		own_sens = sens.getOwner()
		print '    sensor:', sens.getName(),
		if sens.isPositive():
			print '(true)'
			input = True
		else:
			print '(false)'
	
	for actu in cont.getActuators():
		# The actuator can be on another object, we may want to use it
		own_actu = actu.getOwner()
		print '    actuator:', sens.getName()
		
		# This runs the actuator or turns it off
		# note that actuators will continue to run unless explicitly turned off.
		if input:
			GameLogic.addActiveActuator(actu, True)
		else:
			GameLogic.addActiveActuator(actu, False)
	
	# Its also good practice to get sensors and actuators by names
	# so any changes to their order wont break the script.
	
	# sens_key = cont.getSensor('key_sensor')
	# actu_motion = cont.getActuator('motion')
	
	
	# Loop through all other objects in the scene
	sce = GameLogic.getCurrentScene()
	print 'Scene Objects:', sce.getName()
	for ob in sce.getObjectList():
		print '   ', ob.getName(), ob.getPosition()
	
	
	# Example where collision objects are checked for their properties
	# adding to our objects "life" property
	"""
	actu_collide = cont.getSensor('collision_sens')
	for ob in actu_collide.getHitObjectList():
		# Check to see the object has this property
		if hasattr(ob, 'life'):
			own.life += ob.life
			ob.life = 0
	print own.life
	"""

main()
'''

new_text = bpy.data.texts.new('gamelogic_example.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
