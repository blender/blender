"""
Basic Video Playback
++++++++++++++++++++
Example of how to replace a texture in game with a video.
It needs to run everyframe.
To avoid any confusion with the location of the file,
we will use ``GameLogic.expandPath()`` to build an absolute file name,
assuming the video file is in the same directory as the blend-file.
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

    # Note that we can change the ``Texture`` source at any time.
    # Suppose we want to switch between two movies during the game:
    logic.mySources[0] = texture.VideoFFmpeg('movie1.avi')
    logic.mySources[1] = texture.VideoFFmpeg('movie2.avi')

    # And then assign (and reassign) the source during the game
    logic.video.source = logic.mySources[movieSel]

    # quick off the movie, but it wont play in the background
    logic.video.source.play()


# Video playback is not a background process: it happens only when we refresh the texture.
# So you need to call this function every frame to ensure update of the texture.
logic.video.refresh(True)
