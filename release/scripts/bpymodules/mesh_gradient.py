# This is not to be used directly, vertexGradientPick can be used externaly

import Blender
import BPyMesh
import BPyWindow

mouseViewRay= BPyWindow.mouseViewRay
from Blender import Mathutils, Window, Scene, Draw, sys
from Blender.Mathutils import Vector, Intersect, LineIntersect, AngleBetweenVecs
LMB= Window.MButs['L']

def mouseup():
	# Loop until click
	mouse_buttons = Window.GetMouseButtons()
	while not mouse_buttons & LMB:
		sys.sleep(10)
		mouse_buttons = Window.GetMouseButtons()
	while mouse_buttons & LMB:
		sys.sleep(10)
		mouse_buttons = Window.GetMouseButtons()

def mousedown_wait():
	# If the menu has just been pressed dont use its mousedown,
	mouse_buttons = Window.GetMouseButtons()
	while mouse_buttons & LMB:
		mouse_buttons = Window.GetMouseButtons()

eps= 0.0001
def vertexGradientPick(ob, MODE):
	#MODE 0 == VWEIGHT,  1 == VCOL 
	
	me= ob.getData(mesh=1)
	if not me.faceUV:	me.faceUV= True
	
	Window.DrawProgressBar (0.0, '')
	
	mousedown_wait()
	
	if MODE==0:
		act_group= me.activeGroup
		if act_group == None:
			mousedown_wait()
			Draw.PupMenu('Error, mesh has no active group.')
			return
	
	# Loop until click
	Window.DrawProgressBar (0.25, 'Click to set gradient start')
	mouseup()
	
	obmat= ob.matrixWorld
	screen_x, screen_y = Window.GetMouseCoords()
	mouseInView, OriginA, DirectionA = mouseViewRay(screen_x, screen_y, obmat)
	if not mouseInView or not OriginA:
		return
	
	# get the mouse weight
	
	if MODE==0:
		pickValA= BPyMesh.pickMeshGroupWeight(me, act_group, OriginA, DirectionA)
	if MODE==1:
		pickValA= BPyMesh.pickMeshGroupVCol(me, OriginA, DirectionA)
	
	Window.DrawProgressBar (0.75, 'Click to set gradient end')
	mouseup()
	
	TOALPHA= Window.GetKeyQualifiers() & Window.Qual.SHIFT
	
	screen_x, screen_y = Window.GetMouseCoords()
	mouseInView, OriginB, DirectionB = mouseViewRay(screen_x, screen_y, obmat)
	if not mouseInView or not OriginB:
		return
	
	if not TOALPHA: # Only get a second opaque value if we are not blending to alpha
		if MODE==0:	pickValB= BPyMesh.pickMeshGroupWeight(me, act_group, OriginB, DirectionB)
		else:
			pickValB= BPyMesh.pickMeshGroupVCol(me, OriginB, DirectionB)
	else:
		if MODE==0: pickValB= 0.0
		else: pickValB= [0.0, 0.0, 0.0] # Dummy value
	
	# Neither points touched a face
	if pickValA == pickValB == None:
		return
	
	# clicking on 1 non face is fine. just set the weight to 0.0
	if pickValA==None:
		pickValA= 0.0
		
		# swap A/B
		OriginA, OriginB= OriginB, OriginA
		DirectionA, DirectionB= DirectionB, DirectionA
		pickValA, pickValB= pickValA, pickValB
		
		TOALPHA= True
		
	if pickValB==None:
		pickValB= 0.0
		TOALPHA= True
	
	# set up 2 lines so we can measure their distances and calc the gradient
	
	# make a line 90d to the grad in screenspace.
	if (OriginA-OriginB).length <= eps: # Persp view. same origin different direction
		cross_grad= DirectionA.cross(DirectionB)
		ORTHO= False
		
	else: # Ortho - Same direction, different origin
		cross_grad= DirectionA.cross(OriginA-OriginB)
		ORTHO= True
	
	cross_grad.normalize()
	cross_grad= cross_grad * 100
	
	lineA= (OriginA, OriginA+(DirectionA*100))
	lineB= (OriginB, OriginB+(DirectionB*100))
	
	if not ORTHO:
		line_angle= AngleBetweenVecs(lineA[1], lineB[1])/2
		line_mid= (lineA[1]+lineB[1])*0.5

	VSEL= [False] * (len(me.verts))
	
	# Get the selected faces and apply the selection to the verts.
	for f in me.faces:
		if f.sel:
			for v in f.v:
				VSEL[v.index]= True
	groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
	
	
	
	def grad_weight_from_co(v):
		'''
		Takes a vert and retuens its gradient radio between A and B
		'''
		
		if not VSEL[v.index]: # Not bart of a selected face?
			return None, None
		
		v_co= v.co
		# make a line 90d to the 2 lines the user clicked.
		vert_line= (v_co - cross_grad, v_co + cross_grad)
		
		xA= LineIntersect(vert_line[0], vert_line[1], lineA[0], lineA[1])
		xB= LineIntersect(vert_line[0], vert_line[1], lineB[0], lineB[1])
		
		if not xA or not xB: # Should never happen but support it anyhow
			return None, None
		
		wA= (xA[0]-xA[1]).length
		wB= (xB[0]-xB[1]).length
		
		wTot= wA+wB
		if not wTot: # lines are on the same point.
			return None, None
		
		'''
		Get the length of the line between both intersections on the 
		2x view lines.
		if the dist between  lineA+VertLine and lineB+VertLine is 
		greater then the lenth between lineA and lineB intersection points, it means
		that the verts are not inbetween the 2 lines.
		'''
		lineAB_length= (xA[1]-xB[1]).length
		
		# normalzie
		wA= wA/wTot
		wB= wB/wTot
		
		if ORTHO: # Con only use line length method with parelelle lines
			if wTot > lineAB_length+eps:
				# vert is outside the range on 1 side. see what side of the grad
				if wA>wB:		wA, wB= 1.0, 0.0
				else:			wA, wB= 0.0, 1.0
		else:
			# PERSP, lineA[0] is the same origin as lineB[0]
			
			# Either xA[0] or xB[0]  can be used instead of a possible x_mid between the 2
			# as long as the point is inbetween lineA and lineB it dosent matter.
			a= AngleBetweenVecs(lineA[0]-xA[0], line_mid)
			if a>line_angle:
				# vert is outside the range on 1 side. see what side of the grad
				if wA>wB:		wA, wB= 1.0, 0.0
				else:			wA, wB= 0.0, 1.0
		
		return wA, wB
		
	
	grad_weights= [grad_weight_from_co(v) for v in me.verts]
	
	
	if MODE==0:
		for v in me.verts:
			i= v.index
			if VSEL[i]:
				wA, wB = grad_weights[i]
				if wA != None: # and wB 
					if TOALPHA:
						# Do alpha by using the exiting weight for 
						try:		pickValB= vWeightDict[i][act_group]
						except:	pickValB= 0.0 # The weights not there? assume zero
					# Mix2 2 opaque weights
					vWeightDict[i][act_group]= pickValB*wA + pickValA*wB
	
	else: # MODE==1 VCol
		for f in me.faces:
			if f.sel:
				f_v= f.v
				for i in xrange(len(f_v)):
					v= f_v[i]
					wA, wB = grad_weights[v.index]
					
					c= f.col[i]
					
					if TOALPHA:
						pickValB= c.r, c.g, c.b
					
					c.r = int(pickValB[0]*wA + pickValA[0]*wB)
					c.g = int(pickValB[1]*wA + pickValA[1]*wB)
					c.b = int(pickValB[2]*wA + pickValA[2]*wB)
					
	
	
	
	# Copy weights back to the mesh.
	BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)
	Window.DrawProgressBar (1.0, '')


