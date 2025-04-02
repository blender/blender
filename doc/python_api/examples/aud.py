"""
Basic Sound Playback
++++++++++++++++++++

This script shows how to use the classes: :class:`Device`, :class:`Sound` and
:class:`Handle`.
"""
import aud

device = aud.Device()
# Load sound file (it can be a video file with audio).
sound = aud.Sound('music.ogg')

# Play the audio, this return a handle to control play/pause.
handle = device.play(sound)
# If the audio is not too big and will be used often you can buffer it.
sound_buffered = aud.Sound.cache(sound)
handle_buffered = device.play(sound_buffered)

# Stop the sounds (otherwise they play until their ends).
handle.stop()
handle_buffered.stop()
