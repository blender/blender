# Blender.Sound module and the Sound PyType object

"""
The Blender.Sound submodule.

Sound
=====

This module provides access to B{Sound} objects in Blender.

Example::
  import Blender
  from Blender import Sound
  #
  sound = Sound.Load("/path/to/my/sound.wav")    # load a sound file
  print "Sound from", sound.getFilename(),
  print "loaded to obj", sound.getName())
  print "All Sounds available now:", Sound.Get()
"""

def Load (filename):
  """
  Load the sound called 'filename' into a Sound object.
  @type filename: string
  @param filename: The full path to the sound file.
  @rtype:  Blender Sound
  @return: A Blender Sound object with the data from I{filename}.
  """

def Get (name = None):
  """
  Get the Sound object(s) from Blender.
  @type name: string
  @param name: The name of the Sound object.
  @rtype: Blender Sound or a list of Blender Sounds
  @return: It depends on the I{name} parameter:
      - (name): The Sound object called I{name}, None if not found;
      - (): A list with all Sound objects in the current scene.
  """


class Sound:
  """
  The Sound object
  ================
    This object gives access to Sounds in Blender.
  @ivar name: The name of this Sound object.
  @ivar filename: The filename (path) to the sound file loaded into this Sound
     object.
  """

  def getName():
    """
    Get the name of this Sound object.
    @rtype: string
    """

  def getFilename():
    """
    Get the filename of the sound file loaded into this Sound object.
    @rtype: string
    """

  def play():
    """
    Play this sound.
    """

  def setCurrent():
    """
    Make this the active sound in the sound buttons window (also redraws).
    """

  def getVolume():
    """
    Get this sound's volume.
    rtype: float
    """

  def setVolume(f):
    """
    Set this sound's volume.
    @type f: float
    @param f: the new volume value in the range [0.0, 1.0].
    """

  def getAttenuation():
    """
    Get this sound's attenuation value.
    rtype: float
    """

  def setAttenuation(f):
    """
    Set this sound's attenuation.
    @type f: float
    @param f: the new attenuation value in the range [0.0, 5.0].
    """

  def getPitch():
    """
    Get this sound's pitch value.
    rtype: float
    """

  def setPitch(f):
    """
    Set this sound's pitch.
    @type f: float
    @param f: the new pitch value in the range [-12.0, 12.0].    
    """

