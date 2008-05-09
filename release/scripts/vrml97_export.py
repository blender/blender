#!BPY
""" Registration info for Blender menus:
Name: 'VRML97 (.wrl)...'
Blender: 241
Group: 'Export'
Tooltip: 'Export to VRML97 file (.wrl)'
"""

__author__ = ("Rick Kimball", "Ken Miller", "Steve Matthews", "Bart")
__url__ = ["blender", "blenderartists.org",
"Author's (Rick) homepage, http://kimballsoftware.com/blender",
"Author's (Bart) homepage, http://www.neeneenee.de/vrml"]
__email__ = ["Bart, bart:neeneenee*de"]
__version__ = "2006/01/17"
__bpydoc__ = """\
This script exports to VRML97 format.

Usage:

Run this script from "File->Export" menu.  A pop-up will ask whether you
want to export only selected or all relevant objects.
"""


# $Id$
#
#------------------------------------------------------------------------
# VRML97 exporter for blender 2.36 or above
#
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
#

####################################
# Library dependancies
####################################

import Blender
from Blender import Object, Mesh, Lamp, Draw, BGL, \
	 Image, Text, sys, Mathutils, Registry
from Blender.Scene import Render

import math

####################################
# Global Variables
####################################

scene = Blender.Scene.getCurrent()
world = Blender.World.GetCurrent() 
worldmat = Blender.Texture.Get()
filename = Blender.Get('filename')
_safeOverwrite = True
extension = ''

# Matrices below are used only when export_rotate_z_to_y.val:
#
# Blender is Z up, VRML is Y up, both are right hand coordinate
# systems, so to go from Blender coords to VRML coords we rotate
# by 90 degrees around the X axis. In matrix notation, we have a
# matrix, and it's inverse, as:
M_blen2vrml = Mathutils.Matrix([1,0,0,0], \
							   [0,0,1,0], \
							   [0,-1,0,0], \
							   [0,0,0,1])
M_vrml2blen = Mathutils.Matrix([1,0,0,0], \
							   [0,0,-1,0], \
							   [0,1,0,0], \
							   [0,0,0,1])


class DrawTypes:
	"""Object DrawTypes enum values
    BOUNDS - draw only the bounding box of the object
    WIRE - draw object as a wire frame
    SOLID - draw object with flat shading
    SHADED - draw object with OpenGL shading
"""
	BOUNDBOX  = 1
	WIRE      = 2
	SOLID     = 3
	SHADED    = 4
	TEXTURE   = 5

if not hasattr(Blender.Object,'DrawTypes'):
	Blender.Object.DrawTypes = DrawTypes()

##########################################################
# Functions for writing output file
##########################################################

class VRML2Export:

	def __init__(self, filename):
		#--- public you can change these ---
		self.wire = 0
		self.proto = 1
		self.facecolors = 0
		self.vcolors = 0
		self.billnode = 0
		self.halonode = 0
		self.collnode = 0
		self.tilenode = 0
		self.wire     = 0
		self.twosided = 0

		# level of verbosity in console 0-none, 1-some, 2-most
		try:
			rt = Blender.Get('rt')
			if (rt == 42):
				self.verbose = 1
			elif (rt == 43):
				self.verbose = 2
			else:
				self.verbose = 0
		except:
			self.verbose = 0
			
		# decimals for material color values     0.000 - 1.000
		self.cp=7
		# decimals for vertex coordinate values  0.000 - n.000
		self.vp=7
		# decimals for texture coordinate values 0.000 - 1.000
		self.tp=7
		
		#--- class private don't touch ---
		self.texNames={}   # dictionary of textureNames
		self.matNames={}   # dictionary of materialNames
		self.meshNames={}   # dictionary of meshNames
		self.coordNames={}   # dictionary of coordNames
		self.indentLevel=0 # keeps track of current indenting
		self.filename=filename
		self.file = open(filename, "w")
		self.bNav=0
		self.nodeID=0
		self.namesReserved=[ "Anchor", "Appearance", "AudioClip",
							 "Background","Billboard", "Box",
							 "Collision", "Color", "ColorInterpolator",
							 "Cone", "Coordinate",
							 "CoordinateInterpolator", "Cylinder",
							 "CylinderSensor",
							 "DirectionalLight",
							 "ElevationGrid", "Extrustion",
							 "Fog", "FontStyle", "Group",
							 "ImageTexture", "IndexedFaceSet",
							 "IndexedLineSet", "Inline",
							 "LOD", "Material", "MovieTexture",
							 "NavigationInfo", "Normal",
							 "NormalInterpolator",
							 "OrientationInterpolator", "PixelTexture",
							 "PlaneSensor", "PointLight", "PointSet",
							 "PositionInterpolator", "ProxmimitySensor",
							 "ScalarInterpolator", "Script", "Shape",
							 "Sound", "Sphere", "SphereSensor",
							 "SpotLight", "Switch", "Text",
							 "TextureCoordinate", "TextureTransform",
							 "TimeSensor", "TouchSensor", "Transform",
							 "Viewpoint", "VisibilitySensor", "WorldInfo" ]
		self.namesStandard=[ "Empty", "Empty.000", "Empty.001",
							 "Empty.002", "Empty.003", "Empty.004",
							 "Empty.005", "Empty.006", "Empty.007",
							 "Empty.008", "Empty.009", "Empty.010",
							 "Empty.011", "Empty.012",
							 "Scene.001", "Scene.002", "Scene.003",
							 "Scene.004", "Scene.005", "Scene.06",
							 "Scene.013", "Scene.006", "Scene.007",
							 "Scene.008", "Scene.009", "Scene.010",
							 "Scene.011","Scene.012",
							 "World", "World.000", "World.001",
							 "World.002", "World.003", "World.004",
							 "World.005" ]
		self.namesFog=[ "", "LINEAR"," EXPONENTIAL", "" ]

##########################################################
# Writing nodes routines
##########################################################

	def writeHeader(self):
		bfile = sys.expandpath(Blender.Get('filename'))
		self.file.write("#VRML V2.0 utf8\n\n")
		self.file.write("# This file was authored with Blender " \
						"(http://www.blender.org/)\n")
		self.file.write("# Blender version %s\n" % Blender.Get('version'))
		self.file.write("# Blender file %s\n" % sys.basename(bfile))
		self.file.write("# Exported using VRML97 exporter " \
						"v1.55 (2006/01/17)\n\n")

	def writeInline(self):
		inlines = Blender.Scene.Get()
		allinlines = len(inlines)
		if scene != inlines[0]:
			return
		else:
			for i in range(allinlines):
				nameinline=inlines[i].getName()
				if (nameinline not in self.namesStandard) and (i > 0):
					self.writeIndented("DEF %s Inline {\n" % \
									   (self.cleanStr(nameinline)), 1)
					nameinline = nameinline+".wrl"
					self.writeIndented("url \"%s\" \n" % nameinline)
					self.writeIndented("}\n", -1)
					self.writeIndented("\n")

	def writeScript(self):
		textEditor = Blender.Text.Get() 
		alltext = len(textEditor)
		for i in range(alltext):
			nametext = textEditor[i].getName()
			nlines = textEditor[i].getNLines()
			if (self.proto == 1):
				if (nametext == "proto" or nametext == "proto.js" or \
					nametext == "proto.txt") and (nlines != None):
					nalllines = len(textEditor[i].asLines())
					alllines = textEditor[i].asLines()
					for j in range(nalllines):
						self.writeIndented(alllines[j] + "\n")
			elif (self.proto == 0):
				if (nametext == "route" or nametext == "route.js" or \
					nametext == "route.txt") and (nlines != None):
					nalllines = len(textEditor[i].asLines())
					alllines = textEditor[i].asLines()
					for j in range(nalllines):
						self.writeIndented(alllines[j] + "\n")
		self.writeIndented("\n")

	def writeViewpoint(self, thisObj):
		# NOTE: The transform node above this will take care of
		# the position and orientation of the camera
		context = scene.getRenderingContext()
		ratio = float(context.imageSizeY()) / float(context.imageSizeX())
		temp  = ratio * 16 / thisObj.data.getLens()
		lens = 2 * math.atan(temp)
		lens = min(lens, math.pi) 

		self.writeIndented("DEF %s Viewpoint {\n" % \
						   (self.cleanStr(thisObj.name)), 1)
		self.writeIndented('description "%s" \n' % thisObj.name)
		self.writeIndented("position 0.0 0.0 0.0\n")
		# Need camera to point to -y in local space to accomodate
		# the transforma node above
		self.writeIndented("orientation 1.0 0.0 0.0 %f\n" % (-math.pi/2.0))
		self.writeIndented("fieldOfView %.3f\n" % (lens))
		self.writeIndented("}\n", -1)
		self.writeIndented("\n")

	def writeFog(self):
		if world:
			mtype = world.getMistype()
			mparam = world.getMist()
			grd = world.getHor()
			grd0, grd1, grd2 = grd[0], grd[1], grd[2]
		else:
			return
		if (mtype == 1 or mtype == 2):
			self.writeIndented("Fog {\n",1)
			self.writeIndented('fogType "%s"\n' % self.namesFog[mtype])
			self.writeIndented("color %s %s %s\n" % \
							   (round(grd0,self.cp), \
								round(grd1,self.cp), \
								round(grd2,self.cp)))
			self.writeIndented("visibilityRange %s\n" % \
							   round(mparam[2],self.cp))  
			self.writeIndented("}\n",-1)
			self.writeIndented("\n")   
		else:
			return

	def writeNavigationInfo(self, scene):
		allObj = []
		allObj = list(scene.objects)
		headlight = "TRUE"
		vislimit = 0.0
		for thisObj in allObj:
			objType=thisObj.type
			if objType == "Camera":
				vislimit = thisObj.data.getClipEnd()
			elif objType == "Lamp":
				headlight = "FALSE"
		self.writeIndented("NavigationInfo {\n",1)
		self.writeIndented("headlight %s\n" % headlight)
		self.writeIndented("visibilityLimit %s\n" % \
						   (round(vislimit,self.cp)))
		self.writeIndented("type [\"EXAMINE\", \"ANY\"]\n")            
		self.writeIndented("avatarSize [0.25, 1.75, 0.75]\n")
		self.writeIndented("} \n",-1)
		self.writeIndented(" \n")

	def writeSpotLight(self, object, lamp):
		# Note: location and orientation are handled by the
		# transform node above this object
		if world:
			ambi = world.getAmb()
			ambientIntensity = ((float(ambi[0] + ambi[1] + ambi[2]))/3)/2.5
		else:
			ambi = 0
			ambientIntensity = 0

		# compute cutoff and beamwidth
		intensity=min(lamp.energy/1.75,1.0)
		beamWidth=((lamp.spotSize*math.pi)/180.0)*.37;
		cutOffAngle=beamWidth*1.3

		radius = lamp.dist*math.cos(beamWidth)
		self.writeIndented("DEF %s SpotLight {\n" % \
						   self.cleanStr(object.name),1)
		self.writeIndented("radius %s\n" % (round(radius,self.cp)))
		self.writeIndented("ambientIntensity %s\n" % \
						   (round(ambientIntensity,self.cp)))
		self.writeIndented("intensity %s\n" % (round(intensity,self.cp)))
		self.writeIndented("color %s %s %s\n" % \
						   (round(lamp.col[0],self.cp), \
							round(lamp.col[1],self.cp), \
							round(lamp.col[2],self.cp)))
		self.writeIndented("beamWidth %s\n" % (round(beamWidth,self.cp)))
		self.writeIndented("cutOffAngle %s\n" % \
						   (round(cutOffAngle,self.cp)))
		# Note: point down -Y axis, transform node above will rotate
		self.writeIndented("direction 0.0 -1.0 0.0\n")
		self.writeIndented("location 0.0 0.0 0.0\n")
		self.writeIndented("}\n",-1)
		self.writeIndented("\n")
		
	def writeDirectionalLight(self, object, lamp):
		# Note: location and orientation are handled by the
		# transform node above this object
		if world:
			ambi = world.getAmb()
			ambientIntensity = ((float(ambi[0] + ambi[1] + ambi[2]))/3)/2.5
		else:
			ambi = 0
			ambientIntensity = 0

		intensity=min(lamp.energy/1.75,1.0) 
		self.writeIndented("DEF %s DirectionalLight {\n" % \
						   self.cleanStr(object.name),1)
		self.writeIndented("ambientIntensity %s\n" % \
						   (round(ambientIntensity,self.cp)))
		self.writeIndented("color %s %s %s\n" % \
						   (round(lamp.col[0],self.cp), \
							round(lamp.col[1],self.cp), \
							round(lamp.col[2],self.cp)))
		self.writeIndented("intensity %s\n" % \
						   (round(intensity,self.cp)))
		# Note: point down -Y axis, transform node above will rotate
		self.writeIndented("direction 0.0 -1.0 0.0\n")
		self.writeIndented("}\n",-1)
		self.writeIndented("\n")

	def writePointLight(self, object, lamp):
		# Note: location is at origin because parent transform node
		# takes care of this
		if world:
			ambi = world.getAmb()
			ambientIntensity = ((float(ambi[0] + ambi[1] + ambi[2]))/3)/2.5
		else:
			ambi = 0
			ambientIntensity = 0
		om = object.getMatrix()
		intensity=min(lamp.energy/1.75,1.0) 
		radius = lamp.dist
		self.writeIndented("DEF %s PointLight {\n" % \
						   self.cleanStr(object.name),1)
		self.writeIndented("ambientIntensity %s\n" % \
						   (round(ambientIntensity,self.cp)))
		self.writeIndented("color %s %s %s\n" % \
						   (round(lamp.col[0],self.cp), \
							round(lamp.col[1],self.cp), \
							round(lamp.col[2],self.cp)))
		self.writeIndented("intensity %s\n" % (round(intensity,self.cp)))
		self.writeIndented("location 0.0 0.0 0.0\n")
		self.writeIndented("radius %s\n" % radius )
		self.writeIndented("}\n",-1)
		self.writeIndented("\n")

	def writeNode(self, thisObj):
		# Note: location and orientation are handled by the
		# transform node above this object
		objectname=str(thisObj.getName())
		if objectname in self.namesStandard:
			return
		else:
			self.writeIndented("%s {\n" % objectname,1)
			# May need to check that the direction is done right
			self.writeIndented("direction 0.0 -1.0 0.0\n")
			self.writeIndented("location 0.0 0.0 0.0\n")
			self.writeIndented("}\n",-1)
			self.writeIndented("\n")

	def secureName(self, name):
		name = name + str(self.nodeID)
		self.nodeID += 1
		if len(name) <= 3:
			newname = "_" + str(self.nodeID)
			return "%s" % (newname)
		else:
			for bad in ['"','#',"'",',','.','[','\\',']','{','}']:
				name=name.replace(bad,'_')
			if name in self.namesReserved:
				newname = name[0:3] + "_" + str(self.nodeID)
				return "%s" % (newname)
			elif name[0].isdigit():
				newname = "_" + name + str(self.nodeID)
				return "%s" % (newname)
			else:
				newname = name
				return "%s" % (newname)

	def classifyMesh(self, me, ob):
		self.halonode = 0
		self.billnode = 0
		self.facecolors = 0
		self.vcolors = 0
		self.tilenode = 0
		self.colnode = 0
		self.wire = 0
		if me.faceUV:
			for face in me.faces:
				if (face.mode & Mesh.FaceModes['HALO']):
					self.halonode = 1
				if (face.mode & Mesh.FaceModes['BILLBOARD']):
					self.billnode = 1
				if (face.mode & Mesh.FaceModes['OBCOL']):
					self.facecolors = 1
				if (face.mode & Mesh.FaceModes['SHAREDCOL']):
					self.vcolors = 1
				if (face.mode & Mesh.FaceModes['TILES']):
					self.tilenode = 1
				if not (face.mode & Mesh.FaceModes['DYNAMIC']):
					self.collnode = 1
				if (face.mode & Mesh.FaceModes['TWOSIDE']):
					self.twosided = 1

		# Bit of a crufty trick, but if mesh has vertex colors
		# (as a non-face property) and if first material has
		# vcol paint set, we export the vertex colors
		if (me.vertexColors):
			if len(me.materials) > 0:
				mat = me.materials[0]
				if mat:
					if (mat.mode & Blender.Material.Modes['VCOL_PAINT']):
						self.vcolors = 1
			
		# check if object is wireframe only
		if ob.drawType == Blender.Object.DrawTypes.WIRE:
			# user selected WIRE=2 on the Drawtype=Wire on (F9) Edit page
			self.wire = 1

	###
	### The next few functions nest Collision/Billboard/Halo nodes.
	### For real mesh data export, jump down to writeMeshData()
	###
	def writeMesh(self, ob, normals = 0):

		imageMap={}   # set of used images
		sided={}      # 'one':cnt , 'two':cnt
		vColors={}    # 'multi':1

		if (len(ob.modifiers) > 0):
			me = Mesh.New()
			me.getFromObject(ob.name)
			# Careful with the name, the temporary mesh may
			# reuse the default name for other meshes. So we
			# pick our own name.
			me.name = "MOD_%s" % (ob.name)
		else:
			me = ob.getData(mesh = 1)

		self.classifyMesh(me, ob)

		if (self.collnode):
			self.writeCollisionMesh(me, ob, normals)
			return
		else:
			self.writeRegularMesh(me, ob, normals)
			return

	def writeCollisionMesh(self, me, ob, normals = 0):
		self.writeIndented("Collision {\n",1)
		self.writeIndented("collide FALSE\n")
		self.writeIndented("children [\n")

		self.writeRegularMesh(me, ob, normals)

		self.writeIndented("]\n", -1)
		self.writeIndented("}\n", -1)
		
	def writeRegularMesh(self, me, ob, normals = 0):
		if (self.billnode):
			self.writeBillboardMesh(me, ob, normals)
		elif (self.halonode):
			self.writeHaloMesh(me, ob, normals)
		else:
			self.writeMeshData(me, ob, normals)

	def writeBillboardMesh(self, me, ob, normals = 0):
		self.writeIndented("Billboard {\n",1)
		self.writeIndented("axisOfRotation 0 1 0\n")
		self.writeIndented("children [\n")

		self.writeMeshData(me, ob, normals)

		self.writeIndented("]\n", -1)
		self.writeIndented("}\n", -1)
		
	def writeHaloMesh(self, me, ob, normals = 0):
		self.writeIndented("Billboard {\n",1)
		self.writeIndented("axisOfRotation 0 0 0\n")
		self.writeIndented("children [\n")

		self.writeMeshData(me, ob, normals)

		self.writeIndented("]\n", -1)
		self.writeIndented("}\n", -1)

	###
	### Here is where real mesh data is written
	### 
	def writeMeshData(self, me, ob, normals = 0):
		meshName = self.cleanStr(me.name)

		if self.meshNames.has_key(meshName):
			self.writeIndented("USE ME_%s\n" % meshName, 0)
			self.meshNames[meshName]+=1
			if (self.verbose == 1):
				print "  Using Mesh %s (Blender mesh: %s)\n" % \
					  (meshName, me.name)
			return
		self.meshNames[meshName]=1

		if (self.verbose == 1):
			print "  Writing Mesh %s (Blender mesh: %s)\n" % \
				  (meshName, me.name)
			return

		self.writeIndented("DEF ME_%s Group {\n" % meshName,1)
		self.writeIndented("children [\n", 1)
			
		hasImageTexture = 0
		issmooth = 0

		maters = me.materials
		nummats = self.getNumMaterials(me)

		# Vertex and Face colors trump materials and image textures
		if (self.facecolors or self.vcolors):
			if nummats > 0:
				if maters[0]:
					self.writeShape(ob, me, 0, None)
				else:
					self.writeShape(ob, me, -1, None)
			else:
				self.writeShape(ob, me, -1, None)
		# Do meshes with materials, possible with image textures
		elif nummats > 0:
			for matnum in range(len(maters)):
				if maters[matnum]:
					images = []
					if me.faceUV:
						images = self.getImages(me, matnum)
						if len(images) > 0:
							for image in images:
								self.writeShape(ob, me, matnum, image)
						else:
							self.writeShape(ob, me, matnum, None)
					else:
						self.writeShape(ob, me, matnum, None)
		else:
			if me.faceUV:
				images = self.getImages(me, -1)
				if len(images) > 0:
					for image in images:
						self.writeShape(ob, me, -1, image)
				else:
					self.writeShape(ob, me, -1, None)
			else:
				self.writeShape(ob, me, -1, None)

			
		self.writeIndented("]\n", -1)
		self.writeIndented("}\n", -1)

	def getImages(self, me, matnum):
		imageNames = {}
		images = []
		for face in me.faces:
			if (matnum == -1) or (face.mat == matnum):
				if (face.image):
					imName = self.cleanStr(face.image.name)
					if not imageNames.has_key(imName):
						images.append(face.image)
						imageNames[imName]=1
		return images

	def getNumMaterials(self, me):
		# Oh silly Blender, why do you sometimes have 'None' as
		# a member of the me.materials array?
		num = 0
		for mat in me.materials:
			if mat:
				num = num + 1
		return num

	def writeCoordinates(self, me, meshName):
		coordName = "coord_%s" % (meshName)
		# look up coord name, use it if available
		if self.coordNames.has_key(coordName):
			self.writeIndented("coord USE %s\n" % coordName, 0)
			self.coordNames[coordName]+=1
			return;
	
		self.coordNames[coordName]=1

		#-- vertices
		self.writeIndented("coord DEF %s Coordinate {\n" % (coordName), 1)
		self.writeIndented("point [\n", 1)
		meshVertexList = me.verts

		for vertex in meshVertexList:
			vrmlvert = vertex.co
			if export_rotate_z_to_y.val:
				vrmlvert = M_blen2vrml * vrmlvert
			self.writeUnindented("%s %s %s\n " % (vrmlvert[0], vrmlvert[1], vrmlvert[2]))
		self.writeIndented("]\n", -1)
		self.writeIndented("}\n", -1)
		self.writeIndented("\n")

	def writeShape(self, ob, me, matnum, image):
		# Note: at this point it is assumed for matnum!=-1 that the 
		# material in me.materials[matnum] is not equal to 'None'.
		# Such validation should be performed by the function that
		# calls this one.
		self.writeIndented("Shape {\n",1)

		self.writeIndented("appearance Appearance {\n", 1)
		if (matnum != -1):
			mater = me.materials[matnum]
			self.writeMaterial(mater, self.cleanStr(mater.name,''))
			if (mater.mode & Blender.Material.Modes['TEXFACE']):
				if image != None:
					self.writeImageTexture(image.name, image.filename)
		else:
			if image != None:
				self.writeImageTexture(image.name, image.filename)

		self.writeIndented("}\n", -1)

		self.writeGeometry(ob, me, matnum, image)

		self.writeIndented("}\n", -1)

	def writeGeometry(self, ob, me, matnum, image):

		#-- IndexedFaceSet or IndexedLineSet
		meshName = self.cleanStr(me.name)

		# check if object is wireframe only
		if (self.wire):
			ifStyle="IndexedLineSet"
		else:
			# user selected BOUNDS=1, SOLID=3, SHARED=4, or TEXTURE=5
			ifStyle="IndexedFaceSet"

		self.writeIndented("geometry %s {\n" % ifStyle, 1)
		if not self.wire:
			if self.twosided == 1:
				self.writeIndented("solid FALSE\n")
			else:
				self.writeIndented("solid TRUE\n")

		self.writeCoordinates(me, meshName)
		self.writeCoordIndex(me, meshName, matnum, image)
		self.writeTextureCoordinates(me, meshName, matnum, image)
		if self.facecolors:
			self.writeFaceColors(me)
		elif self.vcolors:
			self.writeVertexColors(me)
		self.writeIndented("}\n", -1)

	def writeCoordIndex(self, me, meshName, matnum, image):
		meshVertexList = me.verts
		self.writeIndented("coordIndex [\n", 1)
		coordIndexList=[]  
		for face in me.faces:
			if (matnum == -1) or (face.mat == matnum):
				if (image == None) or (face.image == image):
					cordStr=""
					for v in face.verts:
						indx=v.index
						cordStr = cordStr + "%s " % indx
					self.writeUnindented(cordStr + "-1, \n")
		self.writeIndented("]\n", -1)

	def writeTextureCoordinates(self, me, meshName, matnum, image):
		if (image == None):
			return
		
		texCoordList=[] 
		texIndexList=[]
		j=0

		for face in me.faces:
			coordStr = ""
			indexStr = ""
			if (matnum == -1) or (face.mat == matnum):
				if (face.image == image):
					for i in range(len(face.verts)):
						uv = face.uv[i]
						indexStr += "%s " % (j)
						coordStr += "%s %s, " % \
									(round(uv[0], self.tp), \
									 round(uv[1], self.tp))
						j=j+1
					indexStr += "-1"
			texIndexList.append(indexStr)
			texCoordList.append(coordStr)

		self.writeIndented("texCoord TextureCoordinate {\n", 1)
		self.writeIndented("point [\n", 1)
		for coord in texCoordList:
			self.writeUnindented("%s\n" % (coord))
		self.writeIndented("]\n", -1)
		self.writeIndented("}\n", -1)

		self.writeIndented("texCoordIndex [\n", 1)
		for ind in texIndexList:
			self.writeUnindented("%s\n" % (ind))
		self.writeIndented("]\n", -1)

	def writeFaceColors(self, me):
		self.writeIndented("colorPerVertex FALSE\n")
		self.writeIndented("color Color {\n",1)
		self.writeIndented("color [\n", 1)

		for face in me.faces:
			if face.col:
				c=face.col[0]
				if self.verbose >= 2:
					print "Debug: face.col r=%d g=%d b=%d" % (c.r, c.g, c.b)

				aColor = self.rgbToFS(c)
				self.writeUnindented("%s,\n" % aColor)
		self.writeIndented("]\n",-1)
		self.writeIndented("}\n",-1)

	def writeVertexColors(self, me):
		self.writeIndented("colorPerVertex TRUE\n")
		self.writeIndented("color Color {\n",1)
		self.writeIndented("color [\n\t\t\t\t\t\t", 1)

		cols = [None] * len(me.verts)

		for face in me.faces:
			for vind in range(len(face.v)):
				vertex = face.v[vind]
				i = vertex.index
				if cols[i] == None:
					cols[i] = face.col[vind]
					
		for i in range(len(me.verts)):
			aColor = self.rgbToFS(cols[i])
			self.writeUnindented("%s\n" % aColor)

		self.writeIndented("\n", 0)
		self.writeIndented("]\n",-1)
		self.writeIndented("}\n",-1)

	def writeMaterial(self, mat, matName):
		# look up material name, use it if available
		if self.matNames.has_key(matName):
			self.writeIndented("material USE MA_%s\n" % matName)
			self.matNames[matName]+=1
			return;
	
		self.matNames[matName]=1

		ambient = mat.amb/3
		diffuseR, diffuseG, diffuseB = \
				  mat.rgbCol[0], mat.rgbCol[1],mat.rgbCol[2]
		if world:
			ambi = world.getAmb()
			ambi0, ambi1, ambi2 = (ambi[0]*mat.amb) * 2, \
								  (ambi[1]*mat.amb) * 2, \
								  (ambi[2]*mat.amb) * 2
		else:
			ambi0, ambi1, ambi2 = 0, 0, 0
		emisR, emisG, emisB = (diffuseR*mat.emit+ambi0) / 2, \
							  (diffuseG*mat.emit+ambi1) / 2, \
							  (diffuseB*mat.emit+ambi2) / 2

		shininess = mat.hard/512.0
		specR = (mat.specCol[0]+0.001) / (1.25/(mat.getSpec()+0.001))
		specG = (mat.specCol[1]+0.001) / (1.25/(mat.getSpec()+0.001))
		specB = (mat.specCol[2]+0.001) / (1.25/(mat.getSpec()+0.001))
		transp = 1 - mat.alpha
		matFlags = mat.getMode()
		if matFlags & Blender.Material.Modes['SHADELESS']:
			ambient = 1
			shine = 1
			specR = emitR = diffuseR
			specG = emitG = diffuseG
			specB = emitB = diffuseB
		self.writeIndented("material DEF MA_%s Material {\n" % matName, 1)
		self.writeIndented("diffuseColor %s %s %s\n" % \
						   (round(diffuseR,self.cp), \
							round(diffuseG,self.cp), \
							round(diffuseB,self.cp)))
		self.writeIndented("ambientIntensity %s\n" % \
						   (round(ambient,self.cp)))
		self.writeIndented("specularColor %s %s %s\n" % \
						   (round(specR,self.cp), \
							round(specG,self.cp), \
							round(specB,self.cp)))
		self.writeIndented("emissiveColor  %s %s %s\n" % \
						   (round(emisR,self.cp), \
							round(emisG,self.cp), \
							round(emisB,self.cp)))
		self.writeIndented("shininess %s\n" % (round(shininess,self.cp)))
		self.writeIndented("transparency %s\n" % (round(transp,self.cp)))
		self.writeIndented("}\n",-1)

	def writeImageTexture(self, name, filename):
		if self.texNames.has_key(name):
			self.writeIndented("texture USE %s\n" % self.cleanStr(name))
			self.texNames[name] += 1
			return
		else:
			self.writeIndented("texture DEF %s ImageTexture {\n" % \
							   self.cleanStr(name), 1)
			self.writeIndented('url "%s"\n' % \
							   filename.split("\\")[-1].split("/")[-1])
			self.writeIndented("}\n",-1)
			self.texNames[name] = 1

	def writeBackground(self):
		if world:
			worldname = world.getName()
		else:
			return
		blending = world.getSkytype()	
		grd = world.getHor()
		grd0, grd1, grd2 = grd[0], grd[1], grd[2]
		sky = world.getZen()
		sky0, sky1, sky2 = sky[0], sky[1], sky[2]
		mix0, mix1, mix2 = grd[0]+sky[0], grd[1]+sky[1], grd[2]+sky[2]
		mix0, mix1, mix2 = mix0/2, mix1/2, mix2/2
		if worldname in self.namesStandard:
			self.writeIndented("Background {\n",1)
		else:
			self.writeIndented("DEF %s Background {\n" % \
							   self.secureName(worldname),1)
		# No Skytype - just Hor color
		if blending == 0:
			self.writeIndented("groundColor %s %s %s\n" % \
							   (round(grd0,self.cp), \
								round(grd1,self.cp), \
								round(grd2,self.cp)))
			self.writeIndented("skyColor %s %s %s\n" % \
							   (round(grd0,self.cp), \
								round(grd1,self.cp), \
								round(grd2,self.cp)))
		# Blend Gradient
		elif blending == 1:
			self.writeIndented("groundColor [ %s %s %s, " % \
							   (round(grd0,self.cp), \
								round(grd1,self.cp), \
								round(grd2,self.cp)))
			self.writeIndented("%s %s %s ]\n" % \
							   (round(mix0,self.cp), \
								round(mix1,self.cp), \
								round(mix2,self.cp)))
			self.writeIndented("groundAngle [ 1.57, 1.57 ]\n")
			self.writeIndented("skyColor [ %s %s %s, " % \
							   (round(sky0,self.cp), \
								round(sky1,self.cp), \
								round(sky2,self.cp)))
			self.writeIndented("%s %s %s ]\n" % \
							   (round(mix0,self.cp), \
								round(mix1,self.cp), \
								round(mix2,self.cp)))
			self.writeIndented("skyAngle [ 1.57, 1.57 ]\n")
		# Blend+Real Gradient Inverse
		elif blending == 3:
			self.writeIndented("groundColor [ %s %s %s, " % \
							   (round(sky0,self.cp), \
								round(sky1,self.cp), \
								round(sky2,self.cp)))
			self.writeIndented("%s %s %s ]\n" % \
							   (round(mix0,self.cp), \
								round(mix1,self.cp), \
								round(mix2,self.cp)))
			self.writeIndented("groundAngle [ 1.57, 1.57 ]\n")
			self.writeIndented("skyColor [ %s %s %s, " % \
							   (round(grd0,self.cp), \
								round(grd1,self.cp), \
								round(grd2,self.cp)))
			self.writeIndented("%s %s %s ]\n" % \
							   (round(mix0,self.cp), \
								round(mix1,self.cp), \
								round(mix2,self.cp)))
			self.writeIndented("skyAngle [ 1.57, 1.57 ]\n")
		# Paper - just Zen Color
		elif blending == 4:
			self.writeIndented("groundColor %s %s %s\n" % \
							   (round(sky0,self.cp), \
								round(sky1,self.cp), \
								round(sky2,self.cp)))
			self.writeIndented("skyColor %s %s %s\n" % \
							   (round(sky0,self.cp), \
								round(sky1,self.cp), \
								round(sky2,self.cp)))
		# Blend+Real+Paper - komplex gradient
		elif blending == 7:
			self.writeIndented("groundColor [ %s %s %s, " % \
							   (round(sky0,self.cp), \
								round(sky1,self.cp), \
								round(sky2,self.cp)))
			self.writeIndented("%s %s %s ]\n" % \
							   (round(grd0,self.cp), \
								round(grd1,self.cp), \
								round(grd2,self.cp)))
			self.writeIndented("groundAngle [ 1.57, 1.57 ]\n")
			self.writeIndented("skyColor [ %s %s %s, " % \
							   (round(sky0,self.cp), \
								round(sky1,self.cp), \
								round(sky2,self.cp)))
			self.writeIndented("%s %s %s ]\n" % \
							   (round(grd0,self.cp),
								round(grd1,self.cp),
								round(grd2,self.cp)))
			self.writeIndented("skyAngle [ 1.57, 1.57 ]\n")
		# Any Other two colors
		else:
			self.writeIndented("groundColor %s %s %s\n" % \
							   (round(grd0,self.cp), \
								round(grd1,self.cp), \
								round(grd2,self.cp)))
			self.writeIndented("skyColor %s %s %s\n" % \
							   (round(sky0,self.cp), \
								round(sky1,self.cp), \
								round(sky2,self.cp)))
		alltexture = len(worldmat)
		for i in xrange(alltexture):
			namemat = worldmat[i].getName()
			pic = worldmat[i].getImage()
			if pic:
				# Stripped path.
				pic_path= pic.filename.split('\\')[-1].split('/')[-1]
				if namemat == "back":
					self.writeIndented('backUrl "%s"\n' % pic_path)
				elif namemat == "bottom":
					self.writeIndented('bottomUrl "%s"\n' % pic_path)
				elif namemat == "front":
					self.writeIndented('frontUrl "%s"\n' % pic_path)
				elif namemat == "left":
					self.writeIndented('leftUrl "%s"\n' % pic_path)
				elif namemat == "right":
					self.writeIndented('rightUrl "%s"\n' % pic_path)
				elif namemat == "top":
					self.writeIndented('topUrl "%s"\n' % pic_path)
		self.writeIndented("}",-1)
		self.writeIndented("\n\n")

	def writeLamp(self, ob):
		la = ob.data
		laType = la.getType()

		if laType == Lamp.Types.Lamp:
			self.writePointLight(ob, la)
		elif laType == Lamp.Types.Spot:
			self.writeSpotLight(ob, la)
		elif laType == Lamp.Types.Sun:
			self.writeDirectionalLight(ob, la)
		else:
			self.writeDirectionalLight(ob, la)

	def writeObject(self, ob):

		obname = self.cleanStr(ob.name)

		try:
			obtype=ob.getType()
		except AttributeError:
			print "Error: Unable to get type info for %s" % obname
			return

		if self.verbose >= 1:
			print "++ Writing %s object %s (Blender name: %s)\n" % \
				  (obtype, obname, ob.name)

		# Note: I am leaving empties out for now -- the original
		# script does some really weird stuff with empties
		if ( (obtype != "Camera") and \
			 (obtype != "Mesh") and \
			 (obtype != "Lamp") ):
			print "Info: Ignoring [%s], object type [%s] " \
				  "not handle yet" % (obname, obtype)
			return

		ob_matrix = Mathutils.Matrix(ob.getMatrix('worldspace'))
		if export_rotate_z_to_y.val:
			matrix = M_blen2vrml * ob_matrix * M_vrml2blen
		else:
			matrix = ob_matrix
		e      = matrix.rotationPart().toEuler()

		v = matrix.translationPart()
		(axis, angle) = self.eulToVecRot(self.deg2rad(e.x), \
										 self.deg2rad(e.y), \
										 self.deg2rad(e.z))

		mrot = e.toMatrix().resize4x4()
		try:
			mrot.invert()
		except:
			print "Warning: %s has degenerate transformation!" % (obname)
			return
		
		diag = matrix * mrot
		sizeX = diag[0][0]
		sizeY = diag[1][1]
		sizeZ = diag[2][2]

		if self.verbose >= 1:
			print "  Transformation:\n" \
				  "    loc:  %f %f %f\n" \
				  "    size: %f %f %f\n" \
				  "    Rot:  (%f %f %f), %f\n" % \
				  (v.x, v.y, v.z, \
				   sizeX, sizeY, sizeZ, \
				   axis[0], axis[1], axis[2], angle)

		self.writeIndented("DEF OB_%s Transform {\n" % (obname), 1)
		self.writeIndented("translation %f %f %f\n" % \
						   (v.x, v.y, v.z) )

		self.writeIndented("rotation %f %f %f %f\n" % \
						   (axis[0],axis[1],axis[2],angle) )
		
		self.writeIndented("scale %f %f %f\n" % \
						   (sizeX, sizeY, sizeZ) )

		self.writeIndented("children [\n", 1)

		self.writeObData(ob)

		self.writeIndented("]\n", -1) # end object
		self.writeIndented("}\n", -1) # end object

	def writeObData(self, ob):

		obtype = ob.getType()

		if obtype == "Camera":
			self.writeViewpoint(ob)
		elif obtype == "Mesh":
			self.writeMesh(ob)
		elif obtype == "Lamp":
			self.writeLamp(ob)
		elif obtype == "Empty":
			self.writeNode(ob)


##########################################################
# export routine
##########################################################

	def export(self, scene, world, worldmat):
		print "Info: starting VRML97 export to " + self.filename + "..."
		self.writeHeader()
		self.writeScript()
		self.writeNavigationInfo(scene)
		self.writeBackground()
		self.writeFog()
		self.proto = 0
		allObj = []
		if export_selection_only.val:
			allObj = list(scene.objects.context)
		else:
			allObj = list(scene.objects)
			self.writeInline()

		for thisObj in allObj:
			self.writeObject(thisObj)

		if not export_selection_only.val:
			self.writeScript()
		self.cleanup()

##########################################################
# Utility methods
##########################################################

	def cleanup(self):
		self.file.close()
		self.texNames={}
		self.matNames={}
		self.indentLevel=0
		print "Info: finished VRML97 export to %s\n" % self.filename

	def cleanStr(self, name, prefix='rsvd_'):
		"""cleanStr(name,prefix) - try to create a valid VRML DEF \
		name from object name"""

		newName=name[:]
		if len(newName) == 0:
			self.nNodeID+=1
			return "%s%d" % (prefix, self.nNodeID)
		
		if newName in self.namesReserved:
			newName='%s%s' % (prefix,newName)
		
		if newName[0].isdigit():
			newName='%s%s' % ('_',newName)

		for bad in (' ','"','#',"'",',','.','[','\\',']','{','}'):
			newName=newName.replace(bad,'_')
		return newName

	def rgbToFS(self, c):
		s = "%s %s %s" % \
			(round(c.r/255.0,self.cp), \
			 round(c.g/255.0,self.cp), \
			 round(c.b/255.0,self.cp))
		return s

	def rad2deg(self, v):
		return round(v*180.0/math.pi,4)

	def deg2rad(self, v):
		return (v*math.pi)/180.0;

	def eulToVecRot(self, RotX, RotY, RotZ):
		
		ti = RotX*0.5
		tj = RotY*0.5
		th = RotZ*0.5

		ci = math.cos(ti)
		cj = math.cos(tj)
		ch = math.cos(th)
		si = math.sin(ti)
		sj = math.sin(tj)
		sh = math.sin(th)
		cc = ci*ch
		cs = ci*sh
		sc = si*ch
		ss = si*sh
        
		q0 = cj*cc + sj*ss
		q1 = cj*sc - sj*cs
		q2 = cj*ss + sj*cc
		q3 = cj*cs - sj*sc

		angle = 2 * math.acos(q0)
		if (math.fabs(angle) < 0.000001):
			axis = [1.0, 0.0, 0.0]
		else:
			sphi = 1.0/math.sqrt(1.0 - (q0*q0))
			axis = [q1 * sphi, q2 * sphi, q3 * sphi]

		a = Mathutils.Vector(axis)
		a.normalize()
		return ([a.x, a.y, a.z], angle)


	# For writing well formed VRML code
	#----------------------------------
	def writeIndented(self, s, inc=0):
		if inc < 1:
			self.indentLevel = self.indentLevel + inc
		
		self.file.write( self.indentLevel*"\t" + s)

		if inc > 0:
			self.indentLevel = self.indentLevel + inc

	# Sometimes better to not have too many
	# tab characters in a long list, for file size
	#----------------------------------
	def writeUnindented(self, s):
		self.file.write(s)

##########################################################
# Callbacks, needed before Main
##########################################################

def select_file(filename):
	if sys.exists(filename) and _safeOverwrite:
		result = \
			Draw.PupMenu("File Already Exists, Overwrite?%t|Yes%x1|No%x0")
		if(result != 1):
			return

	if not filename.endswith(extension):
		filename += extension

	wrlexport=VRML2Export(filename)
	wrlexport.export(scene, world, worldmat)

#########################################################
# UI and Registry utilities
#########################################################

export_selection_only = Draw.Create(0)
export_rotate_z_to_y = Draw.Create(0)
export_compressed = Draw.Create(0)

def save_to_registry():
	d = {}
	d['selection_only'] = export_selection_only.val
	d['rotate_z_to_y'] = export_rotate_z_to_y.val
	d['compressed'] = export_compressed.val
	Registry.SetKey('vrml97_export', d, True)

def load_from_registry():
	d = Registry.GetKey('vrml97_export', True)
	if d:
		try:
			export_selection_only.val = d['selection_only']
			export_rotate_z_to_y.val = d['rotate_z_to_y']
			export_compressed.val = d['compressed']
		except: save_to_registry() # If data is not valid, rewrite it.

def show_popup():
	pup_block = [
		('Selection Only', export_selection_only, 'Only export objects in visible selection. Else export whole scene.'),
		('Rotate +Z to +Y', export_rotate_z_to_y, 'Rotate such that +Z axis (Blender up) becomes +Y (VRML up).'),
		('Compress', export_compressed, 'Generate a .wrz file (normal VRML compressed by gzip).')
		]
	return Draw.PupBlock('Export VRML 97...', pup_block) 

#########################################################
# main routine
#########################################################

load_from_registry()

# Note that show_popup must be done before Blender.Window.FileSelector,
# because export_compressed affects the suggested extension of resulting
# file.

if show_popup():
	save_to_registry()
	if export_compressed.val:
		try:
			extension=".wrz"
			from gzip import *
		except:
			print "could not import gzip, file will be exported uncompressed"
			pass
	else:
		extension=".wrl"
	Blender.Window.FileSelector(select_file, "Export VRML97", \
								sys.makename(ext=extension))
