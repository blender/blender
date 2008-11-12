#!BPY

"""
Name: 'Raw Faces (.raw)...'
Blender: 242
Group: 'Import'
Tooltip: 'Import Raw Triangle File Format (.raw)'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "blenderartists.org",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script imports Raw Triangle File format files to Blender.

The raw triangle format is very simple; it has no verts or faces lists.
It's just a simple ascii text file with the vertices of each triangle
listed on each line. There were some very old utilities (when the PovRay
forum was in existence on CompuServe) that preformed operations on these
files.

Usage:<br>
	Execute this script from the "File->Import" menu and choose a Raw file to
open.

Notes:<br>
	Generates the standard verts and faces lists, but without duplicate
verts. Only *exact* duplicates are removed, there is no way to specify a
tolerance.
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | April 28, 2002                                          |
# | Read and write RAW Triangle File Format (*.raw)         |
# +---------------------------------------------------------+

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

import Blender

# ================================
# === Read RAW Triangle Format ===
# ================================
def read(filename):
	t = Blender.sys.time()
	file = open(filename, "rb")

	# Collect data from RAW format
	def line_to_face(line):
		# Each triplet is an xyz float
		line_split= map(float, line.split())
		if len(line_split)==9: # Tri
			f1, f2, f3, f4, f5, f6, f7, f8, f9 = line_split
			return [(f1, f2, f3), (f4, f5, f6), (f7, f8, f9)]
		if len(line_split)==12: # Quad
			f1, f2, f3, f4, f5, f6, f7, f8, f9, A, B, C = line_split
			return [(f1, f2, f3), (f4, f5, f6), (f7, f8, f9), (A, B, C)]
	
	faces = [ line_to_face(line) for line in file.readlines()]
	file.close()
	
	# Generate verts and faces lists, without duplicates
	verts = []
	coords = {}
	index = 0
	
	for f in faces:
		if f: # Line might be blank
			for i, v in enumerate(f):
				try:
					f[i]= coords[v]
				except:
					f[i]= coords[v] = index
					index += 1
					verts.append(v)
	
	me= Blender.Mesh.New()
	me.verts.extend(verts)
	me.faces.extend(faces)
	
	
	scn= Blender.Scene.GetCurrent()
	for obj in scn.objects:
		obj.sel= 0
	
	me.name= Blender.sys.splitext(Blender.sys.basename(filename))[0]
	ob = scn.objects.new(me)
	Blender.Redraw()
	
	print 'Successfully imported "%s" in %.4f seconds' % (Blender.sys.basename(filename), Blender.sys.time()-t)


def main():
	Blender.Window.FileSelector(read, 'RAW Import', Blender.sys.makename(ext='.raw'))

if __name__=='__main__':
	main()
