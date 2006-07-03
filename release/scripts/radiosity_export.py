#!BPY

"""
Name: 'Radiosity (.radio)...'
Blender: 232
Group: 'Export'
Tooltip: 'Export selected mesh (with vertex colors) to Radiosity File Format (.radio)'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "elysiun",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script exports meshes to Radiosity file format.

The Radiosity file format is my own personal format. I created it to
learn how meshes and vertex colors were stored. See IO-Examples.zip, the
example *.radio files on my web page.

Usage:<br>
	Select meshes to be exported and run this script from "File->Export" menu.

Notes:<br>
	Before exporting to .radio format, the mesh must have vertex colors.
Here's how to assign them:

1. Use radiosity!

2. Set up lights and materials, select a mesh, switch the drawing mode
to "textured," press the VKEY.

3. Press the VKEY and paint manually.

4. Use a custom script to calculate and apply simple diffuse shading and
specular highlights to the vertex colors.

5. The Videoscape format also allows vertex colors to be specified.
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
import BPyMesh

try:
	import struct
	NULL_COLOR= struct.pack('<BBBB', 255,255,255,255)
except:
	struct= None



# ================================
# ====== Write Radio Format ======
# ================================
def write(filename):
	if not filename.lower().endswith('.radio'):
		filename += '.radio'
	
	scn= Blender.Scene.GetCurrent()
	object= scn.getActiveObject()
	if not object:
		Blender.Draw.PupMenu('Error%t|Select 1 active object')
		return
	objname= object.name
	
	file = open(filename, 'wb')
	mesh = BPyMesh.getMeshFromObject(object, None, True, False, scn)
	if not mesh:
		Blender.Draw.PupMenu('Error%t|Could not get mesh data from active object')
		return
		
	mesh.transform(object.matrixWorld)
	
	
	
	start = Blender.sys.time()
	file = open(filename, "wb")

	if not mesh.faceUV:
		mesh.faceUV= 1
		#message = 'Please assign vertex colors before exporting "%s"|object was not saved' % object.name
		#Blender.Draw.PupMenu("ERROR%t|"+message)
		#return

	# === Object Name ===
	file.write(struct.pack("<h", len(objname)))
	file.write(struct.pack("<"+`len(objname)`+"s", objname))

	# === Vertex List ===
	file.write(struct.pack("<l", len(mesh.verts)))
	for v in mesh.verts:
		#if not i%100 and meshtools.show_progress:
		#	Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Writing Verts")
		x, y, z = tuple(v.co)
		file.write(struct.pack("<fff", x, y, z))

	# === Face List ===
	file.write(struct.pack('<l', len(mesh.faces)))
	#for i in range(len(mesh.faces)):
	for f in mesh.faces:
		#if not i%100 and meshtools.show_progress:
		#	Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Faces")
		
		file.write(struct.pack('<b', len(f) ))
		#for j in range(len(mesh.faces[i].v)):
		for v in f.v:
			file.write(struct.pack('<h', v.index))
		
		f_col= f.col
		for c in f_col: # .col always has a length of 4
			file.write(struct.pack('<BBBB', c.r, c.g, c.b, c.a))
		
		# Write the last values out again. always have 4 cols even for tris
		if len(f_col) == 3:
			file.write(NULL_COLOR)
	
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	end = Blender.sys.time()
	message = 'Successfully exported "%s" in %.2f seconds' % (Blender.sys.basename(filename), end-start)
	meshtools.print_boxed(message)


def main():
	if not struct:
		Blender.Draw.PupMenu('ERROR%t|Error: you need a full Python install to run this script')
		return
	
	Blender.Window.FileSelector(write, 'Export Radio', Blender.sys.makename(ext='.radio'))

if __name__ == '__main__':
	main()
