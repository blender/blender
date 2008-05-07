#!BPY
"""
Name: 'xfig export (.fig)'
Blender: 244
Group: 'Export'
Tooltip: 'Export selected mesh to xfig Format (.fig)'
"""

__author__ = 'Dino Ghilardi',  'Campbell Barton AKA Ideasman42'
__url__ = ("blender", "blenderartists.org")
__version__ = "1.1"

__bpydoc__ = """\
		This script exports the selected mesh to xfig (www.xfig.org) file format (i.e.: .fig)

		The starting point of this script was Anthony D'Agostino's raw triangle format export.
		(some code is still here and there, cut'n pasted from his script)

		Usage:<br>
			Select the mesh to be exported and run this script from "File->Export" menu.
			The toggle button 'export 3 files' enables the generation of 4 files: one global
			and three with the three different views of the object.
			This script is licensed under the GPL license. (c) Dino Ghilardi, 2005
			
"""

# .fig export, mostly brutally cut-n pasted from the 
# 'Raw triangle export' (Anthony D'Agostino, http://www.redrival.com/scorpius)|

import Blender
from Blender import Draw
import BPyObject
#, meshtools
import sys
import bpy
#import time

# =================================
# === Write xfig Format.===
# =================================

def collect_edges(edges):
	"""Gets the max-min coordinates of the mesh"""
	
	"""Getting the extremes of the mesh to be exported"""
	
	maxX=maxY=maxZ = -1000000000
	minX=minY=minZ =  1000000000
	
	FGON= Blender.Mesh.EdgeFlags.FGON
	
	me = bpy.data.meshes.new()
	for ob_base in bpy.data.scenes.active.objects.context:
		for ob in BPyObject.getDerivedObjects(ob_base):
			me.verts = None
			try:	me.getFromObject(ob[0])
			except: pass
			
			if me.edges:
				me.transform(ob[1])
				
				for ed in me.edges:
					if not ed.flag & FGON:
						x,y,z = v1 = tuple(ed.v1.co)
						maxX = max(maxX, x)
						maxY = max(maxY, y)
						maxZ = max(maxZ, z)
						minX = min(minX, x)
						minY = min(minY, y)
						minZ = min(minZ, z)
						
						x,y,z = v2 = tuple(ed.v2.co)
						maxX = max(maxX, x)
						maxY = max(maxY, y)
						maxZ = max(maxZ, z)
						minX = min(minX, x)
						minY = min(minY, y)
						minZ = min(minZ, z)
						
						edges.append( (v1, v2) )
					
	me.verts = None # free memory
	return maxX,maxY,maxZ,minX,minY,minZ

def xfigheader(file):
	file.write('#FIG 3.2  Produced by xfig version 3.2.5-alpha5\n')
	file.write('Landscape\n')
	file.write('Center\n')
	file.write('Metric\n')
	file.write('A4\n')
	file.write('100.00\n')
	file.write('Single\n')
	file.write('-2\n')
	file.write('1200 2\n')

def figdata(file, edges, expview, bounds, scale, space):
	maxX,maxY,maxZ,minX,minY,minZ = bounds
	
	def xytransform(ed):
		"""gives the face vertexes coordinates in the xfig format/translation (view xy)"""	
		x1,y1,z1 = ed[0]
		x2,y2,z2 = ed[1]
		y1=-y1; y2=-y2
		return x1,y1,z1,x2,y2,z2

	def xztransform(ed):
		"""gives the face vertexes coordinates in the xfig format/translation (view xz)"""
		x1,y1,z1 = ed[0]
		x2,y2,z2 = ed[1]
		y1=-y1
		y2=-y2
		
		z1=-z1+maxZ-minY +space
		z2=-z2+maxZ-minY +space
		return x1,y1,z1,x2,y2,z2

	def yztransform(ed):
		"""gives the face vertexes coordinates in the xfig format/translation (view xz)"""
		x1,y1,z1 = ed[0]
		x2,y2,z2 = ed[1]
		y1=-y1; y2=-y2
		z1=-(z1-maxZ-maxX-space)
		z2=-(z2-maxZ-maxX-space)
		return x1,y1,z1,x2,y2,z2

	def transform(ed, expview, scale):
		if		expview=='xy':
			x1,y1,z1,x2,y2,z2 = xytransform(ed)
			return int(x1*scale),int(y1*scale),int(x2*scale),int(y2*scale)
		elif	expview=='xz':
			x1,y1,z1,x2,y2,z2 = xztransform(ed)
			return int(x1*scale),int(z1*scale),int(x2*scale),int(z2*scale)
		elif	expview=='yz':
			x1,y1,z1,x2,y2,z2 = yztransform(ed)
			return int(z1*scale),int(y1*scale),int(z2*scale),int(y2*scale)
	
	
	"""Prints all the xfig data (no header)"""
	for ed in edges:
		file.write('2 1 0 1 0 7 50 -1 -1 0.000 0 0 -1 0 0 2\n')
		file.write('\t %i %i %i %i\n' % transform(ed, expview, scale))

def writexy(edges, bounds, filename, scale, space):
	"""writes the x-y view file exported"""
	
	file = open(filename, 'wb')
	xfigheader(file)
	figdata(file, edges, 'xy', bounds, scale, space)
	file.close()
	print 'Successfully exported ', Blender.sys.basename(filename)# + seconds

def writexz(edges, bounds, filename, scale, space):
	"""writes the x-z view file exported"""
	#start = time.clock()
	file = open(filename, 'wb')
	xfigheader(file)
	figdata(file, edges, 'xz', bounds, scale, space)
	file.close()
	print 'Successfully exported ', Blender.sys.basename(filename)# + seconds

def writeyz(edges, bounds, filename, scale, space):
	"""writes the y-z view file exported"""
	
	#start = time.clock()
	file = open(filename, 'wb')
	xfigheader(file)
	figdata(file, edges, 'yz', bounds, scale, space)
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	print 'Successfully exported ', Blender.sys.basename(filename)# + seconds

def writeall(edges, bounds, filename, scale=450, space=2.0):
	"""writes all 3 views
	
	Every view is a combined object in the resulting xfig. file."""
	
	maxX,maxY,maxZ,minX,minY,minZ = bounds
	
	file = open(filename, 'wb')

	xfigheader(file)
	file.write('#upper view (7)\n')
	file.write('6 % i % i % i % i ')
	file.write('%.6f %.6f %.6f %.6f\n' % (minX, minY, maxX, maxY))
	
	figdata(file, edges, 'xy', bounds, scale, space)
	file.write('-6\n')
	file.write('#bottom view (1)\n')
	file.write('6 %i %i %i %i ')
	file.write('%.6f %.6f %.6f %.6f\n' % (minX, -minZ+maxZ-minY +space, maxX,-maxZ+maxZ-minY +space))
	
	figdata(file, edges, 'xz', bounds, scale, space)
	file.write('-6\n')
	
	file.write('#right view (3)\n')
	file.write('6 %i %i %i %i ')
	file.write('%.6f %.6f %.6f %.6f\n' % (minX, -minZ+maxZ-minY +space, maxX,-maxZ+maxZ-minY +space))
	figdata(file, edges, 'yz', bounds, scale, space)
	file.write('-6\n')
	
	file.close()
	print 'Successfully exported ', Blender.sys.basename(filename)# + seconds

import BPyMessages

def write_ui(filename):
	if filename.lower().endswith('.fig'): filename = filename[:-4]
	
	PREF_SEP= Draw.Create(0)
	PREF_SCALE= Draw.Create(1200)
	PREF_SPACE= Draw.Create(2.0)
	
	block = [\
		("Separate Files", PREF_SEP, "Export each view axis as a seperate file"),\
		("Space: ", PREF_SPACE, 0.0, 10.0, "Space between views in blender units"),\
		("Scale: ", PREF_SCALE, 10, 100000, "Scale, 1200 is a good default")]
	
	if not Draw.PupBlock("Export FIG", block):
		return
	
	edges = []
	bounds = collect_edges(edges)
	
	if PREF_SEP.val:
		writexy(edges, bounds, filename + '_XY.fig', PREF_SCALE.val, PREF_SPACE.val)
		writexz(edges, bounds, filename + '_XZ.fig', PREF_SCALE.val, PREF_SPACE.val)
		writeyz(edges, bounds, filename + '_YZ.fig', PREF_SCALE.val, PREF_SPACE.val)
	
	writeall(edges, bounds, filename + '.fig', PREF_SCALE.val, PREF_SPACE.val)

if __name__ == '__main__':
	Blender.Window.FileSelector(write_ui, 'Export XFIG', Blender.sys.makename(ext='.fig'))
	