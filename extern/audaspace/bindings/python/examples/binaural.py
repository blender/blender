#!/usr/bin/python
import aud, sys, time, multiprocessing
device = aud.Device()
hrtf = aud.HRTF().loadLeftHrtfSet(".wav", sys.argv[2])
threadPool = aud.ThreadPool(multiprocessing.cpu_count())
source = aud.Source(0, 0, 0)
sound = aud.Sound.file(sys.argv[1]).rechannel(1).binaural(hrtf, source, threadPool)
handle = device.play(sound)

while handle.status:
    source.azimuth += 1
    print("Azimuth: " + str(source.azimuth))
    time.sleep(0.1)