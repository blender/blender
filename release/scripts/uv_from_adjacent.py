#!BPY
"""
Name: 'UVs from adjacent'
Blender: 241
Group: 'UV'
Tooltip: 'Assign UVs to selected faces from surrounding unselected faces.'
"""
__author__ = "Campbell Barton"
__url__ = ("blender", "elysiun")
__version__ = "1.0 2006/02/07"

__bpydoc__ = """\
This script sets the UV mapping and image of selected faces from adjacent unselected faces.

Use this script in face select mode.
"""

from Blender import *


def mostUsedImage(imageList): # Returns the image most used in the list.
	if not imageList:
		return None
	elif len(imageList) < 3:
		return imageList[0]
	
	# 3+ Images, Get the most used image for surrounding faces.
	imageCount = {}
	for image in imageList:
		try:
			imageCount[image.name]['imageCount'] +=1 # an extra user of this image
		except:
			imageCount[image.name] = {'imageCount':1, 'blenderImage':image} # start with 1 user.
	
	# Now a list of tuples, (imageName, {imageCount, image})
	imageCount = imageCount.items()
	
	imageCount.sort(lambda a,b: cmp(a[1], b[1]))
	
	return imageCount[-1][1]['blenderImage']	


def main():
	scn = Scene.GetCurrent()
	ob = scn.getActiveObject()
	if ob == None or ob.getType() != 'Mesh':
		Draw.PupMenu('ERROR: No mesh object in face select mode.')
		return
	me = ob.getData(mesh=1)
	
	if not me.faceUV:
		Draw.PupMenu('ERROR: No mesh object in face select mode.')
		return
	SEL_FLAG = Mesh.FaceFlags['SELECT']
	selfaces = [f for f in me.faces if f.flag & SEL_FLAG]
	unselfaces = [f for f in me.faces if not f.flag & SEL_FLAG]
	
	
	# Gather per Vert UV and Image, store in vertUvAverage
	vertUvAverage = [{'averageUv':[], 'vertImages':[]} for i in xrange(len(me.verts))]
	
	for f in unselfaces: # Unselected faces only.
		if f.image:
			for i,v in enumerate(f.v):
				vertUvAverage[v.index]['averageUv'].append(f.uv[i])
				vertUvAverage[v.index]['vertImages'].append(f.image)
			
	# Average per vectex UV coords
	for vertUvData in vertUvAverage:
		uvList = vertUvData['averageUv']
		if uvList:
			# Convert from a list of vectors into 1 vector.
			vertUvData['averageUv'] = reduce(lambda a,b: a+b, uvList, Mathutils.Vector(0,0)) * (1.0/len(uvList))
		else:
			vertUvData['averageUv'] = None
			
	# Assign to selected faces
	for f in selfaces:
		uvlist = []
		imageList = []
		for i,v in enumerate(f.v):
			uv = vertUvAverage[v.index]['averageUv']
			vImages = vertUvAverage[v.index]['vertImages']
			uvlist.append( uv )
			imageList.extend(vImages)
		
		if None not in uvlist:			
			# all the faces images used by this faces vert. some faces will be added twice but thats ok.
			# Get the most used image and assign to the face.
			image = mostUsedImage(imageList) 
			print uvlist
			f.uv = tuple(uvlist)
			f.image = image
			f.mode |= Mesh.FaceModes['TEX']

if __name__ == '__main__':
	main()