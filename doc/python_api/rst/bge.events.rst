
Game Keys (bge.events)
======================

*****
Intro
*****

This module holds key constants for the SCA_KeyboardSensor.

.. module:: bge.events

.. code-block:: python

	# Set a connected keyboard sensor to accept F1
	import bge
	
	co = bge.logic.getCurrentController()
	# 'Keyboard' is a keyboard sensor
	sensor = co.sensors["Keyboard"]
	sensor.key = bge.events.F1KEY

.. code-block:: python

	# Do the all keys thing
	import bge
	
	co = bge.logic.getCurrentController()
	# 'Keyboard' is a keyboard sensor
	sensor = co.sensors["Keyboard"]

	for key,status in sensor.events:
		# key[0] == bge.events.keycode, key[1] = status
		if status == bge.logic.KX_INPUT_JUST_ACTIVATED:
			if key == bge.events.WKEY:
				# Activate Forward!
			if key == bge.events.SKEY:
				# Activate Backward!
			if key == bge.events.AKEY:
				# Activate Left!
			if key == bge.events.DKEY:
				# Activate Right!

.. code-block:: python

	# The all keys thing without a keyboard sensor (but you will
	# need an always sensor with pulse mode on)
	import bge
	
	# Just shortening names here
	keyboard = bge.logic.keyboard
	JUST_ACTIVATED = bge.logic.KX_INPUT_JUST_ACTIVATED
	
	if keyboard.events[bge.events.WKEY] == JUST_ACTIVATED:
		print("Activate Forward!")
	if keyboard.events[bge.events.SKEY] == JUST_ACTIVATED:
		print("Activate Backward!")	
	if keyboard.events[bge.events.AKEY] == JUST_ACTIVATED:
		print("Activate Left!")	
	if keyboard.events[bge.events.DKEY] == JUST_ACTIVATED:
		print("Activate Right!")
		

*********
Functions
*********

.. function:: EventToString(event)

   Return the string name of a key event. Will raise a ValueError error if its invalid.

   :arg event: key event constant from :mod:`bge.events` or the keyboard sensor.
   :type event: int
   :rtype: string
   
.. function:: EventToCharacter(event, shift)

   Return the string name of a key event. Returns an empty string if the event cant be represented as a character.
   
   :type event: int
   :arg event: key event constant from :mod:`bge.events` or the keyboard sensor.
   :type shift: bool
   :arg shift: set to true if shift is held.
   :rtype: string

****************
Keys (Constants)
****************

.. _mouse-keys:

==========
Mouse Keys
==========

.. data:: LEFTMOUSE
.. data:: MIDDLEMOUSE
.. data:: RIGHTMOUSE
.. data:: WHEELUPMOUSE
.. data:: WHEELDOWNMOUSE
.. data:: MOUSEX
.. data:: MOUSEY

.. _keyboard-keys:

=============
Keyboard Keys
=============

-------------
Alphabet keys
-------------

.. data:: AKEY
.. data:: BKEY
.. data:: CKEY
.. data:: DKEY
.. data:: EKEY
.. data:: FKEY
.. data:: GKEY
.. data:: HKEY
.. data:: IKEY
.. data:: JKEY
.. data:: KKEY
.. data:: LKEY
.. data:: MKEY
.. data:: NKEY
.. data:: OKEY
.. data:: PKEY
.. data:: QKEY
.. data:: RKEY
.. data:: SKEY
.. data:: TKEY
.. data:: UKEY
.. data:: VKEY
.. data:: WKEY
.. data:: XKEY
.. data:: YKEY
.. data:: ZKEY

-----------
Number keys
-----------

.. data:: ZEROKEY
.. data:: ONEKEY
.. data:: TWOKEY
.. data:: THREEKEY
.. data:: FOURKEY
.. data:: FIVEKEY
.. data:: SIXKEY
.. data:: SEVENKEY
.. data:: EIGHTKEY
.. data:: NINEKEY

--------------
Modifiers Keys
--------------

.. data:: CAPSLOCKKEY
.. data:: LEFTCTRLKEY
.. data:: LEFTALTKEY
.. data:: RIGHTALTKEY
.. data:: RIGHTCTRLKEY
.. data:: RIGHTSHIFTKEY
.. data:: LEFTSHIFTKEY

----------
Arrow Keys
----------

.. data:: LEFTARROWKEY
.. data:: DOWNARROWKEY
.. data:: RIGHTARROWKEY
.. data:: UPARROWKEY

--------------
Numberpad Keys
--------------

.. data:: PAD0
.. data:: PAD1
.. data:: PAD2
.. data:: PAD3
.. data:: PAD4
.. data:: PAD5
.. data:: PAD6
.. data:: PAD7
.. data:: PAD8
.. data:: PAD9
.. data:: PADPERIOD
.. data:: PADSLASHKEY
.. data:: PADASTERKEY
.. data:: PADMINUS
.. data:: PADENTER
.. data:: PADPLUSKEY

-------------
Function Keys
-------------

.. data:: F1KEY
.. data:: F2KEY
.. data:: F3KEY
.. data:: F4KEY
.. data:: F5KEY
.. data:: F6KEY
.. data:: F7KEY
.. data:: F8KEY
.. data:: F9KEY
.. data:: F10KEY
.. data:: F11KEY
.. data:: F12KEY
.. data:: F13KEY
.. data:: F14KEY
.. data:: F15KEY
.. data:: F16KEY
.. data:: F17KEY
.. data:: F18KEY
.. data:: F19KEY

----------
Other Keys
----------

.. data:: ACCENTGRAVEKEY
.. data:: BACKSLASHKEY
.. data:: BACKSPACEKEY
.. data:: COMMAKEY
.. data:: DELKEY
.. data:: ENDKEY
.. data:: EQUALKEY
.. data:: ESCKEY
.. data:: HOMEKEY
.. data:: INSERTKEY
.. data:: LEFTBRACKETKEY
.. data:: LINEFEEDKEY
.. data:: MINUSKEY
.. data:: PAGEDOWNKEY
.. data:: PAGEUPKEY
.. data:: PAUSEKEY
.. data:: PERIODKEY
.. data:: QUOTEKEY
.. data:: RIGHTBRACKETKEY
.. data:: RETKEY (Deprecated: use bge.events.ENTERKEY)
.. data:: ENTERKEY
.. data:: SEMICOLONKEY
.. data:: SLASHKEY
.. data:: SPACEKEY
.. data:: TABKEY
