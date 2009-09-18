#!BPY

"""
 Name: 'Autodesk DXF (.dxf/dwg)'
 Blender: 249
 Group: 'Export'
 Tooltip: 'Export geometry to DXF/DWG-r12 (Drawing eXchange Format).'
"""

__version__ = "1.35 - 2009.06.18"
__author__  = "Remigiusz Fiedler (AKA migius)"
__license__ = "GPL"
__url__  = "http://wiki.blender.org/index.php/Scripts/Manual/Export/autodesk_dxf"
__bpydoc__ ="""The script exports Blender geometry to DXF format r12 version.

Version %s
Copyright %s
License %s

extern dependances: dxfLibrary.py, dxfColorMap.py (optionaly: DConvertCon.exe)

CONTRIBUTORS:
Remigiusz Fiedler (AKA migius)
Alexandros Sigalas (AKA alxarch)
Stani Michiels (AKA stani)

See the homepage for documentation.
url: %s

IDEAs:
- HPGL output, usefull for correct scaled printing of 2d drawings
		
TODO:
- export dupligroups and dupliverts as blocks (as option) 
- optimize POLYFACE routine: remove double-vertices
- fix support for X,Y-rotated curves(to POLYLINEs): fix blender negative-matrix.invert()
- support hierarchies: groups, instances, parented structures
- support n/f-gons as POLYFACEs with invisible edges
- mapping materials to DXF-styles
- ProgressBar
- export rotation of Camera to VIEW/VPORT
- export parented Cameras to VIEW/VPORT
- wip: write drawing extends for automatic view positioning in CAD
- wip: fix text-objects in persp-projection
- wip: translate current 3D-View to *ACTIVE-VPORT
- wip: fix support Include-Duplis, cause not conform with INSERT-method

History
v1.35 - 2009.06.18 by migius
- export multiple-instances of Curve-Objects as BLOCK/INSERTs
- added export Cameras (ortho and persp) to VPORTs, incl. clipping
- added export Cameras (ortho and persp) to VIEWs, incl. clipping
- export multiple-instances of Mesh-Objects as BLOCK/INSERTs
- on start prints dxfLibrary version
v1.34 - 2009.06.08 by migius
- export Lamps and Cameras as POINTs
- export passepartout for perspective projection
- added option for export objects only from visible layers
- optimized POLYFACE output: remove loose vertices in back-faces-mode
- cleaning code
- fix nasty bug in getExtrusion()
- support text-objects, also in ortho/persp-projection
- support XYmirrored 2d-curves to 2dPOLYLINEs
- support thickness and elevation for curve-objects
- fix extrusion 210-code (3d orientation vector)
- fix POLYFACE export, synchronized also dxfLibrary.py
- changed to the new 2.49 method Vector.cross()
- output style manager (first try)
v1.33 - 2009.05.25 by migius
- bugfix flipping normals in mirrored mesh-objects
- added UI-Button for future Shadow Generator
- support curve objects in projection-2d mode
- UI stuff: camera selector/manager
v1.32 - 2009.05.22 by migius
- debug mode for curve-objects: output redirect to Blender
- wip support 210-code(extrusion) calculation
- default settings for 2D and 3D export
v1.31 - 2009.05.18 by migius
- globals translated to GUI_A/B dictionary
- optimizing back-faces removal for "hidden-lines" mode
- presets for global location and scale (architecture)
- UI layout: scrollbars, pan with MMB/WHEEL, dynamic width
- new GUI with Draw.Register() from DXF-importer.py
v1.30 - 2008.12.14 by migius
- started work on GUI with Draw.Register()
v1.29 - 2009.04.11 by stani
- added DWG support, Stani Michiels idea for binding an extern DXF-DWG-converter 
v1.28 - 2009.02.05 by Alexandros Sigalas (alxarch)
- added option to apply modifiers on exported meshes
- added option to also export duplicates (from dupliverts etc)
v1.28 - 2008.10.22 by migius
- workaround for PVert-bug on ubuntu (reported by Yorik)
- add support for FGons - ignore invisible_tagged edges
- add support for camera: ortho and perspective
v1.27 - 2008.10.07 by migius
- exclude Stani's DXF-Library to extern module
v1.26 - 2008.10.05 by migius
- add "hidden mode" substitut: back-faces removal
- add support for mesh ->POLYFACE
- optimized code for "Flat" procedure
v1.25 - 2008.09.28 by migius
- modif FACE class for r12
- add mesh-polygon -> Bezier-curve converter (Yorik's code)
- add support for curves ->POLYLINEs
- add "3d-View to Flat" - geometry projection to XY-plane
v1.24 - 2008.09.27 by migius
- add start UI with preferences
- modif POLYLINE class for r12
- changing output format from r9 to r12(AC1009)
v1.23 - 2008.09.26 by migius
- add finish message-box
v1.22 - 2008.09.26 by migius
- add support for curves ->LINEs
- add support for mesh-edges ->LINEs
v1.21 - 2008.06.04 by migius
- initial adaptation for Blender
v1.1 (20/6/2005) by Stani Michiels www.stani.be/python/sdxf
- Python library to generate dxf drawings
______________________________________________________________
""" % (__author__,__version__,__license__,__url__)

# --------------------------------------------------------------------------
# Script copyright (C) 2008 Remigiusz Fiedler (AKA migius)
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


import Blender
from Blender import Mathutils, Window, Scene, Draw, Camera, BezTriple
from Blender import Registry, Object, Mesh, Curve
import os
import subprocess

try:
	import dxfLibrary as DXF
	#reload(DXF)
	#reload(dxfLibrary)
	#from dxfLibrary import *
except:
	DXF=None
	print "DXF-Exporter: error! found no dxfLibrary.py module in Blender script folder"
	Draw.PupMenu("Error%t|found no dxfLibrary.py module in script folder")
	

import math
from math import atan, atan2, log10, sin, cos

#pi = math.pi
#pi = 3.14159265359
r2d = 180.0 / math.pi
d2r = math.pi / 180.0
#note: d2r * angle == math.radians(angle)
#note: r2d * angle == math.degrees(angle)


#DEBUG = True #activates debug mode


#----globals------------------------------------------
ONLYSELECTED = 1 # 0/1 = False/True
ONLYVISIBLE = 1 # ignore objects on invisible layers
POLYLINES = 1 # prefer POLYLINEs not LINEs
POLYFACES = 1 # prefer POLYFACEs not 3DFACEs
PROJECTION = 0 # output geometry will be projected to XYplane with Z=0.0
HIDDEN_LINES = 0 #filter out hidden geometry
SHADOWS = 0 # sun/shadows simulation
CAMERA = 1 # selected camera index
PERSPECTIVE = 0 # projection (camera) type: perspective, opposite to orthographic
CAMERAVIEW = 0 # use camera for projection, opposite is 3d-view
INSTANCES = 1 # Export instances of Mesh/Curve as BLOCK/INSERTs   on/off
APPLY_MODIFIERS = 1
INCLUDE_DUPLIS = 0
OUTPUT_DWG = 0 #optional save to DWG with extern converter

G_SCALE = 1.0	  #(0.0001-1000) global scaling factor for output dxf data
G_ORIGIN = [0.0,0.0,0.0]   #global translation-vector (x,y,z) in Blender units
ELEVATION = 0.0 #standard elevation = coordinate Z value in Blender units

BYBLOCK = 0 #DXF-attribute: assign property to BLOCK defaults
BYLAYER = None #256 #DXF-attribute: assign property to LAYER defaults
PREFIX = 'BF_' #used as prefix for DXF names
LAYERNAME_DEF = '' #default layer name
LAYERCOLOR_DEF = 7 #default layer color index
LAYERLTYPE_DEF = 0 #'CONTINUOUS' - default layer lineType
ENTITYLAYER_DEF = LAYERNAME_DEF #default entity color index
ENTITYCOLOR_DEF = BYLAYER #default entity color index
ENTITYLTYPE_DEF = BYLAYER #default entity lineType

E_M = 0
LAB = "scroll MMB/WHEEL           . wip   .. todo" #"*) parts under construction"
M_OBJ = 0

FILENAME_MAX = 180  #max length of path+file_name string  (FILE_MAXDIR + FILE_MAXFILE)
NAMELENGTH_MAX = 80   #max_obnamelength in DXF, (limited to 256? )
INIFILE_DEFAULT_NAME = 'exportDXF'
INIFILE_EXTENSION = '.ini'
INIFILE_HEADER = '#ExportDXF.py ver.1.0 config data'
INFFILE_HEADER = '#ExportDXF.py ver.1.0 analyze of DXF-data'

BLOCKREGISTRY = {} # registry and map for BLOCKs
SCENE = None
WORLDX = Mathutils.Vector((1,0,0))
WORLDY = Mathutils.Vector((0,1,0))
WORLDZ = Mathutils.Vector((0,0,1))

AUTO = BezTriple.HandleTypes.AUTO
FREE = BezTriple.HandleTypes.FREE
VECT = BezTriple.HandleTypes.VECT
ALIGN = BezTriple.HandleTypes.ALIGN


#-------- DWG support ------------------------------------------
extCONV_OK = True
extCONV = 'DConvertCon.exe'
extCONV_PATH = os.path.join(Blender.Get('scriptsdir'),extCONV)
if not os.path.isfile(extCONV_PATH):
	extCONV_OK = False
	extCONV_TEXT = 'DWG-Exporter: Abort, nothing done!|\
Copy first %s into Blender script directory.|\
More details in online Help.' %extCONV
else:
	if not os.sys.platform.startswith('win'):
		# check if Wine installed:   
		if subprocess.Popen(('which', 'winepath'), stdout=subprocess.PIPE).stdout.read().strip():
			extCONV_PATH    = 'wine %s'%extCONV_PATH
		else: 
			extCONV_OK = False
			extCONV_TEXT = 'DWG-Exporter: Abort, nothing done!|\
The external DWG-converter (%s) needs Wine installed on your system.|\
More details in online Help.' %extCONV
#print 'extCONV_PATH = ', extCONV_PATH


#----------------------------------------------
def updateMenuCAMERA():
	global CAMERAS
	global MenuCAMERA
	global MenuLIGHT

	scn = Scene.GetCurrent()
	objs = scn.getChildren()
	currcam = scn.getCurrentCamera()
	if currcam: currcam = currcam.getName()
	maincams = []
	MenuCAMERA = "Select Camera%t"
	for cam in objs:
		if cam.getType() == 'Camera':
			if cam.getName()[0:4] != "Temp":
				maincams.append(cam.getName())
	maincams.sort()
	maincams.reverse() 
	CAMERAS = maincams
	for i, cam in enumerate(CAMERAS):
		if cam==currcam:
			MenuCAMERA += "|* " + cam
		else: MenuCAMERA += "| " + cam
	MenuCAMERA += "|current 3d-View"			
	MenuLIGHT = "Select Sun%t| *todo"			


#----------------------------------------------
def updateCAMERA():
	global CAMERA, GUI_A
	#CAMERA = 1
	scn = Scene.GetCurrent()
	currcam = scn.getCurrentCamera()
	if currcam: currcam = currcam.getName()
	if currcam in CAMERAS:
		CAMERA = CAMERAS.index(currcam)+1
	GUI_A['camera_selected'].val = CAMERA

#----------------------------------------------
def gotoCAMERA():
	cam =	Object.Get(CAMERAS[CAMERA-1])
	#print 'deb: CAMERA, cam',CAMERA, cam
	if cam.getType() != 'Camera':
		sure = Draw.PupMenu("Info: %t| It is not a Camera Object.")
	else:
		scn = Scene.getCurrent()   
		scn.setCurrentCamera(cam)
		Window.CameraView(0)
		Window.Redraw()
		updateMenuCAMERA()


#------- Duplicates support ----------------------------------------------
def dupTest(object):
	"""
	Checks objects for duplicates enabled (any type)
	object: Blender Object.
	Returns: Boolean - True if object has any kind of duplicates enabled.
	"""
	if (object.enableDupFrames or \
		object.enableDupGroup or \
		object.enableDupVerts):
		return True
	else:
		return False

def getObjectsAndDuplis(oblist,MATRICES=False,HACK=False):
	"""
	Return a list of real objects and duplicates and optionally their matrices
	oblist: List of Blender Objects
	MATRICES: Boolean - Check to also get the objects matrices, default=False
	HACK: Boolean - See note, default=False
	Returns: List of objects or
			 List of tuples of the form:(ob,matrix) if MATRICES is set to True
	NOTE: There is an ugly hack here that excludes all objects whose name
	starts with "dpl_" to exclude objects that are parented to a duplicating
	object, User must name objects properly if hack is used.
	"""

	result = []
	for ob in oblist:
		if INCLUDE_DUPLIS and dupTest(ob):
			dup_obs=ob.DupObjects
			if len(dup_obs):
				for dup_ob, dup_mx in dup_obs:
					if MATRICES:
						result.append((dup_ob,dup_mx))
					else:
						result.append(dup_ob)
		else:
			if HACK:
				if ob.getName()[0:4] != "dpl_":
					if MATRICES:
						mx = ob.mat
						result.append((ob,mx))
					else:
						result.append(ob)
			else:
				if MATRICES:
					mx = ob.mat
					result.append((ob,mx))
				else:
					result.append(ob)
	return result

#-----------------------------------------------------
def hidden_status(faces, mx, mx_n):
	# sort out back-faces = with normals pointed away from camera
	#print 'HIDDEN_LINES: caution! not full implemented yet'
	front_faces = []
	front_edges = []
	for f in faces:
		#print 'deb: face=', f #---------
		#print 'deb: dir(face)=', dir(f) #---------
		# get its normal-vector in localCS
		vec_normal = f.no.copy()
		#print 'deb: vec_normal=', vec_normal #------------------
		# must be transfered to camera/view-CS
		vec_normal *= mx_n
		#vec_normal *= mb.rotationPart()
		#print 'deb:2vec_normal=', vec_normal #------------------
		#vec_normal *= mw0.rotationPart()
		#print 'deb:3vec_normal=', vec_normal, '\n' #------------------

		
		frontFace = False
		if not PERSPECTIVE: #for ortho mode ----------
			# normal must point the Z direction-hemisphere
			if vec_normal[2] > 0.00001:
				frontFace = True
		else:
			v = f.verts[0]
			vert = Mathutils.Vector(v.co) * mx
			if Mathutils.DotVecs(vert, vec_normal) < 0.00001:
				frontFace = True

		if frontFace:
			front_faces.append(f.index)
			for key in f.edge_keys:
				#this test can be done faster with set()
				if key not in front_edges:
					 front_edges.append(key)

	#print 'deb: amount of visible faces=', len(front_faces) #---------
	#print 'deb: visible faces=', front_faces #---------
	#print 'deb: amount of visible edges=', len(front_edges) #---------
	#print 'deb: visible edges=', front_edges #---------
	return front_faces, front_edges


#---- migration to 2.49-------------------------------------------------
if 'cross' in dir(Mathutils.Vector()):
	#Draw.PupMenu('DXF exporter: Abort%t|This script version works for Blender up 2.49 only!')
	def	M_CrossVecs(v1,v2):
		return v1.cross(v2) #for up2.49
	def M_DotVecs(v1,v2):
		return v1.dot(v2) #for up2.49
else:
	def	M_CrossVecs(v1,v2):
		return Mathutils.CrossVecs(v1,v2) #for pre2.49
	def M_DotVecs(v1,v2):
		return Mathutils.DotVecs(v1,v2) #for pre2.49
	

#-----------------------------------------------------
def	getExtrusion(matrix):
	"""calculates DXF-Extrusion = Arbitrary Xaxis and Zaxis vectors
		
	"""
	AZaxis = matrix[2].copy().resize3D().normalize() # = ArbitraryZvector
	Extrusion = [AZaxis[0],AZaxis[1],AZaxis[2]]
	if AZaxis[2]==1.0:
		Extrusion = None
		AXaxis = matrix[0].copy().resize3D() # = ArbitraryXvector
	else:
		threshold = 1.0 / 64.0
		if abs(AZaxis[0]) < threshold and abs(AZaxis[1]) < threshold:
			# AXaxis is the intersection WorldPlane and ExtrusionPlane
			AXaxis = M_CrossVecs(WORLDY,AZaxis)
		else:
			AXaxis = M_CrossVecs(WORLDZ,AZaxis)
	#print 'deb:\n' #-------------
	#print 'deb:getExtrusion()  Extrusion=', Extrusion #---------
	return Extrusion, AXaxis.normalize()


#-----------------------------------------------------
def	getZRotation(AXaxis, rot_matrix_invert):
	"""calculates ZRotation = angle between ArbitraryXvector and obj.matrix.Xaxis
		
	"""
	# this works: Xaxis is the obj.matrix-Xaxis vector
	# but not correct for all orientations
	#Xaxis = matrix[0].copy().resize3D() # = ArbitraryXvector
	##Xaxis.normalize() # = ArbitraryXvector
	#ZRotation = - Mathutils.AngleBetweenVecs(Xaxis,AXaxis) #output in radians

	# this works for all orientations, maybe a bit faster
	# transform AXaxis into OCS:Object-Coord-System 
	#rot_matrix = normalizeMat(matrix.rotationPart())
	#rot_matrix_invert = rot_matrix.invert()
	vec = AXaxis * rot_matrix_invert
	##vec = AXaxis * matrix.copy().invert()
	##vec.normalize() # not needed for atan2()
	#print '\ndeb:getExtrusion()  vec=', vec #---------
	ZRotation = - atan2(vec[1],vec[0]) #output in radians

	#print 'deb:ZRotation()  ZRotation=', ZRotation*r2d #---------
	return ZRotation


#------------------------------------------
def normalizeMat(matrix):
	mat12 = matrix.copy()
	mat12 = [Mathutils.Vector(v).normalize() for v in mat12]
	if len(mat12)>3:
		matr12 = Mathutils.Matrix(mat12[0],mat12[1],mat12[2],mat12[3])
	else:
		matr12 = Mathutils.Matrix(mat12[0],mat12[1],mat12[2])
	return matr12


#-----------------------------------------------------
def projected_co(verts, matrix):
	""" converts coordinates of points from OCS to WCS->ScreenCS
	needs matrix: a projection matrix
	needs verts: a list of vectors[x,y,z]
	returns a list of [x,y,z]
	"""
	#print 'deb:projected_co()  verts=', verts #---------
	temp_verts = [Mathutils.Vector(v)*matrix for v in verts]
	#print 'deb:projected_co()  temp_verts=', temp_verts #---------

	if GUI_A['Z_force_on'].val: locZ = GUI_A['Z_elev'].val
	else: locZ = 0.0

	if PROJECTION:
		if PERSPECTIVE:
			clipStart = 10.0
			for v in temp_verts:
				coef = - clipStart / v[2]
				v[0] *= coef
				v[1] *= coef
				v[2] = locZ
		for v in temp_verts:
	 		v[2] = locZ
	temp_verts = [v[:3] for v in temp_verts]
	#print 'deb:projected_co()  out_verts=', temp_verts #---------
	return temp_verts


#-----------------------------------------------------
def isLeftHand(matrix):
	#Is the matrix a left-hand-system, or not?
	ma = matrix.rotationPart()
	crossXY = M_CrossVecs(ma[0], ma[1])
	check = M_DotVecs(ma[2], crossXY)
	if check < 0.00001: return 1
	return 0


#-----------------------------------------------------
def	exportMesh(ob, mx, mx_n, me=None, **common):
	"""converts Mesh-Object to desired projection and representation(DXF-Entity type)
	"""
	global BLOCKREGISTRY
	entities = []
	block = None
	#print 'deb:exportMesh() given common=', common #---------
	if me==None:
		me = ob.getData(mesh=1)
	else:
		me.getFromObject(ob)
	# idea: me.transform(mx); get verts data; me.transform(mx_inv)= back to the origin state
	# the .transform-method is fast, but bad, cause invasive:
	# it manipulates original geometry and by retransformation lefts back rounding-errors
	# we dont want to manipulate original data!
	#temp_verts = me.verts[:] #doesn't work on ubuntu(Yorik), bug?
	if me.verts:
		#print 'deb:exportMesh() started' #---------

		#print 'deb:exportMesh() ob.name=', ob.name #---------
		#print 'deb:exportMesh() me.name=', me.name #---------
		#print 'deb:exportMesh() me.users=', me.users #---------
		# check if there are more instances of this mesh (if used by other objects), then write to BLOCK/INSERT
		if GUI_A['instances_on'].val and me.users>1 and not PROJECTION:
			if me.name in BLOCKREGISTRY.keys():
				insert_name = BLOCKREGISTRY[me.name]
				# write INSERT to entities
				entities = exportInsert(ob, mx,insert_name, **common)
			else:
				# generate geom_output in ObjectCS
				allpoints = [v.co for v in me.verts]
				identity_matrix = Mathutils.Matrix().identity()
				allpoints = projected_co(allpoints, identity_matrix)
				#allpoints = toGlobalOrigin(allpoints)
				faces=[]
				edges=[]
				for e in me.edges: edges.append(e.key)
				faces = [[v.index for v in f.verts] for f in me.faces]
				entities = writeMeshEntities(allpoints, edges, faces, **common)
				if entities: # if not empty block
					# write BLOCK definition and INSERT entity
					# BLOCKREGISTRY = dictionary 'blender_name':'dxf_name'.append(me.name)
					BLOCKREGISTRY[me.name]=validDXFr12name(('ME_'+ me.name))
					insert_name = BLOCKREGISTRY[me.name]
					block = DXF.Block(insert_name,flag=0,base=(0,0,0),entities=entities)
					# write INSERT as entity
					entities = exportInsert(ob, mx, insert_name, **common)

		else: # no other instances, so go the standard way
			allpoints = [v.co for v in me.verts]
			allpoints = projected_co(allpoints, mx)
			allpoints = toGlobalOrigin(allpoints)
			faces=[]
			edges=[]
			if me.faces and PROJECTION and HIDDEN_LINES:
				#if DEBUG: print 'deb:exportMesh HIDDEN_LINES mode' #---------
				faces, edges = hidden_status(me.faces, mx, mx_n)
				faces = [[v.index for v in me.faces[f_nr].verts] for f_nr in faces]
			else:
				#if DEBUG: print 'deb:exportMesh STANDARD mode' #---------
				for e in me.edges: edges.append(e.key)
				#faces = [f.index for f in me.faces]
				faces = [[v.index for v in f.verts] for f in me.faces]
				#faces = [[allpoints[v.index] for v in f.verts] for f in me.faces]
			#print 'deb: allpoints=\n', allpoints #---------
			#print 'deb: edges=\n', edges #---------
			#print 'deb: faces=\n', faces #---------
			if isLeftHand(mx): # then change vertex-order in every face
				for f in faces:
					f.reverse()
					#f = [f[-1]] + f[:-1] #TODO: might be needed
				#print 'deb: faces=\n', faces #---------
			entities = writeMeshEntities(allpoints, edges, faces, **common)

	return entities, block
	

#-------------------------------------------------
def writeMeshEntities(allpoints, edges, faces, **common):
	"""help routine for exportMesh()
	"""
	entities = []
	
	c = mesh_as_list[GUI_A['mesh_as'].val]
	if 'POINTs'==c: # export Mesh as multiple POINTs
		for p in allpoints:
			dxfPOINT = DXF.Point(points=[p],**common)
			entities.append(dxfPOINT)
	elif 'LINEs'==c or (not faces):
		if edges and allpoints:
			if DEBUG: mesh_drawBlender(allpoints, edges, None) #deb: draw to blender scene
			for e in edges:
				points = [allpoints[e[0]], allpoints[e[1]]]
				dxfLINE = DXF.Line(points, **common)
				entities.append(dxfLINE)
	elif faces:
		if c in ('POLYFACE','POLYLINE'):
			if allpoints:
				#TODO: purge allpoints: left only vertices used by faces
				if DEBUG: mesh_drawBlender(allpoints, None, faces) #deb: draw to scene
				if not (PROJECTION and HIDDEN_LINES):
					faces = [[v+1 for v in f] for f in faces]
				else:
					# for back-Faces-mode remove face-free verts
					map=verts_state= [0]*len(allpoints)
					for f in faces:
						for v in f:
							verts_state[v]=1
					if 0 in verts_state: # if dirty state
						i,newverts=0,[]
						for used_i,used in enumerate(verts_state):
							if used:
								newverts.append(allpoints[used_i])	
								map[used_i]=i
								i+=1
						allpoints = newverts
						faces = [[map[v]+1 for v in f] for f in faces]
				dxfPOLYFACE = DXF.PolyLine([allpoints, faces], flag=64, **common)
				#print '\n deb: dxfPOLYFACE=',dxfPOLYFACE #-------------
				entities.append(dxfPOLYFACE)
		elif '3DFACEs'==c:
			if DEBUG: mesh_drawBlender(allpoints, None, faces) #deb: draw to scene
			for f in faces:
				#print 'deb: face=', f #---------
				points = [allpoints[key] for key in f]
				#points = [p.co[:3] for p in points]
				#print 'deb: pointsXX=\n', points #---------
				dxfFACE = DXF.Face(points, **common)
				entities.append(dxfFACE)
				
	return entities


#-----------------------------------------------------
def mesh_drawBlender(vertList, edgeList, faceList, name="dxfMesh", flatten=False, AT_CUR=True, link=True):
	#print 'deb:mesh_drawBlender started XXXXXXXXXXXXXXXXXX' #---------
	ob = Object.New("Mesh",name)
	me = Mesh.New(name)
	#print 'deb: vertList=\n', vertList #---------
	#print 'deb: edgeList=\n', edgeList #---------
	#print 'deb: faceList=\n', faceList #---------
	me.verts.extend(vertList)
	if edgeList: me.edges.extend(edgeList)
	if faceList: me.faces.extend(faceList)
	if flatten:
		for v in me.verts: v.co.z = 0.0
	ob.link(me)
	if link:
		sce = Scene.getCurrent()
		sce.objects.link(ob)
		#me.triangleToQuad()
		if AT_CUR:
			cur_loc = Window.GetCursorPos()
			ob.setLocation(cur_loc)
		Blender.Redraw()
	#return ob

#-----------------------------------------------------
def curve_drawBlender(vertList, org_point=[0.0,0.0,0.0], closed=0, name="dxfCurve", flatten=False, AT_CUR=True, link=True):
	#print 'deb:curve_drawBlender started XXXXXXXXXXXXXXXXXX' #---------
	ob = Object.New("Curve",name)
	cu = Curve.New(name)
	#print 'deb: vertList=\n', vertList #---------
	curve = cu.appendNurb(BezTriple.New(vertList[0]))
	for p in vertList[1:]:
		curve.append(BezTriple.New(p))
	for point in curve:
		#point.handleTypes = [VECT, VECT]
		point.handleTypes = [FREE, FREE]
		point.radius = 1.0
	curve.flagU = closed # 0 sets the curve not cyclic=open
	cu.setResolu(6)
	cu.update() #important for handles calculation
	if flatten:
		for v in cu.verts: v.co.z = 0.0
	ob.link(cu)
	if link:
		sce = Scene.getCurrent()
		sce.objects.link(ob)
		#me.triangleToQuad()
		if AT_CUR:
			cur_loc = Window.GetCursorPos()
			ob.setLocation(cur_loc)
		elif org_point:
			cur_loc=org_point
			ob.setLocation(cur_loc)
		Blender.Redraw()
	#return ob


#-----------------------------------------------------
def toGlobalOrigin(points):
	"""relocates points to the new location
	needs a list of points [x,y,z]
	"""
	if GUI_A['g_origin_on'].val:
		for p in points:
			p[0] += G_ORIGIN[0]
			p[1] += G_ORIGIN[1]
			p[2] += G_ORIGIN[2]
	return points


#-----------------------------------------------------
def exportEmpty(ob, mx, mw, **common):
	"""converts Empty-Object to desired projection and representation(DXF-Entity type)
	"""
	p =  Mathutils.Vector(ob.loc)
	[p] = projected_co([p], mx)
	[p] = toGlobalOrigin([p])

	entities = []
	c = empty_as_list[GUI_A['empty_as'].val]
	if c=="POINT": # export Empty as POINT
		dxfPOINT = DXF.Point(points=[p],**common)
		entities.append(dxfPOINT)
	return entities

#-----------------------------------------------------
def exportCamera(ob, mx, mw, **common):
	"""converts Camera-Object to desired projection and representation(DXF-Entity type)
	"""
	location =  Mathutils.Vector(ob.loc)
	[location] = projected_co([location], mx)
	[location] = toGlobalOrigin([location])
	view_name=validDXFr12name(('CAM_'+ ob.name))

	camera = Camera.Get(ob.getData(name_only=True))
	#print 'deb: camera=', dir(camera) #------------------
	if camera.type=='persp':
		mode = 1+2+4+16
		# mode flags: 1=persp, 2=frontclip, 4=backclip,16=FrontZ
	elif camera.type=='ortho':
		mode = 0+2+4+16

	leftBottom=(0.0,0.0) # default
	rightTop=(1.0,1.0) # default
	center=(0.0,0.0) # default

	direction = Mathutils.Vector(0.0,0.0,1.0) * mx.rotationPart() # in W-C-S
	direction.normalize()
	target=Mathutils.Vector(ob.loc) - direction # in W-C-S
	#ratio=1.0
	width=height= camera.scale # for ortho-camera
	lens = camera.lens # for persp-camera
	frontClipping = -(camera.clipStart - 1.0)
	backClipping = -(camera.clipEnd - 1.0)

	entities, vport, view = [], None, None
	c = camera_as_list[GUI_A['camera_as'].val]
	if c=="POINT": # export as POINT
		dxfPOINT = DXF.Point(points=[location],**common)
		entities.append(dxfPOINT)
	elif c=="VIEW": # export as VIEW
		view = DXF.View(name=view_name,
			center=center, width=width, height=height,
			frontClipping=frontClipping,backClipping=backClipping,
			direction=direction,target=target,lens=lens,mode=mode
			)
	elif c=="VPORT": # export as VPORT
		vport = DXF.VPort(name=view_name,
			center=center, ratio=1.0, height=height,
			frontClipping=frontClipping,backClipping=backClipping,
			direction=direction,target=target,lens=lens,mode=mode
			)
	return entities, vport, view

#-----------------------------------------------------
def exportLamp(ob, mx, mw, **common):
	"""converts Lamp-Object to desired projection and representation(DXF-Entity type)
	"""
	p =  Mathutils.Vector(ob.loc)
	[p] = projected_co([p], mx)
	[p] = toGlobalOrigin([p])

	entities = []
	c = lamp_as_list[GUI_A['lamp_as'].val]
	if c=="POINT": # export as POINT
		dxfPOINT = DXF.Point(points=[p],**common)
		entities.append(dxfPOINT)
	return entities

#-----------------------------------------------------
def exportInsert(ob, mx, insert_name, **common):
	"""converts Object to DXF-INSERT in given orientation
	"""
	WCS_loc = ob.loc # WCS_loc is object location in WorldCoordSystem
	sizeX = ob.SizeX
	sizeY = ob.SizeY
	sizeZ = ob.SizeZ
	rotX  = ob.RotX
	rotY  = ob.RotY
	rotZ  = ob.RotZ
	#print 'deb: sizeX=%s, sizeY=%s' %(sizeX, sizeY) #---------

	Thickness,Extrusion,ZRotation,Elevation = None,None,None,None

	AXaxis = mx[0].copy().resize3D() # = ArbitraryXvector
	if not PROJECTION:
		#Extrusion, ZRotation, Elevation = getExtrusion(mx)
		Extrusion, AXaxis = getExtrusion(mx)

	entities = []

	if 1:
		if not PROJECTION:
			ZRotation,Zrotmatrix,OCS_origin,ECS_origin = getTargetOrientation(mx,Extrusion,\
				AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ)
			ZRotation *= r2d
			point = ECS_origin
		else:	#TODO: fails correct location
			point1 = Mathutils.Vector(ob.loc)
			[point] = projected_co([point1], mx)
			if PERSPECTIVE:
				clipStart = 10.0
				coef = -clipStart / (point1*mx)[2]
				#print 'deb: coef=', coef #--------------
				#TODO: ? sizeX *= coef
				#sizeY *= coef
				#sizeZ *= coef
	
		#print 'deb: point=', point #--------------
		[point] = toGlobalOrigin([point])

		#if DEBUG: text_drawBlender(textstr,points,OCS_origin) #deb: draw to scene
		common['extrusion']= Extrusion
		#common['elevation']= Elevation
		#print 'deb: common=', common #------------------
		if 0: #DEBUG
			#linepoints = [[0,0,0], [AXaxis[0],AXaxis[1],AXaxis[2]]]
			linepoints = [[0,0,0], point]
			dxfLINE = DXF.Line(linepoints,**common)
			entities.append(dxfLINE)

		xscale=sizeX
		yscale=sizeY
		zscale=sizeZ
		cols=None
		colspacing=None
		rows=None
		rowspacing=None

		dxfINSERT = DXF.Insert(insert_name,point=point,rotation=ZRotation,\
			xscale=xscale,yscale=yscale,zscale=zscale,\
			cols=cols,colspacing=colspacing,rows=rows,rowspacing=rowspacing,\
			**common)
		entities.append(dxfINSERT)

	return entities


#-----------------------------------------------------
def exportText(ob, mx, mw, **common):
	"""converts Text-Object to desired projection and representation(DXF-Entity type)
	"""
	text3d = ob.getData()
	textstr = text3d.getText()
	WCS_loc = ob.loc # WCS_loc is object location in WorldCoordSystem
	sizeX = ob.SizeX
	sizeY = ob.SizeY
	sizeZ = ob.SizeZ
	rotX  = ob.RotX
	rotY  = ob.RotY
	rotZ  = ob.RotZ
	#print 'deb: sizeX=%s, sizeY=%s' %(sizeX, sizeY) #---------

	Thickness,Extrusion,ZRotation,Elevation = None,None,None,None

	AXaxis = mx[0].copy().resize3D() # = ArbitraryXvector
	if not PROJECTION:
		#Extrusion, ZRotation, Elevation = getExtrusion(mx)
		Extrusion, AXaxis = getExtrusion(mx)

		# no thickness/width for TEXTs converted into ScreenCS
		if text3d.getExtrudeDepth():
			Thickness = text3d.getExtrudeDepth() * sizeZ

	#Horizontal text justification type, code 72, (optional, default = 0)
	# integer codes (not bit-coded)
	#0=left, 1=center, 2=right
	#3=aligned, 4=middle, 5=fit
	Alignment = None
	alignment = text3d.getAlignment().value
	if alignment in (1,2): Alignment = alignment

	textHeight = text3d.getSize() / 1.7
	textFlag = 0
	if sizeX < 0.0: textFlag |= 2 # set flag for horizontal mirrored
	if sizeZ < 0.0: textFlag |= 4 # vertical mirrored

	entities = []
	c = text_as_list[GUI_A['text_as'].val]

	if c=="TEXT": # export text as TEXT
		if not PROJECTION:
			ZRotation,Zrotmatrix,OCS_origin,ECS_origin = getTargetOrientation(mx,Extrusion,\
				AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ)
			ZRotation *= r2d
			point = ECS_origin
		else:	#TODO: fails correct location
			point1 = Mathutils.Vector(ob.loc)
			[point] = projected_co([point1], mx)
			if PERSPECTIVE:
				clipStart = 10.0
				coef = -clipStart / (point1*mx)[2]
				textHeight *= coef
				#print 'deb: coef=', coef #--------------
	
		#print 'deb: point=', point #--------------
		[point] = toGlobalOrigin([point])
		point2 = point

		#if DEBUG: text_drawBlender(textstr,points,OCS_origin) #deb: draw to scene
		common['extrusion']= Extrusion
		#common['elevation']= Elevation
		common['thickness']= Thickness
		#print 'deb: common=', common #------------------
		if 0: #DEBUG
			#linepoints = [[0,0,0], [AXaxis[0],AXaxis[1],AXaxis[2]]]
			linepoints = [[0,0,0], point]
			dxfLINE = DXF.Line(linepoints,**common)
			entities.append(dxfLINE)

		dxfTEXT = DXF.Text(text=textstr,point=point,alignment=point2,rotation=ZRotation,\
			flag=textFlag,height=textHeight,justifyhor=Alignment,**common)
		entities.append(dxfTEXT)
		if Thickness:
			common['thickness']= -Thickness
			dxfTEXT = DXF.Text(text=textstr,point=point,alignment=point2,rotation=ZRotation,\
				flag=textFlag,height=textHeight,justifyhor=Alignment,**common)
			entities.append(dxfTEXT)
	return entities


#-------------------------------------------
def euler2matrix(rx, ry, rz):
	"""creates full 3D rotation matrix (optimized)
	needs rx, ry, rz angles in radians
	"""
	#print 'rx, ry, rz: ', rx, ry, rz
	A, B = sin(rx), cos(rx)
	C, D = sin(ry), cos(ry)
	E, F = sin(rz), cos(rz)
	AC, BC = A*C, B*C
	return Mathutils.Matrix([D*F,  D*E, -C],
		[AC*F-B*E, AC*E+B*F, A*D],
		[BC*F+A*E, BC*E-A*F, B*D])
		
	
#-----------------------------------------------------
def getTargetOrientation(mx,Extrusion,AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ):
	"""given 
	"""
	if 1:
		rot_matrix = normalizeMat(mx.rotationPart())
		#TODO: workaround for blender negative-matrix.invert()
		# partially done: works only for rotX,rotY==0.0
		if sizeX<0.0: rot_matrix[0] *= -1
		if sizeY<0.0: rot_matrix[1] *= -1
		#if sizeZ<0.0: rot_matrix[2] *= -1
		rot_matrix_invert = rot_matrix.invert()
	else: #TODO: to check, why below rot_matrix_invert is not equal above one
		rot_euler_matrix = euler2matrix(rotX,rotY,rotZ)
		rot_matrix_invert = euler2matrix(-rotX,-rotY,-rotZ)
	
	# OCS_origin is Global_Origin in ObjectCoordSystem
	OCS_origin = Mathutils.Vector(WCS_loc) * rot_matrix_invert
	#print 'deb: OCS_origin=', OCS_origin #---------

	ZRotation = rotZ
	if Extrusion!=None:
		ZRotation = getZRotation(AXaxis,rot_matrix_invert)
	#Zrotmatrix = Mathutils.RotationMatrix(-ZRotation, 3, "Z")
	rs, rc = sin(ZRotation), cos(ZRotation)
	Zrotmatrix = Mathutils.Matrix([rc, rs,0.0],[-rs,rc,0.0],[0.0,0.0,1.0])
	#print 'deb: Zrotmatrix=\n', Zrotmatrix #--------------

	# ECS_origin is Global_Origin in EntityCoordSystem
	ECS_origin = OCS_origin * Zrotmatrix
	#print 'deb: ECS_origin=', ECS_origin #---------
	#TODO: it doesnt work yet for negative scaled curve-objects!
	return ZRotation,Zrotmatrix,OCS_origin,ECS_origin


#-----------------------------------------------------
def exportCurve(ob, mx, mw, **common):
	"""converts Curve-Object to desired projection and representation(DXF-Entity type)
	"""
	entities = []
	block = None
	curve = ob.getData()
	#print 'deb: curve=', dir(curve) #---------
	# TODO: should be: if curve.users>1 and not (PERSPECTIVE or (PROJECTION and HIDDEN_MODE):
	if GUI_A['instances_on'].val and curve.users>1 and not PROJECTION:
		if curve.name in BLOCKREGISTRY.keys():
			insert_name = BLOCKREGISTRY[curve.name]
			# write INSERT to entities
			entities = exportInsert(ob, mx,insert_name, **common)
		else:
			# generate geom_output in ObjectCS
			imx = Mathutils.Matrix().identity()
			WCS_loc = [0,0,0] # WCS_loc is object location in WorldCoordSystem
			#print 'deb: WCS_loc=', WCS_loc #---------
			sizeX = sizeY = sizeZ = 1.0
			rotX  = rotY  = rotZ = 0.0
			Thickness,Extrusion,ZRotation,Elevation = None,None,None,None
			ZRotation,Zrotmatrix,OCS_origin,ECS_origin = None,None,None,None
			AXaxis = imx[0].copy().resize3D() # = ArbitraryXvector
			OCS_origin = [0,0,0]
			if not PROJECTION:
				#Extrusion, ZRotation, Elevation = getExtrusion(mx)
				Extrusion, AXaxis = getExtrusion(imx)
		
				# no thickness/width for POLYLINEs converted into Screen-C-S
				#print 'deb: curve.ext1=', curve.ext1 #---------
				if curve.ext1: Thickness = curve.ext1 * sizeZ
				if curve.ext2 and sizeX==sizeY:
					Width = curve.ext2 * sizeX
				if "POLYLINE"==curve_as_list[GUI_A['curve_as'].val]: # export as POLYLINE
					ZRotation,Zrotmatrix,OCS_origin,ECS_origin = getTargetOrientation(imx,Extrusion,\
						AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ)

			entities = writeCurveEntities(curve, imx,
				Thickness,Extrusion,ZRotation,Elevation,AXaxis,Zrotmatrix,
				WCS_loc,OCS_origin,ECS_origin,sizeX,sizeY,sizeZ,
				**common)

			if entities: # if not empty block
				# write BLOCK definition and INSERT entity
				# BLOCKREGISTRY = dictionary 'blender_name':'dxf_name'.append(me.name)
				BLOCKREGISTRY[curve.name]=validDXFr12name(('CU_'+ curve.name))
				insert_name = BLOCKREGISTRY[curve.name]
				block = DXF.Block(insert_name,flag=0,base=(0,0,0),entities=entities)
				# write INSERT as entity
				entities = exportInsert(ob, mx, insert_name, **common)

	else: # no other instances, so go the standard way
		WCS_loc = ob.loc # WCS_loc is object location in WorldCoordSystem
		#print 'deb: WCS_loc=', WCS_loc #---------
		sizeX = ob.SizeX
		sizeY = ob.SizeY
		sizeZ = ob.SizeZ
		rotX  = ob.RotX
		rotY  = ob.RotY
		rotZ  = ob.RotZ
		#print 'deb: sizeX=%s, sizeY=%s' %(sizeX, sizeY) #---------
	
		Thickness,Extrusion,ZRotation,Elevation = None,None,None,None
		ZRotation,Zrotmatrix,OCS_origin,ECS_origin = None,None,None,None
		AXaxis = mx[0].copy().resize3D() # = ArbitraryXvector
		OCS_origin = [0,0,0]
		if not PROJECTION:
			#Extrusion, ZRotation, Elevation = getExtrusion(mx)
			Extrusion, AXaxis = getExtrusion(mx)
	
			# no thickness/width for POLYLINEs converted into Screen-C-S
			#print 'deb: curve.ext1=', curve.ext1 #---------
			if curve.ext1: Thickness = curve.ext1 * sizeZ
			if curve.ext2 and sizeX==sizeY:
				Width = curve.ext2 * sizeX
			if "POLYLINE"==curve_as_list[GUI_A['curve_as'].val]: # export as POLYLINE
				ZRotation,Zrotmatrix,OCS_origin,ECS_origin = getTargetOrientation(mx,Extrusion,\
					AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ)
		entities = writeCurveEntities(curve, mx,
				Thickness,Extrusion,ZRotation,Elevation,AXaxis,Zrotmatrix,
				WCS_loc,OCS_origin,ECS_origin,sizeX,sizeY,sizeZ,
				**common)

	return entities, block


#-------------------------------------------------
def writeCurveEntities(curve, mx,
		Thickness,Extrusion,ZRotation,Elevation,AXaxis,Zrotmatrix,
		WCS_loc,OCS_origin,ECS_origin,sizeX,sizeY,sizeZ,
		**common):
	"""help routine for exportCurve()
	"""
	entities = []
	
	if 1:
		for cur in curve:
			#print 'deb: START cur=', cur #--------------
			points = []
			if cur.isNurb():
				for point in cur:
					#print 'deb:isNurb point=', point #---------
					vec = point[0:3]
					#print 'deb: vec=', vec #---------
					pkt = Mathutils.Vector(vec)
					#print 'deb: pkt=', pkt #---------
					points.append(pkt)
			else:
				for point in cur:
					#print 'deb:isBezier point=', point.getTriple() #---------
					vec = point.getTriple()[1]
					#print 'deb: vec=', vec #---------
					pkt = Mathutils.Vector(vec)
					#print 'deb: pkt=', pkt #---------
					points.append(pkt)
	
			#print 'deb: points', points #--------------
			if len(points)>1:
				c = curve_as_list[GUI_A['curve_as'].val]

				if c=="POLYLINE": # export Curve as POLYLINE
					if not PROJECTION:
						# recalculate points(2d=X,Y) into Entity-Coords-System
						for p in points: # list of vectors
							p[0] *= sizeX
							p[1] *= sizeY
							p2 = p * Zrotmatrix
							p2[0] += ECS_origin[0]
							p2[1] += ECS_origin[1]
							p[0],p[1] = p2[0],p2[1]
					else:
						points = projected_co(points, mx)
					#print 'deb: points', points #--------------
	
					if cur.isCyclic(): closed = 1
					else: closed = 0
					points = toGlobalOrigin(points)
	
					if DEBUG: curve_drawBlender(points,OCS_origin,closed) #deb: draw to scene

					common['extrusion']= Extrusion
					##common['rotation']= ZRotation
					##common['elevation']= Elevation
					common['thickness']= Thickness
					#print 'deb: common=', common #------------------
	
					if 0: #DEBUG
						p=AXaxis[:3]
						entities.append(DXF.Line([[0,0,0], p],**common))
						p=ECS_origin[:3]
						entities.append(DXF.Line([[0,0,0], p],**common))
						common['color']= 5
						p=OCS_origin[:3]
						entities.append(DXF.Line([[0,0,0], p],**common))
						#OCS_origin=[0,0,0] #only debug----------------
						dxfPLINE = DXF.PolyLine(points,OCS_origin,closed,**common)
						entities.append(dxfPLINE)
	
					dxfPLINE = DXF.PolyLine(points,OCS_origin,closed,**common)
					entities.append(dxfPLINE)
					if Thickness:
						common['thickness']= -Thickness
						dxfPLINE = DXF.PolyLine(points,OCS_origin,closed,**common)
						entities.append(dxfPLINE)
	
				elif c=="LINEs": # export Curve as multiple LINEs
					points = projected_co(points, mx)
					if cur.isCyclic(): points.append(points[0])
					#print 'deb: points', points #--------------
					points = toGlobalOrigin(points)
	
					if DEBUG: curve_drawBlender(points,WCS_loc,closed) #deb: draw to scene
					common['extrusion']= Extrusion
					common['elevation']= Elevation
					common['thickness']= Thickness
					#print 'deb: common=', common #------------------
					for i in range(len(points)-1):
						linepoints = [points[i], points[i+1]]
						dxfLINE = DXF.Line(linepoints,**common)
						entities.append(dxfLINE)
					if Thickness:
						common['thickness']= -Thickness
						for i in range(len(points)-1):
							linepoints = [points[i], points[i+1]]
							dxfLINE = DXF.Line(linepoints,**common)
							entities.append(dxfLINE)
	
				elif c=="POINTs": # export Curve as multiple POINTs
					points = projected_co(points, mx)
					for p in points:
						dxfPOINT = DXF.Point(points=[p],**common)
						entities.append(dxfPOINT)
	return entities


#-----------------------------------------------------
def getClipBox(camera):
	"""calculates Field-of-View-Clipping-Box of given Camera
	returns clip_box: a list of vertices
	returns matr: translation matrix
	"""
	sce = Scene.GetCurrent()
	context = sce.getRenderingContext()
	#print 'deb: context=\n', context #------------------
	sizeX = context.sizeX
	sizeY = context.sizeY
	ratioXY = sizeX/float(sizeY)
	#print 'deb: size X,Y, ratio=', sizeX, sizeY, ratioXY #------------------

	clip1_Z = - camera.clipStart
	clip2_Z = - camera.clipEnd
	#print 'deb: clip Start=', camera.clipStart #------------------
	#print 'deb: clip   End=', camera.clipEnd #------------------

	if camera.type=='ortho':
		scale = camera.scale
		#print 'deb: camscale=', scale #------------------
		clip1shiftX = clip2shiftX = camera.shiftX * scale
		clip1shiftY = clip2shiftY = camera.shiftY * scale
		clip1_X = scale * 0.5
		clip1_Y = scale * 0.5
		if ratioXY > 1.0: clip1_Y /= ratioXY
		else: clip1_X *= ratioXY
		clip2_X = clip1_X
		clip2_Y = clip1_Y

		near = clip1_Z
		far = clip2_Z
		right, left = clip1_X, -clip1_X
		top, bottom = clip1_Y, -clip1_Y

		scaleX = 2.0/float(right - left)
		x3 = -float(right + left)/float(right - left)
		scaleY = 2.0/float(top - bottom)
		y3 = -float(top + bottom)/float(top - bottom)
		scaleZ = 1.0/float(far - near)
		z3 =  -float(near)/float(far - near)
		
		matrix = Mathutils.Matrix(  [scaleX, 0.0, 0.0, x3],
						[0.0, scaleY, 0.0, y3],
						[0.0, 0.0, scaleZ, z3],
						[0.0, 0.0, 0.0, 1.0])

	elif camera.type=='persp': 
		#viewpoint = [0.0, 0.0, 0.0] #camera's coordinate system, hehe
		#lens = camera.lens
		angle = camera.angle
		#print 'deb: cam angle=', angle #------------------
		shiftX = camera.shiftX
		shiftY = camera.shiftY
		fov_coef = atan(angle * d2r)
		fov_coef *= 1.3  #incl. passpartou
		clip1_k = clip1_Z * fov_coef
		clip2_k = clip2_Z * fov_coef
		clip1shiftX = - camera.shiftX * clip1_k
		clip2shiftX = - camera.shiftX * clip2_k
		clip1shiftY = - camera.shiftY * clip1_k
		clip2shiftY = - camera.shiftY * clip2_k
		clip1_X = clip1_Y = clip1_k * 0.5
		clip2_X = clip2_Y = clip2_k * 0.5
		if ratioXY > 1.0:
			clip1_Y /= ratioXY
			clip2_Y /= ratioXY
		else:
			clip1_X *= ratioXY
			clip2_X *= ratioXY

		near = clip1_Z
		far = clip2_Z
		right, left = clip1_X, -clip1_X
		top, bottom = clip1_Y, -clip1_Y
		#return Matrix(  [scaleX,   0.0,	x2,	 0.0],
						#[0.0,	  scaleY, y2,	 0.0],
						#[0.0,	  0.0,	scaleZ, wZ],
						#[0.0,	  0.0,	-1.0,   0.0])
		matrix = Mathutils.Matrix(  [(2.0 * near)/float(right - left), 0.0, float(right + left)/float(right - left), 0.0],
						[0.0, (2.0 * near)/float(top - bottom), float(top + bottom)/float(top - bottom), 0.0],
						[0.0, 0.0, -float(far + near)/float(far - near), -(2.0 * far * near)/float(far - near)],
						[0.0, 0.0, -1.0, 0.0])


	clip_box = [
		-clip1_X + clip1shiftX, clip1_X + clip1shiftX,
		-clip1_Y + clip1shiftY, clip1_Y + clip1shiftY,
		-clip2_X + clip2shiftX, clip2_X + clip2shiftX,
		-clip2_Y + clip2shiftY, clip2_Y + clip2shiftY,
		clip1_Z, clip2_Z]
	#print 'deb: clip_box=\n', clip_box #------------------
	#drawClipBox(clip_box)
	return clip_box, matrix


#-----------------------------------------------------
def drawClipBox(clip_box):
	"""debug tool: draws Clipping-Box of a Camera View
	"""
	min_X1, max_X1, min_Y1, max_Y1,\
	min_X2, max_X2, min_Y2, max_Y2,\
		min_Z, max_Z = clip_box
	verts = []
	verts.append([min_X1, min_Y1, min_Z])
	verts.append([max_X1, min_Y1, min_Z])
	verts.append([max_X1, max_Y1, min_Z])
	verts.append([min_X1, max_Y1, min_Z])
	verts.append([min_X2, min_Y2, max_Z])
	verts.append([max_X2, min_Y2, max_Z])
	verts.append([max_X2, max_Y2, max_Z])
	verts.append([min_X2, max_Y2, max_Z])
	faces = [[0,1,2,3],[4,5,6,7]]
	newmesh = Mesh.New()
	newmesh.verts.extend(verts)
	newmesh.faces.extend(faces)
	
	plan = Object.New('Mesh','clip_box')
	plan.link(newmesh)
	sce = Scene.GetCurrent()
	sce.objects.link(plan)
	plan.setMatrix(sce.objects.camera.matrix)


#-------------------------------------------------
def getCommons(ob):
	"""set up common attributes for output style:
	 color=None
	 extrusion=None
	 layer='0',
	 lineType=None
	 lineTypeScale=None
	 lineWeight=None
	 thickness=None
	 parent=None
	"""

	layers = ob.layers #gives a list e.g.[1,5,19]
	if layers: ob_layer_nr = layers[0]
	#print 'ob_layer_nr=', ob_layer_nr #--------------

	materials = ob.getMaterials()
	if materials:
		ob_material = materials[0]
		ob_mat_color = ob_material.rgbCol
	else: ob_mat_color, ob_material = None, None
	#print 'ob_mat_color, ob_material=', ob_mat_color, ob_material #--------------

	data = ob.getData()
	data_materials = ob.getMaterials()
	if data_materials:
		data_material = data_materials[0]
		data_mat_color = data_material.rgbCol
	else: data_mat_color, data_material = None, None
	#print 'data_mat_color, data_material=', data_mat_color, data_material #--------------
	
	entitylayer = ENTITYLAYER_DEF
	c = entitylayer_from_list[GUI_A['entitylayer_from'].val]
	#["default_LAYER","obj.name","obj.layer","obj.material","obj.data.name","obj.data.material","..vertexgroup","..group","..map_table"]
	if c=="default_LAYER":
		entitylayer = LAYERNAME_DEF
	elif c=="obj.layer" and ob_layer_nr:
		entitylayer = 'LAYER'+ str(ob_layer_nr)
	elif c=="obj.material" and ob_material:
		entitylayer = ob_material.name
	elif c=="obj.name":
		entitylayer = ob.name
	elif c=="obj.data.material" and ob_material:
		entitylayer = data_material.name
	elif c=="obj.data.name":
		entitylayer = data.name
	entitylayer = validDXFr12name(PREFIX+entitylayer)
	if entitylayer=="": entitylayer = "BF_0"

	entitycolor = ENTITYCOLOR_DEF
	c = entitycolor_from_list[GUI_A['entitycolor_from'].val]
	if c=="default_COLOR":
		entitycolor = LAYERCOLOR_DEF
	elif c=="BYLAYER":
		entitycolor = BYLAYER
	elif c=="BYBLOCK":
		entitycolor = BYBLOCK
	elif c=="obj.layer" and ob_layer_nr:
		entitycolor = ob_layer_nr
	elif c=="obj.color" and ob.color:
		entitycolor = col2DXF(ob.color)
	elif c=="obj.material" and ob_mat_color:
		 entitycolor = col2DXF(ob_mat_color)
	elif c=="obj.data.material" and data_mat_color:
		 entitycolor = col2DXF(data_mat_color)
	#if entitycolor!=None:	layercolor = entitycolor

	entityltype = ENTITYLTYPE_DEF
	c = entityltype_from_list[GUI_A['entityltype_from'].val]
	if c=="default_LTYPE":
		entityltype = LAYERLTYPE_DEF
	elif c=="BYLAYER":
		entityltype = BYLAYER
	elif c=="BYBLOCK":
		entityltype = BYBLOCK
	elif c:
		entityltype = c

	return entitylayer,entitycolor,entityltype


#-----------------------------------------------------
def do_export(export_list, filepath):
	global PERSPECTIVE, CAMERAVIEW, BLOCKREGISTRY
	Window.WaitCursor(1)
	t = Blender.sys.time()

	# init Drawing ---------------------
	d=DXF.Drawing()
	# add Tables -----------------
	# initialized automatic: d.blocks.append(b)				 #section BLOCKS
	# initialized automatic: d.styles.append(DXF.Style())			#table STYLE

	#table LTYPE ---------------
	#d.linetypes.append(DXF.LineType(name='CONTINUOUS',description='--------',elements=[0.0]))
	d.linetypes.append(DXF.LineType(name='DOT',description='. . . . . . .',elements=[0.25, 0.0, -0.25]))
	d.linetypes.append(DXF.LineType(name='DASHED',description='__ __ __ __ __',elements=[0.8, 0.5, -0.3]))
	d.linetypes.append(DXF.LineType(name='DASHDOT',description='__ . __ . __ .',elements=[1.0, 0.5, -0.25, 0.0, -0.25]))
	d.linetypes.append(DXF.LineType(name='DIVIDE',description='____ . . ____ . . ',elements=[1.25, 0.5, -0.25, 0.0, -0.25, 0.0, -0.25]))
	d.linetypes.append(DXF.LineType(name='BORDER',description='__ __ . __ __ . ',elements=[1.75, 0.5, -0.25, 0.5, -0.25, 0.0, -0.25]))
	d.linetypes.append(DXF.LineType(name='HIDDEN',description='__ __ __ __ __',elements=[0.4, 0.25, -0.25]))
	d.linetypes.append(DXF.LineType(name='CENTER',description='____ _ ____ _ __',elements=[2.0, 1.25, -0.25, 0.25, -0.25]))

	#d.vports.append(DXF.VPort('*ACTIVE'))
	d.vports.append(DXF.VPort('*ACTIVE',center=(-5.0,1.0),height=10.0))
	#d.vports.append(DXF.VPort('*ACTIVE',leftBottom=(-100.0,-60.0),rightTop=(100.0,60.0)))
	#d.views.append(DXF.View('Normal'))	  #table view
	d.views.append(DXF.ViewByWindow('BF_TOPVIEW',leftBottom=(-100,-60),rightTop=(100,60)))  #idem

	# add Entities --------------------
	BLOCKREGISTRY = {} # registry and map for BLOCKs
	PERSPECTIVE = 0
	something_ready = 0
	selected_len = len(export_list)
	sce = Scene.GetCurrent()

	mw = Mathutils.Matrix(  [1.0, 0.0, 0.0, 0.0],
						[0.0, 1.0, 0.0, 0.0],
						[0.0, 0.0, 1.0, 0.0],
						[0.0, 0.0, 0.0, 1.0])
	if PROJECTION:
		if CAMERA<len(CAMERAS)+1: # the biggest number is the current 3d-view
			act_camera =	Object.Get(CAMERAS[CAMERA-1])
			#context = sce.getRenderingContext()
			#print 'deb: context=\n', context #------------------
			#print 'deb: context=\n', dir(context) #------------------
			#act_camera = sce.objects.camera
			#print 'deb: act_camera=', act_camera #------------------
			if act_camera:
				CAMERAVIEW = 1
				mc0 = act_camera.matrix.copy()
				#print 'deb: camera.Matrix=\n', mc0 #------------------
				camera = Camera.Get(act_camera.getData(name_only=True))
				#print 'deb: camera=', dir(camera) #------------------
				if camera.type=='persp': PERSPECTIVE = 1
				elif camera.type=='ortho': PERSPECTIVE = 0
				# mcp is matrix.camera.perspective
				clip_box, mcp = getClipBox(camera)
				if PERSPECTIVE:
					# get border
					# lens = camera.lens
					min_X1, max_X1, min_Y1, max_Y1,\
					min_X2, max_X2, min_Y2, max_Y2,\
						min_Z, max_Z = clip_box
					verts = []
					verts.append([min_X1, min_Y1, min_Z])
					verts.append([max_X1, min_Y1, min_Z])
					verts.append([max_X1, max_Y1, min_Z])
					verts.append([min_X1, max_Y1, min_Z])
					border=verts
				mw = mc0.copy().invert()
	
		else: # get 3D-View instead of camera-view	
			#ViewVector = Mathutils.Vector(Window.GetViewVector())
			#print 'deb: ViewVector=\n', ViewVector #------------------
			#TODO: what is Window.GetViewOffset() for?
			#print 'deb: Window.GetViewOffset():', Window.GetViewOffset() #---------
			#Window.SetViewOffset([0,0,0])
			mw0 = Window.GetViewMatrix()
			#print 'deb: mwOrtho	=\n', mw0	 #---------
			mwp = Window.GetPerspMatrix() #TODO: how to get it working?
			#print 'deb: mwPersp	=\n', mwp	 #---------
			mw = mw0.copy()
	
	#print 'deb: ViewMatrix=\n', mw #------------------
	
	if APPLY_MODIFIERS: tmp_me = Mesh.New('tmp')
	else: tmp_me = None

	if GUI_A['paper_space_on'].val==1: espace=1
	else: espace=None

	layernames = []
	for ob,mx in export_list:
		entities = []
		block = None
		#mx = ob.matrix.copy()
		#print 'deb: ob	=', ob	 #---------
		#print 'deb: ob.type	=', ob.type	 #---------
		#print 'deb: mx	=\n', mx	 #---------
		#print 'deb: mw0	=\n', mw0	 #---------
		#mx_n is trans-matrix for normal_vectors for front-side faces
		mx_n = mx.rotationPart() * mw.rotationPart()
		if G_SCALE!=1.0: mx *= G_SCALE
		mx *= mw
		
		#mx_inv = mx.copy().invert()
		#print 'deb: mx	=\n', mx	 #---------
		#print 'deb: mx_inv=\n', mx_inv #---------

		if ob.type in ('Mesh','Curve','Empty','Text','Camera','Lamp'):
			elayer,ecolor,eltype = getCommons(ob)
			#print 'deb: elayer,ecolor,eltype =', elayer,ecolor,eltype #--------------

			#TODO: use ob.boundBox for drawing extends

			if elayer!=None:
				if elayer not in layernames:
					layernames.append(elayer)
					if ecolor!=None: tempcolor = ecolor
					else: tempcolor = LAYERCOLOR_DEF
					d.layers.append(DXF.Layer(color=tempcolor, name=elayer))

			if (ob.type == 'Mesh') and GUI_B['bmesh'].val:
				entities, block = exportMesh(ob, mx, mx_n, tmp_me,\
						paperspace=espace, color=ecolor, layer=elayer, lineType=eltype)
			elif (ob.type == 'Curve') and GUI_B['bcurve'].val:
				entities, block = exportCurve(ob, mx, mw, \
						paperspace=espace, color=ecolor, layer=elayer, lineType=eltype)
			elif (ob.type == 'Empty') and GUI_B['empty'].val:
				entities = exportEmpty(ob, mx, mw, \
						paperspace=espace, color=ecolor, layer=elayer, lineType=eltype)
			elif (ob.type == 'Text') and GUI_B['text'].val:
				entities = exportText(ob, mx, mw, \
					paperspace=espace, color=ecolor, layer=elayer, lineType=eltype)
			elif (ob.type == 'Camera') and GUI_B['camera'].val:
				entities, vport, view = exportCamera(ob, mx, mw, \
					paperspace=espace, color=ecolor, layer=elayer, lineType=eltype)
				if vport: d.vports.append(vport)
				if view: d.views.append(view)
			elif (ob.type == 'Lamp') and GUI_B['lamp'].val:
				entities = exportLamp(ob, mx, mw, \
					paperspace=espace, color=ecolor, layer=elayer, lineType=eltype)
	
			if entities:
				something_ready += 1
				for e in entities:
					d.append(e)
	
			if block:
				d.blocks.append(block) #add to BLOCK-section


	if something_ready:
		if PERSPECTIVE: # generate view border - passepartout
			identity_matrix = Mathutils.Matrix().identity()
			points = projected_co(border, identity_matrix)
			closed = 1
			points = toGlobalOrigin(points)
			c = curve_as_list[GUI_A['curve_as'].val]
			if c=="LINEs": # export Curve as multiple LINEs
				for i in range(len(points)-1):
					linepoints = [points[i], points[i+1]]
					dxfLINE = DXF.Line(linepoints,paperspace=espace,color=LAYERCOLOR_DEF)
					entities.append(dxfLINE)
			else:
				dxfPLINE = DXF.PolyLine(points,points[0],closed,\
					paperspace=espace, color=LAYERCOLOR_DEF)
				d.append(dxfPLINE)


		if not GUI_A['outputDWG_on'].val:
			print 'writing to %s' % filepath
			try:
				d.saveas(filepath)
				Window.WaitCursor(0)
				#print '%s objects exported to %s' %(something_ready,filepath)
				print '%s/%s objects exported in %.2f seconds. -----DONE-----' %(something_ready,selected_len,(Blender.sys.time()-t))
				Draw.PupMenu('DXF Exporter: job finished!| %s/%s objects exported in %.2f sek.' %(something_ready,selected_len, (Blender.sys.time()-t)))
			except IOError:
				Window.WaitCursor(0)
				Draw.PupMenu('DXF Exporter: Write Error:     Permission denied:| %s' %filepath)
				
		else:
			if not extCONV_OK:
				Draw.PupMenu(extCONV_TEXT)
				Window.WaitCursor(False)
			else:
				print 'temp. exporting to %s' % filepath
				d.saveas(filepath)
				#Draw.PupMenu('DXF Exporter: job finished')
				#print 'exported to %s' % filepath
				#print 'finished in %.2f seconds' % (Blender.sys.time()-t)
				filedwg = filepath[:-3]+'dwg'
				print 'exporting to %s' % filedwg
				os.system('%s %s  -acad13 -dwg' %(extCONV_PATH,filepath))
				#os.chdir(cwd)
				os.remove(filepath)
				Window.WaitCursor(0)
				print '  finished in %.2f seconds. -----DONE-----' % (Blender.sys.time()-t)
				Draw.PupMenu('DWG Exporter: job finished!| %s/%s objects exported in %.2f sek.' %(something_ready,selected_len, (Blender.sys.time()-t)))
	else:
		Window.WaitCursor(0)
		print "Abort: selected objects dont match choosen export option, nothing exported!"
		Draw.PupMenu('DXF Exporter:   nothing exported!|selected objects dont match choosen export option!')


#------------------------------------------------------
"""
v = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,\
28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,46,47,58,59,60,61,62,63,64,91,92,93,94,96,\
123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,\
151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,\
171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,\
191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,\
211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,\
231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254]
invalid = ''.join([chr(i) for i in v])
del v, i
"""
#TODO: validDXFr14 = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.'
validDXFr12 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_'

#------------------------------------------------------
def cleanName(name,valid):
	validname = ''
	for ch in name:
		if ch not in valid:	ch = '_'
		validname += ch
	return validname

#------------------------------------------------------
def validDXFr12name(str_name):
	dxfname = str(str_name)
	dxfname = dxfname[:NAMELENGTH_MAX].upper()
	dxfname = cleanName(dxfname,validDXFr12)
	return dxfname

#print cleanName('dumka',validDXFr12)
#print validDXFr12name('dum 15%ka')

#------------------------------------------------------
def col2RGB(color):
	return [int(floor(255*color[0])),
			int(floor(255*color[1])),
			int(floor(255*color[2]))]

global dxfColors
dxfColors=None

#------------------------------------------------------
def col2DXF(rgbcolor):
	global dxfColors
	if dxfColors is None:
		from dxfColorMap import color_map
		dxfColors = [(tuple(color),idx) for idx, color in color_map.iteritems()]
		dxfColors.sort()
	entry = (tuple(rgbcolor), -1)
	dxfColors.append(entry)
	dxfColors.sort()
	i = dxfColors.index(entry)
	dxfColors.pop(i)
	return dxfColors[i-1][1]



# NEW UI -----#################################################-----------------
# ------------#################################################-----------------

class Settings:  #-----------------------------------------------------------------
	"""A container for all the export settings and objects.

	This is like a collection of globally accessable persistant properties and functions.
	"""
	# Optimization constants
	MIN = 0
	MID = 1
	PRO = 2
	MAX = 3

	def __init__(self, keywords, drawTypes):
		"""initialize all the important settings used by the export functions.
		"""
		self.obj_number = 1 #global object_number for progress_bar

		self.var = dict(keywords)	 #a dictionary of (key_variable:Value) control parameter
		self.drawTypes = dict(drawTypes) #a dictionary of (entity_type:True/False) = import on/off for this entity_type

		self.var['colorFilter_on'] = False   #deb:remi------------
		self.acceptedColors = [0,2,3,4,5,6,7,8,9,
							   10 ]

		self.var['materialFilter_on'] = False   #deb:remi------------
		self.acceptedLayers = ['3',
						   '0'
						  ]

		self.var['groupFilter_on'] = False   #deb:remi------------
		self.acceptedLayers = ['3',
						   '0'
						  ]

		#self.var['objectFilter_on'] = 0   #deb:remi------------
		self.acceptedBlocks = ['WALL_1871',
						   'BOX02'
						  ]
		self.unwantedBlocks = ['BOX05',
						   'BOX04'
						  ]


	def update(self, keywords, drawTypes):
		"""update all the important settings used by the draw functions.
		mostly used after loading parameters from INI-file
		"""

		for k, v in keywords.iteritems():
			self.var[k] = v
			#print 'deb:settings_update var %s= %s' %(k, self.var[k]) #--------------
		for t, v in drawTypes.iteritems():
			self.drawTypes[t] = v
			#print 'deb:settings_update drawType %s= %s' %(t, self.drawTypes[t]) #--------------

		self.drawTypes['arc'] = self.drawTypes['surface']
		self.drawTypes['circle'] = self.drawTypes['surface']
		self.drawTypes['ellipse'] = self.drawTypes['surface']
		self.drawTypes['trace'] = self.drawTypes['solid']
		self.drawTypes['insert'] = self.drawTypes['group']
		#self.drawTypes['vport'] = self.drawTypes['view']

		#print 'deb:self.drawTypes', self.drawTypes #---------------


	def validate(self, drawing):
		"""Given the drawing, build dictionaries of Layers, Colors and Blocks.
		"""
		global oblist
		#adjust the distance parameter to globalScale
		if self.var['g_scale'] != 1.0:
			self.var['dist_min']  = self.var['dist_min'] / self.var['g_scale']
		self.g_origin = Mathutils.Vector(self.var['g_originX'], self.var['g_originY'], self.var['g_originZ'])


	def write(self, text, newline=True):
		"""Wraps the built-in print command in a optimization check.
		"""
		if self.var['optimization'] <= self.MID:
			if newline:
				print text
			else:
				print text,


	def redraw(self):
		"""Update Blender if optimization level is low enough.
		"""
		if self.var['optimization'] <= self.MIN:
			Blender.Redraw()


	def progress(self, done, text):
		"""Wrapper for Blender.Window.DrawProgressBar.
		"""
		if self.var['optimization'] <= self.PRO:
			progressbar = done * self.obj_number
			Window.DrawProgressBar(progressbar, text)
			#print 'deb:drawer done, progressbar: ', done, progressbar  #-----------------------



# GUI STUFF -----#################################################-----------------
from Blender.BGL import *

EVENT_NONE = 1
EVENT_START = 2
EVENT_REDRAW = 3
EVENT_LOAD_INI = 4
EVENT_SAVE_INI = 5
EVENT_RESET = 6
EVENT_CHOOSE_INI = 7
EVENT_CHOOSE_DXF = 8
EVENT_HELP = 9
EVENT_CAMERA = 10
EVENT_LIGHT =11
EVENT_DXF_DIR = 12
EVENT_setCAMERA = 13
# = 14
EVENT_ORIGIN = 15
EVENT_SCALE = 16
EVENT_PRESET2D = 20
EVENT_PRESET3D = 21
EVENT_PRESETPLINE = 22
EVENT_PRESETS = 23
EVENT_EXIT = 100

GUI_A = {}  # GUI-buttons dictionary for parameter
GUI_B = {}  # GUI-buttons dictionary for drawingTypes

# settings default, initialize ------------------------

#-----------------------------------------------
def prepareMenu(title,list):
	menu = title	
	for i, item in enumerate(list):
		menu += '|'+ item + ' %x' + str(i)
	return menu

#-----------------------------------------------
mesh_as_list  = ["3DFACEs","POLYFACE","POLYLINE","LINEs","POINTs"]
mesh_as_menu = prepareMenu("export to: %t", mesh_as_list) 

curve_as_list  = ["LINEs","POLYLINE","..LWPOLYLINE r14","..SPLINE r14","POINTs"]
curve_as_menu = prepareMenu("export to: %t", curve_as_list) 

surface_as_list   = ["..3DFACEs","..POLYFACE","..POINTs","..NURBS"]
surface_as_menu = prepareMenu("export to: %t", surface_as_list) 

meta_as_list  = ["..3DFACEs","..POLYFACE","..3DSOLID"]
meta_as_menu = prepareMenu("export to: %t", meta_as_list) 

text_as_list   = ["TEXT","..MTEXT","..ATTRIBUT"]
text_as_menu = prepareMenu("export to: %t", text_as_list) 

empty_as_list  = ["POINT","..INSERT","..XREF"]
empty_as_menu = prepareMenu("export to: %t|", empty_as_list) 

group_as_list  = ["..GROUP","..BLOCK","..ungroup"]
group_as_menu = prepareMenu("export to: %t", group_as_list) 

parent_as_list = ["..BLOCK","..ungroup"]
parent_as_menu = prepareMenu("export to: %t", parent_as_list) 

proxy_as_list = ["..BLOCK","..XREF","..ungroup","..POINT"]
proxy_as_menu = prepareMenu("export to: %t", proxy_as_list) 

camera_as_list = ["..BLOCK","..A_CAMERA","VPORT","VIEW","POINT"]
camera_as_menu = prepareMenu("export to: %t", camera_as_list) 
	
lamp_as_list  = ["..BLOCK","..A_LAMP","POINT"]
lamp_as_menu = prepareMenu("export to: %t", lamp_as_list) 
	
material_to_list= ["COLOR","LAYER","..LINESTYLE","..BLOCK","..XDATA","..INI-File"]
material_to_menu = prepareMenu("export to: %t", material_to_list) 

ltype_map_list= ["object_rgb","material_rgb","..map_table"]
ltype_map_menu = prepareMenu("export to: %t", ltype_map_list) 



layername_from_list = [LAYERNAME_DEF,"drawing_name","scene_name"]
layername_from_menu = prepareMenu("defaultLAYER: %t", layername_from_list)

layerltype_def_list = ["CONTINUOUS","DOT","DASHED","DASHDOT","BORDER","HIDDEN"]
layerltype_def_menu = prepareMenu("LINETYPE set to: %t",layerltype_def_list)

entitylayer_from_list = ["default_LAYER","obj.name","obj.layer","obj.material","obj.data.name","obj.data.material","..vertexgroup","..group","..map_table"]
entitylayer_from_menu = prepareMenu("entityLAYER from: %t", entitylayer_from_list) 
#print 'deb: entitylayer_from_menu=', entitylayer_from_menu #--------------
	
entitycolor_from_list = ["default_COLOR","BYLAYER","BYBLOCK","obj.layer","obj.color","obj.material","obj.data.material","..map_table"]
entitycolor_from_menu = prepareMenu("entityCOLOR set to: %t",entitycolor_from_list)

entityltype_from_list = ["default_LTYPE","BYLAYER","BYBLOCK","CONTINUOUS","DOT","DASHED","DASHDOT","BORDER","HIDDEN"]
entityltype_from_menu = prepareMenu("entityCOLOR set to: %t",entityltype_from_list)

#dxf-LINE,ARC,CIRCLE,ELLIPSE

g_scale_list	= ''.join((
	'scale factor: %t',
	'|user def. %x12',
	'|yard to m %x8',
	'|feet to m %x7',
	'|inch to m %x6',
	'|  x  100000 %x5',
	'|  x  10000 %x4',
	'|  x  1000 %x3',
	'|  x  100 %x2',
	'|  x  10 %x1',
	'|  x  1 %x0',
	'|  x  0.1 %x-1',
	'|  x  0.01 %x-2',
	'|  x  0.001 %x-3',
	'|  x  0.0001 %x-4',
	'|  x  0.00001 %x-5'))

#print 'deb:  g_scale_list', g_scale_list #-----------

dxfFileName = Draw.Create("")
iniFileName = Draw.Create(INIFILE_DEFAULT_NAME + INIFILE_EXTENSION)
user_preset = 0
config_UI = Draw.Create(0)   #switch_on/off extended config_UI
g_scale_as = Draw.Create(int(log10(G_SCALE)))


keywords_org = {
	'optimization': 2,
	'dummy_on' : 0,

	'xref_on' : 1,
	'block_nn': 0,

	'paper_space_on': 0,
	'layFrozen_on': 0,
	'objectFilter_on': 0,
	'materialFilter_on': 0,
	'colorFilter_on': 0,
	'groupFilter_on': 0,

	'only_selected_on': ONLYSELECTED,
	'only_visible_on': ONLYVISIBLE,
	'projection_on' : PROJECTION,
	'hidden_lines_on': HIDDEN_LINES,
	'shadows_on'  : SHADOWS,
	'light_on'  : 1,
	'outputDWG_on' : OUTPUT_DWG,
	'to_polyline_on': POLYLINES,
	'to_polyface_on': POLYFACES,
	'instances_on': INSTANCES,
	'apply_modifiers_on': APPLY_MODIFIERS,
	'include_duplis_on': INCLUDE_DUPLIS,
	'camera_selected': CAMERA,

	'g_originX'   : G_ORIGIN[0],
	'g_originY'   : G_ORIGIN[1],
	'g_originZ'   : G_ORIGIN[2],
	'g_origin_on': 0,
	'g_scale'   : float(G_SCALE),
#   'g_scale_as': int(log10(G_SCALE)), #   0,
	'g_scale_on': 0,
	'Z_force_on': 0,
	'Z_elev': float(ELEVATION),


	'prefix_def' : PREFIX,
	'layername_def' : LAYERNAME_DEF,
	'layercolor_def': LAYERCOLOR_DEF,
	'layerltype_def': LAYERLTYPE_DEF,
	'entitylayer_from': 5,
	'entitycolor_from': 1,
	'entityltype_from' : 1,

	'material_on': 1,
	'material_to': 2,
	'fill_on'   : 1,

	'mesh_as'  : 1,
	'curve_as' : 1,
	'surface_as' : 1,
	'meta_as'  : 1,
	'text_as' : 0,
	'empty_as' : 0,
	'group_as' : 0,
	'parent_as' : 0,
	'proxy_as' : 0,
	'camera_as': 3,
	'lamp_as' : 2,
	}

drawTypes_org = {
	'bmesh' : 1,
	'bcurve': 1,
	'surface': 0,
	'bmeta' : 0,
	'text'  : 1,
	'empty' : 1,
	'group' : 1,
	'parent': 1,
	'proxy' : 0,
	'camera': 0,
	'lamp'  : 0,

#   'view' : 0,
	}

# creating of GUI-buttons
# GUI_A - GUI-buttons dictionary for parameter
# GUI_B - GUI-buttons dictionary for drawingTypes
for k, v in keywords_org.iteritems():
	GUI_A[k] = Draw.Create(v)
for k, v in drawTypes_org.iteritems():
	GUI_B[k] = Draw.Create(v)
#print 'deb:init GUI_A: ', GUI_A #---------------
#print 'deb:init GUI_B: ', GUI_B #---------------

model_space_on = Draw.Create(1)

# initialize settings-object controls how dxf entities are drawn
settings = Settings(keywords_org, drawTypes_org)


def update_RegistryKey(key, item): #
	"""updates key in Blender.Registry
	"""
	cache = True # data is also saved to a file
	rdict = Registry.GetKey('DXF_Exporter', cache)
	if not rdict: rdict = {}
	if item:
		rdict[key] = item
		Registry.SetKey('DXF_Exporter', rdict, cache)
		#print  'deb:update_RegistryKey rdict', rdict #---------------


def check_RegistryKey(key):
	""" check if the key is already there (saved on a previous execution of this script)
	"""
	cache = True # data is also saved to a file
	rdict = Registry.GetKey('DXF_Exporter', cache)
	#print 'deb:check_RegistryKey  rdict:', rdict #----------------
	if rdict: # if found, get the values saved there
		try:
			item = rdict[key]
			return item
		except:
			#update_RegistryKey() # if data isn't valid rewrite it
			pass

def saveConfig():  #--todo-----------------------------------------------
	"""Save settings/config/materials from GUI to INI-file.

	Write all config data to INI-file.
	"""
	global iniFileName

	iniFile = iniFileName.val
	#print 'deb:saveConfig inifFile: ', inifFile #----------------------
	if iniFile.lower().endswith(INIFILE_EXTENSION):

		#--todo-- sort key.list for output
		#key_list = GUI_A.keys().val
		#key_list.sort()
		#for key in key_list:
		#   l_name, l_data = key, GUI_A[key].val
		#   list_A

		output_str = '[%s,%s]' %(GUI_A, GUI_B)
		if output_str =='None':
			Draw.PupMenu('DXF-Exporter: INI-file:  Alert!%t|no config-data present to save!')
		else:
			if Blender.sys.exists(iniFile):
				f = file(iniFile, 'r')
				header_str = f.readline()
				f.close()
				if header_str.startswith(INIFILE_HEADER[0:13]):
					if Draw.PupMenu('  OK ? %t|SAVE OVER: ' + '\'%s\'' %iniFile) == 1:
						save_ok = True
					else: save_ok = False
				elif Draw.PupMenu('  OK ? %t|SAVE OVER: ' + '\'%s\'' %iniFile +
					 '|Alert: this file has no valid ImportDXF-header| ! it may belong to another aplication !') == 1:
					save_ok = True
				else: save_ok = False
			else: save_ok = True

			if save_ok:
				# replace: ',' -> ',\n'
				# replace: '{' -> '\n{\n'
				# replace: '}' -> '\n}\n'
				output_str = ',\n'.join(output_str.split(','))
				output_str = '\n}'.join(output_str.split('}'))
				output_str = '{\n'.join(output_str.split('{'))
				try:
					f = file(iniFile, 'w')
					f.write(INIFILE_HEADER + '\n# this is a comment line\n')
					f.write(output_str)
					f.close()
					#Draw.PupMenu('DXF-Exporter: INI-file: Done!%t|config-data saved in ' + '\'%s\'' %iniFile)
				except:
					Draw.PupMenu('DXF-Exporter: INI-file: Error!%t|failure by writing to ' + '\'%s\'|no config-data saved!' %iniFile)

	else:
		Draw.PupMenu('DXF-Exporter: INI-file:  Alert!%t|no valid name/extension for INI-file selected!')
		print "DXF-Exporter: Alert!: no valid INI-file selected."
		if not iniFile:
			if dxfFileName.val.lower().endswith('.dxf'):
				iniFileName.val = dxfFileName.val[0:-4] + INIFILE_EXTENSION


def loadConfig():  #remi--todo-----------------------------------------------
	"""Load settings/config/materials from INI-file.

	TODO: Read material-assignements from config-file.
	"""
	#20070724 buggy Window.FileSelector(loadConfigFile, 'Load config data from INI-file', inifilename)
	global iniFileName, GUI_A, GUI_B

	iniFile = iniFileName.val
	update_RegistryKey('iniFileName', iniFile)
	#print 'deb:loadConfig iniFile: ', iniFile #----------------------
	if iniFile.lower().endswith(INIFILE_EXTENSION) and Blender.sys.exists(iniFile):
		f = file(iniFile, 'r')
		header_str = f.readline()
		if header_str.startswith(INIFILE_HEADER):
			data_str = f.read()
			f.close()
			#print 'deb:loadConfig data_str from %s: \n' %iniFile , data_str #-----------------
			data = eval(data_str)
			for k, v in data[0].iteritems():
				try: GUI_A[k].val = v
				except: GUI_A[k] = Draw.Create(v)
			for k, v in data[1].iteritems():
				try: GUI_B[k].val = v
				except: GUI_B[k] = Draw.Create(v)
		else:
			f.close()
			Draw.PupMenu('DXF-Exporter: INI-file:  Alert!%t|no valid header in INI-file: ' + '\'%s\'' %iniFile)
	else:
		Draw.PupMenu('DXF-Exporter: INI-file:  Alert!%t|no valid INI-file selected!')
		print "DXF-Exporter: Alert!: no valid INI-file selected."
		if not iniFileName:
			if dxfFileName.val.lower().endswith('.dxf'):
				iniFileName.val = dxfFileName.val[0:-4] + INIFILE_EXTENSION



def updateConfig(keywords, drawTypes):  #-----------------------------------------------
	"""updates GUI_settings with given dictionaries

	"""
	global GUI_A, GUI_B
	#print 'deb:lresetDefaultConfig keywords_org: \n', keywords_org #---------
	for k, v in keywords.iteritems():
		GUI_A[k].val = v
	for k, v in drawTypes.iteritems():
		GUI_B[k].val = v

def resetDefaultConfig():  #-----------------------------------------------
	"""Resets settings/config/materials to defaults.

	"""
	#print 'deb:lresetDefaultConfig keywords_org: \n', keywords_org #---------
	updateConfig(keywords_org, drawTypes_org)


def presetConfig_polyline(activate):  #-----------------------------------------------
	"""Sets settings/config for polygon representation: POLYLINE(FACE) or LINEs/3DFACEs.

	"""
	global GUI_A
	if activate:
		GUI_A['to_polyline_on'].val = 1
		GUI_A['mesh_as'].val = 1
		GUI_A['curve_as'].val = 1
	else:
		GUI_A['to_polyline_on'].val = 0
		GUI_A['mesh_as'].val = 0
		GUI_A['curve_as'].val = 0
	
def resetDefaultConfig_2D():  #-----------------------------------------------
	"""Sets settings/config/materials to defaults 2D.

	"""
	keywords2d = {
		'projection_on' : 1,
		'fill_on' : 1,
		'text_as' : 0,
		'group_as' : 0,
		}

	drawTypes2d = {
		'bmesh' : 1,
		'bcurve': 1,
		'surface':0,
		'bmeta' : 0,
		'text'  : 1,
		'empty' : 1,
		'group' : 1,
		'parent' : 1,
		#'proxy' : 0,
		#'camera': 0,
		#'lamp'  : 0,

		}
	presetConfig_polyline(1)
	updateConfig(keywords2d, drawTypes2d)

def resetDefaultConfig_3D():  #-----------------------------------------------
	"""Sets settings/config/materials to defaults 3D.

	"""
	keywords3d = {
		'projection_on' : 0,
		'fill_on' : 0,
		'text_as' : 0,
		'group_as' : 0,
		}

	drawTypes3d = {
		'bmesh' : 1,
		'bcurve': 1,
		'surface':0,
		'bmeta' : 0,
		'text'  : 0,
		'empty' : 1,
		'group' : 1,
		'parent' : 1,
		#'proxy' : 0,
		#'camera': 1,
		#'lamp'  : 1,
		}
	presetConfig_polyline(1)
	updateConfig(keywords3d, drawTypes3d)


def inputGlobalScale():
	"""Pop-up UI-Block for global scale factor
	"""
	global GUI_A
	#print 'deb:inputGlobalScale ##########' #------------
	x_scale = Draw.Create(GUI_A['g_scale'].val)
	block = []
	#block.append("global translation vector:")
	block.append(("", x_scale, 0.0, 10000000.0))

	retval = Draw.PupBlock("set global scale factor:", block)

	GUI_A['g_scale'].val = float(x_scale.val)

	
def inputOriginVector():
	"""Pop-up UI-Block for global translation vector
	"""
	global GUI_A
	#print 'deb:inputOriginVector ##########' #------------
	x_origin = Draw.Create(GUI_A['g_originX'].val)
	y_origin = Draw.Create(GUI_A['g_originY'].val)
	z_origin = Draw.Create(GUI_A['g_originZ'].val)
	block = []
	#block.append("global translation vector:")
	block.append(("X: ", x_origin, -100000000.0, 100000000.0))
	block.append(("Y: ", y_origin, -100000000.0, 100000000.0))
	block.append(("Z: ", z_origin, -100000000.0, 100000000.0))

	retval = Draw.PupBlock("set global translation vector:", block)

	GUI_A['g_originX'].val = x_origin.val
	GUI_A['g_originY'].val = y_origin.val
	GUI_A['g_originZ'].val = z_origin.val


def update_globals():  #-----------------------------------------------------------------
	""" update globals if GUI_A changed
	"""
	global  ONLYSELECTED,ONLYVISIBLE, DEBUG,\
	PROJECTION, HIDDEN_LINES,	CAMERA, \
	G_SCALE, G_ORIGIN,\
	PREFIX, LAYERNAME_DEF, LAYERCOLOR_DEF, LAYERLTYPE_DEF,\
	APPLY_MODIFIERS, INCLUDE_DUPLIS,\
	OUTPUT_DWG
	#global POLYLINES
	
	ONLYSELECTED = GUI_A['only_selected_on'].val
	ONLYVISIBLE = GUI_A['only_visible_on'].val
	"""
	POLYLINES = GUI_A['to_polyline_on'].val
	if GUI_A['curve_as'].val==1: POLYLINES=1
	else: POLYLINES=0
	"""
	
	if GUI_A['optimization'].val==0: DEBUG = 1
	else: DEBUG = 0
	PROJECTION = GUI_A['projection_on'].val
	HIDDEN_LINES = GUI_A['hidden_lines_on'].val
	CAMERA = GUI_A['camera_selected'].val
	G_SCALE = GUI_A['g_scale'].val
	if GUI_A['g_origin_on'].val:
		G_ORIGIN[0] = GUI_A['g_originX'].val
		G_ORIGIN[1] = GUI_A['g_originY'].val
		G_ORIGIN[2] = GUI_A['g_originZ'].val
	if GUI_A['g_scale_on'].val:
		G_ORIGIN[0] *= G_SCALE
		G_ORIGIN[1] *= G_SCALE
		G_ORIGIN[2] *= G_SCALE

	PREFIX = GUI_A['prefix_def'].val
	LAYERNAME_DEF  = GUI_A['layername_def'].val
	LAYERCOLOR_DEF = GUI_A['layercolor_def'].val
	LAYERLTYPE_DEF = layerltype_def_list[GUI_A['layerltype_def'].val]

	APPLY_MODIFIERS = GUI_A['apply_modifiers_on'].val
	INCLUDE_DUPLIS = GUI_A['include_duplis_on'].val
	OUTPUT_DWG = GUI_A['outputDWG_on'].val
	#print 'deb: GUI HIDDEN_LINES=', HIDDEN_LINES #---------
	#print 'deb: GUI GUI_A: ', GUI_A['hidden_lines_on'].val #---------------
	#print 'deb: GUI GUI_B: ', GUI_B #---------------


def draw_UI():  #-----------------------------------------------------------------
	""" Draw startUI and setup Settings.
	"""
	global GUI_A, GUI_B #__version__
	global user_preset, iniFileName, dxfFileName, config_UI, g_scale_as
	global model_space_on
	global SCROLL

	global mPAN_X, menu_orgX, mPAN_Xmax
	global mPAN_Y, menu_orgY, mPAN_Ymax
	global menu__Area, headerArea, screenArea, scrollArea

	size=Buffer(GL_FLOAT, 4)
	glGetFloatv(GL_SCISSOR_BOX, size) #window X,Y,sizeX,sizeY
	size= size.list
	#print '-------------size:', size #--------------------------
	for s in [0,1,2,3]: size[s]=int(size[s])
	window_Area = [0,0,size[2],size[3]-2]
	scrollXArea = [0,0,window_Area[2],15]
	scrollYArea = [0,0,15,window_Area[3]]

	menu_orgX = -mPAN_X
	#menu_orgX = 0 #scrollW
	#if menu_pan: menu_orgX -= mPAN_X
	if menu_orgX < -mPAN_Xmax: menu_orgX, mPAN_X = -mPAN_Xmax,mPAN_Xmax
	if menu_orgX > 0: menu_orgX, mPAN_X = 0,0

	menu_orgY = -mPAN_Y
	#if menu_pan: menu_orgY -= mPAN_Y
	if menu_orgY < -mPAN_Ymax: menu_orgY, mPAN_Y = -mPAN_Ymax,mPAN_Ymax
	if menu_orgY > 0: menu_orgY, mPAN_Y = 0,0


	menu_margin = 10
	butt_margin = 10
	common_column = int((window_Area[2] - (3 * butt_margin) - (2 * menu_margin)-30) / 4.0)
	common_column = 70
	# This is for easy layout changes
	but_0c = common_column  #button 1.column width
	but_1c = common_column  #button 1.column width
	but_2c = common_column  #button 2.column
	but_3c = common_column  #button 3.column
	menu_w = (3 * butt_margin) + but_0c + but_1c + but_2c + but_3c  #menu width

	simple_menu_h = 260
	extend_menu_h = 345
	menu_h = simple_menu_h		# y is menu upper.y
	if config_UI.val:
		menu_h += extend_menu_h

	mPAN_Xmax = menu_w-window_Area[2]+50
	mPAN_Ymax = menu_h-window_Area[3]+30

	y = menu_h
	x = 0 #menu left.x
	x +=menu_orgX+20
	y +=menu_orgY+20


	but0c = x + menu_margin  #buttons 0.column position.x
	but1c = but0c + but_0c + butt_margin
	but2c = but1c + but_1c + butt_margin
	but3c = but2c + but_2c + butt_margin
	but4c = but3c + but_3c

	# Here starts menu -----------------------------------------------------
	#glClear(GL_COLOR_BUFFER_BIT)
	#glRasterPos2d(8, 125)


	ui_box(x, y, x+menu_w+menu_margin*2, y-menu_h)
	y -= 20
	Draw.Label("DXF(r12)-Exporter  v" + __version__, but0c, y, menu_w, 20)

	if config_UI.val:
		b0, b0_ = but0c, but_0c-20 + butt_margin
		b1, b1_ = but1c-20, but_1c+20
		y_top = y

		y -= 10
		y -= 20
		Draw.BeginAlign()
		GUI_B['bmesh'] = Draw.Toggle('Mesh', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['bmesh'].val, "Export Mesh-Objects   on/off")
		if GUI_B['bmesh'].val:
			GUI_A['mesh_as'] = Draw.Menu(mesh_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['mesh_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['bcurve'] = Draw.Toggle('Curve', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['bcurve'].val, "Export Curve-Objects   on/off")
		if GUI_B['bcurve'].val:
			GUI_A['curve_as'] = Draw.Menu(curve_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['curve_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['surface'] = Draw.Toggle('..Surface', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['surface'].val, "(*todo) Export Surface-Objects   on/off")
		if GUI_B['surface'].val:
			GUI_A['surface_as'] = Draw.Menu(surface_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['surface_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['bmeta'] = Draw.Toggle('..Meta', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['bmeta'].val, "(*todo) Export Meta-Objects   on/off")
		if GUI_B['bmeta'].val:
			GUI_A['meta_as'] = Draw.Menu(meta_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['meta_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['text'] = Draw.Toggle('Text', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['text'].val, "Export Text-Objects   on/off")
		if GUI_B['text'].val:
			GUI_A['text_as'] = Draw.Menu(text_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['text_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['empty'] = Draw.Toggle('Empty', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['empty'].val, "Export Empty-Objects   on/off")
		if GUI_B['empty'].val:
			GUI_A['empty_as'] = Draw.Menu(empty_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['empty_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y_down = y
		# -----------------------------------------------

		y = y_top
		b0, b0_ = but2c, but_2c-20 + butt_margin
		b1, b1_ = but3c-20, but_3c+20

		y -= 10
		y -= 20
		Draw.BeginAlign()
		GUI_B['group'] = Draw.Toggle('..Group', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['group'].val, "(*todo) Export Group-Relationships   on/off")
		if GUI_B['group'].val:
			GUI_A['group_as'] = Draw.Menu(group_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['group_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['parent'] = Draw.Toggle('..Parent', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['parent'].val, "(*todo) Export Parent-Relationships   on/off")
		if GUI_B['parent'].val:
			GUI_A['parent_as'] = Draw.Menu(parent_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['parent_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['proxy'] = Draw.Toggle('..Proxy', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['proxy'].val, "(*todo) Export Proxy-Objects   on/off")
		if GUI_B['proxy'].val:
			GUI_A['proxy_as'] = Draw.Menu(proxy_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['proxy_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['camera'] = Draw.Toggle('Camera', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['camera'].val, "(*wip) Export Camera-Objects   on/off")
		if GUI_B['camera'].val:
			GUI_A['camera_as'] = Draw.Menu(camera_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['camera_as'].val, "Select target DXF-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['lamp'] = Draw.Toggle('Lamp', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['lamp'].val, "(*wip) Export Lamp-Objects   on/off")
		if GUI_B['lamp'].val:
			GUI_A['lamp_as'] = Draw.Menu(lamp_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['lamp_as'].val, "Select target DXF-object")
		Draw.EndAlign()


		if y < y_down: y_down = y
		# -----end supported objects--------------------------------------

		y_top = y_down
		y = y_top
		y -= 10
		y -= 20
		but_ = menu_w / 6
		b0 = but0c + (menu_w - but_*6)/2
		Draw.BeginAlign()
		#GUI_A['dummy_on'] = Draw.Toggle('-', EVENT_NONE, b0+but_*0, y, but_, 20, GUI_A['dummy_on'].val, "placeholder only   on/off")
		GUI_A['paper_space_on'] = Draw.Toggle('Paper', EVENT_NONE, b0+but_*0, y, but_, 20, GUI_A['paper_space_on'].val, "Export to Paper-Space, otherwise to Model-Space   on/off")
		GUI_A['layFrozen_on'] = Draw.Toggle ('..frozen', EVENT_NONE, b0+but_*1, y, but_, 20, GUI_A['layFrozen_on'].val, "(*todo) Support LAYER.frozen status   on/off")
		GUI_A['materialFilter_on'] = Draw.Toggle('..material', EVENT_NONE, b0+but_*2, y, but_, 20, GUI_A['materialFilter_on'].val, "(*todo) Material filtering   on/off")
		GUI_A['colorFilter_on'] = Draw.Toggle('..color', EVENT_NONE, b0+but_*3, y, but_, 20, GUI_A['colorFilter_on'].val, "(*todo) Color filtering   on/off")
		GUI_A['groupFilter_on'] = Draw.Toggle('..group', EVENT_NONE, b0+but_*4, y, but_, 20, GUI_A['groupFilter_on'].val, "(*todo) Group filtering   on/off")
		GUI_A['objectFilter_on'] = Draw.Toggle('..object', EVENT_NONE, b0+but_*5, y, but_, 20, GUI_A['objectFilter_on'].val, "(*todo) Object filtering   on/off")
		Draw.EndAlign()

		# -----end filters--------------------------------------

		b0, b0_ = but0c, but_0c + butt_margin
		b1, b1_ = but1c, but_1c

		y -= 10
		y -= 20
		Draw.BeginAlign()
		GUI_A['g_origin_on'] = Draw.Toggle('Location', EVENT_REDRAW, b0, y, b0_, 20, GUI_A['g_origin_on'].val, "Global relocate all objects   on/off")
		if GUI_A['g_origin_on'].val:
			tmp = Draw.PushButton('=', EVENT_ORIGIN, b1, y, 20, 20, "Edit relocation-vector (x,y,z in DXF units)")
			origin_str = '(%.4f, %.4f, %.4f)'  % (
				GUI_A['g_originX'].val,
				GUI_A['g_originY'].val,
				GUI_A['g_originZ'].val
				)
			tmp = Draw.Label(origin_str, b1+20, y, 300, 20)
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_A['g_scale_on'] = Draw.Toggle('Scale', EVENT_REDRAW, b0, y, b0_, 20, GUI_A['g_scale_on'].val, "Global scale all objects   on/off")
		if GUI_A['g_scale_on'].val:
			g_scale_as = Draw.Menu(g_scale_list, EVENT_SCALE, b1, y, 45, 20, g_scale_as.val, "Factor for scaling the DXFdata")
			if g_scale_as.val == 12:
				pass
			else:
				if g_scale_as.val == 6: #scale inches to meters
					GUI_A['g_scale'].val = 0.0254000
				elif g_scale_as.val == 7: #scale feets to meters
					GUI_A['g_scale'].val = 0.3048000
				elif g_scale_as.val == 8: #scale yards to meters
					GUI_A['g_scale'].val = 0.9144000
				else:
					GUI_A['g_scale'].val = 10.0 ** int(g_scale_as.val)
			scale_float = GUI_A['g_scale'].val
			if scale_float < 0.000001 or scale_float > 1000000:
				scale_str = ' = %s' % GUI_A['g_scale'].val
			else:   
				scale_str = ' = %.6f' % GUI_A['g_scale'].val
			Draw.Label(scale_str, b1+45, y, 200, 20)
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_A['Z_force_on'] = Draw.Toggle('Elevation', EVENT_REDRAW, b0, y, b0_, 20, GUI_A['Z_force_on'].val, "Overwrite Z-coordinates (flatten geometry)   on/off")
		if GUI_A['Z_force_on'].val:
			GUI_A['Z_elev'] = Draw.Number('', EVENT_NONE, b1, y, b1_, 20, GUI_A['Z_elev'].val, -1000, 1000, "Set value for default Z-coordinate (in DXF units)")
		Draw.EndAlign()

		"""
		y -= 30
		Draw.BeginAlign()
		GUI_A['material_on'] = Draw.Toggle('.material', EVENT_REDRAW, b0, y, b0_-20, 20, GUI_A['material_on'].val, "Support for material assignment   on/off")
		if GUI_A['material_on'].val:
			GUI_A['material_to'] = Draw.Menu(material_to_menu,   EVENT_NONE, b1-20, y, b1_+20, 20, GUI_A['material_to'].val, "Material assigned to?")
		Draw.EndAlign()
		"""

		#b0, b0_ = but0c, but_0c + butt_margin
		b0, b0_ = but0c, 50
		b1, b1_ = b0+b0_, but_0c-b0_+ but_1c + butt_margin
		b2, b2_ = but2c, but_2c
		b3, b3_ = but3c, but_3c

		y -= 30
		Draw.Label('Output:', b0, y, b0_, 20)
		Draw.Label('LAYER:', b1, y, b1_, 20)
		Draw.Label('COLOR:', b2, y, b2_, 20)
		Draw.Label('LINETYPE:', b3, y, b3_, 20)
		#Draw.Label('LINESIZE:', b4, y, b4_, 20)

		y -= 20
		Draw.BeginAlign()
		GUI_A['prefix_def'] = Draw.String('', EVENT_NONE, b0, y, b0_, 20, GUI_A['prefix_def'].val, 10, "Type Prefix for LAYERs")
		GUI_A['layername_def'] = Draw.String('', EVENT_NONE, b1, y, b1_, 20, GUI_A['layername_def'].val, 10, "Type default LAYER name")
		GUI_A['layercolor_def'] = Draw.Number('', EVENT_NONE, b2, y, b2_, 20, GUI_A['layercolor_def'].val, 1, 255, "Set default COLOR. (0=BYBLOCK,256=BYLAYER)")
		GUI_A['layerltype_def'] = Draw.Menu(layerltype_def_menu, EVENT_NONE, b3, y, b3_, 20, GUI_A['layerltype_def'].val, "Set default LINETYPE")
		Draw.EndAlign()

		y -= 25
		Draw.Label('Style:', b0, y, b0_, 20)
		Draw.BeginAlign()
		GUI_A['entitylayer_from'] = Draw.Menu(entitylayer_from_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['entitylayer_from'].val, "entity LAYER assigned to?")
		GUI_A['entitycolor_from'] = Draw.Menu(entitycolor_from_menu, EVENT_NONE, b2, y, b2_, 20, GUI_A['entitycolor_from'].val, "entity COLOR assigned to?")
		GUI_A['entityltype_from'] = Draw.Menu(entityltype_from_menu, EVENT_NONE, b3, y, b3_, 20, GUI_A['entityltype_from'].val, "Set entity LINETYPE")
		Draw.EndAlign()
		
		y -= 10

		y_down = y
		# -----end material,translate,scale------------------------------------------


		#--------------------------------------
		y_top = y_down
		y = y_top

		y -= 30
		Draw.BeginAlign()
		Draw.PushButton('INI file >', EVENT_CHOOSE_INI, but0c, y, but_0c, 20, 'Select INI-file with file selector')
		iniFileName = Draw.String(' :', EVENT_NONE, but1c, y, menu_w-but_1c-60, 20, iniFileName.val, FILENAME_MAX, "Write here the name of the INI-file")
		but = but4c-60
		Draw.PushButton('#', EVENT_PRESETS, but, y, 20, 20, "Toggle Preset-INI-files")
		Draw.PushButton('L', EVENT_LOAD_INI, but+20, y, 20, 20, 'Loads configuration from selected ini-file: %s' % iniFileName.val)
		Draw.PushButton('S', EVENT_SAVE_INI, but+40, y, 20, 20, 'Saves configuration to selected ini-file: %s' % iniFileName.val)
		Draw.EndAlign()

	bm = butt_margin/2

	y -= 10
	y -= 20
	Draw.BeginAlign()
	Draw.PushButton('DXFfile >', EVENT_CHOOSE_DXF, but0c, y, but_0c, 20, 'Select DXF-file with file selector')
	dxfFileName = Draw.String(' :', EVENT_NONE, but1c, y, menu_w-but_0c-menu_margin, 20, dxfFileName.val, FILENAME_MAX, "Type path/name of output DXF-file")
	Draw.EndAlign()

	y -= 30
	config_UI = Draw.Toggle('CONFIG', EVENT_REDRAW, but0c, y, but_0c+bm, 20, config_UI.val, 'Advanced configuration   on/off' )
	Draw.BeginAlign()
	but, but_ = but1c, but_1c+bm
	but_ /= 3
	Draw.PushButton('X', EVENT_RESET, but, y, 15, 20, "Reset configuration to defaults")
	Draw.PushButton('2D', EVENT_PRESET2D, but+but_, y, but_, 20, 'Set to standard configuration for 2D export')
	Draw.PushButton('3D', EVENT_PRESET3D, but+(but_*2), y, but_, 20, 'Set to standard configuration for 3D import')
	Draw.EndAlign()


	y -= 30
	b0, b0_ = but0c, but_0c + butt_margin +but_1c
	GUI_A['only_selected_on'] = Draw.Toggle('Export Selection', EVENT_NONE, b0, y, b0_, 20, GUI_A['only_selected_on'].val, "Export only selected geometry   on/off")
	b0, b0_ = but2c, but_2c + butt_margin + but_3c
	Draw.BeginAlign()
	GUI_A['projection_on'] = Draw.Toggle('2d Projection', EVENT_REDRAW, b0, y, b0_, 20, GUI_A['projection_on'].val, "Export a 2d Projection according 3d-View or Camera-View   on/off")
	if GUI_A['projection_on'].val:
		GUI_A['camera_selected'] = Draw.Menu(MenuCAMERA, EVENT_CAMERA, b0, y-20, b0_-20, 20, GUI_A['camera_selected'].val, 'Choose the camera to be rendered')		
		Draw.PushButton('>', EVENT_setCAMERA, b0+b0_-20, y-20, 20, 20, 'switch to selected Camera - make it active')
		GUI_A['hidden_lines_on'] = Draw.Toggle('Remove backFaces', EVENT_NONE, b0, y-40, b0_, 20, GUI_A['hidden_lines_on'].val, "Filter out backFaces   on/off")
		#GUI_A['shadows_on'] = Draw.Toggle('..Shadows', EVENT_REDRAW, b0, y-60, but_2c, 20, GUI_A['shadows_on'].val, "(*todo) Shadow tracing   on/off")
		#GUI_A['light_on'] = Draw.Menu(MenuLIGHT, EVENT_LIGHT, but3c, y-60, but_3c, 20, GUI_A['light_on'].val, '(*todo) Choose the light source(sun) to be rendered')		
	Draw.EndAlign()

	y -= 20
	b0, b0_ = but0c, but_0c + butt_margin +but_1c
	GUI_A['only_visible_on'] = Draw.Toggle('Visible only', EVENT_PRESETPLINE, b0, y, b0_, 20, GUI_A['only_visible_on'].val, "Export only from visible layers   on/off")
	#b0, b0_ = but2c, but_2c + butt_margin + but_3c

	y -= 20
	b0, b0_ = but0c, but_0c + butt_margin +but_1c
	GUI_A['to_polyline_on'] = Draw.Toggle('POLYLINE-Mode', EVENT_PRESETPLINE, b0, y, b0_, 20, GUI_A['to_polyline_on'].val, "Export to POLYLINE/POLYFACEs, otherwise to LINEs/3DFACEs   on/off")
	#b0, b0_ = but2c, but_2c + butt_margin + but_3c

	y -= 20
	b0, b0_ = but0c, but_0c + butt_margin +but_1c
	GUI_A['instances_on'] = Draw.Toggle('Instances as BLOCKs', EVENT_NONE, b0, y, b0_, 20, GUI_A['instances_on'].val, "Export instances (multi-users) of Mesh/Curve as BLOCK/INSERTs   on/off")
	#b0, b0_ = but2c, but_2c + butt_margin + but_3c

	y -= 20
	b0, b0_ = but0c, but_0c + butt_margin +but_1c
	GUI_A['apply_modifiers_on'] = Draw.Toggle('Apply Modifiers', EVENT_NONE, b0, y, b0_, 20, GUI_A['apply_modifiers_on'].val, "Apply modifier stack to mesh objects before export   on/off")
	#b0, b0_ = but2c, but_2c + butt_margin + but_3c

	y -= 20
	b0, b0_ = but0c, but_0c + butt_margin +but_1c
	GUI_A['include_duplis_on'] = Draw.Toggle('Include Duplis', EVENT_NONE, b0, y, b0_, 20, GUI_A['include_duplis_on'].val, "Export Duplicates (dupliverts, dupliframes, dupligroups)   on/off")
	#b0, b0_ = but2c, but_2c + butt_margin + but_3c
	

	
	y -= 30
	Draw.PushButton('EXIT', EVENT_EXIT, but0c, y, but_0c+bm, 20, '' )
	Draw.PushButton('HELP', EVENT_HELP, but1c, y, but_1c+bm, 20, 'goes to online-Manual on wiki.blender.org')
	GUI_A['optimization'] = Draw.Number('', EVENT_NONE, but2c, y, 40, 20, GUI_A['optimization'].val, 0, 3, "Optimization Level: 0=Debug/Draw-in, 1=Verbose, 2=ProgressBar, 3=SilentMode")
	GUI_A['outputDWG_on'] = Draw.Toggle('DWG*', EVENT_NONE, but2c, y+20, 40, 20, GUI_A['outputDWG_on'].val, "converts DXF to DWG (needs external converter)   on/off")

	Draw.BeginAlign()
	Draw.PushButton('START EXPORT', EVENT_START, but2c+40, y, but_2c-40+but_3c+butt_margin, 40, 'Start the export process. For Cancel go to console and hit Ctrl-C')
	Draw.EndAlign()

	y -= 20
	#Draw.BeginAlign()
	#Draw.Label(' ', but0c-menu_margin, y, menu_margin, 20)
	#Draw.Label(LAB, but0c, y, menu_w, 20)
	Draw.Label(LAB, 30, y, menu_w, 20)
	#Draw.Label(' ', but0c+menu_w, y, menu_margin, 20)
	#Draw.EndAlign()

	ui_scrollbarX(menu_orgX, menu_w+50, scrollXArea, c_fg, c_bg)
	ui_scrollbarY(menu_orgY, menu_h+30, scrollYArea, c_fg, c_bg)




#-- END GUI Stuf-----------------------------------------------------

c0=[0.2,0.2,0.2,0.0]
c1=[0.7,0.7,0.9,0.0]
c2=[0.71,0.71,0.71,0.0]
c3=[0.4,0.4,0.4,0.0]
c4=[0.95,0.95,0.9,0.0]
c5=[0.64,0.64,0.64,0]
c6=[0.75,0.75,0.75,0]
c7=[0.6,0.6,0.6,0]
c8=[1.0,0.0,0.0,0]
c9=[0.7,0.0,0.0,0]
c10=[0.64,0.81,0.81,0]
c11=[0.57,0.71,0.71,0]
c_nor= c5[:3]
c_act= c10[:3]
c_sel= c11[:3]
c_tx = c0[:3]
c_fg = c2[:3]
c_bg = c5[:3]

def ui_rect(coords,color):
	[X1,Y1,X2,Y2],[r,g,b] = coords,color
	glColor3f(r,g,b)
	glRecti(X1,Y1,X2,Y2)
def ui_rectA(coords,color):
	[X1,Y1,X2,Y2],[r,g,b,a] = coords,color
	glColor4f(r,g,b,a)
	glRecti(X1,Y1,X2,Y2)  #integer coords
	#glRectf(X1,Y1,X2,Y2) #floating coords
def ui_line(coords,color):
	[X1,Y1,X2,Y2],[r,g,b] = coords,color
	glColor3f(r,g,b)
	glBegin(GL_LINES)
	glVertex2i(X1,Y1)
	glVertex2i(X2,Y2)
	glEnd()
def ui_panel(posX,posY,L,H,color):
	[r,g,b] = color
	ui_rect([posX+4,posY-4,posX+L+4,posY-H-4],[.55,.55,.55])		#1st shadow
	ui_rect([posX+3,posY-3,posX+L+3,posY-H-3],[.45,.45,.45])
	ui_rect([posX+3,posY-3,posX+L+2,posY-H-2],[.30,.30,.30])		#2nd shadow
	ui_rect([posX,posY-H,posX+L,posY],[r,g,b])						#Main
	ui_rect([posX+3,posY-19,posX+L-3,posY-2],[.75*r,.75*g,.75*b])	#Titlebar
	ui_line([posX+3,posY-19,posX+3,posY-2],[.25,.25,.25])
	ui_line([posX+4,posY-19,posX+4,posY-2],[(r+.75)/4,(g+.75)/4,(b+.75)/4])
	ui_line([posX+4,posY-2,posX+L-3,posY-2],[(r+.75)/4,(g+.75)/4,(b+.75)/4])
def ui_box(x,y,xright,bottom):
	color = [0.75, 0.75, 0.75]
	coords = x+1,y+1,xright-1,bottom-1
	ui_rect(coords,color)

def ui_scrollbarX(Focus,PanelH,Area, color_fg, color_bg):
	# Area = ScrollBarArea
	# point1=down/left, point2=top/right
	P1X,P1Y,P2X,P2Y = Area
	AreaH = P2X-P1X
	if PanelH > AreaH:
		Slider = int(AreaH * (AreaH / float(PanelH)))
		if Slider<3: Slider = 3 #minimal slider heigh
		posX = -int(AreaH * (Focus / float(PanelH)))
		ui_rect([P1X,P1Y,P2X,P2Y], color_bg)
		ui_rect([P1X+posX,P1Y+3,P1X+posX+Slider,P2Y-3], color_fg)

def ui_scrollbarY(Focus,PanelH,Area, color_fg, color_bg):
	# Area = ScrollBarArea
	# point1=down/left, point2=top/right
	P1X,P1Y,P2X,P2Y = Area
	AreaH = P2Y-P1Y
	if PanelH > AreaH:
		Slider = int(AreaH * (AreaH / float(PanelH)))
		if Slider<3: Slider = 3 #minimal slider heigh
		posY = -int(AreaH * (Focus / float(PanelH)))
		ui_rect([P1X,P1Y,P2X-1,P2Y], color_bg)
		#ui_rect([P1X+3,P2Y-posY,P2X-4,P2Y-posY-Slider], color_fg)
		ui_rect([P1X+3,P1Y+posY,P2X-4,P1Y+posY+Slider], color_fg)


#------------------------------------------------------------
def dxf_callback(input_filename):
	global dxfFileName
	dxfFileName.val=input_filename
#   dirname == Blender.sys.dirname(Blender.Get('filename'))
#   update_RegistryKey('DirName', dirname)
#   update_RegistryKey('dxfFileName', input_filename)
	
def ini_callback(input_filename):
	global iniFileName
	iniFileName.val=input_filename

#------------------------------------------------------------
def getSpaceRect():
	__UI_RECT__ = Buffer(GL_FLOAT, 4)
	glGetFloatv(GL_SCISSOR_BOX, __UI_RECT__)
	__UI_RECT__ = __UI_RECT__.list
	return (int(__UI_RECT__[0]), int(__UI_RECT__[1]), int(__UI_RECT__[2]), int(__UI_RECT__[3]))

def getRelMousePos(mco, winRect):
	# mco = Blender.Window.GetMouseCoords()
	if pointInRect(mco, winRect):
		return (mco[0] - winRect[0], mco[1] - winRect[1])
	return None


def pointInRect(pt, rect):
	if  rect[0] < pt[0] < rect[0]+rect[2] and\
		rect[1] < pt[1] < rect[1]+rect[3]:
		return True
	else:
		return False



#--- variables UI menu ---------------------------
mco = [0,0]  # mouse coordinaten
mbX, mbY = 0,0  # mouse buffer coordinaten
scrollW = 20 # width of scrollbar
rowH = 20 # height of menu raw
menu__H = 2 * rowH +5 # height of menu bar
headerH = 1 * rowH # height of column header bar
scroll_left = True # position of scrollbar
menu_bottom = False # position of menu
edit_mode = False # indicator/activator
iconlib_mode = False # indicator/activator
icon_maps = [] #[['blenderbuttons.png',12,25,20,21],
#['referenceicons.png',12,25,20,21]]
help_text = False # indicator/activator
menu_pan = False # indicator/activator
compact_DESIGN = True # toggle UI
showLINK = True # toggle Links
filterList=[-1,-1,-1,-1,-1]
dubbleclik_delay = 0.25

PAN_X,PAN_Y = 0,0 # pan coordinates in characters
mPAN_X,mPAN_Y = 0,0 # manu pan coordinates in characters
menu_orgX = 0
menu_orgY = 0
mPAN_Xmax = 800
mPAN_Ymax = 800


#------------------------------------------------------------
def event(evt, val):
	global mbX, mbY, UP, UP0, scroll_pan, FOCUS_fix
	global menu_bottom, scroll_left, mco
	global PAN_X, PAN_Y, PAN_X0, PAN_Y0
	global mPAN_X, mPAN_Y, mPAN_X0, mPAN_Y0, menu_pan

	#if Blender.event:
	#	print 'Blender.event:%s, evt:%s' %(Blender.event, evt) #------------

	if evt in (Draw.QKEY, Draw.ESCKEY) and not val:
		print 'DXF-Exporter  *** end ***'   #---------------------
		Draw.Exit()

	elif val:
		if evt==Draw.MIDDLEMOUSE:
			mco2 = Window.GetMouseCoords()
			relativeMouseCo = getRelMousePos(mco2, getSpaceRect())
			if relativeMouseCo != None:
				#rect = [menu__X1,menu__Y1,menu__X2,menu__Y2]
				if 1: #pointInRect(relativeMouseCo, menu__Area):
					menu_pan = True
					mPAN_X0 = mPAN_X
					mPAN_Y0 = mPAN_Y
					mco = mco2
		elif evt == Draw.MOUSEY or evt == Draw.MOUSEX:
			if menu_pan:
				mco2 = Window.GetMouseCoords()
				mbX = mco2[0]-mco[0]
				mbY = mco2[1]-mco[1]
				mPAN_X = mPAN_X0 - mbX
				mPAN_Y = mPAN_Y0 - mbY
				#print mbX, mbY #--------------------
				Draw.Redraw()
		elif evt == Draw.WHEELDOWNMOUSE:
			mPAN_Y -= 80
			Draw.Redraw()
		elif evt == Draw.WHEELUPMOUSE:
			mPAN_Y += 80
			Draw.Redraw()
	else: # = if val==False:
		if evt==Draw.LEFTMOUSE:
			scroll_pan = False
		elif evt==Draw.MIDDLEMOUSE:
			menu_pan = False

def bevent(evt):
	global config_UI, user_preset
	global CAMERA, GUI_A

	######### Manages GUI events
	if (evt==EVENT_EXIT):
		Draw.Exit()
		print 'DXF-Exporter  *** end ***'   #---------------------
	elif (evt==EVENT_CHOOSE_INI):
		Window.FileSelector(ini_callback, "INI-file Selection", '*.ini')
	elif (evt==EVENT_REDRAW):
		Draw.Redraw()
	elif (evt==EVENT_RESET):
		resetDefaultConfig()
		Draw.Redraw()
	elif (evt==EVENT_PRESET2D):
		resetDefaultConfig_2D()
		Draw.Redraw()
	elif (evt==EVENT_PRESET3D):
		resetDefaultConfig_3D()
		Draw.Redraw()
	elif evt in (EVENT_CAMERA,EVENT_LIGHT):
		CAMERA = GUI_A['camera_selected'].val
		if CAMERA==len(CAMERAS)+1:
			doAllCameras = True
		else:
			pass #print 'deb: CAMERAS=',CAMERAS #----------------
		Draw.Redraw()
	elif (evt==EVENT_setCAMERA):
		if CAMERA<len(CAMERAS)+1:
			gotoCAMERA()

	elif (evt==EVENT_SCALE):
		if g_scale_as.val == 12:
			inputGlobalScale()
		if GUI_A['g_scale'].val < 0.00000001:
			GUI_A['g_scale'].val = 0.00000001
		Draw.Redraw()
	elif (evt==EVENT_ORIGIN):
		inputOriginVector()
		Draw.Redraw()
	elif (evt==EVENT_PRESETPLINE):
		presetConfig_polyline(GUI_A['to_polyline_on'].val)
		Draw.Redraw()
	elif (evt==EVENT_PRESETS):
		user_preset += 1
		index = str(user_preset)
		if user_preset > 5: user_preset = 0; index = ''
		iniFileName.val = INIFILE_DEFAULT_NAME + index + INIFILE_EXTENSION
		Draw.Redraw()
	elif (evt==EVENT_HELP):
		try:
			import webbrowser
			webbrowser.open('http://wiki.blender.org/index.php?title=Scripts/Manual/Export/autodesk_dxf')
		except:
			Draw.PupMenu('DXF-Exporter: HELP Alert!%t|no connection to manual-page on Blender-Wiki! try:|\
http://wiki.blender.org/index.php?title=Scripts/Manual/Export/autodesk_dxf')
		Draw.Redraw()
	elif (evt==EVENT_LOAD_INI):
		loadConfig()
		Draw.Redraw()
	elif (evt==EVENT_SAVE_INI):
		saveConfig()
		Draw.Redraw()
	elif (evt==EVENT_DXF_DIR):
		dxfFile = dxfFileName.val
		dxfPathName = ''
		if '/' in dxfFile:
			dxfPathName = '/'.join(dxfFile.split('/')[:-1]) + '/'
		elif '\\' in dxfFile:
			dxfPathName = '\\'.join(dxfFile.split('\\')[:-1]) + '\\'
		dxfFileName.val = dxfPathName + '*.dxf'
#	   dirname == Blender.sys.dirname(Blender.Get('filename'))
#	   update_RegistryKey('DirName', dirname)
#	   update_RegistryKey('dxfFileName', dxfFileName.val)
		GUI_A['only_selected_on'].val = 1
		Draw.Redraw()
	elif (evt==EVENT_CHOOSE_DXF):
		filename = '' # '*.dxf'
		if dxfFileName.val: filename = dxfFileName.val
		Window.FileSelector(dxf_callback, "DXF-file Selection", filename)
	elif (evt==EVENT_START):
		dxfFile = dxfFileName.val
		#print 'deb: dxfFile file: ', dxfFile #----------------------
		if E_M: dxfFileName.val, dxfFile = e_mode(dxfFile) #evaluation mode
		update_RegistryKey('dxfFileName', dxfFileName.val)
		update_globals()
		if dxfFile.lower().endswith('*.dxf'):
			if Draw.PupMenu('DXF-Exporter:  OK?|will write multiple DXF-files, one for each Scene, in:|%s' % dxfFile) == 1:
				global UI_MODE
				UI_MODE = False
				#TODO: multi_export(dxfFile[:-5])  # cut last 5 characters '*.dxf'
				Draw.Redraw()
				UI_MODE = True
			else:
				Draw.Redraw()
		elif dxfFile.lower()[-4:] in ('.dxf','.dwg'): # and Blender.sys.exists(dxfFile):
			print 'preparing for export ---' #Standard Mode: activated
			filepath = dxfFile
			sce = Scene.GetCurrent()
			if ONLYSELECTED: sel_group = sce.objects.selected
			else: sel_group = sce.objects
				
			if ONLYVISIBLE:
				sel_group_temp = []
				layerlist = sce.getLayers()
				for ob in sel_group:
					for lay in ob.layers:
					  if lay in layerlist:
							sel_group_temp.append(ob)
							break
				sel_group = sel_group_temp
								
			export_list = getObjectsAndDuplis(sel_group,MATRICES=True)
		
			if export_list: do_export(export_list, filepath)
			else:
				print "Abort: selection was empty, no object to export!"
				Draw.PupMenu('DXF Exporter:   nothing exported!|empty selection!')
		else:
			Draw.PupMenu('DXF-Exporter:  Alert!%t|no valid DXF-file selected!')
			print "DXF-Exporter: error, no valid DXF-file selected! try again"
			Draw.Redraw()




def multi_export(DIR): #TODO:
	"""Imports all DXF-files from directory DIR.
	
	"""
	global SCENE
	batchTIME = Blender.sys.time()
	#if #DIR == "": DIR = os.path.curdir
	if DIR == "": DIR = Blender.sys.dirname(Blender.Get('filename'))
	print 'Multifiles Import from %s' %DIR
	files = \
		[Blender.sys.join(DIR, f) for f in os.listdir(DIR) if f.lower().endswith('.dxf')] 
	if not files:
		print '...None DXF-files found. Abort!'
		return
	
	i = 0
	for dxfFile in files:
		i += 1
		print '\nDXF-file', i, 'of', len(files) #,'\nImporting', dxfFile
		if ONLYSELECTED:
			_dxf_file = dxfFile.split('/')[-1].split('\\')[-1]
			_dxf_file = _dxf_file[:-4]  # cut last char:'.dxf'
			_dxf_file = _dxf_file[:NAMELENGTH_MAX]  #? [-NAMELENGTH_MAX:])
			SCENE = Blender.Scene.New(_dxf_file)
			SCENE.makeCurrent()
			#or so? Blender.Scene.makeCurrent(_dxf_file)
			#sce = bpy.data.scenes.new(_dxf_file)
			#bpy.data.scenes.active = sce
		else:
			SCENE = Blender.Scene.GetCurrent()
			SCENE.objects.selected = [] # deselect all
		main(dxfFile)
		#Blender.Redraw()

	print 'TOTAL TIME: %.6f' % (Blender.sys.time() - batchTIME)
	print '\a\r', # beep when done


#-----------------------------------------------------
if __name__=='__main__':

	if DXF:
		print '\n\n\n'
		print 'DXF-Exporter v%s *** start ***' %(__version__)   #---------------------
		print 'with Library %s' %(DXF.__version__)   #---------------------
		if not DXF.copy:
			print "DXF-Exporter: dxfLibrary.py script requires a full Python install"
			Draw.PupMenu('Error%t|The dxfLibrary.py script requires a full Python install')
		else:
			#Window.FileSelector(dxf_export_ui, 'EXPORT DXF', Blender.sys.makename(ext='.dxf'))
			# recall last used DXF-file and INI-file names
			dxffilename = check_RegistryKey('dxfFileName')
			#print 'deb:start dxffilename:', dxffilename #----------------
			if dxffilename: dxfFileName.val = dxffilename
			else:
				dirname = Blender.sys.dirname(Blender.Get('filename'))
				#print 'deb:start dirname:', dirname #----------------
				dxfFileName.val = Blender.sys.join(dirname, '')
			inifilename = check_RegistryKey('iniFileName')
			if inifilename: iniFileName.val = inifilename
		
			updateMenuCAMERA()
			updateCAMERA()
		
			Draw.Register(draw_UI, event, bevent)
			
	