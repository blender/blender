#!BPY
"""
Name: 'Billboard Render on Active'
Blender: 242
Group: 'Image'
Tooltip: 'Selected objects and lamps to rendered faces on the act mesh'
""" 
__author__= "Campbell Barton"
__url__= ["blender", "blenderartist"]
__version__= "1.0"

__bpydoc__= """\
Render Billboard Script
This can texture a simple billboard mesh from any number of selected objects.

Renders objects in the selection to quad faces on the active mesh.

Usage
* Light your model or enable the shadless flag so it is visible
* Make a low poly mesh out of quads with 90d corners. (this will be you billboard mesh)
* Select the model and any lamps that light it
* Select the billboard mesh so that it is active
* Run this script, Adjust settings such as image size or oversampling.
* Select a place to save the PNG image.
* Once the script has finished running return to the 3d view by pressing Shift+F5
* To see the newly applied textures change the drawtype to 'Textured Solid'
"""


import Blender as B
import BPyMathutils
# reload(BPyMathutils)
import BPyRender
# reload(BPyRender)
import boxpack2d
# reload(boxpack2d) # for developing.
from Blender.Scene import Render

import os
Vector= B.Mathutils.Vector

def alpha_mat(image):
	# returns a material useable for 
	mtl= B.Material.New()
	mtl.mode |= (B.Material.Modes.SHADELESS | B.Material.Modes.ZTRANSP | B.Material.Modes.FULLOSA )
	mtl.alpha= 0.0 # so image sets the alpha
	
	tex= B.Texture.New()
	tex.type= B.Texture.Types.IMAGE
	tex.setImageFlags('InterPol', 'UseAlpha', 'Anti')
	tex.setExtend('Clip')
	tex.image= image
	
	mtl.setTexture(0, tex, B.Texture.TexCo.UV, B.Texture.MapTo.COL | B.Texture.MapTo.ALPHA)
	
	return mtl

# PupBlock Settings
GLOBALS= {}
PREF_RES= B.Draw.Create(512)
PREF_TILE_RES= B.Draw.Create(256)
PREF_AA = B.Draw.Create(1)
PREF_ALPHA= B.Draw.Create(1)
PREF_Z_OFFSET = B.Draw.Create(10.0)


def save_billboard(PREF_IMAGE_PATH):
	
	ob_sel= GLOBALS['ob_sel']
	me_ob = GLOBALS['me_ob']
	me_data = GLOBALS['me_data']
	
	time= B.sys.time()
	
	me_mat= me_ob.matrixWorld
	
	# Render images for all faces
	face_data= [] # Store faces, images etc
	boxes2Pack= []
	temp_images= []
	me_data.faceUV= True
	for i, f in enumerate(me_data.faces):
		no= f.no
		# Offset the plane by the zoffset on the faces normal
		plane= [v.co * me_mat for v in f]
		plane.reverse()
		no= B.Mathutils.QuadNormal(*plane)
		plane= [v + no*PREF_Z_OFFSET.val for v in plane]
		
		cent= (plane[0]+plane[1]+plane[2]+plane[3] ) /4.0
		camera_matrix= BPyMathutils.plane2mat(plane)
		tmp_path= '%s.%d' % (PREF_IMAGE_PATH, i)
		img= BPyRender.imageFromObjectsOrtho(ob_sel, tmp_path, PREF_TILE_RES.val, PREF_TILE_RES.val, PREF_AA.val, PREF_ALPHA.val, camera_matrix)
		# img.reload()
		#img.pack() # se we can keep overwriting the path
		#img.filename= ""
		temp_images.append(img)
		
		face_data.append( (f, img, plane) )
		w= ((plane[0]-plane[1]).length + (plane[2]-plane[3]).length)/2
		h= ((plane[1]-plane[2]).length + (plane[3]-plane[0]).length)/2
		
		f.mode |= B.Mesh.FaceModes.TEX
		f.image = img
		f.uv=Vector(1,1), Vector(1,0), Vector(0,0), Vector(0,1)
		boxes2Pack.append( (i, w, h) )

	
	# pack the quads into a square
	packWidth, packHeight, packedLs = boxpack2d.boxPackIter(boxes2Pack)
	
	render_obs= []
	
	# Add geometry to the mesh
	for i, box in enumerate(packedLs):
		
		orig_f, img, plane= face_data[i]
		
		# New Mesh and Object
		render_mat= alpha_mat(img)
		
		render_me= B.Mesh.New()
		render_me.verts.extend([Vector(0,0,0)]) # Stupid, dummy vert, preverts errors. when assigning UV's/
		render_ob= B.Object.New('Mesh')
		render_me.materials= [render_mat]
		render_ob.link(render_me)
		
		render_obs.append(render_ob)
		
		# Add verts clockwise from the bottom left.
		_x= box[1] / packWidth
		_y= box[2] / packHeight
		_w= box[3] / packWidth
		_h= box[4] / packHeight
		
		render_me.verts.extend([\
		Vector(_x, _y, 0),\
		Vector(_x, _y +_h, 0),\
		Vector(_x + _w, _y +_h, 0),\
		Vector(_x + _w, _y, 0),\
		])
		
		render_me.faces.extend([\
		render_me.verts[-1],\
		render_me.verts[-2],\
		render_me.verts[-3],\
		render_me.verts[-4],\
		])
		render_me.faceUV= True
		
		# target_face= render_me.faces[-1]
		# TEXFACE isnt used because of the renderign engine cant to alpha's for texdface.
		#target_face.image= img
		#target_face.mode |= B.Mesh.FaceModes.TEX
		
		# Set the UV's, we need to flip them HOZ?
		orig_f.uv[2].x= orig_f.uv[3].x= _x+_w
		orig_f.uv[0].x= orig_f.uv[1].x= _x
		
		orig_f.uv[1].y= orig_f.uv[2].y= _y+_h
		orig_f.uv[0].y= orig_f.uv[3].y= _y
	
	target_image= BPyRender.imageFromObjectsOrtho(render_obs, PREF_IMAGE_PATH, PREF_RES.val, PREF_RES.val, PREF_AA.val, PREF_ALPHA.val, None)
	
	# Set to the 1 image.
	for f in me_data.faces:
		f.image= target_image
		if PREF_ALPHA.val:
			f.transp |= B.Mesh.FaceTranspModes.ALPHA
	
	# Free the images data and remove
	for img in temp_images:
		os.remove(img.filename)
		img.reload()
	
	me_data.update()
	me_ob.makeDisplayList()
	B.Window.WaitCursor(0)
	print '%.2f secs taken' % (B.sys.time()-time)
	



def main():
	
	
	
	block = [\
	'Image Pixel Size',\
	("Output: ", PREF_RES, 128, 2048, "Pixel width and height to render the billboard to"),\
	("Tile: ", PREF_TILE_RES, 64, 1024, "Pixel  width and height for each tile to render to"),\
	'Render Settings',\
	("Oversampling", PREF_AA , "Higher quality woth extra sampling"),\
	("Alpha Clipping", PREF_ALPHA , "Render empty areas as transparent"),\
	("Cam ZOffset: ", PREF_Z_OFFSET, 0.1, 100, "Distance to place the camera away from the quad when rendering")\
	]
	
	if not B.Draw.PupBlock("Billboard Render", block):
		return
	
	B.Window.WaitCursor(1)
	
	
	
	scn= B.Scene.GetCurrent()
	ob_sel= list(scn.objects.context)
	
	PREF_KEEP_ASPECT= False
	
	
	if len(ob_sel) < 2:
		B.Draw.PupMenu("Error%t|Select 2 mesh objects")
		return
		
	me_ob= scn.objects.active
	
	if not me_ob:
		B.Draw.PupMenu("Error%t|No active mesh selected.")
	
	try:
		ob_sel.remove(me_ob)
	except:
		pass
	
	if me_ob.type != 'Mesh':
		B.Draw.PupMenu("Error%t|Active Object must be a mesh to write billboard images too")
		return
	
	me_data= me_ob.getData(mesh=1)
	
	
	for f in me_data.faces:
		if len(f) != 4:
			B.Draw.PupMenu("Error%t|Active mesh must have only quads")
			return
	
	GLOBALS['ob_sel'] = ob_sel
	GLOBALS['me_ob'] = me_ob
	GLOBALS['me_data'] = me_data
	
	B.Window.FileSelector(save_billboard, 'SAVE BILLBOARD', B.sys.makename(ext='.png'))

if __name__=='__main__':
	main()
