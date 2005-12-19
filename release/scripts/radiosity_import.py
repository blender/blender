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
#import time

try:
	import struct
except:
	msg = "Error: you need a full Python install to run this script."
	meshtools.print_boxed(msg)
	Blender.Draw.PupMenu("ERROR%t|"+msg)

# ===============================
# ====== Read Radio Format ======
# ===============================
def read(filename):
	#start = time.clock()
	file = open(filename, "rb")
	mesh = Blender.NMesh.GetRaw()
	#mesh.addMaterial(Blender.Material.New())

	# === Object Name ===
	namelen, = struct.unpack("<h",  file.read(2))
	objname, = struct.unpack("<"+`namelen`+"s", file.read(namelen))

	# === Vertex List ===
	numverts, = struct.unpack("<l", file.read(4))
	for i in range(numverts):
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numverts, "Reading Verts")
		x, y, z = struct.unpack("<fff", file.read(12))
		mesh.verts.append(Blender.NMesh.Vert(x, y, z))

	# === Face List ===
	numfaces, = struct.unpack("<l", file.read(4))
	for i in range(numfaces):
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numfaces, "Reading Faces")

		face = Blender.NMesh.Face()
		numfaceverts, = struct.unpack("<b", file.read(1))

		for j in range(numfaceverts):
			index, = struct.unpack("<h", file.read(2))
			face.v.append(mesh.verts[index])

		for j in range(4):
			r, g, b, a = struct.unpack("<BBBB", file.read(4))
			vertexcolor = Blender.NMesh.Col(r, g, b, a)
			face.col.append(vertexcolor)

		if len(face.v) == 3:
			face.uv = [ (0,0), (0,1), (1,1) ]
		else:
			face.uv = [ (0,0), (0,1), (1,1), (1,0) ]

		face.mode = 0
		mesh.faces.append(face)

	# ->tools.create_mesh(verts, faces, objname):
	Blender.NMesh.PutRaw(mesh, objname)
	object = Blender.Object.GetSelected()
	object[0].name=objname
	# ->tools.create_mesh(verts, faces, objname):

	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully imported " + Blender.sys.basename(filename)# + seconds
	meshtools.print_boxed(message)

def fs_callback(filename):
	read(filename)

Blender.Window.FileSelector(fs_callback, "Import Radio")
