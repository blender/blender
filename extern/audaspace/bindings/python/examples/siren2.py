#!/usr/bin/python
import aud, math, time
length = 0.5
fadelength = 0.05
runtime = 10
distance = 100
velocity = 2 * distance / runtime

device = aud.Device()
high = aud.Sound.sine(880).limit(0, length).fadein(0, fadelength).fadeout(length - fadelength, length)
low = aud.Sound.sine(700).limit(0, length).fadein(0, fadelength).fadeout(length - fadelength, length).volume(0.6)
sound = high.join(low)
handle = device.play(sound)
handle.loop_count = -1

handle.velocity = [velocity, 0, 0]

start = time.time()

while time.time() - start < runtime:
	location = -distance + velocity * (time.time() - start)

	handle.location = [location, 10, 0]
