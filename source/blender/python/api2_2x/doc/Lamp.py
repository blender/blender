# Blender.Lamp module and the Lamp PyType object

"""
The Blender.Lamp submodule.

B{New}: L{Lamp.clearScriptLinks} accepts a parameter now.

Lamp Data
=========

This module provides control over B{Lamp Data} objects in Blender.

Example::

	from Blender import Lamp, Scene
	l = Lamp.New('Spot')            # create new 'Spot' lamp data
	l.setMode('Square', 'Shadow')   # set these two lamp mode flags
	scn = Scene.GetCurrent()
	ob = scn.objects.new(l)

@type Types: read-only dictionary
@var Types: The lamp types.
	- 'Lamp': 0
	- 'Sun' : 1
	- 'Spot': 2
	- 'Hemi': 3
	- 'Area': 4
	- 'Photon': 5
@type Falloffs: read-only dictionary
@var Falloffs: The lamp falloff types.
	- CONSTANT  - Constant falloff
	- INVLINEAR - Inverse linear
	- INVSQUARE - Inverse square
	- CUSTOM    - Custom curve
	- LINQUAD   - Lin/Quad weighted
@type Modes: read-only dictionary
@var Modes: The lamp modes.  Modes may be ORed together.
	- 'Shadows'
	- 'Halo'
	- 'Layer'
	- 'Quad'
	- 'Negative'
	- 'OnlyShadow'
	- 'Sphere'
	- 'Square'
	- 'NoDiffuse'
	- 'NoSpecular'
	- 'RayShadow'

	Example::
		from Blender import Lamp, Object
		# Change the mode of selected lamp objects.
		for ob in Object.GetSelected():   # Loop through the current selection
			if ob.getType() == "Lamp":      # if this is a lamp.
				lamp = ob.getData()           # get the lamp data.
				if lamp.type == Lamp.Types["Spot"]:  # Lamp type is not a flag
					lamp.mode &= ~Lamp.Modes["RayShadow"] # Disable RayShadow.
					lamp.mode |= Lamp.Modes["Shadows"]    # Enable Shadowbuffer shadows
"""

def New (type = 'Lamp', name = 'LampData'):
	"""
	Create a new Lamp Data object.
	@type type: string
	@param type: The Lamp type: 'Lamp', 'Sun', 'Spot', 'Hemi', 'Area', or 'Photon'.
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

	@ivar B:  Lamp color blue component.
	Value is clamped to the range [0.0,1.0].
	@type B:  float
	@ivar G:  Lamp color green component.
	Value is clamped to the range [0.0,1.0].
	@type G:  float
	@ivar R:  Lamp color red component.
	Value is clamped to the range [0.0,1.0].
	@type R:  float
	@ivar bias:  Lamp shadow map sampling bias.
	Value is clamped to the range [0.01,5.0].
	@type bias:  float
	@ivar bufferSize:  Lamp shadow buffer size.
	Value is clamped to the range [512,5120].
	@type bufferSize:  int
	@ivar clipEnd:  Lamp shadow map clip end.
	Value is clamped to the range [1.0,5000.0].
	@type clipEnd:  float
	@ivar clipStart:  Lamp shadow map clip start.
	Value is clamped to the range [0.1,1000.0].
	@type clipStart:  float
	@ivar col:  Lamp RGB color triplet.
	Components are clamped to the range [0.0,1.0].
	@type col:  RGB tuple
	@ivar dist:  Lamp clipping distance.
	Value is clamped to the range [0.1,5000.0].
	@type dist:  float
	@ivar energy:  Lamp light intensity.
	Value is clamped to the range [0.0,10.0].
	@type energy:  float
	@ivar haloInt:  Lamp spotlight halo intensity.
	Value is clamped to the range [0.0,5.0].
	@type haloInt:  float
	@ivar haloStep:  Lamp volumetric halo sampling frequency.
	Value is clamped to the range [0,12].
	@type haloStep:  int
	@ivar ipo:  Lamp Ipo.
	Contains the Ipo if one is assigned to the object, B{None} otherwise.  Setting to B{None} clears the current Ipo..
	@type ipo:  Blender Ipo
	@ivar mode:  Lamp mode bitfield.  See L{Modes} for values.
	@type mode:  int
	@ivar quad1:  Quad lamp linear distance attenuation.
	Value is clamped to the range [0.0,1.0].
	@type quad1:  float
	@ivar quad2:  Quad lamp quadratic distance attenuation.
	Value is clamped to the range [0.0,1.0].
	@type quad2:  float
	@ivar samples:  Lamp shadow map samples.
	Value is clamped to the range [1,16].
	@type samples:  int
	@ivar raySamplesX:  Lamp raytracing X samples (X is used for the Y axis with square area lamps).
	Value is clamped to the range [1,16].
	@type raySamplesX:  int
	@ivar raySamplesY:  Lamp raytracing Y samples (Y is only used for rectangle area lamps).
	Value is clamped to the range [1,16].
	@type raySamplesY:  int
	@ivar areaSizeX:  Lamp X size (X is used for the Y axis with square area lamps)
	Value is clamped to the range [0.01,100.0].
	@type areaSizeX:  float
	@ivar areaSizeY:  Lamp Y size (Y is only used for rectangle area lamps).
	Value is clamped to the range [0.01,100.0].
	@type areaSizeY:  float
	@ivar softness:  Lamp shadow sample area size.
	Value is clamped to the range [1.0,100.0].
	@type softness:  float
	@ivar spotBlend:  Lamp spotlight edge softness.
	Value is clamped to the range [0.0,1.0].
	@type spotBlend:  float
	@ivar spotSize:  Lamp spotlight beam angle (in degrees).
	Value is clamped to the range [1.0,180.0].
	@type spotSize:  float
	@ivar type:  Lamp type.  See L{Types} for values.
	@type type:  int
	@ivar falloffType:  Lamp falloff type.  See L{Falloffs} for values.
	@type falloffType:  int

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
		@param type: The Lamp type: 'Lamp', 'Sun', 'Spot', 'Hemi', 'Area', or 'Photon'
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

	def getRaySamplesX():
		"""
		Get this lamp's raytracing sample value on the X axis.
		This value is only used for area lamps.
		@rtype: int
		"""

	def setRaySamplesX():
		"""
		Set the lamp's raytracing sample value on the X axis, between 1 and 16.
		This value is only used for area lamps.
		@rtype: int
		"""

	def getRaySamplesY():
		"""
		Get this lamp's raytracing sample value on the Y axis.
		This value is only used for rectangle area lamps.
		@rtype: int
		"""

	def setRaySamplesY():
		"""
		Set the lamp's raytracing sample value on the Y axis, between 1 and 16.
		This value is only used for rectangle area lamps.
		@rtype: int
		"""

	def getAreaSizeX():
		"""
		Get this lamp's size on the X axis.
		This value is only used for area lamps.
		@rtype: int
		"""

	def setAreaSizeX():
		"""
		Set this lamp's size on the X axis.
		This value is only used for area lamps.
		@rtype: int
		"""

	def getAreaSizeY():
		"""
		Get this lamp's size on the Y axis.
		This value is only used for rectangle area lamps.
		@rtype: int
		"""

	def setAreaSizeY():
		"""
		Set this lamp's size on the Y axis.
		This value is only used for rectangle area lamps.
		@rtype: int
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

	def getScriptLinks (event):
		"""
		Get a list with this Lamp's script links of type 'event'.
		@type event: string
		@param event: "FrameChanged", "Redraw" or "Render".
		@rtype: list
		@return: a list with Blender L{Text} names (the script links of the given
				'event' type) or None if there are no script links at all.
		"""

	def clearScriptLinks (links = None):
		"""
		Delete script links from this Lamp.  If no list is specified, all
		script links are deleted.
		@type links: list of strings
		@param links: None (default) or a list of Blender L{Text} names.
		"""

	def addScriptLink (text, event):
		"""
		Add a new script link to this Lamp.
		@type text: string
		@param text: the name of an existing Blender L{Text}.
		@type event: string
		@param event: "FrameChanged", "Redraw" or "Render".
		"""

	def getIpo():
		"""
		Get the Ipo associated with this Lamp object, if any.
		@rtype: Ipo
		@return: the wrapped ipo or None.
		"""

	def setIpo(ipo):
		"""
		Link an ipo to this Lamp object.
		@type ipo: Blender Ipo
		@param ipo: a "lamp data" ipo.
		"""

	def clearIpo():
		"""
		Unlink the ipo from this Lamp object.
		@return: True if there was an ipo linked or False otherwise.
		"""
		
	def insertIpoKey(keytype):
		"""
		Inserts keytype values in lamp ipo at curframe. Uses module constants.
		@type keytype: Integer
		@param keytype:
			-RGB
			-ENERGY
			-SPOTSIZE
			-OFFSET
			-SIZE
		@return: None
		"""    

	def __copy__ ():
		"""
		Make a copy of this lamp
		@rtype: Lamp
		@return:  a copy of this lamp
		"""

import id_generics
Lamp.__doc__ += id_generics.attributes
