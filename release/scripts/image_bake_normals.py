#!BPY
"""
Name: 'Bake Normal Map Image from UVs'
Blender: 242
Group: 'Image'
Tooltip: 'Generate a normal map image from selected mesh objects.'
"""
__author__= ['Campbell Barton']
__url__= ('blender', 'elysiun', 'http://www.gametutorials.com')
__version__= '0.1'
__bpydoc__= '''\
Bake Vertex Colors to an image

This script makes an image from a meshes vertex colors, using the UV coordinates
to draw the faces into the image.

This makes it possible to bake radiosity into a texture.
Make sure your UV Coordinates do not overlap.
LSCM Unwrapper or archimap unwrapper work well to automaticaly do this.
'''

import Blender
import BPyRender
#reload(BPyRender)
import BPyMessages
Vector= Blender.Mathutils.Vector
Create= Blender.Draw.Create


def main():
	# Create the variables.
	# Filename without path or extension.
	# Big LC, gets all unique mesh objects from the selection that have UV coords.
	
	def worldnormals(ob):
		nor_mtx= ob.matrixWorld.rotationPart()
		me= ob.getData(mesh=1)
		for v in me.verts:
			v.no= v.no*nor_mtx
		return me
	
	ob_s= dict([\
	  (ob.getData(name_only=1), ob)\
	  for ob in Blender.Object.GetSelected()\
	  if ob.getType()=='Mesh'  if ob.getData(mesh=1).faceUV]).values()
	
	me_s= [worldnormals(ob) for ob in ob_s]
	del ob_s
	
	if not me_s:
		BPyMessages.Error_NoMeshUvSelected()
		return
		
	newpath= Blender.Get('filename').split('/')[-1].split('\\')[-1].replace('.blend', '')
	PREF_IMAGE_PATH = Create('//%s_nor' % newpath)
	PREF_IMAGE_SIZE = Create(512)
	PREF_IMAGE_BLEED = Create(4)
	PREF_IMAGE_SMOOTH= Create(1)
	
	PREF_SEL_FACES_ONLY= Create(0)
	
	pup_block = [\
	'Image Path: (no ext)',\
	('', PREF_IMAGE_PATH, 3, 100, 'Path to new Image. "//" for curent blend dir.'),\
	'Image Options',
	('Pixel Size:', PREF_IMAGE_SIZE, 64, 4096, 'Image Width and Height.'),\
	('Pixel Bleed:', PREF_IMAGE_BLEED, 0, 64, 'Extend pixels from boundry edges to avoid mipmapping errors on rendering.'),\
	('Smooth lines', PREF_IMAGE_SMOOTH, 'Render smooth lines.'),\
	'',\
	('Selected Faces only', PREF_SEL_FACES_ONLY, 'Only bake from selected faces.'),\
	]
	
	if not Blender.Draw.PupBlock('Texface Image Bake', pup_block):
		# Update the normals before we exit
		for me in me_s:
			me.update()
		return
	
	
	
	
	
	
	
	# Defaults for VCol, user cant change
	PREF_IMAGE_WIRE= False
	PREF_IMAGE_WIRE_INVERT= False
	PREF_IMAGE_WIRE_UNDERLAY= False
	
	PREF_USE_IMAGE= False 
	PREF_USE_VCOL= False
	PREF_USE_MATCOL= False
	
	PREF_USE_NORMAL= True # of course we need this one 
	
	BPyRender.vcol2image(me_s,\
	PREF_IMAGE_PATH.val,\
	PREF_IMAGE_SIZE.val,\
	PREF_IMAGE_BLEED.val,\
	PREF_IMAGE_SMOOTH.val,\
	PREF_IMAGE_WIRE,\
	PREF_IMAGE_WIRE_INVERT,\
	PREF_IMAGE_WIRE_UNDERLAY,\
	PREF_USE_IMAGE,\
	PREF_USE_VCOL,\
	PREF_USE_MATCOL,\
	PREF_USE_NORMAL,\
	PREF_SEL_FACES_ONLY.val)
	
	# Restore normals
	for me in me_s:
		me.update()
	
	Blender.Window.RedrawAll()

if __name__ == '__main__':
	main()
