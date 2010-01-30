# This is not a real module, it's simply an introductory text.

"""
The Blender Game Engine Python API Reference
============================================

	See U{release notes<http://wiki.blender.org/index.php/Dev:Ref/Release_Notes/2.49/Game_Engine>} for updates, changes and new functionality in the Game Engine Python API.

	Blender Game Engine Modules:
	----------------------------
	
		Modules that include methods for accessing GameEngine data and functions.
		
			- L{GameLogic} utility functons for game logic.
			- L{GameKeys} keyboard input and event conversion.
			- L{Rasterizer} display and rendering.
			- L{GameTypes} contains all the python types spesific to the GameEngine.
	
	Modules with documentation in progress:
	---------------------
		- L{VideoTexture}
		- L{PhysicsConstraints}
	
	Additional Modules:
	-------------------
	
		These modules have no GameEngine specific functionality but are useful in many cases.
		
			- L{Mathutils}
			- L{Geometry}
			- L{BGL}


Introduction:
=============

	This reference documents the Blender Python API, a growing collection of
	Python modules (libraries) that give access to part of the program's internal
	data and functions.
	
	Through scripting Blender can be extended in real-time via
	U{Python <www.python.org>}, an impressive high level, multi-paradigm, open
	source language.  Newcomers are recommended to start with the tutorial that
	comes with it.

	This opens many interesting possibilities not available with logic bricks.

	Game Engine API Stability:
	--------------------------

	When writing python scripts there are a number of situations you should avoid to prevent crashes or unstable behavior.
	While the API tries to prevent problems there are some situations where error checking would be too time consuming.
	
	Known cases:
		- Memory Limits.

				There is nothing stopping you from filling a list or making a string so big that that causes blender to run out of memory, in this case python should rasie a MemoryError, but its likely blender will crash before this point.
				
		- Accessing any data that has been freed.

				For instance accessing a KX_GameObject after its End Object actuator runs.
				This will cause a SystemError, however for L{KX_MeshProxy}, L{KX_VertexProxy} and L{KX_VertexProxy} it will crash the blender game engine.
				
				See: L{GameTypes.PyObjectPlus.invalid} which many types inherit.

		- Mixing L{KX_GameObject} between scenes.

				For instance tracking/parenting an L{KX_GameObject} object to an object from other scene.

	External Modules:
	-----------------
	
	Since 2.49 support for importing modules has been added.

	This allows you to import any blender textblock with a .py extension.
	
	External python scripts may be imported as modules when the script is in the same directory as the blend file.
	
	The current blend files path is included in the sys.path for loading modules.
	All linked libraries will also be included so you can be sure when linking in assets from another blend file the scripts will load too.
	
	A note to newbie script writers:
	--------------------------------

	Interpreted languages are known to be much slower than compiled code, but for
	many applications the difference is negligible or acceptable.  Also, with
	profiling (or even simple direct timing with L{Blender.sys.time<Sys.time>}) to
	identify slow areas and well thought optimizations, the speed can be
	I{considerably} improved in many cases.  Try some of the best BPython scripts
	to get an idea of what can be done, you may be surprised.

@author: The Blender Python Team
@requires: Blender 2.49 or newer.
@version: 2.49
@see: U{www.blender.org<http://www.blender.org>}: documentation and forum
@see: U{blenderartists.org<http://blenderartists.org>}: user forum
@see: U{projects.blender.org<http://projects.blender.org>}
@see: U{www.python.org<http://www.python.org>}
@see: U{www.python.org/doc<http://www.python.org/doc>}
@see: U{Blending into Python<en.wikibooks.org/wiki/Blender_3D:_Blending_Into_Python>}: User contributed documentation, featuring a blender/python cookbook with many examples.

@note: the official version of this reference guide is only updated for each
	new Blender release.  But you can build the current SVN
	version yourself: install epydoc, grab all files in the
	source/gameengine/PyDoc/ folder of Blender's SVN and use the
	epy_docgen.sh script also found there to generate the html docs.
	Naturally you will also need a recent Blender binary to try the new
	features.  If you prefer not to compile it yourself, there is a testing
	builds forum at U{blender.org<http://www.blender.org>}.
"""
