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

def makelist(a):
	res = []
	for i in a:
		res.append(i)
	return res

def dotProduct(a):
	sum = 0.0
	for i in a:
		sum += i*i
	return sum

# For now simply decompose in a triangle fan
def DecomposePolygon(poly):
	for i in range(2, len(poly), 1):
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
	ox = -float(x_samples)*0.5
	oy = -float(y_samples)*0.5

	def scale(a):
		return ( (a[0] + ox)*dx , (a[1] + oy)*dy )
			
	used = matrix(x_samples, y_samples)
	for a in range(x_samples):
		for b in range(y_samples):
			if dotProduct(image.getPixelHDR( (int)(a*dx), (int)(b*dy))) <= 1:
				used[a][b] = 1

	for a in range(x_samples-1):
		for b in range(y_samples-1):
			sum = used[a][b] + used[a+1][b] + used[a][b+1] + used[a+1][b+1]
			
			if sum == 4:
				yield map( scale, [ (a,b) , (a+1,b), (a+1,b+1), (a,b+1) ] )
			elif sum == 3:
				if not used[a][b]:
					yield map( scale, [ (a+1,b), (a+1,b+1) , (a,b+1) ] )
				if not used[a+1][b]:
					yield map( scale, [ (a,b), (a+1,b+1) , (a,b+1) ] )
				if not used[a][b+1]:
					yield map( scale, [ (a,b), (a+1,b) , (a+1,b+1) ] )
				if not used[a+1][b+1]:
					yield map( scale, [ (a,b), (a+1,b) , (a,b+1) ] )
					

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



#use the current image on the image editor? or ask the user what image to load
#image = Blender.Image.GetCurrent()

def load_image(filename):
	print "Loading ",filename
	#for now create a new mesh
	mesh = bpy.data.meshes.new('Plane')
	Blender.Scene.GetCurrent().objects.new(mesh)

	image = Blender.Image.Load(filename)
	print image
	ImportPlaneFromImage(image, mesh)
	Blender.Redraw()
	

image = Blender.Window.FileSelector(load_image, "Load Image")


