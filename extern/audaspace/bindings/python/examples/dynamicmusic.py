import aud, sys, time

device=aud.Device()
dMusic = aud.DynamicMusic(device)
sound1 = aud.Sound.file(sys.argv[1])
sound2 = aud.Sound.file(sys.argv[2])
effect = aud.Sound.file(sys.argv[3])

dMusic.addScene(sound1)
dMusic.addScene(sound2)
dMusic.addTransition(1,2,effect)

dMusic.fadeTime=3
dMusic.volume=0.5

dMusic.scene=1
time.sleep(5)
dMusic.scene=2

time.sleep(500)