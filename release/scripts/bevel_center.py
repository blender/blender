#!BPY

""" Registration info for Blender menus
Name: 'Bevel Center'
Blender: 234
Group: 'Mesh'
Tip: 'Bevel selected vertices'
"""

__author__ = "Loic Berthe"
__url__ = ("blender", "elysiun")
__version__ = "1.0"

__bpydoc__ = """\
This script implements vertex bevelling in Blender.

Usage:

Select the mesh you want to work on, enter Edit Mode and select the vertices
to bevel.  Then run this script from the 3d View's Mesh->Scripts menu.

You can control the thickness of the bevel with the slider -- redefine the
end points for bigger or smaller ranges.  The thickness can be changed even
after applying the bevel, as many times as needed.

For an extra smoothing after or instead of direct bevel, set the level of
recursiveness and use the "Recursive" button.

Notes:<br>
    You can undo and redo your steps just like with normal mesh operations in
Blender.
"""

# $Id$
#
######################################################################
# Bevel Center v1 for Blender
#
# This script lets you bevel the selected vertices and control the
# thickness of the bevel
#
# (c) 2004 Loïc Berthe (loic.berthe@lilotux.net)
# released under Blender Artistic License
#
######################################################################

import Blender
from Blender import NMesh, Window
from Blender.Draw import *
from Blender.BGL import *

from math import pi, sin, sqrt

######################################################################
# Functions to handle the global structures of the script NV, NE and NC
# which contain informations about the vertices, faces and corners to be
# created

class Dir:
	def __init__(self, co):
		self.co = co

def add_to_NV(old,co,new):
	dir = Dir(co)
	#
	if old in NV.keys():
		NV[old][dir] = new
	else:
		NV[old] = {dir:new}

def is_in_NV(old,co):
	if old in NV.keys():
		for dir in NV[old]:
			if dir.co == co : return NV[old][dir]
	#
	return False

def add_to_NE(old, new):
	ind1 = old[0].index
	ind2 = old[1].index
	if ind1 > ind2:
		new.reverse()
		ind1,ind2 = ind2,ind1
	id = str(ind1)+"_"+str(ind2)
	if id in NE.keys():
		[NE[id].append(v) for v in new]
	else:
		NE[id] = new

def add_to_NC(old,edge):
	if old in NC.keys():
		NC[old].append(edge)
	else:
		NC[old] = [edge]
		
######################################################################
# Geometric functions
		
def norm(vec):
	n = sqrt(vec[0]**2+vec[1]**2+vec[2]**2)
	return [vec[0]/n,vec[1]/n,vec[2]/n]

def parall_coord(old, dir):
	co = old.co
	vec = [0.0,0.0,0.0]
	nco = [0.0,0.0,0.0]
	#
	if len(dir) == 1:
		for i in range(3): vec[i] = dir[0].co[i] - co[i] 
		vec = norm(vec)
	#
	elif len(dir) == 2:
		vec1 = [0.0,0.0,0.0]
		vec2 = [0.0,0.0,0.0]
		for i in range(3):
			vec1[i] = dir[0].co[i] - co[i] 
			vec2[i] = dir[1].co[i] - co[i] 
		vec1 = norm(vec1)
		vec2 = norm(vec2)
		for i in range(3) : vec[i] = vec1[i]+vec2[i]
	#
	for i in range(3): nco[i] = co[i] + dist.val*vec[i]
	return (nco,vec)

def get_vert(old, dir):
	""" Look in NV if a vertice corresponding to the vertex old and the
	direction dir already exists, and create one otherwise""" 
	(nco, vec) = parall_coord(old, dir)
	v = is_in_NV(old,vec)
	if v: return v
	#
	v = NMesh.Vert(nco[0],nco[1],nco[2])
	v.sel = 1
	me.verts.append(v)
	add_to_NV(old,vec,v)
	return v
			
######################################################################
# Functions to create the differents faces
	
def make_NF():
	""" Analyse the mesh, sort the faces containing selected vertices and
	create a liste NF : NF = [[flag, vertlist, old_face]]. Flag describes the
	topology of the face.""" 
	#
	for f in me.faces:
		V = f.v
		v_sel = [x.sel for x in V]
		nb_sel = sum(v_sel)
		if nb_sel == 0 :
			pass
		else:
			nb_v = len(V)
			#
			if nb_v == 4:
				#
				if nb_sel == 4:
					NF.append([1,V,f])
				#
				elif nb_sel == 3:
					if v_sel == [0,1,1,1]: V = [V[1],V[2],V[3],V[0]]
					elif v_sel == [1,0,1,1]: V = [V[2],V[3],V[0],V[1]]
					elif v_sel == [1,1,0,1]: V = [V[3],V[0],V[1],V[2]]
					NF.append([2,V,f])
				#		
				elif nb_sel == 2:
					if v_sel == [1,0,1,0] or v_sel == [0,1,0,1]:
						if	v_sel == [0,1,0,1]: V = [V[1],V[2],V[3],V[0]]
						NF.append([5,[V[0],V[1],V[3]],f])
						NF.append([5,[V[2],V[1],V[3]]])
					else:
						if v_sel == [0,1,1,0]: V = [V[1],V[2],V[3],V[0]]
						elif v_sel == [0,0,1,1]: V = [V[2],V[3],V[0],V[1]]
						elif v_sel == [1,0,0,1]: V = [V[3],V[0],V[1],V[2]]
						NF.append([3,V,f])
				#
				else:
					if v_sel == [0,1,0,0]: V = [V[1],V[2],V[3],V[0]]
					elif v_sel == [0,0,1,0]: V = [V[2],V[3],V[0],V[1]]
					elif v_sel == [0,0,0,1]: V = [V[3],V[0],V[1],V[2]]
					NF.append([4,V,f])
			#
			elif nb_v == 3:
				#
				if nb_sel == 3:
					NF.append([6,V,f])
				#
				elif nb_sel == 2:
					if v_sel == [0,1,1]: V = [V[1],V[2],V[0]]
					elif v_sel == [1,0,1]: V = [V[2],V[0],V[1]]
					NF.append([7,V,f])
				#
				else:
					if v_sel == [0,1,0]: V = [V[1],V[2],V[0]]
					elif v_sel == [0,0,1]: V = [V[2],V[0],V[1]]
					NF.append([5,V,f])

def make_faces():
	""" Make the new faces according to NF """
	#
	for N in NF:
		cas = N[0]
		V = N[1]
		#
		if cas < 6:
			new_v = [0,0,0,0]
			if cas == 1:				# v_sel = [1,1,1,1]
				for i in range(-1,3):
					new_v[i] = get_vert(V[i],[V[i-1],V[i+1]])
				new_f = NMesh.Face(new_v)
				me.faces.append(new_f)
				for i in range(-1,3):
					add_to_NE([V[i],V[i+1]],[new_v[i],new_v[i+1]])
			#
			elif cas == 2:				# v_sel = [1,1,1,0]
				new_v[0] = get_vert(V[0],[V[3]])
				new_v[1] = get_vert(V[1],[V[0],V[2]])
				new_v[2] = get_vert(V[2],[V[3]])
				new_v[3] = V[3]
				#
				new_f = NMesh.Face(new_v)
				me.faces.append(new_f)
				#
				add_to_NE([V[0],V[1]],[new_v[0],new_v[1]])
				add_to_NE([V[1],V[2]],[new_v[1],new_v[2]])
			#		
			elif cas == 3:				# v_sel = [1,1,0,0]
				new_v[0] = get_vert(V[0],[V[3]])
				new_v[1] = get_vert(V[1],[V[2]])
				new_v[2] = V[2]
				new_v[3] = V[3]
				#
				new_f = NMesh.Face(new_v)
				me.faces.append(new_f)
				#
				add_to_NE([V[0],V[1]],[new_v[0],new_v[1]])
			#
			elif cas == 4:				# v_sel = [1,0,0,0]
				new_v[0] = get_vert(V[0],[V[3]])
				new_v[1] = get_vert(V[0],[V[1]])
				new_v[2] = V[1]
				new_v[3] = V[3]
				#
				new_f = NMesh.Face(new_v)
				me.faces.append(new_f)
				#
				add_to_NC(V[0], new_v[0:2])
				#
				new_v[0] = V[1]
				new_v[1] = V[2]
				new_v[2] = V[3]
				#
				new_f = NMesh.Face(new_v[:3])
				me.faces.append(new_f)
			#
			else:				# v_sel = [1,0,0]
				new_v[0] = get_vert(V[0],[V[2]])
				new_v[1] = get_vert(V[0],[V[1]])
				new_v[2] = V[1]
				new_v[3] = V[2]
				#
				new_f = NMesh.Face(new_v)
				me.faces.append(new_f)
				#
				add_to_NC(V[0], new_v[0:2])
		#
		else:
			new_v = [0,0,0]
			#
			if cas == 6:				# v_sel = [1,1,1]
				for i in range(-1,2):
					new_v[i] = get_vert(V[i],[V[i-1],V[i+1]])
				new_f = NMesh.Face(new_v)
				me.faces.append(new_f)
				for i in range(-1,2):
					add_to_NE([V[i],V[i+1]],[new_v[i],new_v[i+1]])
			#
			elif cas == 7:				# v_sel = [1,1,0]
				new_v[0] = get_vert(V[0],[V[2]])
				new_v[1] = get_vert(V[1],[V[2]])
				new_v[2] = V[2]
				#
				new_f = NMesh.Face(new_v)
				me.faces.append(new_f)
				add_to_NE([V[0],V[1]],[new_v[0],new_v[1]])

def make_edges():
	""" Make the faces corresponding to selected edges """
	#
	for l in NE.values():
		if len(l) == 4:
			f = NMesh.Face([l[0],l[1],l[3],l[2]])
			me.faces.append(f)

def make_corners():
	""" Make the faces corresponding to selected corners """
	#
	for v in NV.keys():
		V = NV[v].values()
		nb_v = len(V)
		#
		if nb_v < 3:
			pass
		#
		elif nb_v == 3:
			new_f = NMesh.Face(V)
			me.faces.append(new_f)
		#
		else:
			# We need to know which are the edges around the corner.
			# First, we look for the quads surrounding the corner.
			q = [NE[id] for id in NE.keys() if str(v.index) in id.split('_')]
			#
			# We will put the associated edges in the list eed
			is_in_v = lambda x:x in V
			eed =  [filter(is_in_v, l) for l in q]
			#
			# We will add the edges coming from faces where only one vertice is selected.
			# They are stocked in NC.
			if v in NC.keys():
				eed = eed+NC[v]
			b = eed.pop()
			# b will contain the sorted list of vertices
			#
			while  eed:
				for l in eed:
					if l[0] == b[-1]:
						b.append(l[1])
						eed.remove(l)
						break
					elif l[1] == b[-1]:
						b.append(l[0])
						eed.remove(l)
						break
			# Now we can create the faces
			if nb_v == 4:
				new_f = NMesh.Face(b[:4])
				me.faces.append(new_f)
			#
			else:
				co = [0.0, 0.0,0.0]
				vec = [0.0, 0.0,0.0]
				for x in V:
					co[0] += x[0]
					co[1] += x[1]
					co[2] += x[2]
				#
				for dir in NV[v]:
					vec[0] += dir.co[0]
					vec[1] += dir.co[1]
					vec[2] += dir.co[2]
				#
				co = [x/nb_v for x in co]
				vec = [x/nb_v for x in vec]
				center = NMesh.Vert(co[0],co[1],co[2])
				center.sel = 1
				me.verts.append(center)
				add_to_NV(v,vec,center)
				#
				for k in range(nb_v):
					new_f = NMesh.Face([center, b[k], b[k+1]])
					me.faces.append(new_f)
		#

def clear_old():
	""" Erase old faces and vertices """
	for F in NF:
		if len(F) == 3:
			me.faces.remove(F[2])
	#
	for v in NV.keys():
		me.verts.remove(v)

######################################################################
# Interface
#
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
	global dist, left, right, num
	global EVENT_NOEVENT, EVENT_BEVEL, EVENT_UPDATE, EVENT_RECURS, EVENT_EXIT

	glClear(GL_COLOR_BUFFER_BIT)
	Button("Bevel",EVENT_BEVEL,10,100,280,25)
	left=Number('',  EVENT_NOEVENT,10,70,45, 20,left.val,0,right.val,'Set the minimum of the slider')
	right = Number("",EVENT_NOEVENT,245,70,45,20,right.val,left.val,200,"Set the maximum of the slider")
	dist=Slider("Thickness	",EVENT_UPDATE,60,70,180,20,dist.val,left.val,right.val,0,"Thickness of the bevel, can be changed even after bevelling")
	glRasterPos2d(8,40)
	Text('To finish, you can use recursive bevel to smooth it')
	num=Number('',	EVENT_NOEVENT,10,10,40, 16,num.val,1,100,'Recursion level')
	Button("Recursive",EVENT_RECURS,55,10,100,16)
	Button("Exit",EVENT_EXIT,210,10,80,20)

def event(evt, val):
	if ((evt == QKEY or evt == ESCKEY) and not val):
		Exit()

def bevent(evt):
	if evt == EVENT_EXIT :
		Exit()
	#
	elif evt == EVENT_BEVEL:
		bevel()
	#
	elif evt == EVENT_UPDATE:
		try:
			bevel_update()
		except NameError:
			pass
	#
	elif evt == EVENT_RECURS:
		recursive()

Register(draw, event, bevent)

######################################################################
def bevel():
	""" The main function, which creates the bevel """
	global me,NF,NV,NE,NC, old_dist
	#
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(0)
	objects = Blender.Object.GetSelected() 
	me = NMesh.GetRaw(objects[0].data.name)
	#
	NF = []
	NV = {}
	NE = {}
	NC = {}
	#
	make_NF()
	make_faces()
	make_edges()
	make_corners()
	clear_old()
	#
	old_dist = dist.val
	#
	me.update(1)
	if is_editmode: Window.EditMode(1)
	Blender.Redraw()

def bevel_update():
	""" Use NV to update the bevel """
	global dist, old_dist
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(0)
	fac = dist.val - old_dist
	old_dist = dist.val
	#
	for old_v in NV.keys():
		for dir in NV[old_v].keys():
			for i in range(3):
				NV[old_v][dir].co[i] += fac*dir.co[i]
	#
	me.update(1)
	if is_editmode: Window.EditMode(1)
	Blender.Redraw()

def recursive():
	""" Make a recursive bevel... still experimental """
	global dist
	#
	if num.val > 1:
		a = pi/4
		ang = []
		for k in range(num.val):
			ang.append(a)
			a = (pi+2*a)/4
		#
		l = [2*(1-sin(x))/sin(2*x) for x in ang]
		R = dist.val/sum(l)
		l = [x*R for x in l]
		#
		dist.val = l[0]
		bevel_update()
		#
		for x in l[1:]:
			dist.val = x
			bevel()
	
# vim:set ts=4 sw=4:

