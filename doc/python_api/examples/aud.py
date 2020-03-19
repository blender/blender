"""
Basic Sound Playback
++++++++++++++++++++

This script shows how to use the classes: :class:`Device`, :class:`Sound` and
:class:`Handle`.
"""
import aud

device = aud.Device()
# load sound file (it can be a video file with audio)
sound = aud.Sound('music.ogg')

# play the audio, this return a handle to control play/pause
handle = device.play(sound)
# if the audio is not too big and will be used often you can buffer it
sound_buffered = aud.Sound.buffer(sound)
handle_buffered = device.play(sound_buffered)

# stop the sounds (otherwise they play until their ends)
handle.stop()
handle_buffered.stop()
