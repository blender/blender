# Blender.Lamp module and the Lamp PyType object

"""
The Blender.Lamp submodule.

Lamp Data
=========

This module provides control over B{Lamp Data} objects in Blender.

Example::

  from Blender import Lamp
  l = Lamp.New('Spot')            # create new 'Spot' lamp data
  l.setMode('square', 'shadow')   # set these two lamp mode flags
  ob = Object.New('Lamp')         # create new lamp object
  ob.link(l)                      # link lamp obj with lamp data
"""

def New (type = 'Lamp', name = 'LampData'):
  """
  Create a new Lamp Data object.
  @type type: string
  @param type: The Lamp type: 'Lamp', 'Sun', 'Spot' or 'Hemi'.
  @type name: string
  @param name: The Lamp Data name.
  @rtype: Blender Lamp
  @return: The created Lamp Data object.
  """

def Get (name = None):
  """
  Get the Lamp Data object(s) from Blender.
  @type name: string
  @param name: The name of the Lamp Data.
  @rtype: Blender Lamp or a list of Blender Lamps
  @return: It depends on the I{name} parameter:
      - (name): The Lamp Data object with the given I{name};
      - ():     A list with all Lamp Data objects in the current scene.
  """

class Lamp:
  """
  The Lamp Data object
  ====================
    This object gives access to Lamp-specific data in Blender.
  @cvar name: The Lamp Data name.
  @cvar type: The Lamp type (see the Types dict).
  @cvar Types: The Types dictionary.
      - 'Lamp': 0
      - 'Sun' : 1
      - 'Spot': 2
      - 'Hemi': 3
  @cvar mode: The mode flags: B{or'ed value} of the flags in the Modes dict.
  @cvar Modes: The Modes dictionary.
      - 'Shadows'
      - 'Halo'
      - 'Layer'
      - 'Quad'
      - 'Negative'
      - 'OnlyShadow'
      - 'Sphere'
      - 'Square'
  @cvar samples: The number of shadow map samples in [1, 16].
  @cvar bufferSize: The size of the shadow buffer in [512, 5120].
  @cvar haloStep: Volumetric halo sampling frequency in [0, 12].
  @cvar energy: The intensity of the light in [0.0, 10.0].
  @cvar dist: The distance value in [0.1, 5000.0].
  @cvar spotSize: The angle of the spot beam in degrees in [1.0, 180.0].
  @cvar spotBlend: The softness of the spot edge in [0.0, 1.0].
  @cvar clipStart: The shadow map clip start in [0.1, 1000.0].
  @cvar clipEnd: The shadow map clip end in [1.0, 5000.0].
  @cvar bias: The shadow map sampling bias in [0.01, 5.00].
  @cvar softness: The size of the shadow sample area in [1.0, 100.0].
  @cvar haloInt: The intensity of the spot halo in [0.0, 5.0].
  @cvar quad1: Light intensity value 1 for a Quad lamp in [0.0, 1.0].
  @cvar quad2: Light intensity value 2 for a Quad lamp in [0.0, 1.0].
  @cvar col: The color of the light, with each rgb component in [0.0, 1.0].
      This is an rgb tuple whose values can be accessed in many ways:
        - as a tuple: lamp.col, lamp.col[0], same for 1 and 2.
        - as a dictionary: lamp.col['R'], same for 'G' and 'B'.
        - as an object: lamp.col.R, same for G and B.
  @warning: Most member variables assume values in some [Min, Max] interval.
      When trying to set them, the given parameter will be clamped to lie in
      that range: if val < Min, then val = Min, if val > Max, then val = Max.
  """

  def getName():
    """
    Get the name of this Lamp Data object.
    @rtype: string
    """

  def setName(name):
    """
    Set the name of this Lamp Data object.
    @type name: string
    @param name: The new name.
    """

  def getType():
    """
    Get this Lamp's type.
    @rtype: int
    """

  def setType(type):
    """
    Set this Lamp's type.
    @type type: string
    @param type: The Lamp type: 'Lamp', 'Sun', 'Spot' or 'Hemi'.
    """

  def getMode():
    """
    Get this Lamp's mode flags.
    @rtype: int
    @return: B{OR'ed value}. Use the Modes dictionary to check which flags
        are 'on'.

        Example::
          flags = mylamp.getMode()
          if flags & mylamp.Modes['Shadows']:
            print "This lamp produces shadows"
          else:
            print "The 'Shadows' flag is off"
    """

  def setMode(m = None, m2 = None, m3 = None, m4 = None,
              m5 = None, m6 = None, m7 = None, m8 = None):
    """
    Set this Lamp's mode flags. Mode strings given are turned 'on'.
    Those not provided are turned 'off', so lamp.setMode() -- without 
    arguments -- turns off all mode flags for Lamp lamp.
    @type m: string
    @param m: A mode flag. From 1 to 8 can be set at the same time.
    """

  def getSamples():
    """
    Get this lamp's samples value.
    @rtype: int
    """

  def setSamples(samples):
    """
    Set the samples value.
    @type samples: int
    @param samples: The new samples value.
    """

  def getBufferSize():
    """
    Get this lamp's buffer size.
    @rtype: int
    """

  def setBufferSize(bufsize):
    """
    Set the buffer size value.
    @type bufsize: int
    @param bufsize: The new buffer size value.
    """

  def getHaloStep():
    """
    Get this lamp's halo step value.
    @rtype: int
    """

  def setHaloStep(hastep):
    """
    Set the halo step value.
    @type hastep: int
    @param hastep: The new halo step value.
    """

  def getEnergy():
    """
    Get this lamp's energy intensity value.
    @rtype: float
    """

  def setEnergy(energy):
    """
    Set the energy intensity value.
    @type energy: float
    @param energy: The new energy value.
    """

  def getDist():
    """
    Get this lamp's distance value.
    @rtype: float
    """

  def setDist(distance):
    """
    Set the distance value.
    @type distance: float
    @param distance: The new distance value.
    """

  def getSpotSize():
    """
    Get this lamp's spot size value.
    @rtype: float
    """

  def setSpotSize(spotsize):
    """
    Set the spot size value.
    @type spotsize: float
    @param spotsize: The new spot size value.
    """

  def getSpotBlend():
    """
    Get this lamp's spot blend value.
    @rtype: float
    """

  def setSpotBlend(spotblend):
    """
    Set the spot blend value.
    @type spotblend: float
    @param spotblend: The new spot blend value.
    """

  def getClipStart():
    """
    Get this lamp's clip start value.
    @rtype: float
    """

  def setClipStart(clipstart):
    """
    Set the clip start value.
    @type clipstart: float
    @param clipstart: The new clip start value.
    """

  def getClipEnd():
    """
    Get this lamp's clip end value.
    @rtype: float
    """

  def setClipEnd(clipend):
    """
    Set the clip end value.
    @type clipend: float
    @param clipend: The new clip end value.
    """ 

  def getBias():
    """
    Get this lamp's bias value.
    @rtype: float
    """

  def setBias(bias):
    """
    Set the bias value.
    @type bias: float
    @param bias: The new bias value.
    """ 

  def getSoftness():
    """
    Get this lamp's softness value.
    @rtype: float
    """

  def setSoftness(softness):
    """
    Set the softness value.
    @type softness: float
    @param softness: The new softness value.
    """ 

  def getHaloInt():
    """
    Get this lamp's halo intensity value.
    @rtype: float
    """

  def setHaloInt(haloint):
    """
    Set the halo intensity value.
    @type haloint: float
    @param haloint: The new halo intensity value.
    """ 

  def getQuad1():
    """
    Get this lamp's quad 1 value.
    @rtype: float
    @warning: this only applies to Lamps with the 'Quad' flag on.
    """

  def setQuad1(quad1):
    """
    Set the quad 1 value.
    @type quad1: float
    @warning: this only applies to Lamps with the 'Quad' flag on.
    """ 

  def getQuad2():
    """
    Get this lamp's quad 2 value.
    @rtype: float
    @warning: this only applies to Lamps with the 'Quad' flag on.
    """

  def setQuad2(quad2):
    """
    Set the quad 2 value.
    @type quad2: float
    @param quad2: The new quad 2 value.
    @warning: this only applies to Lamps with the 'Quad' flag on.
    """ 
