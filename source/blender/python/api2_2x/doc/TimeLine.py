# Blender.Scene.TimeLine module

"""
The Blender.Scene.TimeLine submodule.

TimeLine
========

This module gives access to B{Scene TimeLine Contexts} in Blender.

Example::
  import Blender
  from Blender import Scene

  # Only the current scene has a radiosity context.
  # Naturally, any scene can be made the current one
  # with scene.makeCurrent()

  scn = Scene.GetCurrent()

  # this is the only way to access the radiosity object:

  time_line = scn.getTimeLine ()
  time_line.add (50)
  time_line.add (100)
  time_line.setName (50, 'first')
  time_line.setName (100, 'second')
  
  Blender.Redraw(-1)
"""

class TimeLine:
  """
  The TimeLine object
  ===================
  This object wraps the current Scene's time line context in Blender.
  """
 
  def add(ival):
    """
    add new marker to time line
    @type ival: int
    @param ival: the frame number.
    """

  def delete(ival):
    """
    delete frame.
    @type ival: int
    @param ival: the frame number.    
    """

  def setName(ival, sval):
    """
    set name of frame.
    @type ival: int
    @type sval: string
    @param ival: the frame number.
    @param sval: the frame name.
    """

  def getName(ival):
    """
    Get name of frame.
    @type ival: int
    @param ival: the frame number.
    @rtype: string
    @return: the frame name.
    """

  def getMarked(ival):
    """
    Get name of frame.
    @type ival: int
    @param ival: the frame number.
    @rtype: int|string
    @return: the list of frame number or name.

    """

