#!BPY

"""
Name: 'Auto Image Layout'
Blender: 241
Group: 'UV'
Tooltip: 'Pack all texture images into 1 image and remap faces.'
"""

__author__ = "Campbell Barton"
__url__ = ("blender", "blenderartists.org")
__version__ = "1.0 2005/05/20"

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
import boxpack2d
from Blender.Mathutils import Vector
from Blender.Scene import Render

BIGNUM= 1<<30
class faceGroup(object):
	__slots__= 'xmax', 'ymax', 'xmin', 'ymin',\
	'image', 'faces', 'box_pack', 'size'\
	
	def __init__(self, mesh_list, image, size, PREF_IMAGE_MARGIN):
		self.image= image
		self.size= size
		# Add to our face group and set bounds.
		xmin=ymin= BIGNUM
		xmax=ymax= -BIGNUM
		self.faces= []
		for me in mesh_list:
			for f in me.faces:
				if f.image==image:
					self.faces.append(f)
					for uv in f.uv:
						xmax= max(xmax, uv.x)
						xmin= min(xmin, uv.x)
						ymax= max(ymax, uv.y)
						ymin= min(ymin, uv.y)
					
		# The box pack list is to be passed to the external function "boxpack2d"
		# format is ID, w,h
		
		# Store the bounds, impliment the margin.
		self.xmax= xmax + (PREF_IMAGE_MARGIN/size[0])
		self.xmin= xmin - (PREF_IMAGE_MARGIN/size[0])
		self.ymax= ymax + (PREF_IMAGE_MARGIN/size[1])
		self.ymin= ymin - (PREF_IMAGE_MARGIN/size[1])
		
		self.box_pack=[\
		image.name,\
		size[0]*(self.xmax - self.xmin),\
		size[1]*(self.ymax - self.ymin)] 
		
		
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
		offset_x= self.box_pack[1]/width
		offset_y= self.box_pack[2]/height
		
		for f in self.faces:
			for uv in f.uv:
				uv.x= offset_x+ (((uv.x-self.xmin) * self.size[0])/width)
				uv.y= offset_y+ (((uv.y-self.ymin) * self.size[1])/height)

def auto_layout_tex(mesh_list, scn, PREF_IMAGE_PATH, PREF_IMAGE_SIZE, PREF_KEEP_ASPECT, PREF_IMAGE_MARGIN): #, PREF_SIZE_FROM_UV=True):
	
	# Get all images used by the mesh
	face_groups= {}
	
	for me in mesh_list:
		for f in me.faces:
			if f.image:
				try:
					face_groups[f.image.name] # will fail if teh groups not added.
				except:
					image= f.image
					try:
						size= image.size
					except:
						B.Draw.PupMenu('Aborting: Image cold not be loaded|' + image.name)
						return
						
					face_groups[f.image.name]= faceGroup(mesh_list, f.image, size, PREF_IMAGE_MARGIN)
	
	if not face_groups:
		B.Draw.PupMenu('No Images found in mesh. aborting.')
		return
	
	if len(face_groups)==1:
		B.Draw.PupMenu('Only 1 image found|use meshes using 2 or more images.')
		return
		
	'''
	if PREF_SIZE_FROM_UV:
		for fg in face_groups.itervalues():
			fg.set_worldspace_scale()
	'''
	
	# RENDER THE FACES.
	render_scn= B.Scene.New()
	render_scn.makeCurrent()
	
	
	# Set the render context
	
	PREF_IMAGE_PATH_EXPAND= B.sys.expandpath(PREF_IMAGE_PATH) + '.png'
	
	# TEST THE FILE WRITING.
	try:
		# Can we write to this file???
		f= open(PREF_IMAGE_PATH_EXPAND, 'w')
		f.close()
	except:
		B.Draw.PupMenu('Error: Could not write to path|' + PREF_IMAGE_PATH_EXPAND)
		return
	
	render_context= render_scn.getRenderingContext()
	render_context.imageSizeX(PREF_IMAGE_SIZE)
	render_context.imageSizeY(PREF_IMAGE_SIZE)
	render_context.startFrame(1) 
	render_context.endFrame(1)
	render_context.enableOversampling(True) 
	render_context.setOversamplingLevel(16) 
	render_context.setRenderWinSize(100)
	render_context.setImageType(Render.PNG)
	render_context.enableExtensions(True) 
	render_context.enableSky() # No alpha needed.
	render_context.enableRGBColor()
	
	Render.EnableDispView() # Broken??
	
	# New Mesh and Object
	render_mat= B.Material.New()
	render_mat.mode |= B.Material.Modes.SHADELESS
	render_mat.mode |= B.Material.Modes.TEXFACE
	
	
	render_me= B.Mesh.New()
	render_me.verts.extend([Vector(0,0,0)]) # Stupid, dummy vert, preverts errors.
	render_ob= B.Object.New('Mesh')
	render_ob.link(render_me)
	render_scn.link(render_ob)
	render_me.materials= [render_mat]
	
	
	# New camera and object
	render_cam_data= B.Camera.New('ortho')
	render_cam_ob= B.Object.New('Camera')
	render_cam_ob.link(render_cam_data)
	render_scn.link(render_cam_ob)
	render_scn.setCurrentCamera(render_cam_ob)
	
	render_cam_data.type= 1 # ortho
	render_cam_data.scale= 1.0
	
	
	# Position the camera
	render_cam_ob.LocZ= 1.0 # set back 1
	render_cam_ob.LocX= 0.5 # set back 1
	render_cam_ob.LocY= 0.5 # set back 1
	#render_cam_ob.RotY= 180 * 0.017453292519943295 # pi/180.0
	
	# List to send to to boxpack function.
	boxes2Pack= [ fg.box_pack for fg in face_groups.itervalues()]
	
	packWidth, packHeight, packedLs = boxpack2d.boxPackIter(boxes2Pack)
	
	if PREF_KEEP_ASPECT:
		packWidth= packHeight= max(packWidth, packHeight)
	
	
	# packedLs is a list of [(anyUniqueID, left, bottom, width, height)...]
	# Re assign the face groups boxes to the face_group.
	for box in packedLs:
		face_groups[ box[0] ].box_pack= box # box[0] is the ID (image name)
	
	
	# Add geometry to the mesh
	for fg in face_groups.itervalues():
		# Add verts clockwise from the bottom left.
		_x= fg.box_pack[1] / packWidth
		_y= fg.box_pack[2] / packHeight
		_w= fg.box_pack[3] / packWidth
		_h= fg.box_pack[4] / packHeight
		
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
		target_face.mode |= B.Mesh.FaceModes.TEX
		
		# Set the UV's, we need to flip them HOZ?
		target_face.uv[0].x= target_face.uv[1].x= fg.xmax
		target_face.uv[2].x= target_face.uv[3].x= fg.xmin
		
		target_face.uv[0].y= target_face.uv[3].y= fg.ymin
		target_face.uv[1].y= target_face.uv[2].y= fg.ymax
		
		# VCOLS
		# Set them white.
		for c in target_face.col:
			c.r= c.g= c.b= 255
	
	render_context.render()
	render_context.saveRenderedImage(PREF_IMAGE_PATH_EXPAND)
	
	#if not B.sys.exists(PREF_IMAGE_PATH):
	#	raise 'Error!!!'
	Render.CloseRenderWindow()
	
	# NOW APPLY THE SAVED IMAGE TO THE FACES!
	#print PREF_IMAGE_PATH_EXPAND
	target_image= B.Image.Load(PREF_IMAGE_PATH_EXPAND)
	
	# Set to the 1 image.
	for me in mesh_list:
		for f in me.faces:
			if f.image:
				f.image= target_image
	
	for fg in face_groups.itervalues():
		fg.move2packed(packWidth, packHeight)
		
	scn.makeCurrent()
	B.Scene.Unlink(render_scn)


def main():
	scn= B.Scene.GetCurrent()
	ob= scn.getActiveObject()
	
	if not ob or ob.getType() != 'Mesh':
		B.Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	# Create the variables.
	# Filename without path or extension.
	newpath= B.Get('filename').split('/')[-1].split('\\')[-1].replace('.blend', '')
	
	PREF_IMAGE_PATH = B.Draw.Create('//%s_grp' % newpath)
	PREF_IMAGE_SIZE = B.Draw.Create(512)
	PREF_IMAGE_MARGIN = B.Draw.Create(6)
	PREF_KEEP_ASPECT = B.Draw.Create(1)
	PREF_ALL_SEL_OBS = B.Draw.Create(0)
	
	pup_block = [\
	'image path: no ext',\
	('', PREF_IMAGE_PATH, 3, 100, 'Path to new Image. "//" for curent blend dir.'),\
	'Image Options',
	('Pixel Size:', PREF_IMAGE_SIZE, 64, 4096, 'Image Width and Height.'),\
	('Pixel Margin:', PREF_IMAGE_MARGIN, 0, 64, 'Image Width and Height.'),\
	('Keep Image Aspect', PREF_KEEP_ASPECT, 'If disabled, will stretch the images to the bounds of the texture'),\
	'Texture Source',\
	('All Sel Objects', PREF_ALL_SEL_OBS, 'Combine and replace textures from all objects into 1 texture.'),\
	]
	
	if not B.Draw.PupBlock('Auto Texture Layout', pup_block):
		return
	
	PREF_IMAGE_PATH= PREF_IMAGE_PATH.val
	PREF_IMAGE_SIZE= PREF_IMAGE_SIZE.val
	PREF_IMAGE_MARGIN= PREF_IMAGE_MARGIN.val
	PREF_KEEP_ASPECT= PREF_KEEP_ASPECT.val
	PREF_ALL_SEL_OBS= PREF_ALL_SEL_OBS.val
	
	if PREF_ALL_SEL_OBS:
		mesh_list= [ob.getData(mesh=1) for ob in B.Object.GetSelected() if ob.getType()=='Mesh']
		# Make sure we have no doubles- dict by name, then get the values back.
		mesh_list= dict([(me.name, me) for me in mesh_list])
		mesh_list= mesh_list.values()
	else:
		mesh_list= [ob.getData(mesh=1)]
	
	auto_layout_tex(mesh_list, scn, PREF_IMAGE_PATH, PREF_IMAGE_SIZE, PREF_KEEP_ASPECT, PREF_IMAGE_MARGIN)

if __name__=='__main__':
	main()
