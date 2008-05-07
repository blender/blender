#!BPY
"""
Name: 'UVs from unselected adjacent'
Blender: 242
Group: 'UVCalculation'
Tooltip: 'Assign UVs to selected faces from surrounding unselected faces.'
"""
__author__ = "Campbell Barton"
__url__ = ("blender", "blenderartists.org")
__version__ = "1.0 2006/02/07"

__bpydoc__ = """\
This script sets the UV mapping and image of selected faces from adjacent unselected faces.

Use this script in face select mode for texturing between textured faces.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
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


from Blender import *
import bpy

def mostUsedImage(imageList): # Returns the image most used in the list.
	if not imageList:
		return None
	elif len(imageList) < 3:
		return imageList[0]
	
	# 3+ Images, Get the most used image for surrounding faces.
	imageCount = {}
	for image in imageList:
		if image:
			image_key= image.name
		else:
			image_key = None
		
		try:
			imageCount[image_key]['imageCount'] +=1 # an extra user of this image
		except:
			imageCount[image_key] = {'imageCount':1, 'blenderImage':image} # start with 1 user.
	
	# Now a list of tuples, (imageName, {imageCount, image})
	imageCount = imageCount.items()
	
	try:	imageCount.sort(key=lambda a: a[1])
	except:	imageCount.sort(lambda a,b: cmp(a[1], b[1]))
	
	
	return imageCount[-1][1]['blenderImage']	


def main():
	sce = bpy.data.scenes.active
	ob = sce.objects.active
	
	if ob == None or ob.type != 'Mesh':
		Draw.PupMenu('ERROR: No mesh object in face select mode.')
		return
	me = ob.getData(mesh=1)
	
	if not me.faceUV:
		Draw.PupMenu('ERROR: No mesh object in face select mode.')
		return
	
	selfaces = [f for f in me.faces if f.sel]
	unselfaces = [f for f in me.faces if not f.sel]
	
	
	# Gather per Vert UV and Image, store in vertUvAverage
	vertUvAverage = [[[],[]] for i in xrange(len(me.verts))]
	
	for f in unselfaces: # Unselected faces only.
		fuv = f.uv
		for i,v in enumerate(f):
			vertUvAverage[v.index][0].append(fuv[i])
			vertUvAverage[v.index][1].append(f.image)
			
	# Average per vectex UV coords
	for vertUvData in vertUvAverage:
		uvList = vertUvData[0]
		if uvList:
			# Convert from a list of vectors into 1 vector.
			vertUvData[0] = reduce(lambda a,b: a+b, uvList, Mathutils.Vector(0,0)) * (1.0/len(uvList))
		else:
			vertUvData[0] = None
	
	# Assign to selected faces
	TEX_FLAG = Mesh.FaceModes['TEX']
	for f in selfaces:
		uvlist = []
		imageList = []
		for i,v in enumerate(f):
			uv, vImages = vertUvAverage[v.index]
			uvlist.append( uv )
			imageList.extend(vImages)
		
		if None not in uvlist:			
			# all the faces images used by this faces vert. some faces will be added twice but thats ok.
			# Get the most used image and assign to the face.
			image = mostUsedImage(imageList) 
			f.uv = uvlist
			
			if image:
				f.image = image
				f.mode |= TEX_FLAG
	Window.RedrawAll()
	
if __name__ == '__main__':
	main()