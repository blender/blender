# Blender.Image module and the Image PyType object

"""
The Blender.Image submodule.

Image
=====

B{New}: L{Image.clampX}, L{Image.clampY}.

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

@type Sources: readonly dictionary
@var Sources: The available Image Source.
		- STILL: Single image file
		- MOVIE: Movie file
		- SEQUENCE: Multiple image files, as sequence
		- GENERATED: Generated image
"""

def Load (filename):
	"""
	Load the image called 'filename' into an Image object.
	@type filename: string
	@param filename: The full path to the image file.
	@rtype:  Blender Image
	@return: A Blender Image object with the data from I{filename}.
	"""

def New (name, width, height, depth):
	"""
	Create a new Image object.
	@type name: string
	@param name: The name of the new Image object.
	@type width: int
	@param width: The width of the new Image object, between 1 and 5000.
	@type height: int
	@param height: The height of the new Image object, between 1 and 5000.
	@type depth: int
	@param depth: The color depth of the new Image object. (32:RGBA 8bit channels, 128:RGBA 32bit high dynamic range float channels).
	@rtype: Blender Image
	@return: A new Blender Image object.
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

def GetCurrent ():
	"""
	Get the currently displayed Image from Blenders UV/Image window.
	When multiple images are displayed, the last active UV/Image windows image is used.
	@rtype: Blender Image
	@return: The Current Blender Image, If there is no current image it returns None.
	"""

from IDProp import IDGroup, IDArray
class Image:
	"""
	The Image object
	================
		This object gives access to Images in Blender.
	@ivar filename: The filename (path) to the image file loaded into this Image
		 object.
	@type filename: string
	@ivar size: The [width, height] dimensions of the image (in pixels).
	@type size: list
	@ivar depth: The pixel depth of the image, read only. [8, 16, 18, 24, 32, 128 (for 32bit float color channels)]
	@type depth: int
	@ivar xrep: Texture tiling: the number of repetitions in the x (horizontal)
		 axis. [1, 16].
	@ivar yrep: Texture tiling: the number of repetitions in the y (vertical)
		 axis [1, 16].
	@type xrep: int 
	@type yrep: int 
	@ivar start: Texture's animation start frame [0, 128].
	@type start: int
	@ivar end: Texture's animation end frame [0, 128].
	@type end: int
	@ivar speed: Texture's animation speed [1, 100].
	@type speed: int
	@ivar packed: True when the Texture is packed (readonly).
	@type packed: boolean
	@ivar has_data: True when the image has pixel data (readonly).
	@type has_data: boolean
	@ivar fields: enable or disable the fields option for this image.
	@type fields: boolean
	@ivar fields_odd: enable or disable the odd fields option for this image.
	@type fields_odd: boolean
	@ivar antialias: enable or disable the antialias option for this image.
	@type antialias: boolean
	@ivar bindcode: Texture's bind code (readonly).
	@type bindcode: int
	@ivar source: Image source type.  See L{the Sources dictionary<Sources>} .
	@type source: int
	@ivar clampX: When true the image will not tile horizontally.
	@type clampX: bool
	@ivar clampY: When true the image will not tile vertically.
	@type clampY: bool
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
		Get the pixel depth of this image. [8,16,24,32,128 for 32bit float images]
		@rtype: int
		"""

	def getPixelHDR(x, y):
		"""
		Get the the colors of the current pixel in the form [r,g,b,a].
		For float image types, returned values can be greater then the useual [0.0, 1.0] range.
		Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
		@returns: [ r, g, b, a]
		@rtype: list of 4 floats
		@type x: int
		@type y: int
		@param x:  the x coordinate of pixel.
		@param y:  the y coordinate of pixel.  
		"""
		
	def getPixelF(x, y):
		"""
		Get the the colors of the current pixel in the form [r,g,b,a].
		Returned values are floats normalized to 0.0 - 1.0.
		Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
		@returns: [ r, g, b, a]
		@rtype: list of 4 floats
		@type x: int
		@type y: int
		@param x:  the x coordinate of pixel.
		@param y:  the y coordinate of pixel.  
		"""
		
	def getPixelI(x, y):
		"""
		Get the the colors of the current pixel in the form [r,g,b,a].
		Returned values are ints normalized to 0 - 255.
		Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
		@returns: [ r, g, b, a]
		@rtype: list of 4 ints
		@type x: int
		@type y: int
		@param x:  the x coordinate of pixel.
		@param y:  the y coordinate of pixel.  
		"""

	def getMaxXY():
		"""
		Get the  x & y size for the image.  Image coordinates range from 0 to size-1.
		@returns: [x, y]
		@rtype: list of 2 ints
		"""

	def getMinXY():
		"""
		Get the x & y origin for the image. Image coordinates range from 0 to size-1.
		@returns: [x, y]
		@rtype: list of 2 ints
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

	def getBindCode():
		"""
		Get the Image's bindcode.  This is for texture loading using BGL calls.
		See, for example, L{BGL.glBindTexture} and L{glLoad}.
		@rtype: int
		"""

	def getStart():
		"""
		Get the Image's start frame. Used for animated textures.
		@rtype: int
		"""

	def getEnd():
		"""
		Get the Image's end frame. Used for animated textures.
		@rtype: int
		"""

	def getSpeed():
		"""
		Get the Image's speed (fps). Used for animated textures.
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

	def updateDisplay():
		"""
		Update the display image from the floating point buffer (if it exists)
		@returns: None
		"""

	def glLoad():
		"""
		Load this image's data into OpenGL texture memory, if it is not already
		loaded (image.bindcode is 0 if it is not loaded yet).
		@note: Usually you don't need to call this method.  It is only necessary
			if you want to draw textured objects in the Scripts window and the
			image's bind code is zero at that moment, otherwise Blender itself can
			take care of binding / unbinding textures.  Calling this method for an
			image with nonzero bind code simply returns the image's bind code value
			(see L{getBindCode}).
		@rtype: int
		@returns: the texture's bind code.
		"""

	def glFree():
		"""
		Delete this image's data from OpenGL texture memory, only (the image itself
		is not removed from Blender's memory).  Internally, glDeleteTextures (see
		L{BGL.glDeleteTextures}) is used, but this method also updates Blender's
		Image object so that its bind code is set to 0.  See also L{Image.glLoad},
		L{Image.getBindCode}.
		"""

	def setName(name):
		"""
		Set the name of this Image object.
		@type name: string
		@param name: The new name.
		"""

	def setFilename(name):
		"""
		Change the filename of this Image object.
		@type name: string
		@param name: The new full filename.
		@warn: use this with caution and note that the filename is truncated if
			larger than 160 characters.
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

	def setStart(start):
		"""
		Get the Image's start frame. Used for animated textures.
		@type start: int
		@param start: The new value in [0, 128].
		"""

	def setEnd(end):
		"""
		Set the Image's end frame. Used for animated textures.
		@type end: int
		@param end: The new value in [0, 128].
		"""

	def setSpeed(speed):
		"""
		Set the Image's speed (fps). Used for animated textures.
		@type speed: int
		@param speed: The new value in [1, 100].
		"""

	def setPixelHDR(x, y, (r, g, b,a )):
		"""
		Set the the colors of the current pixel in the form [r,g,b,a].
		For float image types, returned values can be greater then the useual [0.0, 1.0] range.
		Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
		@type x: int
		@type y: int
		@type r: float
		@type g: float
		@type b: float
		@type a: float
		@returns: nothing
		@rtype: none
		"""
		
	def setPixelF(x, y, (r, g, b,a )):
		"""
		Set the the colors of the current pixel in the form [r,g,b,a].
		Color values must be floats in the range 0.0 - 1.0.
		Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
		@type x: int
		@type y: int
		@type r: float
		@type g: float
		@type b: float
		@type a: float
		@returns: nothing
		@rtype: none
		"""
		
	def setPixelI(x, y, (r, g, b, a)):
		"""
		Set the the colors of the current pixel in the form [r,g,b,a].
		Color values must be ints in the range 0 - 255.
		Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
		@type x: int
		@type y: int
		@type r: int
		@type g: int
		@type b: int
		@type a: int
		@returns: nothing
		@rtype: none
		"""
		
	def save():
		"""
		Saves the current image to L{filename}
		@note: Saving to a directory that doent exist will raise an error.
		@note: Saving a packed image will make a unique (numbered) name if the file alredy exists. Remove the file first to be sure it will not be renamed.
		@returns: None
		"""
	
	def pack():
		"""
		Packs the image into the current blend file.
		
		Since 2.44 new images without valid filenames can be packed.
		
		If the image is alredy packed, it will be repacked.
		
		@returns: nothing
		@rtype: none
		"""

	def unpack(mode):
		"""
		Unpacks the image to the images filename.
		@param mode: One of the values in L{Blender.UnpackModes}.
		@note: An error will be raised if the image is not packed or the filename path does not exist.
		@returns: nothing
		@rtype: none
		@type mode: int
		"""
	def makeCurrent():
		"""
		Set the currently displayed Image from Blenders UV/Image window.
		When multiple images are displayed, the last active UV/Image windows image is used.
		@warn: Deprecated, set bpy.data.images.active = image instead.
		@rtype: bool
		@return: True if the current image could be set, if no window was available, return False.
		"""
import id_generics
Image.__doc__ += id_generics.attributes
