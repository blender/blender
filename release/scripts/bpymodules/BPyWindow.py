import Blender
from Blender import Mathutils, Window, Scene, Draw, Mesh
from Blender.Mathutils import Matrix, Vector, Intersect

# DESCRIPTION:
# screen_x, screen_y the origin point of the pick ray
# it is either the mouse location
# localMatrix is used if you want to have the returned values in an objects localspace.
#    this is usefull when dealing with an objects data such as verts.
# or if useMid is true, the midpoint of the current 3dview
# returns
# Origin - the origin point of the pick ray
# Direction - the direction vector of the pick ray
# in global coordinates
epsilon = 1e-3 # just a small value to account for floating point errors

def mouseViewRay(screen_x, screen_y, localMatrix=None, useMid = False):
	
	# Constant function variables
	p = mouseViewRay.p
	d = mouseViewRay.d
	
	for win3d in Window.GetScreenInfo(Window.Types.VIEW3D): # we search all 3dwins for the one containing the point (screen_x, screen_y) (could be the mousecoords for example) 
		win_min_x, win_min_y, win_max_x, win_max_y = win3d['vertices']
		# calculate a few geometric extents for this window

		win_mid_x  = (win_max_x + win_min_x + 1.0) * 0.5
		win_mid_y  = (win_max_y + win_min_y + 1.0) * 0.5
		win_size_x = (win_max_x - win_min_x + 1.0) * 0.5
		win_size_y = (win_max_y - win_min_y + 1.0) * 0.5

		#useMid is for projecting the coordinates when we subdivide the screen into bins
		if useMid: # == True
			screen_x = win_mid_x
			screen_y = win_mid_y
		
		# if the given screencoords (screen_x, screen_y) are within the 3dwin we fount the right one...
		if (win_max_x > screen_x > win_min_x) and (  win_max_y > screen_y > win_min_y):
			# first we handle all pending events for this window (otherwise the matrices might come out wrong)
			Window.QHandle(win3d['id'])
			
			# now we get a few matrices for our window...
			# sorry - i cannot explain here what they all do
			# - if you're not familiar with all those matrices take a look at an introduction to OpenGL...
			pm	= Window.GetPerspMatrix()   # the prespective matrix
			pmi  = Matrix(pm); pmi.invert() # the inverted perspective matrix
			
			if (1.0 - epsilon < pmi[3][3] < 1.0 + epsilon):
				# pmi[3][3] is 1.0 if the 3dwin is in ortho-projection mode (toggled with numpad 5)
				hms = mouseViewRay.hms
				ortho_d = mouseViewRay.ortho_d
				
				# ortho mode: is a bit strange - actually there's no definite location of the camera ...
				# but the camera could be displaced anywhere along the viewing direction.
				
				ortho_d.x, ortho_d.y, ortho_d.z = Window.GetViewVector()
				ortho_d.w = 0
				
				# all rays are parallel in ortho mode - so the direction vector is simply the viewing direction
				#hms.x, hms.y, hms.z, hms.w = (screen_x-win_mid_x) /win_size_x, (screen_y-win_mid_y) / win_size_y, 0.0, 1.0
				hms[:] = (screen_x-win_mid_x) /win_size_x, (screen_y-win_mid_y) / win_size_y, 0.0, 1.0
				
				# these are the homogenious screencoords of the point (screen_x, screen_y) ranging from -1 to +1
				p=(hms*pmi) + (1000*ortho_d)
				p.resize3D()
				d[:] = ortho_d[:3]
				

			# Finally we shift the position infinitely far away in
			# the viewing direction to make sure the camera if outside the scene
			# (this is actually a hack because this function
			# is used in sculpt_mesh to initialize backface culling...)
			else:
				# PERSPECTIVE MODE: here everything is well defined - all rays converge at the camera's location
				vmi  = Matrix(Window.GetViewMatrix()); vmi.invert() # the inverse viewing matrix
				fp = mouseViewRay.fp
				
				dx = pm[3][3] * (((screen_x-win_min_x)/win_size_x)-1.0) - pm[3][0]
				dy = pm[3][3] * (((screen_y-win_min_y)/win_size_y)-1.0) - pm[3][1]
				
				fp[:] = \
				pmi[0][0]*dx+pmi[1][0]*dy,\
				pmi[0][1]*dx+pmi[1][1]*dy,\
				pmi[0][2]*dx+pmi[1][2]*dy
				
				# fp is a global 3dpoint obtained from "unprojecting" the screenspace-point (screen_x, screen_y)
				#- figuring out how to calculate this took me quite some time.
				# The calculation of dxy and fp are simplified versions of my original code
				#- so it's almost impossible to explain what's going on geometrically... sorry
				
				p[:] = vmi[3][:3]
				
				# the camera's location in global 3dcoords can be read directly from the inverted viewmatrix
				#d.x, d.y, d.z =normalize_v3(sub_v3v3(p, fp))
				d[:] = p.x-fp.x, p.y-fp.y, p.z-fp.z
				
				#print 'd', d, 'p', p, 'fp', fp
				
			
			# the direction vector is simply the difference vector from the virtual camera's position
			#to the unprojected (screenspace) point fp
			
			# Do we want to return a direction in object's localspace?
			
			if localMatrix:
				localInvMatrix = Matrix(localMatrix)
				localInvMatrix.invert()
				localInvMatrix_notrans = localInvMatrix.rotationPart()
				p = p * localInvMatrix
				d = d * localInvMatrix # normalize_v3
				
				# remove the translation from d
				d.x -= localInvMatrix[3][0]
				d.y -= localInvMatrix[3][1]
				d.z -= localInvMatrix[3][2]
				
			
			d.normalize()			
			'''
			# Debugging
			me = Blender.Mesh.New()
			me.verts.extend([p[0:3]])
			me.verts.extend([(p-d)[0:3]])
			me.edges.extend([0,1])
			ob = Blender.Scene.GetCurrent().objects.new(me)
			'''
			return True, p, d # Origin, Direction	
	
	# Mouse is not in any view, return None.
	return False, None, None

# Constant function variables
mouseViewRay.d = Vector(0,0,0) # Perspective, 3d
mouseViewRay.p = Vector(0,0,0)
mouseViewRay.fp = Vector(0,0,0)

mouseViewRay.hms = Vector(0,0,0,0) # ortho only 4d
mouseViewRay.ortho_d = Vector(0,0,0,0) # ortho only 4d


LMB= Window.MButs['L']
def mouseup():
	# Loop until click
	mouse_buttons = Window.GetMouseButtons()
	while not mouse_buttons & LMB:
		Blender.sys.sleep(10)
		mouse_buttons = Window.GetMouseButtons()
	while mouse_buttons & LMB:
		Blender.sys.sleep(10)
		mouse_buttons = Window.GetMouseButtons()


if __name__=='__main__':
	mouseup()
	x,y= Window.GetMouseCoords()
	isect, point, dir= mouseViewRay(x,y)
	if isect:
		scn= Blender.Scene.GetCurrent()
		me = Blender.Mesh.New()
		ob= Blender.Object.New('Mesh')
		ob.link(me)
		scn.link(ob)
		ob.sel= 1
		me.verts.extend([point, dir])
		me.verts[0].sel= 1
		
	print isect, point, dir
	
	

def spaceRect():
	'''
	Returns the space rect
	xmin,ymin,width,height
	'''
	
	__UI_RECT__ = Blender.BGL.Buffer(Blender.BGL.GL_FLOAT, 4)
	Blender.BGL.glGetFloatv(Blender.BGL.GL_SCISSOR_BOX, __UI_RECT__) 
	__UI_RECT__ = __UI_RECT__.list
	__UI_RECT__ = int(__UI_RECT__[0]), int(__UI_RECT__[1]), int(__UI_RECT__[2])-1, int(__UI_RECT__[3]) 
	
	return __UI_RECT__

def mouseRelativeLoc2d(__UI_RECT__= None):
	if not __UI_RECT__:
		__UI_RECT__ = spaceRect()
	
	mco = Window.GetMouseCoords()
	if	mco[0] > __UI_RECT__[0] and\
	mco[1] > __UI_RECT__[1] and\
	mco[0] < __UI_RECT__[0] + __UI_RECT__[2] and\
	mco[1] < __UI_RECT__[1] + __UI_RECT__[3]:
	
		return (mco[0] - __UI_RECT__[0], mco[1] - __UI_RECT__[1])
		
	else:
		return None
	



	



	