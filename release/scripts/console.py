#!BPY

"""
Name: 'Interactive Console'
Blender: 237
Group: 'System'
Tooltip: 'Interactive Python Console'
"""

__author__ = "Campbell Barton AKA Ideasman"
__url__ = ["Author's homepage, http://members.iinet.net.au/~cpbarton/ideasman/", "blender", "elysiun", "Official Python site, http://www.python.org"]
__bpydoc__ = """\
This is an interactive console, similar to Python's own command line interpreter.  Since it is embedded in Blender, it has access to all Blender Python modules.

Those completely new to Python are recommended to check the link button above
that points to its official homepage, with news, downloads and documentation.

Usage:<br>
  Type your code and hit "Enter" to get it executed.<br>
  - Right mouse click: Console Menu (Save output, etc);<br>
  - Arrow keys: command history and cursor;<br>
  - Shift + arrow keys: jump words;<br>
  - Ctrl + Tab: auto compleate based on variable names and modules loaded -- multiple choices popup a menu;<br>
  - Ctrl + Enter: multiline functions -- delays executing code until only Enter is pressed.
"""

import Blender
from Blender import *
import sys as python_sys
import StringIO
import types

# Constants
__DELIMETERS__ = '. ,=+-*/%<>&~][{}():'
__LINE_HISTORY__ = 200

global __LINE_HEIGHT__
__LINE_HEIGHT__ = 14
global __FONT_SIZE__
__FONT_SIZE__ = "normal"


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

class cmdLine:
	# cmd: is the command string, or any other message
	# type: 0:user input  1:program feedback  2:error message.  3:option feedback
	# exe; 0- not yet executed   1:executed
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
	textNames = [tex.name for tex in Text.Get()]
	if textNames:
		choice = Draw.PupMenu('|'.join(textNames))
		if choice != -1:
			text = Text.Get()[choice-1]
			
			# Add the text!
			for l in text.asLines():
				cmdBuffer.append(cmdLine('%s ' % l, 0, 0))
			Draw.Redraw()
	

COLLECTED_VAR_NAMES = {} # a list of keys, each key has a list of absolute paths

# Pain and simple recursice dir(), accepts a string
def rdir(dirString):
	global COLLECTED_VAR_NAMES
	dirStringSplit = dirString.split('.')
	
	exec('dirList = dir(%s)' % dirString) 
	for dirItem in dirList:
		if not dirItem.startswith('_'):
			if dirItem not in COLLECTED_VAR_NAMES.keys():
				COLLECTED_VAR_NAMES[dirItem] = []
			
			# Add the string
			splitD = dirString.split('"')[-2]
			if splitD not in COLLECTED_VAR_NAMES[dirItem]:
				COLLECTED_VAR_NAMES[dirItem].append(splitD)
			
			
				
			# Stops recursice stuff, overlooping
			if type(dirItem) == types.ClassType or \
				 type(dirItem) == types.ModuleType:
				
				print dirString, splitD, dirItem
				
				# Dont loop up dirs for strings ints etc.
				if d not in dirStringSplit:
					rdir( '%s.%s' % (dirString, d)) 

def recursive_dir():
	global COLLECTED_VAR_NAMES
	
	for name in __CONSOLE_VAR_DICT__.keys():
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
		for __TMP_VAR_NAME__ in __CONSOLE_VAR_DICT__.keys():
			exec('%s%s%s%s' % (__TMP_VAR_NAME__,'=__CONSOLE_VAR_DICT__["', __TMP_VAR_NAME__, '"]'))
		del __TMP_VAR_NAME__
		
		# Now all the vars are loaded, execute the code. # Newline thanks to phillip,
		exec(compile(__USER_CODE_STRING__, 'blender_cmd.py', 'single')) #exec(compile(__USER_CODE_STRING__, 'blender_cmd.py', 'exec'))
		
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
		error = str(python_sys.exc_value)
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
		# Check for the neter kay hit
		if Window.GetKeyQualifiers() & Window.Qual.CTRL: # HOLDING DOWN SHIFT, GO TO NEXT LINE.
			cmdBuffer.append(cmdLine(' ', 0, 0))
		else:
			# Multiline code will still run with 1 line,
			multiLineCode = ['if 1:'] 
			if cmdBuffer[-1].cmd != ' ':
				multiLineCode = ['%s%s' % (' ', cmdBuffer[-1].cmd)] # added space for fake if.
			else:
				cmdBuffer[-1].type = 1
				multiLineCode = []
			cmdBuffer[-1].exe = 1
			i = 2
			while cmdBuffer[-i].exe == 0:
				if cmdBuffer[-i].cmd == ' ':# Tag as an output type so its not used in the key history
					cmdBuffer[-i].type = 1
				else: # space added at the start for added if 1: statement
					multiLineCode.append('%s%s' % (' ', cmdBuffer[-i].cmd) )
				
				# Mark as executed
				cmdBuffer[-i].exe = 1
				i+=1
			
			# add if to the end, reverse will make it the start.
			multiLineCode.append('if 1:')
			multiLineCode.reverse()
			multiLineCode.append(' pass') # Now this is the end
			
			
			runUserCode('\n'.join(multiLineCode))
			
			# Clear the output based on __LINE_HISTORY__
			if len(cmdBuffer) > __LINE_HISTORY__:
				cmdBuffer = cmdBuffer[-__LINE_HISTORY__:]
		
		histIndex = cursor = -1 # Reset cursor and history
	
	def actionUpKey():
		global histIndex, cmdBuffer
		if abs(histIndex)+1 >= len(cmdBuffer):
			histIndex = -1
		histIndex -= 1
		while cmdBuffer[histIndex].type != 0 and abs(histIndex) < len(cmdBuffer):
			histIndex -= 1
		if cmdBuffer[histIndex].type == 0: # we found one
			cmdBuffer[-1].cmd = cmdBuffer[histIndex].cmd			
	
	def actionDownKey():
		global histIndex, cmdBuffer
		if histIndex >= -2:
			histIndex = -len(cmdBuffer)
		histIndex += 1
		while cmdBuffer[histIndex].type != 0 and histIndex != -2:
			histIndex += 1
		if cmdBuffer[histIndex].type == 0: # we found one
			cmdBuffer[-1].cmd = cmdBuffer[histIndex].cmd
	
	def actionRightMouse():
		global __FONT_SIZE__
		global __LINE_HEIGHT__
		choice = Draw.PupMenu('Console Menu%t|Write Input Data (white)|Write Output Data (blue)|Write Error Data (red)|Write All Text|%l|Insert Blender text|%l|Font Size|%l|Help|%l|Quit')
		# print choice
		if choice == 1:
			writeCmdData(cmdBuffer, 0) # type 0 user
		elif choice == 2:
			writeCmdData(cmdBuffer, 1) # type 1 user output
		elif choice == 3:
			writeCmdData(cmdBuffer, 2) # type 2 errors
		elif choice == 4:
			writeCmdData(cmdBuffer, 3) # All
		elif choice == 6:
			insertCmdData(cmdBuffer) # All
		elif choice == 8:
			# Fontsize.
			font_choice = Draw.PupMenu('Font Size%t|Large|Normal|Small|Tiny')
			if font_choice != -1:
				if font_choice == 1:
					__FONT_SIZE__ = 'large'
					__LINE_HEIGHT__ = 16
				elif font_choice == 2:
					__FONT_SIZE__ = 'normal'
					__LINE_HEIGHT__ = 14
				elif font_choice == 3:
					__FONT_SIZE__ = 'small'
					__LINE_HEIGHT__ = 12
				elif font_choice == 4:
					__FONT_SIZE__ = 'tiny'
					__LINE_HEIGHT__ = 10
				Draw.Redraw()
		elif choice == 10:
			Blender.ShowHelp('console.py')
		elif choice == 12: # Exit
			Draw.Exit()
	
	
	# Auto compleating, quite complex- use recutsice dir for the moment.
	def actionAutoCompleate(): # Ctrl + Tab
		RECURSIVE_DIR = recursive_dir()
		
		# get last name of user input
		editVar = cmdBuffer[-1].cmd[:cursor]
		
		# Split off spaces operators etc from the staryt of the command so we can use the startswith function.
		for splitChar in __DELIMETERS__:
			editVar = editVar.split(splitChar)[-1]
		
		# Now we should have the var by its self
		if editVar:
			
			possibilities = []
			
			print editVar, 'editVar'
			for __TMP_VAR_NAME__ in RECURSIVE_DIR.keys():
				if __TMP_VAR_NAME__ == editVar:
					# print 'ADITVAR IS A VAR'
					continue
				elif __TMP_VAR_NAME__.startswith( editVar ):
					possibilities.append( __TMP_VAR_NAME__ )
					
					
			if len(possibilities) == 1:
				cmdBuffer[-1].cmd = ('%s%s%s' % (cmdBuffer[-1].cmd[:cursor - len(editVar)], possibilities[0], cmdBuffer[-1].cmd[cursor:]))    
			
			elif possibilities: # If its not just []
				# -1 with insert is the second last.
				
				# Text choice
				#cmdBuffer.insert(-1, cmdLine('options: %s' % ' '.join(possibilities), 3, None))
				
				menuText = 'Choices (hold shift for whole name)%t|' 
				menuList = []
				menuListAbs = []
				possibilities.sort() # make nice :)
				for __TMP_VAR_NAME__ in possibilities:
					for usage in RECURSIVE_DIR[__TMP_VAR_NAME__]:
						# Account for non absolute (variables for eg.)
						if usage: # not ''
							menuListAbs.append('%s.%s' % (usage, __TMP_VAR_NAME__)) # Used for names and can be entered when pressing shift.
						else:
							menuListAbs.append(__TMP_VAR_NAME__) # Used for names and can be entered when pressing shift.
							
						menuList.append(__TMP_VAR_NAME__) # Used for non Shift
				
				
				#choice = Draw.PupMenu('Select Variabe name%t|' + '|'.join(possibilities)  )
				choice = Draw.PupMenu(menuText + '|'.join(menuListAbs))
				if choice != -1:
					
					if not Window.GetKeyQualifiers() & Window.Qual.SHIFT: # Only paste in the Short name
						cmdBuffer[-1].cmd = ('%s%s%s' % (cmdBuffer[-1].cmd[:cursor - len(editVar)], menuList[choice-1], cmdBuffer[-1].cmd[cursor:]))    
					else: # Put in the long name
						cmdBuffer[-1].cmd = ('%s%s%s' % (cmdBuffer[-1].cmd[:cursor - len(editVar)], menuListAbs[choice-1], cmdBuffer[-1].cmd[cursor:]))    
		else:
			# print 'NO EDITVAR'
			return
		
	# ------------------end------------------# 
	
	
	#if (evt == Draw.ESCKEY and not val):
	#	Draw.Exit()
	if evt == Draw.MOUSEX: # AVOID TOO MANY REDRAWS.
		return
	elif evt == Draw.MOUSEY:
		return
	
	
	global cursor
	global histIndex	
	
	ascii = Blender.event
	
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
		if Window.GetKeyQualifiers() & Window.Qual.CTRL:
			actionAutoCompleate()
		else:
			insCh('\t')	
	
	elif (evt == Draw.BACKSPACEKEY and val): cmdBuffer[-1].cmd = ('%s%s' % (cmdBuffer[-1].cmd[:cursor-1] , cmdBuffer[-1].cmd[cursor:]))
	elif (evt == Draw.DELKEY and val) and cursor < -1:
		cmdBuffer[-1].cmd = cmdBuffer[-1].cmd[:cursor] + cmdBuffer[-1].cmd[cursor+1:]
		cursor +=1
		
	elif ((evt == Draw.RETKEY or evt == Draw.PADENTER) and val): actionEnterKey()
	elif (evt == Draw.RIGHTMOUSE and not val):actionRightMouse(); return
	
	elif ascii:
		insCh(chr(ascii))
	else:
		return # dont redraw.
	Draw.Redraw()


def draw_gui():
	
	# Get the bounds from ObleGL directly
	__CONSOLE_RECT__ = BGL.Buffer(BGL.GL_FLOAT, 4)
	BGL.glGetFloatv(BGL.GL_SCISSOR_BOX, __CONSOLE_RECT__) 
	__CONSOLE_RECT__= __CONSOLE_RECT__.list
	
	# Clear the screen
	BGL.glClearColor(0.0, 0.0, 0.0, 1.0)
	BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)         # use it to clear the color buffer
	
	# Draw cursor location colour
	cmd2curWidth = Draw.GetStringWidth(cmdBuffer[-1].cmd[:cursor], __FONT_SIZE__)
	BGL.glColor3f(0.8, 0.2, 0.2)
	if cmd2curWidth == 0:
		BGL.glRecti(0,2,2, __LINE_HEIGHT__+2)
	else:
		BGL.glRecti(cmd2curWidth-2,2,cmd2curWidth, __LINE_HEIGHT__+2)
	
	BGL.glColor3f(1,1,1)
	# Draw the set of cammands to the buffer
	
	consoleLineIdx = 1
	wrapLineIndex = 0
	while consoleLineIdx < len(cmdBuffer) and  __CONSOLE_RECT__[3] > consoleLineIdx*__LINE_HEIGHT__ :
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
		
		if consoleLineIdx == 1: # NEVER WRAP THE USER INPUT
			BGL.glRasterPos2i(0, (__LINE_HEIGHT__*consoleLineIdx) - 8)
			Draw.Text(cmdBuffer[-consoleLineIdx].cmd, __FONT_SIZE__)
			
		
		else: # WRAP?
			# LINE WRAP
			if Draw.GetStringWidth(cmdBuffer[-consoleLineIdx].cmd, __FONT_SIZE__) >  __CONSOLE_RECT__[2]:
				wrapLineList = []
				copyCmd = [cmdBuffer[-consoleLineIdx].cmd, '']
				while copyCmd != ['','']:
					while Draw.GetStringWidth(copyCmd[0], __FONT_SIZE__) > __CONSOLE_RECT__[2]:
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
					BGL.glRasterPos2i(0, (__LINE_HEIGHT__*(consoleLineIdx + wrapLineIndex)) - 8)
					Draw.Text(wline, __FONT_SIZE__)
					wrapLineIndex += 1
				wrapLineIndex-=1 # otherwise we get a silly extra line.
				
			else: # no wrapping.
				
				BGL.glRasterPos2i(0, (__LINE_HEIGHT__*(consoleLineIdx+wrapLineIndex)) - 8)
				Draw.Text(cmdBuffer[-consoleLineIdx].cmd, __FONT_SIZE__)
			
			
		consoleLineIdx += 1
			

# This recieves the event index, call a function from here depending on the event.
def handle_button_event(evt):
	pass


# Run the console
__CONSOLE_VAR_DICT__ = {} # Initialize var dict


# Print Startup lines
cmdBuffer = [cmdLine("Welcome to Ideasman's Blender Console", 1, None),\
	cmdLine(' * Right Click:  Console Menu (Save output, etc.)', 1, None),\
	cmdLine(' * Arrow Keys:  Command history and cursor', 1, None),\
	cmdLine(' * Shift With Arrow Keys:  Jump words', 1, None),\
	cmdLine(' * Ctrl + Tab:  Auto compleate based on variable names and modules loaded, multiple choices popup a menu', 1, None),\
	cmdLine(' * Ctrl + Enter:  Multiline functions, delays executing code until only Enter is pressed.', 1, None)]
	
histIndex = cursor = -1 # How far back from the first letter are we? - in current CMD line, history if for moving up and down lines.

# Autoexec, startup code.
console_autoexec  = '%s%s' % (Get('datadir'), '/console_autoexec.py')
if not sys.exists(console_autoexec):
	# touch the file
	open(console_autoexec, 'w').close()
	cmdBuffer.append(cmdLine('...console_autoexec.py not found, making new in scripts data dir', 1, None))
else:
	cmdBuffer.append(cmdLine('...Using existing console_autoexec.py in scripts data dir', 1, None))



#-Autoexec---------------------------------------------------------------------#
# Just use the function to jump into local naming mode.
# This is so we can loop through all of the autoexec functions / vars and add them to the __CONSOLE_VAR_DICT__
def autoexecToVarList():
	global __CONSOLE_VAR_DICT__ # write autoexec vars to this.
	
	# Execute an external py file as if local
	exec(include(console_autoexec))
	
	# Write local to global __CONSOLE_VAR_DICT__ for reuse,
	for __TMP_VAR_NAME__ in dir() + dir(Blender):
		# Execute the local > global coversion.
		exec('%s%s' % ('__CONSOLE_VAR_DICT__[__TMP_VAR_NAME__]=', __TMP_VAR_NAME__))
		
autoexecToVarList() # pass the blender module
#-end autoexec-----------------------------------------------------------------#


# Append new line to write to
cmdBuffer.append(cmdLine(' ', 0, 0))

#------------------------------------------------------------------------------#
#                    register the event handling code, GUI                     #
#------------------------------------------------------------------------------#
def main():
	Draw.Register(draw_gui, handle_event, handle_button_event)

main()
