# $Id$
#
# --------------------------------------------------------------------------
# BPyNMesh.py version 0.1
# --------------------------------------------------------------------------
# helper functions to be used by other scripts
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


# --------------------------------------------------------------------------
# "Apply size and rotation" function by Jonas Petersen
# --------------------------------------------------------------------------
# This function does (hopefully) exactly what the
# "Apply size and rotation" command does (CTRL-A in Object Mode).
def ApplySizeAndRotation(obj): 
	if obj.getType() != "Mesh": return
	if obj.SizeX==1.0 and obj.SizeY==1.0 and obj.SizeZ==1.0 and obj.RotX == 0.0 and obj.RotY == 0.0 and obj.RotZ == 0.0: return
	mesh = obj.getData()
	matrix = obj.matrix
	v = [0,0,0]
	for vert in mesh.verts:
		co = vert.co
		v[0] = co[0]*matrix[0][0] + co[1]*matrix[1][0] + co[2]*matrix[2][0] 
		v[1] = co[0]*matrix[0][1] + co[1]*matrix[1][1] + co[2]*matrix[2][1]
		v[2] = co[0]*matrix[0][2] + co[1]*matrix[1][2] + co[2]*matrix[2][2]
		co[0], co[1], co[2] = v
	obj.SizeX = obj.SizeY = obj.SizeZ = 1.0
	obj.RotX = obj.RotY = obj.RotZ = 0.0
	mesh.update()

