# $Id$
"""
New Python Functionality in this Version of Blender
===================================================

This document lists what has been changed in the Game Engine Python API.

Blender CVS
------------
	- Added tic rate methods to L{GameLogic}

Blender 2.34
------------

	- Added getType() and setType() to L{BL_ActionActuator} and L{KX_SoundActuator} (sgefant)
	- New Scene module: L{KX_Scene}
	- New Camera module: L{KX_Camera}
	- New Light module: L{KX_Light}
	- Added attributes to L{KX_GameObject}, L{KX_VertexProxy}
	- L{KX_SCA_AddObjectActuator}.setObject(), L{KX_TrackToActuator}.setObject() and 
	  L{KX_SceneActuator}.setCamera() now accept L{KX_GameObject}s as parameters

"""
