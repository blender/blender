#!BPY
"""
Name: 'Relax selected UVs.'
Blender: 237
Group: 'UV'
Tooltip: 'Relaxes selected UVs '
"""

__author__ = "Campbell Barton"
__url__ = ("blender", "elysiun")
__version__ = "1.0 2006/02/07"

__bpydoc__ = """\
This script relaxes selected UV verts in relation to there surrounding geometry.

Use this script in face select mode.
Left Click to finish or wait until no more relaxing can be done.
"""

from Blender import Scene, Object, Mesh, Window
from Blender.Mathutils import Vector

class relaxVert(object):
	__slots__= 'bvert', 'edges', 'bfaces', 'uv', 'sel'
	def __init__(self, bvert):
		self.bvert= bvert
		self.edges= []
		self.bfaces= [] # list pf tuples, bface and this verts index in the bface.
		self.uv= Vector(0,0)
		self.sel= False
	
	def setBFaceUV(self):
		x= self.uv.x
		y= self.uv.y
		for bface,i in self.bfaces:
			bface.uv[i].x= x
			bface.uv[i].y= y
		
class relaxEdge(object):
	__slots__= 'v1', 'v2', 'length3d', 'lengthUv', 'lengthUvOrig'
	def __init__(self, v1,v2):
		self.v1= v1
		self.v2= v2
		self.length3d= (v1.bvert.co-v2.bvert.co).length
		self.lengthUv= None
		self.lengthUvOrig= None
	
	def otherVert(self, v):
		if v==self.v1 and v!=self.v2:
			return self.v2
		elif v==self.v2 and v!=self.v1:
			return self.v1
		else:
			raise 'Vert not in edge'
	
	def setUvLength(self):
		self.lengthUv= (self.v1.uv - self.v2.uv).length
		if self.lengthUvOrig==None:
			self.lengthUvOrig= self.lengthUv
def main():
	scn= Scene.GetCurrent()
	ob= scn.getActiveObject()
	if not ob or ob.getType() != 'Mesh':
		Draw.PupMenu('ERROR: No mesh object in face select mode.')
		return
	
	me= ob.getData(mesh=1)
	
	
	def sortPair(a,b):
		return min(a,b), max(a,b)
	
	
	# Build edge data from faces.
	relaxEdgeDict= {}
	relaxVertList= [relaxVert(v) for v in me.verts]
	
	tempVertUVList = [Vector(0,0,0) for v in me.verts] # Z is an int for scaling the first 2 values.
	
	for f in me.faces:
		for i in xrange(len(f.v)):
			v1,v2= f.v[i], f.v[i-1]
			key= sortPair(v1.index, v2.index)
			rv1= relaxVertList[v1.index]
			rv2= relaxVertList[v2.index]
			try: # Do nothing if we exist.
				tmpEdge= relaxEdgeDict[key]
			except:
				tmpEdge= relaxEdgeDict[key]= relaxEdge(rv1,rv2)
			
			# Add the edges to the face.
			rv1.edges.append(tmpEdge)
			rv2.edges.append(tmpEdge)
			
			# Will get to all v1's no need to add both per edge.
			rv1.bfaces.append( (f, i)   )
			tempVertUVList[v1.index].x += f.uv[i].x
			tempVertUVList[v1.index].y += f.uv[i].y
			tempVertUVList[v1.index].z+=1
			if f.uvSel[i]:
				rv1.sel= True
	
	# Now average UV's into the relaxVerts
	for i, rv in enumerate(relaxVertList):
		if tempVertUVList[i].z > 0:
			newUV= tempVertUVList[i] * (1/tempVertUVList[i].z)
			rv.uv.x= newUV.x
			rv.uv.y= newUV.y
	del tempVertUVList
	
	# Loop while the button is held, so clicking on a menu wont immediatly exit.
	while Window.GetMouseButtons() & Window.MButs['L']:
		pass
	
	# NOW RELAX
	#ITERATIONS=1000
	#for iter in range(ITERATIONS):
	Window.DrawProgressBar(0.0, '')
	iter=0
	while not Window.GetMouseButtons() & Window.MButs['L']:
		tempUV= Vector(0,0)
		iter+=1
		if not iter%10:
			Window.DrawProgressBar(0.1, 'Left Mouse to Exit %i' % iter)
		# Set the uv lengths each iteration.
		for re in relaxEdgeDict.itervalues():
			re.setUvLength()
		changed=False
		for rv in relaxVertList:
			if rv.sel:
				backupUV= rv.uv
				totUvLen= tot3dLen= 0
				
				for re in rv.edges:
					tot3dLen+= re.length3d
					totUvLen+= re.lengthUvOrig # So the UV edges dont keep growing.
					
				# the radio is the scaler between 3d space and UV space - so we can relax the UVs
				# so proportionaly the UV lengths match the 3d lengths.
				ratio = totUvLen/tot3dLen
				
				# Make a list of new UVs that match the 3d length of the edges.
				tempUV.y= tempUV.x= 0
				for re in rv.edges:
					otherRelaxVert= re.otherVert(rv)
					targetDist= re.length3d*ratio
					newUV= rv.uv-otherRelaxVert.uv
					newUV.normalize()
					newUV= newUV*targetDist
					
					#tempUVs.append(newUV+otherRelaxVert.uv)
					tempUV= tempUV+ ( (newUV+otherRelaxVert.uv) * (1/float(len(rv.edges))))
				
				
				if (rv.uv!=tempUV):
					changed= True
					rv.uv= (rv.uv+tempUV)* 0.5
		
		if not changed:
			break
			
				
		
		# Connection data done.
		for rv in relaxVertList:
			if rv.sel:
				rv.setBFaceUV()
		
			
		
		me.update()
		Window.RedrawAll()
			
		
	Window.DrawProgressBar(1.0, '')

if __name__=='__main__':
	main()