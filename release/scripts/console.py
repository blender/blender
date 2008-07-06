#!BPY

"""
Name: 'Interactive Python Console'
Blender: 245
Group: 'System'
Tooltip: 'Interactive Python Console'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__bpydoc__ = """\
This is an interactive console, similar to Python's own command line interpreter.  Since it is embedded in Blender, it has access to all Blender Python modules.

Those completely new to Python are recommended to check the link button above
that points to its official homepage, with news, downloads and documentation.

Usage:<br>
  Type your code and hit "Enter" to get it executed.<br>
  - Right mouse click: Console Menu (Save output, etc);<br>
  - Mousewheel: Scroll text
  - Arrow keys: command history and cursor;<br>
  - Shift + Backspace: Backspace whole word;<br>
  - Shift + Arrow keys: jump words;<br>
  - Ctrl + (+/- or mousewheel): Zoom text size;<br>
  - Ctrl + Enter: auto compleate based on variable names and modules loaded -- multiple choices popup a menu;<br>
  - Shift + Enter: multiline functions -- delays executing code until only Enter is pressed.
"""

# -------------------------------------------------------------------------- 
# ***** BEGIN GPL LICENSE BLOCK ***** 
# 
# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation; either version 2 
# of the License, or (at your option) any later version. 
# 
# This program is distributed in the hope that it will be useful, 
# but WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the 
# GNU General Public License for more details. 
# 
# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software Foundation, 
# Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA. 
# 
# ***** END GPL LICENCE BLOCK ***** 
# -------------------------------------------------------------------------- 

import Blender
import bpy
from Blender import *
import sys as python_sys
import StringIO

# Constants
__DELIMETERS__ = '. ,=+-*/%<>&~][{}():\t'
__VARIABLE_DELIMETERS__ = ' ,=+-*/%<>&~{}():\t'

__LINE_HISTORY__ = 500

global __FONT_SIZE__

__FONT_SIZES__ = ( ('tiny', 10), ('small', 12), ('normalfix', 14), ('large', 16) )
__FONT_SIZE__ = 2 # index for the list above, normal default.

global __CONSOLE_LINE_OFFSET__
__CONSOLE_LINE_OFFSET__ = 0

'''
# Generic Blender functions
def getActScriptWinRect():
	area = Window.GetAreaSize()
	area = (area[0]-1, area[1]-1)
	for scrInfo in Window.GetScreenInfo(Window.Types['SCRIPT'], 'win', ''):
		if scrInfo['vertices'][2]-scrInfo['vertices'][0] == area[0]:
			if scrInfo['vertices'][3]-scrInfo['vertices'][1] == area[1]:
				return scrInfo['vertices']
	return None
'''


# menuText, # per group
def PupMenuLess(menu, groupSize=35):
	more = ['   more...']
	less = ['   less...']
	
	menuList= menu.split('|')
	
	# No Less Needed, just call.
	if len(menuList) < groupSize:
		return Draw.PupMenu(menu)
	
	title = menuList[0].split('%t')[0]
	
	# Split the list into groups
	menuGroups = [[]]
	for li in menuList[1:]:
		if len(menuGroups[-1]) < groupSize:
			menuGroups[-1].append(li)
		else:
			menuGroups.append([li])
	
	# Stores teh current menu group we are looking at
	groupIdx = 0
	while 1:
		# Give us a title with the menu number
		numTitle = [ ' '.join([title, str(groupIdx + 1), 'of', str(len(menuGroups)), '%t'])]
		if groupIdx == 0:
			menuString = '|'.join(numTitle + menuGroups[groupIdx] + more)
		elif groupIdx == len(menuGroups)-1:
			menuString = '|'.join(numTitle + less + menuGroups[groupIdx])
		else: # In the middle somewhere so Show a more and less
			menuString = '|'.join(numTitle + less + menuGroups[groupIdx] + more)
		result = Draw.PupMenu(menuString)
		# User Exit
		if result == -1:
			return -1
		
		if groupIdx == 0: # First menu
			if result-1 < groupSize:
				return result
			else: # must be more
				groupIdx +=1
		elif groupIdx == len(menuGroups): # Last Menu
			if result == 1: # Must be less
				groupIdx -= 1
			else: # Must be a choice
				return result + (groupIdx*groupSize)
			
		else:	
			if result == 1: # Must be less
				groupIdx -= 1
			elif result-2 == groupSize:
				groupIdx +=1
			else:
				return result - 1 + (groupIdx*groupSize)
				


# Use newstyle classes, Im not bothering with inheretence
# but slots are faster.
class cmdLine(object):
	__slots__ = [\
	'cmd', # is the command string, or any other message
	'type',# type: 0:user input  1:program feedback  2:error message.  3:option feedback
	'exe' #  0- not yet executed   1:executed
	]
	def __init__(self, cmd, type, exe):
		self.cmd = cmd
		self.type = type
		self.exe = exe

# Include external file with internal namespace
def include(filename):
	file = open(filename, 'r')
	filedata = file.read()
	file.close()
	return compile(filedata, filename, 'exec')

# Writes command line data to a blender text file.
def writeCmdData(cmdLineList, type):
	if type == 3:
		typeList = [0,1,2, 3, None] # all
	else:
		typeList = [type] # so we can use athe lists 'in' methiod
	
	newText = Text.New('command_output.py', 1)
	for myCmd in cmdLineList:
		if myCmd.type in typeList: # user input
			newText.write('%s\n' % myCmd.cmd)
	Draw.PupMenu('%s written' % newText.name)

def insertCmdData(cmdBuffer):
	texts = list(bpy.data.texts)
	textNames = [tex.name for tex in texts]
	if textNames:
		choice = Draw.PupMenu('|'.join(textNames))
		if choice != -1:
			text = texts[choice-1]
			
			# Add the text!
			for l in text.asLines():
				cmdBuffer.append(cmdLine('%s ' % l, 0, 0))
			Draw.Redraw()
	

COLLECTED_VAR_NAMES = {} # a list of keys, each key has a list of absolute paths

# Pain and simple recursice dir(), accepts a string
unused_types = str, dict, list, float, int, str, type, tuple, type(dir), type(None)
def rdir(dirString, depth=0):
	#print ' ' * depth, dirString
	# MAX DEPTH SET HERE
	if depth > 5:
		# print 'maxdepoth reached.'
		return
		
	global COLLECTED_VAR_NAMES
	dirStringSplit = dirString.split('.')
	
	exec('value=' + dirString)	
	
	if type(value) in unused_types:
		# print 'bad type'
		return
	dirList = dir(value)
	
	for dirItem in dirList:
		if dirItem.startswith('_'):
			continue
			
		dirData = None
		try:
			# Rare cases this can mess up, material.shader was a problem.
			exec('dirData = %s.%s' % (dirString, dirItem))
			#print dirData
		except:
			# Dont bother with this data.
			continue
		#print  'HEY', dirData, dirItem
		#if type(dirItem) != str:
		#	print dirItem, type(dirItem)
		
		if dirItem not in COLLECTED_VAR_NAMES: # .keys()
			COLLECTED_VAR_NAMES[dirItem] = []
		
		# Add the string
		# splitD = dirString.split('"')[-2]
		
		# Example of dirString
		# __CONSOLE_VAR_DICT__["Main"].scenes.active.render
		
		# Works but can be faster
		# splitD = dirString.replace('__CONSOLE_VAR_DICT__["', '').replace('"]', '')
		
		splitD = dirString[22:].replace('"]', '')
		
		if splitD not in COLLECTED_VAR_NAMES[dirItem]:
			# print dirItem, dirString, splitD,
			COLLECTED_VAR_NAMES[dirItem].append(splitD)
		
		
		# Stops recursice stuff, overlooping
		#print type(dirItem)
		#if type(dirData) == types.ClassType or \
		#	 type(dirData) == types.ModuleType:
		type_dirData = type(dirData)
		if type_dirData not in unused_types:
			# print type(dirData), dirItem
			# Dont loop up dirs for strings ints etc.
			if dirItem not in dirStringSplit:
				rdir( '%s.%s' % (dirString, dirItem), depth+1)
		'''
		elif depth == 0: # Add local variables
			# print type(dirData), dirItem
			# Dont loop up dirs for strings ints etc.
			if dirItem not in dirStringSplit:
				rdir( '%s.%s' % (dirString, dirItem), depth+1)
		'''

def recursive_dir():
	global COLLECTED_VAR_NAMES
	
	for name in __CONSOLE_VAR_DICT__: # .keys()
		if not name.startswith('_'): # Dont pick names like __name__
			rdir('__CONSOLE_VAR_DICT__["%s"]' % name)
			#print COLLECTED_VAR_NAMES
			COLLECTED_VAR_NAMES[name] = [''] 
	return COLLECTED_VAR_NAMES

# Runs the code line(s) the user has entered and handle errors
# As well as feeding back the output into the blender window.
def runUserCode(__USER_CODE_STRING__):
	global __CONSOLE_VAR_DICT__ # We manipulate the variables here. loading and saving from localspace to this global var.
	
	# Open A File like object to write all output to, that would useually be printed. 
	python_sys.stdout.flush() # Get rid of whatever came before
	__FILE_LIKE_STRING__ = StringIO.StringIO() # make a new file like string, this saves up from making a file.
	__STD_OUTPUT__ = python_sys.stdout # we need to store the normal output.
	python_sys.stdout=__FILE_LIKE_STRING__ # Now anything printed will be written to the file like string.
	
	# Try and run the user entered line(s)
	try:
		# Load all variabls from global dict to local space.
		__TMP_VAR_NAME__ = __TMP_VAR__ = '' # so as not to raise an error when del'ing

		for __TMP_VAR_NAME__, __TMP_VAR__ in __CONSOLE_VAR_DICT__.items():
			exec('%s%s' % (__TMP_VAR_NAME__,'=__TMP_VAR__'))
		del __TMP_VAR_NAME__
		del __TMP_VAR__
		
		# Now all the vars are loaded, execute the code. # Newline thanks to phillip,
		exec(compile(__USER_CODE_STRING__, 'blender_cmd.py', 'single')) #exec(compile(__USER_CODE_STRING__, 'blender_cmd.py', 'exec'))
		
		# Flush global dict, allow the user to remove items.
		__CONSOLE_VAR_DICT__ = {}

		__TMP_VAR_NAME__ = '' # so as not to raise an error when del'ing	
		# Write local veriables to global __CONSOLE_VAR_DICT__
		for __TMP_VAR_NAME__ in dir():
			if	__TMP_VAR_NAME__ != '__FILE_LIKE_STRING__' and\
					__TMP_VAR_NAME__ != '__STD_OUTPUT__' and\
					__TMP_VAR_NAME__ != '__TMP_VAR_NAME__' and\
					__TMP_VAR_NAME__ != '__USER_CODE_STRING__':
				
				# Execute the local > global coversion.
				exec('%s%s' % ('__CONSOLE_VAR_DICT__[__TMP_VAR_NAME__]=', __TMP_VAR_NAME__))
		del __TMP_VAR_NAME__
	
	except: # Prints the REAL exception.
		error = '%s:  %s' % (python_sys.exc_type, python_sys.exc_value)		
		for errorLine in error.split('\n'):
			cmdBuffer.append(cmdLine(errorLine, 2, None)) # new line to type into
	
	python_sys.stdout = __STD_OUTPUT__ # Go back to output to the normal blender console
	
	# Copy all new output to cmdBuffer
	
	__FILE_LIKE_STRING__.seek(0) # the readline function requires that we go back to the start of the file.
	
	for line in __FILE_LIKE_STRING__.readlines():
		cmdBuffer.append(cmdLine(line, 1, None))
		
	cmdBuffer.append(cmdLine(' ', 0, 0)) # new line to type into
	python_sys.stdout=__STD_OUTPUT__
	__FILE_LIKE_STRING__.close()





#------------------------------------------------------------------------------#
#                             event handling code                              #
#------------------------------------------------------------------------------#
def handle_event(evt, val):
	
	# Insert Char into the cammand line
	def insCh(ch): # Instert a char
		global cmdBuffer
		global cursor
		# Later account for a cursor variable
		cmdBuffer[-1].cmd = ('%s%s%s' % ( cmdBuffer[-1].cmd[:cursor], ch, cmdBuffer[-1].cmd[cursor:]))
	
	#------------------------------------------------------------------------------#
	#                        Define Complex Key Actions                            #
	#------------------------------------------------------------------------------#
	def actionEnterKey():
		global histIndex, cursor, cmdBuffer
		
		def getIndent(string):
			# Gather white space to add in the previous line
			# Ignore the last char since its padding.
			whiteSpace = ''
			#for i in range(len(cmdBuffer[-1].cmd)):
			for i in xrange(len(string)-1):
				if cmdBuffer[-1].cmd[i] == ' ' or cmdBuffer[-1].cmd[i] == '\t':
					whiteSpace += string[i]
				else:
					break
			return whiteSpace
		
		# Autocomplete
		if Window.GetKeyQualifiers() & Window.Qual.CTRL:
			actionAutoCompleate()
			return
		
		# Are we in the middle of a multiline part or not?
		# try be smart about it
		if cmdBuffer[-1].cmd.split('#')[0].rstrip().endswith(':'):
			# : indicates an indent is needed
			cmdBuffer.append(cmdLine('\t%s ' % getIndent(cmdBuffer[-1].cmd), 0, 0))
			print ': indicates an indent is needed'		
		
		elif cmdBuffer[-1].cmd[0] in [' ', '\t'] and len(cmdBuffer[-1].cmd) > 1 and cmdBuffer[-1].cmd.split():
			# white space at the start means he havnt finished the multiline.
			cmdBuffer.append(cmdLine('%s ' % getIndent(cmdBuffer[-1].cmd), 0, 0))
			print 'white space at the start means he havnt finished the multiline.'
		
		elif Window.GetKeyQualifiers() & Window.Qual.SHIFT:
			# Crtl forces multiline
			cmdBuffer.append(cmdLine('%s ' % getIndent(cmdBuffer[-1].cmd), 0, 0))
			print 'Crtl forces multiline'			
		
		else: # Execute multiline code block
			
			# Multiline code will still run with 1 line,
			multiLineCode = ['if 1:'] # End of the multiline first.
			
			# Seek the start of the file multiline
			i = 1
			while cmdBuffer[-i].exe == 0:
				i+=1
			
			while i > 1:
				i-=1
				
				if cmdBuffer[-i].cmd == ' ':# Tag as an output type so its not used in the key history
					cmdBuffer[-i].type = 1
				else: # Tab added at the start for added if 1: statement
					multiLineCode.append('\t%s' % cmdBuffer[-i].cmd )
				
				# Mark as executed
				cmdBuffer[-i].exe = 1				
				
			multiLineCode.append('\tpass') # reverse will make this the start.			
			
			# Dubug, print the code that is executed.
			#for m in multiLineCode: print m
			
			runUserCode('\n'.join(multiLineCode))
			
			# Clear the output based on __LINE_HISTORY__
			if len(cmdBuffer) > __LINE_HISTORY__:
				cmdBuffer = cmdBuffer[-__LINE_HISTORY__:]
		
		histIndex = cursor = -1 # Reset cursor and history
	
	def actionUpKey():
		global histIndex, cmdBuffer
		if abs(histIndex)+1 >= len(cmdBuffer):
			histIndex = -1
		histIndex_orig = histIndex
		histIndex -= 1
		
		while	(cmdBuffer[histIndex].type != 0 and abs(histIndex) < len(cmdBuffer)) or \
				( cmdBuffer[histIndex].cmd == cmdBuffer[histIndex_orig].cmd):
			histIndex -= 1
			
		if cmdBuffer[histIndex].type == 0: # we found one
			cmdBuffer[-1].cmd = cmdBuffer[histIndex].cmd			
	
	def actionDownKey():
		global histIndex, cmdBuffer
		if histIndex >= -2:
			histIndex = -len(cmdBuffer)
		histIndex_orig = histIndex
		histIndex += 1
		while	(cmdBuffer[histIndex].type != 0 and histIndex != -2) or \
				( cmdBuffer[histIndex].cmd == cmdBuffer[histIndex_orig].cmd):
			
			histIndex += 1
			
		if cmdBuffer[histIndex].type == 0: # we found one
			cmdBuffer[-1].cmd = cmdBuffer[histIndex].cmd
	
	def actionRightMouse():
		global __FONT_SIZE__
		choice = Draw.PupMenu('Console Menu%t|Write Input Data (white)|Write Output Data (blue)|Write Error Data (red)|Write All Text|%l|Insert Blender text|%l|Font Size|%l|Quit')
		
		if choice == 1:
			writeCmdData(cmdBuffer, 0) # type 0 user
		elif choice == 2:
			writeCmdData(cmdBuffer, 1) # type 1 user output
		elif choice == 3:
			writeCmdData(cmdBuffer, 2) # type 2 errors
		elif choice == 4:
			writeCmdData(cmdBuffer, 3) # All
		elif choice == 6:
			insertCmdData(cmdBuffer) # Insert text from Blender and run it.
		elif choice == 8:
			# Fontsize.
			font_choice = Draw.PupMenu('Font Size%t|Large|Normal|Small|Tiny')
			if font_choice != -1:
				if font_choice == 1:
					__FONT_SIZE__ = 3
				elif font_choice == 2:
					__FONT_SIZE__ = 2
				elif font_choice == 3:
					__FONT_SIZE__ = 1
				elif font_choice == 4:
					__FONT_SIZE__ = 0
				Draw.Redraw()
				
		elif choice == 10: # Exit
			Draw.Exit()
	
	
	# Auto compleating, quite complex- use recutsice dir for the moment.
	def actionAutoCompleate(): # Ctrl + Tab
		if not cmdBuffer[-1].cmd[:cursor].split():
			return
		
		
		RECURSIVE_DIR = recursive_dir()
		
		# get last name of user input
		editVar = cmdBuffer[-1].cmd[:cursor]
		# Split off spaces operators etc from the staryt of the command so we can use the startswith function.
		for splitChar in __VARIABLE_DELIMETERS__:
			editVar = editVar[:-1].split(splitChar)[-1] + editVar[-1]
		
		
		# Now we should have the var by its self
		if editVar:
			possibilities = []
			
			for __TMP_VAR_NAME__ in RECURSIVE_DIR: #.keys():
				#print '\t', __TMP_VAR_NAME__
				if __TMP_VAR_NAME__ == editVar:
					# print 'ADITVAR IS A VAR'
					pass
				'''
				elif __TMP_VAR_NAME__.startswith( editVar ):
					print __TMP_VAR_NAME__, 'aaa'
					possibilities.append( __TMP_VAR_NAME__ )
				'''
				possibilities.append( __TMP_VAR_NAME__ )
			
			if len(possibilities) == 1:
				cmdBuffer[-1].cmd = ('%s%s%s' % (cmdBuffer[-1].cmd[:cursor - len(editVar)], possibilities[0], cmdBuffer[-1].cmd[cursor:]))    
			
			elif possibilities: # If its not just []
				# -1 with insert is the second last.
				
				# Text choice
				#cmdBuffer.insert(-1, cmdLine('options: %s' % ' '.join(possibilities), 3, None))
				
				menuList = [] # A lits of tuples- ABSOLUTE, RELATIVE
				
				for __TMP_VAR_NAME__ in possibilities:
					for usage in RECURSIVE_DIR[__TMP_VAR_NAME__]:
						# Account for non absolute (variables for eg.)
						if usage: # not ''
							absName = '%s.%s' % (usage, __TMP_VAR_NAME__)
							
							if __TMP_VAR_NAME__.startswith(editVar):
								menuList.append( # Used for names and can be entered when pressing shift.
								  (absName, # Absolute name
								  __TMP_VAR_NAME__) # Relative name, non shift
								  )
						
						#else:
						#	if absName.find(editVar) != -1:
						#		menuList.append((__TMP_VAR_NAME__, __TMP_VAR_NAME__)) # Used for names and can be entered when pressing shift.
				
				# No items to display? no menu
				if not menuList:
					return
					
				menuList.sort()
				
				choice = PupMenuLess( # Menu for the user to choose the autocompleate
				'Choices (Shift for local name, Ctrl for Docs)%t|' + # Title Text
				'|'.join(['%s,  %s' % m for m in menuList])) # Use Absolute names m[0]
				
				if choice != -1:
					if Window.GetKeyQualifiers() & Window.Qual.CTRL:  # Help
						cmdBuffer[-1].cmd = ('help(%s%s) ' % (cmdBuffer[-1].cmd[:cursor - len(editVar)], menuList[choice-1][0]))    
					elif Window.GetKeyQualifiers() & Window.Qual.SHIFT:  # Put in the long name
						cmdBuffer[-1].cmd = ('%s%s%s' % (cmdBuffer[-1].cmd[:cursor - len(editVar)], menuList[choice-1][1], cmdBuffer[-1].cmd[cursor:]))    
					else: # Only paste in the Short name
						cmdBuffer[-1].cmd = ('%s%s%s' % (cmdBuffer[-1].cmd[:cursor - len(editVar)], menuList[choice-1][0], cmdBuffer[-1].cmd[cursor:]))    
						
						
		else:
			# print 'NO EDITVAR'
			return
		
	# ------------------end------------------ #
	
	# Quit from menu only
	#if (evt == Draw.ESCKEY and not val):
	#	Draw.Exit()
	if evt == Draw.MOUSEX or evt == Draw.MOUSEY: # AVOID TOO MANY REDRAWS.
		return	
	
	
	global cursor
	global histIndex	
	global __FONT_SIZE__
	global __CONSOLE_LINE_OFFSET__
	
	ascii = Blender.event
	
	resetScroll = True
	
	#------------------------------------------------------------------------------#
	#                             key codes and key handling                       #
	#------------------------------------------------------------------------------#
	
	# UP DOWN ARROW KEYS, TO TRAVERSE HISTORY
	if (evt == Draw.UPARROWKEY and val): actionUpKey()
	elif (evt == Draw.DOWNARROWKEY and val): actionDownKey()
	
	elif (evt == Draw.RIGHTARROWKEY and val):
		if Window.GetKeyQualifiers() & Window.Qual.SHIFT:
			wordJump = False
			newCursor = cursor+1
			while newCursor<0:
				
				if cmdBuffer[-1].cmd[newCursor] not in __DELIMETERS__:
					newCursor+=1
				else:
					wordJump = True
					break
			if wordJump: # Did we find a new cursor pos?
				cursor = newCursor
			else:
				cursor = -1 # end of line
		else:
			cursor +=1
			if cursor > -1:
				cursor = -1
	
	elif (evt == Draw.LEFTARROWKEY and val):
		if Window.GetKeyQualifiers() & Window.Qual.SHIFT:
			wordJump = False
			newCursor = cursor-1
			while abs(newCursor) < len(cmdBuffer[-1].cmd):
				
				if cmdBuffer[-1].cmd[newCursor] not in __DELIMETERS__ or\
				newCursor == cursor:
					newCursor-=1
				else:
					wordJump = True
					break
			if wordJump: # Did we find a new cursor pos?
				cursor = newCursor
			else: 
				cursor = -len(cmdBuffer[-1].cmd) # Start of line
			
		else:
			if len(cmdBuffer[-1].cmd) > abs(cursor):
				cursor -=1
	
	elif (evt == Draw.HOMEKEY and val):
		cursor  = -len(cmdBuffer[-1].cmd)
	
	elif (evt == Draw.ENDKEY and val):
		cursor = -1
	
	elif (evt == Draw.TABKEY and val):
		insCh('\t')	
	
	elif (evt == Draw.BACKSPACEKEY and val):
		if Window.GetKeyQualifiers() & Window.Qual.SHIFT:
			i = -1
			for d in __DELIMETERS__:
				i = max(i, cmdBuffer[-1].cmd[:cursor-1].rfind(d))
			if i == -1:
				i=0
			cmdBuffer[-1].cmd = ('%s%s' % (cmdBuffer[-1].cmd[:i] , cmdBuffer[-1].cmd[cursor:]))
			
		else:
			# Normal backspace.
			cmdBuffer[-1].cmd = ('%s%s' % (cmdBuffer[-1].cmd[:cursor-1] , cmdBuffer[-1].cmd[cursor:]))
			
	elif (evt == Draw.DELKEY and val) and cursor < -1:
		cmdBuffer[-1].cmd = cmdBuffer[-1].cmd[:cursor] + cmdBuffer[-1].cmd[cursor+1:]
		cursor +=1
	
	elif ((evt == Draw.RETKEY or evt == Draw.PADENTER) and val):
		actionEnterKey()
			
	elif (evt == Draw.RIGHTMOUSE and not val): actionRightMouse(); return
	
	elif (evt == Draw.PADPLUSKEY or evt == Draw.EQUALKEY or evt == Draw.WHEELUPMOUSE) and val and Window.GetKeyQualifiers() & Window.Qual.CTRL:
		__FONT_SIZE__ += 1
		__FONT_SIZE__ = min(len(__FONT_SIZES__)-1, __FONT_SIZE__)
	elif (evt == Draw.PADMINUS or evt == Draw.MINUSKEY or evt == Draw.WHEELDOWNMOUSE) and val and Window.GetKeyQualifiers() & Window.Qual.CTRL:
		__FONT_SIZE__ -=1
		__FONT_SIZE__ = max(0, __FONT_SIZE__)
	
	
	elif evt == Draw.WHEELUPMOUSE and val:
		__CONSOLE_LINE_OFFSET__ += 1
		__CONSOLE_LINE_OFFSET__ = min(len(cmdBuffer)-2, __CONSOLE_LINE_OFFSET__)
		resetScroll = False
		
	elif evt == Draw.WHEELDOWNMOUSE and val:
		__CONSOLE_LINE_OFFSET__ -= 1
		__CONSOLE_LINE_OFFSET__ = max(0, __CONSOLE_LINE_OFFSET__)
		resetScroll = False
	

	elif ascii:
		insCh(chr(ascii))
	else:
		return # dont redraw.
	
	# If the user types in anything then scroll to bottom.
	if resetScroll:
		__CONSOLE_LINE_OFFSET__ = 0
	Draw.Redraw()


def draw_gui():
	# Get the bounds from ObleGL directly
	__CONSOLE_RECT__ = BGL.Buffer(BGL.GL_FLOAT, 4)
	BGL.glGetFloatv(BGL.GL_SCISSOR_BOX, __CONSOLE_RECT__) 
	__CONSOLE_RECT__= __CONSOLE_RECT__.list
	
	# Clear the screen
	BGL.glClearColor(0.0, 0.0, 0.0, 1.0)
	BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)         # use it to clear the color buffer
	
	
	# Fixed margin. use a margin since 0 margin can be hard to seewhen close to a crt's edge.
	margin = 4
	
	# Draw cursor location colour
	if __CONSOLE_LINE_OFFSET__ == 0:
		cmd2curWidth = Draw.GetStringWidth(cmdBuffer[-1].cmd[:cursor], __FONT_SIZES__[__FONT_SIZE__][0])
		BGL.glColor3f(0.8, 0.2, 0.2)
		if cmd2curWidth == 0:
			BGL.glRecti(margin,2,margin+2, __FONT_SIZES__[__FONT_SIZE__][1]+2)
		else:
			BGL.glRecti(margin + cmd2curWidth-2,2, margin+cmd2curWidth, __FONT_SIZES__[__FONT_SIZE__][1]+2)
	
	BGL.glColor3f(1,1,1)
	# Draw the set of cammands to the buffer
	consoleLineIdx = __CONSOLE_LINE_OFFSET__ + 1
	wrapLineIndex = 0
	while consoleLineIdx < len(cmdBuffer) and  __CONSOLE_RECT__[3] > (consoleLineIdx - __CONSOLE_LINE_OFFSET__) * __FONT_SIZES__[__FONT_SIZE__][1]:
		if cmdBuffer[-consoleLineIdx].type == 0:
			BGL.glColor3f(1, 1, 1)
		elif cmdBuffer[-consoleLineIdx].type == 1:
			BGL.glColor3f(.3, .3, 1)
		elif cmdBuffer[-consoleLineIdx].type == 2:
			BGL.glColor3f(1.0, 0, 0)
		elif cmdBuffer[-consoleLineIdx].type == 3:
			BGL.glColor3f(0, 0.8, 0)
		else:  
			BGL.glColor3f(1, 1, 0)
		
		if consoleLineIdx == 1: # user input
			BGL.glRasterPos2i(margin, (__FONT_SIZES__[__FONT_SIZE__][1] * (consoleLineIdx-__CONSOLE_LINE_OFFSET__)) - 8)
			Draw.Text(cmdBuffer[-consoleLineIdx].cmd, __FONT_SIZES__[__FONT_SIZE__][0])		
		else:
			BGL.glRasterPos2i(margin, (__FONT_SIZES__[__FONT_SIZE__][1] * ((consoleLineIdx-__CONSOLE_LINE_OFFSET__)+wrapLineIndex)) - 8)
			Draw.Text(cmdBuffer[-consoleLineIdx].cmd, __FONT_SIZES__[__FONT_SIZE__][0])

		# Wrapping is totally slow, can even hang blender - dont do it!
		'''
		if consoleLineIdx == 1: # NEVER WRAP THE USER INPUT
			BGL.glRasterPos2i(margin, (__FONT_SIZES__[__FONT_SIZE__][1] * (consoleLineIdx-__CONSOLE_LINE_OFFSET__)) - 8)
			# BUG, LARGE TEXT DOSENT DISPLAY
			Draw.Text(cmdBuffer[-consoleLineIdx].cmd, __FONT_SIZES__[__FONT_SIZE__][0])
			
		
		else: # WRAP?
			# LINE WRAP
			if Draw.GetStringWidth(cmdBuffer[-consoleLineIdx].cmd, __FONT_SIZES__[__FONT_SIZE__][0]) >  __CONSOLE_RECT__[2]:
				wrapLineList = []
				copyCmd = [cmdBuffer[-consoleLineIdx].cmd, '']
				while copyCmd != ['','']:
					while margin + Draw.GetStringWidth(copyCmd[0], __FONT_SIZES__[__FONT_SIZE__][0]) > __CONSOLE_RECT__[2]:
						#print copyCmd
						copyCmd[1] = '%s%s'% (copyCmd[0][-1], copyCmd[1]) # Add the char on the end
						copyCmd[0] = copyCmd[0][:-1]# remove last chat
					
					# Now we have copyCmd[0] at a good length we can print it.					
					if copyCmd[0] != '':
						wrapLineList.append(copyCmd[0])
					
					copyCmd[0]=''
					copyCmd = [copyCmd[1], copyCmd[0]]
				
				# Now we have a list of lines, draw them (OpenGLs reverse ordering requires this odd change)
				wrapLineList.reverse()
				for wline in wrapLineList:
					BGL.glRasterPos2i(margin, (__FONT_SIZES__[__FONT_SIZE__][1]*((consoleLineIdx-__CONSOLE_LINE_OFFSET__) + wrapLineIndex)) - 8)
					Draw.Text(wline, __FONT_SIZES__[__FONT_SIZE__][0])
					wrapLineIndex += 1
				wrapLineIndex-=1 # otherwise we get a silly extra line.
				
			else: # no wrapping.
				
				BGL.glRasterPos2i(margin, (__FONT_SIZES__[__FONT_SIZE__][1] * ((consoleLineIdx-__CONSOLE_LINE_OFFSET__)+wrapLineIndex)) - 8)
				Draw.Text(cmdBuffer[-consoleLineIdx].cmd, __FONT_SIZES__[__FONT_SIZE__][0])
		'''
		consoleLineIdx += 1
			

# This recieves the event index, call a function from here depending on the event.
def handle_button_event(evt):
	pass


# Run the console
__CONSOLE_VAR_DICT__ = {} # Initialize var dict


# Print Startup lines, add __bpydoc__ to the console startup.
cmdBuffer = []
for l in __bpydoc__.split('<br>'):
	cmdBuffer.append( cmdLine(l, 1, None) )
	

histIndex = cursor = -1 # How far back from the first letter are we? - in current CMD line, history if for moving up and down lines.

# Autoexec, startup code.
scriptDir = Get('scriptsdir')
console_autoexec = None
if scriptDir:
	if not scriptDir.endswith(Blender.sys.sep):
		scriptDir += Blender.sys.sep
	
	console_autoexec  = '%s%s' % (scriptDir, 'console_autoexec.py')

	if not sys.exists(console_autoexec):
		# touch the file
		try:
			open(console_autoexec, 'w').close()
			cmdBuffer.append(cmdLine('...console_autoexec.py not found, making new in scripts dir', 1, None))
		except:
			cmdBuffer.append(cmdLine('...console_autoexec.py could not write, this is ok', 1, None))
			scriptDir = None # make sure we only use this for console_autoexec.py
	
	if not sys.exists(console_autoexec):
		console_autoexec = None
	
	else:
		cmdBuffer.append(cmdLine('...Using existing console_autoexec.py in scripts dir', 1, None))



#-Autoexec---------------------------------------------------------------------#
# Just use the function to jump into local naming mode.
# This is so we can loop through all of the autoexec functions / vars and add them to the __CONSOLE_VAR_DICT__
def include_console(includeFile):
	global __CONSOLE_VAR_DICT__ # write autoexec vars to this.
	
	# Execute an external py file as if local
	exec(include(includeFile))

def standard_imports():
	# Write local to global __CONSOLE_VAR_DICT__ for reuse,
	for ls in (dir(), dir(Blender)):
		for __TMP_VAR_NAME__ in ls:
			# Execute the local > global coversion.
			exec('%s%s' % ('__CONSOLE_VAR_DICT__[__TMP_VAR_NAME__]=', __TMP_VAR_NAME__))
	
	exec('%s%s' % ('__CONSOLE_VAR_DICT__["bpy"]=', 'bpy'))

if scriptDir and console_autoexec:
	include_console(console_autoexec) # pass the blender module

standard_imports() # import Blender and bpy

#-end autoexec-----------------------------------------------------------------#


# Append new line to write to
cmdBuffer.append(cmdLine(' ', 0, 0))

#------------------------------------------------------------------------------#
#                    register the event handling code, GUI                     #
#------------------------------------------------------------------------------#
def main():
	Draw.Register(draw_gui, handle_event, handle_button_event)

if __name__ == '__main__':
	main()
