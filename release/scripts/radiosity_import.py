#!BPY

"""
Name: 'Radiosity (.radio)...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Radiosity File Format (.radio) with vertex colors'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "elysiun",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script imports Radiosity files to Blender.

The Radiosity file format is my own personal format. I created it to
learn how meshes and vertex colors were stored. See IO-Examples.zip, the
example *.radio files on my web page.

Usage:<br>
	Execute this script from the "File->Import" menu and choose a Radiosity
file to open.
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | April 11, 2002                                          |
# | Read and write Radiosity File Format (*.radio)          |
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

import Blender, meshtools

try:
	import struct
except:
	struct= None

# ===============================
# ====== Read Radio Format ======
# ===============================
def read(filename):
	start = Blender.sys.time()
	file = open(filename, "rb")
	mesh = Blender.NMesh.GetRaw()
	#mesh.addMaterial(Blender.Material.New())
	
	NULL_UV3= [ (0,0), (0,1), (1,1) ]
	NULL_UV4= [ (0,0), (0,1), (1,1), (1,0) ]
	
	
	# === Object Name ===
	namelen, = struct.unpack("<h",  file.read(2))
	objname, = struct.unpack("<"+`namelen`+"s", file.read(namelen))

	# === Vertex List ===
	Vert= Blender.NMesh.Vert
	numverts, = struct.unpack("<l", file.read(4))
	
	# Se we can run in a LC
	def _vert_():
		x,y,z= struct.unpack('<fff', file.read(12))
		return Vert(x, y, z)
	
	mesh.verts= [_vert_() for i in xrange(numverts)]
	del _vert_
	
	
	# === Face List ===
	Face= Blender.NMesh.Face
	Col= Blender.NMesh.Col
	numfaces, = struct.unpack("<l", file.read(4))
	for i in xrange(numfaces):
		#if not i%100 and meshtools.show_progress:
		#	Blender.Window.DrawProgressBar(float(i)/numfaces, "Reading Faces")

		
		numfaceverts, = struct.unpack("<b", file.read(1))
		
		
		face = Face(\
		 [\
		 mesh.verts[\
		  struct.unpack("<h", file.read(2))[0]] for j in xrange(numfaceverts)\
		 ]
		)
		
		face.col= [ Col(r, g, b, a) \
		for j in xrange(4)\
		for r,g,b,a in ( struct.unpack("<BBBB", file.read(4)), )]
		
		if len(face) == 3:
			face.uv = NULL_UV3
		else:
			face.uv = NULL_UV4

		
		face.mode = 0
		mesh.faces.append(face)

	scn= Blender.Scene.GetCurrent()
	for obj in scn.getChildren():
		obj.sel= 0
	
	obj= Blender.Object.New('Mesh', objname)
	mesh.name= objname
	obj.link(mesh)
	scn.link(obj)
	obj.sel= 1
	obj.Layers= scn.Layers
	
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	end = Blender.sys.time()
	message = 'Successfully imported "%s" in %.2f seconds' % (Blender.sys.basename(filename), end-start)
	meshtools.print_boxed(message)


def main():
	if not struct:
		Blender.Draw.PupMenu('ERROR%t|Error: you need a full Python install to run this script')
		return
	
	Blender.Window.FileSelector(read, "Import Radio", Blender.sys.makename(ext='.radio'))

if __name__ == '__main__':
	main()
