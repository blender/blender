"""The Blender Image module

  This module provides (yet) basic support for Blender *Image* data blocks

  Example::

    from Blender import Image
    im = Image.Load('dead-parrot.jpg')
"""

#import _Blender.Image as _Image
import shadow

class Image(shadow.shadow):
	"""Image DataBlock object

    See above example on how to create instances of Image objects.

  Attributes

    xrep  -- Texture image tiling factor (subdivision) in X

    yrep  -- Texture image tiling factor (subdivision) in Y

    LATER:

    * Image buffer access

    * better loading / saving of images
"""
	pass

def get(name):
	"""If 'name' given, the Image 'name' is returned if existing, 'None' otherwise.
If no name is given, a list of all Images is returned"""
	pass

def Load(filename):
	"""Returns image from file 'filename' as Image object if found, 'None' else."""
	pass
	
def New(name):
	"""This function is currently not implemented"""
	pass

# override all functions again, the above classes are just made
# for documentation

get = _Image.get
Get = get
Load = _Image.Load

