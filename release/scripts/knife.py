#!BPY

"""
Name: 'Blender Knife Tool'
Blender: 232
Group: 'Modifiers'
Tooltip: 'Cut a mesh along a plane w/o creating doubles'
"""

# $Id$
#
###################################################################
#                                                                 #
# Blender Knife Tool                                              #
#                                                                 #
# v. 0.0.0 - 0.0.6 (C) December 2002 Stefano <S68> Selleri        #
# v. 0.0.7 (C) March 2004 Wim Van Hoydonck                        #
# v. 0.0.8 (C) March 2004 Wim Van Hoydonck & Stefano <S68> Selleri#
#                                                                 #
# Released under the Blender Artistic Licence (BAL)               #
# See www.blender.org                                             #
#                                                                 #
# Works in Blender 2.32 and higher                                #
#                                                                 #
# this script can be found online at:                             #
# http://users.pandora.be/tuinbels/scripts/knife-0.0.8.py         #
# http://www.selleri.org/Blender                                  #
#                                                                 #
# email: tuinbels@hotmail.com                                     #
#        selleri@det.unifi.it                                     #
###################################################################
# History                                                         #
# V: 0.0.0 - 08-12-02 - The script starts to take shape, a        #
#                       history is now deserved :)                #
#    0.0.1 - 09-12-02 - The faces are correctly selected and      #
#                       assigned to the relevant objects now the  #
#                       hard (splitting) part...                  #
#    0.0.2 - 14-12-02 - Still hacking on the splitting...         #
#                       It works, but I have to de-globalize      #
#                       the intersection coordinates              #
#    0.0.3 - 15-12-02 - First Alpha version                       #
#    0.0.4 - 17-12-02 - Upgraded accordingly to eeshlo tips       #
#                       Use Matrices for coordinate transf.       #
#                       Add a GUI                                 #
#                       Make it Run on 2.23                       #
#    0.0.5 - 17-12-02 - Eeshlo solved some problems....           #
#                       Theeth too adviced me                     #
#    0.0.6 - 18-12-02 - Better error messages                     #
#    0.0.7 - 26-03-04 - Developer team doubles!                   #
#                       This version is by Wim!                   #
#                       Doesn't create doubles (AFAIK)            #
#                     - Faster (for small meshes), global         #
#                       coordinates of verts are calculated only  #
#                       once                                      #
#                     - Editing the CutPlane in editmode (move)   #
#                       shouldn't cause problems anymore          #
#                     - Menu button added to choose between the   #
#                       different Edit Methods                    #
#                     - If a mesh is cut twice at the same place, #
#                       this gives errors :( (also happened in    #
#                       previous versions)                        #
#                     - Willian Padovani Germano solved           #
#                       a problem, many thanks :)                 #
#                     - Stefano Selleri made some good            #
#                       suggestions, thanks :)                    #
#    0.0.8 - 26-03-04 - General Interface rewrite (Stefano)       #
#    0.0.8a- 31-03-04 - Added some error messages                 #
#                     - Cut multiple meshes at once               #
#                                                                 #
###################################################################

import Blender
from Blender import *
from Blender.sys import time
from math import *

Epsilon = 0.00001
msg = ''
RBmesh0 = Draw.Create(0)
RBmesh1 = Draw.Create(0)
RBmesh2 = Draw.Create(1)

VERSION = '0.0.8'

# see if time module is available
#try:
#	import time
#	timport = 1
#except:
#	timport = 0


BL_VERSION = Blender.Get('version')
if (BL_VERSION<=223):
	import Blender210

#=================================#
# Vector and matrix manipulations #
#=================================#

# vector addition
def vecadd(a, b):
	return [a[0] - b[0], a[1] - b[1], a[2] + b[2]]

# vector substration
def vecsub(a, b):
	return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]

# vector crossproduct
def veccross(x, y):
	v = [0, 0, 0]
	v[0] = x[1]*y[2] - x[2]*y[1]
	v[1] = x[2]*y[0] - x[0]*y[2]
	v[2] = x[0]*y[1] - x[1]*y[0]
	return v

# vector dotproduct
def vecdot(x, y):
	return x[0]*y[0] + x[1]*y[1] + x[2]*y[2]

# vector length
def length(v):
	return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2])

# vector multiplied by constant s
def vecmul(a, s):
	return[a[0]*s, a[1]*s, a[2]*s]

# vector divided by constant s
def vecdiv(a, s):
	if s!=0.0: s = 1.0/s
	return vecmul(a, s)

# matrix(4x3) vector multiplication
def mulmatvec4x3(a, b):
	# a is vector, b is matrix
	r = [0, 0, 0]
	r[0] = a[0]*b[0][0] + a[1]*b[1][0] + a[2]*b[2][0] + b[3][0]
	r[1] = a[0]*b[0][1] + a[1]*b[1][1] + a[2]*b[2][1] + b[3][1]
	r[2] = a[0]*b[0][2] + a[1]*b[1][2] + a[2]*b[2][2] + b[3][2]
	return r

# Normalization of a vector
def Normalize(a):
	lengte = length(a)
	return vecdiv(a, lengte)

# calculate normal from 3 verts
def Normal(v0, v1, v2):
	return veccross(vecsub(v0, v1),vecsub(v0, v2))

#===========================#
# Coordinatetransformations #
#===========================#

def GlobalPosition(P, Obj):

	if (BL_VERSION<=223):
		m = Obj.matrix
	else:
		m = Obj.getMatrix()

	return mulmatvec4x3(P, m)

def LocalPosition(P, Obj):

	if (BL_VERSION<=223):
		m = Blender210.getObject(Obj.name).inverseMatrix
	else:
		m = Obj.getInverseMatrix()

	return mulmatvec4x3(P, m)

#================#
# Get Plane Data #
#================#

def PlaneData(Plane):
	global msg
	#
	# Calculate: 
	# - the normal of the plane, 
	# - the offset of the plane wrt the global coordinate system
	#   in the direction of the normal of the plane
	# 
	PlaneMesh   = NMesh.GetRawFromObject(Plane.name)

	if (len(PlaneMesh.faces)>1):
		msg =  "ERROR: Active object must be a single face plane"
		return ((0,0,0),(0,0,0),1)
	else:
		if (len(PlaneMesh.verts)<3):
			msg = "ERROR: 3 vertices needed to define a plane"
			return ((0,0,0),(0,0,0),1)
		else:
			v0 = GlobalPosition(PlaneMesh.faces[0].v[0].co, Plane)
			v1 = GlobalPosition(PlaneMesh.faces[0].v[1].co, Plane)
			v2 = GlobalPosition(PlaneMesh.faces[0].v[2].co, Plane)
			
			# the normal of the plane, calculated from the first 3 verts
			PNormal = Normalize(Normal(v0,v1,v2))

			# offset of the plane, using 1st vertex instead of Plane.getLocaction()
			POffset = vecdot(v0,PNormal)

			return PNormal, POffset, 0

#====================================#
# Position with respect to Cut Plane #
#====================================#

def Distance(P, N, d0):
	#
	# distance from a point to a plane
	#
	return vecdot(P, N) - d0

def FacePosition(dist):
	#
	# position of a face wrt to the plane
	#
	np, nn, nz = 0, 0, 0

	for d in dist:

		# the distances are calculated in advance
		if d > 0:
			np += 1
		elif d < 0:
			nn += 1
		else:
			nz += 1 

	if np == 0:
		return -1
	if nn == 0:
		return 1
	return 0

#==========================================#
# Append existing faces / create new faces #
#==========================================#

def FaceAppend(me, fidx):
	#
	# append a face to a mesh based on a list of vertex-indices
	#
	nf = NMesh.Face()

	for i in fidx:
		nf.v.append(me.verts[i])
	me.faces.append(nf)

def FaceMake(me, vl):
	#
	# make one or two new faces based on a list of vertex-indices
	#
	idx = len(me.verts)

 	if len(vl) <= 4:
		nf = NMesh.Face()
		for i in range(len(vl)):
			nf.v.append(me.verts[vl[i]])
		me.faces.append(nf)
	else:
		nf = NMesh.Face()
		nf.v.append(me.verts[vl[0]])
		nf.v.append(me.verts[vl[1]])
		nf.v.append(me.verts[vl[2]])
		nf.v.append(me.verts[vl[3]])
		me.faces.append(nf)

		nf = NMesh.Face()
		nf.v.append(me.verts[vl[3]])
		nf.v.append(me.verts[vl[4]])
		nf.v.append(me.verts[vl[0]])
		me.faces.append(nf)
   
#=====================================#
# Generate vertex lists for new faces #
#=====================================#

def Split(Obj, MeshPos, MeshNeg, Vglob, Vidx, N, d0, newvidx, newvcoo, totverts, d):
	#
	# - calculate intersectionpoints of the plane with faces
	# - see if this intersectionpoint already exists (look for vertices close to the new vertex)
	# - if it does not yet exist, append a vertex to the mesh,
	#   remember its index and location and append the index to the appropriate vertex-lists
	# - if it does, use that vertex (and its index) to create the face
	#
 
	vp = []
	vn = []

	# distances of the verts wrt the plane are calculated in main part of script
	
	for i in range(len(d)):
		# the previous vertex
		dim1 = d[int(fmod(i-1,len(d)))]
		Vim1 = Vglob[int(fmod(i-1,len(d)))]

		if abs(d[i]) < Epsilon:
			# if the vertex lies in the cutplane			
			vp.append(Vidx[i])
			vn.append(Vidx[i])
		else:
			if abs(dim1) < Epsilon:
				# if the previous vertex lies in cutplane
				if d[i] > 0:
					vp.append(Vidx[i])
				else:
					vn.append(Vidx[i])
			else:
				if d[i]*dim1 > 0:
					# if they are on the same side of the plane
					if d[i] > 0:
						vp.append(Vidx[i])
					else:
						vn.append(Vidx[i])
				else:
					# the vertices are not on the same side of the plane, so we have an intersection

					Den = vecdot(vecsub(Vglob[i],Vim1),N)

					Vi = []    
					Vi.append ( ((Vim1[0]*Vglob[i][1]-Vim1[1]*Vglob[i][0])*N[1]+(Vim1[0]*Vglob[i][2]-Vim1[2]*Vglob[i][0])*N[2]+(Vglob[i][0]-Vim1[0])*d0)/Den)
					Vi.append ( ((Vim1[1]*Vglob[i][0]-Vim1[0]*Vglob[i][1])*N[0]+(Vim1[1]*Vglob[i][2]-Vim1[2]*Vglob[i][1])*N[2]+(Vglob[i][1]-Vim1[1])*d0)/Den)
					Vi.append ( ((Vim1[2]*Vglob[i][0]-Vim1[0]*Vglob[i][2])*N[0]+(Vim1[2]*Vglob[i][1]-Vim1[1]*Vglob[i][2])*N[1]+(Vglob[i][2]-Vim1[2])*d0)/Den)

					ViL = LocalPosition(Vi, Obj)

					if newvidx == []: 
						# if newvidx is empty (the first time Split is called), append a new vertex
						# to the mesh and remember its vertex-index and location
						ViLl = NMesh.Vert(ViL[0],ViL[1],ViL[2])

						if MeshPos == MeshNeg:
							MeshPos.verts.append(ViLl)

						else:
							MeshPos.verts.append(ViLl)
							MeshNeg.verts.append(ViLl)

						nvidx = totverts
						newvidx.append(nvidx)
						newvcoo.append(ViL)

						vp.append(nvidx)
						vn.append(nvidx)
					else:
						# newvidx is not empty
						dist1 = []
						tlr = 0
						for j in range(len(newvidx)): 
							# calculate the distance from the new vertex to the vertices
							# in the list with new vertices
							dist1.append(length(vecsub(ViL, newvcoo[j])))
						for k in range(len(dist1)):
							if dist1[k] < Epsilon:
								# if distance is smaller than epsilon, use the other vertex
								# use newvidx[k] as vert
								vp.append(newvidx[k])
								vn.append(newvidx[k])
								break # get out of closest loop
							else:
								tlr += 1

						if tlr == len(newvidx):
							nvidx = totverts + len(newvidx)
							ViLl = NMesh.Vert(ViL[0],ViL[1],ViL[2])

							if MeshPos == MeshNeg:
								MeshPos.verts.append(ViLl)

							else:
								MeshPos.verts.append(ViLl)
								MeshNeg.verts.append(ViLl)

							newvidx.append(nvidx)
							newvcoo.append(ViL)
							vp.append(nvidx)
							vn.append(nvidx)

					if d[i] > 0:
						vp.append(Vidx[i])
					else:
						vn.append(Vidx[i])
  
	return vp, vn, newvidx, newvcoo

#===========#
# Main part #
#===========#

def CutMesh():
	global msg
	global RBmesh0,RBmesh1,RBmesh2
	#if timport == 1:
	#	start = time.clock()
	start = time()
	
	selected_obs = Object.GetSelected()

	total = len(selected_obs)

	NoErrors=0

	meshes = 0

	# check to see if every selected object is a mesh
	for ob in selected_obs:
		type = ob.getType()
		if type == 'Mesh':
			meshes += 1

	# at least select two objects
	if meshes <= 1:
		msg = "ERROR: At least two objects should be selected"
		NoErrors = 1

	# if not every object is a mesh
	if meshes != total:
		msg = "ERROR: You should only select meshobjects"
		NoErrors=1

	# everything is ok
	if NoErrors == 0:
		Pln = selected_obs[0]
		PNormal, POffset, NoErrors = PlaneData(Pln)

	# loop to cut multiple meshes at once
	for o in range(1, total):
		
		Obj = selected_obs[o]

		if (NoErrors == 0) :
		
			m = Obj.getData()

			if RBmesh1.val == 1:

				MeshNew = NMesh.GetRaw()

			if RBmesh2.val == 1:

				MeshPos = NMesh.GetRaw()
				MeshNeg = NMesh.GetRaw()

			# get the indices of the faces of the mesh
			idx = []
			for i in range(len(m.faces)):
				idx.append(i)

			# if idx is not reversed, this results in a list index out of range if
			# the original mesh is used (RBmesh1 == 0)
			idx.reverse()

			lenface, vertglob, vertidx, vertdist = [], [], [], []

			# total number of vertices
			totverts = len(m.verts)

			# for every face: calculate global coordinates of the vertices
			#                 append the vertex-index to a list
			#                 calculate distance of vertices to cutplane in advance

			for i in idx:
				fvertidx, Ve, dist = [], [], []
				fa = m.faces[i]
				lenface.append(len(fa))
				for v in fa.v:
					globpos = GlobalPosition(v.co, Obj)
					Ve.append(globpos)
					fvertidx.append(v.index)
					dist.append(Distance(globpos, PNormal, POffset))
				vertidx.append(fvertidx)
				vertglob.append(Ve)
				vertdist.append(dist)


			# append the verts of the original mesh to the new mesh
			if RBmesh1.val == 1:
				for v in m.verts:
					MeshNew.verts.append(v)
	
			if RBmesh2.val == 1:
				idx2 = []
				dist2 = []
				for v in m.verts:
					MeshPos.verts.append(v)
					MeshNeg.verts.append(v)
					idx2.append(v.index)
					dist2.append(Distance(GlobalPosition(v.co, Obj), PNormal, POffset))

			# remove all faces of m if the original object has to be used

			if RBmesh0.val == 1:
				m.faces = []

			newvidx, newvcoo = [], []
			testidxpos, testidxneg = [], []

			# what its all about...
			for i in idx:
				fp = FacePosition(vertdist[i])

				# no intersection
				if fp > 0:
					if RBmesh0.val == 1:
						FaceAppend(m, vertidx[i])
			
					elif RBmesh1.val == 1:
						FaceAppend(MeshNew, vertidx[i])
				
					elif RBmesh2.val == 1:
						FaceAppend(MeshPos, vertidx[i])

						if testidxpos == []:
							testidxpos = vertidx[i]
				elif fp < 0:
					if RBmesh0.val == 1:
						FaceAppend(m, vertidx[i])
					elif RBmesh1.val == 1:
						FaceAppend(MeshNew, vertidx[i])
					
					elif RBmesh2.val == 1:
						FaceAppend(MeshNeg, vertidx[i])

						if testidxneg == []:
							testidxneg = vertidx[i]

				# intersected faces
				else:
					# make new mesh
					if RBmesh1.val == 1:
						vlp, vln, newvidx, newvcoo = Split(Obj, MeshNew, MeshNew, vertglob[i], vertidx[i], PNormal, POffset, newvidx, newvcoo, totverts, vertdist[i])

						if vlp != 0 and vln != 0:
							FaceMake(MeshNew, vlp)
							FaceMake(MeshNew, vln)
						# two new meshes
					elif RBmesh2.val == 1:
						vlp, vln, newvidx, newvcoo = Split(Obj, MeshPos, MeshNeg, vertglob[i], vertidx[i], PNormal, POffset, newvidx, newvcoo, totverts, vertdist[i])
	
						if vlp != 0 and vln != 0:
							FaceMake(MeshPos, vlp)
							FaceMake(MeshNeg, vln)

					# use old mesh
					elif RBmesh0.val == 1:
	
						vlp, vln, newvidx, newvcoo = Split(Obj, m, m, vertglob[i], vertidx[i], PNormal, POffset, newvidx, newvcoo, totverts, vertdist[i])
	
						if vlp != 0 and vln != 0:
							FaceMake(m, vlp)
							FaceMake(m, vln)

			if RBmesh1.val == 1:

				ObOne = NMesh.PutRaw(MeshNew)

				ObOne.LocX, ObOne.LocY, ObOne.LocZ = Obj.LocX, Obj.LocY, Obj.LocZ
				ObOne.RotX, ObOne.RotY, ObOne.RotZ = Obj.RotX, Obj.RotY, Obj.RotZ
				ObOne.SizeX, ObOne.SizeY, ObOne.SizeZ = Obj.SizeX, Obj.SizeY, Obj.SizeZ

			elif RBmesh2.val == 1:

				# remove verts that do not belong to a face
				idx2.reverse()
				dist2.reverse()

				for i in range(len(idx2)):
					if dist2[i] < 0:
						v = MeshPos.verts[idx2[i]]
						MeshPos.verts.remove(v)
					if dist2[i] > 0:
						v = MeshNeg.verts[idx2[i]]
						MeshNeg.verts.remove(v)

				ObPos = NMesh.PutRaw(MeshPos)

				ObPos.LocX, ObPos.LocY, ObPos.LocZ = Obj.LocX, Obj.LocY, Obj.LocZ
				ObPos.RotX, ObPos.RotY, ObPos.RotZ = Obj.RotX, Obj.RotY, Obj.RotZ
				ObPos.SizeX, ObPos.SizeY, ObPos.SizeZ = Obj.SizeX, Obj.SizeY, Obj.SizeZ

				ObNeg = NMesh.PutRaw(MeshNeg)

				ObNeg.LocX, ObNeg.LocY, ObNeg.LocZ = Obj.LocX, Obj.LocY, Obj.LocZ
				ObNeg.RotX, ObNeg.RotY, ObNeg.RotZ = Obj.RotX, Obj.RotY, Obj.RotZ
				ObNeg.SizeX, ObNeg.SizeY, ObNeg.SizeZ = Obj.SizeX, Obj.SizeY, Obj.SizeZ

			elif RBmesh0.val == 1:
				m.update()


	#if timport == 1:
		#end = time.clock()
		#total = end - start
		#print "mesh(es) cut in", total, "seconds" 

	end = time()
	total = end - start
	print "mesh(es) cut in", total, "seconds"

#############################################################
# Graphics                                                  #
#############################################################
def Warn():
	BGL.glRasterPos2d(115, 23)
        Blender.Window.Redraw(Blender.Window.Const.TEXT)

def draw():
	global msg
	global RBmesh0,RBmesh1,RBmesh2
	global VERSION
	
	BGL.glClearColor(0.5, 0.5, 0.5, 0.0)
	BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
	BGL.glColor3f(0, 0, 0) 			# Black
	BGL.glRectf(2, 2, 482, 220)
	BGL.glColor3f(0.48, 0.4, 0.57) 		# Light Purple
	BGL.glRectf(4, 179, 480, 210)
	BGL.glRectf(4, 34, 480, 150)
	BGL.glColor3f(0.3, 0.27, 0.35) 		# Dark purple
	BGL.glRectf(4, 151,480, 178)
	BGL.glRectf(4, 4, 480, 33)
	

	BGL.glColor3f(1, 1, 1)
	BGL.glRasterPos2d(8, 200)
	Draw.Text("Blender Knife Tool -  V. 0.0.8a - 26 March 2004")
	BGL.glRasterPos2d(8, 185)
	Draw.Text("by Wim <tuinbels> Van Hoydonck & Stefano <S68> Selleri")
	Draw.Button("Exit", 1, 430, 185, 40, 20)

	RBmesh0 = Draw.Toggle("Edit Object",    10,10,157,153,18,RBmesh0.val, "The knife creates new vertices in the selected object.");
	RBmesh1 = Draw.Toggle("New Object",     11,165,157,153,18,RBmesh1.val, "The knife duplicates the object and creates new vertices in the new object.");
	RBmesh2 = Draw.Toggle("Two New Objects",12,320,157,153,18,RBmesh2.val, "The knife creates two new separate objects.");

	BGL.glRasterPos2d(8, 128)
	Draw.Text("1 - Draw a Mesh Plane defining the Cut Plane")
	BGL.glRasterPos2d(8, 108)
	Draw.Text("2 - Select the Meshes to be Cut and the Cut Plane")
	BGL.glRasterPos2d(8, 88)
	Draw.Text("      (Meshes Dark Purple, Plane Light Purple)")
	BGL.glRasterPos2d(8, 68)
	Draw.Text("3 - Choose the Edit Method (Radio Buttons above)")
	BGL.glRasterPos2d(8, 48)
	Draw.Text("4 - Push the 'CUT' button (below)")
	#Create Buttons
	Draw.Button("CUT", 4, 10, 10, 465, 18, "Cut the selected mesh along the plane")

	
	BGL.glRasterPos2d(10, 223)
	BGL.glColor3f(1,0,0)
	Draw.Text(msg)
	msg = ''
	
def event(evt, val):
	if evt == Draw.QKEY and not val:
		Draw.Exit()
	if evt == Draw.CKEY and not val:
		CutMesh()
		Draw.Redraw()

def bevent(evt):
	global RBmesh0,RBmesh1,RBmesh2

	if evt == 1:
		Draw.Exit()
	elif evt == 4:
		CutMesh()
		Draw.Redraw()
	elif evt == 10:
		RBmesh0.val = 1
		RBmesh1.val = 0
		RBmesh2.val = 0
		Draw.Redraw()
	elif evt == 11:
		RBmesh0.val = 0
		RBmesh1.val = 1
		RBmesh2.val = 0
		Draw.Redraw()
	elif evt == 12:
		RBmesh0.val = 0
		RBmesh1.val = 0
		RBmesh2.val = 1
		Draw.Redraw()

Draw.Register(draw, event, bevent)
