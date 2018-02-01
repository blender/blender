import aud, sys, time

device=aud.Device()
manager = aud.PlaybackManager(device)
sound1 = aud.Sound.file(sys.argv[1])
sound2 = aud.Sound.file(sys.argv[2])
sound3 = aud.Sound.file(sys.argv[3])
sound4 = aud.Sound.file(sys.argv[4])

manager.play(sound1, 0)
manager.play(sound2, 0)
manager.play(sound3, 1)
manager.play(sound4, 1)

manager.setVolume(0.2, 0)
time.sleep(5)
manager.setVolume(0.0, 1)
time.sleep(5)
manager.pause(0)
time.sleep(5)
manager.setVolume(0.5, 1)
manager.setVolume(1.0, 0)
time.sleep(5)
manager.stop(1)
manager.resume(0)

time.sleep(500)