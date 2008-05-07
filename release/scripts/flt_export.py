#!BPY
""" Registration info for Blender menus:
Name: 'OpenFlight (.flt)...'
Blender: 245
Group: 'Export'
Tip: 'Export to OpenFlight v16.0 (.flt)'
"""

__author__ = "Greg MacDonald, Geoffrey Bantle"
__version__ = "2.0 11/21/07"
__url__ = ("blender", "blenderartists.org", "Author's homepage, http://sourceforge.net/projects/blight/")
__bpydoc__ = """\
This script exports v16.0 OpenFlight files.  OpenFlight is a
registered trademark of MultiGen-Paradigm, Inc.

Feature overview and more availible at:
http://wiki.blender.org/index.php/Scripts/Manual/Export/openflight_flt
"""

# flt_export.py is an OpenFlight exporter for blender.
#
# Copyright (C) 2005 Greg MacDonald, 2007 Blender Foundation.
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

import Blender
from Blender import Modifier
import os.path
import flt_properties
import flt_defaultp as defaultp
from flt_filewalker import FltOut
from flt_filewalker import FileFinder
from flt_properties import *
import shutil

FF = FileFinder()
records = process_recordDefs()
		
class ExporterOptions:
	
	def read_state(self):
		reg = Blender.Registry.GetKey('flt_export',1)
		if reg:
			for key in self.state:
				if reg.has_key(key):
					self.state[key] = reg[key]
	
	def write_state(self):
		d = dict()
		for key in self.state:
			d[key] = self.state[key]
			Blender.Registry.SetKey('flt_export', d, 1) 
	def __init__(self):
		self.verbose = 1
		self.tolerance = 0.001
		self.writevcol = True
		
		self.state = {'export_shading' : 0, 
				'shading_default' : 45, 
				'basepath' : os.path.dirname(Blender.Get('filename')),
				'scale': 1.0,
				'doxrefs' : 1,
				'attrib' : 0,
				'copytex' : 0,
				'transform' : 0,
				'xapp' : 1}
				
		#default externals path
		if(os.path.exists(os.path.join(self.state['basepath'],'externals'))):
			self.state['externalspath'] = os.path.join(self.state['basepath'],'externals')
		else:
			self.state['externalspath'] = self.state['basepath'] 
		
		if(os.path.exists(os.path.join(self.state['basepath'],'textures'))):
			self.state['texturespath'] = os.path.join(self.state['basepath'],'textures')
		else:
			self.state['texturespath'] = self.state['basepath']

		self.state['xappath'] = ''
		self.read_state() #read from registry
		
		
options = ExporterOptions()
tex_files = dict() #a list of (possibly) modified texture path names

tex_layers = ['Layer0', 'Layer1', 'Layer2', 'Layer3', 'Layer4', 'Layer5', 'Layer6', 'Layer7']
mask = 2147483648
mtexmasks = []
for i in xrange(7):
	mtexmasks.append(mask)
	mask = mask / 2

FLOAT_TOLERANCE = options.tolerance

#need to move all this stuff to flt_properties.py.
identity_matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]
alltypes = [2,4,14,11,73,63,111]
childtypes = { 
	2 : [111,2,73,4,14,63],
	4 : [111],
	73 : [111,2,73,4,14,63],
	63 : [],
	14 : [111,2,73,4,14,63],
	111 : []
}
recordlen = {
	2: 44,
	4: 28,
	73: 80,
	63: 216,
	14: 384,
	111: 156
}

def is_identity(m):
	for i in xrange(4):
		for j in xrange(4):
			if abs(m[i][j] - identity_matrix[i][j]) > FLOAT_TOLERANCE:
				return False
	return True

class MaterialDesc:
	def __init__(self):
		self.name = 'Blender'

		# Colors, List of 3 floats.
		self.diffuse = [1.0, 1.0, 1.0]
		self.specular = [1.0, 1.0, 1.0]

		# Scalars
		self.ambient = 0.1 # [0.0, 1.0]
		self.emissive = 0.0 # [0.0, 1.0]
		self.shininess = 32.0 # Range is [0.0, 128.0]
		self.alpha = 1.0 # Range is [0.0, 1.0]

class VertexDesc:
	def __init__(self, co=None, no=None, uv=None, fltindex=None,cindex=None):
		if co: self.x, self.y, self.z = tuple(co)
		else: self.x = self.y = self.z = 0.0
		if no: self.nx, self.ny, self.nz = tuple(no)
		else: self.nx = self.ny = self.nz = 0.0
		if uv: self.u, self.v = tuple(uv)
		else: self.u = self.v = 0.0
		if cindex: self.cindex = cindex
		else: self.cindex = 127
		self.fltindex = fltindex
		self.accum = 0

class shadowVert:
	def __init__(self,bvert,object,world,normal):
                global options
		
		self.co = Blender.Mathutils.Vector(bvert.co[0],bvert.co[1],bvert.co[2])
                #if world:
		#	vec = self.co
		#	vec = Blender.Mathutils.Vector(vec[0] * options.scale, vec[1] * options.scale, vec[2] * options.scale) #scale
		#	self.co =  Blender.Mathutils.TranslationMatrix(vec) * (self.co * object.getMatrix('worldspace'))
		
		if normal:
                        #if world:
			#	self.no = Blender.Mathutils.Vector(normal * object.getMatrix('worldspace')).normalize()
			#else:
			self.no = Blender.Mathutils.Vector(normal[0],normal[1],normal[2])
			
		else:
			#if world:
				#self.no = Blender.Mathutils.Vector(bvert.no * object.getMatrix('worldspace')).normalize() 
			#else:
			self.no = Blender.Mathutils.Vector(bvert.no[0],bvert.no[1],bvert.no[2])
			
		#do scaling factor
		#if options.scale != 1.0:
			#self.co[0] = self.co[0] * options.scale
			#self.co[1] = self.co[1] * options.scale
			#self.co[2] = self.co[2] * options.scale
			
		self.index = bvert.index

class GlobalResourceRepository:
	def new_face_name(self):
		self.face_name += 1
		return 'f%i' % (self.face_name-1)

	def vertex_count(self):
		return len(self.vertex_lst)

	def request_vertex_desc(self, i):
		return self.vertex_lst[i]

	def request_vertex_index(self, object, mesh, face, vfindex, uvok,cindex):

		flatShadeNorm = None
		vno = None

		
		if type(face) is list:
			vertex = face[vfindex]
		elif str(type(face)) == "<type " + "'Blender MVert'>": 
			vertex = face
			vno = Blender.Mathutils.Vector(0.0,0.0,1.0)
		elif str(type(face)) == "<type " + "'Blender MEdge'>":
			if vfindex == 1:
				vertex = face.v1
			elif vfindex == 2:
				vertex = face.v2
		elif str(type(face)) == "<type " + "'Blender MFace'>":
                        if not face.smooth:
                                flatShadeNorm = face.no
			vertex = face.v[vfindex]
		else: 
			return None
						
		if not self.namehash.has_key(object.name):
			self.namehash[object.name] = dict()
		indexhash = self.namehash[object.name]
		
		#export in global space? THIS HAS BEEN MADE REDUNDANT... REMOVE ME
		if not options.state['transform']:
			vertex = shadowVert(vertex,object,True,flatShadeNorm)
		else:
                        vertex = shadowVert(vertex,object,False,flatShadeNorm)
		
		if vno:
			vertex.no = vno        
        
     
		#Check to see if this vertex has been visited before. If not, add
		if not indexhash.has_key(vertex.index):
			if uvok:
				newvdesc = VertexDesc(vertex.co, vertex.no, face.uv[vfindex], self.nextvindex,cindex=cindex)
			else:
				newvdesc = VertexDesc(co=vertex.co, no=vertex.no,fltindex=self.nextvindex,cindex=cindex)
			
			indexhash[vertex.index] = [newvdesc]
			self.vertex_lst.append(newvdesc)
			self.nextvindex = self.nextvindex + 1
			return newvdesc.fltindex
		
		else:
			desclist = indexhash[vertex.index]
			if uvok: 
				faceu = face.uv[vfindex][0]
				facev = face.uv[vfindex][1]
			else:
				faceu = 0.0
				facev = 0.0
			for vdesc in desclist:
				if\
				abs(vdesc.x - vertex.co[0]) > FLOAT_TOLERANCE or\
				abs(vdesc.y - vertex.co[1]) > FLOAT_TOLERANCE or\
				abs(vdesc.z - vertex.co[2]) > FLOAT_TOLERANCE or\
				abs(vdesc.nx - vertex.no[0]) > FLOAT_TOLERANCE or\
				abs(vdesc.ny - vertex.no[1]) > FLOAT_TOLERANCE or\
				abs(vdesc.nz - vertex.no[2]) > FLOAT_TOLERANCE or\
				vdesc.cindex != cindex or\
				abs(vdesc.u - faceu) > FLOAT_TOLERANCE or\
				abs(vdesc.v - facev) > FLOAT_TOLERANCE:
					pass
				else:
					return vdesc.fltindex
				
			#if we get this far, we didnt find a match. Add a new one and return
			if uvok:
				newvdesc = VertexDesc(vertex.co, vertex.no, face.uv[vfindex], self.nextvindex,cindex=cindex)
			else:
				newvdesc = VertexDesc(co=vertex.co, no=vertex.no,fltindex=self.nextvindex,cindex=cindex)
			indexhash[vertex.index].append(newvdesc)
			self.vertex_lst.append(newvdesc)
			self.nextvindex = self.nextvindex + 1
			return newvdesc.fltindex
					
			
	def request_texture_index(self, image):
		match = None
		for i in xrange(len(self.texture_lst)):
			if self.texture_lst[i] != image:
				continue
			match = i
			break
		if match != None:
			return match
		else:
			self.texture_lst.append(image)
			return len(self.texture_lst) - 1

	def request_texture_filename(self, index):
		return Blender.sys.expandpath(self.texture_lst[index].getFilename())

	def texture_count(self):
		return len(self.texture_lst)

	def request_material_index(self, desc):
		match = None
		for i in xrange(len(self.material_lst)):
			if self.material_lst[i].diffuse != desc.diffuse:
				continue
			if self.material_lst[i].specular != desc.specular:
				continue
			if self.material_lst[i].ambient != desc.ambient:
				continue
			if self.material_lst[i].emissive != desc.emissive:
				continue
			if self.material_lst[i].shininess != desc.shininess:
				continue
			if self.material_lst[i].alpha != desc.alpha:
				continue
			match = i
			break

		if match != None:
			return i
		else:
			self.material_lst.append(desc)
			return len(self.material_lst) - 1

	def request_material_desc(self, index):
		return self.material_lst[index]

	def material_count(self):
		return len(self.material_lst)

	# Returns not actual index but one that includes intensity information.
	# color_index = 127*intensity + 128*actual_index
	def request_color_index(self, col):
		r,g,b = tuple(col)
		m = max(r, g, b)
		if m > 0.0:
			intensity = m / 1.0
			r = int(round(r/m * 255.0))
			g = int(round(g/m * 255.0))
			b = int(round(b/m * 255.0))
			brightest = [r, g, b]
		else:
			brightest = [255, 255, 255]
			intensity = 0.0

		match = None
		for i in xrange(len(self.color_lst)):
			if self.color_lst[i] != brightest:
				continue

			match = i
			break

		if match != None:
			index = match
		else:
			length = len(self.color_lst)
			if length <= 1024:
				self.color_lst.append(brightest)
				index = length
			else:
				if options.verbose >= 1:
					print 'Warning: Exceeded max color limit.'
				index = 0

		color_index = int(round(127.0*intensity)) + 128*index
		return color_index

	# Returns color from actual index.
	def request_max_color(self, index):
		return self.color_lst[index]

	def color_count(self):
		return len(self.color_lst)

	def __init__(self):
		#Vertex handling
		self.vertex_lst = []
		self.nextvindex = 0
		self.namehash = dict()
		
		self.texture_lst = []
		self.material_lst = []
		self.color_lst = [[255, 255, 255]]
		self.face_name = 0

class Node:
	# Gathers info from blender needed for export.
	# The =[0] is a trick to emulate c-like static function variables
	# that are persistant between calls.
	def blender_export(self, level=[0]):
		if self.object:
			if options.verbose >= 2:
				print '\t' * level[0], self.name, self.object.type
		level[0] += 1
		
		self.children.reverse()
		for child in self.children:
			child.blender_export()
			
		level[0] -= 1

	# Exports this node's info to file.
	def write(self):
		pass

	def write_matrix(self):
		if self.matrix and not is_identity(self.matrix):
			self.header.fw.write_short(49)   # Matrix opcode
			self.header.fw.write_ushort(68)  # Length of record
			for i in xrange(4):
				for j in xrange(4):
					self.header.fw.write_float(self.matrix[i][j])

	def write_push(self):
		self.header.fw.write_short(10)
		self.header.fw.write_ushort(4)

	def write_pop(self):
		self.header.fw.write_short(11)
		self.header.fw.write_ushort(4)
		
	def write_push_extension(self):
		self.header.fw.write_short(21)
		self.header.fw.write_ushort(24)
		self.header.fw.pad(18)
		self.header.fw.write_ushort(0)
		
	def write_pop_extension(self):
		self.header.fw.write_short(22)
		self.header.fw.write_ushort(24)
		self.header.fw.pad(18)
		self.header.fw.write_ushort(0)
		
	def write_longid(self, name):
		length = len(name)
		if length >= 8:
			self.header.fw.write_short(33)              # Long ID opcode
			self.header.fw.write_ushort(length+5)       # Length of record
			self.header.fw.write_string(name, length+1) # name + zero terminator
			
	def write_comment(self,comment):
		length = len(comment)
		if length >= 65535:
			comment = comment[:65530]
			length = len(comment)
		
		pad = (length % 4) - 1
		if pad < 0: 
			pad = None
			reclength = length + 5
		else:
			reclength = length + 5 + pad
		
		self.header.fw.write_short(31)					# Comment Opcode
		self.header.fw.write_ushort(reclength)			# Length of record is 4 + comment length + null terminator + pad
		self.header.fw.write_string(comment,length+1)	# comment + zero terminator
		if pad:
			self.header.fw.pad(pad)						# pad to multiple of 4 bytes
		
	# Initialization sets up basic tree structure.
	def __init__(self, parent, header, object,props):
		global options
		
		self.header = header
		self.object = object
		if object:
			self.name = self.object.name
			if not options.state['transform']:
				oloc = Blender.Mathutils.Vector(object.getLocation('worldspace'))
				vec = Blender.Mathutils.Vector(oloc[0] * options.state['scale'], oloc[1] * options.state['scale'], oloc[2] * options.state['scale']) #scale
				self.matrix =  self.object.getMatrix('worldspace') *  Blender.Mathutils.TranslationMatrix(vec - oloc)			
			else:
				self.matrix = self.object.getMatrix('localspace') #do matrix mult here.
			self.props = props
			self.child_objects = self.header.parenthash[object.name]
		else:
			self.name = 'no name'
			self.matrix = None
			self.props = None
			self.child_objects = self.header.child_objects
		
		self.children = []
		self.parent = parent
		if parent:
			parent.children.append(self)
		
		# Spawn children.
		for child in self.child_objects:			
			if(not child.restrictDisplay):
				childprops = None
				ftype = None
				if not child.properties.has_key('FLT'):
					if child.type == 'Empty':
						if child.DupGroup:
							childprops = FLTXRef.copy()
							ftype = 63
						else:
							childprops = FLTGroup.copy()
							ftype = 2
					elif child.type == 'Mesh':
						if self.header.childhash[child.name] or not child.parent:
							childprops = FLTGroup.copy()
							ftype = 2
						else:
							childprops = FLTObject.copy()
							ftype = 4
							
				else:
					childprops = dict()
					for prop in child.properties['FLT']:
						childprops[prop] = child.properties['FLT'][prop]
					ftype = child.properties['FLT']['type']
				
				if ftype in self.childtypes and ftype in alltypes:
					Newnode = FLTNode(self,header,child,childprops,ftype)
					if child.type == 'Mesh':
						self.header.mnodes.append(Newnode)
class FaceDesc:
	def __init__(self):
		self.vertex_index_lst = []
		self.mface = None
		self.texture_index = -1
		self.material_index = -1
		self.color_index = 127
		self.renderstyle = 0
		self.twoside = 0
		self.name = None #uses next FLT name if not set... fix resolution of conflicts!
		self.billboard = 0
		
		#Multi-Tex info. Dosn't include first UV Layer!
		self.uvlayer = list() #list of list of tuples for UV coordinates.
		self.images = list()  #list of texture indices for seperate UV layers
		self.mtex = list()
		self.subface = None #can either be 'Push' or 'Pop'

def edge_get_othervert(vert, edge):
	if edge.v1 == vert:
		return edge.v2
	elif edge.v2 == vert:
		return edge.v1
	return None

class FLTNode(Node):
	def walkLoop(self, targetvert, startvert, startedge, edgelist, visited, vedges, closeloop):
		loop = [targetvert]
		
		curvert = startvert
		curedge = startedge
		visited[curedge] = True
		found = False
		
		while not found:
			loop.append(curvert)
			disk = vedges[curvert.index]
			if not closeloop:
				if len(disk) == 1:
					visited[curedge] = True
					break
			else:
				if len(disk) < 2: #what?
					visited[curedge] = True
					return None
			
			if disk[0] == curedge:
				curedge = disk[1]
			else:
				curedge = disk[0]
			if curedge.v1.index == curvert.index:
				curvert = curedge.v2
			else:
				curvert = curedge.v1

			visited[curedge] = True
			
			if(curvert == targetvert):
				found = True
		
		return loop
	
	def buildVertFaces(self,vertuse):
		for vert in self.exportmesh.verts:
			if vertuse[vert.index][0] == False and vertuse[vert.index][1] == 0:
				face_desc = FaceDesc()
				face_desc.vertex_index_lst.append(self.header.GRR.request_vertex_index(self.object, self.exportmesh, vert, 0,0,0))
				face_desc.renderstyle = 3
				face_desc.color_index = 227
				self.face_lst.append(face_desc)

	def buildEdgeFaces(self,vertuse):
		for edge in self.exportmesh.edges:
			v1 = vertuse[edge.v1.index]
			v2 = vertuse[edge.v2.index]
			if v1[0] == False and v2[0] == False:
				if v1[1] == 1 and v2[1] == 1:
					face_desc = FaceDesc()
					face_desc.vertex_index_lst.append(self.header.GRR.request_vertex_index(self.object, self.exportmesh, edge, 1, 0,0))
					face_desc.vertex_index_lst.append(self.header.GRR.request_vertex_index(self.object, self.exportmesh, edge, 2, 0,0))					
					face_desc.renderstyle = 3
					face_desc.color_index = 227
					self.face_lst.append(face_desc)


	def vertwalk(self, startvert, loop, disk, visited):
		visited[startvert] = True
		for edge in disk[startvert]:
			othervert = edge_get_othervert(startvert, edge)
			if not visited[othervert]:
				loop.append(othervert)
				self.vertwalk(othervert,loop,disk,visited)

	def buildOpenFacesNew(self, vertuse):
		wireverts = list()
		wiredges = list()
		visited = dict()
		disk = dict()
		loops = list()
		
		for edge in self.exportmesh.edges:
			v1 = vertuse[edge.v1.index]
			v2 = vertuse[edge.v2.index]
			if v1[0] == False and v2[0] == False:
				if v1[1] < 3 and v2[1] < 3:
					wireverts.append(edge.v1)
					wireverts.append(edge.v2)
					wiredges.append(edge)
				
		#build disk data
		for vert in wireverts:
			visited[vert] = False
			disk[vert] = list()
		for edge in wiredges:
			disk[edge.v1].append(edge)
			disk[edge.v2].append(edge)
		
		#first pass: do open faces
		for vert in wireverts:
			if not visited[vert] and vertuse[vert.index][1] == 1:
				visited[vert] = True
				loop = [vert]
				othervert = edge_get_othervert(vert, disk[vert][0])
				self.vertwalk(othervert, loop, disk, visited)
				if len(loop) > 2: loops.append( ('Open', loop) )

		for vert in wireverts:
			if not visited[vert]:
				visited[vert] = True
				loop = [vert]
				othervert = edge_get_othervert(vert,disk[vert][0])
				self.vertwalk(othervert, loop, disk, visited)
				if len(loop) > 2: loops.append( ('closed', loop) )
				
		#now go through the loops and append.
		for l in loops:
			(ftype, loop) = l
			face_desc = FaceDesc()
			for i,vert in enumerate(loop):
				face_desc.vertex_index_lst.append(self.header.GRR.request_vertex_index(self.object,self.exportmesh,loop,i,0,0))
				if ftype  == 'closed':
					face_desc.renderstyle = 2
				else:
					face_desc.renderstyle = 3
				face_desc.color_index = 227
			self.face_lst.append(face_desc)

	def sortFLTFaces(self,a,b):
		aindex = a.getProperty("FLT_ORIGINDEX")
		bindex = b.getProperty("FLT_ORIGINDEX")
		
		if aindex > bindex:
			return 1
		elif aindex < bindex:
			return -1
		return 0

	def buildNormFaces(self):
		
		global options
		meshlayers = self.exportmesh.getUVLayerNames()
		oldlayer = self.exportmesh.activeUVLayer
		uvok = 0
		subfaceok = 0
		subfacelevel = 0
		
		#special case
		if self.exportmesh.faceUV and len(meshlayers) == 1:
			uvok = 1
		elif self.exportmesh.faceUV and tex_layers[0] in meshlayers:
			self.exportmesh.activeUVLayer = tex_layers[0] 
			uvok = 1
		
		#Sort faces according to the subfaces/FLT indices
		if "FLT_ORIGINDEX" in self.exportmesh.faces.properties and "FLT_SFLEVEL" in self.exportmesh.faces.properties:
			exportfaces = list()
			for face in self.exportmesh.faces:
				exportfaces.append(face)
			exportfaces.sort(self.sortFLTFaces)
			subfaceok = 1
		else:
			exportfaces = self.exportmesh.faces
			
		# Faces described as lists of indices into the GRR's vertex_lst.
		for face in exportfaces:
			descs = list()
			#first we export the face as normal
			index_lst = []
			face_v = face.verts
			for i, v in enumerate(face_v):
				index_lst.append(self.header.GRR.request_vertex_index(self.object,self.exportmesh,face,i,uvok,0))
			face_desc = FaceDesc()
			face_desc.vertex_index_lst = index_lst
			face_desc.mface = face
			descs.append(face_desc)
			
			#deal with subfaces			
			if subfaceok:
				fsflevel = face.getProperty("FLT_SFLEVEL")
				for face_desc in descs:
					if fsflevel > subfacelevel:
						face_desc.subface = 'Push'
						subfacelevel = fsflevel
					elif fsflevel < subfacelevel:
						face_desc.subface = 'Pop'
						subfacelevel = fsflevel
		
			
			if uvok and (face.mode & Blender.Mesh.FaceModes.TWOSIDE):
				face_desc.renderstyle = 1
			for face_desc in descs:	
				if "FLT_COL" in self.exportmesh.faces.properties:
					color_index = face.getProperty("FLT_COL")
#					if(color_index < 127):
#						color_index = 127 #sanity check for face color indices
					if(color_index == 0):
						color_index = 127
					face_desc.color_index = color_index
				else:
					face_desc.color_index = 127
				if "FLT_ID" in self.exportmesh.faces.properties:
					face_desc.name = face.getProperty("FLT_ID") #need better solution than this.
				
				if uvok and face.mode & Blender.Mesh.FaceModes["BILLBOARD"]:
					face_desc.billboard = 1
					
				self.face_lst.append(face_desc)
		if uvok:		
			self.exportmesh.activeUVLayer = oldlayer

	def buildTexData(self):
		
		meshlayers = self.exportmesh.getUVLayerNames()
		oldlayer = self.exportmesh.activeUVLayer
		uvok = 0
		
		if self.exportmesh.faceUV and len(meshlayers) == 1:
			uvok = 1
		if self.exportmesh.faceUV and tex_layers[0] in meshlayers:
			self.exportmesh.activeUVLayer = tex_layers[0] 
			uvok = 1
		
		if uvok: 
			#do base layer. UVs have been stored on vertices directly already.
			for i, face in enumerate(self.face_lst):
				if face.mface:
					mface = face.mface
					image = mface.image
					if image != None and mface.mode & Blender.Mesh.FaceModes["TEX"]:
						index = self.header.GRR.request_texture_index(image)
					else:
						index = -1
					face.texture_index = index

			for i, face in enumerate(self.face_lst):
				if face.mface:
					mface_v = face.mface.v
					for v in mface_v:
						face.uvlayer.append([])
			
			for layername in tex_layers[1:]:
				if layername in meshlayers:
					self.exportmesh.activeUVLayer=layername
					for i, face in enumerate(self.face_lst):
						if face.mface:

							face.mtex.append(layername)
							mface = face.mface
							mface_v = mface.v
							image = mface.image
						
							if image != None and mface.mode & Blender.Mesh.FaceModes["TEX"]:
								index = self.header.GRR.request_texture_index(image)
								face.images.append(index)
							else:
								face.images.append(-1)

							for j, v in enumerate(mface_v):
								face.uvlayer[j].append(tuple(mface.uv[j]))
		if uvok:
			self.exportmesh.activeUVLayer = oldlayer
	def blender_export(self):
		global options
		Node.blender_export(self)
		if self.opcode == 111:
			self.exportmesh = Blender.Mesh.New()
			self.exportmesh.getFromObject(self.object.name)			

			for vert in self.exportmesh.verts:
				if not options.state['transform']:
					vec = vert.co
					vec = Blender.Mathutils.Vector(vec[0] * options.state['scale'], vec[1] * options.state['scale'], vec[2] * options.state['scale']) #scale
					vert.co =  Blender.Mathutils.TranslationMatrix(vec) * (vert.co * self.object.getMatrix('worldspace'))						
				
				if options.state['scale'] != 1.0:
					vert.co = vert.co * options.state['scale']

			if("FLT_VCOL") in self.mesh.verts.properties:
				for v in self.exportmesh.verts:
					self.vert_lst.append(self.header.GRR.request_vertex_index(self.object,self.exportmesh,v,0,0,v.getProperty("FLT_VCOL")))
			else:
				for v in self.mesh.verts:
					self.vert_lst.append(self.header.GRR.request_vertex_index(self.object,self.mesh,v,0,0,127))
			
		
		
		elif self.mesh:
                        orig_mesh = self.object.getData(mesh=True)
			self.exportmesh = Blender.Mesh.New()
			default = None


			if options.state['export_shading']:
				mods = self.object.modifiers
				hasedsplit = False
				for mod in mods:
					if mod.type == Blender.Modifier.Types.EDGESPLIT:
						hasedsplit = True
						break
				if not hasedsplit:
					default = mods.append(Modifier.Types.EDGESPLIT)
					default[Modifier.Settings.EDGESPLIT_ANGLE] = options.state['shading_default']
					default[Modifier.Settings.EDGESPLIT_FROM_ANGLE] = True
					default[Modifier.Settings.EDGESPLIT_FROM_SHARP] = False
					self.object.makeDisplayList()

			self.exportmesh.getFromObject(self.object.name)

			#recalculate vertex positions
			for vert in self.exportmesh.verts:
				if not options.state['transform']:
					vec = vert.co
					vec = Blender.Mathutils.Vector(vec[0] * options.state['scale'], vec[1] * options.state['scale'], vec[2] * options.state['scale']) #scale
					vert.co =  Blender.Mathutils.TranslationMatrix(vec) * (vert.co * self.object.getMatrix('worldspace'))						
				
				if options.state['scale'] != 1.0:
					vert.co = vert.co * options.state['scale']			
			
			flipped = self.object.getMatrix('worldspace').determinant()
			
			if not options.state['transform']:
				self.exportmesh.calcNormals()
			

			if default:
                                #remove modifier from list
				mods.remove(default)
				self.object.makeDisplayList()
				
			#build some adjacency data
			vertuse = list()
			wiredges = list()
			openends = list()
			for v in self.exportmesh.verts:
				vertuse.append([False,0])
			
			#build face incidence data
			for face in self.exportmesh.faces:
				for i, v in enumerate(face.verts):
					vertuse[v.index][0] = True

			for edge in self.exportmesh.edges: #count valance
				vertuse[edge.v1.index][1] = vertuse[edge.v1.index][1] + 1
				vertuse[edge.v2.index][1] = vertuse[edge.v2.index][1] + 1

			#create all face types
			self.buildVertFaces(vertuse)
			self.buildEdgeFaces(vertuse)
			self.buildOpenFacesNew(vertuse)
			self.buildNormFaces()
			self.buildTexData()
			
			if not options.state['transform']:
				if flipped < 0:
					for vdesc in self.header.GRR.vertex_lst:
						vdesc.accum = 0
					for face in self.face_lst:
						face.vertex_index_lst.reverse()
						for vert in face.vertex_index_lst:
							self.header.GRR.vertex_lst[vert].accum = 1
							
					for vdesc in self.header.GRR.vertex_lst:
						if vdesc.accum:
							vdesc.nx = vdesc.nx * -1
							vdesc.ny = vdesc.ny * -1
							vdesc.nz = vdesc.nz * -1


	def write_faces(self):
		sublevel = 0
		for face_desc in self.face_lst:
			if face_desc.name:
				face_name = face_desc.name
			else:
				face_name = self.header.GRR.new_face_name()
			
			#grab the alpha value.
			alpha = 0
			if face_desc.texture_index > -1:
				try:
					typestring = os.path.splitext(self.header.GRR.texture_lst[face_desc.texture_index].getFilename())[1]
					if typestring == '.inta' or typestring == '.rgba':
						alpha = 1
				except:
					pass
					
			if not alpha:
				for index in face_desc.images:
					try:
						typestring = os.path.splitext(self.header.GRR.texture_lst[index].getFilename())[1]
						if typestring == '.inta' or typestring == '.rgba':
							alpha = 1
					except:
						pass
						
			if face_desc.billboard:
				alpha = 2
				
			if face_desc.subface:
				if face_desc.subface == 'Push':
					self.header.fw.write_short(19)
					self.header.fw.write_ushort(4)
					sublevel += 1
				else:
					self.header.fw.write_short(20)
					self.header.fw.write_ushort(4)
					sublevel -= 1
			self.header.fw.write_short(5)                                   # Face opcode
			self.header.fw.write_ushort(80)                                 # Length of record
			self.header.fw.write_string(face_name, 8)                       # ASCII ID
			self.header.fw.write_int(-1)                                    # IR color code
			self.header.fw.write_short(0)									# Relative priority
			self.header.fw.write_char(face_desc.renderstyle)                # Draw type
			self.header.fw.write_char(0)                                    # Draw textured white.
			self.header.fw.write_ushort(0)                                  # Color name index
			self.header.fw.write_ushort(0)                                  # Alt color name index
			self.header.fw.write_char(0)                                    # Reserved
			self.header.fw.write_char(alpha)                                    # Template
			self.header.fw.write_short(-1)                                  # Detail tex pat index
			self.header.fw.write_short(face_desc.texture_index)             # Tex pattern index
			self.header.fw.write_short(face_desc.material_index)            # material index
			self.header.fw.write_short(0)                                   # SMC code
			self.header.fw.write_short(0)                                   # Feature 					code
			self.header.fw.write_int(0)                                     # IR material code
			self.header.fw.write_ushort(0)                                  # transparency 0 = opaque
			self.header.fw.write_uchar(0)                                   # LOD generation control
			self.header.fw.write_uchar(0)                                   # line style index
			self.header.fw.write_int(0)                            # Flags
			self.header.fw.write_uchar(2)                                   # Light mode
			#self.header.fw.write_uchar(3)                                   # Light mode

			self.header.fw.pad(7)                                           # Reserved
			self.header.fw.write_uint(0)                                   # Packed color
			self.header.fw.write_uint(0)                                   # Packed alt color
			self.header.fw.write_short(-1)                                  # Tex map index
			self.header.fw.write_short(0)                                   # Reserved
			self.header.fw.write_uint(face_desc.color_index)                # Color index
			self.header.fw.write_uint(127)                                  # Alt color index
			self.header.fw.write_short(0)                                   # Reserved
			self.header.fw.write_short(-1)                                  # Shader index

			self.write_longid(face_name)
			
			
			#Write Multitexture field if appropriate
			mtex = len(face_desc.mtex)
			if mtex:
				uvmask = 0
				for layername in face_desc.mtex:
					mask = mtexmasks[tex_layers.index(layername)-1]
					uvmask |= mask
				self.header.fw.write_ushort(52)									# MultiTexture Opcode
				self.header.fw.write_ushort(8 + (mtex * 8))		# Length
				self.header.fw.write_uint(uvmask)								# UV mask
				for i in xrange(mtex):
					self.header.fw.write_ushort(face_desc.images[i])			# Tex pattern index
					self.header.fw.write_ushort(0)								# Tex effect
					self.header.fw.write_ushort(0)								# Tex Mapping index
					self.header.fw.write_ushort(0)								# Tex data. User defined
			
			self.write_push()

			# Vertex list record
			self.header.fw.write_short(72)                        # Vertex list opcode
			num_verts = len(face_desc.vertex_index_lst)
			self.header.fw.write_ushort(4*num_verts+4)            # Length of record

			for vert_index in face_desc.vertex_index_lst:
				# Offset into vertex palette
				self.header.fw.write_int(vert_index*64+8)
			
			#UV list record
			if mtex:
				#length = 8 + (numverts * multitex * 8)
				self.header.fw.write_ushort(53)									# UV List Ocode
				self.header.fw.write_ushort(8 + (num_verts*mtex*8))				# Record Length
				self.header.fw.write_uint(uvmask)								# UV mask
				for i, vert_index in enumerate(face_desc.vertex_index_lst):
					for uv in face_desc.uvlayer[i]:
						self.header.fw.write_float(uv[0])						#U coordinate
						self.header.fw.write_float(uv[1])						#V coordinate				
			self.write_pop()
		#clean up faces at the end of meshes....
		if sublevel:
			self.header.fw.write_short(20)
			self.header.fw.write_ushort(4)

	def write_lps(self):
		# Vertex list record
		self.write_push()
		self.header.fw.write_short(72)                        # Vertex list opcode
		num_verts = len(self.vert_lst)
		self.header.fw.write_ushort(4*num_verts+4)            # Length of record

		for vert_index in self.vert_lst:
			# Offset into vertex palette
			self.header.fw.write_int(vert_index*64+8)
		self.write_pop()
	def write(self):
		self.header.fw.write_short(self.opcode)
		self.header.fw.write_ushort(recordlen[self.opcode])
		exportdict = FLT_Records[self.opcode].copy()
		if self.object:
			self.props['3t8!id'] = self.object.name[:7]
		for key in exportdict.keys():
			if self.props.has_key(key):
				exportdict[key] = self.props[key]

                if self.opcode == 63 and options.state['externalspath']:
				try:
					exportdict['3t200!filename'] = os.path.join(options.state['externalspath'],self.object.DupGroup.name+'.flt')
					self.header.xrefnames.append(self.object.DupGroup.name)
				except:
					pass
		
		for key in records[self.opcode]:
			(ftype,length,propname) = records[self.opcode][key]
			write_prop(self.header.fw,ftype,exportdict[propname],length)
		
		if self.props.has_key('comment'):
			self.write_comment(self.props['comment'])

		if self.object and self.object.properties.has_key('FLT') and self.object.properties['FLT'].has_key('EXT'):
			datalen = len(self.object.properties['FLT']['EXT']['data'])
			self.write_push_extension()
			self.header.fw.write_short(100)
			self.header.fw.write_ushort(24 + datalen)
			for key in records[100]:
				(ftype,length,propname) = records[100][key]
				write_prop(self.header.fw,ftype,self.object.properties['FLT']['EXT'][propname],length)
			#write extension data
			for i in xrange(datalen):
				self.header.fw.write_char(self.object.properties['FLT']['EXT']['data'][i])
			self.write_pop_extension()


		self.write_longid(self.name) #fix this!
		
		if options.state['transform'] or self.opcode == 63:
			#writing transform matrix....
			self.write_matrix()

		if self.opcode == 111:
			self.write_lps()
		elif self.face_lst != [] or self.children:
			self.write_push()
			if self.face_lst != []:
				#self.write_push()
				self.write_faces()
				#self.write_pop()
		
			if self.children:
				#self.write_push()
				for child in self.children:
					child.write()
				#self.write_pop()
			self.write_pop()
			
	def __init__(self, parent, header, object,props,ftype):
		self.opcode = ftype #both these next two lines need to be in the node class....
		self.childtypes = childtypes[self.opcode]
		Node.__init__(self, parent, header, object,props)
		self.face_lst = []
		self.vert_lst = [] #for light points.
		self.mesh = None
		self.uvlayer = 0
		self.flipx = False
		self.flipy = False
		self.flipz = False
		
				
		if self.object.type == 'Mesh':
			self.mesh = self.object.getData(mesh=True)
			if(self.mesh.faceUV):
				self.uvLayer = len(self.mesh.getUVLayerNames())

class Database(Node):
	def write_header(self):
		if options.verbose >= 2:
			print 'Writing header.'
		self.fw.write_short(1)          # Header opcode
		self.fw.write_ushort(324)       # Length of record
		self.fw.write_string('db', 8)   # ASCII ID
		self.fw.write_int(1600)         # Revision Number
		self.fw.pad(44)
		self.fw.write_short(1)          # Unit multiplier.
		self.fw.write_char(0)           # Units, 0 = meters
		self.fw.write_char(0)           # texwhite on new faces 0 = false
		self.fw.write_uint(0x80000000)  # misc flags set to saving vertex normals
		self.fw.pad(24)
		self.fw.write_int(0)            # projection type, 0 = flat earth
		self.fw.pad(30)
		self.fw.write_short(1)          # double precision
		self.fw.write_int(100)			# database origin type
		self.fw.pad(88)
		try:
			self.fw.write_double(self.header.scene.properties['FLT']['origin lat'])	#database origin lattitude
		except:
			self.fw.write_double(0)
		try:
			self.fw.write_double(self.header.scene.properties['FLT']['origin lon']) #database origin longitude
		except:
			self.fw.write_double(0)
		self.fw.pad(32)
		self.fw.write_int(0)            # ellipsoid model, 0 = WSG 1984
		
		self.fw.pad(52)

	def write_vert_pal(self):
		if options.verbose >= 2:
			print 'Writing vertex palette.'
		# Write record for vertex palette
		self.fw.write_short(67)                             # Vertex palette opcode.
		self.fw.write_short(8)                              # Length of record
		self.fw.write_int(self.GRR.vertex_count() * 64 + 8) # Length of everything.
		# Write records for individual vertices.
		for i in xrange(self.GRR.vertex_count()):
			desc = self.GRR.request_vertex_desc(i)
			self.fw.write_short(70)                         # Vertex with color normal and uv opcode.
			self.fw.write_ushort(64)                        # Length of record
			self.fw.write_ushort(0)							# Color name index
			self.fw.write_short(0x20000000)					# Flags
			self.fw.write_double(desc.x)
			self.fw.write_double(desc.y)
			self.fw.write_double(desc.z)
			self.fw.write_float(desc.nx)
			self.fw.write_float(desc.ny)
			self.fw.write_float(desc.nz)
			self.fw.write_float(desc.u)
			self.fw.write_float(desc.v)
			self.fw.pad(4)
			self.fw.write_uint(desc.cindex)
			self.fw.pad(4)

	def write_tex_pal(self):
		if options.verbose >= 2:
			print 'Writing texture palette.'
		# Write record for texture palette
		for i, img in enumerate(self.GRR.texture_lst):
			filename = tex_files[img.name]
			self.fw.write_short(64)                                         # Texture palette opcode.
			self.fw.write_short(216)                                        # Length of record
			self.fw.write_string(filename, 200) # Filename
			self.fw.write_int(i)                                            # Texture index
			self.fw.write_int(0)                                            # X
			self.fw.write_int(0)                                            # Y

	def write_mat_pal(self):
		if options.verbose >= 2:
			print 'Writing material palette.'
		for i in xrange(self.GRR.material_count()):
			desc = self.GRR.request_material_desc(i)
			self.fw.write_short(113)                # Material palette opcode.
			self.fw.write_short(84)                 # Length of record
			self.fw.write_int(i)                    # Material index
			self.fw.write_string(desc.name, 12)     # Material name
			self.fw.write_uint(0x80000000)          # Flags
			self.fw.write_float(desc.ambient[0])    # Ambient color.
			self.fw.write_float(desc.ambient[1])    # Ambient color.
			self.fw.write_float(desc.ambient[2])    # Ambient color.
			self.fw.write_float(desc.diffuse[0])    # Diffuse color.
			self.fw.write_float(desc.diffuse[1])    # Diffuse color.
			self.fw.write_float(desc.diffuse[2])    # Diffuse color.
			self.fw.write_float(desc.specular[0])   # Specular color.
			self.fw.write_float(desc.specular[1])   # Specular color.
			self.fw.write_float(desc.specular[2])   # Specular color.
			self.fw.write_float(desc.emissive[0])   # Emissive color.
			self.fw.write_float(desc.emissive[1])   # Emissive color.
			self.fw.write_float(desc.emissive[2])   # Emissive color.
			self.fw.write_float(desc.shininess)
			self.fw.write_float(desc.alpha)
			self.fw.write_int(0)                    # Reserved

	def write_col_pal(self):
		if options.verbose >= 2:
			print 'Writing color palette.'
		self.fw.write_short(32)                     # Color palette opcode.
		self.fw.write_short(4228)                   # Length of record
		self.fw.pad(128)
		try:
			cpalette = self.scene.properties['FLT']['Color Palette']
		except:
			cpalette = defaultp.pal
		count = len(cpalette)
		for i in xrange(count):
			color = struct.unpack('>BBBB',struct.pack('>I',cpalette[i]))
			self.fw.write_uchar(color[3])               # alpha
			self.fw.write_uchar(color[2])               # b
			self.fw.write_uchar(color[1])               # g
			self.fw.write_uchar(color[0])               # r
		self.fw.pad(max(4096-count*4, 0))

	def write(self):
		self.write_header()
		self.write_vert_pal()
		self.write_tex_pal()
		self.write_mat_pal()
		self.write_col_pal()

		self.write_push()
		
		for child in self.children:
			child.write()
		self.write_pop()
	
	def export_textures(self,texturepath):
		for i in xrange(self.GRR.texture_count()):
			texture = self.GRR.texture_lst[i]
			
			if options.state['copytex']:
				filename = os.path.normpath(os.path.join(options.state['texturespath'], os.path.basename(self.GRR.request_texture_filename(i))))
			else:
				filename = os.path.normpath(self.GRR.request_texture_filename(i))
			
			tex_files[texture.name] = filename

	def blender_export(self):
		Node.blender_export(self)
		self.export_textures(self)
		return self.xrefnames
	def __init__(self, scene, fw):
		self.fw = fw
		self.opcode = 1
		self.childtypes = [73,14,2,63]
		self.scene = scene
		self.childhash = dict()
		self.parenthash = dict()
		self.child_objects = list()
		self.mnodes = list()
		self.xrefnames = list()
		for i in self.scene.objects:
			self.parenthash[i.name] = list()
			self.childhash[i.name] = False
		for i in self.scene.objects:
			if i.parent:
				self.childhash[i.parent.name] = True
				self.parenthash[i.parent.name].append(i)
			else:
				self.child_objects.append(i)

		self.GRR = GlobalResourceRepository()
		Node.__init__(self, None, self, None,None)

def write_attribute_files():
	for imgname in tex_files:
		blentex = Blender.Image.Get(imgname)
		exportdict = FLT_Records['Image'].copy()
		
		if blentex.properties.has_key('FLT'):
			for key in exportdict.keys():
				if blentex.properties.has_key(key):
					exportdict[key] = blentex.properties['FLT'][key]
		
		# ClampX/Y override
		if blentex.clampX:
			exportdict['11i!WrapU'] = 1
		if blentex.clampY:
			exportdict['12i!WrapV'] = 1 
		
		exportdict['16i!Enviorment'] = 0 
		
		# File type
		typecode = 0
		try:
			typestring = os.path.splitext(blentex.getFilename())[1]
			
			if typestring == '.rgba':
				typecode = 5
			elif typestring == '.rgb':
				typecode = 4
			elif typestring == '.inta':
				typecode = 3
			elif typestring == '.int':
				typecode = 2
		except:
			pass
		
		exportdict['7i!File Format'] = typecode

		fw = FltOut(tex_files[imgname] + '.attr')
		size = blentex.getSize()
		fw.write_int(size[0])
		fw.write_int(size[1])
		for key in records['Image']:
			(ftype,length,propname) = records['Image'][key]
			write_prop(fw,ftype,exportdict[propname],length)
		fw.close_file()

#globals used by the scene export function
exportlevel = None
xrefsdone = None

def dbexport_internal(scene):
	global exportlevel
	global xrefsdone
	global options

	if exportlevel == 0 or not options.state['externalspath']:
		fname = os.path.join(options.state['basepath'],scene.name + '.flt')
	else:
		fname = os.path.join(options.state['externalspath'],scene.name + '.flt')
	
	fw = FltOut(fname)
	db = Database(scene,fw)
	
	if options.verbose >= 1:
		print 'Pass 1: Exporting ', scene.name,'.flt from Blender.\n'
	
	xreflist = db.blender_export()
	if options.verbose >= 1:
		print 'Pass 2: Writing %s\n' % fname
	db.write()
	fw.close_file()
	
	if options.state['doxrefs']:
		for xname in xreflist:
			try:
				xrefscene = Blender.Scene.Get(xname)
			except:
				xrefscene = None
			if xrefscene and xname not in xrefsdone:
				xrefsdone.append(xname)
				exportlevel+=1
				dbexport_internal(xrefscene)
				exportlevel-=1
	return fname
#main database export function
def dbexport():
	global exportlevel
	global xrefsdone
	exportlevel = 0
	xrefsdone = list()
	
	Blender.Window.WaitCursor(True)
	time1 = Blender.sys.time() # Start timing
	
	if options.verbose >= 1:
		print '\nOpenFlight Exporter'
		print 'Version:', __version__
		print 'Author: Greg MacDonald, Geoffrey Bantle'
		print __url__[2]
		print
	
	fname = dbexport_internal(Blender.Scene.GetCurrent())
	if options.verbose >=1:
		print 'Done in %.4f sec.\n' % (Blender.sys.time() - time1)
	Blender.Window.WaitCursor(False)
	
	#optional: Copy textures
	if options.state['copytex']:
		for imgname in tex_files:
			#Check to see if texture exists in target directory
			if not os.path.exists(tex_files[imgname]):
				#Get original Blender file name
				origpath = Blender.sys.expandpath(Blender.Image.Get(imgname).getFilename())
				#copy original to new
				if os.path.exists(origpath):
					shutil.copyfile(origpath,tex_files[imgname])
	
	#optional: Write attribute files
	if options.state['attrib']:
		write_attribute_files()

	if options.state['xapp']:
		cmd= options.state['xappath'] + " " + fname 
		status = os.system(cmd)
	

#Begin UI code
FLTExport = None
FLTClose = None
FLTLabel = None

FLTBaseLabel = None
FLTTextureLabel = None
FLTXRefLabel = None

FLTBaseString = None
FLTTextureString = None
FLTXRefString = None

FLTBasePath = None
FLTTexturePath = None
FLTXRefPath = None

FLTShadeExport = None
FLTShadeDefault = None

FLTCopyTex = None
FLTDoXRef = None
FLTGlobal = None

FLTScale = None

FLTXAPP = None
FLTXAPPath = None
FLTXAPPString = None
FLTXAPPLabel = None
FLTXAPPChooser = None

FLTAttrib = None

def setshadingangle(ID,val):
	global options
	options.state['shading_default'] = val
def setBpath(fname):
	global options
	options.state['basepath'] = os.path.dirname(fname)
	#update xref and textures path too....
	if(os.path.exists(os.path.join(options.state['basepath'],'externals'))):
		options.state['externalspath'] = os.path.join(options.state['basepath'],'externals')
	if(os.path.exists(os.path.join(options.state['basepath'],'textures'))):
		options.state['texturespath'] = os.path.join(options.state['basepath'],'textures')
def setexportscale(ID,val):
	global options
	options.state['scale'] = val

def setTpath(fname):
	global options
	options.state['texturespath'] = os.path.dirname(fname)
def setXpath(fname):
	global options
	options.state['externalspath'] = os.path.dirname(fname)
def setXApath(fname):
	global options
	options.state['xappath'] = fname
def event(evt, val):
	x = 1
def but_event(evt):
	global options
	
	global FLTExport
	global FLTClose 
	global FLTLabel
	
	global FLTBaseLabel
	global FLTTextureLabel
	global FLTXRefLabel

	global FLTBaseString
	global FLTTextureString
	global FLTXRefString
	
	global FLTBasePath
	global FLTTexturePath
	global FLTXRefPath
	
	global FLTShadeExport
	global FLTShadeDefault
	
	global FLTCopyTex
	global FLTDoXRef
	global FLTGlobal
	
	global FLTScale
	
	
	global FLTXAPP
	global FLTXAPPath
	global FLTXAPPString
	global FLTXAPPLabel 	
	global FLTXAPPChooser

	global FLTAttrib
	
	#choose base path for export
	if evt == 4:
		Blender.Window.FileSelector(setBpath, "DB Root", options.state['basepath'])
		
	#choose XREF path
	if evt == 6:
		Blender.Window.FileSelector(setXpath,"DB Externals",options.state['externalspath'])

	#choose texture path
	if evt == 8:
		Blender.Window.FileSelector(setTpath,"DB Textures",options.state['texturespath'])

	#export shading toggle
	if evt == 9:
		options.state['export_shading'] = FLTShadeExport.val
	#export Textures
	if evt == 11:
		options.state['copytex']= FLTCopyTex.val
	#export XRefs
	if evt == 13:
		options.state['doxrefs'] = FLTDoXRef.val
	#export Transforms
	if evt == 12:
		options.state['transform'] = FLTGlobal.val
		
	if evt == 14:
		options.state['xapp'] = FLTXAPP.val
	if evt == 16:
		Blender.Window.FileSelector(setXApath,"External Application",options.state['xappath'])
	if evt == 20:
		options.state['attrib'] = FLTAttrib.val
	
	#Export DB
	if evt == 1:
		dbexport()
	
	#exit
	if evt == 2:
		Draw.Exit()

	options.write_state()

from Blender.BGL import *
from Blender import Draw
def gui():
	
	global options
	
	global FLTExport
	global FLTClose 
	global FLTLabel
	
	global FLTBaseLabel
	global FLTTextureLabel
	global FLTXRefLabel

	global FLTBaseString
	global FLTTextureString
	global FLTXRefString
	
	global FLTBasePath
	global FLTTexturePath
	global FLTXRefPath
	
	global FLTShadeExport
	global FLTShadeDefault
	
	global FLTCopyTex
	global FLTDoXRef
	global FLTGlobal
	
	global FLTScale
	
	global FLTXAPP
	global FLTXAPPath
	global FLTXAPPString
	global FLTXAPPLabel
	global FLTXAPPChooser	
	
	global FLTAttrib
	
	glClearColor(0.880,0.890,0.730,1.0 )
	glClear(GL_COLOR_BUFFER_BIT)
	
	areas = Blender.Window.GetScreenInfo()
	curarea = Blender.Window.GetAreaID()
	curRect = None
	
	for area in areas:
		if area['id'] == curarea:
			curRect = area['vertices']
			break
	
	width = curRect[2] - curRect[0]
	height = curRect[3] - curRect[1]
	#draw from top to bottom....
	cx = 50
	#Draw Title Bar...
	#glRasterPos2d(cx, curRect[3]-100)
	#FLTLabel = Draw.Text("FLT Exporter V2.0",'large')
	cy = height - 80
	
	FLTBaseLabel = Draw.Label("Base Path:",cx,cy,100,20)
	FLTBaseString = Draw.String("",3,cx+100,cy,300,20,options.state['basepath'],255,"Folder to export to")
	FLTBaseChooser = Draw.PushButton("...",4,cx+400,cy,20,20,"Choose Folder")
	
	cy = cy-40
	
	#externals path
	FLTXRefLabel = Draw.Label("XRefs:",cx,cy,100,20)
	FLTXRefString = Draw.String("",5,cx+100,cy,300,20,options.state['externalspath'],255,"Folder for external references")
	FLTXRefChooser = Draw.PushButton("...",6,cx+400,cy,20,20,"Choose Folder")
	cy = cy-40
	#Textures path
	FLTTextureLabel = Draw.Label("Textures:",cx,cy,100,20)
	FLTTextureString = Draw.String("",7,cx+100,cy,300,20,options.state['texturespath'],255,"Folder for texture files")
	FLTTextureChooser = Draw.PushButton("...",8,cx+400,cy,20,20,"Choose Folder")
	cy=cy-40
	#External application path
	FLTXAPPLabel = Draw.Label("XApp:",cx,cy,100,20)
	FLTXAPPString = Draw.String("",15,cx+100,cy,300,20,options.state['xappath'],255,"External application to launch when done")
	FLTXAPPChooser = Draw.PushButton("...",16,cx+400, cy,20,20,"Choose Folder")
	
	cy = cy-60
	#Shading Options
	FLTShadeExport = Draw.Toggle("Default Shading",9,cx,cy,100,20,options.state['export_shading'],"Turn on export of custom shading")
	FLTShadDefault = Draw.Number("",10,cx + 120,cy,100,20,options.state['shading_default'],0.0,180.0,"Default shading angle for objects with no custom shading assigned",setshadingangle)
	
	cy = cy-40
	FLTScale = Draw.Number("Export Scale",14,cx,cy,220,20,options.state['scale'],0.0,100.0,"Export scaling factor",setexportscale)
	
	cy = cy-40
	#misc Options
	FLTCopyTex = Draw.Toggle("Copy Textures",11,cx,cy,220,20,options.state['copytex'],"Copy textures to folder indicated above")
	cy = cy-40
	FLTGlobal = Draw.Toggle("Export Transforms",12,cx,cy,220,20,options.state['transform'],"If unchecked, Global coordinates are used (recommended)")
	cy = cy-40
	FLTDoXRef = Draw.Toggle("Export XRefs", 13,cx,cy,220,20,options.state['doxrefs'],"Export External references (only those below current scene!)")
	cy = cy-40
	FLTXAPP = Draw.Toggle("Launch External App", 14, cx,cy,220,20,options.state['xapp'],"Launch External Application on export")
	cy = cy-40
	FLTAttrib = Draw.Toggle("Write Attribute Files", 20, cx, cy, 220,20,options.state['attrib'], "Write Texture Attribute files")
	#FLTXAPPATH = Draw.String("",15,cx,cy,300,20,options.xappath,255,"External application path")
	

	#Draw export/close buttons
	FLTExport = Draw.PushButton("Export",1,cx,20,100,20,"Export to FLT")
	FLTClose = Draw.PushButton("Close", 2, cx+120,20,100,20,"Close window")
	

Draw.Register(gui,event,but_event)