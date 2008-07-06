#!BPY
# coding: utf-8
""" Registration info for Blender menus
Name: 'Bevel Center'
Blender: 243
Group: 'Mesh'
Tip: 'Bevel selected faces, edges, and vertices'
"""

__author__ = "Loic BERTHE"
__url__ = ("blender", "blenderartists.org")
__version__ = "2.0"

__bpydoc__ = """\
This script implements vertex and edges bevelling in Blender.

Usage:

Select the mesh you want to work on, enter Edit Mode and select the edges
to bevel.  Then run this script from the 3d View's Mesh->Scripts menu.

You can control the thickness of the bevel with the slider -- redefine the
end points for bigger or smaller ranges.  The thickness can be changed even
after applying the bevel, as many times as needed.

For an extra smoothing after or instead of direct bevel, set the level of
recursiveness and use the "Recursive" button. 

This "Recursive" Button, won't work in face select mode, unless you choose
"faces" in the select mode menu.

Notes:<br>
	You can undo and redo your steps just like with normal mesh operations in
Blender.
"""

######################################################################
# Bevel Center v2.0 for Blender

# This script lets you bevel the selected vertices or edges and control the
# thickness of the bevel

# (c) 2004-2006 Loïc Berthe (loic+blender@lilotux.net)
# released under Blender Artistic License

######################################################################

import Blender
from Blender import NMesh, Window, Scene
from Blender.Draw import *
from Blender.Mathutils import *
from Blender.BGL import *
import BPyMessages
#PY23 NO SETS#
'''
try:
	set()
except:
	from sets import set	
'''

######################################################################
# Functions to handle the global structures of the script NF, NE and NC
# which contain informations about faces and corners to be created

global E_selected
E_selected = NMesh.EdgeFlags['SELECT']

old_dist = None

def act_mesh_ob():
	scn = Scene.GetCurrent()
	ob = scn.objects.active
	if ob == None or ob.type != 'Mesh': 
		BPyMessages.Error_NoMeshActive()
		return
	
	if ob.getData(mesh=1).multires:
		BPyMessages.Error_NoMeshMultiresEdit()
		return
	
	return ob

def make_sel_vert(*co):
	v= NMesh.Vert(*co)
	v.sel = 1
	me.verts.append(v)
	return v

def make_sel_face(verts):
	f = NMesh.Face(verts)
	f.sel = 1
	me.addFace(f)

def add_to_NV(old,dir,new):
	try:
		NV[old][dir] = new
	except:
		NV[old] = {dir:new}	   

def get_v(old, *neighbors):
	# compute the direction of the new vert
	if len(neighbors) == 1: dir = (neighbors[0].co - old.co).normalize()
		#dir
	else: dir = (neighbors[0].co - old.co).normalize() + (neighbors[1].co-old.co).normalize()
	 
	# look in NV if this vert already exists
	key = tuple(dir)
	if old in NV and key in NV[old] : return NV[old][key]
	
	# else, create it 
	new = old.co + dist.val*dir
	v = make_sel_vert(new.x,new.y,new.z)
	add_to_NV(old,key,v)
	return v

def make_faces():
	""" Analyse the mesh, make the faces corresponding to selected faces and
	fill the structures NE and NC """

	# make the differents flags consistent
	for e in me.edges:
		if e.flag & E_selected :
			e.v1.sel = 1
			e.v2.sel = 1
	
	NF =[]			  # NF : New faces
	for f in me.faces:
		V = f.v
		nV = len(V)
		enumV = range(nV)
		E = [me.findEdge(V[i],V[(i+1) % nV]) for i in enumV]
		Esel = [x.flag & E_selected for x in E]
		
		# look for selected vertices and creates a list containing the new vertices
		newV = V[:] 
		changes = False
		for (i,v) in enumerate(V):
			if v.sel :
				changes = True
				if   Esel[i-1] == 0 and Esel[i] == 1 :  newV[i] = get_v(v,V[i-1])
				elif Esel[i-1] == 1 and Esel[i] == 0 :  newV[i] = get_v(v,V[(i+1) % nV])
				elif Esel[i-1] == 1 and Esel[i] == 1 :  newV[i] = get_v(v,V[i-1],V[(i+1) % nV])
				else :								  newV[i] = [get_v(v,V[i-1]),get_v(v,V[(i+1) % nV])]
		
		if changes:
			# determine and store the face to be created

			lenV = [len(x) for x in newV]
			if 2 not in lenV :			  
				new_f = NMesh.Face(newV)
				if sum(Esel) == nV : new_f.sel = 1
				NF.append(new_f)
				
			else :
				nb2 = lenV.count(2)
				
				if nV == 4 :				# f is a quad
					if nb2 == 1 :
						ind2 = lenV.index(2)
						NF.append(NMesh.Face([newV[ind2-1],newV[ind2][0],newV[ind2][1],newV[ind2-3]]))
						NF.append(NMesh.Face([newV[ind2-1],newV[ind2-2],newV[ind2-3]]))
					
					elif nb2 == 2 :
						# We must know if the tuples are neighbours
						ind2 = ''.join([str(x) for x in lenV+lenV[:1]]).find('22')
						
						if ind2 != -1 :	 # They are 
							NF.append(NMesh.Face([newV[ind2][0],newV[ind2][1],newV[ind2-3][0],newV[ind2-3][1]]))
							NF.append(NMesh.Face([newV[ind2][0],newV[ind2-1],newV[ind2-2],newV[ind2-3][1]]))
						
						else:			   # They aren't
							ind2 = lenV.index(2)
							NF.append(NMesh.Face([newV[ind2][0],newV[ind2][1],newV[ind2-2][0],newV[ind2-2][1]]))
							NF.append(NMesh.Face([newV[ind2][1],newV[ind2-3],newV[ind2-2][0]]))
							NF.append(NMesh.Face([newV[ind2][0],newV[ind2-1],newV[ind2-2][1]]))
					
					elif nb2 == 3 :
						ind2 = lenV.index(3)
						NF.append(NMesh.Face([newV[ind2-1][1],newV[ind2],newV[ind2-3][0]]))
						NF.append(NMesh.Face([newV[ind2-1][0],newV[ind2-1][1],newV[ind2-3][0],newV[ind2-3][1]]))
						NF.append(NMesh.Face([newV[ind2-3][1],newV[ind2-2][0],newV[ind2-2][1],newV[ind2-1][0]]))
					
					else:
						if	(newV[0][1].co-newV[3][0].co).length + (newV[1][0].co-newV[2][1].co).length \
							< (newV[0][0].co-newV[1][1].co).length + (newV[2][0].co-newV[3][1].co).length :
							ind2 = 0
						else :
							ind2 = 1
						NF.append(NMesh.Face([newV[ind2-1][0],newV[ind2-1][1],newV[ind2][0],newV[ind2][1]]))
						NF.append(NMesh.Face([newV[ind2][1],newV[ind2-3][0],newV[ind2-2][1],newV[ind2-1][0]]))
						NF.append(NMesh.Face([newV[ind2-3][0],newV[ind2-3][1],newV[ind2-2][0],newV[ind2-2][1]]))
				
				else :					  # f is a tri
					if nb2 == 1:
						ind2 = lenV.index(2)
						NF.append(NMesh.Face([newV[ind2-2],newV[ind2-1],newV[ind2][0],newV[ind2][1]]))
					
					elif nb2 == 2:
						ind2 = lenV.index(3)
						NF.append(NMesh.Face([newV[ind2-1][1],newV[ind2],newV[ind2-2][0]]))
						NF.append(NMesh.Face([newV[ind2-2][0],newV[ind2-2][1],newV[ind2-1][0],newV[ind2-1][1]]))
					
					else:
						ind2 = min( [((newV[i][1].co-newV[i-1][0].co).length, i) for i in enumV] )[1]
						NF.append(NMesh.Face([newV[ind2-1][1],newV[ind2][0],newV[ind2][1],newV[ind2-2][0]]))
						NF.append(NMesh.Face([newV[ind2-2][0],newV[ind2-2][1],newV[ind2-1][0],newV[ind2-1][1]]))
				
				# Preparing the corners
				for i in enumV:
					if lenV[i] == 2 :	   NC.setdefault(V[i],[]).append(newV[i])
				
			
			old_faces.append(f)
			
			# Preparing the Edges
			for i in enumV:
				if Esel[i]:
					verts = [newV[i],newV[(i+1) % nV]]
					if V[i].index > V[(i+1) % nV].index : verts.reverse()
					NE.setdefault(E[i],[]).append(verts)
	
	# Create the faces
	for f in NF: me.addFace(f)

def make_edges():
	""" Make the faces corresponding to selected edges """

	for old,new in NE.iteritems() :
		if len(new) == 1 :					  # This edge was on a border 
			oldv = [old.v1, old.v2]
			if old.v1.index < old.v2.index : oldv.reverse()
			
			make_sel_face(oldv+new[0])

			me.findEdge(*oldv).flag   |= E_selected
			me.findEdge(*new[0]).flag |= E_selected

			#PY23 NO SETS# for v in oldv : NV_ext.add(v)
			for v in oldv : NV_ext[v]= None
		
		else:
			make_sel_face(new[0] + new[1][::-1])

			me.findEdge(*new[0]).flag |= E_selected
			me.findEdge(*new[1]).flag |= E_selected

def make_corners():
	""" Make the faces corresponding to corners """

	for v in NV.iterkeys():
		V = NV[v].values()
		nV = len(V)
		
		if nV == 1:	 pass
		
		elif nV == 2 :
			#PY23 NO SETS# if v in NV_ext:
			if v in NV_ext.iterkeys():
				make_sel_face(V+[v])
				me.findEdge(*V).flag   |= E_selected
				
		else:
			#PY23 NO SETS# if nV == 3 and v not in NV_ext : make_sel_face(V)
			if nV == 3 and v not in NV_ext.iterkeys() : make_sel_face(V)
			
			
			else :
				
				# We need to know which are the edges around the corner.
				# First, we look for the quads surrounding the corner.
				eed = []
				for old, new in NE.iteritems():
					if v in (old.v1,old.v2) :
						if v.index == min(old.v1.index,old.v2.index) :   ind = 0
						else										 :   ind = 1
						
						if len(new) == 1:	   eed.append([v,new[0][ind]])
						else :				  eed.append([new[0][ind],new[1][ind]])
				
				# We will add the edges coming from faces where only one vertice is selected.
				# They are stored in NC.
				if v in NC:					 eed = eed+NC[v]

				# Now we have to sort these vertices
				hc = {}
				for (a,b) in eed :
					hc.setdefault(a,[]).append(b)
					hc.setdefault(b,[]).append(a)
				
				for x0,edges in hc.iteritems():
					if len(edges) == 1 :		break
				
				b = [x0]						# b will contain the sorted list of vertices
				
				for i in xrange(len(hc)-1):
					for x in hc[x0] :
						if x not in b :		 break
					b.append(x)
					x0 = x

				b.append(b[0])

				# Now we can create the faces
				if len(b) == 5:				 make_sel_face(b[:4])

				else:
					New_V = Vector(0.0, 0.0,0.0)
					New_d = [0.0, 0.0,0.0]
		
					for x in hc.iterkeys():		 New_V += x.co
					for dir in NV[v] :
						for i in xrange(3):	 New_d[i] += dir[i]

					New_V *= 1./len(hc)
					for i in xrange(3) :		 New_d[i] /= nV
					
					center = make_sel_vert(New_V.x,New_V.y,New_V.z)
					add_to_NV(v,tuple(New_d),center)

					for k in xrange(len(b)-1):   make_sel_face([center, b[k], b[k+1]])
				
		if  2 < nV and v in NC :
			for edge in NC[v] :				 me.findEdge(*edge).flag   |= E_selected

def clear_old():
	""" Erase old faces and vertices """

	for f in old_faces: me.removeFace(f)
	
	for v in NV.iterkeys():
		#PY23 NO SETS# if v not in NV_ext :  me.verts.remove(v)
		if v not in NV_ext.iterkeys() :  me.verts.remove(v)

	for e in me.edges:
		if e.flag & E_selected :
			e.v1.sel = 1
			e.v2.sel = 1
	

######################################################################
# Interface

global dist
	
dist = Create(0.2)
left = Create(0.0)
right = Create(1.0)
num = Create(2)

# Events
EVENT_NOEVENT = 1
EVENT_BEVEL = 2
EVENT_UPDATE = 3
EVENT_RECURS = 4
EVENT_EXIT = 5

def draw():
	global dist, left, right, num, old_dist
	global EVENT_NOEVENT, EVENT_BEVEL, EVENT_UPDATE, EVENT_RECURS, EVENT_EXIT

	glClear(GL_COLOR_BUFFER_BIT)
	Button("Bevel",EVENT_BEVEL,10,100,280,25)
	
	BeginAlign()
	left=Number('',  EVENT_NOEVENT,10,70,45, 20,left.val,0,right.val,'Set the minimum of the slider')
	dist=Slider("Thickness  ",EVENT_UPDATE,60,70,180,20,dist.val,left.val,right.val,0, \
			"Thickness of the bevel, can be changed even after bevelling")
	right = Number("",EVENT_NOEVENT,245,70,45,20,right.val,left.val,200,"Set the maximum of the slider")

	EndAlign()
	glRasterPos2d(8,40)
	Text('To finish, you can use recursive bevel to smooth it')
	
	
	if old_dist != None:
		num=Number('',  EVENT_NOEVENT,10,10,40, 16,num.val,1,100,'Recursion level')
		Button("Recursive",EVENT_RECURS,55,10,100,16)
	
	Button("Exit",EVENT_EXIT,210,10,80,20)

def event(evt, val):
	if ((evt == QKEY or evt == ESCKEY) and not val): Exit()

def bevent(evt):
	if evt == EVENT_EXIT		: Exit()
	elif evt == EVENT_BEVEL	 : bevel()
	elif evt == EVENT_UPDATE	:
		try: bevel_update()
		except NameError		: pass
	elif evt == EVENT_RECURS	: recursive()

Register(draw, event, bevent)

######################################################################
def bevel():
	""" The main function, which creates the bevel """
	global me,NV,NV_ext,NE,NC, old_faces,old_dist
	
	ob = act_mesh_ob()
	if not ob: return
	
	Window.WaitCursor(1) # Change the Cursor
	t= Blender.sys.time()
	is_editmode = Window.EditMode() 
	if is_editmode: Window.EditMode(0)
	
	me = ob.data

	NV = {}
	#PY23 NO SETS# NV_ext = set()
	NV_ext= {}
	NE = {}
	NC = {}
	old_faces = []

	make_faces()
	make_edges()
	make_corners()
	clear_old()

	old_dist = dist.val
	print '\tbevel in %.6f sec' % (Blender.sys.time()-t)
	me.update(1)
	if is_editmode: Window.EditMode(1)
	Window.WaitCursor(0)
	Blender.Redraw()

def bevel_update():
	""" Use NV to update the bevel """
	global dist, old_dist
	
	if old_dist == None:
		# PupMenu('Error%t|Must bevel first.')
		return
	
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(0)
	
	fac = dist.val - old_dist
	old_dist = dist.val

	for old_v in NV.iterkeys():
		for dir in NV[old_v].iterkeys():
			for i in xrange(3):
				NV[old_v][dir].co[i] += fac*dir[i]

	me.update(1)
	if is_editmode: Window.EditMode(1)
	Blender.Redraw()

def recursive():
	""" Make a recursive bevel... still experimental """
	global dist
	from math import pi, sin
	
	if num.val > 1:
		a = pi/4
		ang = []
		for k in xrange(num.val):
			ang.append(a)
			a = (pi+2*a)/4

		l = [2*(1-sin(x))/sin(2*x) for x in ang]
		R = dist.val/sum(l)
		l = [x*R for x in l]

		dist.val = l[0]
		bevel_update()

		for x in l[1:]:
			dist.val = x
			bevel()

