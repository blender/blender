"""
Texture replacement
++++++++++++++++++++++
Example of how to replace a texture in game with an external image.
createTexture() and removeTexture() are to be called from a module Python
Controller.
"""
from bge import logic
from bge import texture


def createTexture(cont):
    """Create a new Dynamic Texture"""
    obj = cont.owner

    # get the reference pointer (ID) of the internal texture
    ID = texture.materialID(obj, 'IMoriginal.png')

    # create a texture object
    object_texture = texture.Texture(obj, ID)

    # create a new source with an external image
    url = logic.expandPath("//newtexture.jpg")
    new_source = texture.ImageFFmpeg(url)

    # the texture has to be stored in a permanent Python object
    logic.texture = object_texture

    # update/replace the texture
    logic.texture.source = new_source
    logic.texture.refresh(False)


def removeTexture(cont):
    """Delete the Dynamic Texture, reversing back the final to its original state."""
    try:
        del logic.texture
    except:
        pass
