#
# Blender.Texture module and the Texture PyType object
#
# Written by Alex Mole
# 

"""
The Blender.Texture submodule.

B{New}:
	- many new attributes in L{MTex} submodule
	- new dictionaries (L{Texture.BlendModes}, L{Texture.Mappings}, L{Texture.Proj}) to use for the values of some of the new L{MTex} attributes.

Texture
=======

This module provides access to B{Texture} objects in Blender.

Example::
		
	from Blender import Texture,Image,Material
	
	footex = Texture.Get('foo')             # get texture named 'foo'
	footex.setType('Image')                 # make foo be an image texture
	img = Image.Load('test.png')            # load an image
	footex.image = img                      # link the image to the texture

	mat = Material.Get('bar')               # get a material
	mtextures = mat.getTextures()           # get a list of the MTex objects
	for mtex in mtextures:
		if mtex.tex.type == Texture.Types.IMAGE: 
			print mtex.tex.image.filename   # print the filenames of all the
											# images in textures linked to "bar"

	mat.setTexture(0, footex)               # set the material's first texture
											# to be our texture


@type Types: readonly dictionary
@var Types: The available texture types:
		- NONE -  No texture
		- CLOUDS - Clouds texture
		- WOOD - Wood texture
		- MARBLE - Marble texture
		- MAGIC - Magic texture
		- BLEND - Blend texture
		- STUCCI - Stucci texture
		- NOISE - Noise texture
		- IMAGE - Image texture
		- PLUGIN - Plugin texture
		- ENVMAP - EnvMap texture
		- MUSGRAVE - Musgrave procedural texture
		- VORONOI - Voronoi procedural texture
		- DISTNOISE - Distorted noise texture

@type Flags: readonly dictionary
@var Flags: The available Texture flags:
		- FLIPBLEND - Flips the blend texture's X and Y directions
		- NEGALPHA - Reverse the alpha value
		- CHECKER_ODD - Fill the "odd" checkerboard tiles
		- CHECKER_EVEN - Fill the "even" checkerboard tiles
		- COLORBAND - Enable colorband for this texture
        - PREVIEW_ALPHA - Show alpha in preview
        - REPEAT_XMIR - Mirrors X direction repeat
        - REPEAT_YMIR - Mirrors Y direction repeat

@type ImageFlags: readonly dictionary
@var ImageFlags: The available image flags for Texture.imageFlags:
		- INTERPOL - Interpolate pixels of the image
		- USEALPHA - Use the alpha layer
		- MIPMAP - Enable mipmapping [cannot be used with FIELDS]
		- FIELDS - Work with field images [cannot be used with MIPMAP]
		- ROT90 - Rotate the image 90 degrees when rendering
		- CALCALPHA - Calculate an alpha from the RGB
		- STFIELD - Denotes this is a standard field
		- MOVIE - Use a movie for an image
		- CYCLIC - Repeat animation image
		- ANTI - Use anti-aliasing
		- NORMALMAP - Use image RGB values for normal mapping

@type ExtendModes: readonly dictionary
@var ExtendModes: Extend, clip, repeat or checker modes for image textures
		- EXTEND - Extends the colour of the edge
		- CLIP - Return alpha 0.0 outside image
		- CLIPCUBE - Return alpha 0.0 around cube-shaped area around image
		- REPEAT - Repeat image vertically and horizontally
		- CHECKER - Repeat image in checkerboard pattern

@type Noise: readonly dictionary
@var Noise: Noise types and bases.  SINE, SAW and TRI are only used for
		marble and wood textures, while the remainder are used for all textures
		which has a noise basis function (for these textures, the constant should
		be used with the second noise basis setting).
			- SINE - Produce bands using sine wave (marble, wood textures)
			- SAW - Produce bands using saw wave (marble, wood textures)
			- TRI - Produce bands using triangle wave (marble, wood textures)
			- BLENDER - Original Blender algorithm
			- PERLIN - Ken Perlin's original (1985) algorithm
			- IMPROVEDPERLIN - Ken Perlin's newer (2002) algorithm
			- VORONOIF1 - none
			- VORONOIF2 - none
			- VORONOIF3 - none
			- VORONOIF4 - none
			- VORONOIF2F1 - none
			- VORONOICRACKLE - none
			- CELLNOISE - Steven Worley's cellular basis algorithm (1996)

@type BlendModes: readonly dictionary
@var BlendModes: The available texture blending modes:
		- MIX - mix texture with value
		- MULTIPLY - multiply texture with value
		- ADD - add texture to value
		- SUBTRACT - subtract texture from value
		- DIVIDE - divide value by texture
		- DARKEN - replace value with texture if texture is darker
		- DIFFERENCE - difference of texture from value
		- LIGHTEN - replace value with texture if texture is lighter
		- SCREEN - 'screen' mode

@type Mappings: readonly dictionary
@var Mappings: The available 2D texture coordinate mappings for images:
		- FLAT - flat projection
		- CUBE - cube projection
		- TUBE - cylindrical projection
		- SPHERE - spherical projection

@type Proj: readonly dictionary
@var Proj: The available projections per axis:
		- NONE - axis isn't used
		- X - axis is used as final x axis
		- Y - axis is used as final y axis
		- Z - axis is used as final z axis

@type STypes: readonly dictionary
@var STypes: Texture-type specific data. Depending on the value of
		Texture.type, certain groups will make sense. For instance, when a texture 
		is of type CLOUD, the CLD_xxx stypes can be used. Note that the first 
		value in each group is the default.
				1. Clouds type
						- CLD_DEFAULT - Monochromatic noise
						- CLD_COLOR - RGB noise
				2. Wood type
						- WOD_BANDS - Use standard wood texture
						- WOD_RINGS - Use wood rings
						- WOD_BANDNOISE - Add noise to standard wood
						- WOD_RINGNOISE - Add noise to rings
				3. Magic type
						- MAG_DEFAULT - Magic has no STypes
				4. Marble type
						- MBL_SOFT - Use soft marble
						- MBL_SHARP - Use more clearly defined marble
						- MBL_SHARPER - Use very clearly dfefined marble
				5. Blend type
						- BLN_LIN - Use a linear progression
						- BLN_QUAD - Use a quadratic progression
						- BLN_EASE - Uses a more complicated blend function
						- BLN_DIAG - Use a diagonal progression
						- BLN_SPHERE - Use a progression with the shape of a sphere
						- BLN_HALO - Use a quadratic progression with the shape of a sphere
				6. Stucci type
						- STC_PLASTIC - Standard stucci
						- STC_WALLIN - Creates dimples
						- STC_WALLOUT - Creates ridges
				7. Noise type
						- NSE_DEFAULT - Noise has no STypes
				8. Image type
						- IMG_DEFAULT - Image has no STypes
				9. Plugin type
						- PLG_DEFAULT - Plugin has no STypes
				10. Envmap type
						- ENV_STATIC - Calculate map only once
						- ENV_ANIM - Calculate map each rendering
						- ENV_LOAD - Load map from disk
				11. Musgrave type
						- MUS_MFRACTAL - Hetero Multifractal
						- MUS_RIDGEDMF - Ridged Multifractal
						- MUS_HYBRIDMF - Hybrid Multifractal
						- MUS_FBM - Fractal Brownian Motion
						- MUS_HTERRAIN - Hetero Terrain
				12. Voronoi type
						- VN_INT - Only calculate intensity
						- VN_COL1 - Color cells by position
						- VN_COL2 - Same as Col1 plus outline based on F2-F1
						- VN_COL3 - Same as Col2 multiplied by intensity
				13. Distorted noise type
						- DN_BLENDER - Original Blender algorithm
						- DN_PERLIN - Ken Perlin's original (1985) algorithm
						- DN_IMPROVEDPERLIN - Ken Perlin's newer (2002) algorithm
						- DN_VORONOIF1 - none
						- DN_VORONOIF2 - none
						- DN_VORONOIF3 - none
						- DN_VORONOIF4 - none
						- DN_VORONOIF2F1 - none
						- DN_VORONOICRACKLE - none
						- DN_CELLNOISE - Steven Worley's cellular basis algorithm (1996)

@var TexCo: Flags for MTex.texco.
		- ORCO - Use the original coordinates of the mesh
		- REFL - Use reflection vector as texture coordinates
		- NOR - Use normal vector as texture coordinates
		- GLOB - Use global coordinates for the texture coordinates
		- UV - Use UV coordinates for texture coordinates
		- OBJECT - Use linked object's coordinates for texture coordinates
		- WIN - Use screen coordinates as texture coordinates
		- VIEW - Pass camera view vector on to the texture (World texture only!)
		- STICK - Use mesh sticky coordinates for the texture coordinates
		- STRESS - Use mesh stress coordinates for the texture coordinates
		- TANGENT - Use mesh tangent coordinates for the texture coordinates
@type TexCo: readonly dictionary

@var MapTo: Flags for MTex.mapto.
		- COL - Make the texture affect the basic colour of the material
		- NOR - Make the texture affect the rendered normal
		- CSP - Make the texture affect the specularity colour
		- CMIR - Make the texture affect the mirror colour
		- REF - Make the texture affect the diffuse reflectivity value
		- SPEC - Make the texture affect the specularity value
		- HARD - Make the texture affect the hardness value
		- ALPHA - Make the texture affect the alpha value
		- EMIT - Make the texture affect the emit value
		- RAYMIR - Make the texture affect the mirror reflectivity value
		- DISP - Make the texture displace the mesh
		- TRANSLU - Make the texture affect the translucency value
		- AMB - Make the texture affect the ambient value
		- WARP - Make the texture affect texture coordinates for the following textures
@type MapTo: readonly dictionary

"""

def New (name = 'Tex'):
	"""
	Create a new Texture object.
	@type name: string
	@param name: The Texture name.
	@rtype: Blender Texture
	@return: The created Texture object.
	"""

def Get (name = None):
	"""
	Get the Texture object(s) from Blender.
	@type name: string
	@param name: The name of the Texture.
	@rtype: Blender Texture or a list of Blender Textures
	@return: It depends on the I{name} parameter:
			- (name): The Texture object with the given I{name};
			- ():     A list with all Texture objects in the current scene.
	"""

from IDProp import IDGroup, IDArray
class Texture:
	"""
	The Texture object
	==================
		This object gives access to Texture-specific data in Blender.

		Note that many of the attributes of this object are only relevant for
		specific texture types.

	@ivar animFrames:  Number of frames of a movie to use.
	Value is clamped to the range [0,300000].
	@type animFrames:  int
	@ivar animOffset:  Offsets the number of the first movie frame to use.
	Value is clamped to the range [-300000,300000].
	@type animOffset:  int
	@ivar animStart:  Starting frame of the movie to use.
	Value is clamped to the range [1,300000].
	@type animStart:  int
	@ivar anti:  Image anti-aliasing enabled.  Also see L{ImageFlags}.
	@type anti:  int
	@ivar brightness:  Changes the brightness of a texture's color.
	Value is clamped to the range [0.0,2.0].
	@type brightness:  float
	@ivar calcAlpha:  Calculation of image's alpha channel enabled. Also see L{ImageFlags}.
	@type calcAlpha:  int
	@ivar contrast:  Changes the contrast of a texture's color.
	Value is clamped to the range [0.01,5.0].
	@type contrast:  float
	@ivar crop:  Sets the cropping extents (for image textures).
	@type crop:  tuple of 4 ints
	@ivar cyclic:  Looping of animated frames enabled. Also see L{ImageFlags}.
	@type cyclic:  boolean
	@ivar distAmnt:  Amount of distortion (for distorted noise textures).
	Value is clamped to the range [0.0,10.0].
	@type distAmnt:  float
	@ivar distMetric:  The distance metric (for Voronoi textures).
	@type distMetric:  int
	@ivar exp:  Minkovsky exponent (for Minkovsky Voronoi textures).
	Value is clamped to the range [0.01,10.0].
	@type exp:  float
	@ivar extend:  Texture's 'Extend' mode (for image textures). See L{ExtendModes}.
	@type extend:  int
	@ivar fields:  Use of image's fields enabled. Also see L{ImageFlags}.
	@type fields:  int
	@ivar fieldsPerImage:  Number of fields per rendered frame.
	Value is clamped to the range [1,200].
	@type fieldsPerImage:  int
	@ivar filterSize:  The filter size (for image and envmap textures).
	Value is clamped to the range [0.1,25.0].
	@type filterSize:  float
	@ivar flags:  Texture's 'Flag' bitfield.  See L{Flags}.
	bitmask.
	@type flags:  int
	@ivar hFracDim:  Highest fractional dimension (for Musgrave textures).
	Value is clamped to the range [0.0001,2.0].
	@type hFracDim:  float
	@ivar iScale:  Intensity output scale (for Musgrave and Voronoi textures).
	Value is clamped to the range [0.0,10.0].
	@type iScale:  float
	@ivar image:  Texture's image object.
	@type image:  Blender Image (or None)
	@ivar imageFlags:  Texture's 'ImageFlags' bits.
	@type imageFlags:  int
	@ivar interpol:  Interpolate image's pixels to fit texture mapping enabled. Also see L{ImageFlags}.
	@type interpol:  int
	@ivar ipo:  Texture Ipo data.
	Contains the Ipo if one is assigned to the object, B{None} otherwise.  Setting to B{None} clears the current Ipo..
	@type ipo:  Blender Ipo
	@ivar lacunarity:  Gap between succesive frequencies (for Musgrave textures).
	Value is clamped to the range [0.0,6.0].
	@type lacunarity:  float
	@ivar mipmap:  Mipmaps enabled. Also see L{ImageFlags}.
	@type mipmap:  int
	@ivar movie:  Movie frames as images enabled. Also see L{ImageFlags}.
	@type movie:  int
	@ivar noiseBasis:  Noise basis type (wood, stucci, marble, clouds,
	Musgrave, distorted).  See L{Noise} dictionary.
	@type noiseBasis:  int
	@ivar noiseBasis2:  Additional noise basis type (wood, marble, distorted
	noise).  See L{Noise} dictionary.
	@type noiseBasis2:  int
	@ivar noiseDepth:  Noise depth (magic, marble, clouds).
	Value is clamped to the range [0,6].
	@type noiseDepth:  int
	@ivar noiseSize:  Noise size (wood, stucci, marble, clouds, Musgrave,
	distorted noise).
	Value is clamped to the range [0.0001,2.0].
	@type noiseSize:  float
	@ivar noiseType:  Noise type (for wood, stucci, marble, clouds textures).		Valid values are 'hard' or 'soft'.
	@type noiseType:  string 
	@ivar normalMap:  Use of image RGB values for normal mapping enabled. 
	Also see L{ImageFlags}.
	@type normalMap:  int
	@ivar octs:  Number of frequencies (for Musgrave textures).
	Value is clamped to the range [0.0,8.0].
	@type octs:  float
	@ivar repeat:  Repetition multiplier (for image textures).
	@type repeat:  tuple of 2 ints
	@ivar rgbCol:  RGB color tuple.
	@type rgbCol:  tuple of 3 floats
	@ivar rot90:  X/Y flip for rendering enabled. Also see L{ImageFlags}.
	@type rot90:  int
	@ivar saw:  Produce bands using saw wave (marble, wood textures). Also see L{Noise}.
	@type saw:  int
	@ivar sine:  Produce bands using sine wave (marble, wood textures). Also see L{Noise}.
	@type sine:  int
	@ivar stField:  Standard field deinterlacing enabled. Also see L{ImageFlags}.
	@type stField:  int
	@ivar stype:  Texture's 'SType' mode.  See L{STypes}.
	@type stype:  int
	@ivar tri:  Produce bands using triangle wave (marble, wood textures). Also see L{Noise}.
	@type tri:  int
	@ivar turbulence:  Turbulence (for magic, wood, stucci, marble textures).
	Value is clamped to the range [0.0,200.0].
	@type turbulence:  float
	@ivar type:  Texture's 'Type' mode. See L{Types}.
	Value must be in the range [0,13].
	@type type:  int
	@ivar useAlpha:  Use of image's alpha channel enabled. Also see L{ImageFlags}.
	@type useAlpha:  int
	@ivar weight1:  Weight 1 (for Voronoi textures).
	Value is clamped to the range [-2.0,2.0].
	@type weight1:  float
	@ivar weight2:  Weight 2 (for Voronoi textures).
	Value is clamped to the range [-2.0,2.0].
	@type weight2:  float
	@ivar weight3:  Weight 3 (for Voronoi textures).
	Value is clamped to the range [-2.0,2.0].
	@type weight3:  float
	@ivar weight4:  Weight 4 (for Voronoi textures).
	Value is clamped to the range [-2.0,2.0].
	@type weight4:  float
	@ivar colorband:  Texture colorband, a list of colors, 
	each color a list of 5 floats [0 - 1], [r,g,b,a,pos].
	The colorband can have between 1 and 31 colors.
	@type colorband:  list
	@ivar autoRefresh: Refresh image on frame changes enabled.
	@type autoRefresh:  boolean
	"""
	
	def getExtend():
		"""
		Get the extend mode of the texture. See L{setExtend}.
		@rtype: string.
		"""
	
	def getImage():
		"""
		Get the Image associated with this texture (or None).
		@rtype: Blender Image
		"""

	def getName():
		"""
		Get the name of this Texture object.
		@rtype: string
		"""

	def getType():
		"""
		Get this Texture's type.  See L{setType}.
		@rtype: string
		"""

	def setExtend(extendmode):
		"""
		Set the extend mode of this texture (only used for IMAGE textures)
		@param extendmode: The new extend mode. One of: 
				'Extend', 'Clip', 'ClipCube' and 'Repeat'
		@type extendmode: string
		"""

	def setFlags(f1=None, f2=None, f3=None, f4=None):
		"""
		Set this object's flags.
		@param f1,f2,f3,f4: Flags to be set (omitted flags are cleared). Can be any of 
				'FlipBlendXY', 'NegAlpha', 'CheckerOdd', and 'CheckerEven'
		@type f1,f2,f3,f4: string
		"""

	def setImage(image):
		"""
		Set the Image of this texture.
		@param image: The new Image.
		@type image: Blender Image or None.
		@warning: This sets the texture's type to 'Image' if it is not already.
		"""

	def setImageFlags(f1=None, f2=None, f3=None, etc=None):
		"""
		Set the Image flags (only makes sense for IMAGE textures). Omitted
		flags are cleared.
		@param f1, f2, f3, etc: Flag to set. See L{ImageFlags} for their meanings. Can be 
				any of: 'InterPol', 'UseAlpha', 'MipMap', 'Fields', 'Rot90',
				'CalcAlpha', 'Cyclic', 'Movie', 'StField', 'Anti' and 'NormalMap'
		@type f1, f2, f3, etc: string
		"""

	def setName(name):
		"""
		Set the name of this Texture object.
		@param name: The new name.
		@type name: string
		"""

	def setSType(stype):
		"""
		Set the SType.
		@param stype: The new stype. This can be any of the values listed in
				 L{STypes} or 'Default' which sets the stype to the default value.
		@type stype: string

		@note: the set of valid parameters is dependent on the current
		texture type.  Be sure to always set the texture type B{before}
		setting the texture's stype; otherwise an exception might occur.
		"""

	def setType(type):
		"""
		Set this Texture's type.
		@param type: The new type. Possible options are: 
				'None', 'Clouds', 'Wood', 'Marble', 'Magic', 'Blend', 'Stucci', 
				'Noise', 'Image', 'Plugin', 'EnvMap', 'Musgrave', 'Voronoi'
				and 'DistNoise'
		@type type: string
		"""
	def evaluate(coord):
		"""
		Evaluates the texture at this location and returns the result.
		
		The return value is a 4D vector where (x,y,z,w) are (red, green, blue, intensity)
		For greyscale textures, often intensity only will be used.
		@type coord: vector or tuple of 3 numbers
		"""
		
import id_generics
Texture.__doc__ += id_generics.attributes

class MTex:
	"""
	The MTex Object
	===============

	This object links a material to a texture. It allows the same texture to be
	used in several different ways.

	@ivar tex: The Texture this is linked to.
	@type tex: Blender Texture
	@ivar texco: Texture coordinates ("Map input"). See L{TexCo}
	@ivar mapto: "Map to" field of texture. OR'd values of L{MapTo}
	@ivar object: Object whose space to use when texco is Object
	@type object: Blender Object
	@ivar col: Color that the texture blends with
	@ivar dvar: Value that the texture blends with when not blending colors
	@ivar blendmode: Texture blending mode. L{BlendModes}
	@ivar colfac: Factor by which texture affects color
	@ivar norfac: Factor by which texture affects normal
	@ivar varfac: Factor by which texture affects most variables
	@ivar dispfac: Factor by which texture affects displacement
	@ivar warpfac: Factor by which texture affects warp
	@ivar ofs: Offset to adjust texture space
	@ivar size: Size to scale texture space
	@ivar mapping: Mapping of texture coordinates (flat, cube, etc.). L{Mappings}
	@ivar stencil: Stencil mode
	@ivar neg: Negate texture values mode
	@ivar noRGB: Convert texture RGB values to intensity values
	@ivar correctNor: Correct normal mapping for Texture space and Object space
	@ivar fromDupli: Dupli's instanced from verts, faces or particles, inherit texture coordinate from their parent
	@ivar fromOrig: Dupli's derive their object coordinates from the original objects transformation
	@ivar xproj: Projection of X axis to Texture space. L{Proj}
	@ivar yproj: Projection of Y axis to Texture space. L{Proj}
	@ivar zproj: Projection of Z axis to Texture space. L{Proj}
	@ivar mtCol: How texture maps to color
	@ivar mtNor: How texture maps to normals
	@ivar mtCsp: How texture maps to specularity color
	@ivar mtCmir: How texture maps to mirror color
	@ivar mtRef: How texture maps to reflectivity
	@ivar mtSpec: How texture maps to specularity
	@ivar mtEmit: How texture maps to emit value
	@ivar mtAlpha: How texture maps to alpha value
	@ivar mtHard: How texture maps to hardness
	@ivar mtRayMir: How texture maps to RayMir value
	@ivar mtTranslu: How texture maps to translucency
	@ivar mtAmb: How texture maps to ambient value
	@ivar mtDisp: How texture maps to displacement
	@ivar mtWarp: How texture maps to warp
	@ivar uvlayer: The name of the UV Layer this texture is mapped to (when left blank uses render layer)
	@type uvlayer: string
	"""

	def getIpo():
		"""
		Get the Ipo associated with this texture object, if any.
		@rtype: Ipo
		@return: the wrapped ipo or None.
		"""

	def setIpo(ipo):
		"""
		Link an ipo to this texture object.
		@type ipo: Blender Ipo
		@param ipo: a "texture data" ipo.
		"""

	def clearIpo():
		"""
		Unlink the ipo from this texture object.
		@return: True if there was an ipo linked or False otherwise.
		"""
