import aud, sys, time

device=aud.Device()
sound1 = aud.Sound.file(sys.argv[1])
sound2 = aud.Sound.file(sys.argv[2])
sound3 = aud.Sound.file(sys.argv[3])
sound4 = aud.Sound.file(sys.argv[4])
list=aud.Sound.list(True)

list.addSound(sound1)
list.addSound(sound2)
list.addSound(sound3)
list.addSound(sound4)
mutable=aud.Sound.mutable(list)

device.lock()
handle=device.play(mutable)
handle.loop_count=2
device.unlock()

time.sleep(500)