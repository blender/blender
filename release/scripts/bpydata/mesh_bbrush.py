# SPACEHANDLER.VIEW3D.EVENT
# Dont run, event handelers are accessed in the from the 3d View menu.

import Blender
from Blender import Mathutils, Window, Scene, Draw, Mesh, NMesh
from Blender.Mathutils import CrossVecs, Matrix, Vector, Intersect, LineIntersect


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

def getPickRay(screen_x, screen_y, localMatrix=None, useMid = False):
	
	# Constant function variables
	p = getPickRay.p
	d = getPickRay.d
	
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
				hms = getPickRay.hms
				ortho_d = getPickRay.ortho_d
				
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
				fp = getPickRay.fp
				
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
				p = p*localInvMatrix
				d = d*localInvMatrix # normalize_v3
				p.x += localInvMatrix[3][0]
				p.y += localInvMatrix[3][1]
				p.z += localInvMatrix[3][2]
				
			#else: # Worldspace, do nothing
			
			d.normalize()
			return True, p, d # Origin, Direction	
	
	# Mouse is not in any view, return None.
	return False, None, None

# Constant function variables
getPickRay.d = Vector(0,0,0) # Perspective, 3d
getPickRay.p = Vector(0,0,0)
getPickRay.fp = Vector(0,0,0)

getPickRay.hms = Vector(0,0,0,0) # ortho only 4d
getPickRay.ortho_d = Vector(0,0,0,0) # ortho only 4d



def ui_set_preferences(user_interface=1):
	# Create data and set defaults.
	ADAPTIVE_GEOMETRY_but = Draw.Create(0)
	BRUSH_MODE_but = Draw.Create(1)
	BRUSH_PRESSURE_but = Draw.Create(0.05)
	BRUSH_RADIUS_but = Draw.Create(0.25)
	RESOLUTION_MIN_but = Draw.Create(0.1)
	DISPLACE_NORMAL_MODE_but = Draw.Create(2)
	STATIC_NORMAL_but = Draw.Create(1)
	XPLANE_CLIP_but = Draw.Create(0)
	STATIC_MESH_but = Draw.Create(1)
	FIX_TOPOLOGY_but = Draw.Create(0)
	
	# Remember old variables if alredy set.
	try:
		ADAPTIVE_GEOMETRY_but.val = Blender.bbrush['ADAPTIVE_GEOMETRY']
		BRUSH_MODE_but.val = Blender.bbrush['BRUSH_MODE']
		BRUSH_PRESSURE_but.val = Blender.bbrush['BRUSH_PRESSURE']
		BRUSH_RADIUS_but.val = Blender.bbrush['BRUSH_RADIUS']
		RESOLUTION_MIN_but.val = Blender.bbrush['RESOLUTION_MIN']
		DISPLACE_NORMAL_MODE_but.val = Blender.bbrush['DISPLACE_NORMAL_MODE']
		STATIC_NORMAL_but.val = Blender.bbrush['STATIC_NORMAL']
		XPLANE_CLIP_but.val = Blender.bbrush['XPLANE_CLIP']
		STATIC_MESH_but.val = Blender.bbrush['STATIC_MESH']
		FIX_TOPOLOGY_but.val = Blender.bbrush['FIX_TOPOLOGY']
	except:
		Blender.bbrush = {}
	
	if user_interface:
		pup_block = [\
		'Brush Options',\
		('Adaptive Geometry', ADAPTIVE_GEOMETRY_but, 'Add and remove detail as needed. Uses min/max resolution.'),\
		('Brush Type: ', BRUSH_MODE_but, 1, 5, 'Push/Pull:1, Grow/Shrink:2, Spin:3, Relax:4, Goo:5'),\
		('Pressure: ', BRUSH_PRESSURE_but, 0.0, 1.0, 'Pressure of the brush.'),\
		('Size: ', BRUSH_RADIUS_but, 0.01, 2.0, 'Size of the brush.'),\
		('Geometry Res: ', RESOLUTION_MIN_but, 0.01, 0.5, 'Size of the brush & Adaptive Subdivision.'),\
		('Displace Vector: ', DISPLACE_NORMAL_MODE_but, 1, 4, 'Vertex Normal:1, Median Normal:2, Face Normal:3, View Normal:4'),\
		('Static Normal', STATIC_NORMAL_but, 'Use the initial normal only.'),\
		('No X Crossing', XPLANE_CLIP_but, 'Dont allow verts to have a negative X axis (use for x-mirror).'),\
		('Static Mesh', STATIC_MESH_but, 'During mouse interaction, dont update the mesh.'),\
		#('Fix Topology', FIX_TOPOLOGY_but, 'Fix the mesh structure by rotating edges '),\
		]
		
		Draw.PupBlock('BlenBrush Prefs (RMB)', pup_block)
	
	Blender.bbrush['ADAPTIVE_GEOMETRY'] = ADAPTIVE_GEOMETRY_but.val
	Blender.bbrush['BRUSH_MODE'] = BRUSH_MODE_but.val
	Blender.bbrush['BRUSH_PRESSURE'] = BRUSH_PRESSURE_but.val
	Blender.bbrush['BRUSH_RADIUS'] = BRUSH_RADIUS_but.val
	Blender.bbrush['RESOLUTION_MIN'] = RESOLUTION_MIN_but.val
	Blender.bbrush['DISPLACE_NORMAL_MODE'] = DISPLACE_NORMAL_MODE_but.val
	Blender.bbrush['STATIC_NORMAL'] = STATIC_NORMAL_but.val
	Blender.bbrush['XPLANE_CLIP'] = XPLANE_CLIP_but.val
	Blender.bbrush['STATIC_MESH'] = STATIC_MESH_but.val
	Blender.bbrush['FIX_TOPOLOGY'] = FIX_TOPOLOGY_but.val


def triangulateNMesh(nm):
	'''
	Converts the meshes faces to tris, modifies the mesh in place.
	'''
	
	#============================================================================#
	# Returns a new face that has the same properties as the origional face      #
	# but with no verts							  #
	#============================================================================#
	def copyFace(face):
		newFace = NMesh.Face()
		# Copy some generic properties
		newFace.mode = face.mode
		if face.image != None:
			newFace.image = face.image
		newFace.flag = face.flag
		newFace.mat = face.mat
		newFace.smooth = face.smooth
		return newFace
	
	# 2 List comprehensions are a lot faster then 1 for loop.
	tris = [f for f in nm.faces if len(f) == 3]
	quads = [f for f in nm.faces if len(f) == 4]
	
	
	if quads: # Mesh may have no quads.
		has_uv = quads[0].uv 
		has_vcol = quads[0].col
		for quadFace in quads:
			# Triangulate along the shortest edge
			#if (quadFace.v[0].co - quadFace.v[2].co).length < (quadFace.v[1].co - quadFace.v[3].co).length:
			a1 = Mathutils.TriangleArea(quadFace.v[0].co, quadFace.v[1].co, quadFace.v[2].co)
			a2 = Mathutils.TriangleArea(quadFace.v[0].co, quadFace.v[2].co, quadFace.v[3].co)
			b1 = Mathutils.TriangleArea(quadFace.v[1].co, quadFace.v[2].co, quadFace.v[3].co)
			b2 = Mathutils.TriangleArea(quadFace.v[1].co, quadFace.v[3].co, quadFace.v[0].co)
			a1,a2 = min(a1, a2), max(a1, a2)
			b1,b2 = min(b1, b2), max(b1, b2)
			if a1/a2 < b1/b2:
				
				# Method 1
				triA = 0,1,2
				triB = 0,2,3
			else:
				# Method 2
				triA = 0,1,3
				triB = 1,2,3
				
			for tri1, tri2, tri3 in (triA, triB):
				newFace = copyFace(quadFace)
				newFace.v = [quadFace.v[tri1], quadFace.v[tri2], quadFace.v[tri3]]
				if has_uv: newFace.uv = [quadFace.uv[tri1], quadFace.uv[tri2], quadFace.uv[tri3]]
				if has_vcol: newFace.col = [quadFace.col[tri1], quadFace.col[tri2], quadFace.col[tri3]]
				
				nm.addEdge(quadFace.v[tri1], quadFace.v[tri3]) # Add an edge where the 2 tris are devided.
				tris.append(newFace)
		
		nm.faces = tris

import mesh_tri2quad
def fix_topolagy(mesh):
	ob = Scene.GetCurrent().getActiveObject()
	
	for f in mesh.faces:
		f.sel = 1
	mesh.quadToTriangle(0) 
	nmesh = ob.getData()

	mesh_tri2quad.tri2quad(nmesh, 100, 0)
	triangulateNMesh(nmesh)
	nmesh.update()
	
	mesh = Mesh.Get(mesh.name)
	for f in mesh.faces:
		f.sel=1	
	mesh.quadToTriangle()
	Mesh.Mode(Mesh.SelectModes['EDGE'])
	
	
	
	


def event_main():
	#print Blender.event
	#mod =[Window.Qual.CTRL,  Window.Qual.ALT, Window.Qual.SHIFT]
	mod =[Window.Qual.CTRL,  Window.Qual.ALT]
	
	qual = Window.GetKeyQualifiers()
	SHIFT_FLAG = Window.Qual.SHIFT
	CTRL_FLAG = Window.Qual.CTRL
	
	
	# UNDO
	"""
	is_editmode = Window.EditMode() # Exit Editmode.
	if is_editmode: Window.EditMode(0)
	if Blender.event == Draw.UKEY:
		if is_editmode:
			Blender.event = Draw.UKEY
			return
		else:
			winId = [win3d for win3d in Window.GetScreenInfo(Window.Types.VIEW3D)][0]
			Blender.event = None
			Window.QHandle(winId['id'])
			Window.EditMode(1)
			Window.QHandle(winId['id'])
			Window.QAdd(winId['id'],Draw.UKEY,1) # Change KeyPress Here for EditMode
			Window.QAdd(winId['id'],Draw.UKEY,0)
			Window.QHandle(winId['id'])
			Window.EditMode(0)
			Blender.event = None
			return
	"""
	
	ob = Scene.GetCurrent().getActiveObject()
	if not ob or ob.getType() != 'Mesh':
		return
	
	# Mouse button down with no modifiers.	
	if Blender.event == Draw.LEFTMOUSE and not [True for m in mod if m & qual]:
		# Do not exit (draw)
		pass
	elif Blender.event == Draw.RIGHTMOUSE and not [True for m in mod if m & qual]:
		ui_set_preferences()
		return
	else:
		return 
		
	del qual
	
	
	try:
		Blender.bbrush
	except:
		# First time run
		ui_set_preferences() # No ui
		return

	ADAPTIVE_GEOMETRY = Blender.bbrush['ADAPTIVE_GEOMETRY'] # 1
	BRUSH_MODE = Blender.bbrush['BRUSH_MODE'] # 1
	BRUSH_PRESSURE_ORIG = Blender.bbrush['BRUSH_PRESSURE'] # 0.1
	BRUSH_RADIUS = Blender.bbrush['BRUSH_RADIUS'] # 0.5
	RESOLUTION_MIN = Blender.bbrush['RESOLUTION_MIN'] # 0.08
	STATIC_NORMAL = Blender.bbrush['STATIC_NORMAL'] # 0
	XPLANE_CLIP = Blender.bbrush['XPLANE_CLIP'] # 0
	DISPLACE_NORMAL_MODE = Blender.bbrush['DISPLACE_NORMAL_MODE'] # 'Vertex Normal%x1|Median Normal%x2|Face Normal%x3|View Normal%x4'
	STATIC_MESH = Blender.bbrush['STATIC_MESH']
	FIX_TOPOLOGY = Blender.bbrush['FIX_TOPOLOGY']
	
	
	# Angle between Vecs wrapper.
	AngleBetweenVecs = Mathutils.AngleBetweenVecs
	def ang(v1,v2):
		try:
			return AngleBetweenVecs(v1,v2)
		except:
			return 180
	"""
	def Angle2D(x1, y1, x2, y2):
		import math
		RAD2DEG = 57.295779513082323
		'''
		   Return the angle between two vectors on a plane
		   The angle is from vector 1 to vector 2, positive anticlockwise
		   The result is between -pi -> pi
		'''
		dtheta = math.atan2(y2,x2) - math.atan2(y1,x1) # theta1 - theta2
		while dtheta > math.pi:
			dtheta -= (math.pi*2)
		while dtheta < -math.pi:
			dtheta += (math.pi*2)
		return dtheta * RAD2DEG  #(180.0 / math.pi)
	"""
	
	def faceIntersect(f):
		isect = Intersect(f.v[0].co, f.v[1].co, f.v[2].co, Direction, Origin, 1) # Clipped.
		if isect:
			return isect
		elif len(f.v) == 4:
			isect = Intersect(f.v[0].co, f.v[2].co, f.v[3].co, Direction, Origin, 1) # Clipped.
		return isect
	"""
	# Unused so farm, too slow.
	def removeDouble(v1,v2, me):
		v1List = [f for f in me.faces if v1 in f.v]
		v2List = [f for f in me.faces if v2 in f.v]
		#print v1List
		#print v2List
		remFaces = []
		newFaces = []
		for f2 in v2List:
			f2ls = list(f2.v)
			i = f2ls.index(v2)
			f2ls[i] = v1
			#remFaces.append(f2)
			if f2ls.count(v1) == 1:
				newFaces.append(tuple(f2ls))
		if remFaces:
			me.faces.delete(1, remFaces)
		#me.verts.delete(v2)
		if newFaces:
			me.faces.extend(newFaces)
	"""
	
	
	me = ob.getData(mesh=1)
	
	is_editmode = Window.EditMode() # Exit Editmode.
	if is_editmode: Window.EditMode(0)
	
	Mesh.Mode(Mesh.SelectModes['EDGE'])
	
	# At the moment ADAPTIVE_GEOMETRY is the only thing that uses selection.
	if ADAPTIVE_GEOMETRY:
		# Deslect all
		SEL_FLAG = Mesh.EdgeFlags['SELECT']
		'''
		for ed in me.edges:
			#ed.flag &= ~SEL_FLAG # deselect. 34
			ed.flag = 32
		'''
		#filter(lambda ed: setattr(ed, 'flag', 32), me.edges)
		
		'''for v in me.verts:
			v.sel = 0'''
		#filter(lambda v: setattr(v, 'sel', 0), me.verts)
		# DESELECT ABSOLUTLY ALL
		Mesh.Mode(Mesh.SelectModes['FACE'])
		filter(lambda f: setattr(f, 'sel', 0), me.faces)
		
		Mesh.Mode(Mesh.SelectModes['EDGE'])
		filter(lambda ed: setattr(ed, 'flag', 32), me.edges)
		
		Mesh.Mode(Mesh.SelectModes['VERTEX'])
		filter(lambda v: setattr(v, 'sel', 0), me.verts)		
		
		Mesh.Mode(Mesh.SelectModes['EDGE'])
		
	i = 0
	time = Blender.sys.time()
	last_best_isect = None # used for goo only
	old_screen_x, old_screen_y = 1<<30, 1<<30
	goo_dir_vec = last_goo_dir_vec = gooRotMatrix = None # goo mode only.
	
	# Normal stuff
	iFaceNormal = medainNormal = None
	
	# Store all vert normals for now.
	if BRUSH_MODE == 1 and STATIC_NORMAL: # Push pull
		vert_orig_normals = dict([(v, v.no) for v in me.verts])
	
	elif BRUSH_MODE == 4: # RELAX, BUILD EDGE CONNECTIVITE DATA.
		# we need edge connectivity
		#vertEdgeUsers = [list() for i in xrange(len(me.verts))]
		verts_connected_by_edge = [list() for i in xrange(len(me.verts))]
		
		for ed in me.edges:
			i1, i2 = ed.v1.index,  ed.v2.index
			#vertEdgeUsers[i1].append(ed)
			#vertEdgeUsers[i2].append(ed)
			
			verts_connected_by_edge[i1].append(ed.v2)
			verts_connected_by_edge[i2].append(ed.v1)
	
	if STATIC_MESH:
		
		# Try and find a static mesh to reuse.
		# this is because we dont want to make a new mesh for each stroke.
		mesh_static = None
		for _me_name_ in Blender.NMesh.GetNames():
			_me_ = Mesh.Get(_me_name_)
			#print _me_.users , len(me.verts)
			if _me_.users == 0 and len(_me_.verts) == 0:
				mesh_static = _me_
				#print 'using', _me_.name
				break
		del _me_name_
		del _me_
		
		if not mesh_static:
			mesh_static = Mesh.New()
			print 'Making new mesh', mesh_static.name
		
		mesh_static.verts.extend([v.co for v in me.verts])
		mesh_static.faces.extend([tuple([mesh_static.verts[v.index] for v in f.v]) for f in me.faces])
	
	
	best_isect = gooPlane = None 
	
	while Window.GetMouseButtons() == 1:
		i+=1
		screen_x, screen_y = Window.GetMouseCoords()
		
		# Skip when no mouse movement, Only for Goo!
		if screen_x == old_screen_x and screen_y == old_screen_y:
			if BRUSH_MODE == 5: # Dont modify while mouse is not moved for goo.
				continue
		else: # mouse has moved get the new mouse ray.
			old_screen_x, old_screen_y = screen_x, screen_y
			mouseInView, Origin, Direction = getPickRay(screen_x, screen_y, ob.matrixWorld)
			if not mouseInView or not Origin:
				return
			Origin_SCALE = Origin * 100 
			
		# Find an intersecting face!
		bestLen = 1<<30 # start with an assumed realy bad match.
		best_isect = None # last intersect is used for goo.
		best_face = None
		
		if not last_best_isect:
			last_best_isect = best_isect
		
		if not mouseInView:
			last_best_isect = None	
			
		else:
			# Find Face intersection closest to the view. 
			#for f in [f for f in me.faces if ang(f.no, Direction) < 90]:
			
			# Goo brush only intersects faces once, after that the brush follows teh view plain.
			if BRUSH_MODE == 5 and gooPlane != None and gooPlane:
				best_isect = Intersect( gooPlane[0], gooPlane[1], gooPlane[2], Direction, Origin, 0) # Non clipped
			else:
				if STATIC_MESH:
					intersectingFaces = [(f, ix) for f in mesh_static.faces for ix in (faceIntersect(f),) if ix]
				else:
					intersectingFaces = [(f, ix) for f in me.faces for ix in (faceIntersect(f),) if ix]
				
				for f, isect in intersectingFaces:
					l = (Origin_SCALE-isect).length 
					if l < bestLen:
						best_face = f
						best_isect = isect
						bestLen = l
		
		if not best_isect:
			# Dont interpolate once the mouse moves off the mesh.
			lastGooVec = last_best_isect = None
			
		else: # mouseInView must be true also
			
			# Use the shift key to modify the pressure.
			if SHIFT_FLAG & Window.GetKeyQualifiers():
				BRUSH_PRESSURE = -BRUSH_PRESSURE_ORIG
			else:
				BRUSH_PRESSURE =  BRUSH_PRESSURE_ORIG
			
			brush_verts = [(v,le) for v in me.verts for le in ((v.co-best_isect).length,) if le <= BRUSH_RADIUS]
			
			# SETUP ONCE ONLY VARIABLES
			if STATIC_NORMAL: # Only set the normal once.
				if not iFaceNormal:
					iFaceNormal = best_face.no
			else:
				if best_face:
					iFaceNormal = best_face.no
			
			
			if DISPLACE_NORMAL_MODE == 2: # MEDIAN NORMAL
				if (STATIC_NORMAL and medainNormal == None) or not STATIC_NORMAL or str(medainNormal.x) == 'nan':
					medainNormal = Vector(0,0,0)
					if brush_verts:
						for v, l in brush_verts:
							medainNormal += v.no*(BRUSH_RADIUS-l)
						medainNormal.normalize()
					
			
			
			# ================================================================#
			# == Tool code, loop on the verts and operate on them ============#
			# ================================================================#
			if BRUSH_MODE == 1: # NORMAL PAINT
				for v,l in brush_verts:
					if XPLANE_CLIP:
						origx = False
						if abs(v.co.x) < 0.001: origx = True
							
					
					v.sel = 1 # MARK THE VERT AS DIRTY.
					falloff = BRUSH_PRESSURE * ((BRUSH_RADIUS-l) / BRUSH_RADIUS) # falloff between 0 and 1
					if DISPLACE_NORMAL_MODE == 1: # VERTEX NORMAL
						if STATIC_NORMAL:
							try:
								no = vert_orig_normals[v]
							except:
								no = vert_orig_normals[v] = v.no
							v.co += no * falloff
						else:
							v.co += no * falloff
					elif DISPLACE_NORMAL_MODE == 2: # MEDIAN NORMAL # FIXME
						v.co += medainNormal * falloff
						
					elif DISPLACE_NORMAL_MODE == 3: # FACE NORMAL
						v.co += iFaceNormal * falloff
					elif DISPLACE_NORMAL_MODE == 4: # VIEW NORMAL
						v.co += Direction * falloff
					# Clamp back to original x if needs be.
					if XPLANE_CLIP and origx:
						v.co.x = 0
			
			elif BRUSH_MODE == 2: # SCALE
				for v,l in brush_verts:
					
					if XPLANE_CLIP:
						origx = False
						if abs(v.co.x) < 0.001: origx = True
					
					v.sel = 1 # MARK THE VERT AS DIRTY.
					falloff = (BRUSH_RADIUS-l) / BRUSH_RADIUS # falloff between 0 and 1
					
					vert_scale_vec = v.co - best_isect
					vert_scale_vec.normalize()
					# falloff needs to be scaled for this tool
					falloff = falloff / 10
					v.co += vert_scale_vec * (BRUSH_PRESSURE * falloff)# FLAT BRUSH
					
					# Clamp back to original x if needs be.
					if XPLANE_CLIP and origx:
						v.co.x = 0
			
			if BRUSH_MODE == 3: # ROTATE.
				
				if DISPLACE_NORMAL_MODE == 1: # VERTEX NORMAL
					ROTATE_MATRIX = Mathutils.RotationMatrix(BRUSH_PRESSURE*10, 4, 'r', iFaceNormal)  # Cant use vertex normal, use face normal
				elif DISPLACE_NORMAL_MODE == 2: # MEDIAN NORMAL
					ROTATE_MATRIX = Mathutils.RotationMatrix(BRUSH_PRESSURE*10, 4, 'r', medainNormal)  # Cant use vertex normal, use face normal
				elif DISPLACE_NORMAL_MODE == 3: # FACE NORMAL
					ROTATE_MATRIX = Mathutils.RotationMatrix(BRUSH_PRESSURE*10, 4, 'r', iFaceNormal)  # Cant use vertex normal, use face normal
				elif DISPLACE_NORMAL_MODE == 4: # VIEW NORMAL
					ROTATE_MATRIX = Mathutils.RotationMatrix(BRUSH_PRESSURE*10, 4, 'r', Direction)  # Cant use vertex normal, use face normal
				# Brush code
				
				for v,l in brush_verts:
					
					if XPLANE_CLIP:
						origx = False
						if abs(v.co.x) < 0.001: origx = True
					
					# MARK THE VERT AS DIRTY.
					v.sel = 1
					falloff = (BRUSH_RADIUS-l) / BRUSH_RADIUS # falloff between 0 and 1
				
					# Vectors handeled with rotation matrix creation.
					rot_vert_loc = (ROTATE_MATRIX * (v.co-best_isect)) + best_isect
					v.co = (v.co*(1-falloff)) + (rot_vert_loc*(falloff))
					
					# Clamp back to original x if needs be.
					if XPLANE_CLIP and origx:
						v.co.x = 0
				
			elif BRUSH_MODE == 4: # RELAX
				vert_orig_loc = [Vector(v.co) for v in me.verts ] # save orig vert location.
				#vertOrigNor = [Vector(v.no) for v in me.verts ] # save orig vert location.
				
				# Brush code
				for v,l in brush_verts:
					
					if XPLANE_CLIP:
						origx = False
						if abs(v.co.x) < 0.001: origx = True
					
					v.sel = 1 # Mark the vert as dirty.
					falloff = (BRUSH_RADIUS-l) / BRUSH_RADIUS # falloff between 0 and 1
					connected_verts = verts_connected_by_edge[v.index]
					relax_point = reduce(lambda a,b: a + vert_orig_loc[b.index], connected_verts, Mathutils.Vector(0,0,0)) * (1.0/len(connected_verts))
					falloff = falloff * BRUSH_PRESSURE
					# Old relax.
					#v.co = (v.co*(1-falloff)) + (relax_point*(falloff))
					
					ll = (v.co-relax_point).length
					newpoint = (v.co*(1-falloff)) + (relax_point*(falloff)) - v.co
					newpoint = newpoint * (1/(1+ll))
					v.co = v.co + newpoint
					
					'''
					# New relax
					relax_normal = vertOrigNor[v.index]
					v1,v2,v3,v4 = v.co, v.co+relax_normal, relax_point-(relax_normal*10), relax_point+(relax_normal*10)
					print v1,v2,v3,v4
					try:
						a,b = LineIntersect(v1,v2,v3,v4) # Scale the normal to make a line. we know we will intersect with.
						v.co = (v.co*(1-falloff)) + (a*(falloff))
					except:
						pass
					'''
					
					# Clamp back to original x if needs be.
					if XPLANE_CLIP and origx:
						v.co.x = 0
					
			elif BRUSH_MODE == 5: # GOO
				#print last_best_isect, best_isect, 'AA'
				if not last_best_isect:
					last_best_isect = best_isect
					
					# Set up a triangle orthographic to the view plane
					gooPlane = [best_isect, CrossVecs(best_isect, Direction), None]
					
					
					if DISPLACE_NORMAL_MODE == 4: # View Normal
						tempRotMatrix = Mathutils.RotationMatrix(90, 3, 'r', Direction)
					else:
						tempRotMatrix = Mathutils.RotationMatrix(90, 3, 'r', CrossVecs(best_face.no, Direction))
					
					gooPlane[2] =  best_isect + (tempRotMatrix * gooPlane[1])
					gooPlane[1] = gooPlane[1] + best_isect
					
					continue # we need another point of reference.
					
				elif last_best_isect == best_isect:
					# Mouse has not moved, no point in trying to goo.
					continue
				else:
					if goo_dir_vec:
						last_goo_dir_vec = goo_dir_vec
					# The direction the mouse moved in 3d space. use for gooing
					
					# Modify best_isect so its not moving allong the view z axis.
					# Assume Origin hasnt changed since the view wont change while the mouse is drawing. ATM.
					best_isect = Intersect( gooPlane[0], gooPlane[1], gooPlane[2], Direction, Origin, 0) # Non clipped
					goo_dir_vec = (best_isect - last_best_isect) * 2
					
					
					# make a goo rotation matrix so the head of the goo rotates with the mouse.
					"""
					if last_goo_dir_vec and goo_dir_vec != last_goo_dir_vec:
						'''
						vmi  = Matrix(Window.GetViewMatrix()); vmi.invert() # the inverse viewing matrix
						a = last_goo_dir_vec * vmi
						b = goo_dir_vec * vmi
						c = Angle2D(a.x, a.y, b.x, b.y)
						gooRotMatrix = Mathutils.RotationMatrix((c * goo_dir_vec.length)*-20, 3, 'r', Direction)
						'''
						pass
					else:
						gooRotMatrix = None
					"""
					
					if goo_dir_vec.x == 0 and goo_dir_vec.y == 0 and goo_dir_vec.z == 0:
						continue
					
					# Brush code
					for v,l in brush_verts:
						
						if XPLANE_CLIP:
							origx = False
							if abs(v.co.x) < 0.001: origx = True
						
						# MARK THE VERT AS DIRTY.
						v.sel = 1
						
						''' # ICICLES!!!
						a = AngleBetweenVecs(goo_dir_vec, v.no)
						if a > 66:
							continue
							
						l = l * ((1+a)/67.0)
						l = max(0.00000001, l)
						'''
						
						falloff = (BRUSH_RADIUS-l) / BRUSH_RADIUS # falloff between 0 and 1
						goo_loc = (v.co*(1-falloff)) + ((v.co+goo_dir_vec) *falloff)
						
						v.co = (goo_loc*BRUSH_PRESSURE) + (v.co*(1-BRUSH_PRESSURE))
						
						'''
						if gooRotMatrix:
							rotatedVertLocation = (gooRotMatrix * (v.co-best_isect)) + best_isect
							v.co = (v.co*(1-falloff)) + (rotatedVertLocation*(falloff))
							# USe for goo only.					
						'''
						
						# Clamp back to original x if needs be.
						if XPLANE_CLIP and origx:
							v.co.x = 0
			
				
			# Remember for the next sample
			last_best_isect = best_isect
			last_goo_dir_vec = goo_dir_vec
			
			# Post processing after the verts have moved
			# Subdivide any large edges, all but relax.
			
			MAX_SUBDIV = 10 # Maximum number of subdivisions per redraw. makes things useable.
			SUBDIV_COUNT = 0
			# Cant use adaptive geometry for relax because it keeps connectivity data.
			if ADAPTIVE_GEOMETRY and (BRUSH_MODE == 1 or BRUSH_MODE == 2 or BRUSH_MODE == 3 or BRUSH_MODE == 5):
				Mesh.Mode(Mesh.SelectModes['EDGE'])
				orig_len_edges = 0
				#print 'ADAPTIVE_GEOMETRY'
				while len(me.edges) != orig_len_edges and SUBDIV_COUNT < MAX_SUBDIV:
					#print 'orig_len_edges', len(me.edges) 
					#me = ob.getData(mesh=1)
					orig_len_edges = len(me.edges)
					EDGE_COUNT = 0
					for ed in me.edges:
						if ed.v1.sel or ed.v2.sel:
							l = (ed.v1.co - ed.v2.co).length
							if l > max(RESOLUTION_MIN*1.5, BRUSH_RADIUS):
							#if l > BRUSH_RADIUS:
								#print 'adding edge'
								ed.flag |= SEL_FLAG
								#ed.flag = 35
								SUBDIV_COUNT += 1
								EDGE_COUNT +=1
							"""
							elif l < RESOLUTION_MIN:
								'''
								print 'removing edge'
								v1 =e.v1
								v2 =e.v2
								v1.co = v2.co = (v1.co + v2.co) * 0.5
								v1.sel = v2.sel = 1
								me.remDoubles(0.001)
								me = ob.getData(mesh=1)
								break
								'''
								# Remove edge in python
								print 'removing edge'
								v1 =ed.v1
								v2 =ed.v2
								v1.co = v2.co = (v1.co + v2.co) * 0.5
								
								removeDouble(v1, v2, me)
								me = ob.getData(mesh=1)
								break
							"""		
							
					if EDGE_COUNT:
						me.subdivide(1)
						me = ob.getData(mesh=1)
						filter(lambda ed: setattr(ed, 'flag', 32), me.edges)
							
					# Deselect all, we know theres only 2 selected
					'''
					for ee in me.edges:
						if ee.flag & SEL_FLAG:
							#ee.flag &= ~SEL_FLAG
							ee.flag = 32
						elif l < RESOLUTION_MIN:
							print 'removing edge'
							e.v1.co = e.v2.co = (e.v1.co + e.v2.co) * 0.5
							me.remDoubles(0.001)
							break
						'''
				# Done subdividing
				# Now remove doubles
				#print Mesh.SelectModes['VERT']
				#Mesh.Mode(Mesh.SelectModes['VERTEX'])
				
				filter(lambda v: setattr(v, 'sel', 1), me.verts)
				filter(lambda v: setattr(v[0], 'sel', 0), brush_verts)
				
				# Cycling editmode is too slow.
				
				remdoubles = False
				for ed in me.edges:
					
					if (not ed.v1.sel) and (not ed.v1.sel):
						if XPLANE_CLIP:
							# If 1 vert is on the edge and abother is off dont collapse edge.
							if (abs(ed.v1.co.x) < 0.001) !=\
							(abs(ed.v2.co.x) < 0.001):
								continue
						l = (ed.v1.co - ed.v2.co).length
						if l < RESOLUTION_MIN:
							ed.v1.sel = ed.v2.sel = 1
							newco = (ed.v1.co + ed.v2.co)*0.5
							#ed.v1.co.x = ed.v2.co.x = newco.x
							#ed.v1.co.y = ed.v2.co.y = newco.y
							#ed.v1.co.z = ed.v2.co.z = newco.z
							ed.v1.co[:]= ed.v2.co[:]= newco
							remdoubles = True
				
				#if remdoubles:

				
				filter(lambda v: setattr(v, 'sel', 0), me.verts)
				#Mesh.Mode(Mesh.SelectModes['EDGE'])
				# WHILE OVER
				# Clean up selection.
				#for v in me.verts:
				#	v.sel = 0
				'''
				for ee in me.edges:
					if ee.flag & SEL_FLAG:
						ee.flag &= ~SEL_FLAG
						#ee.flag = 32
				'''
				filter(lambda ed: setattr(ed, 'flag', 32), me.edges)
				
				
			if XPLANE_CLIP:
				filter(lambda v: setattr(v.co, 'x', max(0, v.co.x)), me.verts)
			
			
			me.update()
			#Window.SetCursorPos(best_isect.x, best_isect.y, best_isect.z)
			Window.Redraw(Window.Types.VIEW3D)
	#Window.DrawProgressBar(1.0, '')
	if STATIC_MESH:
		#try:
		mesh_static.verts =  None
		print len(mesh_static.verts)
		mesh_static.update()
		#except:
		#	pass
	if FIX_TOPOLOGY:
		fix_topolagy(me)
	
	# Remove short edges of we have edaptive geometry enabled.
	if ADAPTIVE_GEOMETRY:
		Mesh.Mode(Mesh.SelectModes['VERTEX'])
		filter(lambda v: setattr(v, 'sel', 1), me.verts)
		me.remDoubles(0.001)
		print 'removing doubles'
		me = ob.getData(mesh=1) # Get new vert data
		Blender.event = Draw.LEFTMOUSE

	Mesh.Mode(Mesh.SelectModes['EDGE'])
	
	if i:
		Window.EditMode(1)
		if not is_editmode: # User was in edit mode, so stay there.
			Window.EditMode(0)
		print '100 draws in %.6f' % (((Blender.sys.time()-time) / float(i))*100)
	Blender.event = None

if __name__ == '__main__':
	event_main()