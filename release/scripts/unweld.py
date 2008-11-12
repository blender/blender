#!BPY
""" Registration info for Blender menus: <- these words are ignored
Name: 'Unweld vertex/ices'
Blender: 243
Group: 'Mesh'
Tip: 'Unweld all faces from a (or several) selected and common vertex. Made vertex bevelling'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "blenderartists.org",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_faces2vertex.htm#exemple",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "0.4.6 "
__bpydoc__ = """\
This script unwelds faces from one or several selected vertex/vertices.

Usage:

In edit mode Select at least one vertex, then run this script.

The options are:

- unbind points;<br>
		a new point is added to each face connected to the selected one.
		
- with noise;<br>
		the new points location is varied with noise
		
- middle face;<br>
		the new point is located at the center of face to which it is connected
"""

# ------------------------------------------
# Un-Weld script 0.4.6 
# name="UnWeld"
# Tip= 'Unweld all faces from a selected and common vertex.'
# date='06/08/2006'
# split all faces from one selected vertex
# (c) 2004 J-M Soler released under GPL licence
#----------------------------------------------
# Official Page :
# website = 'http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_faces2vertex.htm#exemple'
# Communicate problems and errors on:
# community = 'http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender'
#----------------------------------------------
# Blender Artistic License
# http://download.blender.org/documentation/html/x21254.html
#---------------------------------------------
# Changelog
#----------------------------------------------
# 25/05 :
# -- separate choice, normal (same place) or spread at random, middle of the face
# -- works on several vertices too
# -- Quite vertex bevelling on <<lone>> vertex : create hole in faces around this
# vertex
# 03/06 :
# -- a sort of "bevelled vertex" extrusion controled by horizontal mouse
# displacement. just a beta test to the mouse control.
# 08/08 :
# -- minor correction to completely disconnect face.
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_faces2vertex.htm#exemple
# Commsoler les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
# Blender Artistic License
#    http://download.blender.org/documentation/html/x21254.html
#--------------------------------------------- 
# Changelog
#----------------------------------------------
#     25/05 :
#           -- separation du choix, normal ou dispersion hasardeuse, 
#              milieu de face
#           -- sommets multiples / 
#           -- presque  unvertex bevelling sur un vertex solitaire : cree
#              un trou dans la facette autour du sommet
#     03/06 :
#           -- une sorte de vertex extruder en biseau, controle par
#              movement horizontal de la souris 
#     08/08 :
#           -- correction mineure pour s'assurer que les faces soient 
#              entierment deconnectees
#----------------------------------------------

import Blender
from Blender import Noise
from Blender.Draw import *
from Blender.BGL import *
import BPyMessages
# $Id$

NR=Noise.random
DECAL=0.03
t=[0.0,0.0,0.0]
pl=[]
orig=[]

DEBUG = 0
SUBSURF=0
DIM=Create(1.0)

def  Buffer(v,t):
	if DEBUG : print dir(v)
	for n in range(len(v)): t[n]=t[n]+v[n]
	return t

def  freeBuffer(t):
	for n in range(3): t[n]=0.0
	return t

def  ModalBuffer(t,f):      
	for n in range(3): t[n]/=len(f)
	return t

def  applyModalValue(v,t):
	for n in range(len(v)): v[n]=t[n]
	return v

def docF(f0,f):

	if f0 and f:
		f0.mat=f.mat
		if f.uv :
			f0.uv=f.uv 
		if f.col :
			f0.col=f.col
		if f.image :
			f0.image=f.image
		f0.smooth=f.smooth
		f0.mode=f.mode
		f0.flag=f.flag
		return f0
	

def connectedFacesList(me,thegood):
	listf2v={}
	#tri des faces connectees aux sommets selectionnes                           
	for f in me.faces:
		for v in f.v:
			if v==thegood:
				if v.index not in listf2v: # .keys()
					listf2v[me.verts.index(v)]=[f]
				elif f not in listf2v[me.verts.index(v)]:
					listf2v[me.verts.index(v)].append(f)
	return listf2v


def createAdditionalFace(me,thegood,listf2v):
	global t
	for f in listf2v[thegood.index]:
		f0=Blender.NMesh.Face()
		if result==3: t=freeBuffer(t)
		for v in f.v:
			if result==3: t=Buffer(v,t)
			if v!=thegood:
				f0.append(v)
			else:
				if result==2:                           
					nv=Blender.NMesh.Vert(thegood.co[0]+NR()*DECAL,
						thegood.co[1]+NR()*DECAL,
						thegood.co[2]+NR()*DECAL)
				else:
						nv=Blender.NMesh.Vert(thegood.co[0],
							thegood.co[1],
							thegood.co[2])
				nv.sel=1
				me.verts.append(nv)
				f0.append(me.verts[me.verts.index(nv)])
				localise=me.verts.index(nv)                          
			docF(f0,f)   
		if result==3:
					 t=ModalBuffer(t,f0.v)
					 me.verts[localise]=applyModalValue(me.verts[localise],t)
		me.faces.append(f0)                  
	del me.verts[me.verts.index(thegood)]
	for f in listf2v[thegood.index]:
			del me.faces[me.faces.index(f)]
	return me

def collecte_edge(listf2v,me,thegood):
	back=0
	edgelist = []
	vertlist = []
	if DEBUG : print listf2v    
	for face in listf2v[thegood.index]:
		if len(face.v) == 4:
			vlist = [0,1,2,3,0]
		elif len(face.v) == 3:
			vlist = [0,1,2,0]
		else:
			vlist = [0,1]
		for i in xrange(len(vlist)-1):              
			vert0 = min(face.v[vlist[i]].index,face.v[vlist[i+1]].index)
			vert1 = max(face.v[vlist[i]].index,face.v[vlist[i+1]].index)              
			edgeinlist = 0
			if vert0==thegood.index or vert1==thegood.index:                 
				for edge in edgelist:
					if ((edge[0]==vert0) and (edge[1]==vert1)):
						edgeinlist = 1
						edge[2] = edge[2]+1
						edge.append(me.faces.index(face))
						break                  
				if edgeinlist==0:
					edge = [vert0,vert1,1,me.faces.index(face)]
					edgelist.append(edge)
					
	for i, edge in enumerate(edgelist):
		#print edge
		if len(edge)==4:
			del edgelist[i]
				
	edges=len(edgelist)
	if DEBUG : print 'number of edges : ',edges," Edge list : " ,edgelist    
	return edges, edgelist     

import bpy
OBJECT= bpy.data.scenes.active.objects.active

if OBJECT and OBJECT.type=='Mesh':
	if OBJECT.getData(mesh=1).multires:
		BPyMessages.Error_NoMeshMultiresEdit()
	elif not BPyMessages.Warning_MeshDistroyLayers(OBJECT.getData(mesh=1)):
		pass
	else:
		EDITMODE=Blender.Window.EditMode()
		Blender.Window.EditMode(0)
		name = "Unweld %t|Unbind Points %x1|With Noise %x2|Middle Face %x3"
		result = Blender.Draw.PupMenu(name)
		if result:
			me=OBJECT.getData()
			
			for v in me.verts:
				if v.sel:
					thegood=v    
					if DEBUG : print thegood
					listf2v=connectedFacesList(me,thegood)
					if listf2v:
						me=createAdditionalFace(me,thegood,listf2v)
						#OBJECT.link(me)
						me.update()
			
			OBJECT.makeDisplayList()
			
		Blender.Window.EditMode(EDITMODE)
	
else:
	BPyMessages.Error_NoMeshActive()
