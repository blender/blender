# $Id$
"""
The VideoTexture module allows you to manipulate textures during the game.

Several sources for texture are possible: video files, image files, 
video capture, memory buffer, camera render or a mix of that.

The video and image files can be loaded from the internet using an URL 
instead of a file name.

In addition, you can apply filters on the images before sending them to the GPU, allowing video effect: blue screen, 
color band, gray, normal map.

VideoTexture uses FFmpeg to load images and videos. All the formats and codecs
that FFmpeg supports are supported by VideoTexture, including but not limited to::

	* AVI
	* Ogg
	* Xvid
	* Theora
	* dv1394 camera
	* video4linux capture card (this includes many webcams)
	* videoForWindows capture card (this includes many webcams)
	* JPG 

The principle is simple: first you identify a texture on an existing object using 
the L{materialID} function, then you create a new texture with dynamic content
and swap the two textures in the GPU.

The GE is not aware of the substitution and continues to display the object as always, 
except that you are now in control of the texture.

When the texture object is deleted, the new texture is deleted and the old texture restored. 

Example::
	import VideoTexture
	import GameLogic

	contr = GameLogic.getCurrentController()
	obj = contr.owner
	
	# the creation of the texture must be done once: save the 
	# texture object in an attribute of GameLogic module makes it persistent
	if not hasattr(GameLogic, 'video'):
	
		# identify a static texture by name
		matID = VideoTexture.materialID(obj, 'IMvideo.png')
		
		# create a dynamic texture that will replace the static texture
		GameLogic.video = VideoTexture.Texture(obj, matID)
		
		# define a source of image for the texture, here a movie
		movie = GameLogic.expandPath('//trailer_400p.ogg')
		GameLogic.video.source = VideoTexture.VideoFFmpeg(movie)
		GameLogic.video.source.scale = True
		
		# quick off the movie, but it wont play in the background
		GameLogic.video.source.play()	
		
		# you need to call this function every frame to ensure update of the texture.
		GameLogic.video.refresh(True)
	

"""
def getLastError():
	"""
	Returns the description of the last error that occured in a VideoTexture function.
	
	@rtype: string
	"""
def imageToArray(image,mode):
	"""
	Returns a BGL.buffer corresponding to the current image stored in a texture source object

	@param image: Image source object.
	@type image: object of type L{VideoFFmpeg}, L{ImageFFmpeg}, L{ImageBuff}, L{ImageMix}, L{ImageRender}, L{ImageMirror} or L{ImageViewport}
	@param mode: optional argument representing the pixel format. 
	             You can use the characters R, G, B for the 3 color channels, A for the alpha channel, 
	             0 to force a fixed 0 color channel and 1 to force a fixed 255 color channel.
	             Example: "BGR" will return 3 bytes per pixel with the Blue, Green and Red channels in that order. \
	                      "RGB1" will return 4 bytes per pixel with the Red, Green, Blue channels in that order and the alpha channel forced to 255.
	             The default mode is "RGBA".
	@type mode: string
	@rtype: BGL.buffer
	@returns: object representing the image as one dimensional array of bytes of size (pixel_size*width*height), line by line starting from the bottom of the image. The pixel size and format is determined by the mode parameter.
	"""

def materialID(object,name):
	"""
	Returns a numeric value that can be used in L{Texture} to create a dynamic texture.

	The value corresponds to an internal material number that uses the texture identified
	by name. name is a string representing a texture name with IM prefix if you want to
	identify the texture directly. 	This method works for basic tex face and for material,
	provided the material has a texture channel using that particular texture in first
	position of the texture stack. 	name can also have MA prefix if you want to identify
	the texture by material. In that case the material must have a texture channel in first
	position.
	
	If the object has no material that matches name, it generates a runtime error. Use try/except to catch the exception.
	
	Ex: VideoTexture.materialID(obj, 'IMvideo.png')
	
	@param object: the game object that uses the texture you want to make dynamic
	@type object: game object
	@param name: name of the texture/material you want to make dynamic. 
	@type name: string
	@rtype: integer
	"""
def setLogFile(filename):
	"""
	Sets the name of a text file in which runtime error messages will be written, in addition to the printing
	of the messages on the Python console. Only the runtime errors specific to the VideoTexture module
	are written in that file, ordinary runtime time errors are not written. 

	@param filename: name of error log file
	@type filename: string
	@rtype: integer
	"""
def FilterBGR24():
	"""
	Returns a new input filter object to be used with L{ImageBuff} object when the image passed 
	to the ImageBuff.load() function has the 3-bytes pixel format BGR. 
	
	@rtype: object of type FilterBGR24
	"""
def FilterBlueScreen():
	"""
	Does something
	
	@rtype: 
	"""
def FilterColor():
	"""
	Does something
	
	@rtype: 
	"""
def FilterGray():
	"""
	Does something
	
	@rtype: 
	"""
def FilterLevel():
	"""
	Does something
	
	@rtype: 
	"""
def FilterNormal():
	"""
	Does something
	
	@rtype: 
	"""
def FilterRGB24():
	"""
	Returns a new input filter object to be used with L{ImageBuff} object when the image passed 
	to the ImageBuff.load() function has the 3-bytes pixel format RBG.
	
	@rtype: object of type FilterRGB24
	"""
def FilterRGBA32():
	"""
	Returns a new input filter object to be used with L{ImageBuff} object when the image passed 
	to the ImageBuff.load() function has the 4-bytes pixel format RGBA.
	
	@rtype: object of type FilterRGBA32
	"""
def ImageBuff():
	"""
	Does something
	
	@rtype: 
	"""
def ImageFFmpeg():
	"""
	Does something
	
	@rtype: 
	"""
def ImageMirror():
	"""
	Does something
	
	@rtype: 
	"""
def ImageMix():
	"""
	Does something
	
	@rtype: 
	"""
def ImageRender():
	"""
	Does something
	
	@rtype: 
	"""
def ImageViewport():
	"""
	Does something
	
	@rtype: 
	"""
def Texture():
	"""
	Does something
	
	@rtype: L{Texture}
	"""
def VideoFFmpeg():
	"""
	Does something
	
	@rtype: 
	"""