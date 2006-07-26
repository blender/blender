from Blender import Scene, sys, Camera, Object, Image
from Blender.Scene import Render

def imageFromObjectsOrtho(objects, path, width, height, alpha= True):
	'''
	Takes any number of objects and renders them on the z axis, between x:y-0 and x:y-1
	Usefull for making images from a mesh without per pixel operations
	- objects must be alredy placed
	- path renders to a PNG image
	'''
	
	# remove an extension if its alredy there
	if path.lower().endswith('.png'):
		path= path[:-4] 
	
	path_expand= sys.expandpath(path) + '.png'
	
	# Touch the path
	try:
		f= open(path_expand, 'w')
		f.close()
	except:
		raise 'Error, could not write to path'
	
	
	# RENDER THE FACES.
	scn= Scene.GetCurrent()
	render_scn= Scene.New()
	render_scn.makeCurrent()
	
	# Add objects into the current scene
	for ob in objects:
		render_scn.link(ob)
		# set layers
	
	render_context= render_scn.getRenderingContext()
	render_context.setRenderPath('') # so we can ignore any existing path and save to the abs path.
	
	
	render_context.imageSizeX(width)
	render_context.imageSizeY(height)
	render_context.enableOversampling(True) 
	render_context.setOversamplingLevel(16) 
	render_context.setRenderWinSize(100)
	render_context.setImageType(Render.PNG)
	render_context.enableExtensions(True) 
	#render_context.enableSky() # No alpha needed.
	if alpha:
		render_context.alphaMode= 2
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
	render_scn.setCurrentCamera(render_cam_ob)
	
	render_cam_data.type= 1 # ortho
	render_cam_data.scale= 1.0
	
	
	# Position the camera
	render_cam_ob.LocZ= 1.0
	render_cam_ob.LocX= 0.5
	render_cam_ob.LocY= 0.5
	
	render_context.render()
	#Render.CloseRenderWindow()
	render_context.saveRenderedImage(path)
	
	#if not B.sys.exists(PREF_IMAGE_PATH_EXPAND):
	#	raise 'Error!!!'
	
	
	# NOW APPLY THE SAVED IMAGE TO THE FACES!
	#print PREF_IMAGE_PATH_EXPAND
	try:
		target_image= Image.Load(path_expand)
	except:
		raise 'Error: Could not render or load the image at path "%s"' % path_expand
		return
	
	#scn.makeCurrent()
	#Scene.Unlink(render_scn)
