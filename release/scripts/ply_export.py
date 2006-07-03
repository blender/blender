#!BPY

"""
Name: 'Stanford PLY (*.ply)...'
Blender: 241
Group: 'Export'
Tooltip: 'Export active object to Stanford PLY format'
"""

import Blender
import BPyMesh

__author__ = "Bruce Merry"
__version__ = "0.9"
__bpydoc__ = """\
This script exports Stanford PLY files from Blender. It supports per-vertex
normals and per-face colours and texture coordinates.
"""

# Copyright (C) 2004, 2005: Bruce Merry, bmerry@cs.uct.ac.za
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

# Vector rounding se we can use as keys
def rvec3d(v):	return round(v.x, 6), round(v.y, 6), round(v.z, 6)
def rvec2d(v):	return round(v.x, 6), round(v.y, 6)
def rcol(c):	return c.r, c.g, c.b

def file_callback(filename):
	if not filename.lower().endswith('.ply'):
		filename += '.ply'
	
	scn= Blender.Scene.GetCurrent()
	object= scn.getActiveObject()
	if not object:
		Blender.Draw.PupMenu('Error%t|Select 1 active object')
		return
	
	file = open(filename, 'wb')
	
	mesh = BPyMesh.getMeshFromObject(object, None, True, False, scn)
	if not mesh:
		Blender.Draw.PupMenu('Error%t|Could not get mesh data from active object')
		return
		
	mesh.transform(object.matrixWorld)
	
	if mesh.vertexColors or mesh.faceUV:
		mesh.faceUV= 1
		have_uv= True
	else:
		have_uv= False
	
	
	verts = [] # list of dictionaries
	vdict = {} # (index, normal, uv) -> new index
	for i, f in enumerate(mesh.faces):
		f_col= f.col
		f_uv= f.uv
		for j, v in enumerate(f.v):
			index = v.index
			
			if have_uv:
				vdata = v.co, v.no
				uv= f_uv[j]
				col= f_col[j]
				vdata = v.co, v.no, uv, col
				key = index, rvec3d(v.no), rcol(col), rvec2d(uv)
			else:
				vdata = v.co, v.no
				key = index, rvec3d(v.no)
				
			if not vdict.has_key(key):
				vdict[key] = len(verts);
				verts.append(vdata)
	file.write('ply\n')
	file.write('format ascii 1.0\n')
	file.write('Created by Blender3D %s - www.blender.org, source file: %s\n' % (Blender.Get('version'), Blender.Get('filename').split('/')[-1].split('\\')[-1] ))
	
	file.write('element vertex %d\n' % len(verts))
	
	file.write('property float32 x\n')
	file.write('property float32 y\n')
	file.write('property float32 z\n')
	file.write('property float32 nx\n')
	file.write('property float32 ny\n')
	file.write('property float32 nz\n')
	
	if have_uv:
		file.write('property float32 s\n')
		file.write('property float32 t\n')
		file.write('property uint8 red\n')
		file.write('property uint8 green\n')
		file.write('property uint8 blue\n')
	
	file.write('element face %d\n' % len(mesh.faces))
	file.write('property list uint8 int32 vertex_indices\n')
	file.write('end_header\n')

	for i, v in enumerate(verts):
		file.write('%.6f %.6f %.6f ' % tuple(v[0])) # co
		file.write('%.6f %.6f %.6f ' % tuple(v[1])) # no
		
		if have_uv:
			file.write('%.6f %.6f %u %u %u' % (v[2].x, v[2].y, v[3].r, v[3].g, v[3].b)) # uv/col
		file.write('\n')
	
	for (i, f) in enumerate(mesh.faces):
		file.write('%d ' % len(f))
		
		f_col= f.col
		f_uv= f.uv
		
		for j, v in enumerate(f.v):
			index = v.index
			
			if have_uv:
				uv= f_uv[j]
				col= f_col[j]
				key = index, rvec3d(v.no), rcol(col), rvec2d(uv)
			else:
				key = index, rvec3d(v.no)
			
			file.write('%d ' % vdict[key])
		file.write('\n')
	file.close()



def main():
	Blender.Window.FileSelector(file_callback, 'PLY Export', Blender.sys.makename(ext='.ply'))


if __name__=='__main__':
	main()
