#!BPY
""" Registration info for Blender menus:
Name: 'Edges to Curve'
Blender: 241
Group: 'Mesh'
Tip: 'Edges not used by a face are converted into polyline(s)'
"""
__author__ = ("Campbell Barton")
__url__ = ("blender", "elysiun")
__version__ = "1.0 2006/02/08"

__bpydoc__ = """\
This script converts open and closed edge loops into polylines

Supported:<br>
	 Polylines where each vert has no more then 2 edges attached to it.
"""

from Blender import *

def sortPair(a,b):
	return min(a,b), max(a,b)

def polysFromMesh(me):
	# a polyline is 2 
	#polylines are a list
	polyLines = []
	
	# Get edges not used by a face
	edgeDict= dict([ (sortPair(ed.v1.index, ed.v2.index), ed) for ed in me.edges ])
	for f in me.faces:
		for i in xrange(len(f.v)):
			key= sortPair(f.v[i].index, f.v[i-1].index)
			try:
				del edgeDict[key]
			except:
				pass
	edges= edgeDict.values()
	
	
	'''
	# build vert connectivite
	vertConnex= [list() for i in xrange(len(me.verts))]
	for ed in edges:
		i1= ed.v1.index
		i2= ed.v2.index
		vertConnex[i1].append(i2)
		vertConnex[i2].append(i1)
		
		
	# verts without 2 users are none.
	vertConnex=[ [None,vc][len(vc)==2] for vc in vertConnex
	'''

	
	while edges:
		currentEdge= edges.pop()
		startVert= currentEdge.v2
		endVert= currentEdge.v1
		polyLine= [startVert, endVert]
		ok= 1
		while ok:
			ok= 0
			#for i, ed in enumerate(edges):
			i=len(edges)
			while i:
				i-=1
				ed= edges[i]
				if ed.v1 == endVert:
					polyLine.append(ed.v2)
					endVert= polyLine[-1]
					ok=1
					edges.pop(i)
					#break
				elif ed.v2 == endVert:
					polyLine.append(ed.v1)
					endVert= polyLine[-1]
					ok=1
					edges.pop(i)
					#break
				elif ed.v1 == startVert:
					polyLine.insert(0, ed.v2)
					startVert= polyLine[0]
					ok=1
					edges.pop(i)
					#break	
				elif ed.v2 == startVert:
					polyLine.insert(0, ed.v1)
					startVert= polyLine[0]
					ok=1
					edges.pop(i)
					#break
		polyLines.append((polyLine, polyLine[0]==polyLine[-1]))
		print len(edges), len(polyLines)
	return polyLines


def mesh2polys():
	scn= Scene.GetCurrent()
	for ob in scn.getChildren():
		ob.sel= 0
	meshOb= scn.getActiveObject()
	if meshOb==None or meshOb.getType() != 'Mesh':
		Draw.PupMenu( 'ERROR: No Active Mesh Selected, Aborting' )
		return
	Window.WaitCursor(1)
	Window.EditMode(0)
	me = meshOb.getData(mesh=1)
	polygons= polysFromMesh(me)
	w=t=1
	cu= Curve.New()
	cu.setFlag(1)
	ob= Object.New('Curve', me.name)
	ob.link(cu)
	scn.link(ob)
	ob.Layers= meshOb.Layers
	ob.setMatrix(meshOb.matrixWorld)
	ob.sel= 1
	i=0
	for poly, closed in polygons:
		if closed:
			vIdx= 1
		else:
			vIdx= 0
		
		v= poly[vIdx]
		cu.appendNurb([v.co.x, v.co.y, v.co.z, w, t])
		cu[i].type= 0 # Poly Line
		
		# Close the polyline if its closed.
		if closed:
			cu[i].setFlagU(1)
		
		# Add all the points in the polyline.
		while vIdx<len(poly):
			v= poly[vIdx]
			cu.appendPoint(i, [v.co.x, v.co.y, v.co.z, w])
			vIdx+=1
		i+=1
	Window.WaitCursor(0)

# not used as yet.
"""
def writepolys():
	me = Scene.GetCurrent().getActiveObject().getData(mesh=1)
	polygons= polysFromMesh(me)
	file=open('/polygons.txt', 'w')
	for ply in polygons:
		file.write('polygon ')
		if ply[1]:
			file.write('closed ')
		else:
			file.write('open ')
		file.write('%i\n' % len(ply[0]))
		for pt in ply[0]:
			file.write('%.6f %.6f %.6f\n' % tuple(pt.co) )
	file.close()
"""

if __name__ == '__main__':
	mesh2polys()
