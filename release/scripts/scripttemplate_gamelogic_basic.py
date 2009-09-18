#!BPY
"""
Name: 'GameLogic Template'
Blender: 249
Group: 'ScriptTemplate'
Tooltip: 'Basic template for new game logic scripts'
"""

from Blender import Window
import bpy

script_data = \
'''
def main():

	cont = GameLogic.getCurrentController()
	own = cont.owner
	
	sens = cont.sensors['mySensor']
	actu = cont.actuators['myActuator']
	
	if sens.positive:
		cont.activate(actu)
	else:
		cont.deactivate(actu)

main()
'''

new_text = bpy.data.texts.new('gamelogic_simple.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
