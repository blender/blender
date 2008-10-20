#!BPY

"""
 Name: 'Autodesk DXF (.dxf)'
 Blender: 247
 Group: 'Export'
 Tooltip: 'Export geometry to DXF-r12 (Drawing eXchange Format).'
"""

__version__ = "v1.27beta - 2008.10.07"
__author__  = "Remigiusz Fiedler (AKA migius)"
__license__ = "GPL"
__url__	 = "http://wiki.blender.org/index.php/Scripts/Manual/Export/autodesk_dxf"
__bpydoc__ ="""The script exports Blender geometry to DXF format r12 version.

Version %s
Copyright %s
License %s

extern dependances: dxfLibrary.py

See the homepage for documentation.
url: %s

IDEAs:
 - correct normals for POLYLINE-POLYFACE via proper point-order
 - HPGL output for 2d and flattened 3d content
		
TODO:
- optimize back-faces removal (probably needs matrix transform)
- optimize POLYFACE routine: remove double-vertices
- optimize POLYFACE routine: remove unused vertices
- support hierarchies: groups, instances, parented structures
- support 210-code (3d orientation vector)
- presets for architectural scales
- write drawing extends for automatic view positioning in CAD

History
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
from Blender import Mathutils, Window, Scene, sys, Draw
import BPyMessages

#import dxfLibrary
#reload(dxfLibrary)
from  dxfLibrary import *


#-----------------------------------------------------
def hidden_status(faces, mx_n):
	#print 'HIDDEN_MODE: caution! not full implemented yet'
	ok_faces = []
	ok_edges = []
	#sort out back-faces = with normals pointed away from camera
	for f in faces:
		#print 'deb: face=', f #---------
		# get its normal-vector in localCS
		vec_normal = f.no.copy()
		#print 'deb: vec_normal=', vec_normal #------------------
		#must be transfered to camera/view-CS
		vec_normal *= mx_n
		#vec_normal *= mb.rotationPart()
		#print 'deb:2vec_normal=', vec_normal #------------------
		#vec_normal *= mw0.rotationPart()
		#print 'deb:3vec_normal=', vec_normal, '\n' #------------------

		# normal must point the Z direction-hemisphere
		if vec_normal[2] > 0.0 :
			ok_faces.append(f.index)
			for key in f.edge_keys:
				#this test can be done faster with set()
				if key not in ok_edges:
					 ok_edges.append(key)
	#print 'deb: amount of visible faces=', len(ok_faces) #---------
	#print 'deb: visible faces=', ok_faces #---------
	#print 'deb: amount of visible edges=', len(ok_edges) #---------
	#print 'deb: visible edges=', ok_edges #---------
	return ok_faces, ok_edges


#-----------------------------------------------------
def projected_co(vec, mw):
	# convert the world coordinates of vector to screen coordinates
	#co = vec.co.copy().resize4D()
	co = vec.copy().resize4D()
	co[3] = 1.0
	sc = co * mw
	#print 'deb: viewprojection=', sc #---------
	return [sc[0],sc[1],0.0]


#--------not used---------------------------------------------
def flatten(points, mw):
	for i,v in enumerate(points):
		v = projected_co(v, mw)
		points[i]=v
	#print 'deb: flatten points=', points #---------
	return points

#-----------------------------------------------------
def	exportMesh(ob, mx, mx_n):
	entities = []
	me = ob.getData(mesh=1)
	#me.transform(mx)
	# above is eventualy faster, but bad, cause
	# directly transforms origin geometry and write back rounding errors
	me_verts = me.verts[:] #we dont want manipulate origin data
	#print 'deb: me_verts=', me_verts #---------
	#me.transform(mx_inv) #counterpart to - back to the origin state
	for v in me_verts:
		v.co *= mx
	faces=[]
	edges=[]
	if HIDDEN_MODE:
		ok_faces, ok_edges = hidden_status(me.faces, mx_n)

	#if (not FLATTEN) and len(me.faces)>0 and ONLYFACES:
	if ONLYFACES:
		if POLYFACES: #export 3D as POLYFACEs
			allpoints = []
			allfaces = []
			allpoints =  [v.co[:3] for v in me_verts]
			for f in me.faces:
				#print 'deb: face=', f #---------
				if not HIDDEN_MODE or \
					(HIDDEN_MODE and f.index in ok_faces):
					if 1:
						points = f.verts
						face = [p.index+1 for p in points]
						#print 'deb: face=', face #---------
						allfaces.append(face)
					else: #bad, cause create multiple vertex instances
						points  = f.verts
						points = [ me_verts[p.index].co[:3] for p in points]
						#points = [p.co[:3] for p in points]
						#print 'deb: points=', points #---------
						index = len(allpoints)+1
						face = [index+i for i in range(len(points))]
						allpoints.extend(points)
						allfaces.append(face)
			if allpoints and allfaces:
				#print 'deb: allpoints=', allpoints #---------
				#print 'deb: allfaces=', allfaces #---------
				dxfPOLYFACE = PolyLine([allpoints, allfaces], flag=64)
				entities.append(dxfPOLYFACE)
		else: #export 3D as 3DFACEs
			for f in me.faces:
				#print 'deb: face=', f #---------
				if not HIDDEN_MODE or \
					(HIDDEN_MODE and f.index in ok_faces):
					points  = f.verts
					points = [ me_verts[p.index].co[:3] for p in points]
					#points = [p.co[:3] for p in points]
					#print 'deb: points=', points #---------
					dxfFACE = Face(points)
					entities.append(dxfFACE)

	else:	#export 3D as LINEs
		if HIDDEN_MODE and len(me.faces)!=0:
			for e in ok_edges:
				points = [ me_verts[key].co[:3] for key in e]
				dxfLINE = Line(points)
				entities.append(dxfLINE)
				
		else:
			for e in me.edges:
				#print 'deb: edge=', e #---------
				points=[]
				#points = [e.v1.co*mx, e.v2.co*mx]
				points = [ me_verts[key].co[:3] for key in e.key]
				#print 'deb: points=', points #---------
				dxfLINE = Line(points)
				entities.append(dxfLINE)
	return entities


#-----------------------------------------------------
def exportCurve(ob, mx):
	entities = []
	curve = ob.getData()
	for cur in curve:
		#print 'deb: START cur=', cur #--------------
		if 1: #not cur.isNurb():
			#print 'deb: START points' #--------------
			points = []
			org_point = [0.0,0.0,0.0]
			for point in cur:
				#print 'deb: point=', point #---------
				if cur.isNurb():
					vec = point[0:3]
				else:
					point = point.getTriple()
					#print 'deb: point=', point #---------
					vec = point[1]
				#print 'deb: vec=', vec #---------
				pkt = Mathutils.Vector(vec) * mx
				#print 'deb: pkt=', pkt #---------
				#pkt *= SCALE_FACTOR
				if 0: #FLATTEN:
					pkt = projected_co(pkt, mw)
				points.append(pkt)
			if cur.isCyclic(): closed = 1
			else: closed = 0
			if len(points)>1:
				#print 'deb: points', points #--------------
				if POLYLINES: dxfPLINE = PolyLine(points,org_point,closed)
				else: dxfPLINE = LineList(points,org_point,closed)
				entities.append(dxfPLINE)
	return entities

#-----------------------------------------------------
def do_export(sel_group, filepath):
	Window.WaitCursor(1)
	t = sys.time()

	#init Drawing ---------------------
	d=Drawing()
	#add Tables -----------------
	#d.blocks.append(b)					#table blocks
	d.styles.append(Style())			#table styles
	d.views.append(View('Normal'))		#table view
	d.views.append(ViewByWindow('Window',leftBottom=(1,0),rightTop=(2,1)))  #idem

	#add Entities --------------------
	something_ready = False
	#ViewVector = Mathutils.Vector(Window.GetViewVector())
	#print 'deb: ViewVector=', ViewVector #------------------
	mw0 = Window.GetViewMatrix()
	#mw0 = Window.GetPerspMatrix() #TODO: how get it working?
	mw = mw0.copy()
	if FLATTEN:
		m0 = Mathutils.Matrix()
		m0[2][2]=0.0
		mw *= m0 #flatten ViewMatrix

	for ob in sel_group:
		entities = []
		mx = ob.matrix.copy()
		mb = mx.copy()
		#print 'deb: mb    =\n', mb     #---------
		#print 'deb: mw0    =\n', mw0     #---------
		mx_n = mx.rotationPart() * mw0.rotationPart() #trans-matrix for normal_vectors
		if SCALE_FACTOR!=1.0: mx *= SCALE_FACTOR
		if FLATTEN:	mx *= mw
			
		#mx_inv = mx.copy().invert()
		#print 'deb: mx    =\n', mx     #---------
		#print 'deb: mx_inv=\n', mx_inv #---------

		if (ob.type == 'Mesh'):
			entities = exportMesh(ob, mx, mx_n)
		elif (ob.type == 'Curve'):
			entities = exportCurve(ob, mx)

		for e in entities:
			d.append(e)
			something_ready = True

	if something_ready:
		d.saveas(filepath)
		Window.WaitCursor(0)
		#Draw.PupMenu('DXF Exporter: job finished')
		print 'exported to %s' % filepath
		print 'finished in %.2f seconds' % (sys.time()-t)
	else:
		Window.WaitCursor(0)
		print "Abort: selected objects dont mach choosen export option, nothing exported!"
		Draw.PupMenu('DXF Exporter:   nothing exported!|selected objects dont mach choosen export option!')

#----globals------------------------------------------
ONLYSELECTED = True
POLYLINES = True
ONLYFACES = False
POLYFACES = 1
FLATTEN = 0
HIDDEN_MODE = False #filter out hidden lines
SCALE_FACTOR = 1.0 #optional, can be done later in CAD too



#-----------------------------------------------------
def dxf_export_ui(filepath):
	global	ONLYSELECTED,\
	POLYLINES,\
	ONLYFACES,\
	POLYFACES,\
	FLATTEN,\
	HIDDEN_MODE,\
	SCALE_FACTOR

	print '\n\nDXF-Export %s -----------------------' %__version__
	#filepath = 'blend_test.dxf'
	# Dont overwrite
	if not BPyMessages.Warning_SaveOver(filepath):
		print 'Aborted by user: nothing exported'
		return
	#test():return


	PREF_ONLYSELECTED= Draw.Create(ONLYSELECTED)
	PREF_POLYLINES= Draw.Create(POLYLINES)
	PREF_ONLYFACES= Draw.Create(ONLYFACES)
	PREF_POLYFACES= Draw.Create(POLYFACES)
	PREF_FLATTEN= Draw.Create(FLATTEN)
	PREF_HIDDEN_MODE= Draw.Create(HIDDEN_MODE)
	PREF_SCALE_FACTOR= Draw.Create(SCALE_FACTOR)
	PREF_HELP= Draw.Create(0)
	block = [\
	("only selected", PREF_ONLYSELECTED, "export only selected geometry"),\
	("global Scale:", PREF_SCALE_FACTOR, 0.001, 1000, "set global Scale factor for exporting geometry"),\
	("only faces", PREF_ONLYFACES, "from mesh-objects export only faces, otherwise only edges"),\
	("write POLYFACE", PREF_POLYFACES, "export mesh to POLYFACE, otherwise to 3DFACEs"),\
	("write POLYLINEs", PREF_POLYLINES, "export curve to POLYLINE, otherwise to LINEs"),\
	("3D-View to Flat", PREF_FLATTEN, "flatten geometry according current 3d-View"),\
	("Hidden Mode", PREF_HIDDEN_MODE, "filter out hidden lines"),\
	#(''),\
	("online Help", PREF_HELP, "calls DXF-Exporter Manual Page on Wiki.Blender.org"),\
	]
	
	if not Draw.PupBlock("DXF-Exporter %s" %__version__[:10], block): return

	if PREF_HELP.val!=0:
		try:
			import webbrowser
			webbrowser.open('http://wiki.blender.org/index.php?title=Scripts/Manual/Export/autodesk_dxf')
		except:
			Draw.PupMenu('DXF Exporter: %t|no connection to manual-page on Blender-Wiki!	try:|\
http://wiki.blender.org/index.php?title=Scripts/Manual/Export/autodesk_dxf')
		return

	ONLYSELECTED = PREF_ONLYSELECTED.val
	POLYLINES = PREF_POLYLINES.val
	ONLYFACES = PREF_ONLYFACES.val
	POLYFACES = PREF_POLYFACES.val
	FLATTEN = PREF_FLATTEN.val
	HIDDEN_MODE = PREF_HIDDEN_MODE.val
	SCALE_FACTOR = PREF_SCALE_FACTOR.val

	sce = Scene.GetCurrent()
	if ONLYSELECTED: sel_group = sce.objects.selected
	else: sel_group = sce.objects

	if sel_group: do_export(sel_group, filepath)
	else:
		print "Abort: selection was empty, no object to export!"
		Draw.PupMenu('DXF Exporter:   nothing exported!|empty selection!')
	# Timing the script is a good way to be aware on any speed hits when scripting



#-----------------------------------------------------
if __name__=='__main__':
	#main()
	if not copy:
		Draw.PupMenu('Error%t|This script requires a full python install')
	Window.FileSelector(dxf_export_ui, 'EXPORT DXF', sys.makename(ext='.dxf'))
	
	
	