# $Id$
"""
Documentation for the GameKeys module.
======================================

This module holds key constants for the SCA_KeyboardSensor.

Alphabet keys
-------------
	- AKEY
	- BKEY
	- CKEY
	- DKEY
	- EKEY
	- FKEY
	- GKEY
	- HKEY
	- IKEY
	- JKEY
	- KKEY
	- LKEY
	- MKEY
	- NKEY
	- OKEY
	- PKEY
	- QKEY
	- RKEY
	- SKEY
	- TKEY
	- UKEY
	- VKEY
	- WKEY
	- XKEY
	- YKEY
	- ZKEY

Number keys
-----------
	- ZEROKEY
	- ONEKEY
	- TWOKEY
	- THREEKEY
	- FOURKEY
	- FIVEKEY
	- SIXKEY
	- SEVENKEY
	- EIGHTKEY
	- NINEKEY

Shift Modifiers
---------------
	- CAPSLOCKKEY

	- LEFTCTRLKEY
	- LEFTALTKEY
	- RIGHTALTKEY
	- RIGHTCTRLKEY
	- RIGHTSHIFTKEY
	- LEFTSHIFTKEY

Arrow Keys
----------
	- LEFTARROWKEY
	- DOWNARROWKEY
	- RIGHTARROWKEY
	- UPARROWKEY

Numberpad Keys
--------------
	- PAD0
	- PAD1
	- PAD2
	- PAD3
	- PAD4
	- PAD5
	- PAD6
	- PAD7
	- PAD8
	- PAD9
	- PADPERIOD
	- PADSLASHKEY
	- PADASTERKEY
	- PADMINUS
	- PADENTER
	- PADPLUSKEY

Function Keys
-------------
	- F1KEY
	- F2KEY
	- F3KEY
	- F4KEY
	- F5KEY
	- F6KEY
	- F7KEY
	- F8KEY
	- F9KEY
	- F10KEY
	- F11KEY
	- F12KEY

Other Keys
----------
	- ACCENTGRAVEKEY
	- BACKSLASHKEY
	- BACKSPACEKEY
	- COMMAKEY
	- DELKEY
	- ENDKEY
	- EQUALKEY
	- ESCKEY
	- HOMEKEY
	- INSERTKEY
	- LEFTBRACKETKEY
	- LINEFEEDKEY
	- MINUSKEY
	- PAGEDOWNKEY
	- PAGEUPKEY
	- PAUSEKEY
	- PERIODKEY
	- QUOTEKEY
	- RIGHTBRACKETKEY
	- RETKEY
	- SEMICOLONKEY
	- SLASHKEY
	- SPACEKEY
	- TABKEY

Example::
	# Set a connected keyboard sensor to accept F1
	import GameLogic
	import GameKeys
	
	co = GameLogic.getCurrentController()
	# 'Keyboard' is a keyboard sensor
	sensor = co.getSensor('Keyboard')
	sensor.setKey(GameKeys.F1KEY)

Example::
	# Do the all keys thing
	import GameLogic
	import GameKeys

	# status: these should be added to a module somewhere
	KX_NO_INPUTSTATUS = 0
	KX_JUSTACTIVATED = 1
	KX_ACTIVE = 2
	KX_JUSTRELEASED = 3
		
	co = GameLogic.getCurrentController()
	# 'Keyboard' is a keyboard sensor
	sensor = co.getSensor('Keyboard')
	keylist = sensor.getPressedKeys()
	for key in keylist:
		# key[0] == GameKeys.keycode, key[1] = status
		if key[1] == KX_JUSTACTIVATED:
			if key[0] == GameKeys.WKEY:
				# Activate Forward!
			if key[0] == GameKeys.SKEY:
				# Activate Backward!
			if key[0] == GameKeys.AKEY:
				# Activate Left!
			if key[0] == GameKeys.DKEY:
				# Activate Right!
		
"""

def EventToString(event):
	"""
	Return the string name of a key event. Will raise a ValueError error if its invalid.
	
	@type event: int
	@param event: key event from GameKeys or the keyboard sensor.
	@rtype: string
	"""
