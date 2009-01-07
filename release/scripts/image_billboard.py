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
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton 2006
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

import Blender
from Blender import Mesh, Material, Draw
import BPyMathutils
import bpy
import BPyRender
from Blender.Scene import Render

# reload(BPyRender)
# reload(BPyMathutils)

import os
Vector= Blender.Mathutils.Vector

def alpha_mat(image):
	# returns a material useable for 
	mtl= bpy.data.materials.new()
	mtl.mode |= (Material.Modes.SHADELESS | Material.Modes.ZTRANSP | Material.Modes.FULLOSA | Material.Modes.TEXFACE | Material.Modes.TEXFACE_ALPHA )
	return mtl

# PupBlock Settings
GLOBALS= {}
PREF_RES= Draw.Create(512)
PREF_TILE_RES= Draw.Create(256)
PREF_AA = Draw.Create(1)
PREF_ALPHA= Draw.Create(1)
PREF_Z_OFFSET = Draw.Create(10.0)
PREF_IMG_PACK= Draw.Create(1)


def save_billboard(PREF_IMAGE_PATH):
	Blender.Window.WaitCursor(1)
	# remove png, add it later
	PREF_IMAGE_PATH= PREF_IMAGE_PATH.replace('.png', '')
	
	ob_sel= GLOBALS['ob_sel']
	me_ob = GLOBALS['me_ob']
	me_data = GLOBALS['me_data']
	
	time= Blender.sys.time()
	
	me_mat= me_ob.matrixWorld
	
	# Render images for all faces
	face_data= [] # Store faces, images etc
	boxes2Pack= []
	me_data.faceUV= True
	
	for i, f in enumerate(me_data.faces):
		no= f.no
		
		# Offset the plane by the zoffset on the faces normal
		plane= [v.co * me_mat for v in f]
		
		# Horizontal stacking, make sure 0,1 and 2,3 are the longest
		if\
		(plane[0]-plane[1]).length + (plane[2]-plane[3]).length < \
		(plane[1]-plane[2]).length + (plane[3]-plane[0]).length:
			plane.append(plane.pop(0))
			rot90= True
		else:
			rot90= False
		
		no= Blender.Mathutils.QuadNormal(*plane)
		plane= [v + no*PREF_Z_OFFSET.val for v in plane]
		
		cent= (plane[0]+plane[1]+plane[2]+plane[3] ) /4.0
		camera_matrix= BPyMathutils.plane2mat(plane)
		tmp_path= '%s_%d' % (PREF_IMAGE_PATH, i)
		img= BPyRender.imageFromObjectsOrtho(ob_sel, tmp_path, PREF_TILE_RES.val, PREF_TILE_RES.val, PREF_AA.val, PREF_ALPHA.val, camera_matrix)
		img.reload()
		#img.pack() # se we can keep overwriting the path
		#img.filename= ""
		
		if rot90:
			f.uv=Vector(1,1), Vector(0,1), Vector(0,0), Vector(1,0)
		else:	
			f.uv= Vector(0,1), Vector(0,0), Vector(1,0), Vector(1,1)
		
		if not PREF_IMG_PACK.val:
			f.mode |= Mesh.FaceModes.TEX
			f.image = img
			
			if PREF_ALPHA.val:
				f.transp |= Mesh.FaceTranspModes.ALPHA
		else:
			w= ((plane[0]-plane[1]).length + (plane[2]-plane[3]).length)/2
			h= ((plane[1]-plane[2]).length + (plane[3]-plane[0]).length)/2
			
			face_data.append( (f, img) )
			boxes2Pack.append( [0.0,0.0,h, w, i] )
	
	if PREF_IMG_PACK.val:
		# pack the quads into a square
		packWidth, packHeight = Blender.Geometry.BoxPack2D(boxes2Pack)
		
		render_obs= []
		
		render_mat= alpha_mat(img)
		
		# Add geometry to the mesh
		for box in boxes2Pack:
			i= box[4]
			
			orig_f, img= face_data[i]
			
			# New Mesh and Object
			
			render_me= bpy.data.meshes.new()
			
			render_ob= Blender.Object.New('Mesh')
			render_me.materials= [render_mat]
			render_ob.link(render_me)
			
			render_obs.append(render_ob)
			
			# Add verts clockwise from the bottom left.
			_x= box[0] / packWidth
			_y= box[1] / packHeight
			_w= box[2] / packWidth
			_h= box[3] / packHeight
			
			
			render_me.verts.extend([\
			Vector(_x, _y, 0),\
			Vector(_x, _y +_h, 0),\
			Vector(_x + _w, _y +_h, 0),\
			Vector(_x + _w, _y, 0),\
			])
				
			render_me.faces.extend(list(render_me.verts))
			render_me.faceUV= True
			
			render_me.faces[0].uv = [Vector(0,0), Vector(0,1), Vector(1,1), Vector(1,0)]
			render_me.faces[0].image = img
			
			# Set the UV's, we need to flip them HOZ?
			for uv in orig_f.uv:
				uv.x = _x + (uv.x * _w)
				uv.y = _y + (uv.y * _h)
		
		target_image= BPyRender.imageFromObjectsOrtho(render_obs, PREF_IMAGE_PATH, PREF_RES.val, PREF_RES.val, PREF_AA.val, PREF_ALPHA.val, None)
		target_image.reload() # incase your overwriting an existing image.
		
		# Set to the 1 image.
		for f in me_data.faces:
			f.image= target_image
			if PREF_ALPHA.val:
				f.transp |= Mesh.FaceTranspModes.ALPHA
		
		# Free the images data and remove
		for data in face_data:
			img= data[1]
			os.remove(img.filename)
			img.reload()
			
	# Finish pack
	
	me_data.update()
	me_ob.makeDisplayList()
	Blender.Window.WaitCursor(0)
	print '%.2f secs taken' % (Blender.sys.time()-time)


def main():
	scn= bpy.data.scenes.active
	ob_sel= list(scn.objects.context)
	
	PREF_KEEP_ASPECT= False
	
	# Error Checking
	if len(ob_sel) < 2:
		Draw.PupMenu("Error%t|Select 2 mesh objects")
		return
		
	me_ob= scn.objects.active
	
	if not me_ob:
		Draw.PupMenu("Error%t|No active mesh selected.")
	
	try:
		ob_sel.remove(me_ob)
	except:
		pass
	
	if me_ob.type != 'Mesh':
		Draw.PupMenu("Error%t|Active Object must be a mesh to write billboard images too")
		return
	
	me_data= me_ob.getData(mesh=1)
	
	for f in me_data.faces:
		if len(f) != 4:
			Draw.PupMenu("Error%t|Active mesh must have only quads")
			return
	
	
	# Get user input
	block = [\
	'Image Pixel Size',\
	("Packed Size: ", PREF_RES, 128, 2048, "Pixel width and height to render the billboard to"),\
	("Tile Size: ", PREF_TILE_RES, 64, 1024, "Pixel  width and height for each tile to render to"),\
	'Render Settings',\
	("Pack Final", PREF_IMG_PACK , "Pack the image for each face into images into a single image"),\
	("Oversampling", PREF_AA , "Higher quality woth extra sampling"),\
	("Alpha Clipping", PREF_ALPHA , "Render empty areas as transparent"),\
	("Cam ZOffset: ", PREF_Z_OFFSET, 0.1, 100, "Distance to place the camera away from the quad when rendering")\
	]
	
	if not Draw.PupBlock("Billboard Render", block):
		return
	
	# Set globals
	GLOBALS['ob_sel'] = ob_sel
	GLOBALS['me_ob'] = me_ob
	GLOBALS['me_data'] = me_data
	
	Blender.Window.FileSelector(save_billboard, 'SAVE BILLBOARD', Blender.sys.makename(ext='.png'))
	# save_billboard('/tmp/test.png')

if __name__=='__main__':
	main()
