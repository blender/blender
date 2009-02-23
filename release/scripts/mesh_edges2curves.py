#!BPY
""" Registration info for Blender menus:
Name: 'Edges to Curve'
Blender: 241
Group: 'Mesh'
Tip: 'Edges not used by a face are converted into polyline(s)'
"""
__author__ = ("Campbell Barton")
__url__ = ("blender", "blenderartists.org")
__version__ = "1.0 2006/02/08"

__bpydoc__ = """\
Edges to Curves

This script converts open and closed edge loops into curve polylines

Supported:<br>
	 Polylines where each vert has no more then 2 edges attached to it.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
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
# --------------------------------------------------------------------------

from Blender import *

def polysFromMesh(me):
	# a polyline is 2 
	#polylines are a list
	polyLines = []
	
	# Get edges not used by a face
	edgeDict= dict([ (ed.key, ed) for ed in me.edges ])
	for f in me.faces:
		for key in f.edge_keys:
			try:
				del edgeDict[key]
			except:
				pass
	
	edges= edgeDict.values()
	
	
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
					del edges[i]
					#break
				elif ed.v2 == endVert:
					polyLine.append(ed.v1)
					endVert= polyLine[-1]
					ok=1
					del edges[i]
					#break
				elif ed.v1 == startVert:
					polyLine.insert(0, ed.v2)
					startVert= polyLine[0]
					ok=1
					del edges[i]
					#break	
				elif ed.v2 == startVert:
					polyLine.insert(0, ed.v1)
					startVert= polyLine[0]
					ok=1
					del edges[i]
					#break
		polyLines.append((polyLine, polyLine[0]==polyLine[-1]))
		# print len(edges), len(polyLines)
	return polyLines


def mesh2polys():
	scn= Scene.GetCurrent()
	scn.objects.selected = []
	
	meshOb= scn.objects.active
	if meshOb==None or meshOb.type != 'Mesh':
		Draw.PupMenu( 'ERROR: No Active Mesh Selected, Aborting' )
		return
	Window.WaitCursor(1)
	Window.EditMode(0)
	me = meshOb.getData(mesh=1)
	polygons= polysFromMesh(me)
	w = 1.0
	cu= Curve.New()
	cu.name = me.name
	cu.setFlag(1)
	
	ob = scn.objects.active = scn.objects.new(cu)
	ob.setMatrix(meshOb.matrixWorld)
	
	i=0
	for poly, closed in polygons:
		if closed:
			vIdx= 1
		else:
			vIdx= 0
		
		v= poly[vIdx]
		cu.appendNurb((v.co.x, v.co.y, v.co.z, w))
		vIdx += 1
		cu[i].type= 0 # Poly Line
		
		# Close the polyline if its closed.
		if closed:
			cu[i].setFlagU(1)
		
		# Add all the points in the polyline.
		while vIdx<len(poly):
			v= poly[vIdx]
			cu.appendPoint(i, (v.co.x, v.co.y, v.co.z, w))
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
