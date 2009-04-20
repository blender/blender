import Blender
from Blender import Scene, sys, Camera, Object, Image
from Blender.Scene import Render
Vector= Blender.Mathutils.Vector


def extFromFormat(format):
	if format == Render.TARGA: return 'tga'
	if format == Render.RAWTGA: return 'tga'
	if format == Render.HDR: return 'hdr'
	if format == Render.PNG: return 'png'
	if format == Render.BMP: return 'bmp'
	if format == Render.JPEG: return 'jpg'
	if format == Render.HAMX: return 'ham'
	if format == Render.TIFF: return 'tif'
	if format == Render.CINEON: return 'cine'
	if format == Render.DPX: return 'tif'
	if format == Render.OPENEXR: return 'exr'
	if format == Render.IRIS: return 'rgb'
	return ''

	

def imageFromObjectsOrtho(objects, path, width, height, smooth, alpha= True, camera_matrix= None, format=Render.PNG):
	'''
	Takes any number of objects and renders them on the z axis, between x:y-0 and x:y-1
	Usefull for making images from a mesh without per pixel operations
	- objects must be alredy placed
	- smooth, anti alias True/False
	- path renders to a PNG image
	- alpha weather to render background as alpha
	
	returns the blender image
	'''
	ext = '.' + extFromFormat(format)
	print ext
	# remove an extension if its alredy there
	if path.lower().endswith(ext):
		path= path[:-4] 
	
	path_expand= sys.expandpath(path) + ext
	
	print path_expand, 'path'
	
	# Touch the path
	try:
		f= open(path_expand, 'w')
		f.close()
	except:
		raise 'Error, could not write to path:' + path_expand
	
	
	# RENDER THE FACES.
	scn= Scene.GetCurrent()
	render_scn= Scene.New()
	render_scn.makeCurrent()
	render_scn.Layers |= (1<<20)-1 # all layers enabled
	 
	# Add objects into the current scene
	for ob in objects:
		render_scn.link(ob)
	
	render_context= render_scn.getRenderingContext()
	render_context.setRenderPath('') # so we can ignore any existing path and save to the abs path.
	
	
	render_context.imageSizeX(width)
	render_context.imageSizeY(height)
	
	if smooth:
		render_context.enableOversampling(True) 
		render_context.setOversamplingLevel(16)
	else:
		render_context.enableOversampling(False) 
	
	render_context.setRenderWinSize(100)
	render_context.setImageType(format)
	render_context.enableExtensions(True) 
	#render_context.enableSky() # No alpha needed.
	if alpha:
		render_context.alphaMode= 1
		render_context.enableRGBAColor()
	else:
		render_context.alphaMode= 0
		render_context.enableRGBColor()
	
	render_context.displayMode= 0 # fullscreen
	
	# New camera and object
	render_cam_data= Camera.New('ortho')
	render_cam_ob= Object.New('Camera')
	render_cam_ob.link(render_cam_data)
	render_scn.link(render_cam_ob)
	render_scn.objects.camera = render_cam_ob
	
	render_cam_data.type= 'ortho'
	
	
	
	# Position the camera
	if camera_matrix:
		render_cam_ob.setMatrix(camera_matrix)
		# We need to take into account the matrix scaling when setting the size
		# so we get the image bounds defined by the matrix
		# first get the x and y factors from the matrix.
		# To render the correct dimensions we must use the aspy and aspy to force the matrix scale to
		# override the aspect enforced by the width and weight.
		cent= Vector() * camera_matrix
		xvec= Vector(1,0,0) * camera_matrix
		yvec= Vector(0,1,0) * camera_matrix
		# zvec= Vector(0,0,1) * camera_matrix
		xlen = (cent-xvec).length # half height of the image
		ylen = (cent-yvec).length # half width of the image
		# zlen = (cent-zvec).length # dist to place the camera? - just use the loc for now.
		
		
		# less then 1.0 portrate, 1.0 or more is portrate
		asp_cam_mat= xlen/ylen # divide by zero? - possible but scripters fault.
		asp_image_res= float(width)/height
		#print 'asp quad', asp_cam_mat, 'asp_image', asp_image_res
		#print 'xylen', xlen, ylen, 'w/h', width, height
		# Setup the aspect
		
		if asp_cam_mat > asp_image_res:
			# camera is wider then image res.
			# to make the image wider, reduce the aspy
			asp_diff= asp_image_res/asp_cam_mat
			min_asp= asp_diff * 200
			#print 'X', min_asp
			
		elif asp_cam_mat < asp_image_res: # asp_cam_mat < asp_image_res
			# camera is narrower then image res
			# to make the image narrower, reduce the aspx
			asp_diff= asp_cam_mat/asp_image_res
			min_asp= asp_diff * 200
			#print 'Y', min_asp
		else:
			min_asp= 200
		
		# set the camera size
		if xlen > ylen:
			if asp_cam_mat > asp_image_res:
				render_context.aspectX= 200 # get the greatest range possible
				render_context.aspectY= min_asp # get the greatest range possible
			else:
				render_context.aspectY= 200 # get the greatest range possible
				render_context.aspectX= min_asp # get the greatest range possible
			#print "xlen bigger"
			render_cam_data.scale= xlen * 2
		elif xlen < ylen:# ylen is bigger
			if asp_cam_mat > asp_image_res:
				render_context.aspectX= 200 # get the greatest range possible
				render_context.aspectY= min_asp # get the greatest range possible
			else:
				render_context.aspectY= 200 # get the greatest range possible
				render_context.aspectX= min_asp # get the greatest range possible
			#print "ylen bigger"
			render_cam_data.scale= ylen *2 
		else:
			# asppect 1:1
			#print 'NOLEN Bigger'
			render_cam_data.scale= xlen * 2

		#print xlen, ylen, 'xlen, ylen'
		
	else:
		if width > height:
			min_asp = int((float(height) / width) * 200)
			render_context.aspectX= min_asp
			render_context.aspectY= 200
		else:
			min_asp = int((float(width) / height) * 200)
			render_context.aspectX= 200
			render_context.aspectY= min_asp
		
		
		render_cam_data.scale= 1.0
		render_cam_ob.LocZ= 1.0
		render_cam_ob.LocX= 0.5
		render_cam_ob.LocY= 0.5
	
	Blender.Window.RedrawAll()
	
	render_context.render()
	render_context.saveRenderedImage(path)
	Render.CloseRenderWindow()
	#if not B.sys.exists(PREF_IMAGE_PATH_EXPAND):
	#	raise 'Error!!!'
	
	scn.makeCurrent()
	Scene.Unlink(render_scn)
	
	# NOW APPLY THE SAVED IMAGE TO THE FACES!
	#print PREF_IMAGE_PATH_EXPAND
	try:
		target_image= Image.Load(path_expand)
		return target_image
	except:
		raise 'Error: Could not render or load the image at path "%s"' % path_expand
		return



#-----------------------------------------------------------------------------#
# UV Baking functions, make a picture from mesh(es) uvs                       #
#-----------------------------------------------------------------------------#

def mesh2uv(me_s, PREF_SEL_FACES_ONLY=False):
	'''
	Converts a uv mapped mesh into a 2D Mesh from UV coords.
	returns a triple -
	(mesh2d, face_list, col_list)
	"mesh" is the new mesh and...
	"face_list" is the faces that were used to make the mesh,
	"material_list" is a list of materials used by each face
	These are in alligned with the meshes faces, so you can easerly copy data between them
	
	'''
	render_me= Blender.Mesh.New()
	render_me.verts.extend( [Vector(0,0,0),] ) # 0 vert uv bugm dummy vert
	face_list= []
	material_list= []
	for me in me_s:
		me_materials= me.materials
		if PREF_SEL_FACES_ONLY:
			me_faces= [f for f in me.faces if f.sel]
		else:
			me_faces= me.faces
		
		face_list.extend(me_faces)
		
		# Dittro
		if me_materials:
			material_list.extend([me_materials[f.mat] for f in me_faces])
		else:
			material_list.extend([None]*len(me_faces))
		
	# Now add the verts
	render_me.verts.extend( [ Vector(uv.x, uv.y, 0) for f in face_list for uv in f.uv ] )
	
	# Now add the faces
	tmp_faces= []
	vert_offset= 1
	for f in face_list:
		tmp_faces.append( [ii+vert_offset for ii in xrange(len(f))] )
		vert_offset+= len(f)
	
	render_me.faces.extend(tmp_faces)
	render_me.faceUV=1
	return render_me, face_list, material_list


def uvmesh_apply_normals(render_me, face_list):
	'''Worldspace normals to vertex colors'''
	for i, f in enumerate(render_me.faces):
		face_orig= face_list[i]
		f_col= f.col
		for j, v in enumerate(face_orig):
			c= f_col[j]
			nx, ny, nz= v.no
			c.r= int((nx+1)*128)-1
			c.g= int((ny+1)*128)-1
			c.b= int((nz+1)*128)-1

def uvmesh_apply_image(render_me, face_list):
	'''Copy the image and uvs from the original faces'''
	for i, f in enumerate(render_me.faces):
		f.uv= face_list[i].uv
		f.image= face_list[i].image


def uvmesh_apply_vcol(render_me, face_list):
	'''Copy the vertex colors from the original faces'''
	for i, f in enumerate(render_me.faces):
		face_orig= face_list[i]
		f_col= f.col
		for j, c_orig in enumerate(face_orig.col):
			c= f_col[j]
			c.r= c_orig.r
			c.g= c_orig.g
			c.b= c_orig.b

def uvmesh_apply_matcol(render_me, material_list):
	'''Get the vertex colors from the original materials'''
	for i, f in enumerate(render_me.faces):
		mat_orig= material_list[i]
		f_col= f.col
		if mat_orig:
			for c in f_col:
				c.r= int(mat_orig.R*255)
				c.g= int(mat_orig.G*255)
				c.b= int(mat_orig.B*255)
		else:
			for c in f_col:
				c.r= 255
				c.g= 255
				c.b= 255

def uvmesh_apply_col(render_me, color):
	'''Get the vertex colors from the original materials'''
	r,g,b= color
	for i, f in enumerate(render_me.faces):
		f_col= f.col
		for c in f_col:
			c.r= r
			c.g= g
			c.b= b


def vcol2image(me_s,\
	PREF_IMAGE_PATH,\
	PREF_IMAGE_SIZE,\
	PREF_IMAGE_BLEED,\
	PREF_IMAGE_SMOOTH,\
	PREF_IMAGE_WIRE,\
	PREF_IMAGE_WIRE_INVERT,\
	PREF_IMAGE_WIRE_UNDERLAY,\
	PREF_USE_IMAGE,\
	PREF_USE_VCOL,\
	PREF_USE_MATCOL,\
	PREF_USE_NORMAL,\
	PREF_USE_TEXTURE,\
	PREF_SEL_FACES_ONLY):
	
	
	def rnd_mat():
		render_mat= Blender.Material.New()
		mode= render_mat.mode
		
		# Dont use lights ever
		mode |= Blender.Material.Modes.SHADELESS
		
		if PREF_IMAGE_WIRE:
			# Set the wire color
			if PREF_IMAGE_WIRE_INVERT:
				render_mat.rgbCol= (1,1,1)
			else:
				render_mat.rgbCol= (0,0,0)
			
			mode |= Blender.Material.Modes.WIRE
		if PREF_USE_VCOL or PREF_USE_MATCOL or PREF_USE_NORMAL: # both vcol and material color use vertex cols to avoid the 16 max limit in materials
			mode |= Blender.Material.Modes.VCOL_PAINT
		if PREF_USE_IMAGE:
			mode |= Blender.Material.Modes.TEXFACE
		
		# Copy back the mode
		render_mat.mode |= mode
		return render_mat
	
	
	render_me, face_list, material_list= mesh2uv(me_s, PREF_SEL_FACES_ONLY)

	# Normals exclude all others
	if PREF_USE_NORMAL:
		uvmesh_apply_normals(render_me, face_list)
	else:
		if PREF_USE_IMAGE:
			uvmesh_apply_image(render_me, face_list)
			uvmesh_apply_vcol(render_me, face_list)
	
		elif PREF_USE_VCOL:
			uvmesh_apply_vcol(render_me, face_list)
		
		elif PREF_USE_MATCOL:
			uvmesh_apply_matcol(render_me, material_list)
		
		elif PREF_USE_TEXTURE:
			# if we have more then 16 materials across all the mesh objects were stuffed :/
			# get unique materials
			tex_unique_materials= dict([(mat.name, mat) for mat in material_list]).values()[:16] # just incase we have more then 16 
			tex_me= Blender.Mesh.New()
			
			# Backup the original shadless setting
			tex_unique_materials_shadeless= [ mat.mode & Blender.Material.Modes.SHADELESS for mat in tex_unique_materials ]
			
			# Turn shadeless on
			for mat in tex_unique_materials:
				mat.mode |= Blender.Material.Modes.SHADELESS
			
			# Assign materials
			render_me.materials= tex_unique_materials
			
			
			
			tex_material_indicies= dict([(mat.name, i) for i, mat in enumerate(tex_unique_materials)])
			
			tex_me.verts.extend([Vector(0,0,0),]) # dummy
			tex_me.verts.extend( [ Vector(v.co) for f in face_list for v in f ] )
			
			# Now add the faces
			tmp_faces= []
			vert_offset= 1
			for f in face_list:
				tmp_faces.append( [ii+vert_offset for ii in xrange(len(f))] )
				vert_offset+= len(f)
			
			tex_me.faces.extend(tmp_faces)
			
			# Now we have the faces, put materials and normal, uvs into the mesh
			if len(tex_me.faces) != len(face_list):
				# Should never happen
				raise "Error face length mismatch"
			
			# Copy data to the mesh that could be used as texture coords
			for i, tex_face in enumerate(tex_me.faces):
				orig_face= face_list[i]
				
				# Set the material index
				try:
					render_face.mat= tex_material_indicies[ material_list[i].name ]
				except:
					# more then 16 materials
					pass
				
				
				# set the uvs on the texmesh mesh
				tex_face.uv= orig_face.uv
				
				orig_face_v= orig_face.v
				# Set the normals
				for j, v in enumerate(tex_face):
					v.no= orig_face_v[j].no
			
			# Set the texmesh
			render_me.texMesh= tex_me
		# END TEXMESH
			
			
	# Handel adding objects
	render_ob= Blender.Object.New('Mesh')
	render_ob.link(render_me)
	
	if not PREF_USE_TEXTURE: # textures use the original materials
		render_me.materials= [rnd_mat()]
	
	
	obs= [render_ob]
	
	
	if PREF_IMAGE_WIRE_UNDERLAY:
		# Make another mesh with the material colors
		render_me_under, face_list, material_list= mesh2uv(me_s, PREF_SEL_FACES_ONLY)
		
		uvmesh_apply_matcol(render_me_under, material_list)
		
		# Handel adding objects
		render_ob= Blender.Object.New('Mesh')
		render_ob.link(render_me_under)
		render_ob.LocZ= -0.01
		
		# Add material and disable wire
		mat= rnd_mat()
		mat.rgbCol= 1,1,1
		mat.alpha= 0.5
		mat.mode &= ~Blender.Material.Modes.WIRE
		mat.mode |= Blender.Material.Modes.VCOL_PAINT
		
		render_me_under.materials= [mat]
		
		obs.append(render_ob)
		
	elif PREF_IMAGE_BLEED and not PREF_IMAGE_WIRE:
		# EVIL BLEEDING CODE!! - Just do copys of the mesh and place behind. Crufty but better then many other methods I have seen. - Cam
		BLEED_PIXEL= 1.0/PREF_IMAGE_SIZE
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
	
	
	
	image= imageFromObjectsOrtho(obs, PREF_IMAGE_PATH, PREF_IMAGE_SIZE, PREF_IMAGE_SIZE, PREF_IMAGE_SMOOTH)
	
	# Clear from memory as best as we can
	render_me.verts= None
	
	if PREF_IMAGE_WIRE_UNDERLAY:
		render_me_under.verts= None
	
	if PREF_USE_TEXTURE:
		tex_me.verts= None
		# Restire Shadeless setting
		for i, mat in enumerate(tex_unique_materials):
			# we know there all on so turn it off of its not set
			if not tex_unique_materials_shadeless[i]:
				mat.mode &= ~Blender.Material.Modes.SHADELESS
	
	return image

def bakeToPlane(sce, ob_from, width, height, bakemodes, axis='z', margin=0, depth=32):
	'''
	Bakes terrain onto a plane from one object
	sce - scene to bake with
	ob_from - mesh object
	width/height - image size
	bakemodes - list of baking modes to use, Blender.Scene.Render.BakeModes.NORMALS, Blender.Scene.Render.BakeModes.AO ... etc
	axis - axis to allign the plane to.
	margin - margin setting for baking.
	depth - bit depth for the images to bake into, (32 or 128 for floating point images)
	Example:
		import Blender
		from Blender import *
		import BPyRender
		sce = Scene.GetCurrent()
		ob = Object.Get('Plane')
		BPyRender.bakeToPlane(sce, ob, 512, 512, [Scene.Render.BakeModes.DISPLACEMENT, Scene.Render.BakeModes.NORMALS], 'z', 8 )
	'''
	
	# Backup bake settings
	rend = sce.render
	BACKUP_bakeDist = rend.bakeDist
	BACKUP_bakeBias = rend.bakeBias
	BACKUP_bakeMode = rend.bakeMode
	BACKUP_bakeClear = rend.bakeClear
	BACKUP_bakeMargin = rend.bakeMargin
	BACKUP_bakeToActive = rend.bakeToActive
	BACKUP_bakeNormalize = rend.bakeNormalize
	
	# Backup object selection
	BACKUP_obsel = list(sce.objects.selected)
	BACKUP_obact = sce.objects.active
	
	# New bake settings
	rend.bakeClear = True
	rend.bakeMargin = margin
	rend.bakeToActive  = True
	rend.bakeNormalize = True
	
	# Assume a mesh
	me_from = ob_from.getData(mesh=1)
	
	xmin = ymin = zmin = 10000000000
	xmax = ymax = zmax =-10000000000
	
	# Dont trust bounding boxes :/
	#bounds = ob_from.boundingBox
	#for v in bounds:
	#	x,y,z = tuple(v)
	mtx = ob_from.matrixWorld
	for v in me_from.verts:
		x,y,z = tuple(v.co*mtx)
		
		xmax = max(xmax, x)
		ymax = max(ymax, y)
		zmax = max(zmax, z)
		
		xmin = min(xmin, x)
		ymin = min(ymin, y)
		zmin = min(zmin, z)
	
	if axis=='x':
		xmed = (xmin+xmax)/2.0
		co1 = (xmed, ymin, zmin)
		co2 = (xmed, ymin, zmax)
		co3 = (xmed, ymax, zmax)
		co4 = (xmed, ymax, zmin)
		rend.bakeDist = ((xmax-xmin)/2.0) + 0.000001 # we need a euler value for this since it 
	elif axis=='y':
		ymed = (ymin+ymax)/2.0
		co1 = (xmin, ymed, zmin)
		co2 = (xmin, ymed, zmax)
		co3 = (xmax, ymed, zmax)
		co4 = (xmax, ymed, zmin)
		rend.bakeDist = ((ymax-ymin)/2.0) + 0.000001
	elif axis=='z':
		zmed = (zmin+zmax)/2.0
		co1 = (xmin, ymin, zmed)
		co2 = (xmin, ymax, zmed)
		co3 = (xmax, ymax, zmed)
		co4 = (xmax, ymin, zmed)
		rend.bakeDist = ((zmax-zmin)/2.0) + 0.000001
	else:
		raise "invalid axis"
	me_plane = Blender.Mesh.New()
	ob_plane = Blender.Object.New('Mesh')
	ob_plane.link(me_plane)
	sce.objects.link(ob_plane)
	ob_plane.Layers = ob_from.Layers
	
	ob_from.sel = 1 # make active
	sce.objects.active = ob_plane
	ob_plane.sel = 1
	
	me_plane.verts.extend([co4, co3, co2, co1])
	me_plane.faces.extend([(0,1,2,3)])
	me_plane.faceUV = True
	me_plane_face = me_plane.faces[0]
	uvs = me_plane_face.uv
	uvs[0].x = 0.0;	uvs[0].y = 0.0
	uvs[1].x = 0.0;	uvs[1].y = 1.0
	uvs[2].x = 1.0;	uvs[2].y = 1.0
	uvs[3].x = 1.0;	uvs[3].y = 0.0
	
	images_return = []
	
	for mode in bakemodes:
		img = Blender.Image.New('bake', width, height, depth)
		
		me_plane_face.image = img
		rend.bakeMode = mode
		rend.bake()
		images_return.append( img )
	
	# Restore bake settings
	#'''
	rend.bakeDist = BACKUP_bakeDist
	rend.bakeBias = BACKUP_bakeBias
	rend.bakeMode = BACKUP_bakeMode
	rend.bakeClear = BACKUP_bakeClear
	rend.bakeMargin = BACKUP_bakeMargin
	rend.bakeToActive = BACKUP_bakeToActive
	rend.bakeNormalize = BACKUP_bakeNormalize
	
	
	# Restore obsel
	sce.objects.selected = BACKUP_obsel
	sce.objects.active = BACKUP_obact
	
	me_plane.verts = None
	sce.objects.unlink(ob_plane)
	#'''
	
	return images_return

