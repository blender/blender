#!BPY
"""
Name: 'GameLogic Template'
Blender: 245
Group: 'ScriptTemplate'
Tooltip: 'Basic template for new game logic scripts'
"""

from Blender import Window
import bpy

script_data = \
'''
def main():

	cont = GameLogic.getCurrentController()
	own = cont.getOwner()
	
	sens = cont.getSensor('mySensor')
	actu = cont.getActuator('myActuator')
	
	if sens.isPositive():
		GameLogic.addActiveActuator(actu, True)
	else:
		GameLogic.addActiveActuator(actu, False)

main()
'''

new_text = bpy.data.texts.new('gamelogic_simple.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
