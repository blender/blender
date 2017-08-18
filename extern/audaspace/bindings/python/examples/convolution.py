#!/usr/bin/python
import aud, sys, time, multiprocessing
device = aud.Device()
ir = aud.ImpulseResponse(aud.Sound.file(sys.argv[2]))
threadPool = aud.ThreadPool(multiprocessing.cpu_count())
sound = aud.Sound.file(sys.argv[1]).convolver(ir, threadPool)
handle = device.play(sound)
handle.volume = 0.1
while handle.status:
    time.sleep(0.1)