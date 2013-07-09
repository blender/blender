"""
Basic Video Playback
++++++++++++++++++++
Example of how to replace a texture in game with a video. It needs to run everyframe
"""
import bge
from bge import texture
from bge import logic

cont = logic.getCurrentController()
obj = cont.owner

# the creation of the texture must be done once: save the
# texture object in an attribute of bge.logic module makes it persistent
if not hasattr(logic, 'video'):

    # identify a static texture by name
    matID = texture.materialID(obj, 'IMvideo.png')

    # create a dynamic texture that will replace the static texture
    logic.video = texture.Texture(obj, matID)

    # define a source of image for the texture, here a movie
    movie = logic.expandPath('//trailer_400p.ogg')
    logic.video.source = texture.VideoFFmpeg(movie)
    logic.video.source.scale = True

    # quick off the movie, but it wont play in the background
    logic.video.source.play()

# you need to call this function every frame to ensure update of the texture.
logic.video.refresh(True)
