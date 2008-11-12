#!BPY

"""
Name: 'DEC Object File Format (.off)...'
Blender: 232
Group: 'Export'
Tooltip: 'Export selected mesh to DEC Object File Format (*.off)'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "blenderartists.org",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script exports meshes to DEC Object File Format.

The DEC (Digital Equipment Corporation) OFF format is very old and
almost identical to Wavefront's OBJ. I wrote this so I could get my huge
meshes into Moonlight Atelier. (DXF can also be used but the file size
is five times larger than OFF!) Blender/Moonlight users might find this
script to be very useful.

Usage:<br>
	Select meshes to be exported and run this script from "File->Export" menu.

Notes:<br>
	Only exports a single selected mesh.
"""

# $Id:
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | February 3, 2001                                        |
# | Read and write Object File Format (*.off)               |
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
import BPyMessages

# Python 2.3 has no reversed.
try:
	reversed
except:
	def reversed(l): return l[::-1]

# ==============================
# ====== Write OFF Format ======
# ==============================
def write(filename):
	file = open(filename, 'wb')
	scn= Blender.Scene.GetCurrent()
	object= scn.objects.active
	if not object or object.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return
	
	Blender.Window.WaitCursor(1)
	mesh = object.getData(mesh=1)

	# === OFF Header ===
	file.write('OFF\n')
	file.write('%d %d %d\n' % (len(mesh.verts), len(mesh.faces), 0))

	# === Vertex List ===
	for i, v in enumerate(mesh.verts):
		file.write('%.6f %.6f %.6f\n' % tuple(v.co))

	# === Face List ===
	for i, f in enumerate(mesh.faces):
		file.write('%i' % len(f))
		for v in reversed(f.v):
			file.write(' %d' % v.index)
		file.write('\n')
	
	file.close()
	Blender.Window.WaitCursor(0)
	message = 'Successfully exported "%s"' % Blender.sys.basename(filename)
	

def fs_callback(filename):
	if not filename.lower().endswith('.off'): filename += '.off'
	write(filename)

Blender.Window.FileSelector(fs_callback, "Export OFF", Blender.sys.makename(ext='.off'))
