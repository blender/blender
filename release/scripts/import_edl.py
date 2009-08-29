#!BPY
 
"""
Name: 'Video Sequence (.edl)...'
Blender: 248
Group: 'Import'
Tooltip: 'Load a CMX formatted EDL into the sequencer'
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2009: Campbell Barton, ideasman42@gmail.com
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# --------------------------------------------------------------------------

class TimeCode(object):
	'''
	Simple timecode class
	also supports conversion from other time strings used by EDL
	'''
	def __init__(self, data, fps):
		self.fps= fps
		if type(data)==str:
			self.fromString(data)
			frame = self.asFrame()
			self.fromFrame(frame)
		else:
			self.fromFrame(data)
		
	def fromString(self, text):
		# hh:mm:ss:ff
		# No dropframe support yet

		if text.lower().endswith('mps'): # 5.2mps
			return self.fromFrame( int( float(text[:-3]) * self.fps ) )
		elif text.lower().endswith('s'): # 5.2s
			return self.fromFrame( int( float(text[:-1]) * self.fps ) )
		elif text.isdigit(): # 1234
			return self.fromFrame( int(text) )
		elif ':' in text: # hh:mm:ss:ff
			text= text.replace(';', ':').replace(',', ':').replace('.', ':')
			text= text.split(':')
			
			self.hours= int(text[0])
			self.minutes= int(text[1])
			self.seconds= int(text[2])
			self.frame= int(text[3])
			return self
		else:
			print 'ERROR: could not convert this into timecode "%s"' % test
			return self

		
	def fromFrame(self, frame):
		
		if frame < 0:
			frame = -frame;
			neg=True
		else:
			neg=False
		
		fpm = 60 * self.fps
		fph = 60 * fpm
		
		if frame < fph:
			self.hours= 0
		else:
			self.hours= int(frame/fph)
			frame = frame % fph
		
		if frame < fpm:
			self.minutes= 0
		else:
			self.minutes= int(frame/fpm)
			frame = frame % fpm
		
		if frame < self.fps:
			self.seconds= 0
		else:
			self.seconds= int(frame/self.fps)
			frame = frame % self.fps
		
		self.frame= frame
		
		if neg:
			self.frame = -self.frame
			self.seconds = -self.seconds
			self.minutes = -self.minutes
			self.hours = -self.hours
		
		return self
		
	def asFrame(self):
		abs_frame= self.frame
		abs_frame += self.seconds * self.fps
		abs_frame += self.minutes * 60 * self.fps
		abs_frame += self.hours * 60 * 60 * self.fps
		
		return abs_frame
	
	def asString(self):
		self.fromFrame(int(self))
		return '%.2d:%.2d:%.2d:%.2d' % (self.hours, self.minutes, self.seconds, self.frame)
	
	def __repr__(self):
		return self.asString()
	
	# Numeric stuff, may as well have this
	def __neg__(self):			return TimeCode(-int(self), self.fps)
	def __int__(self):			return self.asFrame()
	def __sub__(self, other):		return TimeCode(int(self)-int(other), self.fps)
	def __add__(self, other):		return TimeCode(int(self)+int(other), self.fps)
	def __mul__(self, other):		return TimeCode(int(self)*int(other), self.fps)
	def __div__(self, other):		return TimeCode(int(self)/int(other), self.fps)
	def __abs__(self):			return TimeCode(abs(int(self)), self.fps)
	def __iadd__(self, other):	return self.fromFrame(int(self)+int(other))
	def __imul__(self, other):	return self.fromFrame(int(self)*int(other))
	def __idiv__(self, other):		return self.fromFrame(int(self)/int(other))
# end timecode


'''Comments
Comments can appear at the beginning of the EDL file (header) or between the edit lines in the EDL. The first block of comments in the file is defined to be the header comments and they are associated with the EDL as a whole. Subsequent comments in the EDL file are associated with the first edit line that appears after them.
Edit Entries
<filename|tag>  <EditMode>  <TransitionType>[num]  [duration]  [srcIn]  [srcOut]  [recIn]  [recOut]

    * <filename|tag>: Filename or tag value. Filename can be for an MPEG file, Image file, or Image file template. Image file templates use the same pattern matching as for command line glob, and can be used to specify images to encode into MPEG. i.e. /usr/data/images/image*.jpg
    * <EditMode>: 'V' | 'A' | 'VA' | 'B' | 'v' | 'a' | 'va' | 'b' which equals Video, Audio, Video_Audio edits (note B or b can be used in place of VA or va).
    * <TransitonType>: 'C' | 'D' | 'E' | 'FI' | 'FO' | 'W' | 'c' | 'd' | 'e' | 'fi' | 'fo' | 'w'. which equals Cut, Dissolve, Effect, FadeIn, FadeOut, Wipe.
    * [num]: if TransitionType = Wipe, then a wipe number must be given. At the moment only wipe 'W0' and 'W1' are supported.
    * [duration]: if the TransitionType is not equal to Cut, then an effect duration must be given. Duration is in frames.
    * [srcIn]: Src in. If no srcIn is given, then it defaults to the first frame of the video or the first frame in the image pattern. If srcIn isn't specified, then srcOut, recIn, recOut can't be specified.
    * [srcOut]: Src out. If no srcOut is given, then it defaults to the last frame of the video - or last image in the image pattern. if srcOut isn't given, then recIn and recOut can't be specified.
    * [recIn]: Rec in. If no recIn is given, then it is calculated based on its position in the EDL and the length of its input.
      [recOut]: Rec out. If no recOut is given, then it is calculated based on its position in the EDL and the length of its input. first frame of the video. 

For srcIn, srcOut, recIn, recOut, the values can be specified as either timecode, frame number, seconds, or mps seconds. i.e.
[tcode | fnum | sec | mps], where:

    * tcode : SMPTE timecode in hh:mm:ss:ff
    * fnum : frame number (the first decodable frame in the video is taken to be frame 0).
    * sec : seconds with 's' suffix (e.g. 5.2s)
    * mps : seconds with 'mps' suffix (e.g. 5.2mps). This corresponds to the 'seconds' value displayed by Windows MediaPlayer.

More notes, 
Key
	
'''

enum= 0
TRANSITION_UNKNOWN= enum
TRANSITION_CUT= enum;				enum+=1
TRANSITION_DISSOLVE= enum;			enum+=1
TRANSITION_EFFECT= enum;			enum+=1
TRANSITION_FADEIN= enum;			enum+=1
TRANSITION_FADEOUT= enum;			enum+=1
TRANSITION_WIPE= enum;				enum+=1
TRANSITION_KEY= enum;				enum+=1

TRANSITION_DICT={ \
				'c':TRANSITION_CUT,
				'd':TRANSITION_DISSOLVE,
				'e':TRANSITION_EFFECT,
				'fi':TRANSITION_FADEIN,
				'fo':TRANSITION_FADEOUT,
				'w':TRANSITION_WIPE,
				'k':TRANSITION_KEY,
				}

enum= 0
EDIT_UNKNOWN= 				1<<enum; enum+=1
EDIT_VIDEO= 				1<<enum; enum+=1
EDIT_AUDIO= 				1<<enum; enum+=1
EDIT_AUDIO_STEREO=			1<<enum; enum+=1
EDIT_VIDEO_AUDIO=			1<<enum; enum+=1

EDIT_DICT=		{ \
				'v':EDIT_VIDEO,
				'a':EDIT_AUDIO,
				'aa':EDIT_AUDIO_STEREO,
				'va':EDIT_VIDEO_AUDIO,
				'b':EDIT_VIDEO_AUDIO
				}


enum= 0
WIPE_UNKNOWN= enum
WIPE_0= enum;					enum+=1
WIPE_1= enum;					enum+=1

enum= 0
KEY_UNKNOWN= enum
KEY_BG= enum;					enum+=1 # K B
KEY_IN= enum;					enum+=1 # This is assumed if no second type is set
KEY_OUT= enum;					enum+=1 # K O


'''
Most sytems:
Non-dropframe: 1:00:00:00 - colon in last position
Dropframe: 1:00:00;00 - semicolon in last position
PAL/SECAM: 1:00:00:00 - colon in last position

SONY:
Non-dropframe: 1:00:00.00 - period in last position
Dropframe: 1:00:00,00 - comma in last position
PAL/SECAM: 1:00:00.00 - period in last position
'''

'''
t = abs(timecode('-124:-12:-43:-22', 25))
t /= 2
print t
'''

def editFlagsToText(flag):
	items = []
	for item, val in EDIT_DICT.items():
		if val & flag:
			items.append(item)
	return '/'.join(items)
	

class EditDecision(object):
	def __init__(self, text= None, fps= 25):
		# print text
		self.number = -1
		self.reel = '' # Uniqie name for this 'file' but not filename, when BL signifies black
		self.transition_duration= 0
		self.edit_type= EDIT_UNKNOWN
		self.transition_type= TRANSITION_UNKNOWN
		self.wipe_type = WIPE_UNKNOWN
		self.key_type = KEY_UNKNOWN
		self.key_fade = -1	# true/false
		self.srcIn = None  # Where on the original field recording the event begins
		self.srcOut = None # Where on the original field recording the event ends
		self.recIn = None  # Beginning of the original event in the edited program
		self.recOut = None # End of the original event in the edited program
		self.m2 = None		# fps set by the m2 command
		self.filename = '' 
		
		self.custom_data= [] # use for storing any data you want (blender strip for eg)

		if text != None:
			self.read(text, fps)
	
	def __repr__(self):
		txt= 'num: %d, ' % self.number
		txt += 'reel: %s, ' % self.reel
		txt += 'edit_type: '
		txt += editFlagsToText(self.edit_type) + ', '
		
		txt += 'trans_type: '
		for item, val in TRANSITION_DICT.items():
			if val == self.transition_type:
				txt += item + ', '
				break
		
		
		txt += 'm2: '
		if self.m2:
			txt += '%g' % float(self.m2.fps)
			txt += '\n\t'
			txt += self.m2.data
		else:
			txt += 'nil'
			
		txt += ', '
		txt += 'recIn: ' + str(self.recIn) + ', '
		txt += 'recOut: ' + str(self.recOut) + ', '
		txt += 'srcIn: ' + str(self.srcIn) + ', '
		txt += 'srcOut: ' + str(self.srcOut) + ', '
		
		return txt
		
		
	def read(self, line, fps):
		line= line.split()
		index= 0
		self.number= int(line[index]); index+=1
		self.reel= line[index].lower(); index+=1
		
		# AA/V can be an edit type
		self.edit_type= 0
		for edit_type in line[index].lower().split('/'):
			self.edit_type |= EDIT_DICT[edit_type]; 
		index+=1
		
		tx_name = ''.join([c for c in line[index].lower() if not c.isdigit()])
		self.transition_type= TRANSITION_DICT[tx_name]; # advance the index later
		
		if self.transition_type== TRANSITION_WIPE:
			tx_num = ''.join([c for c in line[index].lower() if c.isdigit()])
			if tx_num:	tx_num = int(tx_num)
			else:		tx_num = 0
					
			self.wipe_type= tx_num
		
		elif self.transition_type== TRANSITION_KEY: # UNTESTED
			
			val= line[index+1].lower()
			
			if val == 'b':
				self.key_type= KEY_BG
				index+=1
			elif val == 'o':
				self.key_type= KEY_OUT
				index+=1
			else:
				self.key_type= KEY_IN # if no args given
				
			# there may be an (F) after, eg 'K B (F)'
			# in the docs this should only be after K B but who knows, it may be after K O also?
			val= line[index+1].lower()
			if val == '(f)':
				index+=1
				self.key_fade = True
			else:
				self.key_fade = False
			
		index+=1

		if self.transition_type in (TRANSITION_DISSOLVE, TRANSITION_EFFECT, TRANSITION_FADEIN, TRANSITION_FADEOUT, TRANSITION_WIPE):
			self.transition_duration= TimeCode(line[index], fps); index+=1

		if index < len(line):
			self.srcIn= TimeCode(line[index], fps); index+=1
		if index < len(line):
			self.srcOut= TimeCode(line[index], fps); index+=1
		
		if index < len(line):
			self.recIn= TimeCode(line[index], fps); index+=1
		if index < len(line):
			self.recOut= TimeCode(line[index], fps); index+=1
	
	def renumber(self):
		self.edits.sort( key=lambda e: int(e.recIn) )
		for i, edit in enumerate(self.edits):
			edit.number= i
		
	def clean(self):
		'''
		Clean up double ups
		'''
		self.renumber()
		
		# TODO
	def asName(self):
		cut_type = 'nil'
		for k,v in TRANSITION_DICT.iteritems():
			if v==self.transition_type:
				cut_type = k
				break
		
		return '%d_%s_%s' % (self.number, self.reel, cut_type)

class M2(object):
	def __init__(self):
		self.reel = None
		self.fps = None
		self.time = None
		self.data = None
		
		self.index = -1
		self.tot = -1
	
	def read(self, line, fps):
		
		# M2   TAPEC          050.5                00:08:11:08
		words = line.split()
		
		self.reel= 	words[1].lower()
		self.fps=	float(words[2])
		self.time=	TimeCode(words[3], fps)
		
		self.data = line
	
class EditList(object):
	def __init__(self):
		self.edits= []
		self.title= ''
		
	def parse(self, filename, fps):
		try:
			file= open(filename, 'rU')
		except:
			return False

		self.edits= []
		edits_m2 = [] # edits with m2's
		
		has_m2 = False
		
		for line in file:
			line= ' '.join(line.split())
			
			if not line or line.startswith('*') or line.startswith('#'):
				continue
			elif line.startswith('TITLE:'):
				self.title= ' '.join(line.split()[1:])
			elif line.split()[0].lower() == 'm2':
				has_m2 = True
				m2 = M2()
				m2.read(line, fps)
				edits_m2.append( m2 )
			elif not line.split()[0].isdigit():
				print 'Ignoring:', line
			else:
				self.edits.append( EditDecision(line, fps) )
				edits_m2.append( self.edits[-1] )
		
		if has_m2:
			# Group indexes
			i = 0
			for item in edits_m2:
				if isinstance(item, M2):
					item.index = i
					i += 1
				else:
					# not an m2
					i = 0
			
			# Set total group indexes
			for item in reversed(edits_m2):
				if isinstance(item, M2):
					if tot_m2 == -1:
						tot_m2 = item.index + 1
					
					item.tot = tot_m2
				else:
					# not an m2
					tot_m2 = -1
			
			
			for i, item in enumerate(edits_m2):
				if isinstance(item, M2):
					# make a list of all items that match the m2's reel name
					edits_m2_tmp = [item_tmp for item_tmp in edits_m2 if (isinstance(item, M2) or item_tmp.reel == item.reel)]
					
					# get the new index
					i_tmp = edits_m2_tmp.index(item)
					
					# Seek back to get the edit.
					edit = edits_m2[i_tmp-item.tot]
					
					# Note, docs say time should also match with edit start time
					# but from final cut pro, this seems not to be the case
					if not isinstance(edit, EditDecision):
						print "ERROR!", 'M2 incorrect'
					else:
						edit.m2 = item
			
			
		return True
	
	def testOverlap(self, edit_test):
		recIn= int(edit_test.recIn)
		recOut= int(edit_test.recOut)
		
		for edit in self.edits:
			if edit is edit_test:
				break
			
			recIn_other= int(edit.recIn)
			recOut_other= int(edit.recOut)
			
			if recIn_other < recIn < recOut_other:
				return True
			if recIn_other < recOut < recOut_other:
				return True
			
			if recIn < recIn_other < recOut:
				return True
			if recIn < recOut_other < recOut:
				return True
			
		return False
	
	def getReels(self):
		reels = {}
		for edit in self.edits:
			reels.setdefault(edit.reel, []).append(edit)
		
		return reels
		
		
		
# from DNA
SEQ_IMAGE=		0
SEQ_META=		1
SEQ_SCENE=		2
SEQ_MOVIE=		3
SEQ_RAM_SOUND=	4
SEQ_HD_SOUND=            5
SEQ_MOVIE_AND_HD_SOUND=  6

SEQ_EFFECT=		8
SEQ_CROSS=		8
SEQ_ADD=		9
SEQ_SUB=		10
SEQ_ALPHAOVER=	11
SEQ_ALPHAUNDER=	12
SEQ_GAMCROSS=	13
SEQ_MUL=		14
SEQ_OVERDROP=	15
SEQ_PLUGIN=		24
SEQ_WIPE=		25
SEQ_GLOW=		26
SEQ_TRANSFORM=	27
SEQ_COLOR=		28
SEQ_SPEED=		29

# Blender spesific stuff starts here
import bpy
import Blender

def scale_meta_speed(seq, mov, scale):
	# Add an effect
	speed= seq.new((SEQ_SPEED, mov,), 199, mov.channel+1)
	speed.speedEffectFrameBlending = True
	meta= seq.new([mov, speed], 199, mov.channel)

	if scale >= 1.0:
		mov.endStill = int(mov.length * (scale - 1.0))
	else:
		speed.speedEffectGlobalSpeed = 1.0/scale
		meta.endOffset = mov.length - int(mov.length*scale)

	speed.update()
	meta.update()
	return meta

def apply_dissolve_ipo(mov, blendin):
	len_disp = float(mov.endDisp - mov.startDisp)
	
	if len_disp <= 0.0:
		print 'Error, strip is zero length'
		return
	
	mov.ipo= ipo= bpy.data.ipos.new("fade", "Sequence")
	icu= ipo.addCurve('Fac')
	
	icu.interpolation= Blender.IpoCurve.InterpTypes.LINEAR
	icu.append((0, 0))
	icu.append(((int(blendin)/len_disp) * 100, 1))
	
	if mov.type not in (SEQ_HD_SOUND, SEQ_RAM_SOUND):
		mov.blendMode = Blender.Scene.Sequence.BlendModes.ALPHAOVER


def replace_ext(path, ext):
	return path[:path.rfind('.')+1] + ext

def load_edl(filename, reel_files, reel_offsets):
	'''
	reel_files - key:reel <--> reel:filename
	'''
	
	# For test file
	# frame_offset = -769
	
	
	sce= bpy.data.scenes.active
	fps= sce.render.fps
	
	elist= EditList()
	if not elist.parse(filename, fps):
		return 'Unable to parse "%s"' % filename
	# elist.clean()
	
	
	seq= sce.sequence
	
	track= 0 
	
	edits = elist.edits[:]
	# edits.sort(key = lambda edit: int(edit.recIn))
	
	prev_edit = None
	for edit in edits:
		print edit
		frame_offset = reel_offsets[edit.reel]
		
		
		src_start=	int(edit.srcIn) + frame_offset
		src_end=	int(edit.srcOut) + frame_offset
		src_length=	src_end-src_start
		
		rec_start=	int(edit.recIn) + 1
		rec_end=	int(edit.recOut) + 1
		rec_length=	rec_end-rec_start
		
		# print src_length, rec_length, src_start
		
		if edit.m2 != None:	scale = fps/float(edit.m2.fps)
		else:					scale = 1.0
		
		unedited_start= rec_start - src_start
		offset_start = src_start - int(src_start*scale) # works for scaling up AND down
		
		if edit.transition_type == TRANSITION_CUT and (not elist.testOverlap(edit)):
			track = 1
		
		strip= None
		final_strips = []
		if edit.reel.lower()=='bw':
			strip= seq.new((0,0,0), rec_start, track+1)
			strip.length= rec_length # for color its simple
			final_strips.append(strip)
		else:

			path_full = reel_files[edit.reel]
			path_fileonly= path_full.split('/')[-1].split('\\')[-1] # os.path.basename(full)
			path_dironly= path_full[:-len(path_fileonly)] # os.path.dirname(full)
			
			if edit.edit_type & EDIT_VIDEO: #and edit.transition_type == TRANSITION_CUT:	
				
				try:
					strip= seq.new((path_fileonly, path_dironly, path_full, 'movie'), unedited_start + offset_start, track+1)
				except:
					return 'Invalid input for movie'
				
				# Apply scaled rec in bounds
				if scale != 1.0:
					meta = scale_meta_speed(seq, strip, scale)
					final_strip = meta
				else:
					final_strip = strip
				
				
				final_strip.update()
				final_strip.startOffset= rec_start - final_strip.startDisp
				final_strip.endOffset= rec_end- final_strip.endDisp
				final_strip.update()
				final_strip.endOffset += (final_strip.endDisp - rec_end)
				final_strip.update()
				
				
				if edit.transition_duration:
					if not prev_edit:
						print "Error no previous strip"
					else:
						new_end = rec_start + int(edit.transition_duration)
						for other in prev_edit.custom_data:
							if other.type != SEQ_HD_SOUND and other.type != SEQ_RAM_SOUND:
								other.endOffset += (other.endDisp - new_end)
								other.update()
						
				# Apply disolve
				if edit.transition_type == TRANSITION_DISSOLVE:
					apply_dissolve_ipo(final_strip, edit.transition_duration)
				
				if edit.transition_type == TRANSITION_WIPE:
					other_track = track + 2
					for other in prev_edit.custom_data:
						if other.type != SEQ_HD_SOUND and other.type != SEQ_RAM_SOUND:
							
							strip_wipe= seq.new((SEQ_WIPE, other, final_strip), 1, other_track)
							
							if edit.wipe_type == WIPE_0:
								strip_wipe.wipeEffectAngle =  90
							else:
								strip_wipe.wipeEffectAngle = -90
							
							other_track += 1
							
				
				
				# strip.endOffset= strip.length - int(edit.srcOut)
				#end_offset= (unedited_start+strip.length) - end
				# print start, end, end_offset
				#strip.endOffset = end_offset
				
				# break
				# print strip
				
				final_strips.append(final_strip)
			
				
			if edit.edit_type & (EDIT_AUDIO | EDIT_AUDIO_STEREO | EDIT_VIDEO_AUDIO):
				
				if scale == 1.0:  # TODO - scaled audio
					
					try:
						strip= seq.new((path_fileonly, path_dironly, path_full, 'audio_hd'), unedited_start + offset_start, track+6)
					except:
						
						# See if there is a wave file there
						path_full_wav = replace_ext(path_full, 'wav')
						path_fileonly_wav = replace_ext(path_fileonly, 'wav')
						
						#try:
						strip= seq.new((path_fileonly_wav, path_dironly, path_full_wav, 'audio_hd'), unedited_start + offset_start, track+6)
						#except:
						#	return 'Invalid input for audio'
					
					final_strip = strip
					
					# Copied from above
					final_strip.update()
					final_strip.startOffset= rec_start - final_strip.startDisp
					final_strip.endOffset= rec_end- final_strip.endDisp
					final_strip.update()
					final_strip.endOffset += (final_strip.endDisp - rec_end)
					final_strip.update()
					
					if edit.transition_type == TRANSITION_DISSOLVE:
						apply_dissolve_ipo(final_strip, edit.transition_duration)
					
					final_strips.append(final_strip)
				
			# strip= seq.new((0.6, 0.6, 0.6), start, track+1)
			
		if final_strips:
			for strip in final_strips:
				# strip.length= length
				final_strip.name = edit.asName()
				edit.custom_data[:]= final_strips
				# track = not track
				prev_edit = edit
			track += 1
			
		#break
	
	
	def recursive_update(s):
		s.update(1)
		for s_kid in s:
			recursive_update(s_kid)
			
		
	for s in seq:
		recursive_update(s)
	
	return ''



#load_edl('/fe/edl/EP30CMXtrk1.edl') # /tmp/test.edl
#load_edl('/fe/edl/EP30CMXtrk2.edl') # /tmp/test.edl
#load_edl('/fe/edl/EP30CMXtrk3.edl') # /tmp/test.edl
#load_edl('/root/vid/rush/blender_edl.edl', ['/root/vid/rush/rushes3.avi',]) # /tmp/test.edl




# ---------------------- Blender UI part
from Blender import Draw, Window
import BPyWindow

if 0:
	DEFAULT_FILE_EDL = '/root/vid/rush/blender_edl.edl'
	DEFAULT_FILE_MEDIA = '/root/vid/rush/rushes3_wav.avi'
	DEFAULT_FRAME_OFFSET = -769
else:
	DEFAULT_FILE_EDL = ''
	DEFAULT_FILE_MEDIA = ''
	DEFAULT_FRAME_OFFSET = 0

B_EVENT_IMPORT = 1
B_EVENT_RELOAD = 2
B_EVENT_FILESEL_EDL = 3
B_EVENT_NOP = 4

B_EVENT_FILESEL = 100 # or greater

class ReelItemUI(object):
	__slots__ = 'filename_but', 'offset_but', 'ui_text'
	def __init__(self):
		self.filename_but =	Draw.Create(DEFAULT_FILE_MEDIA)
		self.offset_but =	Draw.Create(DEFAULT_FRAME_OFFSET)
		self.ui_text = ''
	


REEL_UI = {}	# reel:ui_string


#REEL_FILENAMES = {}		# reel:filename
#REEL_OFFSETS = {}		# reel:filename

PREF = {}

PREF['filename'] = Draw.Create(DEFAULT_FILE_EDL)
PREF['reel_act'] = ''

def edl_reload():
	Window.WaitCursor(1)
	filename = PREF['filename'].val
	sce= bpy.data.scenes.active
	fps= sce.render.fps
	
	elist= EditList()
	
	if filename:
		if not elist.parse(filename, fps):
			Draw.PupMenu('Error%t|Could not open the file "' + filename + '"')
		reels = elist.getReels()
	else:
		reels = {}
	
	REEL_UI.clear()
	for reel_key, edits in reels.iteritems():
		
		if reel_key == 'bw':
			continue
		
		flag = 0
		for edit in edits:
			flag |= edit.edit_type
		
		reel_item = REEL_UI[reel_key] = ReelItemUI()
		
		reel_item.ui_text = '%s (%s): ' % (reel_key, editFlagsToText(flag))
		
	Window.WaitCursor(0)

def edl_set_path(filename):
	PREF['filename'].val = filename
	edl_reload()
	Draw.Redraw()
	
def edl_set_path_reel(filename):
	REEL_UI[PREF['reel_act']].filename_but.val = filename
	Draw.Redraw()

def edl_reel_keys():
	reel_keys = REEL_UI.keys()
	
	if 'bw' in reel_keys:
		reel_keys.remove('bw')
	
	reel_keys.sort()
	return reel_keys

def edl_draw():
	
	MARGIN = 4
	rect = BPyWindow.spaceRect()
	but_width = int((rect[2]-MARGIN*2)/4.0) # 72
	# Clamp
	if but_width>100: but_width = 100
	but_height = 17
	
	x=MARGIN
	y=rect[3]-but_height-MARGIN
	xtmp = x
	
	
	
	# ---------- ---------- ---------- ----------
	Blender.Draw.BeginAlign()
	PREF['filename'] =	Draw.String('edl path: ', B_EVENT_RELOAD, xtmp, y, (but_width*3)-20, but_height, PREF['filename'].val, 256, 'EDL Path'); xtmp += (but_width*3)-20;
	Draw.PushButton('..',	B_EVENT_FILESEL_EDL, xtmp, y, 20, but_height,	'Select an EDL file'); xtmp += 20;
	Blender.Draw.EndAlign()
	
	Draw.PushButton('Reload',	B_EVENT_RELOAD, xtmp + MARGIN, y, but_width - MARGIN, but_height,	'Read the ID Property settings from the active curve object'); xtmp += but_width;
	y-=but_height + MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	reel_keys = edl_reel_keys()
	
	
	
	if reel_keys:						text = 'Reel file list...'
	elif PREF['filename'].val == '':	text = 'No EDL loaded.'
	else:								text = 'No reels found!'

	Draw.Label(text, xtmp + MARGIN, y, but_width*4, but_height); xtmp += but_width*4;
	
	y-=but_height + MARGIN
	xtmp = x
	
	# ---------- ---------- ---------- ----------

	
	for i, reel_key in enumerate(reel_keys):
		reel_item = REEL_UI[reel_key]
		
		Blender.Draw.BeginAlign()
		REEL_UI[reel_key].filename_but = Draw.String(reel_item.ui_text, B_EVENT_NOP, xtmp, y, (but_width*3)-20, but_height, REEL_UI[reel_key].filename_but.val, 256, 'Select the reel path'); xtmp += (but_width*3)-20;
		Draw.PushButton('..',	B_EVENT_FILESEL + i, xtmp, y, 20, but_height,	'Media path to use for this reel'); xtmp += 20;
		Blender.Draw.EndAlign()
		
		reel_item.offset_but= Draw.Number('ofs:',	B_EVENT_NOP, xtmp + MARGIN, y, but_width - MARGIN, but_height, reel_item.offset_but.val, -100000, 100000,	'Start offset in frames when applying timecode'); xtmp += but_width - MARGIN;
		
		y-=but_height + MARGIN
		xtmp = x
	
	# ---------- ---------- ---------- ----------
	
	Draw.PushButton('Import CMX-EDL Sequencer Strips',	B_EVENT_IMPORT, xtmp + MARGIN, MARGIN, but_width*4 - MARGIN, but_height,	'Load the EDL file into the sequencer'); xtmp += but_width*4;
	y-=but_height + MARGIN
	xtmp = x


def edl_event(evt, val):
	pass

def edl_bevent(evt):
	
	if evt == B_EVENT_NOP:
		pass
	elif evt == B_EVENT_IMPORT:
		'''
		Load the file into blender with UI settings
		'''
		filename = PREF['filename'].val

		reel_files = {}
		reel_offsets = {}
		
		for reel_key, reel_item in REEL_UI.iteritems():
			reel_files[reel_key] = reel_item.filename_but.val
			reel_offsets[reel_key] = reel_item.offset_but.val
		
		error = load_edl(filename, reel_files, reel_offsets)
		if error != '':
			Draw.PupMenu('Error%t|' + error)
		else:
			Window.RedrawAll()
		
	elif evt == B_EVENT_RELOAD:
		edl_reload()
		Draw.Redraw()
		
	elif evt == B_EVENT_FILESEL_EDL:
		filename = PREF['filename'].val
		if not filename: filename = Blender.sys.join(Blender.sys.expandpath('//'), '*.edl')
			
		Window.FileSelector(edl_set_path, 'Select EDL', filename)
		
	elif evt >= B_EVENT_FILESEL:
		reel_keys = edl_reel_keys()
		reel_key = reel_keys[evt - B_EVENT_FILESEL]
		
		filename = REEL_UI[reel_key].filename_but.val
		if not filename: filename = Blender.sys.expandpath('//')

		PREF['reel_act'] = reel_key # so file set path knows which one to set
		Window.FileSelector(edl_set_path_reel, 'Reel Media', filename)
		
		

if __name__ == '__main__':
	Draw.Register(edl_draw, edl_event, edl_bevent)
	edl_reload()

