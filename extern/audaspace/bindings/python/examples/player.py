#!/usr/bin/python
import aud, sys, time
device = aud.Device()
sound = aud.Sound.file(sys.argv[1])
handle = device.play(sound)
while handle.status:
	time.sleep(0.1)
