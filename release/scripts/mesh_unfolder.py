#!BPY
"""
Name: 'Unfold'
Blender: 245
Group: 'Mesh'
Tip: 'Unfold meshes to create nets'
Version:  v2.5
Author: Matthew Chadwick
"""
import Blender
from Blender import *
from Blender.Mathutils import *
try:
	import sys
	import traceback
	import math
	import re
	from math import *
	import sys
	import random
	import xml.sax, xml.sax.handler, xml.sax.saxutils
	
	# annoying but need so classes dont raise errors
	xml_sax_handler_ContentHandler = xml.sax.handler.ContentHandler

except:
	Draw.PupMenu('Error%t|A full python installation is required to run this script.')
	xml = None
	xml_sax_handler_ContentHandler = type(0)

__author__ = 'Matthew Chadwick'
__version__ = '2.5 06102007'
__url__ = ["http://celeriac.net/unfolder/", "blender", "blenderartist"]
__email__ = ["post at cele[remove this text]riac.net", "scripts"]
__bpydoc__ = """\

Mesh Unfolder

Unfolds the selected mesh onto a plane to form a net

Not all meshes can be unfolded

Meshes must be free of holes, 
isolated edges (not part of a face), twisted quads and other rubbish.
Nice clean triangulated meshes unfold best

This program is free software; you can distribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation; version 2
or later, currently at http://www.gnu.org/copyleft/gpl.html

The idea came while I was riding a bike.
"""	

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

# Face lookup
class FacesAndEdges:
	def __init__(self, mesh):
		self.nfaces = 0
		# straight from the documentation
		self.edgeFaces = dict([(edge.key, []) for edge in mesh.edges])
		for face in mesh.faces:
			face.sel = False
			for key in face.edge_keys:
					self.edgeFaces[key].append(face)
	def findTakenAdjacentFace(self, bface, edge):
		return self.findAdjacentFace(bface, edge)
	# find the first untaken (non-selected) adjacent face in the list of adjacent faces for the given edge (allows for manifold meshes too)
	def findAdjacentFace(self, bface, edge):
		faces = self.edgeFaces[edge.key()]
		for i in xrange(len(faces)):
			if faces[i] == bface:
				j = (i+1) % len(faces)
				while(faces[j]!=bface):
					if faces[j].sel == False:
						return faces[j]
					j = (j+1) % len(faces)
		return None
	def returnFace(self, face):
		face.sel = False
		self.nfaces-=1
	def facesTaken(self):
		return self.nfaces
	def takeAdjacentFace(self, bface, edge):
		if (edge==None):
			return None
		face = self.findAdjacentFace(bface, edge)
		if(face!=None):
			face.sel = True
			self.nfaces+=1
			return face
	def takeFace(self, bface):
		if(bface!=None):
			bface.sel= True
			self.nfaces+=1

	
# A fold between two faces with a common edge
class Fold:
	ids = -1
	def __init__(self, parent, refPoly, poly, edge, angle=None):
		Fold.ids+=1
		self.id = Fold.ids
		self.refPoly = refPoly
		self.poly = poly
		self.srcFace = None
		self.desFace = None
		self.edge = edge
		self.foldedEdge = edge
		self.rm = None
		self.parent = parent
		self.tree = None
		if(refPoly!=None):
			self.refPolyNormal = refPoly.normal()
		self.polyNormal = poly.normal()
		if(angle==None):
			self.angle = self.calculateAngle()
			self.foldingPoly = poly.rotated(edge, self.angle)
		else:
			self.angle = angle
			self.foldingPoly = poly
		self.unfoldedEdge = self.edge
		self.unfoldedNormal = None
		self.animAngle = self.angle
		self.cr = None
		self.nancestors = None
	def reset(self):
		self.foldingPoly = self.poly.rotated(self.edge, self.dihedralAngle())
	def getID(self):
		return self.id
	def getParent(self):
		return self.parent
	def ancestors(self):
		if(self.nancestors==None):
			self.nancestors = self.computeAncestors()
		return self.nancestors
	def computeAncestors(self):	
		if(self.parent==None):
			return 0
		else:
			return self.parent.ancestors()+1
	def dihedralAngle(self):	
		return self.angle
	def unfoldTo(self, f):
		self.animAngle = self.angle*f
		self.foldingPoly = self.poly.rotated(self.edge, self.animAngle)
	def calculateAngle(self):
		sangle = Mathutils.AngleBetweenVecs(self.refPolyNormal, self.polyNormal)
		if(sangle!=sangle):
			sangle=0.0
		ncp = self.refPolyNormal.cross(self.polyNormal)
		dp = ncp.dot(self.edge.vector)
		if(dp>0.0):
			return +sangle
		else:
			return -sangle
	def alignWithParent(self):
		pass
	def unfoldedNormal(self):
		return self.unfoldedNormal
	def getEdge(self):
		return self.edge
	def getFace(self):
		return self.poly
	def testFace(self):
		return Poly.fromVectors([self.edge.v1, self.edge.v2, Vector([0,0,0])])
	def unfoldedFace(self):
		return self.foldingPoly
	def unfold(self):
		if(self.parent!=None):
			self.parent.foldFace(self)
	def foldFace(self, child):
		child.foldingPoly.rotate(self.edge, self.animAngle)		
		if(self.parent!=None):
			self.parent.foldFace(child)
			
class Cut(Fold):
	pass
			
# Trees build folds by traversing the mesh according to a local measure
class Tree:
	def __init__(self, net, parent,fold,otherConstructor=None):
		self.net = net
		self.fold = fold
		self.face = fold.srcFace
		self.poly = Poly.fromBlenderFace(self.face)
		self.generations = net.generations
		self.growing = True
		self.tooLong = False
		self.parent = parent
		self.grown = False
		if not(otherConstructor):
			self.edges = net.edgeIteratorClass(self)
	def goodness(self):
		return self.edges.goodness()
	def compare(self, other):
		if(self.goodness() > other.goodness()):
			return +1
		else:
			return -1
	def isGrowing(self):
		return self.growing
	def beGrowing(self):
		self.growing = True
	def grow(self):
		self.tooLong = self.fold.ancestors()>self.generations
		if(self.edges.hasNext() and self.growing):
			edge = self.edges.next()
			tface = self.net.facesAndEdges.takeAdjacentFace(self.face, edge)
			if(tface!=None):
				self.branch(tface, edge)
			if(self.parent==None):
				self.grow()
		else:
			self.grown = True
	def isGrown(self):
		return self.grown
	def canGrow(self):
		return (self.parent!=None and self.parent.grown)
	def getNet(self):
		return self.net
	def getFold(self):
		return self.fold
	def getFace(self):
		return self.face
	def branch(self, tface, edge):
		fold = Fold(self.fold, self.poly, Poly.fromBlenderFace(tface), edge)
		fold.srcFace = tface
		self.net.myFacesVisited+=1
		tree = Tree(self.net, self, fold)
		fold.tree = tree
		fold.unfold()
		overlaps = self.net.checkOverlaps(fold)
		nc = len(overlaps)
		self.net.overlaps+=nc
		if(nc>0 and self.net.avoidsOverlaps):
			self.handleOverlap(fold, overlaps)
		else:
			self.addFace(fold)
	def handleOverlap(self, fold, overlaps):
		self.net.facesAndEdges.returnFace(fold.srcFace)
		self.net.myFacesVisited-=1
		for cfold in overlaps:
			ttree = cfold.tree
			ttree.growing = True
			ttree.grow()
	def addFace(self, fold):
		ff = fold.unfoldedFace()
		fold.desFace = self.net.addFace(ff, fold.srcFace)
		self.net.folds.append(fold)
		self.net.addBranch(fold.tree)
		fold.tree.growing = not(self.tooLong)
		if(self.net.diffuse==False):
			fold.tree.grow()

# A Net is the result of the traversal of the mesh by Trees
class Net:
	def __init__(self, src, des):
		self.src = src
		self.des = des
		self.firstFace = None
		self.firstPoly = None
		self.refFold = None
		self.edgeIteratorClass = RandomEdgeIterator
		if(src!=None):
			self.srcFaces = src.faces
			self.facesAndEdges = FacesAndEdges(self.src)
		self.myFacesVisited = 0
		self.facesAdded = 0
		self.folds = []
		self.cuts = []
		self.branches = []
		self.overlaps = 0
		self.avoidsOverlaps = True
		self.frame = 1
		self.ff = 180.0
		self.firstFaceIndex = None
		self.trees = 0
		self.foldIPO = None
		self.perFoldIPO = None
		self.IPOCurves = {}
		self.generations = 128
		self.diffuse = True
		self.noise = 0.0
		self.grownBranches = 0
		self.assignsUV = True
		self.animates = False
		self.showProgress = False
		self.feedback = None
	def setSelectedFaces(self, faces):
		self.srcFaces = faces
		self.facesAndEdges = FacesAndEdges(self.srcFaces)
	def setShowProgress(self, show):
		self.showProgress = show
	# this method really needs work
	def unfold(self):
		selectedFaces = [face for face in self.src.faces if (self.src.faceUV and face.sel)]
		if(self.avoidsOverlaps):
			print "unfolding with overlap detection"
		if(self.firstFaceIndex==None):
			self.firstFaceIndex = random.randint(0, len(self.src.faces)-1)
		else:
			print "Using user-selected seed face ", self.firstFaceIndex
		self.firstFace = self.src.faces[self.firstFaceIndex]
		z = min([v.co.z for v in self.src.verts])-0.1
		ff = Poly.fromBlenderFace(self.firstFace)
		if(len(ff.v)<3):
			raise Exception("This mesh contains an isolated edge - it must consist only of faces")
		testFace = Poly.fromVectors( [ Vector([0.0,0.0,0.0]), Vector([0.0,1.0,0.0]), Vector([1.0,1.0,0.0])  ] )
		# hmmm. I honestly can't remember why this needs to be done, but it does.
		u=0
		v=1
		w=2
		if ff.v[u].x==ff.v[u+1].x and ff.v[u].y==ff.v[u+1].y:
			u=1
			v=2
			w=0
		# here we make a couple of folds, not part of the net, which serve to get the net into the xy plane
		xyFace = Poly.fromList( [ [ff.v[u].x,ff.v[u].y, z] , [ff.v[v].x,ff.v[v].y, z] , [ff.v[w].x+0.1,ff.v[w].y+0.1, z] ] )
		refFace = Poly.fromVectors([ ff.v[u], ff.v[v], xyFace.v[1], xyFace.v[0] ] )
		xyFold =  Fold(None,   xyFace, refFace, Edge(xyFace.v[0], xyFace.v[1] ))
		self.refFold = Fold(xyFold, refFace, ff,         Edge(refFace.v[0], refFace.v[1] ))
		self.refFold.srcFace = self.firstFace
		# prepare to grow the trees
		trunk = Tree(self, None, self.refFold)
		trunk.generations = self.generations
		self.firstPoly = ff
		self.facesAndEdges.takeFace(self.firstFace)
		self.myFacesVisited+=1
		self.refFold.unfold()
		self.refFold.tree = trunk
		self.refFold.desFace = self.addFace(self.refFold.unfoldedFace(), self.refFold.srcFace)
		self.folds.append(self.refFold)
		trunk.grow()
		i = 0
		# keep the trees growing while they can
		while(self.myFacesVisited<len(self.src.faces) and len(self.branches) > 0):
			if self.edgeIteratorClass==RandomEdgeIterator:
				i = random.randint(0,len(self.branches)-1)
			tree = self.branches[i]
			if(tree.isGrown()):
				self.branches.pop(i)
			else:
				tree.beGrowing()
				if(tree.canGrow()):
					tree.grow()
					i = 0
				else:
					i = (i + 1) % len(self.branches)
		if self.src.faceUV:
			for face in self.src.faces:
				face.sel = False
			for face in selectedFaces:
				face.sel = True
		self.src.update()
		Window.RedrawAll()
	def assignUVs(self):
		for fold in self.folds:
			self.assignUV(fold.srcFace, fold.unfoldedFace())
		print " assigned uv to ", len(self.folds), len(self.src.faces)
		self.src.update()
	def checkOverlaps(self, fold):
		#return self.getOverlapsBetween(fold, self.folds)
		return self.getOverlapsBetweenGL(fold, self.folds)
	def getOverlapsBetween(self, fold, folds):
		if(fold.parent==None):
			return []
		mf = fold.unfoldedFace()
		c = []
		for afold in folds:
			mdf = afold.unfoldedFace()
			if(afold!=fold):
				# currently need to get agreement from both polys because
				# a touch by a vertex of one the other's edge is acceptable &
				# they disagree on that
				intersects = mf.intersects2D(mdf) and mdf.intersects2D(mf)
				inside = ( mdf.containsAnyOf(mf) or mf.containsAnyOf(mdf) )
				if(  intersects or inside or mdf.overlays(mf)):
					c.append(afold)
		return c
	def getOverlapsBetweenGL(self, fold, folds):
		b = fold.unfoldedFace().bounds()
		polys = len(folds)*4+16 # the buffer is nhits, mindepth, maxdepth, name
		buffer = BGL.Buffer(BGL.GL_INT, polys)
		BGL.glSelectBuffer(polys, buffer)
		BGL.glRenderMode(BGL.GL_SELECT)
		BGL.glInitNames()
		BGL.glPushName(0)
		BGL.glPushMatrix()
		BGL.glMatrixMode(BGL.GL_PROJECTION)
		BGL.glLoadIdentity()
		BGL.glOrtho(b[0].x, b[1].x, b[1].y, b[0].y, 0.0, 10.0)
		#clip = BGL.Buffer(BGL.GL_FLOAT, 4)
		#clip.list = [0,0,0,0]
		#BGL.glClipPlane(BGL.GL_CLIP_PLANE1, clip)
		# could use clipping planes here too
		BGL.glMatrixMode(BGL.GL_MODELVIEW)
		BGL.glLoadIdentity()
		bx = (b[1].x - b[0].x)
		by = (b[1].y - b[0].y)
		cx = bx / 2.0
		cy = by / 2.0
		for f in xrange(len(folds)):
			afold = folds[f]
			if(fold!=afold):
				BGL.glLoadName(f)
				BGL.glBegin(BGL.GL_LINE_LOOP)
				for v in afold.unfoldedFace().v:
					BGL.glVertex2f(v.x, v.y)
				BGL.glEnd()
		BGL.glPopMatrix()
		BGL.glFlush()
		hits = BGL.glRenderMode(BGL.GL_RENDER)
		buffer = [buffer[i] for i in xrange(3, 4*hits, 4)]
		o = [folds[buffer[i]] for i in xrange(len(buffer))]
		return self.getOverlapsBetween(fold, o)
	def colourFace(self, face, cr):
		for c in face.col:
			c.r = int(cr[0])
			c.g = int(cr[1])
			c.b = int(cr[2])
			c.a = int(cr[3])
		self.src.update()
	def setAvoidsOverlaps(self, avoids):
		self.avoidsOverlaps = avoids
	def addBranch(self, branch):
		self.branches.append(branch)
		if self.edgeIteratorClass!=RandomEdgeIterator:
			self.branches.sort(lambda b1, b2: b1.compare(b2))
	def srcSize(self):
		return len(self.src.faces)
	def nBranches(self):
		return len(self.branches)
	def facesCreated(self):
		return len(self.des.faces)
	def facesVisited(self):
		return self.myFacesVisited
	def getOverlaps(self):
		return self.overlaps
	def sortOutIPOSource(self):
		print "Sorting out IPO"
		if self.foldIPO!=None:
			return
		o = None
		try:
			o = Blender.Object.Get("FoldRate")
		except:
			o = Blender.Object.New("Empty", "FoldRate")
			Blender.Scene.GetCurrent().objects.link(o)
		if(o.getIpo()==None):
			ipo = Blender.Ipo.New("Object", "FoldRateIPO")
			z = ipo.addCurve("RotZ")
			print " added RotZ IPO curve"
			z.addBezier((1,0))
			# again, why is this 10x out ?
			z.addBezier((180, self.ff/10.0))
			z.addBezier((361, 0.0))
			o.setIpo(ipo)
			z.recalc()
			z.setInterpolation("Bezier")
			z.setExtrapolation("Cyclic")
		self.setIPOSource(o)
		print " added IPO source"
	def setIPOSource(self, object):
		try:
			self.foldIPO = object
			for i in xrange(self.foldIPO.getIpo().getNcurves()):
				self.IPOCurves[self.foldIPO.getIpo().getCurves()[i].getName()] = i
				print " added ", self.foldIPO.getIpo().getCurves()[i].getName()
		except:
			print "Problem setting IPO object"
			print sys.exc_info()[1]
			traceback.print_exc(file=sys.stdout)
	def setFoldFactor(self, ff):
		self.ff = ff
	def sayTree(self):
		for fold in self.folds:
			if(fold.getParent()!=None):
				print fold.getID(), fold.dihedralAngle(), fold.getParent().getID()
	def report(self):
		p = int(float(self.myFacesVisited)/float(len(self.src.faces)) * 100)
		print str(p) + "% unfolded"
		print "faces created:", self.facesCreated()
		print "faces visited:", self.facesVisited()
		print "originalfaces:", len(self.src.faces)
		n=0
		if(self.avoidsOverlaps):
			print "net avoided at least ", self.getOverlaps(), " overlaps ",
			n = len(self.src.faces) - self.facesCreated()
			if(n>0):
				print "but was unable to avoid ", n, " overlaps. Incomplete net."
			else:
				print "- A complete net."
		else:
			print "net has at least ", self.getOverlaps(), " collision(s)"
		return n
	# fold all my folds to a fraction of their total fold angle
	def unfoldToCurrentFrame(self):
		self.unfoldTo(Blender.Scene.GetCurrent().getRenderingContext().currentFrame())
	def unfoldTo(self, frame):
		frames = Blender.Scene.GetCurrent().getRenderingContext().endFrame()
		if(self.foldIPO!=None and self.foldIPO.getIpo()!=None):
			f = self.foldIPO.getIpo().EvaluateCurveOn(self.IPOCurves["RotZ"],frame)
			# err, this number seems to be 10x less than it ought to be
			fff = 1.0 - (f*10.0 / self.ff)
		else:
			fff = 1.0-((frame)/(frames*1.0))
		for fold in self.folds:
			fold.unfoldTo(fff)
		for fold in self.folds:
			fold.unfold()
			tface = fold.unfoldedFace()
			bface = fold.desFace
			i = 0
			for v in bface.verts:
				v.co.x = tface.v[i].x
				v.co.y = tface.v[i].y
				v.co.z = tface.v[i].z
				i+=1
		Window.Redraw(Window.Types.VIEW3D)
		return None
	def addFace(self, poly, originalFace=None):
		originalLength = len(self.des.verts)
		self.des.verts.extend([Vector(vv.x, vv.y, vv.z) for vv in poly.v])
		self.des.faces.extend([ range(originalLength, originalLength + poly.size()) ])
		newFace = self.des.faces[len(self.des.faces)-1]
		newFace.uv = [vv for vv in poly.v]
		if(originalFace!=None and self.src.vertexColors):
			newFace.col = [c for c in originalFace.col]
		if(self.feedback!=None):
			pu = str(int(self.fractionUnfolded() * 100))+"% unfolded"
			howMuchDone = str(self.myFacesVisited)+" of "+str(len(self.src.faces))+"  "+pu
			self.feedback.say(howMuchDone)
			#Window.DrawProgressBar (p, pu)
		if(self.showProgress):
			Window.Redraw(Window.Types.VIEW3D)
		return newFace
	def fractionUnfolded(self):
		return float(self.myFacesVisited)/float(len(self.src.faces))
	def assignUV(self, face, uv):
		face.uv = [Vector(v.x, v.y) for v in uv.v]
	def unfoldAll(feedback=None):
		objects = Blender.Object.Get()
		for object in objects:
			if(object.getType()=='Mesh' and not(object.getName().endswith("_net")) and len(object.getData(False, True).faces)>1):
				net = Net.createNet(object, feedback)
				net.searchForUnfolding()
				svg = SVGExporter(net, object.getName()+".svg")
				svg.export()
	unfoldAll = staticmethod(unfoldAll)
	def searchForUnfolding(self, limit=-1):
		overlaps = 1
		attempts = 0
		while(overlaps > 0 or attempts<limit):
			self.unfold()
			overlaps = self.report()
			attempts+=1
		return attempts
	def fromSelected(feedback=None, netName=None):
		return Net.createNet(Blender.Object.GetSelected()[0], feedback, netName)
	fromSelected = staticmethod(fromSelected)
	def clone(self, object=None):
		if(object==None):
			object = self.object
		net = Net.createNet(object, self.feedback)
		net.avoidsOverlaps = net.avoidsOverlaps
		return net
	def createNet(ob, feedback=None, netName=None):
		mesh = ob.getData(mesh=1)
		netObject = None
		if(netName==None):
			netName = ob.name[0:16]+"_net"
		try:
			netObject = Blender.Object.Get(netName)
			netMesh = netObject.getData(False, True)
			if(netMesh!=None):
				netMesh.verts = None # clear the mesh
			else:
				netObject = Blender.Object.New("Mesh", netName)
		except:
			if(netObject==None):
				netObject = Blender.Object.New("Mesh", netName)
		netMesh = netObject.getData(False, True)  # True means "as a Mesh not an NMesh"
		try:
			Blender.Scene.GetCurrent().objects.link(netObject)
		except:
			pass
		try:
			netMesh.materials = mesh.materials
			netMesh.vertexColors = True
		except:
			print "Problem setting materials here"
		net = Net(mesh, netMesh)
		if mesh.faceUV and mesh.activeFace>=0 and (mesh.faces[mesh.activeFace].sel):
			net.firstFaceIndex = mesh.activeFace
		net.object = ob
		net.feedback = feedback
		return net
	createNet = staticmethod(createNet)
	def importNet(filename):
		netName = filename.rstrip(".svg").replace("\\","/")
		netName = netName[netName.rfind("/")+1:]
		try:
			netObject = Blender.Object.Get(netName)
		except:
			netObject  = Blender.Object.New("Mesh", netName)
		netObject.getData(mesh=1).name = netName
		try:
			Blender.Scene.GetCurrent().objects.link(netObject)
		except:
			pass
		net = Net(None, netObject.getData(mesh=1))
		handler = NetHandler(net)
		xml.sax.parse(filename, handler)
		Window.Redraw(Window.Types.VIEW3D)
		return net
	importNet = staticmethod(importNet)
	def getSourceMesh(self):
		return self.src
		
# determines the order in which to visit faces according to a local measure		
class EdgeIterator:
	def __init__(self, branch, otherConstructor=None):
		self.branch = branch
		self.bface = branch.getFace()
		self.edge = branch.getFold().getEdge()
		self.net = branch.getNet()
		self.n = len(self.bface)
		self.edges = []
		self.i = 0
		self.gooodness = 0
		self.createEdges()
		self.computeGoodness()
		if(otherConstructor==None):
			self.sequenceEdges()
	def createEdges(self):
		edge = None
		e = Edge.edgesOfBlenderFace(self.net.getSourceMesh(), self.bface)
		for edge in e:
			if not(edge.isBlenderSeam() and edge!=self.edge):
				self.edges.append(edge)
	def sequenceEdges(self):
		pass
	def next(self):
		edge = self.edges[self.i]
		self.i+=1
		return edge
	def size(self):
		return len(self.edges)
	def reset(self):
		self.i = 0
	def hasNext(self):
		return (self.i<len(self.edges))
	def goodness(self):
		return self.gooodness
	def computeGoodness(self):
		self.gooodness = 0
	def rotate(self):
		self.edges.append(self.edges.pop(0))

class RandomEdgeIterator(EdgeIterator):
	def sequenceEdges(self):
		random.seed()
		random.shuffle(self.edges)
	def goodness(self):
		return random.randint(0, self.net.srcSize())
	
		
class Largest(EdgeIterator):
	def sequenceEdges(self):
		for e in self.edges:
			f = self.net.facesAndEdges.findAdjacentFace(self.bface, e)
			if(f!=None):
				e.setGoodness(f.area)
		self.edges.sort(lambda e1, e2: -e1.compare(e2))
	def computeGoodness(self):
		self.gooodness = self.bface.area
		
			
class Brightest(EdgeIterator):
	def sequenceEdges(self):
		for edge in self.edges:
			f = self.net.facesAndEdges.findAdjacentFace(self.bface, edge)
			if(f!=None):
				b = 0
				if self.net.src.vertexColors:
					for c in f.col:
						b+=(c.g+c.r+c.b)
				rc = float(random.randint(0, self.net.srcSize())) / float(self.net.srcSize()) / 100.0
				b+=rc
				edge.setGoodness(b)
		self.edges.sort(lambda e1, e2: e1.compare(e2))
	def computeGoodness(self):
		g = 0
		if self.net.src.vertexColors:
			for c in self.bface.col:
				g+=(c.g+c.r+c.b)
		self.gooodness = g
		
class OddEven(EdgeIterator):
	i = True
	def sequenceEdges(self):
		OddEven.i = not(OddEven.i)
		if(OddEven.i):
			self.edges.reverse()
		
class Curvature(EdgeIterator):
	def sequenceEdges(self):
		p1 = Poly.fromBlenderFace(self.bface)
		gg = 0.0
		for edge in self.edges:
			f = self.net.facesAndEdges.findAdjacentFace(self.bface, edge)
			if(f!=None):
				p2 = Poly.fromBlenderFace(f)
				fold = Fold(None, p1, p2, edge)
				fold.srcFace = f
				b = Tree(self.net, self.branch, fold, self)
				c = Curvature(b, False)
				g = c.goodness()
				gg+=g
				edge.setGoodness(g)
		self.edges.sort(lambda e1, e2: e1.compare(e2))
		tg = (self.gooodness + gg)
		rc = float(random.randint(0, self.net.srcSize())) / float(self.net.srcSize()) / 100.0
		if(tg!=0.0):
			self.gooodness = self.gooodness + rc / tg
	def computeGoodness(self):
		g = 0
		for edge in self.edges:
			f = self.net.facesAndEdges.findAdjacentFace(self.bface, edge)
			if(f!=None):
				p1 = Poly.fromBlenderFace(self.bface)
				p2 = Poly.fromBlenderFace(f)
				f = Fold(None, p1, p2, edge)
				g += f.dihedralAngle()
		self.gooodness = g
		

class Edge:
	def __init__(self, v1=None, v2=None, mEdge=None, i=-1):
		self.idx = i
		if v1 and v2:
			self.v1 = v1.copy()
			self.v2 = v2.copy()
		else:
			self.v1 = mEdge.v1.co.copy()
			self.v2 = mEdge.v2.co.copy()
		self.v1n = -self.v1
		self.vector = self.v1-self.v2
		self.vector.resize3D()
		self.vector.normalize()
		self.bmEdge = mEdge
		self.gooodness = 0.0
	def fromBlenderFace(mesh, bface, i):
		if(i>len(bface)-1):
			return None
		if(i==len(bface)-1):
			j = 0
		else:
			j = i+1
		edge =  Edge( bface.v[i].co.copy(), bface.v[j].co.copy() )
		edge.bEdge = mesh.findEdge(bface.v[i], bface.v[j])
		edge.idx = i
		return edge
	fromBlenderFace=staticmethod(fromBlenderFace)
	def edgesOfBlenderFace(mesh, bmFace):
		edges = [mesh.edges[mesh.findEdges(edge[0], edge[1])] for edge in bmFace.edge_keys]
		v = bmFace.verts
		e = []
		vi = v[0]
		i=0
		for j in xrange(1, len(bmFace)+1):
			vj = v[j%len(bmFace)]
			for ee in edges:
				if((ee.v1.index==vi.index and ee.v2.index==vj.index) or (ee.v2.index==vi.index and ee.v1.index==vj.index)):
					e.append(Edge(vi.co, vj.co, ee, i))
					i+=1
			vi = vj
		return e
	edgesOfBlenderFace=staticmethod(edgesOfBlenderFace)
	def isBlenderSeam(self):
		return (self.bmEdge.flag & Mesh.EdgeFlags.SEAM)
	def isInFGon(self):
		return (self.bmEdge.flag & Mesh.EdgeFlags.FGON)
	def mapTo(self, poly):
		if(self.idx==len(poly.v)-1):
			j = 0
		else:
			j = self.idx+1
		return Edge(poly.v[self.idx], poly.v[j])
	def isDegenerate(self):
		return self.vector.length==0
	def vertices(s):
		return [ [s.v1.x, s.v1.y, s.v1.z], [s.v2.x, s.v2.y,s.v2.z] ]
	def key(self):
		return self.bmEdge.key
	def goodness(self):
		return self.gooodness
	def setGoodness(self, g):
		self.gooodness = g
	def compare(self, other):
		if(self.goodness() > other.goodness()):
			return +1
		else:
			return -1
	# Does the given segment intersect this, for overlap detection.
	# endpoints are allowed to touch the line segment
	def intersects2D(self, s):
		if(self.matches(s)):
			return False
		else:
			i = Geometry.LineIntersect2D(self.v1, self.v2, s.v1, s.v2)
			if(i!=None):
				i.resize4D()
				i.z = self.v1.z # hack to put the point on the same plane as this edge for comparison
			return(i!=None and not(self.endsWith(i)))
	def matches(self, s):
		return ( (self.v1==s.v1 and self.v2==s.v2) or (self.v2==s.v1 and self.v1==s.v2) )
	# Is the given point on the end of this segment ? 10-5 seems to an acceptable limit for closeness in Blender
	def endsWith(self, aPoint, e=0.0001):
		return ( (self.v1-aPoint).length < e or (self.v2-aPoint).length < e )

	
class Poly:
	ids = -1
	def __init__(self):
		Poly.ids+=1
		self.v = []
		self.id = Poly.ids
		self.boundz = None
		self.edges = None
	def getID(self):
		return self.id
	def normal(self):
		a =self.v[0]
		b=self.v[1]
		c=self.v[2]
		p = b-a
		p.resize3D()
		q = a-c
		q.resize3D()
		return p.cross(q)
	def makeEdges(self):
		self.edges = []
		for i in xrange(self.nPoints()):
			self.edges.append(Edge( self.v[i % self.nPoints()], self.v[ (i+1) % self.nPoints()] ))
	def edgeAt(self, i):
		if(self.edges==None):
			self.makeEdges()
		return self.edges[i]
	def intersects2D(self, poly):
		for i in xrange(self.nPoints()):
			edge = self.edgeAt(i)
			for j in xrange(poly.nPoints()):
				if edge.intersects2D(poly.edgeAt(j)):
					return True
		return False
	def isBad(self):
		badness = 0
		for vv in self.v:
			if(vv.x!=vv.x or vv.y!=vv.y or vv.z!=vv.z): # Nan check
				badness+=1
		return (badness>0)
	def midpoint(self):
		x=y=z = 0.0
		n = 0
		for vv in self.v:
			x+=vv.x
			y+=vv.y
			z+=vv.z
			n+=1
		return [ x/n, y/n, z/n ]
	def centerAtOrigin(self):
		mp = self.midpoint()
		mp = -mp
		toOrigin = TranslationMatrix(mp)
		self.v = [(vv * toOrigin) for vv in self.v]
	def move(self, tv):
		mv = TranslationMatrix(tv)
		self.v = [(vv * mv) for vv in self.v]
	def scale(self, s):
		mp = Vector(self.midpoint())
		fromOrigin = TranslationMatrix(mp)
		mp = -mp
		toOrigin = TranslationMatrix(mp)
		sm = ScaleMatrix(s, 4)
		# Todo, the 3 lines below in 1 LC
		self.v = [(vv * toOrigin) for vv in self.v]
		self.v = [(sm * vv) for vv in self.v]
		self.v = [(vv * fromOrigin) for vv in self.v]
	def nPoints(self):
		return len(self.v)
	def size(self):
		return len(self.v)
	def rotated(self, axis, angle):
		p = self.clone()
		p.rotate(axis, angle)
		return p
	def rotate(self, axis, angle):
		rotation = RotationMatrix(angle, 4, "r", axis.vector)
		toOrigin = TranslationMatrix(axis.v1n)
		fromOrigin = TranslationMatrix(axis.v1)
		# Todo, the 3 lines below in 1 LC
		self.v = [(vv * toOrigin) for vv in self.v]
		self.v = [(rotation * vv) for vv in self.v]
		self.v = [(vv * fromOrigin) for vv in self.v]
	def moveAlong(self, vector, distance):
		t = TranslationMatrix(vector)
		s = ScaleMatrix(distance, 4)
		ts = t*s
		self.v = [(vv * ts) for vv in self.v]
	def bounds(self):
		if(self.boundz == None):
			vv = [vv for vv in self.v]
			vv.sort(key=lambda v: v.x)
			minx = vv[0].x
			maxx = vv[len(vv)-1].x
			vv.sort(key=lambda v: v.y)
			miny = vv[0].y
			maxy = vv[len(vv)-1].y
			self.boundz = [Vector(minx, miny, 0), Vector(maxx, maxy, 0)]
		return self.boundz
	def fromBlenderFace(bface):
		p = Poly()
		for vv in bface.v:
			vec = Vector([vv.co[0], vv.co[1], vv.co[2] , 1.0]) 
			p.v.append(vec)
		return p
	fromBlenderFace = staticmethod(fromBlenderFace)
	def fromList(list):
		p = Poly()
		for vv in list:
			vec = Vector( [vvv for vvv in vv] )
			vec.resize4D()
			p.v.append(vec)
		return p
	fromList = staticmethod(fromList)
	def fromVectors(vectors):
		p = Poly()
		p.v.extend([v.copy().resize4D() for v in vectors])
		return p
	fromVectors = staticmethod(fromVectors)
	def clone(self):
		p = Poly()
		p.v.extend(self.v)
		return p
	def hasVertex(self, ttv):
		v = Mathutils.Vector(ttv)
		v.normalize()
		for tv in self.v:
			vv = Mathutils.Vector(tv)
			vv.normalize()
			t = 0.00001
			if abs(vv.x-v.x)<t and abs(vv.y-v.y)<t:
				return True
		return False
	def overlays(self, poly):
		if len(poly.v)!=len(self.v):
			return False
		c = 0
		for point in poly.v:
			if self.hasVertex(point):
				c+=1
		return c==len(self.v)
	def sharesVertexWith(self, poly):
		for point in poly.v:
			if(self.hasVertex(point)):
				return True
		return False
	def containsAnyOf(self, poly):
		for point in poly.v:
			if(not(self.hasVertex(point))):
				if self.contains(point):
					return True
		return False
	def toString(self):
		return self.v
	# This is the BEST algorithm for point-in-polygon detection.
	# It's by W. Randolph Franklin.
	# returns 1 for inside, 1 or 0 for edges
	def contains(self, tp):
		c = 0
		j = len(self.v)-1
		for i in xrange(len(self.v)):
			if(i>0): j=i-1
			cv = self.v[i]
			nv = self.v[j]
			if ((((cv.y<=tp.y) and (tp.y<nv.y)) or ((nv.y<=tp.y) and (tp.y<cv.y))) and (tp.x < (nv.x - cv.x) * (tp.y - cv.y) / (nv.y - cv.y) + cv.x)):
				c = not(c)
		return (c == 1)
		
class SVGExporter:
	def __init__(self, net, filename):
		self.net = net
		print self.net.des.name
		self.object = self.net.object
		print "Exporting ", self.object
		self.filename = filename
		self.file = None
		self.e = None
		self.width = 1024
		self.height = 768
	def start(self):
		print "Exporting SVG to ", self.filename
		self.file = open(self.filename, 'w')
		self.e = xml.sax.saxutils.XMLGenerator(self.file, "UTF-8")
		atts = {}
		atts["width"] = "100%"
		atts["height"] = "100%"
		atts["viewBox"] = str(self.vxmin)+" "+str(self.vymin)+" "+str(self.vxmax-self.vxmin)+" "+str(self.vymax-self.vymin)
		atts["xmlns:nets"] = "http://celeriac.net/unfolder/rdf#"
		atts["xmlns:xlink"] = "http://www.w3.org/1999/xlink"
		atts["xmlns"] ="http://www.w3.org/2000/svg"
		a = xml.sax.xmlreader.AttributesImpl(atts)
		self.e.startDocument()
		self.e.startElement("svg", a)
		self.e.startElement("defs", xml.sax.xmlreader.AttributesImpl({}))
		atts = {}
		atts["type"]="text/css"
		self.e.startElement("style", atts)
		# can't find a proper way to do this
		self.file.write("<![CDATA[")
		self.file.write("polygon.poly{fill:white;stroke:black;stroke-width: 0.001}")
		self.file.write("g#foldLines line.valley{stroke:white;stroke-width:0.01;stroke-dasharray:0.02,0.01,0.02,0.05}")
		self.file.write("g#foldLines line.mountain{stroke:white;stroke-width:0.01;stroke-dasharray:0.02,0.04}")
		self.file.write("]]>")
		self.e.endElement("style")
		self.e.endElement("defs")
		#self.addClipPath()
		self.addMeta()
	def addMeta(self):
		self.e.startElement("metadata", xml.sax.xmlreader.AttributesImpl({}))
		self.e.startElement("nets:net", xml.sax.xmlreader.AttributesImpl({}))
		for i in xrange(1, len(self.net.folds)):
			fold = self.net.folds[i]
			# AttributesNSImpl - documentation is rubbish. using this hack.
			atts = {}
			atts["nets:id"] = "fold"+str(fold.getID())
			if(fold.parent!=None):
				atts["nets:parent"] = "fold"+str(fold.parent.getID())
			else:
				atts["nets:parent"] = "null"
			atts["nets:da"] = str(fold.dihedralAngle())
			if(fold.parent!=None):
				atts["nets:ofPoly"] = "poly"+str(fold.parent.foldingPoly.getID())
			else:
				atts["nets:ofPoly"] = ""
			atts["nets:toPoly"] = "poly"+str(fold.foldingPoly.getID())
			a = xml.sax.xmlreader.AttributesImpl(atts)
			self.e.startElement("nets:fold",  a)
			self.e.endElement("nets:fold")
		self.e.endElement("nets:net")
		self.e.endElement("metadata")
	def end(self):
		self.e.endElement("svg")
		self.e.endDocument()
		print "grown."
	def export(self):
		self.net.unfoldTo(1)
		bb = self.object.getBoundBox()
		print bb
		self.vxmin = bb[0][0]
		self.vymin = bb[0][1]
		self.vxmax = bb[7][0]
		self.vymax = bb[7][1]
		self.start()
		atts = {}
		atts["id"] = self.object.getName()
		a = xml.sax.xmlreader.AttributesImpl(atts)
		self.e.startElement("g", a)
		#self.addUVImage()
		self.addPolys()
		self.addFoldLines()
		#self.addCutLines()
		self.e.endElement("g")
		self.end()
	def addClipPath(self):
		atts = {}
		atts["id"] = "netClip"
		atts["clipPathUnits"] = "userSpaceOnUse"
		atts["x"] = str(self.vxmin)
		atts["y"] = str(self.vymin)
		atts["width"] = "100%"
		atts["height"] = "100%"
		self.e.startElement("clipPath", atts)
		self.addPolys()
		self.e.endElement("clipPath")
	def addUVImage(self):
		image = Blender.Image.GetCurrent() #hmm - how to determine the desired image ?
		if image==None:
			return
		ifn = image.getFilename()
		ifn = self.filename.replace(".svg", ".jpg")
		image.setFilename(ifn)
		ifn = ifn[ifn.rfind("/")+1:]
		image.save()
		atts = {}
		atts["clip-path"] = "url(#netClip)"
		atts["xlink:href"] = ifn
		self.e.startElement("image", atts)
		self.e.endElement("image")
	def addPolys(self):
		atts = {}
		atts["id"] = "polys"
		a = xml.sax.xmlreader.AttributesImpl(atts)
		self.e.startElement("g", a)
		for i in xrange(len(self.net.folds)):
			self.addPoly(self.net.folds[i])
		self.e.endElement("g")
	def addFoldLines(self):
		atts = {}
		atts["id"] = "foldLines"
		a = xml.sax.xmlreader.AttributesImpl(atts)
		self.e.startElement("g", a)
		for i in xrange( 1, len(self.net.folds)):
			self.addFoldLine(self.net.folds[i])
		self.e.endElement("g")
	def addFoldLine(self, fold):
		edge = fold.edge.mapTo(fold.parent.foldingPoly)
		if fold.dihedralAngle()>0:
			foldType="valley"
		else:
			foldType="mountain"
		atts={}
		atts["x1"] = str(edge.v1.x)
		atts["y1"] = str(edge.v1.y)
		atts["x2"] = str(edge.v2.x)
		atts["y2"] = str(edge.v2.y)
		atts["id"] = "fold"+str(fold.getID())
		atts["class"] = foldType
		a = xml.sax.xmlreader.AttributesImpl(atts)
		self.e.startElement("line", a)
		self.e.endElement("line")
	def addCutLines(self):
		atts = {}
		atts["id"] = "cutLines"
		a = xml.sax.xmlreader.AttributesImpl(atts)
		self.e.startElement("g", a)
		for i in xrange( 1, len(self.net.cuts)):
			self.addCutLine(self.net.cuts[i])
		self.e.endElement("g")
	def addCutLine(self, cut):
		edge = cut.edge.mapTo(cut.parent.foldingPoly)
		if cut.dihedralAngle()>0:
			foldType="valley"
		else:
			foldType="mountain"
		atts={}
		atts["x1"] = str(edge.v1.x)
		atts["y1"] = str(edge.v1.y)
		atts["x2"] = str(edge.v2.x)
		atts["y2"] = str(edge.v2.y)
		atts["id"] = "cut"+str(cut.getID())
		atts["class"] = foldType
		a = xml.sax.xmlreader.AttributesImpl(atts)
		self.e.startElement("line", a)
		self.e.endElement("line")
	def addPoly(self, fold):
		face = fold.foldingPoly
		atts = {}
		if fold.desFace.col:
			col = fold.desFace.col[0]
			rgb = "rgb("+str(col.r)+","+str(col.g)+","+str(col.b)+")"
			atts["fill"] = rgb
		atts["class"] = "poly"
		atts["id"] = "poly"+str(face.getID())
		points = ""
		first = True
		for vv in face.v:
			if(not(first)):
				points+=','
			first = False
			points+=str(vv[0])
			points+=' '
			points+=str(vv[1])
		atts["points"] = points
		a = xml.sax.xmlreader.AttributesImpl(atts)
		self.e.startElement("polygon", a)
		self.e.endElement("polygon")
	def fileSelected(filename):
		try:
			net = Registry.GetKey('unfolder')['net']
			exporter = SVGExporter(net, filename)
			exporter.export()
		except:
			print "Problem exporting SVG"
			traceback.print_exc(file=sys.stdout)
	fileSelected = staticmethod(fileSelected)	

# for importing nets saved by the above exporter
class NetHandler(xml.sax.handler.ContentHandler):
	def __init__(self, net):
		self.net = net
		self.first = (41==41)
		self.currentElement = None
		self.chars = None
		self.currentAction = None
		self.foldsPending = {}
		self.polys = {}
		self.actions = {}
		self.actions["nets:fold"] = self.foldInfo
		self.actions["line"] = self.cutOrFold
		self.actions["polygon"] = self.createPoly
	def setDocumentLocator(self, locator):
		pass
	def startDocument(self):
		pass
	def endDocument(self):
		for fold in self.foldsPending.values():
			face = self.net.addFace(fold.unfoldedFace())
			fold.desFace = face
			self.net.folds.append(fold)
		self.net.addFace(self.first)
		self.foldsPending = None
		self.polys = None
	def startPrefixMapping(self, prefix, uri):
		pass
	def endPrefixMapping(self, prefix):
		pass
	def startElement(self, name, attributes):
		self.currentAction = None
		try:
			self.currentAction = self.actions[name]
		except:
			pass
		if(self.currentAction!=None):
			self.currentAction(attributes)
	def endElement(self, name):
		pass
	def startElementNS(self, name, qname, attrs):
		self.currentAction = self.actions[name]
		if(self.currentAction!=None):
			self.currentAction(attributes)
	def endElementNS(self, name, qname):
		pass
	def characters(self, content):
		pass
	def ignorableWhitespace(self):
		pass
	def processingInstruction(self, target, data):
		pass
	def skippedEntity(self, name):
		pass
	def foldInfo(self, atts):
		self.foldsPending[atts["nets:id"]] = atts
	def createPoly(self, atts):
		xy = re.split('[, ]' , atts["points"])
		vectors = []
		for i in xrange(0, len(xy)-1, 2):
			v = Vector([float(xy[i]), float(xy[i+1]), 0.0])
			vectors.append(v)
		poly = Poly.fromVectors(vectors)
		if(self.first==True):
			self.first = poly
		self.polys[atts["id"]] = poly
	def cutOrFold(self, atts):
		fid = atts["id"]
		try:
			fi = self.foldsPending[fid]
		except:
			pass
		p1 = Vector([float(atts["x1"]), float(atts["y1"]), 0.0])
		p2 = Vector([float(atts["x2"]), float(atts["y2"]), 0.0])
		edge = Edge(p1, p2)
		parent = None
		ofPoly = None
		toPoly = None
		try: 
			parent = self.foldsPending[fi["nets:parent"]]
		except:
			pass
		try:
			ofPoly = self.polys[fi["nets:ofPoly"]]
		except:
			pass
		try:
			toPoly = self.polys[fi["nets:toPoly"]]
		except:
			pass
		fold = Fold(parent, ofPoly , toPoly, edge, float(fi["nets:da"]))
		self.foldsPending[fid] = fold
	def fileSelected(filename):
		try:
			net = Net.importNet(filename)
			try:
				Registry.GetKey('unfolder')['net'] = net
			except:
				Registry.SetKey('unfolder', {})
				Registry.GetKey('unfolder')['net'] = net
			Registry.GetKey('unfolder')['lastpath'] = filename
		except:
			print "Problem importing SVG"
			traceback.print_exc(file=sys.stdout)
	fileSelected = staticmethod(fileSelected)		


class GUI:
	def __init__(self):
		self.overlaps = Draw.Create(0)
		self.ani = Draw.Create(0)
		self.selectedFaces =0
		self.search = Draw.Create(0)
		self.diffuse = True
		self.ancestors = Draw.Create(0)
		self.noise = Draw.Create(0.0)
		self.shape = Draw.Create(0)
		self.nOverlaps = 1==2
		self.iterators = [RandomEdgeIterator,Brightest,Curvature,EdgeIterator,OddEven,Largest]
		self.iterator = RandomEdgeIterator
		self.overlapsText = "*"
		self.message = " "
	def makePopupGUI(self):
		useRandom = Draw.Create(0)
		pub = []
		pub.append(("Search", self.search, "Search for non-overlapping net (maybe forever)"))
		pub.append(("Random", useRandom, "Random style net"))
		ok = True
		while ok:
			ok = Blender.Draw.PupBlock("Unfold", pub)
			if ok:
				if useRandom.val:
					self.iterator = RandomEdgeIterator
				else:
					self.iterator = Curvature
				self.unfold()
	def makeStandardGUI(self):
		Draw.Register(self.draw, self.keyOrMouseEvent, self.buttonEvent)
	def installScriptLink(self):
		print "Adding script link for animation"
		s = Blender.Scene.GetCurrent().getScriptLinks("FrameChanged")
		if(s!=None and s.count("frameChanged.py")>0):
			return
		try:
			script = Blender.Text.Get("frameChanged.py")
		except:
			script = Blender.Text.New("frameChanged.py")
			script.write("import Blender\n")
			script.write("import mesh_unfolder as Unfolder\n")
			script.write("u = Blender.Registry.GetKey('unfolder')\n")
			script.write("if u!=None:\n")
			script.write("\tn = u['net']\n")
			script.write("\tif(n!=None and n.animates):\n")
			script.write("\t\tn.unfoldToCurrentFrame()\n")
		Blender.Scene.GetCurrent().addScriptLink("frameChanged.py", "FrameChanged")
	def unfold(self):
		anc = self.ancestors.val
		n = 0.0
		s = True
		self.nOverlaps = 0
		searchLimit = 10
		search = 1
		Draw.Redraw(1)
		net = None
		name = None
		try:
			self.say("Unfolding...")
			Draw.Redraw(1)
			while(s):# and search < searchLimit):
				if(net!=None):
					name = net.des.name
				net = Net.fromSelected(self, name)
				net.setAvoidsOverlaps(not(self.overlaps.val))
				print
				print "Unfolding selected object"
				net.edgeIteratorClass = self.iterator
				print "Using ", net.edgeIteratorClass
				net.animates = self.ani.val
				self.diffuse = (self.ancestors.val==0)
				net.diffuse = self.diffuse
				net.generations = self.ancestors.val
				net.noise = self.noise.val
				print "even:", net.diffuse, " depth:", net.generations
				net.unfold()
				n = net.report()
				t = "."
				if(n<1.0):
					t = "Overlaps>="+str(n)
				else:
					t = "A complete net."
				self.nOverlaps = (n>=1)
				if(self.nOverlaps):
					self.say(self.message+" - unfolding failed - try again ")
				elif(not(self.overlaps.val)):
					self.say("Success. Complete net - no overlaps ")
				else:
					self.say("Unfolding complete")
				self.ancestors.val = anc
				s = (self.search.val and n>=1.0)
				dict = Registry.GetKey('unfolder')
				if(not(dict)):
					dict = {}
				dict['net'] = net
				Registry.SetKey('unfolder', dict)
				if(s):
					net = net.clone()
				search += 1
		except(IndexError):
			self.say("Please select an object to unfold")
		except:
			self.say("Problem unfolding selected object - see console for details")
			print "Problem unfolding selected object:"
			print sys.exc_info()[1]
			traceback.print_exc(file=sys.stdout)
		if(self.ani):
			if Registry.GetKey('unfolder')==None:
				print "no net!"
				return
			Registry.GetKey('unfolder')['net'].sortOutIPOSource()
			self.installScriptLink()
		Draw.Redraw(1)
	def keyOrMouseEvent(self, evt, val):
		if (evt == Draw.ESCKEY and not val):
			Draw.Exit()
	def buttonEvent(self, evt):
		if (evt == 1):
			self.unfold()
		if (evt == 5):
			try:
				Registry.GetKey('unfolder')['net'].setAvoidsOverlaps(self.overlaps.val)
			except:
				pass
		if (evt == 2):
			print "Trying to set IPO curve"
			try:
				s = Blender.Object.GetSelected()
				if(s!=None):
					Registry.GetKey('unfolder')['net'].setIPOSource( s[0] )
					print "Set IPO curve"
				else:
					print "Please select an object to use the IPO of"
			except:
				print "Problem setting IPO source"
			Draw.Redraw(1)
		if (evt == 6):
			Draw.Exit()
		if (evt == 7):
			try:
				if (Registry.GetKey('unfolder')['net']!=None):
					Registry.GetKey('unfolder')['net'].animates = self.ani.val
					if(self.ani):
						Registry.GetKey('unfolder')['net'].sortOutIPOSource()
						self.installScriptLink()
			except:
				print sys.exc_info()[1]
				traceback.print_exc(file=sys.stdout)
			Draw.Redraw(1)
		if (evt == 19):
			pass
		if (evt == 87):
			try:
				if (Registry.GetKey('unfolder')['net']!=None):
					Registry.GetKey('unfolder')['net'].assignUVs()
					self.say("Assigned UVs")
			except:
				print sys.exc_info()[1]
				traceback.print_exc(file=sys.stdout)
			Draw.Redraw(1)
		if(evt==91):
			if( testOverlap() == True):
				self.nOverlaps = 1
			else:
				self.nOverlaps = 0
			Draw.Redraw(1)
		if(evt==233):
			f1 = Poly.fromBlenderFace(Blender.Object.GetSelected()[0].getData().faces[0])
			f2 = Poly.fromBlenderFace(Blender.Object.GetSelected()[1].getData().faces[0])
			print
			print Blender.Object.GetSelected()[0].getName()
			print Blender.Object.GetSelected()[1].getName()
			print f1.intersects2D(f2)
			print f2.intersects2D(f1)
		if(evt==714):
			Net.unfoldAll(self)
			Draw.Redraw(1)
		if(evt==713):
			self.iterator = self.iterators[self.shape.val]
			Draw.Redraw(1)
		if(evt==92):
			if( testContains() == True):
				self.nOverlaps = 1
			else:
				self.nOverlaps = 0
			Draw.Redraw(1)
		if(evt==104):
			try:
				filename = "net.svg"
				s = Blender.Object.GetSelected()
				if(s!=None and len(s)>0):
					filename = s[0].getName()+".svg"
				else:
					if (Registry.GetKey('unfolder')['net']!=None):
						filename = Registry.GetKey('unfolder')['net'].des.name
						if(filename==None):
							filename="net.svg"
						else:
							filename=filename+".svg"
				Window.FileSelector(SVGExporter.fileSelected, "Select filename", filename)
			except:
				print "Problem exporting SVG"
				traceback.print_exc(file=sys.stdout)
		if(evt==107):
			try:
				Window.FileSelector(NetHandler.fileSelected, "Select file")
			except:
				print "Problem importing SVG"
				traceback.print_exc(file=sys.stdout)
	def say(self, m):
		self.message = m
		Draw.Redraw(1)
		Window.Redraw(Window.Types.SCRIPT)
	def draw(self):
		cw = 64
		ch = 16
		l = FlowLayout(32, cw, ch, 350, 64)
		l.y = 70
		self.search = Draw.Toggle("search",     19,   l.nx(), l.ny(), l.cw, l.ch, self.search.val, "Search for non-overlapping mesh (potentially indefinitely)")
		self.overlaps = Draw.Toggle("overlaps",   5,   l.nx(), l.ny(), l.cw, l.ch, self.overlaps.val, "Allow overlaps / avoid overlaps - if off, will not place overlapping faces")
		self.ani = Draw.Toggle("ani",       7,   l.nx(), l.ny(), l.cw, l.ch, self.ani.val, "Animate net")
		Draw.Button("uv",               87,   l.nx(), l.ny(), l.cw, l.ch, "Assign net as UV to source mesh (overwriting existing UV)")
		Draw.Button("Unfold",           1, l.nx(), l.ny(), l.cw, l.ch, "Unfold selected mesh to net")
		Draw.Button("save",             104,   l.nx(), l.ny(), l.cw, l.ch,  "Save net as SVG")
		Draw.Button("load",             107,   l.nx(), l.ny(), l.cw, l.ch,  "Load net from SVG")
		#Draw.Button("test",             233,   l.nx(), l.ny(), l.cw, l.ch,  "test")
		# unfolding enthusiasts - try uncommenting this
		self.ancestors = Draw.Number("depth", 654,        l.nx(), l.ny(), cw, ch, self.ancestors.val, 0, 9999,  "depth of branching 0=diffuse")
		#self.noise = Draw.Number("noise", 631,        l.nx(), l.ny(), cw, ch, self.noise.val, 0.0, 1.0,  "noisyness of branching")
		#Draw.Button("UnfoldAll",           714, l.nx(), l.ny(), l.cw, l.ch, "Unfold all meshes and save their nets")
		options = "order %t|random %x0|brightest %x1|curvature %x2|winding %x3| 1010 %x4|largest %x5"
		self.shape = Draw.Menu(options, 713,  l.nx(), l.ny(), cw, ch, self.shape.val, "shape of net")
		Draw.Button("exit",         6,   l.nx(), l.ny(), l.cw, l.ch, "exit")
		BGL.glClearColor(0.3, 0.3, 0.3, 1)
		BGL.glColor3f(0.3,0.3,0.3)
		l.newLine()
		BGL.glRasterPos2i(32, 100)
		Draw.Text(self.message)

class FlowLayout:
	def __init__(self, margin, cw, ch, w, h):
		self.x = margin-cw-4
		self.y = margin
		self.cw = cw
		self.ch = ch
		self.width = w
		self.height = h
		self.margin = margin
	def nx(self):
		self.x+=(self.cw+4)
		if(self.x>self.width):
			self.x = self.margin
			self.y-=self.ch+4
		return self.x
	def ny(self):
		return self.y
	def newLine(self):
		self.y-=self.ch+self.margin
		self.x = self.margin

# if xml is None, then dont bother running the script
if xml:
	try:
		sys.setrecursionlimit(10000)
		gui = GUI()
		gui.makeStandardGUI()
		#gui.makePopupGUI()
	except:
		traceback.print_exc(file=sys.stdout)
