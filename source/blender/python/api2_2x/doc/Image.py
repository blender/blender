# Blender.Image module and the Image PyType object

"""
The Blender.Image submodule.

Image
=====

This module provides access to B{Image} objects in Blender.

Example::
  import Blender
  from Blender import Image
  #
  image = Image.Load("/path/to/my/image.png")    # load an image file
  print "Image from", image.getFilename(),
  print "loaded to obj", image.getName())
  image.setXRep(4)                               # set x tiling factor
  image.setYRep(2)                               # set y tiling factor
  print "All Images available now:", Image.Get()
"""

def Load (filename):
  """
  Load the image called 'filename' into an Image object.
  @type filename: string
  @param filename: The full path to the image file.
  @rtype:  Blender Image
  @return: A Blender Image object with the data from I{filename}.
  """

def New (name):
  """
  Create a new Image object (not implemented yet!).
  @type name: string
  @param name: The name of the new Image object.
  @rtype: Blender Image
  @return: A new Blender Image object.
  @warn: This function wasn't implemented yet.  It simply returns None.
  """

def Get (name = None):
  """
  Get the Image object(s) from Blender.
  @type name: string
  @param name: The name of the Image object.
  @rtype: Blender Image or a list of Blender Images
  @return: It depends on the I{name} parameter:
      - (name): The Image object called I{name}, None if not found;
      - (): A list with all Image objects in the current scene.
  """


class Image:
  """
  The Image object
  ================
    This object gives access to Images in Blender.
  @cvar name: The name of this Image object.
  @cvar filename: The filename (path) to the image file loaded into this Image
     object.
  @cvar size: The [width, height] dimensions of the image (in pixels).
  @cvar depth: The pixel depth of the image.
  @cvar xrep: Texture tiling: the number of repetitions in the x (horizontal)
     axis.
  @cvar yrep: Texture tiling: the number of repetitions in the y (vertical)
     axis.
  """

  def getName():
    """
    Get the name of this Image object.
    @rtype: string
    """

  def getFilename():
    """
    Get the filename of the image file loaded into this Image object.
    @rtype: string
    """

  def getSize():
    """
    Get the [width, height] dimensions (in pixels) of this image.
    @rtype: list of 2 ints
    """

  def getDepth():
    """
    Get the pixel depth of this image.
    @rtype: int
    """

  def getXRep():
    """
    Get the number of repetitions in the x (horizontal) axis for this Image.
    This is for texture tiling.
    @rtype: int
    """

  def getYRep():
    """
    Get the number of repetitions in the y (vertical) axis for this Image.
    This is for texture tiling.
    @rtype: int
    """
  def reload():
    """
    Reloads this image from the filesystem.  If used within a loop you need to
    redraw the Window to see the change in the image, e.g. with
    Window.RedrawAll().
    @warn: if the image file is corrupt or still being written, it will be
       replaced by a blank image in Blender, but no error will be returned.
    @returns: None
    """

  def setName(name):
    """
    Set the name of this Image object.
    @type name: string
    @param name: The new name.
    """

  def setXRep(xrep):
    """
    Texture tiling: set the number of x repetitions for this Image.
    @type xrep: int
    @param xrep: The new value in [1, 16].
    """

  def setYRep(yrep):
    """
    Texture tiling: set the number of y repetitions for this Image.
    @type yrep: int
    @param yrep: The new value in [1, 16].
    """
