#!BPY
"""
Name: 'Bake Image from UVs (vcol/img/nor)'
Blender: 241
Group: 'Image'
Tooltip: 'Save the active or selected meshes meshes images, vertex colors or normals to an image.'
"""
__author__= ['Campbell Barton']
__url__= ('blender', 'elysiun', 'http://www.gametutorials.com')
__version__= '0.95'
__bpydoc__= '''\

Bake from UVs to image

This script makes an image from a meshes vertex colors, using the UV coordinates
to draw the faces into the image.

This makes it possible to bake radiosity into a texture.
Make sure your UV Coordinates do not overlap. LSCM Unwrapper or archimap unwrapper work well
to automaticaly do this.
'''


import Blender
import BPyRender
import BPyMesh
Vector= Blender.Mathutils.Vector
Create= Blender.Draw.Create


def vcol2image(me_s,\
	PREF_IMAGE_PATH,\
	PREF_IMAGE_SIZE,\
	PREF_IMAGE_BLEED,\
	PREF_IMAGE_SMOOTH,\
	PREF_IMAGE_WIRE,\
	PREF_USE_IMAGE,\
	PREF_USE_VCOL,\
	PREF_USE_MATCOL,\
	PREF_USE_NORMAL):
	
	
	def rnd_mat():
		render_mat= Blender.Material.New()
		mode= render_mat.mode
		
		# Dont use lights ever
		mode |= Blender.Material.Modes.SHADELESS
		
		if PREF_IMAGE_WIRE:
			mode |= Blender.Material.Modes.WIRE
		if PREF_USE_VCOL or PREF_USE_MATCOL: # both vcol and material color use vertex cols to avoid the 16 max limit in materials
			mode |= Blender.Material.Modes.VCOL_PAINT
		if PREF_USE_IMAGE:
			mode |= Blender.Material.Modes.TEXFACE
		
		# Copy back the mode
		render_mat.mode |= mode
		return render_mat
	
	
	BLEED_PIXEL= 1.0/PREF_IMAGE_SIZE
	render_me= Blender.Mesh.New()
	render_me.verts.extend( [Vector(0,0,0),] ) # 0 vert uv bugm dummy vert
	
	
	for me in me_s:
		
		# Multiple mesh support.
			
		if PREF_USE_NORMAL:
			BPyMesh.meshCalcNormals(me)
		
		vert_offset= len(render_me.verts)
		render_me.verts.extend( [ Vector(uv.x-BLEED_PIXEL, uv.y-BLEED_PIXEL/2, 0) for f in me.faces for uv in f.uv ] )
		
		tmp_faces= []
		for f in me.faces:
			tmp_faces.append( [ii+vert_offset for ii in xrange(len(f))] )
			vert_offset+= len(f)
		
		face_offset= len(render_me.faces)
		render_me.faces.extend(tmp_faces)
		
		if PREF_USE_MATCOL:
			materials= []
			for mat in me.materials:
				if mat==None:
					materials.append((1.0, 1.0, 1.0)) # white
				else:
					materials.append(mat.rgbCol)
			
			if not materials: # Well need a dummy material so the index works if we have no materials.
				materials= [(1.0, 1.0, 1.0)]
		
		for i, f in enumerate(me.faces):
			frnd= render_me.faces[face_offset+i]
			if PREF_USE_IMAGE:
				ima= f.image
				if ima:
					frnd.image= ima
					
			frnd.uv= f.uv
			
			# Use normals excludes other color operations
			if PREF_USE_NORMAL:
				for ii, v in enumerate(f.v):
					nx, ny, nz= v.no
					c= frnd.col[ii]
					# Modified to adjust from the current color
					c.r= int((nx+1)*128)-1
					c.g= int((ny+1)*128)-1
					c.b= int((nz+1)*128)-1
			else:
				# Initialize color
				if PREF_USE_VCOL:
					frnd.col= f.col
				
					# Mix with vert color
					if PREF_USE_MATCOL:
						# Multiply with existing color
						r,g,b= materials[f.mat]
						for col in frnd.col:
							col.r= int(col.r*r)
							col.g= int(col.g*g)
							col.b= int(col.b*b)
							
				elif PREF_USE_MATCOL: # Mat color only
						# Multiply with existing color
						r,g,b= materials[f.mat]
						for col in frnd.col:
							col.r= int(255*r)
							col.g= int(255*g)
							col.b= int(255*b)
	
	render_ob= Blender.Object.New('Mesh')
	render_ob.link(render_me)
	obs= [render_ob]
	
	# EVIL BLEEDING CODE!! - Just do copys of the mesh and place behind. Crufty but better then many other methods I have seen.
	if PREF_IMAGE_BLEED and not PREF_IMAGE_WIRE:
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
	im= BPyRender.imageFromObjectsOrtho(obs, PREF_IMAGE_PATH, PREF_IMAGE_SIZE, PREF_IMAGE_SIZE, PREF_IMAGE_SMOOTH)
	
	# Clear from memory as best as we can
	render_me.verts= None




def main():
	# Create the variables.
	# Filename without path or extension.
	scn= Blender.Scene.GetCurrent()
	act_ob= scn.getActiveObject()
	obsel= [ob for ob in Blender.Object.GetSelected() if ob.getType()=='Mesh']
	
	if not act_ob or act_ob.getType() != 'Mesh' or not act_ob.getData(mesh=1).faceUV:
		Blender.Draw.PupMenu('Error, no active mesh selected.')
		return
		
	
	newpath= Blender.Get('filename').split('/')[-1].split('\\')[-1].replace('.blend', '')
	
	PREF_IMAGE_PATH = Create('//%s_grp' % newpath)
	PREF_IMAGE_SIZE = Create(1024)
	PREF_IMAGE_BLEED = Create(6)
	PREF_IMAGE_SMOOTH= Create(1)
	PREF_IMAGE_WIRE= Create(0)
	
	PREF_USE_IMAGE = Create(1)
	PREF_USE_VCOL = Create(1)
	PREF_USE_MATCOL = Create(0)
	PREF_USE_NORMAL = Create(0)
	if len(obsel)>1: PREF_USE_MULIOB = Create(0)
	
	pup_block = [\
	'Image Path: (no ext)',\
	('', PREF_IMAGE_PATH, 3, 100, 'Path to new Image. "//" for curent blend dir.'),\
	'Image Options',
	('Pixel Size:', PREF_IMAGE_SIZE, 64, 4096, 'Image Width and Height.'),\
	('Pixel Bleed:', PREF_IMAGE_BLEED, 0, 64, 'Extend pixels from boundry edges to avoid mipmapping errors on rendering.'),\
	('Smooth lines', PREF_IMAGE_SMOOTH, 'Render smooth lines.'),\
	('Wire Only', PREF_IMAGE_WIRE, 'Renders a wireframe from the mesh, implys bleed is zero.'),\
	
	'Color Source',\
	('Image Texface', PREF_USE_IMAGE, 'Render the faces image in the output.'),\
	('Vertex Colors', PREF_USE_VCOL, 'Use Normals instead of VCols.'),\
	('Material Color', PREF_USE_MATCOL, 'Use the materials color.'),\
	('Normal Map', PREF_USE_NORMAL, 'Use Normals instead of VCols.'),\
	]
	
	if len(obsel)>1:
		pup_block.append('')
		pup_block.append(('All Selected Meshes', PREF_USE_MULIOB, 'Use faces from all selcted meshes, Make sure UV coords dont overlap between objects.'))
		
	
	if not Blender.Draw.PupBlock('VCol to Image', pup_block):
		return
	
	if not PREF_USE_MULIOB.val:
		me_s= [act_ob.getData(mesh=1)]
	else:
		# Make double sure datas unique
		me_s = dict([(ob.getData(name_only=1), ob.getData(mesh=1)) for ob in obsel]).values()
		me_s = [me for me in me_s if me.faceUV]
	
	vcol2image(me_s,\
	PREF_IMAGE_PATH.val,\
	PREF_IMAGE_SIZE.val,\
	PREF_IMAGE_BLEED.val,\
	PREF_IMAGE_SMOOTH.val,\
	PREF_IMAGE_WIRE.val,\
	PREF_USE_IMAGE.val,\
	PREF_USE_VCOL.val,\
	PREF_USE_MATCOL.val,\
	PREF_USE_NORMAL.val)
	
	Blender.Window.RedrawAll()

if __name__ == '__main__':
	main()
