#!BPY
"""
Name: 'Color/Normals to Image'
Blender: 241
Group: 'UV'
Tooltip: 'Save the active meshes vertex colors or normals to an image.'
"""
__author__= ['Campbell Barton']
__url__= ('blender', 'elysiun', 'http://www.gametutorials.com')
__version__= '0.95'
__bpydoc__= '''\

Vert color to Image

This script makes an image from a meshes vertex colors, using the UV coordinates
to draw the faces into the image.

This makes it possible to bake radiosity into a texture.
Make sure your UV Coordinates do not overlap. LSCM Unwrapper or archimap unwrapper work well
to automaticaly do this.
'''


import Blender
import BPyRender
# reload(BPyRender)
import BPyMesh
Vector= Blender.Mathutils.Vector
Create= Blender.Draw.Create

def rnd_mat():
	render_mat= Blender.Material.New()
	render_mat.mode |= Blender.Material.Modes.VCOL_PAINT
	render_mat.mode |= Blender.Material.Modes.SHADELESS
	render_mat.mode |= Blender.Material.Modes.TEXFACE
	return render_mat

def vcol2image(me, PREF_IMAGE_PATH, PREF_IMAGE_SIZE, PREF_IMAGE_BLEED, PREF_USE_IMAGE, PREF_USE_NORMAL):
	
	BLEED_PIXEL= 1.0/PREF_IMAGE_SIZE
	if PREF_USE_NORMAL:
		BPyMesh.meshCalcNormals(me)
	
	render_me= Blender.Mesh.New()
	render_me.verts.extend( [Vector(0,0,0),] )
	
	render_me.verts.extend( [ Vector(uv.x-BLEED_PIXEL, uv.y-BLEED_PIXEL/2, 0) for f in me.faces for uv in f.uv ] )
	i= 1
	tmp_faces= []
	for f in me.faces:
		tmp_faces.append( [ii+i for ii in xrange(len(f))] )
		i+= len(f)
	
	render_me.faces.extend(tmp_faces)
	
	for i, f in enumerate(me.faces):
		frnd= render_me.faces[i]
		if PREF_USE_IMAGE:
			ima= f.image
			if ima:
				frnd.image= ima
				
		frnd.uv= f.uv
		if PREF_USE_NORMAL:
			for ii, v in enumerate(f):
				no= v.no
				c= frnd.col[ii]
				c.r= int((no.x+1)*128)-1
				c.g= int((no.y+1)*128)-1
				c.b= int((no.z+1)*128)-1
		else:
			frnd.col= f.col
			
	
	render_ob= Blender.Object.New('Mesh')
	render_ob.link(render_me)
	obs= [render_ob]
	
	# EVIL BLEEDING CODE!! - Just do copys of the mesh and place behind. Crufty but better then many other methods I have seen.
	if PREF_IMAGE_BLEED:
		z_offset= 0.0
		for i in xrange(PREF_IMAGE_BLEED):
			for diag1, diag2 in ((-1,-1),(-1,1),(1,-1),(1,1), (1,0), (0,1), (-1,0), (0, -1)): # This line extends the object in 8 different directions, top avoid bleeding.
				
				render_ob= Blender.Object.New('Mesh')
				render_ob.link(render_me)
				
				render_ob.LocX= (i+1)*diag1*BLEED_PIXEL
				render_ob.LocY= (i+1)*diag2*BLEED_PIXEL
				render_ob.LocZ= -z_offset
				
				obs.append(render_ob)
				z_offset += 0.01
	
	
	render_me.materials= [rnd_mat()]
	
	im= BPyRender.imageFromObjectsOrtho(obs, PREF_IMAGE_PATH, PREF_IMAGE_SIZE, PREF_IMAGE_SIZE)




def main():
	# Create the variables.
	# Filename without path or extension.
	scn= Blender.Scene.GetCurrent()
	act_ob= scn.getActiveObject()
	if not act_ob or act_ob.getType() != 'Mesh':
		Blender.Draw.PupMenu('Error, no active mesh selected.')
		return
		
	
	newpath= Blender.Get('filename').split('/')[-1].split('\\')[-1].replace('.blend', '')
	
	PREF_IMAGE_PATH = Create('//%s_grp' % newpath)
	PREF_IMAGE_SIZE = Create(1024)
	PREF_IMAGE_BLEED = Create(6)
	PREF_USE_IMAGE = Create(0)
	PREF_USE_NORMAL = Create(0)
	
	pup_block = [\
	'Image Path: (no ext)',\
	('', PREF_IMAGE_PATH, 3, 100, 'Path to new Image. "//" for curent blend dir.'),\
	'Image Options',
	('Pixel Size:', PREF_IMAGE_SIZE, 64, 4096, 'Image Width and Height.'),\
	('Pixel Bleed:', PREF_IMAGE_BLEED, 0, 64, 'Image Bleed pixels.'),\
	('Image Include', PREF_USE_IMAGE, 'Image Bleed pixels.'),\
	'',\
	('Normal Map', PREF_USE_NORMAL, 'Use Normals instead of VCols.'),\
	]
	
	if not Blender.Draw.PupBlock('VCol to Image', pup_block):
		return
	
	vcol2image(act_ob.getData(mesh=1), PREF_IMAGE_PATH.val, PREF_IMAGE_SIZE.val, PREF_IMAGE_BLEED.val, PREF_USE_IMAGE.val, PREF_USE_NORMAL.val)
	Blender.Window.RedrawAll()

if __name__ == '__main__':
	main()
