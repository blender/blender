#!/usr/bin/python

# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
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
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

import struct

# In Blender, selecting scenes in the databrowser (shift+f4) will tag for rendering.

# This struct wont change according to ton.
# Note that the size differs on 32/64bit
'''
typedef struct BHead {
	int code, len;
	void *old;
	int SDNAnr, nr;
} BHead;
'''


def read_blend_rend_chunk(path):
	file = open(path, 'rb')
	
	if file.read(len('BLENDER')) != 'BLENDER':
		return []
	
	# 
	if file.read(1) == '-':
		is64bit = True
	else: # '_'
		is64bit = False

	if file.read(1) == 'V':
		isBigEndian = True # ppc
	else: # 'V'
		isBigEndian = False # x86
	
	
	# Now read the bhead chunk!!!
	file.read(3) # skip the version
	
	scenes = []
	
	while file.read(4) == 'REND':
	
		if is64bit:		sizeof_bhead = sizeof_bhead_left = 24 # 64bit
		else:			sizeof_bhead = sizeof_bhead_left = 20 # 32bit
	
		sizeof_bhead_left -= 4
		
		if isBigEndian:	rend_length = struct.unpack('>i', file.read(4))[0]
		else:			rend_length = struct.unpack('<i', file.read(4))[0]
		
		sizeof_bhead_left -= 4
		
		# We dont care about the rest of the bhead struct
		file.read(sizeof_bhead_left)
		
		# Now we want the scene name, start and end frame. this is 32bites long
		
		if isBigEndian:	start_frame, end_frame = struct.unpack('>2i', file.read(8))
		else:			start_frame, end_frame = struct.unpack('<2i', file.read(8))
		
		scene_name = file.read(24)
		scene_name = scene_name[ : scene_name.index('\0') ]
		
		scenes.append( (start_frame, end_frame, scene_name) )
	return scenes

def main():
	import sys
	for arg in sys.argv[1:]:
		if arg.lower().endswith('.blend'):
			print read_blend_rend_chunk(arg)

if __name__ == '__main__':
	main()

