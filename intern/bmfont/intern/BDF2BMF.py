#!/usr/bin/python

# ***** BEGIN GPL LICENSE BLOCK *****
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

HELP_TXT = \
'''
Convert BDF pixmap fonts into C++ files Blender can read.
Use to replace bitmap fonts or add new ones.

Usage
	python bdf2bmf.py -name=SomeName myfile.bdf

Blender currently supports fonts with a maximum width of 8 pixels.
'''

# -------- Simple BDF parser
import sys
def parse_bdf(f, MAX_CHARS=256):
	lines = [l.strip().upper().split() for l in f.readlines()]

	is_bitmap = False
	dummy = {'BITMAP':[]}
	char_data = [dummy.copy() for i in xrange(MAX_CHARS)]
	context_bitmap = []

	for l in lines:
		if l[0]=='ENCODING':		enc = int(l[1])
		elif l[0]=='BBX':			bbx = [int(c) for c in l[1:]]
		elif l[0]=='DWIDTH':		dwidth = int(l[1])
		elif l[0]=='BITMAP':		is_bitmap = True
		elif l[0]=='ENDCHAR':
			if enc < MAX_CHARS:
				char_data[enc]['BBX'] = bbx
				char_data[enc]['DWIDTH'] = dwidth
				char_data[enc]['BITMAP'] = context_bitmap
				
			context_bitmap = []
			enc = bbx = None
			is_bitmap = False
		else:
			# None of the above, Ok, were reading a bitmap
			if is_bitmap and enc < MAX_CHARS:
				context_bitmap.append( int(l[0], 16) )
	
	return char_data
# -------- end simple BDF parser

def bdf2cpp_name(path):
	return path.split('.')[0] + '.cpp'

def convert_to_blender(bdf_dict, font_name, origfilename, MAX_CHARS=256):
	
	# first get a global width/height, also set the offsets
	xmin = ymin =  10000000
	xmax = ymax = -10000000
	
	bitmap_offsets = [-1] * MAX_CHARS
	bitmap_tot = 0
	for i, c in enumerate(bdf_dict):
		if c.has_key('BBX'):
			bbx = c['BBX']
			xmax = max(bbx[0], xmax)
			ymax = max(bbx[1], ymax)
			xmin = min(bbx[2], xmin)
			ymin = min(bbx[3], ymin)
			
			bitmap_offsets[i] = bitmap_tot
			bitmap_tot += len(c['BITMAP'])
		
		c['BITMAP'].reverse()
	
	# Now we can write. Ok if we have no .'s in the path.
	f = open(bdf2cpp_name(origfilename), 'w')
	
	f.write('''
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BMF_FontData.h"

#include "BMF_Settings.h"
''')
	
	f.write('#if BMF_INCLUDE_%s\n\n' % font_name.upper())
	f.write('static unsigned char bitmap_data[]= {')
	newline = 8
	
	for i, c in enumerate(bdf_dict):
	
		for cdata in c['BITMAP']:
			# Just formatting
			newline+=1
			if newline >= 8:
				newline = 0
				f.write('\n\t')
			# End formatting
			
			f.write('0x%.2hx,' % cdata) # 0x80 <- format
			
	f.write("\n};\n")
	
	f.write("BMF_FontData BMF_font_%s = {\n" % font_name)
	f.write('\t%d, %d,\n' % (xmin, ymin))
	f.write('\t%d, %d,\n' % (xmax, ymax))
	
	f.write('\t{\n')
	

	for i, c in enumerate(bdf_dict):
		if bitmap_offsets[i] == -1 or c.has_key('BBX') == False:
			f.write('\t\t{0,0,0,0,0, -1},\n')
		else:
			bbx = c['BBX']
			f.write('\t\t{%d,%d,%d,%d,%d, %d},\n' % (bbx[0], bbx[1], -bbx[2], -bbx[3], c['DWIDTH'], bitmap_offsets[i]))
	
	f.write('''
	},
	bitmap_data
};

#endif
''')

def main():
	# replace "[-name=foo]" with  "[-name] [foo]"
	args = []
	for arg in sys.argv:
		for a in arg.replace('=', ' ').split():
			args.append(a)
	
	name = 'untitled'
	done_anything = False
	for i, arg in enumerate(args):
		if arg == '-name':
			if i==len(args)-1:
				print 'no arg given for -name, aborting'
				return
			else:
				name = args[i+1]
		
		elif arg.lower().endswith('.bdf'):
			try:
				f = open(arg)
				print '...Writing to:', bdf2cpp_name(arg)
			except:
				print 'could not open "%s", aborting' % arg
			
			
			bdf_dict = parse_bdf(f)
			convert_to_blender(bdf_dict, name, arg)
			done_anything = True
	
	if not done_anything:
		print HELP_TXT
		print '...nothing to do'

if __name__ == '__main__':
	main()
	
