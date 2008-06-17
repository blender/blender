#!BPY

"""
Name: 'Import Plane from Image'
Blender: 245
Group: 'Add'
Tooltip: 'Import a plane topology from a 2d Image'
"""

__author__ = "André Pinto"
__url__ = ["www.blender.org"]
__version__ = "2008-06-06"

__bpydoc__ = """\
This script extracts a plane from an image.
"""

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
# The Original Code is Copyright (C) Blender Foundation.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): André Pinto
#
# ***** END GPL LICENSE BLOCK *****

import Blender, bpy

def matrix(dima, dimb):
	return [[0 for b in range(dimb)] for a in range(dima)]

def dotProduct(a):
	return reduce(lambda x, y: x + y*y, a) 

def manhattanDistance(a, b):
	return reduce(lambda x, y: x + abs(y[0]-y[1]), zip(a, b), 0)

def xrange_tuple(low, upper):
	for i in xrange( low[0], upper[0]):
		for j in xrange( low[1], upper[1]):
			yield (i,j)


# For now simply decompose in a triangle fan
def DecomposePolygon(poly):
	for i in xrange(2, len(poly), 1):
		yield [ poly[0], poly[i-1], poly[i] ]

def Expand3dCoordsFrom2d(coords):
	for c in coords:
		yield ( c[0] , c[1], 0 )


# For now return the full image
def ExtractSections(image):
	x_samples = 250
	y_samples = 250

	mdim = max( image.size )
	dx = float(image.size[0]) / x_samples
	dy = float(image.size[1]) / y_samples

	offset = [ -0.5*i for i in image.size]

	def scale(a):
		return (a[0] + offset[0] , a[1] + offset[1])
	
	sx = 0
#(int) (float(image.size[0]) / (x_samples))
	sy = 0
#(int) (float(image.size[1]) / (y_samples))

	def get( center ):
		best = None
		for pos in xrange_tuple( (max(0, center[0]-sx),max(0, center[1]-sy)) , (min(image.size[0], center[0]+sx)+1, min(image.size[1], center[1]+sy)+1 )):
				if dotProduct(image.getPixelHDR(pos[0],pos[1])) <= 1:
					if best == None or manhattanDistance(center, pos) < manhattanDistance(center, best):
						best = pos
		return best


	pos = matrix(x_samples, y_samples)
	sdx = dx
	sdy = dy
	for a in xrange(x_samples):
		for b in xrange(y_samples):
			pos[a][b] = get(((int)(a*sdx),(int)(b*sdy)))

	for a in xrange(x_samples-1):
		for b in xrange(y_samples-1):
			arround = [ (a,b) , (a+1,b), (a+1,b+1), (a,b+1) ]

			valid = [ pos[c[0]][c[1]] for c in arround if pos[c[0]][c[1]] != None]
			if len(valid) >= 3:
				yield map( scale, valid )
				

def ImportPlaneFromImage(image, mesh):

	new_verts = []
	new_faces = []

	vert_dict = {}

	def getVertex(vert):
		if vert not in vert_dict:
			new_verts.append( (vert[0], vert[1], 0) )
			vert_dict[ vert ] = len( new_verts )-1

		return vert_dict[ vert ]

	for poly in ExtractSections(image):
		offset = len(new_verts)

		poly = map( getVertex, poly )
		if len(poly) == 4:
			new_faces.append( [poly[0], poly[1], poly[2],poly[3]] )
		if len(poly) == 3:
			new_faces.append( [ poly[0], poly[1], poly[2] ] )

	# perform a single extend, extend is O( N )
	mesh.verts.extend( new_verts )
	mesh.faces.extend( new_faces )




def load_image(filename):
	print "Loading ",filename
	#for now create a new mesh
	mesh = bpy.data.meshes.new('Plane')
	Blender.Scene.GetCurrent().objects.new(mesh)

	image = Blender.Image.Load(filename)
	ImportPlaneFromImage(image, mesh)
	Blender.Redraw()
	

Blender.Window.FileSelector(load_image, "Load Image")
#use the current image on the image editor? or ask the user what image to load
#image = Blender.Image.GetCurrent()
#load_image("/home/darkaj/develop/blender/shrinkwrap/road.png")


