#!/usr/bin/python
import aud, time
device = aud.Device()
sine = aud.Sound.sine(440)
square = sine.threshold()
handle = device.play(square)
time.sleep(3)
