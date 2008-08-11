#!BPY

"""
Name: 'Stanford PLY (*.ply)...'
Blender: 241
Group: 'Export'
Tooltip: 'Export active object to Stanford PLY format'
"""

import bpy
import Blender
from Blender import Mesh, Scene, Window, sys, Image, Draw
import BPyMesh

__author__ = "Bruce Merry"
__version__ = "0.93"
__bpydoc__ = """\
This script exports Stanford PLY files from Blender. It supports normals, 
colours, and texture coordinates per face or per vertex.
Only one mesh can be exported at a time.
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
#
# Updated on Aug 11, 2008 by Campbell Barton
#    - added 'comment' prefix to comments - Needed to comply with the PLY spec.
#
# Updated on Jan 1, 2007 by Gabe Ghearing
#    - fixed normals so they are correctly smooth/flat
#    - fixed crash when the model doesn't have uv coords or vertex colors
#    - fixed crash when the model has vertex colors but doesn't have uv coords
#    - changed float32 to float and uint8 to uchar for compatibility
# Errata/Notes as of Jan 1, 2007
#    - script exports texture coords if they exist even if TexFace isn't selected (not a big deal to me)
#    - ST(R) should probably be renamed UV(T) like in most PLY files (importer needs to be updated to take either)
#
# Updated on Jan 3, 2007 by Gabe Ghearing
#    - fixed "sticky" vertex UV exporting
#    - added pupmenu to enable/disable exporting normals, uv coords, and colors
# Errata/Notes as of Jan 3, 2007
#    - ST(R) coords should probably be renamed UV(T) like in most PLY files (importer needs to be updated to take either)
#    - edges should be exported since PLY files support them
#    - code is getting spaghettish, it should be refactored...
#


def rvec3d(v):	return round(v[0], 6), round(v[1], 6), round(v[2], 6)
def rvec2d(v):	return round(v[0], 6), round(v[1], 6)

def file_callback(filename):
	
	if not filename.lower().endswith('.ply'):
		filename += '.ply'
	
	scn= bpy.data.scenes.active
	ob= scn.objects.active
	if not ob:
		Blender.Draw.PupMenu('Error%t|Select 1 active object')
		return
	
	file = open(filename, 'wb')
	
	EXPORT_APPLY_MODIFIERS = Draw.Create(1)
	EXPORT_NORMALS = Draw.Create(1)
	EXPORT_UV = Draw.Create(1)
	EXPORT_COLORS = Draw.Create(1)
	#EXPORT_EDGES = Draw.Create(0)
	
	pup_block = [\
	('Apply Modifiers', EXPORT_APPLY_MODIFIERS, 'Use transformed mesh data.'),\
	('Normals', EXPORT_NORMALS, 'Export vertex normal data.'),\
	('UVs', EXPORT_UV, 'Export texface UV coords.'),\
	('Colors', EXPORT_COLORS, 'Export vertex Colors.'),\
	#('Edges', EXPORT_EDGES, 'Edges not connected to faces.'),\
	]
	
	if not Draw.PupBlock('Export...', pup_block):
		return
	
	is_editmode = Blender.Window.EditMode()
	if is_editmode:
		Blender.Window.EditMode(0, '', 0)
	
	Window.WaitCursor(1)
	
	EXPORT_APPLY_MODIFIERS = EXPORT_APPLY_MODIFIERS.val
	EXPORT_NORMALS = EXPORT_NORMALS.val
	EXPORT_UV = EXPORT_UV.val
	EXPORT_COLORS = EXPORT_COLORS.val
	#EXPORT_EDGES = EXPORT_EDGES.val
	
	mesh = BPyMesh.getMeshFromObject(ob, None, EXPORT_APPLY_MODIFIERS, False, scn)
	
	if not mesh:
		Blender.Draw.PupMenu('Error%t|Could not get mesh data from active object')
		return
	
	mesh.transform(ob.matrixWorld)
	
	faceUV = mesh.faceUV
	vertexUV = mesh.vertexUV
	vertexColors = mesh.vertexColors
	
	if (not faceUV) and (not vertexUV):		EXPORT_UV = False
	if not vertexColors:					EXPORT_COLORS = False
	
	if not EXPORT_UV:						faceUV = vertexUV = False
	if not EXPORT_COLORS:					vertexColors = False
	
	# incase
	color = uvcoord = uvcoord_key = normal = normal_key = None
	
	verts = [] # list of dictionaries
	# vdict = {} # (index, normal, uv) -> new index
	vdict = [{} for i in xrange(len(mesh.verts))]
	vert_count = 0
	for i, f in enumerate(mesh.faces):
		smooth = f.smooth
		if not smooth:
			normal = tuple(f.no)
			normal_key = rvec3d(normal)
			
		if faceUV:			uv = f.uv
		if vertexColors:	col = f.col
		for j, v in enumerate(f):
			if smooth:
				normal=		tuple(v.no)
				normal_key = rvec3d(normal)
			
			if faceUV:
				uvcoord=	tuple(uv[j])
				uvcoord_key = rvec2d(uvcoord)
			elif vertexUV:
				uvcoord=	tuple(v.uvco)
				uvcoord_key = rvec2d(uvcoord)
			
			if vertexColors:	color=		col[j].r, col[j].g, col[j].b
			
			
			key = normal_key, uvcoord_key, color
			
			vdict_local = vdict[v.index]
			
			if (not vdict_local) or (not vdict_local.has_key(key)):
				vdict_local[key] = vert_count;
				verts.append( (tuple(v.co), normal, uvcoord, color) )
				vert_count += 1
	
	
	file.write('ply\n')
	file.write('format ascii 1.0\n')
	file.write('comment Created by Blender3D %s - www.blender.org, source file: %s\n' % (Blender.Get('version'), Blender.Get('filename').split('/')[-1].split('\\')[-1] ))
	
	file.write('element vertex %d\n' % len(verts))
	
	file.write('property float x\n')
	file.write('property float y\n')
	file.write('property float z\n')
	if EXPORT_NORMALS:
		file.write('property float nx\n')
		file.write('property float ny\n')
		file.write('property float nz\n')
	
	if EXPORT_UV:
		file.write('property float s\n')
		file.write('property float t\n')
	if EXPORT_COLORS:
		file.write('property uchar red\n')
		file.write('property uchar green\n')
		file.write('property uchar blue\n')
	
	file.write('element face %d\n' % len(mesh.faces))
	file.write('property list uchar uint vertex_indices\n')
	file.write('end_header\n')

	for i, v in enumerate(verts):
		file.write('%.6f %.6f %.6f ' % v[0]) # co
		if EXPORT_NORMALS:
			file.write('%.6f %.6f %.6f ' % v[1]) # no
		
		if EXPORT_UV:
			file.write('%.6f %.6f ' % v[2]) # uv
		if EXPORT_COLORS:
			file.write('%u %u %u' % v[3]) # col
		file.write('\n')
	
	for (i, f) in enumerate(mesh.faces):
		file.write('%d ' % len(f))
		smooth = f.smooth
		if not smooth: no = rvec3d(f.no)
		
		if faceUV:			uv = f.uv
		if vertexColors:	col = f.col
		for j, v in enumerate(f):
			if f.smooth:		normal=		rvec3d(v.no)
			else:				normal=		no
			if faceUV:			uvcoord=	rvec2d(uv[j])
			elif vertexUV:		uvcoord=	rvec2d(v.uvco)
			if vertexColors:	color=		col[j].r, col[j].g, col[j].b
			
			file.write('%d ' % vdict[v.index][normal, uvcoord, color])
			
		file.write('\n')
	file.close()
	
	if is_editmode:
		Blender.Window.EditMode(1, '', 0)

def main():
	Blender.Window.FileSelector(file_callback, 'PLY Export', Blender.sys.makename(ext='.ply'))

if __name__=='__main__':
	main()
