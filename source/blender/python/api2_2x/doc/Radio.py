# Blender.Scene.Radio module and the Radiosity PyType object

"""
The Blender.Scene.Radio submodule.

Radio
=====

This module gives access to B{Scene Radiosity Contexts} in Blender.

Example::
  import Blender
  from Blender import Scene

  # Only the current scene has a radiosity context.
  # Naturally, any scene can be made the current one
  # with scene.makeCurrent()

  scn = Scene.GetCurrent()

  # this is the only way to access the radiosity object:

  radio = scn.getRadiosityContext()

  radio.setDrawType('Gouraud')
  radio.setMode('ShowLimits', 'Z')

  radio.collectMeshes() # prepare patches
  radio.go() # calculate radiosity
  Blender.Redraw(-1)


@type Modes: readonly dictionary
@var Modes:
    - ShowLimits
    - Z

@type DrawTypes: readonly dictionary
@var DrawTypes:
    - Wire
    - Solid
    - Gouraud
"""

class Radio:
  """
  The Radiosity object
  ====================
    This object wraps the current Scene's radiosity context in Blender.
  """
 
  def go():
    """
    Start the radiosity simulation.  Remember to call L{collectMeshes} first.
    """

  def collectMeshes():
    """
    Convert selected visible meshes to patches for radiosity calculation.
    """

  def freeData():
    """
    Release all memory used by radiosity.
    """

