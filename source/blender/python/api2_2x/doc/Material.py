# Blender.Material module and the Material PyObject

"""
The Blender.Material submodule.

B{New}: access to shader data.

Material 
========

This module provides access to B{Material} objects in Blender.

Example::
	import Blender
	from Blender import Material
	mat = Material.New('newMat')          # create a new Material called 'newMat'
	print mat.rgbCol                      # print its rgb color triplet sequence
	mat.rgbCol = [0.8, 0.2, 0.2]          # change its color
	mat.setAlpha(0.2)                     # mat.alpha = 0.2 -- almost transparent
	mat.emit = 0.7                        # equivalent to mat.setEmit(0.8)
	mat.mode |= Material.Modes.ZTRANSP    # turn on Z-Buffer transparency
	mat.setName('RedBansheeSkin')         # change its name
	mat.setAdd(0.8)                       # make it glow
	mat.setMode('Halo')                   # turn 'Halo' "on" and all others "off"

@type Modes: readonly dictionary
@var Modes: The available Material Modes.

	B{Note}: Some Modes are only available when the 'Halo' mode is I{off} and
	others only when it is I{on}.  But these two subsets of modes share the same
	numerical values in their Blender C #defines. So, for example, if 'Halo' is
	on, then 'NoMist' is actually interpreted as 'HaloShaded'.  We marked all
	such possibilities in the Modes dict below: each halo-related mode that
	uses an already taken value is preceded by "+" and appear below the normal
	mode which also uses that value.

		- TRACEABLE    - Make Material visible for shadow lamps.
		- SHADOW       - Enable Material for shadows.
		- SHADOWBUF    - Enable Material to cast shadows with shadow buffers.
		- SHADELESS    - Make Material insensitive to light or shadow.
		- WIRE         - Render only the edges of faces.
		- VCOL_LIGHT   - Add vertex colors as extra light.
		- VCOL_PAINT   - Replace basic colors with vertex colors.
		- HALO         - Render as a halo.
		- ZTRANSP      - Z-buffer transparent faces.
		- ZINVERT      - Render with inverted Z-buffer.
		- + HALORINGS  - Render rings over the basic halo.
		- ENV          - Do not render Material.
		- + HALOLINES  - Render star shaped lines over the basic halo.
		- ONLYSHADOW   - Let alpha be determined on the degree of shadow.
		- + HALOXALPHA - Use extreme alpha.
		- TEXFACE      - UV-Editor assigned texture gives color and texture info for faces.
		- TEXFACE_ALPHA - When TEXFACE is enabled, use the alpha as well.
		- + HALOSTAR   - Render halo as a star.
		- NOMIST       - Set the Material insensitive to mist.
		- + HALOSHADED - Let halo receive light.
		- HALOTEX      - Give halo a texture.
		- HALOPUNO     - Use the vertex normal to specify the dimension of the halo.
		- HALOFLARE    - Render halo as a lens flare.
		- RAYMIRROR    - Enables raytracing for mirror reflection rendering.
		- RAYTRANSP    - Enables raytracing for transparency rendering.
		- RAYBIAS      - Prevent ray traced shadow errors with Phong interpolated normals.
		- RAMPCOL      - Status of colorband ramp for Material's diffuse color.  This is a read-only bit.
		- RAMPSPEC     - Status of colorband ramp for Material's specular color.  This is a read-only bit.
		- TANGENTSTR   - Uses direction of strands as normal for tangent-shading.
		- TRANSPSHADOW - Lets Material receive transparent shadows based on material color and alpha.
		- FULLOSA      - Force rendering of all OSA samples.
		- TANGENT_V    - Use the tangent vector in V direction for shading
		- NMAP_TS      - Tangent space normal mapping.
		- GROUP_EXCLUSIVE	- Light from this group even if the lights are on a hidden Layer.

@type Shaders: readonly dictionary
@var Shaders: The available Material Shaders.
		- DIFFUSE_LAMBERT    - Make Material use the lambert diffuse shader.
		- DIFFUSE_ORENNAYAR       - Make Material use the Oren-Nayer diffuse shader.
		- DIFFUSE_TOON    - Make Material use the toon diffuse shader.
		- DIFFUSE_MINNAERT  - Make Material use the minnaert diffuse shader.
		- SPEC_COOKTORR   - Make Material use the Cook-Torr specular shader.
		- SPEC_PHONG   - Make Material use the Phong specular shader.
		- SPEC_BLINN         - Make Material use the Blinn specular shader.
		- SPEC_TOON      - Make Material use the toon specular shader.
		- SPEC_WARDISO      - Make Material use the Ward-iso specular shader.
"""

def New (name = 'Mat'):
	"""
	Create a new Material object.
	@type name: string
	@param name: The Material name.
	@rtype: Blender Material
	@return: The created Material object.
	"""

def Get (name = None):
	"""
	Get the Material object(s) from Blender.
	@type name: string
	@param name: The name of the Material.
	@rtype: Blender Material or a list of Blender Materials
	@return: It depends on the 'name' parameter:
		- (name): The Material object with the given name;
		- ():   A list with all Material objects in the current scene.
	"""

class Material:
	"""
	The Material object
	===================
	 This object gives access to Materials in Blender.
	@ivar B:  Diffuse color (L{rgbCol}) blue component.
	Value is clamped to the range [0.0,1.0].
	@type B:  float
	@ivar G:  Diffuse color (L{rgbCol}) green component.
	Value is clamped to the range [0.0,1.0].
	@type G:  float
	@ivar IOR:  Angular index of refraction for raytrace.
	Value is clamped to the range [1.0,3.0].
	@type IOR:  float
	@ivar R:  Diffuse color (L{rgbCol}) red component.
	Value is clamped to the range [0.0,1.0].
	@type R:  float
	@ivar add:  Strength of the add effect.
	Value is clamped to the range [0.0,1.0].
	@type add:  float
	@ivar alpha:  Alpha (translucency) component of the material.
	Value is clamped to the range [0.0,1.0].
	@type alpha:  float
	@ivar shadAlpha:  Shadow Alpha for irregular shadow buffer.
	Value is clamped to the range [0.0,1.0].
	@type shadAlpha:  float
	@ivar amb:  Amount of global ambient color material receives.
	Value is clamped to the range [0.0,1.0].
	@type amb:  float
	@ivar diffuseDarkness:  Material's diffuse darkness ("Minnaert" diffuse shader only).
	Value is clamped to the range [0.0,2.0].
	@type diffuseDarkness:  float
	@ivar diffuseShader:  Diffuse shader type (see L{Shaders}).
	Value must be in the range [0,3].
	@type diffuseShader:  int
	@ivar diffuseSize:  Material's diffuse area size ("Toon" diffuse shader only).
	Value is clamped to the range [0.0,3.14].
	@type diffuseSize:  float
	@ivar diffuseSmooth:  Material's diffuse area smoothing ("Toon" diffuse shader only).
	Value is clamped to the range [0.0,1.0].
	@type diffuseSmooth:  float
	@ivar emit:  Amount of light the material emits.
	Value is clamped to the range [0.0,1.0].
	@type emit:  float
	@ivar filter:  Amount of filtering when transparent raytrace is enabled.
	Value is clamped to the range [0.0,1.0].
	@type filter:  float
	@ivar flareBoost:  Flare's extra strength.
	Value is clamped to the range [0.1,1.0].
	@type flareBoost:  float
	@ivar flareSeed:  Offset in the flare seed table.
	Value is clamped to the range [1,255].
	@type flareSeed:  int
	@ivar flareSize:  Ratio of flare size to halo size.
	Value is clamped to the range [0.1,25.0].
	@type flareSize:  float
	@ivar fresnelDepth:  Power of Fresnel for mirror reflection.
	Value is clamped to the range [0.0,5.0].
	@type fresnelDepth:  float
	@ivar fresnelDepthFac:  Blending factor for Fresnel mirror.
	Value is clamped to the range [1.0,5.0].
	@type fresnelDepthFac:  float
	@ivar fresnelTrans:  Power of Fresnel for transparency.
	Value is clamped to the range [0.0,5.0].
	@type fresnelTrans:  float
	@ivar fresnelTransFac:  Blending factor for Fresnel transparency.
	Value is clamped to the range [1.0,5.0].
	@type fresnelTransFac:  float
	@ivar rbFriction:  Rigid Body Friction coefficient.
	Value is clamped to the range [0.0,100.0].
	@type rbFriction:  float
	@ivar rbRestitution:  Rigid Body Friction restitution.
	Value is clamped to the range [0.0,1.0].
	@type rbRestitution:  float
	@ivar haloSeed:  Randomizes halo ring dimension and line location.
	Value is clamped to the range [1,255].
	@type haloSeed:  int
	@ivar haloSize:  Dimension of the halo.
	Value is clamped to the range [0.0,100.0].
	@type haloSize:  float
	@ivar hard:  Hardness of the specularity.
	Value is clamped to the range [1,255].
	@type hard:  int
	@ivar ipo:  Material Ipo data.
	Contains the Ipo if one is assigned to the object, None otherwise.  Setting to None clears the current Ipo.
	@type ipo:  Blender Ipo
	
	@ivar mirCol:  Mirror RGB color triplet.
	Components are clamped to the range [0.0,1.0].
	@type mirCol:  list of 3 floats
	@ivar mirB:  Mirror color (L{mirCol}) blue component.
	Value is clamped to the range [0.0,1.0].
	@type mirB:  float
	@ivar mirG:  Mirror color (L{mirCol}) green component.
	Value is clamped to the range [0.0,1.0].
	@type mirG:  float
	@ivar mirR:  Mirror color (L{mirCol}) red component.
	Value is clamped to the range [0.0,1.0].
	@type mirR:  float
	
	@ivar sssCol:  SubSsurface scattering RGB color triplet.
	Components are clamped to the range [0.0,1.0].
	@type sssCol:  list of 3 floats
	@ivar sssB:  SubSsurface scattering color (L{sssCol}) blue component.
	Value is clamped to the range [0.0,1.0].
	@type sssB:  float
	@ivar sssG:  SubSsurface scattering color (L{sssCol}) green component.
	Value is clamped to the range [0.0,1.0].
	@type sssG:  float
	@ivar sssR:  SubSsurface scattering color (L{sssCol}) red component.
	Value is clamped to the range [0.0,1.0].
	@type sssR:  float
	
	@ivar mode:  Mode mode bitfield.  See L{the Modes dictionary<Modes>} keys and descriptions.
	@type mode:  int
	@ivar nFlares:  Number of subflares with halo.
	Value is clamped to the range [1,32].
	@type nFlares:  int
	@ivar nLines:  Number of star-shaped lines with halo.
	Value is clamped to the range [0,250].
	@type nLines:  int
	@ivar nRings:  Number of rings with halo.
	Value is clamped to the range [0,24].
	@type nRings:  int
	@ivar nStars:  Number of star points with halo.
	Value is clamped to the range [3,50].
	@type nStars:  int
	@ivar oopsLoc: Material OOPs location.  Returns None if material not found in list.
	@type oopsLoc:  list of 2 floats
	@ivar oopsSel:  Material OOPs selection flag.
	Value must be in the range [0,1].
	@type oopsSel:  int
	@ivar rayMirr:  Mirror reflection amount for raytrace.
	Value is clamped to the range [0.0,1.0].
	@type rayMirr:  float
	@ivar glossMir: Amount of reflection glossy.
	Value is clamped to the range [0.0,1.0].
	@type glossMir: float
	@ivar sampGloss_mir: Reflection glossy samples.
	Value is clamped to the range [1,1024].
	@type sampGloss_mir: int
	@ivar glossTra: Amount of refraction glossy.
	Value is clamped to the range [0.0,1.0].
	@type glossTra: float
	@ivar sampGlossTra: Refraction glossy samples.
	Value is clamped to the range [1,1024].
	@type sampGlossTra: int
	@ivar rayMirrDepth:  Amount of raytrace inter-reflections.
	Value is clamped to the range [0,10].
	@type rayMirrDepth:  int
	@ivar ref:   Amount of reflections (for shader).
	Value is clamped to the range [0.0,1.0].
	@type ref:  float
	@ivar refracIndex:  Material's Index of Refraction (applies to the "Blinn" Specular Shader only.
	Value is clamped to the range [1.0,10.0].
	@type refracIndex:  float
	@ivar rgbCol:  Diffuse RGB color triplet.
	Components are clamped to the range [0.0,1.0].
	@type rgbCol:  list of 3 floats
	@ivar rms: Material's surface slope standard deviation ("WardIso" specular shader only).
	Value is clamped to the range [0.0,0.4].
	@type rms:  float
	@ivar roughness:  Material's roughness ("Oren Nayar" diffuse shader only).
	Value is clamped to the range [0.0,3.14].
	@type roughness:  float
	@ivar spec:  Degree of specularity.
	Value is clamped to the range [0.0,2.0].
	@type spec:  float
	@ivar specB:  Specular color (L{specCol}) blue component.
	Value is clamped to the range [0.0,1.0].
	@type specB:  float
	@ivar specCol:  Specular RGB color triplet.
	Components are clamped to the range [0.0,1.0].
	@type specCol:  list of 3 floats
	@ivar specG:  Specular color (L{specCol}) green component.
	Value is clamped to the range [0.0,1.0].
	@type specG:  float
	@ivar specR:  Specular color (L{specCol}) red component.
	Value is clamped to the range [0.0,1.0].
	@type specR:  float
	@ivar specShader: Specular shader type.  See L{Shaders}.
	Value must be in the range [0,4].
	@type specShader:  int
	@ivar specSize:  Material's specular area size ("Toon" specular shader only).
	Value is clamped to the range [0.0,1.53].
	@type specSize:  float
	@ivar specSmooth:  Sets the smoothness of specular toon area.
	Value is clamped to the range [0.0,1.0].
	@type specSmooth:  float
	@ivar specTransp:  Makes specular areas opaque on transparent materials.
	Value is clamped to the range [0.0,1.0].
	@type specTransp:  float
	@ivar subSize:   Dimension of subflares, dots and circles.
	Value is clamped to the range [0.1,25.0].
	@type subSize:  float
	@ivar transDepth:  calculated maximal.  Amount of refractions for raytrace.
	Value is clamped to the range [0,10].
	@type transDepth:  int
	@ivar translucency:  Amount of diffuse shading of the back side.
	Value is clamped to the range [0.0,1.0].
	@type translucency:  float
	@ivar zOffset:  Artificial offset in the Z buffer (for Ztransp option).
	Value is clamped to the range [0.0,10.0].
	@type zOffset:  float
	@ivar lightGroup:  Limits lights that affect this material to a group.
	@type lightGroup:  Group or None
	@ivar uvlayer:  The uv layer name to use, when UV mapping is enabled.
	@type uvlayer:  string
	@ivar colorband:  Material colorband, a list of colors, 
	each color a list of 5 floats [0 - 1], [r,g,b,a,pos].
	The colorband can have between 1 and 31 colors.
	@type colorband:  list
	
	@ivar colorbandDiffuse:  Material colorband, a list of colors, 
	each color a list of 5 floats [0 - 1], [r,g,b,a,pos].
	The colorband can have between 1 and 31 colors.
	@type colorbandDiffuse:  list
	@ivar colorbandSpecular:  Material colorband, a list of colors, 
	each color a list of 5 floats [0 - 1], [r,g,b,a,pos].
	The colorband can have between 1 and 31 colors.
	@type colorbandSpecular:  list
	
	@ivar enableSSS:  If True, subsurface scattering will be rendered on this material.
	@type enableSSS:  bool
	@ivar sssScale:  If True, subsurface scattering will be rendered on this material.
	Value is clamped to the range [0.1,1000.0].
	@type sssScale:  bool
	@ivar sssRadiusRed:  Mean red scattering path length.
	Value is clamped to the range [0.0,10000.0].
	@type sssRadiusRed:  float
	@ivar sssRadiusGreen:  Mean green scattering path length.
	Value is clamped to the range [0.0,10000.0].
	@type sssRadiusGreen:  float
	@ivar sssRadiusBlue:  Mean blue scattering path length.
	Value is clamped to the range [0.0,10000.0].
	@type sssRadiusBlue:  float
	@ivar sssIOR:  Refraction index.
	Value is clamped to the range [0.1,2.0].
	@type sssIOR:  float
	@ivar sssError:  Error allowance for the calculation (a low value is slower).
	Value is clamped to the range [0.0,10.0].
	@type sssError:  float
	@ivar sssColorBlend:  Blend factor for SSS colors.
	Value is clamped to the range [0.0,1.0].
	@type sssColorBlend:  float
	@ivar sssTextureScatter:  Texture scattering factor.
	Value is clamped to the range [0.0,1.0].
	@type sssTextureScatter:  float
	@ivar sssFront:  Front scattering weight.
	Value is clamped to the range [0.0,2.0].
	@type sssFront:  float
	@ivar sssBack:  Back scattering weight
	Value is clamped to the range [0.0,10.0].
	@type sssBack:  float

	@warning: Most member variables assume values in some [Min, Max] interval.
		When trying to set them, the given parameter will be clamped to lie in
		that range: if val < Min, then val = Min, if val > Max, then val = Max.

	"""

	def getName():
		"""
		Get the name of this Material object.
		@rtype: string
		"""

	def setName(name):
		"""
		Set the name of this Material object.
		@type name: string
		@param name: The new name.
		"""

	def getIpo():
		"""
		Get the Ipo associated with this material, if any.
		@rtype: Ipo
		@return: the wrapped ipo or None.
		"""

	def setIpo(ipo):
		"""
		Link an ipo to this material.
		@type ipo: Blender Ipo
		@param ipo: a material type ipo.
		"""

	def clearIpo():
		"""
		Unlink the ipo from this material.
		@return: True if there was an ipo linked or False otherwise.
		"""

	def insertIpoKey(keytype):
		"""
		Inserts keytype values in material ipo at curframe. Uses module constants.
		@type keytype: Integer
		@param keytype:
					 -RGB
					 -ALPHA
					 -HALOSIZE
					 -MODE
					 -ALLCOLOR
					 -ALLMIRROR
					 -OFS
					 -SIZE
					 -ALLMAPPING
		@return: py_none
		"""

	def getMode():
		"""
		Get this Material's mode flags.
		@rtype: int
		@return: B{OR'ed value}. Use the Modes dictionary to check which flags
				are 'on'.

				Example::
					import Blender
					from Blender import Material
					flags = mymat.getMode()
					if flags & Material.Modes['HALO']:
						print "This material is rendered as a halo"
					else:
						print "Not a halo"
		"""

	def setMode(param, stringN=None):
		"""
		Set this Material's mode flags. Up to 22 mode strings can be given
		and specify the modes which are turned 'on'.  Those not provided are 
		turned 'off', so mat.setMode() -- without arguments -- turns off all 
		mode flags for Material mat.  Valid mode strings are "Traceable", 
		"Shadow", "Shadeless", "Wire", "VColLight", "VColPaint", "Halo",
		"ZTransp", "ZInvert", "HaloRings", "HaloLines", "OnlyShadow",
		"HaloXAlpha", "HaloStar", "TexFace", "HaloTex", "HaloPuno", "NoMist",
		"HaloShaded", "HaloFlare", "Radio", "RayMirr", "ZTransp", "RayTransp",
		"Env"

		An integer can also be given, which directly sets the mode flag.  The
		Modes dictionary keys can (and should) be added or ORed to specify
		which modes to turn 'on'.  The value returned from getMode() can
		also be modified and input to this method.

		@type param: string, None or int
		@param param: A mode value (int) or flag (string).  Can also be None.
		@type stringN: string
		@param stringN: A mode flag. Up to 22 flags can be set at the same time.
		"""

	def getRGBCol():
		"""
		Get the rgb color triplet sequence.
		@rtype: list of 3 floats
		@return: [r, g, b]
		"""

	def setRGBCol(rgb = None):
		"""
		Set the rgb color triplet sequence.  If B{rgb} is None, set the color to black.
		@type rgb: three floats or a list of three floats
		@param rgb: The rgb color values in [0.0, 1.0] as:
				- a list of three floats: setRGBCol ([r, g, b]) B{or}
				- three floats as separate parameters: setRGBCol (r,g,b).
		"""
 
	def getSpecCol():
		"""
		Get the specular color triplet sequence.
		@rtype: list of 3 floats
		@return: [specR, specG, specB]
		"""

	def setSpecCol(rgb = None):
		"""
		Set the specular color triplet sequence.  If B{rgb} is None, set the color to black.
		@type rgb: three floats or a list of three floats
		@param rgb: The rgb color values in [0.0, 1.0] as:
				- a list of three floats: setSpecCol ([r, g, b]) B{or}
				- three floats as separate parameters: setSpecCol (r,g,b).
		"""

	def getMirCol():
		"""
		Get the mirror color triplet sequence.
		@rtype: list of 3 floats
		@return: [mirR, mirG, mirb]
		"""

	def setMirCol(rgb = None):
		"""
		Set the mirror color triplet sequence.  If B{rgb} is None, set the color to black.
		@type rgb: three floats or a list of three floats
		@param rgb: The rgb color values in [0.0, 1.0] as:
				- a list of three floats: setMirCol ([r, g, b]) B{or}
				- three floats as separate parameters: setMirCol (r,g,b).
		"""

	def getAlpha():
		"""
		Get the alpha (transparency) value.
		@rtype: float
		"""

	def setAlpha(alpha):
		"""
		Set the alpha (transparency) value.
		@type alpha: float
		@param alpha: The new value in [0.0, 1.0].
		"""

	def getAmb():
		"""
		Get the ambient color blend factor.
		@rtype: float
		"""

	def setAmb(amb):
		"""
		Set the ambient color blend factor.
		@type amb: float
		@param amb:  The new value in [0.0, 1.0].
		"""

	def getEmit():
		"""
		Get the emitting light intensity.
		@rtype: float
		"""

	def setEmit(emit):
		"""
		Set the emitting light intensity.
		@type emit: float
		@param emit: The new value in [0.0, 1.0].
		"""

	def getRef():
		"""
		Get the reflectivity value.
		@rtype: float
		"""

	def setRef(ref):
		"""
		Set the reflectivity value.
		@type ref: float
		@param ref: The new value in [0.0, 1.0].
		"""

	def getSpec():
		"""
		Get the specularity value.
		@rtype: float
		"""

	def setSpec(spec):
		"""
		Set the specularity value.
		@type spec: float
		@param spec: The new value in [0.0, 2.0].
		"""

	def getSpecTransp():
		"""
		Get the specular transparency.
		@rtype: float
		"""

	def setSpecTransp(spectransp):
		"""
		Set the specular transparency.
		@type spectransp: float
		@param spectransp: The new value in [0.0, 1.0].
		"""

	def setSpecShader(specShader):
		"""
		Set the material's specular shader from one of the shaders in Material.Shaders dict.
		@type specShader: int
		@param specShader: The new value in [0, 4].
		"""

	def getSpecShader(specShader):
		"""
		Get the material's specular shader from one of the shaders in Material.Shaders dict.
		@rtype: int
		"""

	def setDiffuseShader(diffuseShader):
		"""
		Set the material's diffuse shader from one of the shaders in Material.Shaders dict.
		@type diffuseShader: int
		@param diffuseShader: The new value in [0, 3].
		"""

	def getDiffuseShader():
		"""
		Get the material's diffuse shader from one of the shaders in Material.Shaders dict.
		@rtype: int
		"""

	def setRoughness(roughness):
		"""
		Set the material's roughness (applies to the \"Oren Nayar\" Diffuse Shader only)
		@type roughness: float
		@param roughness: The new value in [0.0, 3.14].
		"""

	def getRoughness():
		"""
		Get the material's roughness (applies to the \"Oren Nayar\" Diffuse Shader only)
		@rtype: float
		"""

	def setSpecSize(specSize):
		"""
		Set the material's size of specular area (applies to the \"Toon\" Specular Shader only)
		@type specSize: float
		@param specSize: The new value in [0.0, 1.53].
		"""

	def getSpecSize():
		"""
		Get the material's size of specular area (applies to the \"Toon\" Specular Shader only)
		@rtype specSize: float
		"""

	def setSpecSize(diffuseSize):
		"""
		Set the material's size of diffuse area (applies to the \"Toon\" Diffuse Shader only)
		@type diffuseSize: float
		@param diffuseSize: The new value in [0.0, 3.14].
		"""

	def getSpecSize():
		"""
		Get the material's size of diffuse area (applies to the \"Toon\" Diffuse Shader only)
		@rtype: float
		"""

	def setSpecSmooth(specSmooth):
		"""
		Set the material's smoothing of specular area (applies to the \"Toon\" Specular Shader only)
		@type specSmooth: float
		@param specSmooth: The new value in [0.0, 1.0].
		"""

	def getSpecSmooth():
		"""
		Get the material's smoothing of specular area (applies to the \"Toon\" Specular Shader only)
		@rtype: float
		"""

	def setDiffuseSmooth(diffuseSmooth):
		"""
		Set the material's smoothing of diffuse area (applies to the \"Toon\" Diffuse Shader only)
		@type diffuseSmooth: float
		@param diffuseSmooth: The new value in [0.0, 1.0].
		"""

	def getDiffuseSmooth():
		"""
		Get the material's smoothing of diffuse area (applies to the \"Toon\" Diffuse Shader only)
		@rtype: float
		"""

	def setDiffuseDarkness(diffuseDarkness):
		"""
		Set the material's diffuse darkness (applies to the \"Minnaert\" Diffuse Shader only)
		@type diffuseDarkness: float
		@param diffuseDarkness: The new value in [0.0, 2.0].
		"""

	def getDiffuseDarkness():
		"""
		Get the material's diffuse darkness (applies to the \"Minnaert\" Diffuse Shader only)
		@rtype: float
		"""

	def setRefracIndex(refracIndex):
		"""
		Set the material's Index of Refraction (applies to the \"Blinn\" Specular Shader only)
		@type refracIndex: float
		@param refracIndex: The new value in [1.0, 10.0].
		"""

	def getRefracIndex():
		"""
		Get the material's Index of Refraction (applies to the \"Blinn\" Specular Shader only)
		@rtype: float
		"""

	def setRms(rms):
		"""
		Set the material's standard deviation of surface slope (applies to the \"WardIso\" Specular Shader only)
		@type rms: float
		@param rms: The new value in [0.0, 0.4].
		"""

	def getRms():
		"""
		Get the material's standard deviation of surface slope (applies to the \"WardIso\" Specular Shader only)
		@rtype: float
		"""

	def setFilter(filter):
		"""
		Set the material's amount of filtering when transparent raytrace is enabled
		@type filter: float
		@param filter: The new value in [0.0, 1.0].
		"""

	def getFilter():
		"""
		Get the material's amount of filtering when transparent raytrace is enabled
		@rtype: float
		"""

	def setTranslucency(translucency):
		"""
		Set the material's amount of diffuse shading of the back side
		@type translucency: float
		@param translucency: The new value in [0.0, 1.0].
		"""

	def getTranslucency():
		"""
		Get the material's amount of diffuse shading of the back side
		@rtype: float
		"""

	def getAdd():
		"""
		Get the glow factor.
		@rtype: float
		"""

	def setAdd(add):
		"""
		Set the glow factor.
		@type add: float
		@param add: The new value in [0.0, 1.0].
		"""

	def getZOffset():
		"""
		Get the artificial offset for faces with this Material.
		@rtype: float
		"""

	def setZOffset(zoffset):
		"""
		Set the artificial offset for faces with this Material.
		@type zoffset: float
		@param zoffset: The new value in [0.0, 10.0].
		"""

	def getHaloSize():
		"""
		Get the halo size.
		@rtype: float
		"""

	def setHaloSize(halosize):
		"""
		Set the halo size.
		@type halosize: float
		@param halosize: The new value in [0.0, 100.0].
		"""

	def getHaloSeed():
		"""
		Get the seed for random ring dimension and line location in halos.
		@rtype: int
		"""

	def setHaloSeed(haloseed):
		"""
		Set the seed for random ring dimension and line location in halos.
		@type haloseed: int
		@param haloseed: The new value in [0, 255].
		"""

	def getFlareSize():
		"""
		Get the ratio: flareSize / haloSize.
		@rtype: float
		"""

	def setFlareSize(flaresize):
		"""
		Set the ratio: flareSize / haloSize.
		@type flaresize: float
		@param flaresize: The new value in [0.1, 25.0].
		"""

	def getFlareSeed():
		"""
		Get flare's offset in the seed table.
		@rtype: int
		"""

	def setFlareSeed(flareseed):
		"""
		Set flare's offset in the seed table.
		@type flareseed: int
		@param flareseed: The new value in [0, 255].
		"""

	def getFlareBoost():
		"""
		Get the flare's extra strength.
		@rtype: float
		"""

	def setFlareBoost(flareboost):
		"""
		Set the flare's extra strength.
		@type flareboost: float
		@param flareboost: The new value in [0.1, 10.0].
		"""

	def getSubSize():
		"""
		Get the dimension of subflare, dots and circles.
		@rtype: float
		"""

	def setSubSize(subsize):
		"""
		Set the dimension of subflare, dots and circles.
		@type subsize: float
		@param subsize: The new value in [0.1, 25.0].
		"""

	def getHardness():
		"""
		Get the hardness of the specularity.
		@rtype: int
		"""

	def setHardness(hardness):
		"""
		Set the hardness of the specularity.
		@type hardness: int
		@param hardness: The new value in [1, 511].
		"""

	def getNFlares():
		"""
		Get the number of halo subflares.
		@rtype: int
		"""

	def setNFlares(nflares):
		"""
		Set the number of halo subflares.
		@type nflares: int
		@param nflares: The new value in [1, 32].
		"""

	def getNStars():
		"""
		Get the number of points in the halo stars.
		@rtype: int
		"""

	def setNStars(nstars):
		"""
		Set the number of points in the halo stars.
		@type nstars: int
		@param nstars: The new value in [3, 50].
		"""

	def getNLines():
		"""
		Get the number of star shaped lines on each halo.
		@rtype: int
		"""

	def setNLines(nlines):
		"""
		Set the number of star shaped lines on each halo.
		@type nlines: int
		@param nlines: The new value in [0, 250].
		"""

	def getNRings():
		"""
		Get the number of rings on each halo.
		@rtype: int
		"""

	def setNRings(nrings):
		"""
		Set the number of rings on each halo.
		@type nrings: int
		@param nrings: The new value in [0, 24].
		"""

	def getRayMirr():
		"""
		Get amount mirror reflection for raytrace.
		@rtype: float
		"""

	def setRayMirr(nrmirr):
		"""
		Set amount mirror reflection for raytrace.
		@type nrmirr: float
		@param nrmirr: The new value in [0.0, 1.0].
		"""

	def getRayMirrDepth():
		"""
		Get amount of inter-reflections calculated maximal.
		@rtype: int
		"""

	def setRayMirrDepth(nrmirr):
		"""
		Set amount mirror reflection for raytrace.
		@type nrmirr: int
		@param nrmirr: The new value in [0.0, 1.0].
		"""

	def getFresnelMirr():
		"""
		Get power of Fresnel for mirror reflection.
		@rtype: float
		"""

	def setFresnelMirr(nrmirr):
		"""
		Set power of Fresnel for mirror reflection.
		@type nrmirr: float
		@param nrmirr: The new value in [0.0, 1.0].
		"""

	def getFresnelMirrFac():
		"""
		Get the number of Ray Mirror.
		@rtype: float
		"""

	def setFresnelMirrFac(nrmirr):
		"""
		Set the number of ray mirror
		@type nrmirr: float
		@param nrmirr: The new value in [0.0, 1.0].
		"""

	def getIOR():
		"""
		Get the angular index of refraction for raytrace.
		@rtype: float
		"""

	def setIOR(nrmirr):
		"""
		Set the angular index of refraction for raytrace.
		@type nrmirr: float
		@param nrmirr: The new value in [0.0, 1.0].
		"""

	def getTransDepth():
		"""
		Get amount of refractions calculated maximal.
		@rtype: int
		"""

	def setTransDepth(nrmirr):
		"""
		Set amount of refractions calculated maximal.
		@type nrmirr: int
		@param nrmirr: The new value in [0.0, 1.0].
		"""

	def getFresnelTrans():
		"""
		Get power of Fresnel for transparency.
		@rtype: float
		"""

	def setFresnelTrans(nrmirr):
		"""
		Set power of Fresnel for transparency.
		@type nrmirr: float
		@param nrmirr: The new value in [0.0, 1.0].
		"""

	def getFresnelTransFac():
		"""
		Get blending factor for Fresnel.
		@rtype: float
		"""

	def setFresnelTransFac(nrmirr):
		"""
		Set blending factor for Fresnel.
		@type nrmirr: float
		@param nrmirr: The new value in [0.0, 1.0].
		"""

	def setTexture(index, texture, texco, mapto):
		"""
		Assign a Blender Texture object to slot number 'number'.
		@type index: int
		@param index: material's texture index in [0, 9].
		@type texture: Blender Texture
		@param texture: a Blender Texture object.
		@type texco: int
		@param texco: optional ORed bitflag -- defaults to TexCo.ORCO.  See TexCo var in L{Texture}.
		@type mapto: int
		@param mapto: optional ORed bitflag -- defaults to MapTo.COL.  See MapTo var in L{Texture}.
		"""

	def clearTexture(index):
		"""
		Clear the ith (given by 'index') texture channel of this material.
		@type index: int
		@param index: material's texture channel index in [0, 9].
		"""

	def getTextures ():
		"""
		Get this Material's Texture list.
		@rtype: list of MTex
		@return: a list of Blender MTex objects.  None is returned for each empty
				texture slot.
		"""

	def getScriptLinks (event):
		"""
		Get a list with this Material's script links of type 'event'.
		@type event: string
		@param event: "FrameChanged" or "Redraw".
		@rtype: list
		@return: a list with Blender L{Text} names (the script links of the given
				'event' type) or None if there are no script links at all.
		"""

	def clearScriptLinks (links = None):
		"""
		Delete script links from this Material.  If no list is specified, all
		script links are deleted.
		@type links: list of strings
		@param links: None (default) or a list of Blender L{Text} names.
		"""

	def addScriptLink (text, event):
		"""
		Add a new script link to this Material.
		@type text: string
		@param text: the name of an existing Blender L{Text}.
		@type event: string
		@param event: "FrameChanged" or "Redraw".
		"""

	def __copy__ ():
		"""
		Make a copy of this material
		@rtype: Material
		@return:  a copy of this material
		"""

import id_generics
Material.__doc__ += id_generics.attributes
