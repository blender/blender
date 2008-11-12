#!BPY
"""
Name: 'Script Constraint'
Blender: 245
Group: 'ScriptTemplate'
Tooltip: 'Add a new script for custom constraints'
"""

from Blender import Window
import bpy

script_data = \
"""#BPYCONSTRAINT
'''
	PyConstraint template, access this in the "add constraint" scripts submenu.
	Add docstring here
'''

import Blender
from Blender import Draw
from Blender import Mathutils
import math

'''
 This variable specifies the number of targets 
 that this constraint can use
'''
NUM_TARGETS = 1


'''
 This function is called to evaluate the constraint
	obmatrix:		(Matrix) copy of owner's 'ownerspace' matrix
	targetmatrices:	(List) list of copies of the 'targetspace' matrices of the targets (where applicable)
	idprop:			(IDProperties) wrapped data referring to this 
					constraint instance's idproperties
'''
def doConstraint(obmatrix, targetmatrices, idprop):
	# Separate out the transformation components for easy access.
	obloc = obmatrix.translationPart()	# Translation
	obrot = obmatrix.toEuler()			# Rotation
	obsca = obmatrix.scalePart()		# Scale

	# Define user-settable parameters
	# 	Must also be defined in getSettings().
	if not idprop.has_key('user_toggle'): idprop['user_toggle'] = 1
	if not idprop.has_key('user_slider'): idprop['user_slider'] = 1.0
	
	
	# Do stuff here, changing obloc, obrot, and obsca.

	
	# Convert back into a matrix for loc, scale, rotation,
	mtxloc = Mathutils.TranslationMatrix(obloc)
	mtxrot = obrot.toMatrix().resize4x4()
	mtxsca = Mathutils.Matrix([obsca[0],0,0,0], [0,obsca[1],0,0], [0,0,obsca[2],0], [0,0,0,1])
	
	# Recombine the separate elements into a transform matrix.
	outputmatrix = mtxsca * mtxrot * mtxloc

	# Return the new matrix.
	return outputmatrix



'''
 This function manipulates the matrix of a target prior to sending it to doConstraint()
	target_object:					wrapped data, representing the target object
	subtarget_bone:					wrapped data, representing the subtarget pose-bone/vertex-group (where applicable)
	target_matrix:					(Matrix) the transformation matrix of the target
	id_properties_of_constraint:	(IDProperties) wrapped idproperties
'''
def doTarget(target_object, subtarget_bone, target_matrix, id_properties_of_constraint):
	return target_matrix


'''
 This function draws a pupblock that lets the user set
 the values of custom settings the constraint defines.
 This function is called when the user presses the settings button.
	idprop:	(IDProperties) wrapped data referring to this 
			constraint instance's idproperties
'''
def getSettings(idprop):
	# Define user-settable parameters.
	# Must also be defined in getSettings().
	if not idprop.has_key('user_toggle'): idprop['user_toggle'] = 1
	if not idprop.has_key('user_slider'): idprop['user_slider'] = 1.0
	
	# create temporary vars for interface 
	utoggle = Draw.Create(idprop['user_toggle'])
	uslider = Draw.Create(idprop['user_slider'])
	

	# define and draw pupblock
	block = []
	block.append("Buttons: ")
	block.append(("Toggle", utoggle, "This is a toggle button."))
	block.append("More buttons: ")
	block.append(("Slider", uslider, 0.0000001, 1000.0, "This is a number field."))

	retval = Draw.PupBlock("Constraint Template", block)
	
	# update id-property values after user changes settings
	if (retval):
		idprop['user_toggle']= utoggle.val
		idprop['user_slider']= uslider.val

"""

new_text = bpy.data.texts.new('pyconstraint_template.py')
new_text.write(script_data)
bpy.data.texts.active = new_text
Window.RedrawAll()
