#!BPY
"""
Name: 'GameLogic Module'
Blender: 249
Group: 'ScriptTemplate'
Tooltip: 'Basic template for new game logic modules'
"""

from Blender import Window
import bpy

script_data = \
'''
# This module can be accessed by a python controller with
# its execution method set to 'Module'
# * Set the module string to "gamelogic_module.main" (without quotes)
# * When renaming the script it MUST have a .py extension
# * External text modules are supported as long as they are at
#   the same location as the blendfile or one of its libraries.

import GameLogic

# variables defined here will only be set once when the
# module is first imported. Set object spesific vars
# inside the function if you intend to use the module
# with multiple objects.

def main(cont):
	own = cont.owner
	
	sens = cont.sensors['mySensor']
	actu = cont.actuators['myActuator']
	
	if sens.positive:
		cont.activate(actu)
	else:
		cont.deactivate(actu)

# dont call main(GameLogic.getCurrentController()), the py controller will
'''

new_text = bpy.data.texts.new('gamelogic_module.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
