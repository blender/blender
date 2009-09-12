#!BPY
"""
Name: 'Convert BGE 2.49'
Blender: 246
Group: 'TextPlugin'
Shortcut: ''
Tooltip: 'Attemps to update deprecated usage of game engine API.'
"""

#
# Copyright 2009 Alex Fraser <alex@phatcore.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#
# This script updates game engine scripts that were designed for pre-2.49
# versions of Blender to run with the new API. Deprecated function calls are
# listed in attributeRenameDict. This script searches for instances of the keys
# in a target script and re-writes them.
#
# Some deprecated functions are complicated to re-write. The most common
# conversions have been implemented, but some have not. Running this will reduce
# the number of deprecation warnings in your scripts, but may not eliminate them
# entirely.
#
# NOTE: The conversion is not guaranteed to be perfect. It is strongly
# recommended that you review all changes after running this script.
#
# TODO: The following attributes are either ambiguous or need special processing
# to handle parameters. Deprecated attributes that map to multiple target
# attributes will require a refactor of the main conversion loop in the
# convert248to249 function: currently, it doesn't allow any conversion to
# advance the row index by more than one.
#
# getLinearVelocity  (KX_SCA_AddObjectActuator, KX_ObjectActuator)
#                    Conflicts with KX_GameObject.
#                    Maps to multiple attributes.
# setLinearVelocity  (KX_SCA_AddObjectActuator, KX_ObjectActuator)
#                    Conflicts with KX_GameObject.
#                    Maps to multiple attributes.
# getAngularVelocity (KX_SCA_AddObjectActuator, KX_ObjectActuator)
#                    Conflicts with KX_GameObject.
#                    Maps to multiple attributes.
# setAngularVelocity (KX_SCA_AddObjectActuator, KX_ObjectActuator)
#                    Conflicts with KX_GameObject.
#                    Maps to multiple attributes.
# setAction          (BL_ShapeActionActuator, BL_ActionActuator)
#                    `reset' argument has no conversion target.
# set                (KX_VisibilityActuator, KX_IpoActuator)
#                    Different numbers of arguments.
#                    Arguments map to multiple attributes.
# getIndex           (SCA_JoystickSensor)
#                    Incompatible values: Old = 1-based index; new = 0-based.
# getMesh            (KX_SCA_ReplaceMeshActuator)
#                    Return types differ. Need to call object.name.
# getObject          (KX_SCA_AddObjectActuator, KX_CameraActuator,
#                     KX_TrackToActuator, KX_ParentActuator)
#                    Default return type differs between classes.
# setIndex           (SCA_JoystickSensor)
#                    Incompatible values: Old = 1-based index; new = 0-based.
# setObject          (KX_SCA_AddObjectActuator, KX_CameraActuator,
#                     KX_TrackToActuator, KX_ParentActuator)
#                    Incompatible types: Old = KX_GameObject or String; new =
#                    KX_GameObject.
# setOperation       (KX_SCA_DynamicActuator, KX_StateActuator)
#                    Ambiguous: different target names.
# getDRot            (KX_ObjectActuator)
#                    Maps to multiple attributes.
# setDRot            (KX_ObjectActuator)
#                    Arguments map to multiple attributes.
# getDLoc            (KX_ObjectActuator)
#                    Maps to multiple attributes.
# setDLoc            (KX_ObjectActuator)
#                    Arguments map to multiple attributes.
# getTorque          (KX_ObjectActuator)
#                    Maps to multiple attributes.
# setTorque          (KX_ObjectActuator)
#                    Arguments map to multiple attributes.
# getForce           (KX_ObjectActuator)
#                    Maps to multiple attributes.
# setForce           (KX_ObjectActuator)
#                    Arguments map to multiple attributes.
# position           (KX_GameObject)
#                    Conflicts with KX_SoundActuator.
# orientation        (KX_GameObject)
#                    Conflicts with KX_SoundActuator.
#

import string
import re

COMMENTCHAR = '#'

class ParseError(Exception): pass
class ConversionError(Exception): pass

def findBalancedParens(lines, row, col, openChar = '(', closeChar = ')'):
	"""Finds a balanced pair of parentheses, searching from lines[row][col].
	The opening parenthesis must be on the starting line.
	
	Returns two tuples containing the row and column of the opening paren, and
	the row and column of the matching paren.
	
	Throws a ParseError if the first character is not openChar, or if a matching
	paren cannot be found."""
	
	#
	# Find the opening coordinates.
	#
	oRow = row
	oCol = col
	line = lines[oRow]
	while oCol < len(line):
		if line[oCol] == openChar:
			break
		elif line[oCol] == COMMENTCHAR:
			break
		oCol = oCol + 1
	
	if oCol >= len(line) or line[oCol] != openChar or not re.match(r'^\s*$', line[col:oCol]):
		raise ParseError, "Can't find opening parenthesis. '%s'" % openChar
	
	#
	# Find the closing coordinates.
	#
	eRow = oRow
	eCol = oCol + 1
	level = 1
	while eRow < len(lines) and level > 0:
		line = lines[eRow]
		while eCol < len(line) and level > 0:
			c = line[eCol]
			if c == openChar:
				# Found a nested paren.
				level = level + 1
			elif c == closeChar:
				# Exiting one level of nesting.
				level = level - 1
				if level == 0:
					# Back to top level!
					return (oRow, oCol), (eRow, eCol)
			elif c == COMMENTCHAR:
				# Comment. Skip the rest of the line.
				break
			eCol = eCol + 1
		eRow = eRow + 1
		eCol = 0
	raise ParseError, "Couldn't find closing parenthesis."

def findLastAssignment(lines, row, attrName):
	"""Finds the most recent assignment of `attrName' before `row'. Returns
	everything after the '=' sign or None, if there was no match."""
	contRegex = re.compile(r'[^#]*?' +      # Don't search in comments.
	                       attrName  +
	                       r'\s*=\s*(.*)')  # Assignment
	
	cRow = row - 1
	while cRow >= 0:
		match = contRegex.search(lines[cRow])
		if match:
			return match.group(1)
		cRow = cRow - 1
	return None

def replaceSubstr(s, start, end, newSubStr):
	"""Replace the contents of `s' between `start' and `end' with
	`newSubStr'."""
	return s[:start] + newSubStr + s[end:]

def replaceNextParens(lines, row, colStart, newOpenChar, newCloseChar,
                      oldOpenChar = '(', oldCloseChar = ')'):
	"""Replace the next set of parentheses with different characters. The
	opening parenthesis must be located on line `row', and on or after
	`colStart'. The closing parenthesis may be on the same line or any following
	line. The strings are edited in-place.
	
	Throws a ParseError if the set of parentheses can't be found. In this case,
	the strings in `lines' will be untouched."""
	try:
		pOpen, pClose = findBalancedParens(lines, row, colStart, oldOpenChar,
		                                   oldCloseChar)
	except ParseError:
		raise
	
	# Replacement may change string length. Replace closing paren first.
	r, c = pClose
	lines[r] = replaceSubstr(lines[r], c, c + 1, newCloseChar)
	# Replace opening paren.
	r, c = pOpen
	lines[r] = replaceSubstr(lines[r], c, c + 1, newOpenChar)

def replaceSimpleGetter(lines, row, colStart, colEnd, newName):
	"""Replace a call to a simple getter function with a reference to a
	property, e.g. foo.getBar() -> foo.bar
	
	The function identifier being replaced must be on line `row' and
	between `colStart' and `colEnd'. The opening parenthesis must follow
	immediately (whitespace is allowed). The closing parenthesis may be on the
	same or following lines.
	
	Throws a ConversionError if the parentheses can't be found. In this case
	the content of `lines' will be untouched."""
	try:
		replaceNextParens(lines, row, colEnd, newOpenChar = '', newCloseChar = '')
	except ParseError:
		raise ConversionError, ("Deprecated function reference.")
	
	lines[row] = replaceSubstr(lines[row], colStart, colEnd, newName)

def replaceSimpleSetter(lines, row, colStart, colEnd, newName):
	"""Replace a call to a simple setter function with a reference to a
	property, e.g. foo.setBar(baz) -> foo.bar = baz
	
	The function identifier being replaced must be on line `row' and
	between `colStart' and `colEnd'. The opening parenthesis must follow
	immediately (whitespace is allowed). The closing parenthesis may be on the
	same or following lines.
	
	Throws a ConversionError if the parentheses can't be found. In this case
	the content of `lines' will be untouched."""
	try:
		replaceNextParens(lines, row, colEnd, newOpenChar = '', newCloseChar = '')
	except ParseError:
		raise ConversionError, ("Deprecated function reference.")
	
	lines[row] = replaceSubstr(lines[row], colStart, colEnd, newName + ' = ')

def replaceArgsWithListSetter(lines, row, colStart, colEnd, newName):
	"""Replace a call to a multi-agument setter function with a reference to a
	list property, e.g. foo.setBar(baz, bazZ) -> foo.bar = [baz, bazZ]
	
	The function identifier being replaced must be on line `row' and
	between `colStart' and `colEnd'. The opening parenthesis must follow
	immediately (whitespace is allowed). The closing parenthesis may be on the
	same or following lines.
	
	Throws a ConversionError if the parentheses can't be found. In this case
	the content of `lines' will be untouched."""
	try:
		replaceNextParens(lines, row, colEnd, newOpenChar = '[', newCloseChar = ']')
	except ParseError:
		raise ConversionError, ("Deprecated function reference.")
	
	lines[row] = replaceSubstr(lines[row], colStart, colEnd, newName + ' = ')
	

def replaceKeyedGetter(lines, row, colStart, colEnd, newName):
	"""Replace a call to a keyed getter function with a reference to a
	property, e.g. foo.getBar(baz) -> foo.bar[baz]
	
	The function identifier being replaced must be on line `row' and
	between `colStart' and `colEnd'. The opening parenthesis must follow
	immediately (whitespace is allowed). The closing parenthesis may be on the
	same or following lines.
	
	Throws a ConversionError if the parentheses can't be found. In this case
	the content of `lines' will be untouched."""
	try:
		replaceNextParens(lines, row, colEnd, newOpenChar = '[', newCloseChar = ']')
	except ParseError:
		raise ConversionError, ("Deprecated function reference.")
	
	lines[row] = replaceSubstr(lines[row], colStart, colEnd, newName)
 
def replaceGetXYPosition(lines, row, colStart, colEnd, axis):
	'''SCA_MouseSensor.getXPosition; SCA_MouseSensor.getYPosition.
	This is like a keyed getter, but the key is embedded in the attribute
	name.
	
	Throws a ConversionError if the parentheses can't be found. In this case
	the content of `lines' will be untouched.'''
	try:
		(openRow, openCol), (closeRow, closeCol) = findBalancedParens(lines,
			row, colEnd)
	except ParseError:
		raise ConversionError, "Deprecated function reference."
	if closeRow != row:
		raise ConversionError, "Can't modify multiple lines."
	
	lines[row] = replaceSubstr(lines[row], openCol, closeCol + 1,
		"[%s]" % axis)
	
	lines[row] = replaceSubstr(lines[row], colStart, colEnd, 'position')

def replaceRename(lines, row, colStart, colEnd, newName):
	"""Replace an identifier with another, e.g.
	foo.getBar() -> foo.getBaz()
	foo.bar -> foo.baz
	
	The identifier being replaced must be on line `row' and between `colStart'
	and `colEnd'."""
	lines[row] = replaceSubstr(lines[row], colStart, colEnd, newName)

def replaceAddActiveActuator(lines, row, colStart, colEnd, closure):
	'''Extra work needs to be done here to find out the name of the controller,
	and whether the actuator should be activated or deactivated.
	
	Throws a ConversionError if the actuator, controller or condition can't be
	found. In this case the content of `lines' will be untouched.'''
	try:
		(openRow, openCol), (closeRow, closeCol) = findBalancedParens(lines, row, colEnd)
	except ParseError:
		ConversionError, "Can't find arguments."
	
	if closeRow != openRow:
		raise ConversionError, ("Can't perform conversion: arguments span multiple lines.")
	
	args = lines[row][openCol + 1:closeCol]
	match = re.search(r'([a-zA-Z_]\w*)'       # Actuator identifier
	                  r',\s*'
	                  r'([0-9a-zA-Z_]\w*)',      # Condition (boolean)
					  args)
	if not match:
		raise ConversionError, "Can't find arguments."
	
	actuator = match.group(1)
	condition = match.group(2)
	controller = None
	
	assn = findLastAssignment(lines, row, actuator)
	if assn:
		match = re.search(r'([a-zA-Z_]\w*)'           # Controller identifier
		                  r'\s*\.\s*'                 # Dot
		                  r'(actuators\s*\[|getActuator\s*\()', # Dictionary/getter identifier
		                  assn)
		if match:
			controller = match.group(1)
	
	if not controller:
		raise ConversionError, "Can't find actuator's controller."
	
	gameLogicStart = lines[row].rfind("GameLogic", 0, colStart)
	if gameLogicStart < 0:
		raise ConversionError, "Can't find GameLogic identifier."
	
	newExpr = None
	if condition in ['1', 'True']:
		newExpr = "%s.activate(%s)" % (controller, actuator)
	elif condition in ['0', 'False']:
		newExpr = "%s.deactivate(%s)" % (controller, actuator)
	else:
		newExpr = "(lambda: %s and (%s.activate(%s) or True) or %s.deactivate(%s))()" % (
				condition, controller, actuator, controller, actuator)
	lines[row] = replaceSubstr(lines[row], gameLogicStart, closeCol + 1, newExpr)

def getObject(line, attributeStart):
	'''Get the object that an attribute belongs to. `attributeStart' is the
	index of the first character of the attribute name in the string `line'.
	Returns: the identifier preceding `attributeStart', or None if one can't be
	found.'''
	match = re.search(r'([a-zA-Z_]\w*)\s*\.\s*$', line[0:attributeStart])
	if not match:
		return None
	return match.group(1)

def replaceGetActuator(lines, row, colStart, colEnd, closure):
	'''getActuator is ambiguous: it could belong to SCA_IController or 
	SCA_ActuatorSensor. Try to resolve and then convert.
	
	Raises a ConversionError if the parentheses can't be found, or if the
	ambiguity can't be resolved.'''
	# Get the name of the object this attribute is attached to.
	obName = getObject(lines[row], colStart)
	if obName:
		# Try to find out whether the object is a controller.
		assn = findLastAssignment(lines, row, obName)
		if assn and re.search(r'GameLogic\s*\.\s*getCurrentController', assn):
			# It is (probably) a controller!
			replaceKeyedGetter(lines, row, colStart, colEnd, 'actuators')
			return
	
	raise ConversionError, "Ambiguous: addActiveActuator -> actuators[key] (SCA_IController) or actuator (SCA_ActuatorSensor)."

def replaceSetOrientation(lines, row, colStart, colEnd, closure):
	'''setOrientation is ambiguous: it could belong to KX_SoundActuator or 
	KX_GameObject. Try to resolve and then convert. If the type can't be
	determined, it is assumed to be a KX_GameObject. Currently, only the
	conversion for KX_GameObject is implemented.
	
	Raises a ConversionError if the parentheses can't be found, or if the
	object is found to be a KX_SoundActuator.'''
	# Get the name of the object this attribute is attached to.
	obName = getObject(lines[row], colStart)
	if obName:
		# Try to find out whether the object is an actuator.
		assn = findLastAssignment(lines, row, obName)
		if assn:
			match = re.search(r'([a-zA-Z_]\w*)'           # Controller identifier
							r'\s*\.\s*'                 # Dot
							r'(actuators\s*\[|getActuator\s*\()', # Dictionary/getter identifier
							assn)
			if match:
				# It's probably a KX_SoundActuator.
				raise ConversionError, "Not implemented: Can't convert arguments to matrix."
	
	# It's probably a KX_GameObject.
	replaceSimpleSetter(lines, row, colStart, colEnd, 'localOrientation')

def replaceSetPosition(lines, row, colStart, colEnd, closure):
	'''setPosition is ambiguous: it could belong to KX_SoundActuator or 
	KX_GameObject. Try to resolve and then convert. If the type can't be
	determined, it is assumed to be a KX_GameObject.
	
	Raises a ConversionError if the parentheses can't be found.'''
	# Get the name of the object this attribute is attached to.
	obName = getObject(lines[row], colStart)
	if obName:
		# Try to find out whether the object is an actuator.
		assn = findLastAssignment(lines, row, obName)
		if assn:
			match = re.search(r'([a-zA-Z_]\w*)'           # Controller identifier
							r'\s*\.\s*'                 # Dot
							r'(actuators\s*\[|getActuator\s*\()', # Dictionary/getter identifier
							assn)
			if match:
				# It's probably a KX_SoundActuator.
				replaceSimpleSetter(lines, row, colStart, colEnd, 'position')
	
	# It's probably a KX_GameObject.
	replaceSimpleSetter(lines, row, colStart, colEnd, 'localPosition')

def replaceSplitProperty(lines, row, colStart, colEnd, (newGetter, newSetter)):
	'''Some property attributes behave differently when being written to or read
	from. Try to determine the operation, and replace accordingly. E.G.
	o.position = foo -> o.localPosition = foo   # set
	foo = o.position -> foo = o.worldPosition   # get

	This implementation can not handle cases where the object is returned from
	a function, e.g.
	foo = bar.getObject().position # Error!

	Raises a ConversionError if the operation can't be determined, or if the
	object is returned from a function.'''
	assnRegex = re.compile(r'(=\s*)?' # Getter
							r'[a-zA-Z_]\w*' # Object identifier
							r'\.([a-zA-Z_][a-zA-Z0-9_.]*)+' # Trailing attributes
							r'(\s*=)?') # Setter
	match = assnRegex.search(lines[row])
	
	if not match:
		raise ConversionError, "Can't determine operation (getting or setting)."

	setting = False
	getting = False
	if match.group(1):
			getting = True
	if match.group(3):
			setting = True
	if (getting and setting) or ((not getting) and (not setting)):
			raise ConversionError, "Can't determine operation (getting or setting)."

	if getting:
			replaceRename(lines, row, colStart, colEnd, newGetter)
	else:
			replaceRename(lines, row, colStart, colEnd, newSetter)

def notImplemented(lines, row, colStart, colEnd, classNames):
	message = "Conversion not implemented. See documentation for " +\
	          string.join(classNames, ', ')
	raise ConversionError, message

#
# Deprecated attribute information. The format is:
# deprecatedAttributeName: (conversionFunction, closure)
# Usually the closure will be the name of the superceding attribute.
#
# Since each deprecated attribute can appear in this dictionary only once, it is
# the conversion function's responsibility to resolve ambiguity.
#
attributeRenameDict = {
 # Special cases
 'addActiveActuator': (replaceAddActiveActuator, None), # 
 'getActuator': (replaceGetActuator, None), # SCA_IController, SCA_ActuatorSensor
 'getXPosition': (replaceGetXYPosition, '0'), # SCA_MouseSensor
 'getYPosition': (replaceGetXYPosition, '1'), # SCA_MouseSensor
 'setOrientation': (replaceSetOrientation, None), # KX_GameObject, KX_SoundActuator
 'setPosition': (replaceSetPosition, None), # KX_GameObject, KX_SoundActuator

 # Keyed getters/setters
 'getSensor': (replaceKeyedGetter, 'sensors'), # SCA_IController

 # Multi-arg -> List setter
 'setAxis': (replaceArgsWithListSetter, 'axis'), # SCA_JoystickSensor
 'setForceLimitX': (replaceArgsWithListSetter, 'forceLimitX'), # KX_ObjectActuator
 'setForceLimitY': (replaceArgsWithListSetter, 'forceLimitY'), # KX_ObjectActuator
 'setForceLimitZ': (replaceArgsWithListSetter, 'forceLimitZ'), # KX_ObjectActuator
 'setHat': (replaceArgsWithListSetter, 'hat'), # SCA_JoystickSensor
 'setPID': (replaceArgsWithListSetter, 'pid'), # KX_ObjectActuator
 'setVelocity': (replaceArgsWithListSetter, 'velocity'), # KX_SoundActuator

 # Straight rename
 'getButtonValue': (replaceRename, 'getButtonActiveList'), # SCA_JoystickSensor

 # Split properties
 'scaling': (replaceSplitProperty, ('worldScaling', 'localScaling')), # KX_GameObject

 # Simple getters/setters
 'getSensors': (replaceSimpleGetter, 'sensors'), # SCA_IController
 'getActuators': (replaceSimpleGetter, 'actuators'), # SCA_IController
 'enableViewport': (replaceSimpleSetter, 'useViewport'), # KX_Camera
 'getAction': (replaceSimpleGetter, 'action'), # BL_ShapeActionActuator, BL_ActionActuator
 'getAxis': (replaceSimpleGetter, 'axis'), # SCA_JoystickSensor
 'getAxisValue': (replaceSimpleGetter, 'axisValues'), # SCA_JoystickSensor
 'getBlendin': (replaceSimpleGetter, 'blendIn'), # BL_ShapeActionActuator, BL_ActionActuator
 'getBodies': (replaceSimpleGetter, 'bodies'), # KX_NetworkMessageSensor
 'getButton': (replaceSimpleGetter, 'button'), # SCA_JoystickSensor
 'getCamera': (replaceSimpleGetter, 'camera'), # KX_SceneActuator
 'getConeOrigin': (replaceSimpleGetter, 'coneOrigin'), # KX_RadarSensor
 'getConeTarget': (replaceSimpleGetter, 'coneTarget'), # KX_RadarSensor
 'getContinue': (replaceSimpleGetter, 'useContinue'), # BL_ActionActuator
 'getCurrentlyPressedKeys': (replaceSimpleGetter, 'events'), # SCA_KeyboardSensor
 'getDamping': (replaceSimpleGetter, 'damping'), # KX_ObjectActuator
 'getDelay': (replaceSimpleGetter, 'delay'), # SCA_DelaySensor
 'getDistribution': (replaceSimpleGetter, 'distribution'), # SCA_RandomActuator
 'getDuration': (replaceSimpleGetter, 'duration'), # SCA_DelaySensor
 'getEnd': (replaceSimpleGetter, 'frameEnd'), # BL_ShapeActionActuator, KX_IpoActuator, BL_ActionActuator
 'getExecutePriority': (replaceSimpleGetter, 'executePriority'), # SCA_ILogicBrick
 'getFile': (replaceSimpleGetter, 'fileName'), # KX_GameActuator
 'getFilename': (replaceSimpleGetter, 'fileName'), # KX_SoundActuator
 'getForceIpoActsLocal': (replaceSimpleGetter, 'useIpoLocal'), # KX_IpoActuator
 'getForceLimitX': (replaceSimpleGetter, 'forceLimitX'), # KX_ObjectActuator
 'getForceLimitY': (replaceSimpleGetter, 'forceLimitY'), # KX_ObjectActuator
 'getForceLimitZ': (replaceSimpleGetter, 'forceLimitZ'), # KX_ObjectActuator
 'getFrame': (replaceSimpleGetter, 'frame'), # BL_ShapeActionActuator, BL_ActionActuator
 'getFrameMessageCount': (replaceSimpleGetter, 'frameMessageCount'), # KX_NetworkMessageSensor
 'getFrameProperty': (replaceSimpleGetter, 'framePropName'), # BL_ShapeActionActuator, BL_ActionActuator
 'getFrequency': (replaceSimpleGetter, 'frequency'), # SCA_ISensor
 'getGain': (replaceSimpleGetter, 'volume'), # KX_SoundActuator
 'getHat': (replaceSimpleGetter, 'hat'), # SCA_JoystickSensor
 'getHeight': (replaceSimpleGetter, 'height'), # KX_CameraActuator
 'getHitNormal': (replaceSimpleGetter, 'hitNormal'), # KX_MouseFocusSensor, KX_RaySensor
 'getHitObject': (replaceSimpleGetter, 'hitObject'), # KX_MouseFocusSensor, KX_RaySensor, KX_TouchSensor
 'getHitObjectList': (replaceSimpleGetter, 'hitObjectList'), # KX_TouchSensor
 'getHitPosition': (replaceSimpleGetter, 'hitPosition'), # KX_MouseFocusSensor, KX_RaySensor
 'getHold1': (replaceSimpleGetter, 'hold1'), # SCA_KeyboardSensor
 'getHold2': (replaceSimpleGetter, 'hold2'), # SCA_KeyboardSensor
 'getInvert': (replaceSimpleGetter, 'invert'), # SCA_ISensor
 'getIpoAdd': (replaceSimpleGetter, 'useIpoAdd'), # KX_IpoActuator
 'getIpoAsForce': (replaceSimpleGetter, 'useIpoAsForce'), # KX_IpoActuator
 'getKey': (replaceSimpleGetter, 'key'), # SCA_KeyboardSensor
 'getLastCreatedObject': (replaceSimpleGetter, 'objectLastCreated'), # KX_SCA_AddObjectActuator
 'getLevel': (replaceSimpleGetter, 'level'), # SCA_ISensor
 'getLightList': (replaceSimpleGetter, 'lights'), # KX_Scene
 'getLooping': (replaceSimpleGetter, 'looping'), # KX_SoundActuator
 'getMass': (replaceSimpleGetter, 'mass'), # KX_GameObject
 'getMax': (replaceSimpleGetter, 'max'), # KX_CameraActuator
 'getMin': (replaceSimpleGetter, 'min'), # KX_CameraActuator
 'getName': (replaceSimpleGetter, 'name'), # KX_Scene
 'getNumAxes': (replaceSimpleGetter, 'numAxis'), # SCA_JoystickSensor
 'getNumButtons': (replaceSimpleGetter, 'numButtons'), # SCA_JoystickSensor
 'getNumHats': (replaceSimpleGetter, 'numHats'), # SCA_JoystickSensor
 'getObjectList': (replaceSimpleGetter, 'objects'), # KX_Scene
 'getOperation': (replaceSimpleGetter, 'mode'), # KX_SCA_DynamicActuator
 'getOrientation': (replaceSimpleGetter, 'worldOrientation'), # KX_GameObject
 'getOwner': (replaceSimpleGetter, 'owner'), # SCA_ILogicBrick
 'getPara1': (replaceSimpleGetter, 'para1'), # SCA_RandomActuator
 'getPara2': (replaceSimpleGetter, 'para2'), # SCA_RandomActuator
 'getParent': (replaceSimpleGetter, 'parent'), # KX_GameObject
 'getPID': (replaceSimpleGetter, 'pid'), # KX_ObjectActuator
 'getPitch': (replaceSimpleGetter, 'pitch'), # KX_SoundActuator
 'getPosition': (replaceSimpleGetter, 'worldPosition'), # KX_GameObject
 'getPressedKeys': (replaceSimpleGetter, 'events'), # SCA_KeyboardSensor
 'getPriority': (replaceSimpleGetter, 'priority'), # BL_ShapeActionActuator, BL_ActionActuator
 'getProjectionMatrix': (replaceSimpleGetter, 'projection_matrix'), # KX_Camera
 'getProperty': (replaceSimpleGetter, 'propName'), # SCA_PropertySensor, SCA_RandomActuator, SCA_PropertyActuator
 'getRayDirection': (replaceSimpleGetter, 'rayDirection'), # KX_MouseFocusSensor, KX_RaySensor
 'getRaySource': (replaceSimpleGetter, 'raySource'), # KX_MouseFocusSensor
 'getRayTarget': (replaceSimpleGetter, 'rayTarget'), # KX_MouseFocusSensor
 'getRepeat': (replaceSimpleGetter, 'repeat'), # SCA_DelaySensor
 'getRollOffFactor': (replaceSimpleGetter, 'rollOffFactor'), # KX_SoundActuator
 'getScene': (replaceSimpleGetter, 'scene'), # KX_SceneActuator
 'getScript': (replaceSimpleGetter, 'script'), # SCA_PythonController
 'getSeed': (replaceSimpleGetter, 'seed'), # SCA_RandomActuator
 'getStart': (replaceSimpleGetter, 'frameStart'), # BL_ShapeActionActuator, KX_IpoActuator, BL_ActionActuator
 'getState': (replaceSimpleGetter, 'state'), # SCA_IController, KX_GameObject
 'getSubject': (replaceSimpleGetter, 'subject'), # KX_NetworkMessageSensor
 'getSubjects': (replaceSimpleGetter, 'subjects'), # KX_NetworkMessageSensor
 'getThreshold': (replaceSimpleGetter, 'threshold'), # SCA_JoystickSensor
 'getTime': (replaceSimpleGetter, 'time'), # KX_SCA_AddObjectActuator, KX_TrackToActuator
 'getTouchMaterial': (replaceSimpleGetter, 'useMaterial'), # KX_TouchSensor
 'getType': (replaceSimpleGetter, 'mode'), # SCA_PropertySensor
 'getUse3D': (replaceSimpleGetter, 'use3D'), # KX_TrackToActuator
 'getUseNegPulseMode': (replaceSimpleGetter, 'useNegPulseMode'), # SCA_ISensor
 'getUsePosPulseMode': (replaceSimpleGetter, 'usePosPulseMode'), # SCA_ISensor
 'getUseRestart': (replaceSimpleGetter, 'useRestart'), # KX_SceneActuator
 'getValue': (replaceSimpleGetter, 'value'), # SCA_PropertySensor, SCA_PropertyActuator
 'getVisible': (replaceSimpleGetter, 'visible'), # KX_GameObject
 'getXY': (replaceSimpleGetter, 'useXY'), # KX_CameraActuator
 'isConnected': (replaceSimpleGetter, 'connected'), # SCA_JoystickSensor
 'isPositive': (replaceSimpleGetter, 'positive'), # SCA_ISensor
 'isTriggered': (replaceSimpleGetter, 'triggered'), # SCA_ISensor
 'setActuator': (replaceSimpleSetter, 'actuator'), # SCA_ActuatorSensor
 'setBlendin': (replaceSimpleSetter, 'blendIn'), # BL_ShapeActionActuator, BL_ActionActuator
 'setBlendtime': (replaceSimpleSetter, 'blendTime'), # BL_ShapeActionActuator, BL_ActionActuator
 'setBodyType': (replaceSimpleSetter, 'usePropBody'), # KX_NetworkMessageActuator
 'setBody': (replaceSimpleSetter, 'body'), # KX_NetworkMessageActuator
 'setButton': (replaceSimpleSetter, 'button'), # SCA_JoystickSensor
 'setCamera': (replaceSimpleSetter, 'camera'), # KX_SceneActuator
 'setContinue': (replaceSimpleSetter, 'useContinue'), # BL_ActionActuator
 'setDamping': (replaceSimpleSetter, 'damping'), # KX_ObjectActuator
 'setDelay': (replaceSimpleSetter, 'delay'), # SCA_DelaySensor
 'setDuration': (replaceSimpleSetter, 'duration'), # SCA_DelaySensor
 'setEnd': (replaceSimpleSetter, 'frameEnd'), # BL_ShapeActionActuator, KX_IpoActuator, BL_ActionActuator
 'setExecutePriority': (replaceSimpleSetter, 'executePriority'), # SCA_ILogicBrick
 'setFile': (replaceSimpleSetter, 'fileName'), # KX_GameActuator
 'setFilename': (replaceSimpleSetter, 'fileName'), # KX_SoundActuator
 'setForceIpoActsLocal': (replaceSimpleSetter, 'useIpoLocal'), # KX_IpoActuator
 'setFrame': (replaceSimpleSetter, 'frame'), # BL_ShapeActionActuator, BL_ActionActuator
 'setFrameProperty': (replaceSimpleSetter, 'framePropName'), # BL_ShapeActionActuator, BL_ActionActuator
 'setFrequency': (replaceSimpleSetter, 'frequency'), # SCA_ISensor
 'setGain': (replaceSimpleSetter, 'volume'), # KX_SoundActuator
 'setHeight': (replaceSimpleSetter, 'height'), # KX_CameraActuator
 'setHold1': (replaceSimpleSetter, 'hold1'), # SCA_KeyboardSensor
 'setHold2': (replaceSimpleSetter, 'hold2'), # SCA_KeyboardSensor
 'setInvert': (replaceSimpleSetter, 'invert'), # SCA_ISensor
 'setIpoAdd': (replaceSimpleSetter, 'useIpoAdd'), # KX_IpoActuator
 'setIpoAsForce': (replaceSimpleSetter, 'useIpoAsForce'), # KX_IpoActuator
 'setKey': (replaceSimpleSetter, 'key'), # SCA_KeyboardSensor
 'setLevel': (replaceSimpleSetter, 'level'), # SCA_ISensor
 'setLooping': (replaceSimpleSetter, 'looping'), # KX_SoundActuator
 'setMask': (replaceSimpleSetter, 'mask'), # KX_StateActuator
 'setMax': (replaceSimpleSetter, 'max'), # KX_CameraActuator
 'setMesh': (replaceSimpleSetter, 'mesh'), # KX_SCA_ReplaceMeshActuator
 'setMin': (replaceSimpleSetter, 'min'), # KX_CameraActuator
 'setPitch': (replaceSimpleSetter, 'pitch'), # KX_SoundActuator
 'setPriority': (replaceSimpleSetter, 'priority'), # BL_ShapeActionActuator, BL_ActionActuator
 'setProjectionMatrix': (replaceSimpleSetter, 'projection_matrix'), # KX_Camera
 'setProperty': (replaceSimpleSetter, 'propName'), # KX_IpoActuator, SCA_PropertySensor, SCA_RandomActuator, SCA_PropertyActuator
 'setRepeat': (replaceSimpleSetter, 'repeat'), # SCA_DelaySensor
 'setRollOffFactor': (replaceSimpleSetter, 'rollOffFactor'), # KX_SoundActuator
 'setScene': (replaceSimpleSetter, 'scene'), # KX_SceneActuator
 'setScript': (replaceSimpleSetter, 'script'), # SCA_PythonController
 'setSeed': (replaceSimpleSetter, 'seed'), # SCA_RandomActuator
 'setStart': (replaceSimpleSetter, 'frameStart'), # BL_ShapeActionActuator, KX_IpoActuator, BL_ActionActuator
 'setState': (replaceSimpleSetter, 'state'), # KX_GameObject
 'setSubject': (replaceSimpleSetter, 'subject'), # KX_NetworkMessageActuator
 'setSubjectFilterText': (replaceSimpleSetter, 'subject'), # KX_NetworkMessageSensor
 'setThreshold': (replaceSimpleSetter, 'threshold'), # SCA_JoystickSensor
 'setTime': (replaceSimpleSetter, 'time'), # KX_SCA_AddObjectActuator, KX_TrackToActuator
 'setToPropName': (replaceSimpleSetter, 'propName'), # KX_NetworkMessageActuator
 'setType': (replaceSimpleSetter, 'mode'), # SCA_PropertySensor
 'setUse3D': (replaceSimpleSetter, 'use3D'), # KX_TrackToActuator
 'setUseNegPulseMode': (replaceSimpleSetter, 'useNegPulseMode'), # SCA_ISensor
 'setUsePosPulseMode': (replaceSimpleSetter, 'usePosPulseMode'), # SCA_ISensor
 'setUseRestart': (replaceSimpleSetter, 'useRestart'), # KX_SceneActuator
 'setValue': (replaceSimpleSetter, 'value'), # SCA_PropertySensor, SCA_PropertyActuator
 'setXY': (replaceSimpleSetter, 'useXY'), # KX_CameraActuator

 # Unimplemented!
 'getLinearVelocity': (notImplemented, ['KX_SCA_AddObjectActuator', 'KX_GameObject']),
 'setLinearVelocity': (notImplemented, ['KX_SCA_AddObjectActuator', 'KX_GameObject']),
 'getAngularVelocity': (notImplemented, ['KX_SCA_AddObjectActuator', 'KX_GameObject']),
 'setAngularVelocity': (notImplemented, ['KX_SCA_AddObjectActuator', 'KX_GameObject']),
 'setAction': (notImplemented, ['BL_ShapeActionActuator', 'BL_ActionActuator']),
 'set': (notImplemented, ['KX_VisibilityActuator', 'KX_IpoActuator']),
 'getIndex': (notImplemented, ['SCA_JoystickSensor']),
 'getMesh': (notImplemented, ['KX_SCA_ReplaceMeshActuator']),
 'getObject': (notImplemented, ['KX_SCA_AddObjectActuator', 'KX_CameraActuator', 'KX_TrackToActuator', 'KX_ParentActuator']),
 'setIndex': (notImplemented, ['SCA_JoystickSensor']),
 'setObject': (notImplemented, ['KX_SCA_AddObjectActuator', 'KX_CameraActuator', 'KX_TrackToActuator', 'KX_ParentActuator']),
 'setOperation': (notImplemented, ['KX_SCA_DynamicActuator', 'KX_StateActuator']),
 'position': (notImplemented, ['KX_GameObject', 'KX_SoundActuator']),
 'orientation': (notImplemented, ['KX_GameObject', 'KX_SoundActuator']),
 'getDRot': (notImplemented, ['KX_ObjectActuator']),
 'setDRot': (notImplemented, ['KX_ObjectActuator']),
 'getDLoc': (notImplemented, ['KX_ObjectActuator']),
 'setDLoc': (notImplemented, ['KX_ObjectActuator']),
 'getTorque': (notImplemented, ['KX_ObjectActuator']),
 'getTorque': (notImplemented, ['KX_ObjectActuator']),
 'getForce': (notImplemented, ['KX_ObjectActuator']),
 'setForce': (notImplemented, ['KX_ObjectActuator']),
}

def convert248to249(lines, log = True, logErrors = True, fileName = None):
	# Regular expression for finding attributes. For the string 'a.b', this
	# returns three groups: ['a.b', 'a.', 'b']. The last is the attribute name.
	attrRegex = re.compile(r'\.\s*'           # Dot
	                       r'([a-zA-Z_]\w*)') # Identifier
	
	fileIdStr = ""
	if fileName:
		fileIdStr = fileName + ": "
	row = 0
	col = 0
	nconverted = 0
	nerrors = 0
	while row < len(lines):
		originalLine = lines[row]
		changed = False
		while col < len(lines[row]):
			# Don't search past comment. We have to check each iteration
			# because the line contents may have changed.
			commentStart = lines[row].find('#', col)
			if commentStart < 0:
				commentStart = len(lines[row])
			
			# Search for an attribute identifier.
			match = attrRegex.search(lines[row], col, commentStart)
			if not match:
				break
			
			attrName = match.group(1)
			if attributeRenameDict.has_key(attrName):
				# name is deprecated.
				func, closure = attributeRenameDict[attrName]
				try:
					# Convert!
					func(lines, row, match.start(1), match.end(1), closure)
				except ConversionError, e:
					# Insert a comment saying the conversion failed.
					print "ERROR: %sline %d, %s: %s\n" % (
						fileIdStr, row + 1, attrName, e)
					if logErrors:
						lines.insert(row,
									"##248## ERROR: %s: %s\n" %
									(attrName, e))
						row = row + 1
					nerrors = nerrors + 1
				else:
					changed = True
					nconverted = nconverted + 1
			# Search the rest of this line.
			col = match.start(1)
		
		if changed and log:
			# Insert a comment to showing difference in lines.
			if originalLine[-1] != '\n':
				originalLine = originalLine + '\n'
			lines.insert(row, "##248##%s" % originalLine)
			row = row + 1
		
		row = row + 1
		col = 0
	return nconverted, nerrors

def usage():
	print "Usage: blender248to249.py [options] <infile> [outfile]"
	print "Options:"
	print "\t--nolog         Don't include old lines as comments."
	print "\t--quieterrors   Don't insert errors as comments."

def runAsConsoleScript():
	'''Called when being run as a console script.'''
	try:
		opts, args = getopt.getopt(sys.argv[1:], "", ["nolog", "quieterrors"])
	except getopt.GetoptError, err:
		# print help information and exit:
		print str(err)
		usage()
		sys.exit(2)
	
	log = True
	logErrors = True
	for o, a in opts:
		if o == "--nolog":
			log = False
		elif o == "--quieterrors":
			logErrors = False
	
	try:
		inpath = args.pop(0)
	except IndexError:
		usage()
		sys.exit(2)
	try:
		outpath = args.pop(0)
	except IndexError:
		outpath = inpath
	
	infile = io.FileIO(inpath, 'r')
	# arbitrary file size of around 100kB
	lines = infile.readlines(100000)
	infile.close()

	nconverted, nerrors = convert248to249(lines, log, logErrors)
	
	outfile = io.FileIO(outpath, 'w')
	outfile.writelines(lines)
	outfile.close()
	print "Conversion finished. Modified %d attributes." % nconverted
	print "There were %d errors." % nerrors
	print "Please review all the changes."

def runAsTextPlugin():
	'''Called when run as a text plugin.'''
	
	import Blender
	from Blender import Window, sys, Draw
	import BPyTextPlugin, bpy
	
	message = ("Convert Game Engine script from 4.48 API to 2.49 API%t|"
	           "Run on active script only%x1|"
	           "Run on ALL text buffers%x2")
	convertAllBuffers = Draw.PupMenu(message) == 2
	
	Window.WaitCursor(1)
	try:
		nconverted = 0
		nerrors = 0
		
		if convertAllBuffers:
			texts = bpy.data.texts
		else:
			if not bpy.data.texts.active:
				Draw.PupMenu("No active buffer.")
				return
			texts = [bpy.data.texts.active]
		
		Blender.SaveUndoState('Convert BGE 2.49')

		for txt in texts:
			bufName = txt.name
			if txt.lib:
				bufName = txt.lib + '/' + bufName
			lines = txt.asLines()
			for i in range(0, len(lines)):
				if not lines[i].endswith('\n'):
					lines[i] = lines[i] + '\n'
			
			nc, ne = convert248to249(lines, fileName = bufName)
			nconverted = nconverted + nc
			nerrors = nerrors + ne
			txt.clear()
			for line in lines:
				txt.write(line)
	
	finally:
		Window.WaitCursor(0)
		
		message = "Converted %d attributes." % nconverted
		if nerrors == 1:
			message = message + " There was 1 error (see console)."
		if nerrors > 1:
			message = message + " There were %d errors (see console)." % nerrors
		message = message + "|Please review all the changes."
		Draw.PupMenu(message)

def main():
	try:
		import Blender
	except ImportError:
		runAsConsoleScript()
	else:
		runAsTextPlugin()

# This lets you import the script without running it
if __name__ == "__main__":
	import sys
	import getopt
	import io
	main()
