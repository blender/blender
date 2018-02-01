#!/usr/bin/python
import aud, math, time
length = 0.5
fadelength = 0.05

device = aud.Device()
high = aud.Sound.sine(880).limit(0, length).fadein(0, fadelength).fadeout(length - fadelength, length)
low = aud.Sound.sine(700).limit(0, length).fadein(0, fadelength).fadeout(length - fadelength, length).volume(0.6)
sound = high.join(low)
handle = device.play(sound)
handle.loop_count = -1

start = time.time()

while time.time() - start < 10:
	angle = time.time() - start

	handle.location = [math.sin(angle), 0, -math.cos(angle)]

