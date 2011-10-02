"""
Basic Sound Playback
++++++++++++++++++++
This script shows how to use the classes: :class:`Device`, :class:`Factory` and
:class:`Handle`.
"""
import aud

device = aud.device()
# load sound file (it can be a video file with audio)
factory = aud.Factory('music.ogg')

# play the audio, this return a handle to control play/pause
handle = device.play(factory)
# if the audio is not too big and will be used often you can buffer it
factory_buffered = aud.Factory.buffer(factory)
handle_buffered = device.play(factory_buffered)

# stop the sounds (otherwise they play until their ends)
handle.stop()
handle_buffered.stop()
