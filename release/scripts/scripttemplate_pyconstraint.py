#!BPY
"""
Name: 'Script Constraint'
Blender: 245
Group: 'ScriptTemplate'
Tooltip: 'Add a new text for custom constraints'
"""

from Blender import Window
import bpy

script_data = \
'''#BPYCONSTRAINT

""" <------- Start removable description section -----------> 
PyConstraints are text buffers that start with #BPYCONSTRAINT.

They must define a doConstraint function.  The doConstraint 
function is called with the world-space matrix of the parent object/posebone
as the first argument, the world-space matrix of the target object/posebone as
the second, and an ID property that's attached to the current constraint
instance.  The function then must return a 4x4 Mathutils.Matrix() object.

They must also define a getSettings function. The getSettings 
function is called with the ID property that's attached to the current constraint
instance. It should create a pupblock using the Blender.Draw module to 
get/set the relevant values of the ID properties.

When a constraint needs to have a Target Object/Bone, the USE_TARGET line
below must be present. Also, if any special matrix creation needs to be performed
for the target, a doTarget function must also be defined.

<------- End removable description section -----------> """

# Add a licence here if you wish to re-distribute, we recommend the GPL

# uncomment the following line if Target access is wanted
"""
USE_TARGET = True
""" 

import Blender
from Blender import Draw
from Blender import Mathutils
from math import *

# this function is called to evaluate the constraint
#	inputmatrix: (Matrix) copy of owner's worldspace matrix
#	targetmatrix: (Matrix) copy of target's worldspace matrix (where applicable)
#	idproperty: (IDProperties) wrapped data referring to this 
#			constraint instance's idproperties
def doConstraint(inputmatrix, targetmatrix, idproperty):
	# must return a 4x4 matrix (owner's new matrix)
	return inputmatrix;
	
# this function draws a pupblock that lets the user set
# the values of custom settings the constraint defines
#	idprop: (IDProperties) wrapped data referring to this 
#			constraint instance's idproperties
# You MUST use a pupblock. There are errors if you try to use the UIBlock ones.
def getSettings(idproperty):
	pass;


# this optional function performs special actions that only require
# access to the target data - calculation of special information
"""	
def doTarget (targetobject, subtarget, targetmatix, idproperty):
	# return a 4x4 matrix (which acts as the matrix of the target)
	return targetmatrix;
"""

'''

new_text = bpy.data.texts.new('pyconstraint_template.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()