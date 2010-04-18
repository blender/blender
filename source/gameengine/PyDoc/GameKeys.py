# $Id$
"""
Documentation for the GameKeys module.
======================================

This module holds key constants for the SCA_KeyboardSensor.


Example::
	# Set a connected keyboard sensor to accept F1
	import GameLogic
	import GameKeys
	
	co = GameLogic.getCurrentController()
	# 'Keyboard' is a keyboard sensor
	sensor = co.getSensor('Keyboard')
	sensor.key = GameKeys.F1KEY

Example::
	# Do the all keys thing
	import GameLogic
	import GameKeys
	
	co = GameLogic.getCurrentController()
	# 'Keyboard' is a keyboard sensor
	sensor = co.getSensor('Keyboard')
	keylist = sensor.events
	for key in keylist:
		# key[0] == GameKeys.keycode, key[1] = status
		if key[1] == GameLogic.KX_INPUT_JUST_ACTIVATED:
			if key[0] == GameKeys.WKEY:
				# Activate Forward!
			if key[0] == GameKeys.SKEY:
				# Activate Backward!
			if key[0] == GameKeys.AKEY:
				# Activate Left!
			if key[0] == GameKeys.DKEY:
				# Activate Right!

@group Alphabet keys: AKEY, BKEY, CKEY, DKEY, EKEY, FKEY, GKEY, HKEY, IKEY, JKEY, KKEY, LKEY, MKEY, NKEY, OKEY, PKEY, QKEY, RKEY, SKEY, TKEY, UKEY, VKEY, WKEY, XKEY, YKEY, ZKEY
@var AKEY:
@var BKEY:
@var CKEY:
@var DKEY:
@var EKEY:
@var FKEY:
@var GKEY:
@var HKEY:
@var IKEY:
@var JKEY:
@var KKEY:
@var LKEY:
@var MKEY:
@var NKEY:
@var OKEY:
@var PKEY:
@var QKEY:
@var RKEY:
@var SKEY:
@var TKEY:
@var UKEY:
@var VKEY:
@var WKEY:
@var XKEY:
@var YKEY:
@var ZKEY:

@group Number keys: ZEROKEY, ONEKEY, TWOKEY, THREEKEY, FOURKEY, FIVEKEY, SIXKEY, SEVENKEY, EIGHTKEY, NINEKEY
@var ZEROKEY:
@var ONEKEY:
@var TWOKEY:
@var THREEKEY:
@var FOURKEY:
@var FIVEKEY:
@var SIXKEY:
@var SEVENKEY:
@var EIGHTKEY:
@var NINEKEY:

@group Modifiers: CAPSLOCKKEY, LEFTCTRLKEY, LEFTALTKEY, RIGHTALTKEY, RIGHTCTRLKEY, RIGHTSHIFTKEY, LEFTSHIFTKEY
@var CAPSLOCKKEY:
@var LEFTCTRLKEY:
@var LEFTALTKEY:
@var RIGHTALTKEY:
@var RIGHTCTRLKEY:
@var RIGHTSHIFTKEY:
@var LEFTSHIFTKEY:

@group Arrow Keys: LEFTARROWKEY, DOWNARROWKEY, RIGHTARROWKEY, UPARROWKEY
@var LEFTARROWKEY:
@var DOWNARROWKEY:
@var RIGHTARROWKEY:
@var UPARROWKEY:

@group Numberpad Keys: PAD0, PAD1, PAD2, PAD3, PAD4, PAD5, PAD6, PAD7, PAD8, PAD9, PADPERIOD, PADSLASHKEY, PADASTERKEY, PADMINUS, PADENTER, PADPLUSKEY
@var PAD0:
@var PAD1:
@var PAD2:
@var PAD3:
@var PAD4:
@var PAD5:
@var PAD6:
@var PAD7:
@var PAD8:
@var PAD9:
@var PADPERIOD:
@var PADSLASHKEY:
@var PADASTERKEY:
@var PADMINUS:
@var PADENTER:
@var PADPLUSKEY:

@group Function Keys: F1KEY, F2KEY, F3KEY, F4KEY, F5KEY, F6KEY, F7KEY, F8KEY, F9KEY, F10KEY, F11KEY, F12KEY
@var F1KEY:
@var F2KEY:
@var F3KEY:
@var F4KEY:
@var F5KEY:
@var F6KEY:
@var F7KEY:
@var F8KEY:
@var F9KEY:
@var F10KEY:
@var F11KEY:
@var F12KEY:

@group Other Keys: ACCENTGRAVEKEY, BACKSLASHKEY, BACKSPACEKEY, COMMAKEY, DELKEY, ENDKEY, EQUALKEY, ESCKEY, HOMEKEY, INSERTKEY, LEFTBRACKETKEY, LINEFEEDKEY, MINUSKEY, PAGEDOWNKEY, PAGEUPKEY, PAUSEKEY, PERIODKEY, QUOTEKEY, RIGHTBRACKETKEY, RETKEY, SEMICOLONKEY, SLASHKEY, SPACEKEY, TABKEY
@var ACCENTGRAVEKEY:
@var BACKSLASHKEY:
@var BACKSPACEKEY:
@var COMMAKEY:
@var DELKEY:
@var ENDKEY:
@var EQUALKEY:
@var ESCKEY:
@var HOMEKEY:
@var INSERTKEY:
@var LEFTBRACKETKEY:
@var LINEFEEDKEY:
@var MINUSKEY:
@var PAGEDOWNKEY:
@var PAGEUPKEY:
@var PAUSEKEY:
@var PERIODKEY:
@var QUOTEKEY:
@var RIGHTBRACKETKEY:
@var RETKEY:
@var SEMICOLONKEY:
@var SLASHKEY:
@var SPACEKEY:
@var TABKEY:

@group Mouse Events: LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE, WHEELUPMOUSE, WHEELDOWNMOUSE, MOUSEX, MOUSEY
@var LEFTMOUSE:
@var MIDDLEMOUSE:
@var RIGHTMOUSE:
@var WHEELUPMOUSE:
@var WHEELDOWNMOUSE:
@var MOUSEX:
@var MOUSEY:

"""

def EventToString(event):
	"""
	Return the string name of a key event. Will raise a ValueError error if its invalid.
	
	@type event: int
	@param event: key event from GameKeys or the keyboard sensor.
	@rtype: string
	"""
	
def EventToCharacter(event, shift):
	"""
	Return the string name of a key event. Returns an empty string if the event cant be represented as a character.
	
	@type event: int
	@param event: key event from GameKeys or the keyboard sensor.
	@type shift: bool
	@param shift: set to true if shift is held.
	@rtype: string
	"""

