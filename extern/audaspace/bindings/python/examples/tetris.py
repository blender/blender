#!/usr/bin/python
import aud, math, time
 
def parseNotes(notes, bpm, basefreq, rate = 44100,
               notechars = "XXXCXDXEFXGXAXHcXdXefXgXaXhp"):
	pos = 0
	fadelength = 60/bpm/10
	halfchars = "#b"
	durationchars = "2345678"
	sound = None
 
	while pos < len(notes):
		char = notes[pos]
		mod = None
		dur = 1
		pos += 1
		while pos < len(notes) and notes[pos] not in notechars:
			if notes[pos] in halfchars:
				mod = notes[pos]
			elif notes[pos] in durationchars:
				dur = notes[pos]
			pos += 1
 
		freq = notechars.find(char)
		if mod == '#':
			freq += 1
		elif mod == 'b':
			freq -= 1
 
		freq = math.pow(2, freq/12)*basefreq
		length = float(dur)*60/bpm
 
		snd = aud.Sound.square(freq, rate)
		if char == 'p':
			snd = snd.volume(0)
		snd = snd.limit(0, length)
		snd = snd.fadein(0, fadelength)
		snd = snd.fadeout(length - fadelength, fadelength)
 
		if sound:
			sound = sound.join(snd)
		else:
			sound = snd
	return sound
 
def tetris(bpm = 300, freq = 220, rate = 44100):
	notes = "e2Hcd2cH A2Ace2dc H3cd2e2 c2A2A4 pd2fa2gf e3ce2dc H2Hcd2e2 c2A2A2p2"
	s11 = parseNotes(notes, bpm, freq, rate)
 
	notes = "e4c4 d4H4 c4A4 G#4p4 e4c4 d4H4 A2c2a4 g#4p4"
	s12 = parseNotes(notes, bpm, freq, rate)
 
	notes = "EeEeEeEe AaAaAaAa AbabAbabAbabAbab AaAaAAHC DdDdDdDd CcCcCcCc HhHhHhHh AaAaA2p2"
	s21 = parseNotes(notes, bpm, freq, rate, notechars = "AXHCXDXEFXGXaXhcXdXefXgXp")
 
	notes = "aeaeaeae g#dg#dg#dg#d aeaeaeae g#dg#dg#2p2 aeaeaeae g#dg#dg#dg#d aeaeaeae g#dg#dg#2p2"
	s22 = parseNotes(notes, bpm, freq/2, rate)
 
	return s11.join(s12).join(s11).volume(0.5).mix(s21.join(s22).join(s21).volume(0.3))
 
if __name__ == "__main__":
	dev = aud.Device()
	handle = dev.play(tetris(300, 220, dev.rate))
	while handle.status:
		time.sleep(0.1)

