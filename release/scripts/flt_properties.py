#!BPY
# flt_properties.py. For setting default OpenFLight ID property types
# Copyright (C) 2007 Blender Foundation
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

__bpydoc__ ="""\
Utility functions and data defintions used by OpenFlight I/O and tool scripts. OpenFlight is a
registered trademark of MultiGen-Paradigm, Inc.
"""


import struct 

bitsLSB = [2147483648]
for i in xrange(31):
	bitsLSB.append(bitsLSB[-1]/2)
bitsRSB = bitsLSB[:]
bitsRSB.reverse()

def pack_color(col):
	return struct.pack('>B',col[3]) + struct.pack('>B',col[2]) + struct.pack('>B',col[1]) + struct.pack('>B',col[0])
	
def unpack_color(col):
	string = struct.pack('>I', col)
	r = struct.unpack('>B',string[3:4])
	g = struct.unpack('>B',string[2:3])
	b = struct.unpack('>B',string[1:2])
	a = struct.unpack('>B',string[0:1])
	return [r,g,b,a] 

def reverse_bits(len,num):
	bitbucket = list()
	rval = 0
	
	for i in xrange(len):
		if num & bitsRSB[i]:
			bitbucket.append(1)
		else:
			bitbucket.append(0)
	
	bitbucket.reverse()
	
	for i, bit in enumerate(bitbucket):
		if bit:
			rval |= bitsLSB[i]
	
	return rval
	
	
opcode_name = { 0: 'db',
				1: 'head',
				2: 'grp',
				4: 'obj',
				5: 'face',
				10: 'push',
				11: 'pop',
				14: 'dof',
				19: 'push sub',
				20: 'pop sub',
				21: 'push ext',
				22: 'pop ext',
				23: 'cont',
				31: 'comment',
				32: 'color pal',
				33: 'long id',
				49: 'matrix',
				50: 'vector',
				52: 'multi-tex',
				53: 'uv lst',
				55: 'bsp',
				60: 'rep',
				61: 'inst ref',
				62: 'inst def',
				63: 'ext ref',
				64: 'tex pal',
				67: 'vert pal',
				68: 'vert w col',
				69: 'vert w col & norm',
				70: 'vert w col, norm & uv',
				71: 'vert w col & uv',
				72: 'vert lst',
				73: 'lod',
				74: 'bndin box',
				76: 'rot edge',
				78: 'trans',
				79: 'scl',
				80: 'rot pnt',
				81: 'rot and/or scale pnt',
				82: 'put',
				83: 'eyepoint & trackplane pal',
				84: 'mesh',
				85: 'local vert pool',
				86: 'mesh prim',
				87: 'road seg',
				88: 'road zone',
				89: 'morph vert lst',
				90: 'link pal',
				91: 'snd',
				92: 'rd path',
				93: 'snd pal',
				94: 'gen matrix',
				95: 'txt',
				96: 'sw',
				97: 'line styl pal',
				98: 'clip reg',
				100: 'ext',
				101: 'light src',
				102: 'light src pal',
				103: 'reserved',
				104: 'reserved',
				105: 'bndin sph',
				106: 'bndin cyl',
				107: 'bndin hull',
				108: 'bndin vol cntr',
				109: 'bndin vol orient',
				110: 'rsrvd',
				111: 'light pnt',
				112: 'tex map pal',
				113: 'mat pal',
				114: 'name tab',
				115: 'cat',
				116: 'cat dat',
				117: 'rsrvd',
				118: 'rsrvd',
				119: 'bounding hist',
				120: 'rsrvd',
				121: 'rsrvd',
				122: 'push attrib',
				123: 'pop attrib',
				124: 'rsrvd',
				125: 'rsrvd',
				126: 'curv',
				127: 'road const',
				128: 'light pnt appear pal',
				129: 'light pnt anim pal',
				130: 'indexed lp',
				131: 'lp sys',
				132: 'indx str',
				133: 'shdr pal'}


typecodes = ['c','C','s','S','i','I','f','d','t']

FLT_GRP =	2
FLT_OBJ =	4
FLT_LOD =	73
FLT_XRF =	63
FLT_DOF =	14
FLT_ILP =	111
FLT_DB =	1
FLT_FCE =	5

#not actual opcodes
FLT_NUL =	0
FLT_EXP =	-1

#valid childtypes for each FLT node type
FLT_CHILDTYPES = { 
	FLT_GRP : [111,2,73,4,14,63],
	FLT_OBJ : [111],
	FLT_LOD : [111,2,73,4,14,63],
	FLT_XRF : [],
	FLT_DOF : [111,2,73,4,14,63],
	FLT_ILP : []
}

#List of nodes that can have faces as children
FLT_FACETYPES = [
	FLT_GRP,
	FLT_OBJ,
	FLT_LOD,
	FLT_DOF
]

def write_prop(fw,type,value,length):
	if type == 'c':
		fw.write_char(value)
	elif type == 'C':
		fw.write_uchar(value)
	elif type == 's':
		fw.write_short(value)
	elif type == 'S':
		fw.write_ushort(value)
	elif type == 'i':
		fw.write_int(value)
	elif type == 'I':
		#NOTE!:
		#there is no unsigned int type in python, but we can only store signed ints in ID props
		newvalue = struct.unpack('>I', struct.pack('>i', value))[0]
		fw.write_uint(newvalue)
	elif type == 'd':
		fw.write_double(value)
	elif type == 'f':
		fw.write_float(value)
	elif type == 't':
		fw.write_string(value,length)

def read_prop(fw,type,length):
	rval = None
	if type == 'c':
		rval = fw.read_char()
	elif type == 'C':
		rval = fw.read_uchar()
	elif type == 's':
		rval = fw.read_short()
	elif type == 'S':
		rval = fw.read_ushort()
	elif type == 'i':
		rval = fw.read_int()
	elif type == 'I':
		rval = fw.read_uint()
	elif type == 'd':
		rval = fw.read_double()
	elif type == 'f':
		rval = fw.read_float()
	elif type == 't':
		rval = fw.read_string(length)
	return rval
	
	
FLTExt = {
	'3t8!id' : 'Ext',
	'4t8!sid' : '',
	'5c!reserved': 0,
	'6c!revision' : 0,
	'7S!recordcode' : 0
}
FLTGroup = {
	'3t8!id' : 'G',
	'4s!priority' : 0, 
	'5s!reserved1' : 0, 
	'6i!flags' : 0, 
	'7s!special1' : 0,
	'8s!special2' : 0, 
	'9s!significance' : 0,
	'10c!layer code' : 0,
	'11c!reserved2' : 0,
	'12i!reserved3' : 0,
	'13i!loop count' : 0,
	'14f!loop duration' : 0,
	'15f!last frame duration' : 0
}
FLTGroupDisplay = [5,11,12]

FLTObject = {
	'3t8!id' : 'O',
	'4I!flags' : 0,
	'5s!priority' : 0,
	'6S!transp' : 0,
	'7s!SFX1' : 0,
	'8s!SFX2' : 0,
	'9s!significance' : 0,
	'10s!reserved' : 0
}
FLTObjectDisplay = [10]

FLTLOD = {
	'3t8!id' : 'L',
	'4i!reserved' : 0,
	'5d!switch in' : 0.0,
	'6d!switch out' : 0.0,
	'7s!sfx ID1' : 0,
	'8s!sfx ID2' : 0,
	'9I!flags' : 0,
	'10d!X co' : 0.0,
	'11d!Y co' : 0.0,
	'12d!Z co' : 0.0,
	'13d!Transition' : 0.0,
	'14d!Sig Size' : 0.0
}
FLTLODDisplay = [4]

FLTInlineLP = {
	'3t8!id' : 'Lp',
	'4s!smc' : 0,
	'5s!fid' : 0,
	'6C!back color: a' : 255,
	'7C!back color: b' : 255,
	'8C!back color: g' : 255,
	'9C!back color: r' : 255,
	'10i!display mode' : 0,
	'11f!intensity' : 1.0,
	'12f!back intensity' : 0.0,
	'13f!minimum defocus' : 0.0,
	'14f!maximum defocus' : 1.0,
	'15i!fading mode' : 0,
	'16i!fog punch mode' : 0,
	'17i!directional mode' : 1,
	'18i!range mode' : 0,
	'19f!min pixel size' : 1.0,
	'20f!max pixel size' : 1024,
	'21f!actual size' : 0.25,
	'22f!trans falloff pixel size' : 0.25,
	'23f!trans falloff exponent' : 1.0,
	'24f!trans falloff scalar' : 1.0,
	'25f!trans falloff clamp' : 1.0,
	'26f!fog scalar' : 0.25,
	'27f!fog intensity' : 1.0,
	'28f!size threshold' : 0.1,
	'29i!directionality' : 0,
	'30f!horizontal lobe angle' : 180.0,
	'31f!vertical lobe angle' : 180.0,
	'32f!lobe roll angle' : 0.0,
	'33f!dir falloff exponent' : 1.0,
	'34f!dir ambient intensity' : 0.1,
	'35f!anim period' : 2,
	'36f!anim phase' : 0,
	'37f!anim enabled' : 1.0,
	'38f!significance' : 0.0,
	'39i!draw order' : 0,
	'40I!flags' : 277004288, 
	'41f!roti' : 0,
	'42f!rotj' : 0,
	'43f!rotk' : 1.0
}

FLTInlineLPDisplay = [35,36,37,41,42,43]

FLTXRef = {
	'3t200!filename' : '', #we dont actually use this value on export
	'4i!reserved' : 0,
	'5I!flag' : -478150656,
	'6s!bbox' : 0,
	'7s!reserved' : 0
}

FLTXRefDisplay = [4,7,3]

FLTDOF = {
	'3t8!id' : 'D',		
	'4i!reserved' : 0,
	'5d!ORIGX' : 0.0,
	'6d!ORIGY' : 0.0, 
	'7d!ORIGZ' : 0.0,
	'8d!XAXIS-X' : 10.0,
	'9d!XAXIS-Y' : 0.0,
	'10d!XAXIS-Z' : 0.0,
	'11d!XYPLANE-X' : 0.0,
	'12d!XYPLANE-Y' : 10.0,
	'13d!XZPLANE-Z' : 0.0,
	'14d!ZMIN' : 0.0,
	'15d!ZMAX' : 0.0,
	'16d!ZCUR' : 0.0,
	'17d!ZSTEP' : 0.0,
	'18d!YMIN' : 0.0,
	'19d!YMAX' : 0.0,
	'20d!YCUR' : 0.0,
	'21d!YSTEP' : 0.0,
	'22d!XMIN' : 0.0,
	'23d!XMAX' : 0.0,
	'24d!XCUR' : 0.0,
	'25d!XSTEP' : 0.0,
	'26d!PITCH-MIN' : 0.0,
	'27d!PITCH-MAX' : 0.0,
	'28d!PITCH-CUR' : 0.0,
	'29d!PITCH-STEP' : 0.0,
	'30d!ROLL-MIN' : 0.0,
	'31d!ROLL-MAX' : 0.0,
	'32d!ROLL-CUR' : 0.0,
	'33d!ROLL-STEP' : 0.0,
	'34d!YAW-MIN' : 0.0,
	'35d!YAW-MAX' : 0.0,
	'36d!YAW-CUR' : 0.0,
	'37d!YAW-STEP' : 0.0,
	'38d!ZSIZE-MIN' : 0.0,
	'39d!ZSIZE-MAX' : 0.0,
	'40d!ZSIZE-CUR' : 1.0,
	'41d!ZSIZE-STEP' : 0.0,
	'42d!YSIZE-MIN' : 0.0,
	'43d!YSIZE-MAX' : 0.0,
	'44d!YSIZE-CUR' : 1.0,
	'45d!YSIZE-STEP' : 0.0,
	'46d!XSIZE-MIN' : 0.0,
	'47d!XSIZE-MAX' : 0.0,
	'48d!XSIZE-CUR' : 1.0,
	'49d!XSIZE-STEP' : 0.0,
	'50I!FLAG' : 1897582,
	'51i!reserved2' : 0
}

FLTDOFDisplay = [4]

FLTImage = {
	'3i!RealU Direction' : 0, 
	'4i!RealV Direction' : 0, 
	'5i!UpX' : 0, 
	'6i!UpY' : 0, 
	'7i!File Format' : 0, 
	'8i!Min Filter' : 6, 
	'9i!Mag Filter' : 1, 
	'10i!Wrap' : 0, 
	'11i!WrapU' : 0, 
	'12i!WrapV' : 0, 
	'13i!Modified' : 0,
	'14i!PivotX' : 0, 
	'15i!PivotY' : 0, 
	'16i!Enviorment' : 0, 
	'17i!WhiteAlpha' : 0, 
	'18i!reserved1' : 0,
	'19i!reserved2' : 0,
	'20i!reserved3' : 0,
	'21i!reserved4' : 0,
	'22i!reserved5' : 0,
	'23i!reserved6' : 0,
	'24i!reserved7' : 0,
	'25i!reserved8' : 0,
	'26i!reserved9' : 0,
	'27d!RealU Direction' : 0, 
	'28d!RealV Direction' : 0, 
	'29i!Origin' : 0, 
	'30i!Kernel no.' : 0, 
	'31i!Internal Format' : 0, 
	'32i!External Format' : 0, 
	'33i!MipMap Filter?' : 0, 
	'34f!MMF1' : 0.0, 
	'35f!MMF2' : 0.0, 
	'36f!MMF3' : 0.0, 
	'37f!MMF4' : 0.0, 
	'38f!MMF5' : 0.0, 
	'39f!MMF6' : 0.0, 
	'40f!MMF7' : 0.0, 
	'41f!MMF8' : 0.0, 
	'42i!Tex CPs?' : 0, 
	'43f!LOD0 CP' : 0.0, 
	'44f!Scale0 CP' : 0.0, 
	'45f!LOD1 CP' : 0.0, 
	'46f!Scale1 CP' : 0.0, 
	'47f!LOD2 CP' : 0.0, 
	'48f!Scale2 CP' : 0.0, 
	'49f!LOD3 CP' : 0.0, 
	'50f!Scale3 CP' : 0.0, 
	'51f!LOD4 CP' : 0.0, 
	'52f!Scale4 CP' : 0.0, 
	'53f!LOD5 CP' : 0.0, 
	'54f!Scale5 CP' : 0.0, 
	'55f!LOD6 CP' : 0.0, 
	'56f!Scale6 CP' : 0.0, 
	'57f!LOD7 CP' : 0.0, 
	'58f!Scale7 CP' : 0.0, 
	'59f!Control Clamp' : 0.0, 
	'60i!Mag Alpha Filter' : 0, 
	'61i!Mag Color Filter' : 0, 
	'62f!reserved10' : 0,
	'63f!reserved11' : 0,
	'64f!reserved12' : 0,
	'65f!reserved13' : 0,
	'66f!reserved14' : 0,
	'67f!reserved15' : 0,
	'68f!reserved16' : 0,
	'69f!reserved17' : 0,
	'70f!reserved18' : 0,
	'71d!Lambert Central' : 0.0, 
	'72d!Lambert Upper' : 0.0, 
	'73d!Lambert Lower' : 0.0, 
	'74d!reserved19' : 0,
	'75f!reserved20' : 0,
	'76f!reserved21' : 0,
	'77f!reserved22' : 0,
	'78f!reserved23' : 0,
	'79f!reserved24' : 0,
	'80i!Tex Detail?' : 0, 
	'81i!Tex J' : 0, 
	'82i!Tex K' : 0, 
	'83i!Tex M' : 0, 
	'84i!Tex N' : 0, 
	'85i!Tex Scramble' : 0, 
	'86i!Tex Tile?' : 0, 
	'87f!Tex Tile LLU' : 0.0, 
	'88f!Tex Tile LLV' : 0.0, 
	'89f!Tex Tile URU' : 0.0,
	'90f!Tex Tile URV' : 0.0, 
	'91i!Projection' : 0, 
	'92i!Earth Model' : 0, 
	'93i!reserved25' : 0,
	'94i!UTM Zone' : 0, 
	'95i!Image Origin' : 0,
	'96i!GPU' : 0, 
	'97i!reserved26' : 0,
	'98i!reserved27' : 0,
	'99i!GPU Hemi' : 0, 
	'100i!reserved41' : 0,
	'101i!reserved42' : 0,
	'102i!reserved43' : 0,
	'103i!Cubemap' : 0, 
	'104t588!reserved44' : '',
	'105t512!Comments' : '', 
	'106i!reserved28' : 0,
	'107i!reserved29' : 0,
	'108i!reserved30' : 0,
	'109i!reserved31' : 0,
	'110i!reserved32' : 0,
	'111i!reserved33' : 0,
	'112i!reserved34' : 0,
	'113i!reserved35' : 0,
	'114i!reserved36' : 0,
	'115i!reserved37' : 0,
	'116i!reserved38' : 0,
	'117i!reserved39' : 0,
	'118i!reserved40' : 0,
	'119i!reserved45' : 0,
	'120i!Format Version' : 0, 
	'121i!GPU num' : 0,
}

FLTImageDisplay = [18,19,29,21,22,23,24,25,26,62,63,64,65,66,67,68,69,70,74,75,76,77,78,79,93,97,98,102,114]

FLTHeader = {
	'3t8!id' : 'db',
	'4i!version' : 1620,
	'5i!editversion' : 0,
	'6t32!date' : 0,
	'7s!NGID' : 0,
	'8s!NLID' : 0,
	'9s!NOID' : 0,
	'10s!NFID' : 0,
	'11s!UMULT' : 1,
	'12c!units' : 0,
	'13c!set white' : 0,
	'14I!flags' : 0x80000000,
	'15i!reserved1' : 0,
	'16i!reserved2' : 0,
	'17i!reserved3' : 0,
	'18i!reserved4' : 0,
	'19i!reserved5' : 0,
	'20i!reserved6' : 0,
	'21i!projection type' : 0,
	'22i!reserved7' : 0,
	'23i!reserved8' : 0,
	'24i!reserved9' : 0,
	'25i!reserved10' : 0,
	'26i!reserved11' : 0,
	'27i!reserved12' : 0,
	'28i!reserved13' : 0,
	'29s!NDID' : 0,
	'30s!vstore' : 1,
	'31i!origin' : 0,
	'32d!sw x' : 0,
	'33d!sw y' : 0,
	'34d!dx' : 0,
	'35d!dy' : 0,
	'36s!NSID' : 0,
	'37s!NPID' : 0,
	'38i!reserved14' : 0,
	'39i!reserved15' : 0,
	'40s!NCID' : 0,
	'41s!NTID' : 0,
	'42s!NBID' : 0,
	'43s!NWID' : 0,
	'44i!reserved14' : 0,
	'45d!sw lat' : 0,
	'46d!sw lon' : 0,
	'47d!ne lat' : 0,
	'48d!ne lon' : 0,
	'49d!origin lat' : 0,
	'50d!origin lon' : 0,
	'51d!lambert lat1' : 0,
	'52d!lambert lat2' : 0,
	'53s!NLSID' : 0,
	'54s!NLPID' : 0,
	'55s!NRID' : 0,
	'56s!NCATID' : 0,
	'57s!reserved15' : 0,
	'58s!reserved16' : 0,
	'59s!reserved17' : 0,
	'60s!reserved18' : 0,
	'61i!ellipsoid model' : 1,
	'62s!NAID' : 0,
	'63s!NCVID' : 0,
	'64s!utm zone' : 0,
	'65t6!reserved19' : 0,
	'66d!dz' : 0,
	'67d!radius' : 0,
	'68S!NMID' : 0,
	'69S!NLPSID' : 0,
	'70i!reserved20' : 0,
	'71d!major axis' : 0,
	'72d!minor axis' : 0,
}

FLT_Records = {
		2 : FLTGroup,
		4 : FLTObject,
		73 : FLTLOD,
		63 : FLTXRef,
		14 : FLTDOF,
		1 : FLTHeader,
		111 : FLTInlineLP,
		100 : FLTExt,
		'Image'	: FLTImage
}

def process_recordDefs(): 
	records = dict()
	for record in FLT_Records:
		props = dict()
		for prop in FLT_Records[record]:
			position = ''
			slice = 0
			(format,name) = prop.split('!')
			for i in format:
				if i not in typecodes:
					position = position + i
					slice = slice + 1
				else:
					break
			type = format[slice:]
			length = type[1:] 
			if len(length) == 0:
				length = 1
			else:
				type = type[0]
				length = int(length)
			
			props[int(position)] = (type,length,prop)
		records[record] = props
	return records


