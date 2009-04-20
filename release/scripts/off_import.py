#!BPY

"""
Name: 'DEC Object File Format (.off)...'
Blender: 242
Group: 'Import'
Tooltip: 'Import DEC Object File Format (*.off)'
"""

__author__ = "Anthony D'Agostino (Scorpius), Campbell Barton (Ideasman)"
__url__ = ("blender", "blenderartists.org",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script imports DEC Object File Format files to Blender.

The DEC (Digital Equipment Corporation) OFF format is very old and
almost identical to Wavefront's OBJ. I wrote this so I could get my huge
meshes into Moonlight Atelier. (DXF can also be used but the file size
is five times larger than OFF!) Blender/Moonlight users might find this
script to be very useful.

Usage:<br>
	Execute this script from the "File->Import" menu and choose an OFF file to
open.

Notes:<br>
	UV Coordinate support has been added. - Scorpius
	FGON support has been added. - Cam
	New Mesh module now used. - Cam
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


# =============================
# ====== Read OFF Format ======
# =============================
def read(filename):
	start = Blender.sys.time()
	file = open(filename, "rb")

	verts = [] # verts and uvs are aligned
	uvs = []
	
	faces = []
	
	# === OFF Header ===
	# Skip the comments
	offheader= file.readline()
	while offheader.startswith('#') or offheader.lower().startswith('off'):
		offheader = file.readline()
	
	numverts, numfaces, numedges= map(int, offheader.split())
	if offheader.find('ST') >= 0:
		has_uv = True
		Vector= Mathutils.Vector
	else:
		has_uv = False

	# === Vertex List ===
	for i in xrange(numverts):
		if has_uv:
			x, y, z, u, v = map(float, file.readline().split())
			uvs.append(Vector(u, v))
		else:
			x, y, z = map(float, file.readline().split())
		verts.append((x, y, z))

	# === Face List ===
	def fan_face(face):
		# 'Elp, Only fan fill- if were keen we could use our trusty BPyMesh.ngon function
		# So far I havnt seen any big ngons in on off file - Cam
		return [ (face[0], face[i-1], face[i]) for i in xrange(2,len(face))]
	
	for i in xrange(numfaces):
		line = file.readline().split() # ignore the first value, its just the face count but we can work that out anyway
		
		# appends all the indicies in reverse order except 0
		# xrange(len(line)-1, -1, -1) # normal reverse loop
		# xrange(len(line)-1, 0, -1) # ignoring index 0 because its only a count
		# face= [int(line[j]) for j in xrange(len(line)-1, 0, -1)]
		
		# Some OFF files have floats on the end of the face. what are these for?
		face= [int(line[j]) for j in xrange(int(line[0]), 0, -1)]
		
		
		if len(face)>4:
			faces.extend( fan_face(face) )
		else:
			faces.append(face)
			
	scn= Blender.Scene.GetCurrent()
	name= filename.split('/')[-1].split('\\')[-1].split('.')[0]
	me= Blender.Mesh.New(name)
	me.verts.extend(verts)
	me.faces.extend(faces)
	
	# Now edges if we have them, render fgon
	if numedges:
		FGON_FLAG= Blender.Mesh.EdgeFlags.FGON
			
		edge_dict= {}
		# Set all edges to be fgons by default
		for ed in me.edges:
			ed.flag |= FGON_FLAG
			edge_dict[ed.key]= ed
		
		# Now make known edges visible
		has_edges = True
		for i in xrange(numedges):
			try:
				i1,i2= file.readline().split()
			except:
				has_edges = False
				break # some files dont define edges :/
			
			i1= int(i1)
			i2= int(i2)		
			if i1>i2:
				i1,i2= i2,i1
			
			# We know this edge is seen so unset the fgon flag
			edge_dict[i1,i2].flag &= ~FGON_FLAG
		
		if not has_edges:
			# dang, we'v turned all the edges into fgons and then the file didnt define any edges.
			# oh well, just enable them all
			for ed in me.edges:
				ed.flag &= ~FGON_FLAG
	
	# Assign uvs from vert index
	if has_uv:
		for f in me.faces:
			f_uv= f.uv
			for i, v in enumerate(f): # same as f.v
				f_uv[i]= uvs[v.index]
	
	for ob in scn.objects:
		ob.sel=0
	
	scn.objects.active = scn.objects.new(me, name)
	Blender.Window.RedrawAll()
	print 'Off "%s" imported in %.4f seconds.' % (name, Blender.sys.time()-start)
	

if __name__=='__main__':
	Blender.Window.FileSelector(read, 'Import OFF', '*.off')
