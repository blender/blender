# *** PROPOSAL ***
#    11/11/2003

# Detail of classes MTex, EnvMap and ColorBand not included yet
# Also missing is Tex.imaflag, Tex.flag and Tex.stype, pending discussion on
# their implementation

#
# Blender.Texture module and the Texture PyType object
#
# Written by Alex Mole
# 


"""
The Blender.Texture submodule.

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

@type Flags: readonly dictionary
@var Flags: The available Texture flags:
    - FLIPBLEND - Flips the blend texture's X and Y directions
    - NEGALPHA - Reverse the alpha value

@type ImageFlags: readonly dictionary
@var ImageFlags: The available image flags for Texture.imageFlags:
    - INTERPOL - Interpolate pixels of the image
    - USEALPHA - Use the alpha layer
    - MIPMAP - Enable mipmapping [cannot be used with FIELDS]
    - FIELDS - Work with field images [cannot be used with MIPMAP]
    - ROT90 - Rotate the image 90 degrees when rendering
    - CALCALPHA - Calculate an alpha from the RGB
    - STFIELD - ???
    - MOVIE - Use a movie for an image
    - CYCLIC - Repeat animation image
    - ANTI - Use anti-aliasing

@type ExtendModes: readonly dictionary
@var ExtendModes: Extend, clamp or repeat modes for images
    - EXTEND - Extends the colour of the edge
    - CLIP - Return alpha 0.0 outside image
    - CLIPCUBE - Return alpha 0.0 around cube-shaped area around image
    - REPEAT - Repeat image vertically and horizontally

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
            - BLN_EASE - ???
            - BLN_DIAG - Use a diagonal progression
            - BLN_SPHERE - Use a progression with the shape of a sphere
            - BLN_HALO - Use a quadratic progression with the shape of a sphere
        6. Stucci type
            - STC_PLASTIC - Standard stucci
            - STC_WALLIN - Set start value (?)
            - STC_WALLOUT - Set end value (?)
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
@type TexCo: readonly dictionary

@var MapTo: Flags for MTex.mapto.
    - COL - Make the texture affect the basic colour of the material
    - NOR - Make the texture affect the rendered normal
    - CSP - Make the texture affect the specularity colour
    - CMIR - Make the texture affect the mirror colour
    - REF - Make the texture affect the value of the material's reflectivity
    - SPEC - Make the texture affect the value of specularity
    - HARD - Make the texture affect the hardness value
    - ALPHA - Make the texture affect the alpha value
    - EMIT - Make the texture affext the emit value
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

class Texture:
    """
    The Texture object
    ==================
    This object gives access to Texture-specific data in Blender.

    Note that many of the attributes of this object are only relevant if
    specific modes are enabled.
  
    @cvar name: The Texture name.
    @cvar type: The Texture type. See L{Types}
    @cvar flags: The texture flags (OR'd together). See L{Flags}
    @cvar imageFlags: The texture image flags (OR'd tegether). 
        See L{ImageFlags}
    @cvar stype: Texture-type specific data. See L{STypes}
    @cvar image: The image associated with this texture, or None.
    @type image: Blender Image
    @cvar rgbCol: The texture's RGB color triplet.
    @cvar brightness: The brightness in range [0,2].
    @cvar contrast: The contrast in range [0,2].
    @cvar filterSize: The filter size for the image.
    @cvar extend: Texture extend/repeat mode. See L{ExtendModes}
    @cvar crop: Tuple of image crop values as floats, 
        like C{(xmin, ymin, xmax, ymax)}
    @cvar repeat: Tuple of image repeat values as ints, like 
        C{(xrepeat, yrepeat)}
    @cvar noiseSize: The noise size.
    @cvar noiseDepth: The noise depth.
    @cvar noiseType: The noise type: 'soft' or 'hard'.
    @cvar animLength: Length of the animation.
    @cvar animFrames: Frames of the animation.
    @cvar animOffset: The number of the first picture of the animation.
    @cvar animStart: Start frame of the animation.
    @cvar fieldsPerImage: The number of fields per rendered frame.
    @cvar animMontage: Montage mode data as a tuple of tuples, like
        C{( (fra1,dur1), (fra2,dur2), (fra3,dur3), (fra4,dur4) )}
    """
    
    def getExtend():
        """
        Get the extend mode of the texture. See L{setExtend}
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
        Get this Texture's type.
        @rtype: string
        @return: The Texture's type. See L{setType}
        """

    def setExtend(extendmode):
        """
        Set the extend mode of this texture (only used for IMAGE textures)
        @param extendmode: The new extend mode. One of: 
            'Extend', 'Clip', 'ClipCube' and 'Repeat'
        @type extendmode: string
        """

    def setFlags(f=None, f2=None, f3=None):
        """
        Set this object's flags.
        @param f: Flags to be set (omitted flags are cleared). Can be any of 
            'ColorBand', 'FlipBlendXY', and 'NegAlpha'
        @type f: string
        """
 
    def setImage(image):
        """
        Set the Image of this texture.
        @param image: The new Image.
        @type image: Blender Image
        @warning: This sets the texture's type to 'Image' if it is not already.
        """

    def setImageFlags(f=None, f2=None, f3=None, and_so_on=None):
        """
        Set the Image flags (only makes sense for IMAGE textures). Omitted
        flags are cleared.
        @param f: Flag to set. See L{ImageFlags} for their meanings. Can be 
            any of: 'InterPol', 'UseAlpha', 'MipMap', 'Fields', 'Rot90',
            'CalcAlpha', 'StField', 'Movie' and 'Cyclic'
        @type f: string
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
        @param stype: The new stype. This can be one of the following values
            or 'Default' which sets the stype to the default value. See 
            L{STypes} for their meanings.
            'CloudDefault', 'CloudColor', 'WoodBands', 'WoodRings',
            'WoodBandNoise', 'WoodRingNoise', 'MarbleSoft', 'MarbleSharp',
            'MarbleSharper', 'BlendLin', 'BlendQuad', 'BlendEase',
            'BlendDiag', 'BlendSphere', 'BlendHalo', 'StucciPlastic',
            'StucciWallIn', 'StucciWallOut', 'EnvmapStatic', 'EnvmapAnim',
            'EnvmapLoad'
        @type stype: string
        """
        
    def setType(type):
        """
        Set this Texture's type.
        @param type: The new type. Possible options are: 
            'None', 'Clouds', 'Wood', 'Marble', 'Magic', 'Blend', 'Stucci', 
            'Noise', 'Image', 'Plugin' and 'EnvMap'
        @type type: string
        """


class MTex:
    """
    The MTex Object
    ===============

    This object links a material to a texture. It allows the same texture to be
    used in several different ways.

    @cvar tex: The Texture this is linked to.
    @type tex: Blender Texture
    @cvar texco: Texture coordinates ("Map input"). See L{TexCo}
    @cvar mapto: "Map to" field of texture. OR'd values of L{MapTo}
    """

