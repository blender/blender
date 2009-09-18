#!BPY

"""
Name: 'Consolidate into one image'
Blender: 243
Group: 'Image'
Tooltip: 'Pack all texture images into 1 image and remap faces.'
"""

__author__ = "Campbell Barton"
__url__ = ("blender", "blenderartists.org")
__version__ = "1.1a 2009/04/01"

__bpydoc__ = """\
This script makes a new image from the used areas of all the images mapped to the selected mesh objects.
Image are packed into 1 new image that is assigned to the original faces.
This is usefull for game models where 1 image is faster then many, and saves the labour of manual texture layout in an image editor.

"""
# -------------------------------------------------------------------------- 
# Auto Texture Layout v1.0 by Campbell Barton (AKA Ideasman)
# -------------------------------------------------------------------------- 
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
# ***** END GPL LICENCE BLOCK ***** 
# -------------------------------------------------------------------------- 


# Function to find all the images we use
import Blender as B
from Blender.Mathutils import Vector, RotationMatrix
from Blender.Scene import Render
import BPyMathutils
BIGNUM= 1<<30
TEXMODE= B.Mesh.FaceModes.TEX

def pointBounds(points):
	'''
	Takes a list of points and returns the
	area, center, bounds
	'''
	ymax= xmax= -BIGNUM
	ymin= xmin=  BIGNUM
	
	for p in points:
		x= p.x
		y= p.y
		
		if x>xmax: xmax=x
		if y>ymax: ymax=y
		
		if x<xmin: xmin=x
		if y<ymin: ymin=y
	
	# area and center	
	return\
	(xmax-xmin) * (ymax-ymin),\
	Vector((xmin+xmax)/2, (ymin+ymax)/2),\
	(xmin, ymin, xmax, ymax)
	

def bestBoundsRotation(current_points):
	'''
	Takes a list of points and returns the best rotation for those points
	so they fit into the samllest bounding box
	'''
	
	current_area, cent, bounds= pointBounds(current_points)
	
	total_rot_angle= 0.0
	rot_angle= 45
	while rot_angle > 0.1:
		mat_pos= RotationMatrix( rot_angle, 2)
		mat_neg= RotationMatrix( -rot_angle, 2)
		
		new_points_pos= [v*mat_pos for v in current_points]
		area_pos, cent_pos, bounds_pos= pointBounds(new_points_pos)
		
		# 45d rotations only need to be tested in 1 direction.
		if rot_angle == 45: 
			area_neg= area_pos
		else:
			new_points_neg= [v*mat_neg for v in current_points]
			area_neg, cent_neg, bounds_neg= pointBounds(new_points_neg)
		
		
		# Works!
		#print 'Testing angle', rot_angle, current_area, area_pos, area_neg
		
		best_area= min(area_pos, area_neg, current_area)
		if area_pos == best_area:
			current_area= area_pos
			cent= cent_pos
			bounds= bounds_pos
			current_points= new_points_pos
			total_rot_angle+= rot_angle
		elif rot_angle != 45 and area_neg == best_area:
			current_area= area_neg
			cent= cent_neg
			bounds= bounds_neg
			current_points= new_points_neg
			total_rot_angle-= rot_angle
		
		rot_angle *= 0.5
	
	# Return the optimal rotation.
	return total_rot_angle


class faceGroup(object):
	'''
	A Group of faces that all use the same image, each group has its UVs packed into a square.
	'''
	__slots__= 'xmax', 'ymax', 'xmin', 'ymin',\
	'image', 'faces', 'box_pack', 'size', 'ang', 'rot_mat', 'cent'\
	
	def __init__(self, mesh_list, image, size, PREF_IMAGE_MARGIN):
		self.image= image
		self.size= size
		self.faces= [f for me in mesh_list for f in me.faces if f.mode & TEXMODE and f.image == image]
		
		# Find the best rotation.
		all_points= [uv for f in self.faces for uv in f.uv]
		bountry_indicies= BPyMathutils.convexHull(all_points)
		bountry_points= [all_points[i] for i in bountry_indicies]
		
		# Pre Rotation bounds
		self.cent= pointBounds(bountry_points)[1]
		
		# Get the optimal rotation angle
		self.ang= bestBoundsRotation(bountry_points)
		self.rot_mat= RotationMatrix(self.ang, 2), RotationMatrix(-self.ang, 2)
		
		# Post rotation bounds
		bounds= pointBounds([\
		((uv-self.cent) * self.rot_mat[0]) + self.cent\
		for uv in bountry_points])[2]
		
		# Break the bounds into useable values.
		xmin, ymin, xmax, ymax= bounds
		
		# Store the bounds, include the margin.
		# The bounds rect will need to be rotated to the rotation angle.
		self.xmax= xmax + (PREF_IMAGE_MARGIN/size[0])
		self.xmin= xmin - (PREF_IMAGE_MARGIN/size[0])
		self.ymax= ymax + (PREF_IMAGE_MARGIN/size[1])
		self.ymin= ymin - (PREF_IMAGE_MARGIN/size[1])
		
		self.box_pack=[\
		0.0, 0.0,\
		size[0]*(self.xmax - self.xmin),\
		size[1]*(self.ymax - self.ymin),\
		image.name] 
		
	'''
		# default.
		self.scale= 1.0

	def set_worldspace_scale(self):
		scale_uv= 0.0
		scale_3d= 0.0
		for f in self.faces:
			for i in xrange(len(f.v)):
				scale_uv+= (f.uv[i]-f.uv[i-1]).length * 0.1
				scale_3d+= (f.v[i].co-f.v[i-1].co).length * 0.1
		self.scale= scale_3d/scale_uv
	'''
		
		
	
	def move2packed(self, width, height):
		'''
		Moves the UV coords to their packed location
		using self.box_pack as the offset, scaler.
		box_pack must be set to its packed location.
		width and weight are the w/h of the overall packed area's bounds.
		'''
		# packedLs is a list of [(anyUniqueID, left, bottom, width, height)...]
		# Width and height in float pixel space.
		
		# X Is flipped :/
		#offset_x= (1-(self.box_pack[1]/d)) - (((self.xmax-self.xmin) * self.image.size[0])/d)
		offset_x= self.box_pack[0]/width
		offset_y= self.box_pack[1]/height
		
		for f in self.faces:
			for uv in f.uv:
				uv_rot= ((uv-self.cent) * self.rot_mat[0]) + self.cent
				uv.x= offset_x+ (((uv_rot.x-self.xmin) * self.size[0])/width)
				uv.y= offset_y+ (((uv_rot.y-self.ymin) * self.size[1])/height)

def consolidate_mesh_images(mesh_list, scn, PREF_IMAGE_PATH, PREF_IMAGE_SIZE, PREF_KEEP_ASPECT, PREF_IMAGE_MARGIN): #, PREF_SIZE_FROM_UV=True):
	'''
	Main packing function
	
	All meshes from mesh_list must have faceUV else this function will fail.
	'''
	face_groups= {}
	
	for me in mesh_list:
		for f in me.faces:
			if f.mode & TEXMODE:
				image= f.image
				if image:
					try:
						face_groups[image.name] # will fail if teh groups not added.
					except:
						try:
							size= image.size
						except:
							B.Draw.PupMenu('Aborting: Image cold not be loaded|' + image.name)
							return
							
						face_groups[image.name]= faceGroup(mesh_list, image, size, PREF_IMAGE_MARGIN)
	
	if not face_groups:
		B.Draw.PupMenu('No Images found in mesh(es). Aborting!')
		return
	
	if len(face_groups)<2:
		B.Draw.PupMenu('Only 1 image found|Select a mesh(es) using 2 or more images.')
		return
		
	'''
	if PREF_SIZE_FROM_UV:
		for fg in face_groups.itervalues():
			fg.set_worldspace_scale()
	'''
	
	# RENDER THE FACES.
	render_scn= B.Scene.New()
	render_scn.makeCurrent()
	render_context= render_scn.getRenderingContext()
	render_context.setRenderPath('') # so we can ignore any existing path and save to the abs path.
	
	PREF_IMAGE_PATH_EXPAND= B.sys.expandpath(PREF_IMAGE_PATH) + '.png'
	
	# TEST THE FILE WRITING.
	try:
		# Can we write to this file???
		f= open(PREF_IMAGE_PATH_EXPAND, 'w')
		f.close()
	except:
		B.Draw.PupMenu('Error%t|Could not write to path|' + PREF_IMAGE_PATH_EXPAND)
		return
	
	render_context.imageSizeX(PREF_IMAGE_SIZE)
	render_context.imageSizeY(PREF_IMAGE_SIZE)
	render_context.enableOversampling(True) 
	render_context.setOversamplingLevel(16) 
	render_context.setRenderWinSize(100)
	render_context.setImageType(Render.PNG)
	render_context.enableExtensions(True) 
	render_context.enablePremultiply() # No alpha needed.
	render_context.enableRGBAColor()
	render_context.threads = 2
	
	#Render.EnableDispView() # Broken??
	
	# New Mesh and Object
	render_mat= B.Material.New()
	render_mat.mode |= \
			B.Material.Modes.SHADELESS | \
			B.Material.Modes.TEXFACE | \
			B.Material.Modes.TEXFACE_ALPHA | \
			B.Material.Modes.ZTRANSP
	
	render_mat.setAlpha(0.0)
		
	render_me= B.Mesh.New()
	render_me.verts.extend([Vector(0,0,0)]) # Stupid, dummy vert, preverts errors. when assigning UV's/
	render_ob= B.Object.New('Mesh')
	render_ob.link(render_me)
	render_scn.link(render_ob)
	render_me.materials= [render_mat]
	
	
	# New camera and object
	render_cam_data= B.Camera.New('ortho')
	render_cam_ob= B.Object.New('Camera')
	render_cam_ob.link(render_cam_data)
	render_scn.link(render_cam_ob)
	render_scn.objects.camera = render_cam_ob
	
	render_cam_data.type= 'ortho'
	render_cam_data.scale= 1.0
	
	
	# Position the camera
	render_cam_ob.LocZ= 1.0
	render_cam_ob.LocX= 0.5
	render_cam_ob.LocY= 0.5
	
	# List to send to to boxpack function.
	boxes2Pack= [ fg.box_pack for fg in face_groups.itervalues()]
	packWidth, packHeight = B.Geometry.BoxPack2D(boxes2Pack)
	
	if PREF_KEEP_ASPECT:
		packWidth= packHeight= max(packWidth, packHeight)
	
	
	# packedLs is a list of [(anyUniqueID, left, bottom, width, height)...]
	# Re assign the face groups boxes to the face_group.
	for box in boxes2Pack:
		face_groups[ box[4] ].box_pack= box # box[4] is the ID (image name)
	
	
	# Add geometry to the mesh
	for fg in face_groups.itervalues():
		# Add verts clockwise from the bottom left.
		_x= fg.box_pack[0] / packWidth
		_y= fg.box_pack[1] / packHeight
		_w= fg.box_pack[2] / packWidth
		_h= fg.box_pack[3] / packHeight
		
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
		
		target_face= render_me.faces[-1]
		target_face.image= fg.image
		target_face.mode |= TEXMODE
		
		# Set the UV's, we need to flip them HOZ?
		target_face.uv[0].x= target_face.uv[1].x= fg.xmax
		target_face.uv[2].x= target_face.uv[3].x= fg.xmin
		
		target_face.uv[0].y= target_face.uv[3].y= fg.ymin
		target_face.uv[1].y= target_face.uv[2].y= fg.ymax
		
		for uv in target_face.uv:
			uv_rot= ((uv-fg.cent) * fg.rot_mat[1]) + fg.cent
			uv.x= uv_rot.x
			uv.y= uv_rot.y
	
	render_context.render()
	Render.CloseRenderWindow()
	render_context.saveRenderedImage(PREF_IMAGE_PATH_EXPAND)
	
	#if not B.sys.exists(PREF_IMAGE_PATH_EXPAND):
	#	raise 'Error!!!'
	
	
	# NOW APPLY THE SAVED IMAGE TO THE FACES!
	#print PREF_IMAGE_PATH_EXPAND
	try:
		target_image= B.Image.Load(PREF_IMAGE_PATH_EXPAND)
	except:
		B.Draw.PupMenu('Error: Could not render or load the image at path|' + PREF_IMAGE_PATH_EXPAND)
		return
	
	# Set to the 1 image.
	for me in mesh_list:
		for f in me.faces:
			if f.mode & TEXMODE and f.image:
				f.image= target_image
	
	for fg in face_groups.itervalues():
		fg.move2packed(packWidth, packHeight)
	
	scn.makeCurrent()
	render_me.verts= None # free a tiny amount of memory.
	B.Scene.Unlink(render_scn)
	target_image.makeCurrent()


def main():
	scn= B.Scene.GetCurrent()
	scn_objects = scn.objects
	ob= scn_objects.active
	
	if not ob or ob.type != 'Mesh':
		B.Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	# Create the variables.
	# Filename without path or extension.
	newpath= B.Get('filename').split('/')[-1].split('\\')[-1].replace('.blend', '')
	
	PREF_IMAGE_PATH = B.Draw.Create('//%s_grp' % newpath)
	PREF_IMAGE_SIZE = B.Draw.Create(1024)
	PREF_IMAGE_MARGIN = B.Draw.Create(6)
	PREF_KEEP_ASPECT = B.Draw.Create(0)
	PREF_ALL_SEL_OBS = B.Draw.Create(0)
	
	pup_block = [\
	'Image Path: (no ext)',\
	('', PREF_IMAGE_PATH, 3, 100, 'Path to new Image. "//" for curent blend dir.'),\
	'Image Options',
	('Pixel Size:', PREF_IMAGE_SIZE, 64, 4096, 'Image Width and Height.'),\
	('Pixel Margin:', PREF_IMAGE_MARGIN, 0, 64, 'Use a margin to stop mipmapping artifacts.'),\
	('Keep Aspect', PREF_KEEP_ASPECT, 'If disabled, will stretch the images to the bounds of the texture'),\
	'Texture Source',\
	('All Sel Objects', PREF_ALL_SEL_OBS, 'Combine all selected objects into 1 texture, otherwise active object only.'),\
	]
	
	if not B.Draw.PupBlock('Consolidate images...', pup_block):
		return
	
	PREF_IMAGE_PATH= PREF_IMAGE_PATH.val
	PREF_IMAGE_SIZE= PREF_IMAGE_SIZE.val
	PREF_IMAGE_MARGIN= float(PREF_IMAGE_MARGIN.val) # important this is a float otherwise division wont work properly
	PREF_KEEP_ASPECT= PREF_KEEP_ASPECT.val
	PREF_ALL_SEL_OBS= PREF_ALL_SEL_OBS.val
	
	if PREF_ALL_SEL_OBS:
		mesh_list= [ob.getData(mesh=1) for ob in scn_objects.context if ob.type=='Mesh']
		# Make sure we have no doubles- dict by name, then get the values back.
		
		for me in mesh_list: me.tag = False
		
		mesh_list_new = []
		for me in mesh_list:
			if me.faceUV and me.tag==False:
				me.tag = True
				mesh_list_new.append(me)
		
		# replace list with possible doubles
		mesh_list = mesh_list_new
		
	else:
		mesh_list= [ob.getData(mesh=1)]
		if not mesh_list[0].faceUV:
			B.Draw.PupMenu('Error, active mesh has no images, Aborting!')
			return
	
	consolidate_mesh_images(mesh_list, scn, PREF_IMAGE_PATH, PREF_IMAGE_SIZE, PREF_KEEP_ASPECT, PREF_IMAGE_MARGIN)
	B.Window.RedrawAll()
	
if __name__=='__main__':
	main()
