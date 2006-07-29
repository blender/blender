#!BPY
"""
Name: 'Bake Wire Image from UVs'
Blender: 242
Group: 'Image'
Tooltip: 'Generate an image from the selected mesh objects UV coords and materials'
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
	me_s= dict([\
	  (ob.getData(name_only=1), ob.getData(mesh=1))\
	  for ob in Blender.Object.GetSelected()\
	  if ob.getType()=='Mesh'  if ob.getData(mesh=1).faceUV]).values()
	
	if not me_s:
		BPyMessages.Error_NoMeshUvSelected()
		return
		
	newpath= Blender.Get('filename').split('/')[-1].split('\\')[-1].replace('.blend', '')
	PREF_IMAGE_PATH = Create('//%s_wire' % newpath)
	PREF_IMAGE_SIZE = Create(512)
	PREF_IMAGE_WIRE_INVERT = Create(0)
	PREF_IMAGE_WIRE_UNDERLAY = Create(1)
	PREF_IMAGE_SMOOTH= Create(1)
	
	PREF_SEL_FACES_ONLY= Create(0)
	
	pup_block = [\
	'Image Path: (no ext)',\
	('', PREF_IMAGE_PATH, 3, 100, 'Path to new Image. "//" for curent blend dir.'),\
	'Image Options',
	('Pixel Size:', PREF_IMAGE_SIZE, 64, 4096, 'Image Width and Height.'),\
	('White Wire', PREF_IMAGE_WIRE_INVERT, 'Sets the wire to white (otherwise its black).'),\
	('Fill Faces', PREF_IMAGE_WIRE_UNDERLAY, 'Fill in faces with material color.'),\
	('Smooth lines', PREF_IMAGE_SMOOTH, 'Render smooth lines.'),\
	'',\
	('Selected Faces only', PREF_SEL_FACES_ONLY, 'Only bake from selected faces.'),\
	]
	
	if not Blender.Draw.PupBlock('Wire Bake', pup_block):
		return
	
	
	# Defaults for VCol, user cant change
	PREF_IMAGE_WIRE= True # of course we need this one 
	
	PREF_USE_IMAGE= False
	
	PREF_IMAGE_BLEED= 0
	
	PREF_USE_VCOL= False
	PREF_USE_MATCOL= False
	PREF_USE_NORMAL= False
	
	BPyRender.vcol2image(me_s,\
	PREF_IMAGE_PATH.val,\
	PREF_IMAGE_SIZE.val,\
	PREF_IMAGE_BLEED,\
	PREF_IMAGE_SMOOTH.val,\
	PREF_IMAGE_WIRE,\
	PREF_IMAGE_WIRE_INVERT.val,\
	PREF_IMAGE_WIRE_UNDERLAY.val,\
	PREF_USE_IMAGE,\
	PREF_USE_VCOL,\
	PREF_USE_MATCOL,\
	PREF_USE_NORMAL,\
	PREF_SEL_FACES_ONLY.val)
	
	Blender.Window.RedrawAll()

if __name__ == '__main__':
	main()
