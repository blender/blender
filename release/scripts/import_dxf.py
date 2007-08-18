#!BPY

"""
Name: 'DXF (.dxf)'
Blender: 244
Group: 'Import'
Tooltip: 'Import for DXF geometry data (Drawing eXchange Format).'
"""
__author__ = 'Kitsu(Ed Blake) & migius(Remigiusz Fiedler)'
__version__ = '1.0.beta09 by migius 17.08.2007'
__url__ = ["http://blenderartists.org/forum/showthread.php?t=84319",
	 "http://wiki.blender.org/index.php/Scripts/Manual/Import/DXF-3D"]
__email__ = ["Kitsune_e(at)yahoo.com", "remi_(at)gmx.de"]
__bpydoc__ = """\
This script imports DXF objects (2d/3d) into Blender.

This script imports 2d and 3d Geometery from DXFr12 format files.
This version is focused on import of 3d-objects.

Supported DXF Objects:
 LINE
 POINT
 SOLID
 TRACE
 INSERT  (=block)
 MINSERT (=array)
 CIRCLE
 ARC
 3DFACE
 2d-POLYLINE (incl.:arc-segments, variable-width-segments)
 2d-POLYLINE (curved, spline)
 3d-POLYLINE (curved, spline)
 3d-POLYMESH
 3d-POLYFACE
 TEXT

Supported DXF Objects: (*)-under construction
 *LWPOLYLINE (DXF>r12 LightWeight POLYLINE)
 *MTEXT   (DXF>r12)
 *ELLIPSE (DXF>r12)

Not supported DXF Objects:
 DIMENSION
 XREF (External Reference)
 SPLINE  (DXF>r12)
 GROUP   (DXF>r12)
 RAY/XLINE  (DXF>r12)
 LEADER  (DXF>r12)
 3DSOLID, BODY, REGION (DXF>r12)
 dynamic BLOCK (DXF 2006)

Notes:
- Recommend that you run 'RemoveDoubles' on each imported mesh after using this script
- Blocks are created on layer 19 then referenced at each insert point.

TODO:  
- filtering of unused/not-inserted Blocks
- the new style object visibility
- support for Spline-curves, Besier-curves
- support for real 3d-solids (ACIS)
- (to see more, search for "-todo-" in script)


History:
 v1.0 by migius 08.2007: "full 3d"-release
    TODO:
 -- command-line-mode/batch-mode
 -- human-formating of data in INI-File
 -- suport for MLine
 -- suport for Ellipse
 -- suport for Mtext
 -- blender_object.ID.properties[dxf_layer_name]
 -- added f_layerFilter
 -- to-check: new_scene-idea from ideasman42: each import create a new scene
 -- to-check: obj/mat/group/_mapping-idea from ideasman42:
 -- better support for long dxf-layer-names 
 -- support width_force for LINEs/ARCs/CIRCLEs/ELLIPSEs = "solidify"

 beta09: 17.08.2007 by migius
 f- cleanup code
 f- bugfix: thickness for Bezier/Bsplines into Blender-curves
 f- added import POLYLINE-Bsplines into Blender-NURBSCurves
 f- added import POLYLINE-arc-segments into Blender-BezierCurves
 f- added import POLYLINE-Bezier-curves into Blender-Curves
 d5 rewrite: Optimisations Levels, added 'directDrawing'
 d4 added: f_set_thick(cntrolled by ini-parameters)
 d4 bugfix: face-normals in objects with minus thickness
 d4 added: placeholder'Empty'-size in f_Insert.draw
 d3 rewrite f_Text.Draw: added suport for all Text's parameters
 d2 redesign: progressbar 
 e- tuning by ideasman42
 c- tuning by ideasman42
 b- rewrite f_Text.Draw rotation/transform
 b- bugfix: POLYLINE-segment-intersection more reliable now
 b- bugfix: circle:_thic, 'Empties':no material_assignement
 b- added material assignment (from layer and/or color)
 a- added empty, cylinder and UVsphere for POINTs
 a- added support for 2d-POLYLINE: splines, fitted curves, fitted surfaces
 a- redesign f_Drawer for block_definitions
 a- rewrite import into Blender-Curve-Object
 beta08: 27.07.2007 by migius
 l- bugfix: solid_vgroups, clean:scene.objects.new()
 l- redesign UI to standard Draw.Register+FileSelector, advanced_config_option
 k- bugfix UI:fileSelect() for MacOSX os.listdir()
 k- added reset/save/load for config-data
 k- redesign keywords/drawTypes/Draw.Create_Buttons
 j- new interface using UIBlock() with own FileSelector, cause Window.FileSelector() too buggy
 i- rewritten Class:Settings for better config-parameter management
 h- bugfix: face-normals in objects with minus thickness
 h- added Vertex-Groups in polylines and solids generated Meshes, for easier material assignment
 h- beautify code, whitespace->tabs
 h- added settings.thic_force switch for forcing thickness
 h- added one Object/Mesh for all simple-entities from the same Layer,
	sorted in Vertex-Groups(color_name)  (fewer objects = better import performance)
 g- rewrote: insert-point-handle-object is a small tetrahedron
 e- bugfix: closed-polymesh3d
 - rewrote: startUI, type_map.keys, f_drawer, for all class_f_draw(added "settings" as attribut)
 - added 2d/3d-support for Polyline_Width incl. angleintersection
 beta07: 19.06.2007 by migius
 - added 3d-support for LWPolylines
 - added 2d/3d-support for Points
 beta06: 15.06.2007 by migius
 - cleanup code
 - added 2d/3d-support for MINSERT=BlockArray in f_drawer, added f_rotXY_Vec
 beta05: 14.06.2007 by migius
 - added 2d/3d-support for 3d-PolyLine, PolyMesh and PolyFace
 - added Global-Scale for size control of imported scenes
 beta04: 12.06.2007 by migius
 - rewrote the f_drawBulge for correct import the arc-segments of Polylines
 beta03: 10.06.2007 by migius
 - rewrote interface
 beta02: 09.06.2007 by migius
 - added 3d-support for Arcs and Circles
 - added support for Object_Thickness(=height)
 beta01: 08.06.2007 by migius
 - added 3d-support for Blocks/Inserts within nested-structures
 - rewrote f_transform for correct 3d-location/3d-rotation
 - added 3d-support Lines, 3dFaces
 - added 2d+3d-support for Solids and Traces

 v0.9 by kitsu 01.2007: (for 2.43)
 -

 v0.8 by kitsu 12.2007:
 -

 v0.5b by kitsu 10.2006 (for 2.42a)
 -

"""

# --------------------------------------------------------------------------
# DXF Import v1.0 by Ed Blake (AKA kitsu) and Remigiusz Fiedler (AKA migius)
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
# --------------------------------------------------------------------------

import Blender
import bpy
from Blender import *
#from Blender.Mathutils import Vector, Matrix
#import BPyMessages


from dxfReader import readDXF      # get_name, get_layer
from dxfReader import Object as dxfObject
from dxfColorMap import color_map
from math import *

try:
	import os
	if os.name:# != 'mac':
		import psyco
		psyco.log()
		psyco.full(memory=100)
		psyco.profile(0.05, memory=100)
		psyco.profile(0.2)
except ImportError:
	pass

print '\n\n\n\n'
print 'Import DXF to Blender *** START ***'   #---------------------

SCENE = None
WORLDX = Mathutils.Vector((1,0,0))
WORLDY = Mathutils.Vector((1,1,0))
WORLDZ = Mathutils.Vector((0,0,1))

G_SCALE = 1.0		#(0.0001-1000) global scaling factor for all dxf data
MIN_DIST = 0.001	#cut-off value for sort out short-distance polyline-"duoble_vertex"
ARC_RESOLUTION = 64   #(4-500) arc/circle resolution - number of segments
THIN_RESOLUTION = 8   #(4-500) thin_cylinder arc_resolution - number of segments
MIN_THICK = MIN_DIST * 10.0  #minimal thickness by forced thickness
MIN_WIDTH = MIN_DIST * 10.0  #minimal width by forced width
ANGLECUT_LIMIT = 3.0	 #limit for anglecut of polylines-wide-segments (values:1.0 - 5.0)
TARGET_LAYER = 3    #target blender_layer
GROUP_BYLAYER = 0   #(0/1) all entities from same layer import into one blender-group

FILENAME_MAX = 180    #max length of path+file_name string  (FILE_MAXDIR + FILE_MAXFILE)
MAX_NAMELENGTH = 17   #max_effective_obnamelength in blender =21=17+(.001)
INIFILE_DEFAULT_NAME = 'importDXF.ini'
INIFILE_EXTENSION = '.ini'
INIFILE_HEADER = 'ImportDXF.py ver.1.0 config data'

AUTO = BezTriple.HandleTypes.AUTO
FREE = BezTriple.HandleTypes.FREE
VECT = BezTriple.HandleTypes.VECT
ALIGN = BezTriple.HandleTypes.ALIGN
cur_COUNTER = 0  #counter for progress_bar


"""This module provides wrapper objects for dxf entities.

	The wrappers expect a "dxf object" as input.  The dxf object is
	an object with a type and a data attribute.  Type is a lowercase
	string matching the 0 code of a dxf entity.  Data is a list containing
	dxf objects or lists of [code, data] pairs.

	This module is not general, and is only for dxf import.
"""

# from Stani's dxf writer v1.1 (c)www.stani.be (GPL)
#---color values
BYBLOCK=0
BYLAYER=256

#---block-type flags (bit coded values, may be combined):
ANONYMOUS			=1  # This is an anonymous block generated by hatching, associative dimensioning, other internal operations, or an application
NON_CONSTANT_ATTRIBUTES =2  # This block has non-constant attribute definitions (this bit is not set if the block has any attribute definitions that are constant, or has no attribute definitions at all)
XREF					=4  # This block is an external reference (xref)
XREF_OVERLAY			=8  # This block is an xref overlay
EXTERNAL				=16 # This block is externally dependent
RESOLVED				=32 # This is a resolved external reference, or dependent of an external reference (ignored on input)
REFERENCED		=64 # This definition is a referenced external reference (ignored on input)

#---polyline flags
CLOSED				=1	# This is a closed polyline (or a polygon mesh closed in the M direction)
CURVE_FIT				=2  # Curve-fit vertices have been added
SPLINE_FIT			=4	# Spline-fit vertices have been added
POLYLINE_3D		=8   # This is a 3D polyline
POLYGON_MESH				=16  # This is a 3D polygon mesh
CLOSED_N					=32  # The polygon mesh is closed in the N direction
POLYFACE_MESH			=64   # The polyline is a polyface mesh
CONTINOUS_LINETYPE_PATTERN  =128	# The linetype pattern is generated continuously around the vertices of this polyline

#---text flags
#horizontal
LEFT		= 0
CENTER  = 1
RIGHT	= 2
ALIGNED  = 3 #if vertical alignment = 0
MIDDLE  = 4 #if vertical alignment = 0
FIT   = 5 #if vertical alignment = 0
#vertical
BASELINE	= 0
BOTTOM  = 1
MIDDLE  = 2
TOP   = 3

#---mtext flags
#attachment point
TOP_LEFT		= 1
TOP_CENTER  = 2
TOP_RIGHT	= 3
MIDDLE_LEFT  = 4
MIDDLE_CENTER   = 5
MIDDLE_RIGHT	= 6
BOTTOM_LEFT  = 7
BOTTOM_CENTER   = 8
BOTTOM_RIGHT	= 9
#drawing direction
LEFT_RIGHT  = 1
TOP_BOTTOM  = 3
BY_STYLE		= 5 #the flow direction is inherited from the associated text style
#line spacing style (optional):
AT_LEAST		= 1 #taller characters will override
EXACT		= 2 #taller characters will not override



class Layer:  #-----------------------------------------------------------------
	"""Class for objects representing dxf layers.
	"""
	def __init__(self, obj, name=None, color=None, frozen=None):
		"""Expects an object of type layer as input.
		"""
		self.type = obj.type
		self.data = obj.data[:]

		if name:
			self.name = name
			#self.bfname = name  #remi--todo-----------
		else:
			self.name = obj.get_type(2)[0] #layer name of object

		if color:
			self.color = color
		else:
			self.color = obj.get_type(62)[0]  #color of object

		if frozen:
			self.frozen = frozen
		else:
			self.flags = obj.get_type(70)[0]
			self.frozen = self.flags&1


	def __repr__(self):
		return "%s: name - %s, color - %s" %(self.__class__.__name__, self.name, self.color)



def getit(obj, typ, default=None):  #------------------------------------------
	"""Universal procedure for geting data from list/objects.
	"""
	it = default
	if type(obj) == list:  #if obj is a list, then searching in a list
		for item in obj:
			#print 'deb:getit item, type(item)', item, type(item)
			try:
				if item[0] == typ:
					it = item[1]
					break  #as soon as the first found
			except:
				# TODO - I found one case where item was a text instance
				# that failed with no __getitem__
				pass
	else:	 #else searching in Object with get_type-Methode
		item = obj.get_type(typ)
		if item:
			it = item[0]
	#print 'deb:getit:typ, it', typ, it #----------
	return it



def get_extrusion(data):	 #-------------------------------------------------
	"""Find the axis of extrusion.

	Used to get from object_data the objects Object Coordinate System (ocs).
	"""
	#print 'deb:get_extrusion: data: \n', data  #---------------
	vec = [0,0,1]
	vec[0] = getit(data, 210, 0) # 210 = x
	vec[1] = getit(data, 220, 0) # 220 = y
	vec[2] = getit(data, 230, 1) # 230 = z
	#print 'deb:get_extrusion: vec: ', vec  #---------------
	return vec





class Solid:  #-----------------------------------------------------------------
	"""Class for objects representing dxf solid or trace.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type solid or trace as input.
		"""
		if obj.type == 'trace':
			obj.type = 'solid'
		if not obj.type == 'solid':
			raise TypeError, "Wrong type \'%s\' for solid/trace object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj.data, 8, None)
		self.extrusion = get_extrusion(obj.data)
		self.points = self.get_points(obj.data)



	def get_points(self, data):
		"""Gets start and end points for a solid type object.

		Solids have 3 or 4 points and fixed codes for each value.
		"""

		# start x, y, z and end x, y, z = 0
		a = [0, 0, 0]
		b = [0, 0, 0]
		c = [0, 0, 0]
		d = [0, 0, 0]
		a[0] = getit(data, 10, None) # 10 = x
		a[1] = getit(data, 20, None) # 20 = y
		a[2] = getit(data, 30,  0) # 30 = z
		b[0] = getit(data, 11, None)
		b[1] = getit(data, 21, None)
		b[2] = getit(data, 31,  0)
		c[0] = getit(data, 12, None)
		c[1] = getit(data, 22, None)
		c[2] = getit(data, 32,  0)
		out = [a,b,c]

		d[0] =  getit(data, 13, None)
		if d[0] != None:
			d[1] = getit(data, 23, None)
			d[2] = getit(data, 33,  0)
			out.append(d)
		#print 'deb:solid.vertices:---------\n', out  #-----------------------
		return out


	def __repr__(self):
		return "%s: layer - %s, points - %s" %(self.__class__.__name__, self.layer, self.points)


	def draw(self, settings):
		"""for SOLID: generate Blender_geometry.
		"""
		points = self.points
		if not points: return
		edges, faces = [], []
		l = len(self.points)

		obname = 'so_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]

		vg_left, vg_right, vg_top, vg_bottom, vg_start, vg_end = [], [], [], [], [], []
		thic = set_thick(self.thic, settings)
		if thic != 0:
			thic_points = [[v[0], v[1], v[2] + thic] for v in points[:]]
			if thic < 0.0:
				thic_points.extend(points)
				points = thic_points
			else:
				points.extend(thic_points)

			if   l == 4:
				faces = [[0,1,3,2], [4,6,7,5], [0,4,5,1],
						 [1,5,7,3], [3,7,6,2], [2,6,4,0]]
				vg_left = [2,6,4,0]
				vg_right = [1,5,7,3]
				vg_top = [4,6,7,5]
				vg_bottom = [0,1,3,2]
				vg_start = [0,4,5,1]
				vg_end = [3,7,6,2]
			elif l == 3:
				faces = [[0,1,2], [3,5,4], [0,3,4,1], [1,4,5,2], [2,5,3,0]]
				vg_top = [3,4,5]
				vg_bottom = [0,1,2]
				vg_left = [2,5,3,0]
				vg_right = [1,4,5,2]
				vg_start = [0,3,4,1]
			elif l == 2: faces = [[0,1,3,2]]
		else:
			if   l == 4: faces = [[0,1,3,2]]
			elif l == 3: faces = [[0,1,2]]
			elif l == 2: edges = [[0,1]]



		me = Mesh.New(obname)		   # create a new mesh
		me.verts.extend(points)        # add vertices to mesh
		if faces: me.faces.extend(faces)		   # add faces to the mesh
		if edges: me.edges.extend(edges)		   # add faces to the mesh

		ob = SCENE.objects.new(me) # create a new mesh_object
		if settings.var['vGroup_on']:
			# each MeshSite becomes vertexGroup for easier material assignment ---------------------
			replace = Blender.Mesh.AssignModes.ADD  #or .AssignModes.ADD/REPLACE
			if vg_left:	me.addVertGroup('side.left')  ; me.assignVertsToGroup('side.left',  vg_left, 1.0, replace)
			if vg_right:me.addVertGroup('side.right') ; me.assignVertsToGroup('side.right', vg_right, 1.0, replace)
			if vg_top:  me.addVertGroup('side.top')   ; me.assignVertsToGroup('side.top',   vg_top, 1.0, replace)
			if vg_bottom:me.addVertGroup('side.bottom'); me.assignVertsToGroup('side.bottom',vg_bottom, 1.0, replace)
			if vg_start:me.addVertGroup('side.start') ; me.assignVertsToGroup('side.start', vg_start, 1.0, replace)
			if vg_end:  me.addVertGroup('side.end')   ; me.assignVertsToGroup('side.end',   vg_end,   1.0, replace)

		transform(self.extrusion, 0, ob)

		return ob




class Line:  #-----------------------------------------------------------------
	"""Class for objects representing dxf lines.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type line as input.
		"""
		if not obj.type == 'line':
			raise TypeError, "Wrong type \'%s\' for line object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		#print 'deb:self.thic: ', self.thic #---------------------
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj.data, 8, None)
		self.extrusion = get_extrusion(obj.data)
		self.points = self.get_points(obj.data)


	def get_points(self, data):
		"""Gets start and end points for a line type object.

		Lines have a fixed number of points (two) and fixed codes for each value.
		"""
		# start x,y,z and end x,y,z = 0
		a = [0, 0, 0]
		b = [0, 0, 0]
		a[0] = getit(data, 10, None) # 10 = x
		a[1] = getit(data, 20, None) # 20 = y
		a[2] = getit(data, 30,  0) # 30 = z
		b[0] = getit(data, 11, None)
		b[1] = getit(data, 21, None)
		b[2] = getit(data, 31,  0)
		out = [a,b]
		return out


	def __repr__(self):
		return "%s: layer - %s, points - %s" %(self.__class__.__name__, self.layer, self.points)


	def draw(self, settings):
		"""for LINE: generate Blender_geometry.
		"""
		# Generate the geometery
		#settings.var['curves_on']=False

		points = self.points

		global activObjectLayer
		global activObjectName
		#print 'deb:draw:line.ob IN activObjectName: ', activObjectName #---------------------

		if activObjectLayer == self.layer and settings.var['one_mesh_on']:
			obname = activObjectName
			#print 'deb:line.draw obname from activObjectName: ', obname #---------------------
			ob = Object.Get(obname)  # open an existing mesh_object
			me = Mesh.Get(ob.name)	 # open objects mesh data
		else:
			obname = 'li_%s' %self.layer  # create object name from layer name
			obname = obname[:MAX_NAMELENGTH]
			me = Mesh.New(obname)		   # create a new mesh
			ob = SCENE.objects.new(me) # create a new mesh_object
			activObjectName = ob.name
			activObjectLayer = self.layer
			#print ('deb:line.draw new line.ob+mesh:"%s" created!' %ob.name) #---------------------

		#if settings.var['width_force']: # todo-----------

		faces, edges = [], []
		n = len(me.verts)
		thic = set_thick(self.thic, settings)
		if thic != 0:
			t, e = thic, self.extrusion
			#print 'deb:thic, extr: ', t, e #---------------------
			points.extend([[v[0]+t*e[0], v[1]+t*e[1], v[2]+t*e[2]] for v in points[:]])
			faces = [[0+n, 1+n, 3+n, 2+n]]
		else:
			me.verts.extend(points) # add vertices to mesh
			edges = [[0+n, 1+n]]

		me.verts.extend(points) # add vertices to mesh
		if faces: me.faces.extend(faces)	   # add faces to the mesh
		if edges: me.edges.extend(edges)	   # add faces to the mesh

		if settings.var['vGroup_on']:
			# entities with the same color build one vertexGroup for easier material assignment ---------------------
			ob.link(me) # link mesh to that object
			vG_name = 'color_%s' %self.color_index
			if edges: faces = edges
			replace = Blender.Mesh.AssignModes.ADD  #or .AssignModes.REPLACE or ADD
			try:
				me.assignVertsToGroup(vG_name,  faces[0], 1.0, replace)
				#print 'deb: existed vGroup:', vG_name #---------------------
			except:
				me.addVertGroup(vG_name)
				me.assignVertsToGroup(vG_name,  faces[0], 1.0, replace)
				#print 'deb: create new vGroup:', vG_name #---------------------


		#print 'deb:draw:line.ob OUT activObjectName: ', activObjectName #---------------------
		return ob



class Point:  #-----------------------------------------------------------------
	"""Class for objects representing dxf points.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type point as input.
		"""
		if not obj.type == 'point':
			raise TypeError, "Wrong type %s for point object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		#print 'deb:self.thic: ', self.thic #---------------------
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj.data, 8, None)
		self.extrusion = get_extrusion(obj.data)
		self.points = self.get_points(obj.data)


	def get_points(self, data):
		"""Gets coordinates for a point type object.

		Points have fixed codes for each value.
		"""
		a = [0, 0, 0]
		a[0] = getit(data, 10, None) # 10 = x
		a[1] = getit(data, 20, None) # 20 = y
		a[2] = getit(data, 30,  0) # 30 = z
		out = [a]
		return out


	def __repr__(self):
		return "%s: layer - %s, points - %s" %(self.__class__.__name__, self.layer, self.points)


	def draw(self, settings):
		"""for POINT: generate Blender_geometry.
		"""
		points = self.points
		obname = 'po_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]
		points_as = settings.var['points_as']
		thic = settings.var['thick_min']
		if thic < settings.var['dist_min']: thic = settings.var['dist_min']

		if points_as in [1,3,4]:
			if points_as == 1: # as 'empty'
				c = 'Empty'
			if points_as == 3: # as 'thin sphere'
				res = settings.var['thin_res']
				c = Mesh.Primitives.UVsphere(res,res,thic)
			if points_as == 4: # as 'thin box'
				c = Mesh.Primitives.Cube(thic)
			ob = SCENE.objects.new(c, obname) # create a new object
			transform(self.extrusion, 0, ob)
			ob.loc = tuple(points[0])

		elif points_as == 2: # as 'vertex'
			global activObjectLayer
			global activObjectName
			#print 'deb:draw:point.ob IN activObjectName: ', activObjectName #---------------------
			if activObjectLayer == self.layer and settings.var['one_mesh_on']:
				obname = activObjectName
				#print 'deb:draw:point.ob obname from activObjectName: ', obname #---------------------
				ob = Object.Get(obname)  # open an existing mesh_object
				me = Mesh.Get(ob.name)	 # open objects mesh data
			else:
				me = Mesh.New(obname)		   # create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
				activObjectName = ob.name
				activObjectLayer = self.layer
				#print ('deb:draw:point new point.ob+mesh:"%s" created!' %ob.name) #---------------------
			me.verts.extend(points) # add vertices to mesh

		return ob




class LWpolyline:  #-----------------------------------------------------------------
	"""Class for objects representing dxf LWpolylines.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type lwpolyline as input.
		"""
		#print 'deb:LWpolyline.START:----------------' #------------------------
		if not obj.type == 'lwpolyline':
			raise TypeError, "Wrong type %s for polyline object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# required data
		self.num_points = obj.get_type(90)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)

		self.color_index = getit(obj, 62, BYLAYER)

		self.elevation =  getit(obj, 30, 0)
		self.thic =  getit(obj, 39, 0)
		self.flags = getit(obj, 70, 0)

		self.closed = self.flags&1 # byte coded, 1 = closed, 128 = plinegen

		self.layer = getit(obj.data, 8, None)
		self.points = self.get_points(obj.data)
		self.extrusion = get_extrusion(obj.data)

		#print 'deb:LWpolyline.obj.data:\n', obj.data #------------------------
		#print 'deb:LWpolyline.ENDinit:----------------' #------------------------


	def get_points(self, data):
		"""Gets points for a polyline type object.

		LW-Polylines have no fixed number of verts, and
		each vert can have a number of properties.
		Verts should be coded as
		10:xvalue
		20:yvalue
		40:startwidth or 0
		41:endwidth or 0
		42:bulge or 0
		for each vert
		"""
		num = self.num_points
		point = None
		points = []
		for item in data:
			if item[0] == 10:   # 10 = x
				if point:
					points.append(point)
				point = Vertex()
				point.x = item[1]
			elif item[0] == 20: # 20 = y
				point.y = item[1]
			elif item[0] == 40: # 40 = start width
				point.swidth = item[1]
			elif item[0] == 41: # 41 = end width
				point.ewidth = item[1]
			elif item[0] == 42: # 42 = bulge
				point.bulge = item[1]
		points.append(point)
		return points



	def __repr__(self):
		return "%s: layer - %s, points - %s" %(self.__class__.__name__, self.layer, self.points)


	def draw(self, settings):
		"""for LWPOLYLINE: generate Blender_geometry.
		"""
		#print 'deb:LWpolyline.draw.START:----------------' #------------------------
		points = []
		obname = 'lw_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]
		#settings.var['curves_on'] == True
		#print 'deb:index_len: ', len(self.points) #------------------
		for i, point in enumerate(self.points):
			#print 'deb:index: ', i #------------------
			if not point.bulge:
				points.append(point.loc)
			elif point.bulge and not self.closed and i == len(self.points)-1:
				points.append(point.loc)
			elif point.bulge:	  #
				if i == len(self.points)-1:
					point2 = self.points[0]
				else:
					point2 = self.points[i+1]
				verts = drawBulge(point, point2, settings.var['arc_res'])
#				if i == len(self.points)-1:
#					if self.closed:
#						verts.pop() #remove last(=first) vertex
#				else:
#					verts.pop() #remove last vertex, because this point will be writen as the next vertex
				points.extend(verts)

		thic = self.thic
		if settings.var['thick_force'] and thic == 0: thic = settings.var['thick_min']
		if settings.var['thick_on'] and thic != 0:
			len1 = len(points)
			points.extend([[point[0], point[1], point[2]+thic] for point in points])
			faces = []
			#print 'deb:len1:', len1  #-----------------------
			faces = [[num, num+1, num+len1+1, num+len1] for num in xrange(len1 - 1)]
			if self.closed:
				faces.append([len1-1, 0, len1, 2*len1-1])
			#print 'deb:faces_list:\n', faces  #-----------------------
			me = Mesh.New(obname)		   # create a new mesh
			ob = SCENE.objects.new(me) # create a new mesh_object
			me.verts.extend(points) # add vertices to mesh
			me.faces.extend(faces)	 # add faces to the mesh
		else:
			edges = [[num, num+1] for num in xrange(len(points)-1)]
			if self.closed:
				edges.append([len(points)-1, 0])
			#print 'deb:edges_list:\n', edges  #-----------------------
			me = Mesh.New(obname)		   # create a new mesh
			ob = SCENE.objects.new(me) # create a new mesh_object
			me.verts.extend(points) # add vertices to mesh
			me.edges.extend(edges)	 # add edges to the mesh

		ob.LocZ = self.elevation
		transform(self.extrusion, 0, ob)

		#print 'deb:LWpolyline.draw.END:----------------' #------------------------
		return ob



class Polyline:  #-----------------------------------------------------------------
	"""Class for objects representing dxf Polylines.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type polyline as input.
		"""
		#print 'deb:polyline.init.START:----------------' #------------------------
		if not obj.type == 'polyline':
			raise TypeError, "Wrong type %s for polyline object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]
		#print 'deb:polyline.obj.data[:]:\n', obj.data[:] #------------------------
		self.points = []

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.elevation =  getit(obj, 30, 0)
		#print 'deb:elevation: ', self.elevation #---------------
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.flags = getit(obj, 70, 0)
		self.closed = self.flags & 1   # closed in the M direction
		self.curved = self.flags & 2   # Bezier-curve-fit vertices have been added
		self.spline = self.flags & 4   # Bspline-fit vertices have been added
		self.poly3d = self.flags & 8   # 3D-polyline
		self.plmesh = self.flags & 16  # 3D-polygon mesh
		self.closeN = self.flags & 32  # closed in the N direction
		self.plface = self.flags & 64  # 3D-polyface mesh
		self.contin = self.flags & 128 # the linetype pattern is generated continuously

		if self.poly3d or self.plface or self.plmesh:
			self.poly2d = False  # its not a 2D-polyline
		else:
			self.poly2d = True	 # it is a 2D-polyline

		self.swidth =  getit(obj, 40, 0) # default start width
		self.ewidth =  getit(obj, 41, 0) # default end width
		#self.bulge  =  getit(obj, 42, None) # bulge of the segment
		self.vectorsM =  getit(obj, 71, None) # PolyMesh: expansion in M-direction / PolyFace: number of the vertices
		self.vectorsN =  getit(obj, 72, None) # PolyMesh: expansion in M-direction / PolyFace: number of faces
		#self.resolM =  getit(obj, 73, None) # resolution of surface in M direction
		#self.resolN =  getit(obj, 74, None) # resolution of surface in N direction
		self.curvetyp =  getit(obj, 75, 0) # type of curve/surface: 0=None/5=Quadric/6=Cubic/8=Bezier
		self.curvNormal = False
		self.curvQBspline = False
		self.curvCBspline = False
		self.curvBezier = False
		if   self.curvetyp == 0: self.curvNormal = True
		elif self.curvetyp == 5: self.curvQBspline = True
		elif self.curvetyp == 6: self.curvCBspline = True
		elif self.curvetyp == 8: self.curvBezier = True

		self.layer = getit(obj.data, 8, None)
		self.extrusion = get_extrusion(obj.data)

		self.points = []  #list with vertices coordinats
		self.faces  = []  #list with vertices assigment to faces
		#print 'deb:polyline.init.ENDinit:----------------' #------------



	def __repr__(self):
		return "%s: layer - %s, points - %s" %(self.__class__.__name__, self.layer, self.points)



	def draw(self, settings):   #-------------%%%% DRAW POLYLINE %%%---------------
		"""for POLYLINE: generate Blender_geometry.
		"""
		ob = []
		if self.plface:  #---- 3dPolyFace - mesh with free topology
			ob = self.drawPlFace(settings)
		elif self.plmesh:  #---- 3dPolyMesh - mesh with ortogonal topology
			ob = self.drawPlMesh(settings)
		#---- 2dPolyline - plane polyline with arc/wide/thic segments
		#---- 3dPolyline - noplane polyline (thin segments = without arc/wide/thic)
		elif  self.poly2d or self.poly3d:
			if settings.var['curves_on']: # and self.spline:
				ob = self.drawPolyCurve(settings)
			else:
				ob = self.drawPoly2d(settings)
		return ob



	def drawPlFace(self, settings):  #---- 3dPolyFace - mesh with free topology
		"""Generate the geometery of polyface.
		"""
		#print 'deb:polyface.draw.START:----------------' #------------------------
		points = []
		faces = []
		#print 'deb:len of pointsList ====== ', len(self.points) #------------------------
		for point in self.points:
			if point.face:
				faces.append(point.face)
			else:
				points.append(point.loc)


		#print 'deb:len of points_list:\n', len(points)  #-----------------------
		#print 'deb:points_list:\n', points  #-----------------------
		#print 'deb:faces_list:\n', faces  #-----------------------
		obname = 'pf_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]
		me = Mesh.New(obname)		   # create a new mesh
		ob = SCENE.objects.new(me) # create a new mesh_object
		me.verts.extend(points) # add vertices to mesh
		me.faces.extend(faces)	 # add faces to the mesh

		transform(self.extrusion, 0, ob)
		#print 'deb:polyface.draw.END:----------------' #------------------------

		return ob



	def drawPlMesh(self, settings):  #---- 3dPolyMesh - mesh with orthogonal topology
		"""Generate the geometery of polymesh.
		"""
		#print 'deb:polymesh.draw.START:----------------' #------------------------
		#points = []
		#print 'deb:len of pointsList ====== ', len(self.points) #------------------------
		faces = []
		m = self.vectorsM
		n = self.vectorsN
		for j in xrange(m - 1):
			for i in xrange(n - 1):
				nn = j * n
				faces.append([nn+i, nn+i+1, nn+n+i+1, nn+n+i])

		if self.closed:   #mesh closed in N-direction
			nn = (m-1)*n
			for i in xrange(n - 1):
				faces.append([nn+i, nn+i+1, i+1, i])

		if self.closeN:   #mesh closed in M-direction
			for j in xrange(m-1):
				nn = j * n
				faces.append([nn+n-1, nn, nn+n, nn+n-1+n])

		if self.closed and self.closeN:   #mesh closed in M/N-direction
				faces.append([ (n*m)-1, (m-1)*n, 0, n-1])

		#print 'deb:len of points_list:\n', len(points)  #-----------------------
		#print 'deb:faces_list:\n', faces  #-----------------------
		obname = 'pm_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]
		me = Mesh.New(obname)		   # create a new mesh
		ob = SCENE.objects.new(me) # create a new mesh_object
		me.verts.extend([point.loc for point in self.points]) # add vertices to mesh
		me.faces.extend(faces)	 # add faces to the mesh

		transform(self.extrusion, 0, ob)
		#print 'deb:polymesh.draw.END:----------------' #------------------------
		return ob


	def drawPolyCurve(self, settings):  #---- Polyline - draw as Blender-curve
		"""Generate the geometery of polyline as Blender-curve.
		"""
		#print 'deb:polyline2dCurve.draw.START:----------------' #---
		if len(self.points) < 2:
			#print 'deb:drawPoly2d exit, cause POLYLINE has less than 2 vertices' #---------
			return

		if self.spline: pline_typ = 'ps'	# Polyline-nurbSpline
		elif self.curved: pline_typ = 'pc'	# Polyline-bezierCurve
		else: pline_typ = 'pl'				# Polyline
		obname = '%s_%s' %(pline_typ, self.layer)  # create object_name from layer name
		obname = obname[:MAX_NAMELENGTH]
		d_points = []
		#for DXFr10-format: update all points[].loc[2] == None -> 0.0 
		for point in self.points:
			if point.loc[2] == None:
				point.loc[2] = self.elevation
			d_points.append(point)

		thic = set_thick(self.thic, settings)

		if thic != 0.0:   #hack: Blender<2.45 curve-extrusion
			LocZ = d_points[0].loc[2]
			temp_points = []
			for point in d_points:
				point.loc[2] = 0.0
				temp_points.append(point)
			d_points = temp_points
		
		#print 'deb:polyline2dCurve.draw d_points=', d_points  #---------------
		pline = Curve.New(obname)	# create new curve data

		if False: #self.spline:  # NURBSplines-----FAKE(with Bezier)-----
			#print 'deb:polyline2dCurve.draw self.spline!' #---------------
			curve = pline.appendNurb(BezTriple.New(d_points[0]))
			for p in d_points[1:]:
				curve.append(BezTriple.New(p))
			for point in curve:
				point.handleTypes = [AUTO, AUTO]
			if self.closed:
				curve.flagU = 1 # Set curve cyclic=close
			else:
				curve.flagU = 0 # Set curve not cyclic=open
				curve[0].handleTypes = [FREE, ALIGN]   #remi--todo-----
				curve[-1].handleTypes = [ALIGN, FREE]   #remi--todo-----

		elif self.spline:  # NURBSplines-----TODO--:if curvQBspline: generate middlepoints---
			#print 'deb:polyline2dCurve.draw self.spline!' #---------------
			weight1 = 0.5
			weight2 = 1.0
			# generate middlepoints except start/end-segments ---
			if self.curvQBspline:
				temp_points = []
				point = d_points[0].loc
				point.append(weight1)
				temp_points.append(point)
				for i in xrange(1,len(d_points)-2):
					point1 = d_points[i].loc
					point2 = d_points[i+1].loc
					mpoint = list((Mathutils.Vector(point1) + Mathutils.Vector(point2)) * 0.5)
					mpoint.append(weight2)
					point1.append(weight1)
					temp_points.append(point1)
					temp_points.append(mpoint)
				point2.append(weight1)
				temp_points.append(point2)
				point = d_points[-1].loc
				point.append(weight1)
				temp_points.append(point)
				d_points = temp_points
			else:
				temp_points = []
				for d in d_points:
					d = d.loc
					d.append(weight1)
				 	temp_points.append(d)
			 	d_points = temp_points

			if not self.closed:
				# generate extended startpoint and endpoint------
				point1 = Mathutils.Vector(d_points[0][:3])
				point2 = Mathutils.Vector(d_points[1][:3])
				startpoint = list(point1 - point2 + point1)
				startpoint.append(weight1)
				point1 = Mathutils.Vector(d_points[-1][:3])
				point2 = Mathutils.Vector(d_points[-2][:3])
				endpoint = list(point1 - point2 + point1)
				endpoint.append(weight1)
				temp_points = []
				temp_points.append(startpoint)
				temp_points.extend(d_points)
				d_points = temp_points
				d_points.append(endpoint)

			point = d_points[0]
			curve = pline.appendNurb(point)
			curve.setType(4) #NURBS curve
			for point in d_points[1:]:
				curve.append(point)
			if self.closed:
				curve.flagU = 1 # Set curve cyclic=close
			else:
				curve.flagU = 0 # Set curve not cyclic=open

		elif  self.curved:  #--Bezier-curves---OK-------
			#print 'deb:polyline2dCurve.draw self.curved!' #---------------
			curve = pline.appendNurb(BezTriple.New(d_points[0]))
			for p in d_points[1:]:
				curve.append(BezTriple.New(p))
			for point in curve:
				point.handleTypes = [AUTO, AUTO]
			if self.closed:
				curve.flagU = 1 # Set curve cyclic=close
			else:
				curve.flagU = 0 # Set curve not cyclic=open
				curve[0].handleTypes = [FREE, ALIGN]   #remi--todo-----
				curve[-1].handleTypes = [ALIGN, FREE]   #remi--todo-----

		else:    #--straight line/arc-segments----OK------
			points = []
			d_points.append(d_points[0])  #------ first vertex added -------------
			#curve.setType(0) #polygon_type of Blender_curve
			for i in xrange(len(d_points)-1):
				point1 = d_points[i]
				point2 = d_points[i+1]
				if point1.bulge and (i < len(d_points)-2 or self.closed):
					arc_res = 8
					verts = drawBulge(point1, point2, arc_res) #calculate additional points for bulge
					if i == 0: curve = pline.appendNurb(BezTriple.New(verts[0]))
					else: curve.append(BezTriple.New(verts[0]))
					curve[-1].handleTypes = [VECT, VECT]  #--todo--calculate bezier-tangents
					for p in verts[1:]:
						curve.append(BezTriple.New(p))
						curve[-1].handleTypes = [AUTO, AUTO]  #--todo--calculate bezier-tangents
#					curve[-1].handleTypes = [VECT, VECT]   #--todo--calculate bezier-tangents
				else:
					if i == 0: curve = pline.appendNurb(BezTriple.New(point1.loc))
					else: curve.append(BezTriple.New(point1.loc))
					curve[-1].handleTypes = [VECT, VECT]   #--todo--calculate bezier-tangents
			if self.closed:
				curve.flagU = 1 # Set curve cyclic=close
#				curve[0].handleTypes = [VECT, VECT]   #--todo--calculate bezier-tangents
			else:
				curve.flagU = 0 # Set curve not cyclic=open
				curve[0].handleTypes = [FREE, VECT]   #--todo--calculate bezier-tangents
				curve[-1].handleTypes = [VECT, FREE]  #--todo--calculate bezier-tangents

		pline.update()
		ob = SCENE.objects.new(pline) # create a new curve_object

		if thic != 0.0: #hack: Blender<2.45 curve-extrusion
			thic = thic * 0.5
			pline.setExt1(1.0)  # curve-extrusion accepts only (0.0 - 2.0)
			ob.LocZ = thic + LocZ

		transform(self.extrusion, 0, ob)
		# scaleZ to the thickness
		if thic != 0.0:
			#old_LocZ = ob.LocZ
			#ob.LocZ = 0.0
			ob.SizeZ *= abs(thic)
			#ob.LocZ = old_LocZ

		#print 'deb:polyline2dCurve.draw.END:----------------' #-----
		return ob


	def drawPoly2d(self, settings):  #---- 2dPolyline - plane wide/thic lines
		"""Generate the geometery of regular polyline.
		"""
		#print 'deb:polyline2d.draw.START:----------------' #------------------------
		points = []
		d_points = []
		swidths = []
		ewidths = []
		swidth_default = self.swidth #default start width of POLYLINEs segments
		ewidth_default = self.ewidth #default end width of POLYLINEs segments
		thic = set_thick(self.thic, settings)
		if self.spline: pline_typ = 'ps'
		elif self.curved: pline_typ = 'pc'
		else: pline_typ = 'pl'
		obname = '%s_%s' %(pline_typ, self.layer)  # create object_name from layer name
#		obname = 'pl_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]

		if len(self.points) < 2:
			#print 'deb:drawPoly2d exit, cause POLYLINE has less than 2 vertices' #---------
			return
		#d_points = self.points[:]
		#for DXFr10-format: update all points[].loc[2] == None -> 0.0 
		for point in self.points:
			if point.loc[2] == None:
				point.loc[2] = self.elevation
			d_points.append(point)
		#print 'deb:len of d_pointsList ====== ', len(d_points) #------------------------

		#add duplic of the first vertex at the end of pointslist
		d_points.append(d_points[0])

		#print 'deb:len of d_pointsList ====== ', len(d_points) #------------------------
		#print 'deb:d_pointsList ======:\n ', d_points #------------------------


		#routine to sort out of "double.vertices" --------
		minimal_dist =  settings.var['dist_min'] * 0.1
		temp_points = []
		for i in xrange(len(d_points)-1):
			point = d_points[i]
			point2 = d_points[i+1]
			#print 'deb:double.vertex p1,p2', point, point2 #------------------------
			delta = Mathutils.Vector(point2.loc) - Mathutils.Vector(point.loc)
			if delta.length > minimal_dist:
				 temp_points.append(point)
			#else: print 'deb:double.vertex sort out!' #------------------------
		temp_points.append(d_points[-1])  #------ last vertex added -------------
		d_points = temp_points   #-----vertex.list without "double.vertices"
		#print 'deb:d_pointsList =after DV-outsorting=====:\n ', d_points #------------------------

		#print 'deb:len of d_pointsList ====== ', len(d_points) #------------------------
		if len(d_points) < 2:
			#print 'deb:drawPoly2d corrupted Vertices' #---------
			return

		#analyse of straight- and bulge-segments (generation of additional points for bulge)
		exist_wide_segment = False
		for i in xrange(len(d_points)-1):
			point1 = d_points[i]
			point2 = d_points[i+1]
			#print 'deb:pline.tocalc.point1:', point1 #------------------------
			#print 'deb:pline.tocalc.point2:', point2 #------------------------

			swidth = point1.swidth
			ewidth = point1.ewidth
			if swidth == None: swidth = swidth_default
			if ewidth == None: ewidth = ewidth_default

			if swidth != 0.0 or ewidth != 0.0: exist_wide_segment = True

			if settings.var['width_force']:  # force minimal width for thin segments
				if swidth < settings.var['width_min']: swidth = settings.var['width_min']
				if ewidth < settings.var['width_min']: ewidth = settings.var['width_min']
				if not settings.var['width_on']:  # then force minimal width for all segments
					swidth = settings.var['width_min']
					ewidth = settings.var['width_min']

			if point1.bulge and (i < (len(d_points)-2) or self.closed):
				verts = drawBulge(point1, point2, settings.var['arc_res']) #calculate additional points for bulge
				points.extend(verts)
				delta_width = (ewidth - swidth) / len(verts)
				width_list = [swidth + (delta_width * ii) for ii in xrange(len(verts)+1)]
				swidths.extend(width_list[0:-1])
				ewidths.extend(width_list[1:])
			else:
				points.append(point1.loc)
				swidths.append(swidth)
				ewidths.append(ewidth)


		#--calculate width_vectors: left-side- and right-side-points ----------------
		# 1.level:IF width  ---------------------------------------
		if (settings.var['width_on'] and exist_wide_segment) or settings.var['width_force']:
			points.append(d_points[0].loc)  #temporarly add first vertex at the end (for better loop)

			pointsLs = []   # list of left-start-points
			pointsLe = []   # list of left-end-points
			pointsRs = []   # list of right-start-points
			pointsRe = []   # list of right-end-points
			pointsW  = []   # list of entire-border-points
			#rotMatr90 = Mathutils.Matrix(rotate 90 degree around Z-axis) = normalvectorXY
			rotMatr90 = Mathutils.Matrix([0, -1, 0], [1, 0, 0], [0, 0, 1])
			for i in xrange(len(points)-1):
				point1 = points[i]
				point2 = points[i+1]
				point1vec = Mathutils.Vector(point1)
				point2vec = Mathutils.Vector(point2)
				swidth05 = swidths[i] * 0.5
				ewidth05 = ewidths[i] * 0.5
				if swidth05 == 0: swidth05 = 0.5 * settings.var['dist_min'] #minimal width
				if ewidth05 == 0: ewidth05 = 0.5 * settings.var['dist_min'] #minimal width

				normal_vector = rotMatr90 * (point2vec-point1vec).normalize()
				swidth05vec = swidth05 * normal_vector
				ewidth05vec = ewidth05 * normal_vector
				pointsLs.append(point1vec + swidth05vec) #vertex left start
				pointsRs.append(point1vec - swidth05vec) #vertex right start
				pointsLe.append(point2vec + ewidth05vec) #vertex left end
				pointsRe.append(point2vec - ewidth05vec) #vertex right end

			pointsLc, pointsRc = [], []

			# 2.level:IF width and corner-intersection activated
			if settings.var['pl_section_on']:  #optional clean corner-intersections
				if not self.closed:
					pointsLc.append(pointsLs[0])
					pointsRc.append(pointsRs[0])
					lenL = len(pointsLs)-2 #without the last point at the end of the list
				else:
					pointsLs.append(pointsLs[0])
					pointsRs.append(pointsRs[0])
					pointsLe.append(pointsLe[0])
					pointsRe.append(pointsRe[0])
					points.append(points[0])
					lenL = len(pointsLs)-1  #without the duplic of the first point at the end of the list
					#print 'deb:pointsLs():\n',  pointsLs  #----------------
					#print 'deb:lenL, len.pointsLs():', lenL,',', len(pointsLs)  #----------------
				for i in xrange(lenL):
					pointVec = Mathutils.Vector(points[i+1])
					#print 'deb:pointVec: ', pointVec  #-------------
					#compute left-corner-points
					vecL1 = pointsLs[i]
					vecL2 = pointsLe[i]
					vecL3 = pointsLs[i+1]
					vecL4 = pointsLe[i+1]
					#print 'deb:vectorsL:---------\n', vecL1,'\n',vecL2,'\n',vecL3,'\n',vecL4  #-------------
					#cornerpointL = Geometry.LineIntersect2D(vec1, vec2, vec3, vec4)
					cornerpointL = Mathutils.LineIntersect(vecL1, vecL2, vecL3, vecL4)
					#print 'deb:cornerpointL: ', cornerpointL  #-------------

					#compute right-corner-points
					vecR1 = pointsRs[i]
					vecR2 = pointsRe[i]
					vecR3 = pointsRs[i+1]
					vecR4 = pointsRe[i+1]
					#print 'deb:vectorsR:---------\n', vecR1,'\n',vecR2,'\n',vecR3,'\n',vecR4  #-------------
					#cornerpointR = Geometry.LineIntersect2D(vec1, vec2, vec3, vec4)
					cornerpointR = Mathutils.LineIntersect(vecR1, vecR2, vecR3, vecR4)
					#print 'deb:cornerpointR: ', cornerpointR  #-------------

					#if diststance(cornerL-center-cornerR) < limiter * (seg1_endWidth + seg2_startWidth)
					if cornerpointL != None and cornerpointR != None:
						cornerpointL = cornerpointL[0]
						cornerpointR = cornerpointR[0]
						max_cornerDist = (vecL2 - vecR2).length + (vecL3 - vecR3).length
						is_cornerDist = (cornerpointL - pointVec).length + (cornerpointR - pointVec).length
						# anglecut --------- limited by ANGLECUT_LIMIT (1.0 - 5.0)
						if is_cornerDist < max_cornerDist * settings.var['angle_cut']:
							pointsLc.append(cornerpointL)
							pointsRc.append(cornerpointR)
						else:
							pointsLc.extend((pointsLe[i],pointsLs[i+1]))
							pointsRc.extend((pointsRe[i],pointsRs[i+1]))
					else:
						pointsLc.extend((pointsLe[i],pointsLs[i+1]))
						pointsRc.extend((pointsRe[i],pointsRs[i+1]))
				if not self.closed:
					pointsLc.append(pointsLe[-2])
					pointsRc.append(pointsRe[-2])
				else:
					"""   """

			# 2.level:IF width but not corner-intersection activated
			else:
				# points_multiplexer of start-points and end-points
				lenL = len(pointsLs) - 1 #without the duplic of the first point at the end of list
				if self.closed: lenL += 1  #inclusive the duplic of the first point at the end of list
				for i in xrange(lenL):
					pointsLc.extend((pointsLs[i], pointsLe[i]))
					pointsRc.extend((pointsRs[i], pointsRe[i]))

			pointsW = pointsLc + pointsRc  # all_points_List = left_side + right_side
			#print 'deb:pointsW():\n',  pointsW  #----------------
			len1 = int(len(pointsW) * 0.5)
			#print 'deb:len1:', len1  #-----------------------

			# 2.level:IF width and thickness  ---------------------
			if thic != 0:
				thic_pointsW = []
				thic_pointsW.extend([[point[0], point[1], point[2]+thic] for point in pointsW])
				if thic < 0.0:
					thic_pointsW.extend(pointsW)
					pointsW = thic_pointsW
				else:
					pointsW.extend(thic_pointsW)
				faces = []
				f_start, f_end = [], []
				f_bottom = [[num, num+1, len1+num+1, len1+num] for num in xrange(len1-1)]
				f_top   = [[num, len1+num, len1+num+1, num+1] for num in xrange(len1+len1, len1+len1+len1-1)]
				f_left   = [[num, len1+len1+num, len1+len1+num+1, num+1] for num in xrange(len1-1)]
				f_right  = [[num, num+1, len1+len1+num+1, len1+len1+num] for num in xrange(len1, len1+len1-1)]

				if self.closed:
					f_bottom.append([len1-1, 0, len1, len1+len1-1])  #bottom face
					f_top.append(   [len1+len1+len1-1, len1+len1+len1+len1-1, len1+len1+len1, len1+len1+0])  #top face
					f_left.append(  [0, len1-1, len1+len1+len1-1, len1+len1])  #left face
					f_right.append( [len1, len1+len1+len1, len1+len1+len1+len1-1, len1+len1-1])  #right face
				else:
					f_start = [[0, len1, len1+len1+len1, len1+len1]]
					f_end   = [[len1+len1-1, 0+len1-1, len1+len1+len1-1, len1+len1+len1+len1-1]]

				faces = f_bottom + f_top + f_left + f_right + f_start + f_end
				#faces = f_bottom + f_top
				#faces = f_left + f_right + f_start + f_end
				#print 'deb:faces_list:\n', faces  #-----------------------
				me = Mesh.New(obname)		   # create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
				me.verts.extend(pointsW)		# add vertices to mesh
				me.faces.extend(faces)	# add faces to the mesh

				# each MeshSite becomes vertexGroup for easier material assignment ---------------------
				# The mesh must first be linked to an object so the method knows which object to update.
				# This is because vertex groups in Blender are stored in the object -- not in the mesh,
				# which may be linked to more than one object.
				if settings.var['vGroup_on']:
					# each MeshSite becomes vertexGroup for easier material assignment ---------------------
					replace = Blender.Mesh.AssignModes.REPLACE  #or .AssignModes.ADD
					vg_left, vg_right, vg_top, vg_bottom = [], [], [], []
					for v in f_left: vg_left.extend(v)
					for v in f_right: vg_right.extend(v)
					for v in f_top: vg_top.extend(v)
					for v in f_bottom: vg_bottom.extend(v)
					me.addVertGroup('side.left')  ; me.assignVertsToGroup('side.left',  list(set(vg_left)), 1.0, replace)
					me.addVertGroup('side.right') ; me.assignVertsToGroup('side.right', list(set(vg_right)), 1.0, replace)
					me.addVertGroup('side.top')   ; me.assignVertsToGroup('side.top',   list(set(vg_top)), 1.0, replace)
					me.addVertGroup('side.bottom'); me.assignVertsToGroup('side.bottom',list(set(vg_bottom)), 1.0, replace)
					if not self.closed:
						me.addVertGroup('side.start'); me.assignVertsToGroup('side.start', f_start[0], 1.0, replace)
						me.addVertGroup('side.end')  ; me.assignVertsToGroup('side.end',   f_end[0],   1.0, replace)


			# 2.level:IF width, but no-thickness  ---------------------
			else:
				faces = []
				faces = [[num, len1+num, len1+num+1, num+1] for num in xrange(len1 - 1)]
				if self.closed:
					faces.append([len1, 0, len1-1, len1+len1-1])
				me = Mesh.New(obname)		   # create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
				me.verts.extend(pointsW)		# add vertices to mesh
				me.faces.extend(faces)	# add faces to the mesh


		# 1.level:IF no-width, but thickness ---------------------
		elif thic != 0:
			len1 = len(points)
			thic_points = []
			thic_points.extend([[point[0], point[1], point[2]+thic] for point in points])
			if thic < 0.0:
				thic_points.extend(points)
				points = thic_points
			else:
				points.extend(thic_points)
			faces = []
			faces = [[num, num+1, num+len1+1, num+len1] for num in xrange(len1 - 1)]
			if self.closed:
				faces.append([len1-1, 0, len1, 2*len1-1])
			me = Mesh.New(obname)		   # create a new mesh
			ob = SCENE.objects.new(me) # create a new mesh_object
			me.verts.extend(points)   # add vertices to mesh
			me.faces.extend(faces)	# add faces to the mesh

		# 1.level:IF no-width and no-thickness  ---------------------
		else:
			edges = [[num, num+1] for num in xrange(len(points)-1)]
			if self.closed:
				edges.append([len(points)-1, 0])
			me = Mesh.New(obname)		   # create a new mesh
			ob = SCENE.objects.new(me) # create a new mesh_object
			me.verts.extend(points)   # add vertices to mesh
			me.edges.extend(edges)	# add edges to the mesh

		transform(self.extrusion, 0, ob)
		#print 'deb:polyline.draw.END:----------------' #-----------------------
		return ob




class Vertex(object):  #-----------------------------------------------------------------
	"""Generic vertex object used by polylines (and maybe others).
	"""

	def __init__(self, obj=None):
		"""Initializes vertex data.

		The optional obj arg is an entity object of type vertex.
		"""
		#print 'deb:Vertex.init.START:----------------' #-----------------------
		self.loc = [0,0,0]
		self.face = []
		self.swidth = 0
		self.ewidth = 0
		self.bulge = 0
		if obj is not None:
			if not obj.type == 'vertex':
				raise TypeError, "Wrong type %s for vertex object!" %obj.type
			self.type = obj.type
			self.data = obj.data[:]
			self.get_props(obj.data)
		#print 'deb:Vertex.init.END:----------------' #------------------------


	def get_props(self, data):
		"""Gets coords for a VERTEX type object.

		Each vert can have a number of properties.
		Verts should be coded as
		10:xvalue
		20:yvalue
		40:startwidth or 0
		41:endwidth or 0
		42:bulge or 0
		"""
		self.x = getit(data, 10, None)
		self.y = getit(data, 20, None)
		self.z = getit(data, 30, None)

		self.flags  = getit(data, 70, 0) # flags
		self.curved = self.flags&1   # Bezier-curve-fit:additional-vertex
		self.curv_t = self.flags&2   # Bezier-curve-fit:tangent exists
		self.spline = self.flags&8   # Bspline-fit:additional-vertex
		self.splin2 = self.flags&16  # Bspline-fit:control-vertex
		self.poly3d = self.flags&32  # polyline3d:control-vertex
		self.plmesh = self.flags&64  # polymesh3d:control-vertex
		self.plface = self.flags&128 # polyface

		# if PolyFace.Vertex with Face_definition
		if self.curv_t:
			self.curv_tangent =  getit(data, 50, None) # curve_tangent

		if self.plface and not self.plmesh:
				v1 = getit(data, 71, 0) # polyface:Face.vertex 1.
				v2 = getit(data, 72, 0) # polyface:Face.vertex 2.
				v3 = getit(data, 73, 0) # polyface:Face.vertex 3.
				v4 = getit(data, 74, None) # polyface:Face.vertex 4.
				self.face = [abs(v1)-1,abs(v2)-1,abs(v3)-1]
				if v4 != None:
					self.face.append(abs(v4)-1)
		else:   #--parameter for polyline2d
			self.swidth = getit(data, 40, None) # start width
			self.ewidth = getit(data, 41, None) # end width
			self.bulge  = getit(data, 42, 0) # bulge of segment


	def __len__(self):
		return 3


	def __getitem__(self, key):
		return self.loc[key]


	def __setitem__(self, key, value):
		if key in [0,1,2]:
			self.loc[key]


	def __iter__(self):
		return self.loc.__iter__()


	def __str__(self):
		return str(self.loc)


	def __repr__(self):
		return "Vertex %s, swidth=%s, ewidth=%s, bulge=%s, face=%s" %(self.loc, self.swidth, self.ewidth, self.bulge, self.face)


	def getx(self):
		return self.loc[0]
	def setx(self, value):
		self.loc[0] = value
	x = property(getx, setx)


	def gety(self):
		return self.loc[1]
	def sety(self, value):
		self.loc[1] = value
	y = property(gety, sety)


	def getz(self):
		return self.loc[2]
	def setz(self, value):
		self.loc[2] = value
	z = property(getz, setz)



class Text:  #-----------------------------------------------------------------
	"""Class for objects representing dxf Text.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type text as input.
		"""
		if not obj.type == 'text':
			raise TypeError, "Wrong type %s for text object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# required data
		self.height = 1.7 * obj.get_type(40)[0]  #text.height
		self.value = obj.get_type(1)[0]   #The text string value

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.color_index = getit(obj, 62, BYLAYER)
		self.thic =  getit(obj, 39, 0)

		self.rotation = getit(obj, 50, 0)  # radians
		self.width_factor = getit(obj, 41, 1) # Scaling factor along local x axis
		self.oblique = getit(obj, 51, 0) # oblique angle: skew in degrees -90 <= oblique <= 90

		#self.style = getit(obj, 7, 'STANDARD') # todo---- Text style name (optional, default = STANDARD)

		#Text generation flags (optional, default = 0):
		#2 = backward (mirrored in X),
		#4 = upside down (mirrored in Y)
		self.flags = getit(obj, 71, 0)
		self.mirrorX, self.mirrorY = 1.0, 1.0
		if self.flags&2: self.mirrorX = - 1.0
		if self.flags&4: self.mirrorY = - 1.0

		# vertical.alignment: 0=baseline, 1=bottom, 2=middle, 3=top
		self.valignment = getit(obj, 73, 0)
		#Horizontal text justification type (optional, default = 0) integer codes (not bit-coded)
		#0=left, 1=center, 2=right
		#3=aligned, 4=middle, 5=fit
		self.halignment = getit(obj, 72, 0)

		self.layer = getit(obj.data, 8, None)
		self.loc1, self.loc2 = self.get_loc(obj.data)
		if self.loc2[0] != None and self.halignment != 5: 
			self.loc = self.loc2
		else:
			self.loc = self.loc1
		self.extrusion = get_extrusion(obj.data)



	def get_loc(self, data):
		"""Gets adjusted location for text type objects.

		If group 72 and/or 73 values are nonzero then the first alignment point values
		are ignored and AutoCAD calculates new values based on the second alignment
		point and the length and height of the text string itself (after applying the
		text style). If the 72 and 73 values are zero or missing, then the second
		alignment point is meaningless.
		I don't know how to calc text size...
		"""
		# bottom left x, y, z and justification x, y, z = 0
		#x, y, z, jx, jy, jz = 0, 0, 0, 0, 0, 0
		x  = getit(data, 10, None) #First alignment point (in OCS). 
		y  = getit(data, 20, None)
		z  = getit(data, 30, 0.0)
		jx = getit(data, 11, None) #Second alignment point (in OCS). 
		jy = getit(data, 21, None)
		jz = getit(data, 31, 0.0)
		return [x, y, z],[jx, jy, jz]


	def __repr__(self):
		return "%s: layer - %s, value - %s" %(self.__class__.__name__, self.layer, self.value)


	def draw(self, settings):
		"""for TEXTs: generate Blender_geometry.
		"""
		obname = 'tx_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]
		txt = Text3d.New(obname)
		ob = SCENE.objects.new(txt) # create a new text_object

		txt.setText(self.value)
		txt.setSize(1.0) #Blender<2.45 accepts only (0.0 - 5.0)
		#txt.setSize(self.height)
		#txt.setWidth(self.bold)
		#setLineSeparation(sep)
		txt.setShear(self.oblique/90)

		thic = set_thick(self.thic, settings)
		if thic != 0.0:
			thic = self.thic * 0.5
			self.loc[2] += thic
			txt.setExtrudeDepth(1.0)  #Blender<2.45 accepts only (0.1 - 10.0)
		if self.halignment == 0:
			align = Text3d.LEFT
		elif self.halignment == 1:
			align = Text3d.MIDDLE
		elif self.halignment == 2:
			align = Text3d.RIGHT
		else:
			align = Text3d.LEFT
		txt.setAlignment(align)

		if self.valignment == 1:
			txt.setYoffset(0.0)
		elif self.valignment == 2:
			txt.setYoffset(- self.height * 0.5)
		elif self.valignment == 3:
			txt.setYoffset(- self.height)

		# move the object center to the text location
		ob.loc = tuple(self.loc)
		transform(self.extrusion, self.rotation, ob)

		# flip it and scale it to the text width
		ob.SizeX *= self.height * self.width_factor * self.mirrorX
		ob.SizeY *= self.height * self.mirrorY
		if thic != 0.0:	ob.SizeZ *= abs(thic)
		return ob


	
def set_thick(thickness, settings):
	"""Set thickness relative to settings variables.
	
	python trick: sign(x)=cmp(x,0)
	"""
	if settings.var['thick_force']:
		if settings.var['thick_on']:
			if abs(thickness) <  settings.var['thick_min']:
				thic = settings.var['thick_min'] * cmp(self.thic,0)
			else: thic = thickness
		else: thic = settings.var['thick_min']
	else: 
		if settings.var['thick_on']: thic = thickness
		else: thic = 0.0
	return thic




class Mtext:  #-----------------------------------------------------------------
	"""Class for objects representing dxf Mtext.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type mtext as input.
		"""
		if not obj.type == 'mtext':
			raise TypeError, "Wrong type %s for mtext object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# required data
		self.height = obj.get_type(40)[0]
		self.width = obj.get_type(41)[0]
		self.alignment = obj.get_type(71)[0] # alignment 1=TL, 2=TC, 3=TR, 4=ML, 5=MC, 6=MR, 7=BL, 8=BC, 9=BR
		self.value = self.get_text(obj.data) # The text string value

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.color_index = getit(obj, 62, BYLAYER)
		self.rotation = getit(obj, 50, 0)  # radians

		self.width_factor = getit(obj, 42, 1) # Scaling factor along local x axis
		self.line_space = getit(obj, 44, 1) # percentage of default

		self.layer = getit(obj.data, 8, None)
		self.loc = self.get_loc(obj.data)
		self.extrusion = get_extrusion(obj.data)


	def get_text(self, data):
		"""Reconstructs mtext data from dxf codes.
		"""
		primary = ''
		secondary = []
		for item in data:
			if item[0] == 1: # There should be only one primary...
				primary = item[1]
			elif item[0] == 3: # There may be any number of extra strings (in order)
				secondary.append(item[1])
		if not primary:
			#raise ValueError, "Empty Mtext Object!"
			string = "Empty Mtext Object!"
		if not secondary:
			string = primary.replace(r'\P', '\n')
		else:
			string = ''.join(secondary)+primary
			string = string.replace(r'\P', '\n')
		return string


	def get_loc(self, data):
		"""Gets location for a mtext type objects.

		Mtext objects have only one point indicating 
		"""		
		loc = [0, 0, 0]
		loc[0] = getit(data, 10, None)
		loc[1] = getit(data, 20, None)
		loc[2] = getit(data, 30, 0.0)
		return loc


	def __repr__(self):
		return "%s: layer - %s, value - %s" %(self.__class__.__name__, self.layer, self.value)


	def draw(self, settings):
		"""for MTEXTs: generate Blender_geometry.
		"""
		# Now Create an object
		obname = 'tm_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]
		txt = Text3d.New(obname)
		ob = SCENE.objects.new(txt) # create a new text_object

		txt.setSize(1)
		# Blender doesn't give access to its text object width currently
		# only to the text3d's curve width...
		#txt.setWidth(text.width/10)
		txt.setLineSeparation(self.line_space)
		txt.setExtrudeDepth(0.5)
		txt.setText(self.value)

		# scale it to the text size
		ob.SizeX = self.height * self.width_factor
		ob.SizeY = self.height
		ob.SizeZ = self.height

		# move the object center to the text location
		ob.loc = tuple(self.loc)
		transform(self.extrusion, self.rotation, ob)

		return ob




class Circle:  #-----------------------------------------------------------------
	"""Class for objects representing dxf Circles.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type circle as input.
		"""
		if not obj.type == 'circle':
			raise TypeError, "Wrong type %s for circle object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# required data
		self.radius = obj.get_type(40)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj.data, 8, None)
		self.loc = self.get_loc(obj.data)
		self.extrusion = get_extrusion(obj.data)



	def get_loc(self, data):
		"""Gets the center location for circle type objects.

		Circles have a single coord location.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 10, None)
		loc[1] = getit(data, 20, None)
		loc[2] = getit(data, 30, 0.0)
		return loc



	def __repr__(self):
		return "%s: layer - %s, radius - %s" %(self.__class__.__name__, self.layer, self.radius)


	def draw(self, settings):
		"""for CIRCLE: generate Blender_geometry.
		"""
		obname = 'ci_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]
		radius = self.radius

		thic = set_thick(self.thic, settings)
		if settings.var['curves_on']:
			c = Curve.New(obname)	# create new curve data
			p1 = (0, -radius, 0)
			p2 = (radius, 0, 0)
			p3 = (0, radius, 0)
			p4 = (-radius, 0, 0)

			p1 = BezTriple.New(p1)
			p2 = BezTriple.New(p2)
			p3 = BezTriple.New(p3)
			p4 = BezTriple.New(p4)

			curve = c.appendNurb(p1)
			curve.append(p2)
			curve.append(p3)
			curve.append(p4)
			for point in curve:
				point.handleTypes = [AUTO, AUTO]
			curve.flagU = 1 # Set curve cyclic=closed
			c.update()

		else:
			if radius < 2 * settings.var['dist_min']: # if circumfence is very small
				verts_num = settings.var['thin_res'] # set a fixed number of verts
			else:
				#verts = circ/settings.var['dist_min'] # figure out how many verts we need
				verts_num = settings.var['arc_res'] # figure out how many verts we need
				if verts_num > 500: verts_num = 500 # Blender accepts only values [3:500]
			if thic != 0:
				loc2 = thic * 0.5	#-----blenderAPI draw Cylinder with 2*thickness
				self.loc[2] += loc2  #---new location for the basis of cylinder
				#print 'deb:circleDraw:self.loc2:', self.loc  #-----------------------
				c = Mesh.Primitives.Cylinder(int(verts_num), radius*2, abs(thic))
			else:
				c = Mesh.Primitives.Circle(int(verts_num), radius*2)

		ob = SCENE.objects.new(c, obname) # create a new circle_mesh_object
		ob.loc = tuple(self.loc)
		transform(self.extrusion, 0, ob)
		return ob



class Arc:  #-----------------------------------------------------------------
	"""Class for objects representing dxf arcs.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type arc as input.
		"""
		if not obj.type == 'arc':
			raise TypeError, "Wrong type %s for arc object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# required data
		self.radius = obj.get_type(40)[0]
		self.start_angle = obj.get_type(50)[0]
		self.end_angle = obj.get_type(51)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj.data, 8, None)
		self.loc = self.get_loc(obj.data)
		self.extrusion = get_extrusion(obj.data)



	def get_loc(self, data):
		"""Gets the center location for arc type objects.

		Arcs have a single coord location.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 10, None)
		loc[1] = getit(data, 20, None)
		loc[2] = getit(data, 30, 0.0)
		return loc



	def __repr__(self):
		return "%s: layer - %s, radius - %s" %(self.__class__.__name__, self.layer, self.radius)


	def draw(self, settings):
		"""for ARC: generate Blender_geometry.
		"""
		obname = 'ar_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]

		center = self.loc
		radius = self.radius
		start = self.start_angle
		end = self.end_angle
		#print 'deb:drawArc: center, radius, start, end:\n', center, radius, start, end  #---------
		thic = set_thick(self.thic, settings)

		if settings.var['curves_on']:
			arc_res = 8
			verts, edges = drawArc(None, radius, start, end, arc_res)
			arc = Curve.New(obname)	# create new curve data
			curve = arc.appendNurb(BezTriple.New(verts[0]))
			for p in verts[1:]:
				curve.append(BezTriple.New(p))
			for point in curve:
				point.handleTypes = [AUTO, AUTO]
				#print 'deb:arc.draw point=', point  #---------------
			curve[0].handleTypes = [FREE, VECT]   #remi--todo-----
			curve[-1].handleTypes = [VECT, FREE]   #remi--todo-----
			curve.flagU = 0 # Set curve not cyclic=open
			arc.update()

		else:
			arc = Mesh.New(obname)		   # create a new mesh
			verts, edges = drawArc(None, radius, start, end, settings.var['arc_res'])
			if thic != 0:
				len1 = len(verts)
				thic_verts = []
				thic_verts.extend([[point[0], point[1], point[2]+thic] for point in verts])
				if thic < 0.0:
					thic_verts.extend(verts)
					verts = thic_verts
				else:
					verts.extend(thic_verts)
				faces = []
				#print 'deb:len1:', len1  #-----------------------
				#print 'deb:verts:', verts  #remi-todo----- why is this List inhomogene ----------
				faces = [[num, num+1, num+len1+1, num+len1] for num in xrange(len1 - 1)]

				arc.verts.extend(verts)	# add vertices to mesh
				arc.faces.extend(faces)	 # add faces to the mesh
			else:
				arc.verts.extend(verts)	# add vertices to mesh
				arc.edges.extend(edges)	 # add edges to the mesh

		ob = SCENE.objects.new(arc) # create a new arc_object
		ob.loc = tuple(center)
		transform(self.extrusion, 0, ob)
		#ob.size = (1,1,1)

		return ob


class BlockRecord:  #-----------------------------------------------------------------
	"""Class for objects representing dxf block_records.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type block_record as input.
		"""
		if not obj.type == 'block_record':
			raise TypeError, "Wrong type %s for block_record object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# required data
		self.name =  getit(obj, 2, None)

		# optional data (with defaults)
		self.insertion_units =  getit(obj, 70, None)
		self.insert_units = getit(obj, 1070, None)
		"""code 1070 Einfügeeinheiten:
		0 = Keine Einheiten; 1 = Zoll; 2 = Fuß; 3 = Meilen; 4 = Millimeter;
		5 = Zentimeter; 6 = Meter; 7 = Kilometer; 8 = Mikrozoll;
		9 = Mils; 10 = Yard; 11 = Angstrom; 12 = Nanometer;
		13 = Mikrons; 14 = Dezimeter; 15 = Dekameter;
		16 = Hektometer; 17 = Gigameter; 18 = Astronomische Einheiten;
		19 = Lichtjahre; 20 = Parsecs
		"""


	def __repr__(self):
		return "%s: name - %s, insert units - %s" %(self.__class__.__name__, self.name, self.insertion_units)




class Block:  #-----------------------------------------------------------------
	"""Class for objects representing dxf blocks.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type block as input.
		"""
		if not obj.type == 'block':
			raise TypeError, "Wrong type %s for block object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]
		self.name = obj.name

		# required data
		self.flags = obj.get_type(70)[0]
		self.entities = dxfObject('block_contents') #creates empty entities_container for this block
		self.entities.data = objectify([ent for ent in obj.data if type(ent) != list])

		# optional data (with defaults)
		self.path = getit(obj, 1, '')
		self.discription = getit(obj, 4, '')

		self.layer = getit(obj.data, 8, None)
		self.loc = self.get_loc(obj.data)


	def get_loc(self, data):
		"""Gets the insert point of the block.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 10, None) # 10 = x
		loc[1] = getit(data, 20, None) # 20 = y
		loc[2] = getit(data, 30,  0.0) # 30 = z
		return loc


	def __repr__(self):
		return "%s: name - %s, description - %s, xref-path - %s" %(self.__class__.__name__, self.name, self.discription, self.path)




class Insert:  #-----------------------------------------------------------------
	"""Class for objects representing dxf inserts.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type insert as input.
		"""
		if not obj.type == 'insert':
			raise TypeError, "Wrong type %s for insert object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# required data
		self.blockname = obj.get_type(2)[0]

		# optional data (with defaults)
		self.rotation =  getit(obj, 50, 0)
		self.space = getit(obj, 67, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj.data, 8, None)
		self.loc = self.get_loc(obj.data)
		self.scale = self.get_scale(obj.data)
		self.rows, self.columns = self.get_array(obj.data)
		self.extrusion = get_extrusion(obj.data)



	def get_loc(self, data):
		"""Gets the center location for block type objects.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 10, 0.0)
		loc[1] = getit(data, 20, 0.0)
		loc[2] = getit(data, 30, 0.0)
		return loc



	def get_scale(self, data):
		"""Gets the x/y/z scale factor for the block.
		"""
		scale = [1, 1, 1]
		scale[0] = getit(data, 41, 1.0)
		scale[1] = getit(data, 42, 1.0)
		scale[2] = getit(data, 43, 1.0)
		return scale



	def get_array(self, data):
		"""Returns the pair (row number, row spacing), (column number, column spacing).
		"""
		columns = getit(data, 70, 1)
		rows	= getit(data, 71, 1)
		cspace  = getit(data, 44, 0.0)
		rspace  = getit(data, 45, 0.0)
		return (rows, rspace), (columns, cspace)



	def __repr__(self):
		return "%s: layer - %s, blockname - %s" %(self.__class__.__name__, self.layer, self.blockname)


	def draw(self, settings, deltaloc):
		"""for INSERT(block): draw empty-marker for duplicated Blender_Group.

		Blocks are made of three objects:
			the block_record in the tables section
			the block in the blocks section
			the insert object in the entities section

		block_records give the insert units, blocks provide the objects drawn in the
		block, and the insert object gives the location/scale/rotation of the block
		instances.  To draw a block you must first get a group with all the
		blocks entities drawn in it, then scale the entities to match the world
		units, then dupligroup that data to an object matching each insert object.
		"""

		obname = 'in_%s' %self.blockname  # create object name from block name
		obname = obname[:MAX_NAMELENGTH]

		if settings.drawTypes['insert']:  #if insert_drawType activated
			ob = SCENE.objects.new('Empty', obname) # create a new empty_object
			empty_size = 1.0 * settings.var['g_scale']
			if   empty_size < 0.01:  empty_size = 0.01
			elif empty_size > 10.0:  empty_size = 10.0
			ob.drawSize = empty_size

			# get our block_def-group
			block = settings.blocks(self.blockname)
			ob.DupGroup = block
			ob.enableDupGroup = True

		#print 'deb:draw.block.deltaloc:', deltaloc #--------------------
		ob.loc = tuple(self.loc)
		if deltaloc:
			deltaloc = rotXY_Vec(self.rotation, deltaloc)
			#print 'deb:draw.block.loc:', deltaloc  #--------------------
			ob.loc = [ob.loc[0]+deltaloc[0], ob.loc[1]+deltaloc[1], ob.loc[2]+deltaloc[2]]
		transform(self.extrusion, self.rotation, ob)
		ob.size = tuple(self.scale)
		return ob




class Ellipse:  #-----------------------------------------------------------------
	"""Class for objects representing dxf ellipses.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type ellipse as input.
		"""
		if not obj.type == 'ellipse':
			raise TypeError, "Wrong type %s for ellipse object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# required data
		self.ratio = obj.get_type(40)[0]
		self.start_angle = obj.get_type(41)[0]
		self.end_angle = obj.get_type(42)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0.0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj.data, 8, None)
		self.loc = self.get_loc(obj.data)
		self.major = self.get_major(obj.data)
		self.extrusion = get_extrusion(obj.data)
		self.radius = sqrt(self.major[0]**2 + self.major[0]**2 + self.major[0]**2)


	def get_loc(self, data):
		"""Gets the center location for arc type objects.

		Arcs have a single coord location.
		"""
		loc = [0.0, 0.0, 0.0]
		loc[0] = getit(data, 10, 0.0)
		loc[1] = getit(data, 20, 0.0)
		loc[2] = getit(data, 30, 0.0)
		return loc


	def get_major(self, data):
		"""Gets the major axis for ellipse type objects.

		The ellipse major axis defines the rotation of the ellipse and its radius.
		"""
		loc = [0.0, 0.0, 0.0]
		loc[0] = getit(data, 11, 0.0)
		loc[1] = getit(data, 21, 0.0)
		loc[2] = getit(data, 31, 0.0)
		return loc


	def __repr__(self):
		return "%s: layer - %s, radius - %s" %(self.__class__.__name__, self.layer, self.radius)


	def draw(self, settings):
		"""for ELLIPSE: generate Blender_geometry.
		"""
		# Generate the geometery
		thic = set_thick(self.thic, settings)

		if settings.var['curves_on']:
			ob = drawCurveArc(self)
		else:
			obname = 'el_%s' %self.layer  # create object name from layer name
			obname = obname[:MAX_NAMELENGTH]
			me = Mesh.New(obname)		   # create a new mesh
			ob = SCENE.objects.new(me) # create a new mesh_object

			major = Mathutils.Vector(self.major)
			#remi--todo----AngleBetweenVecs makes abs(value)!-----
			delta = Mathutils.AngleBetweenVecs(major, WORLDX)
			center = self.loc
			radius = major.length
			start = degrees(self.start_angle)
			end = degrees(self.end_angle)
			verts, edges = drawArc(None, radius, start, end, settings.var['arc_res'])

			if thic != 0:
				len1 = len(verts)
				thic_verts = []
				thic_verts.extend([[point[0], point[1], point[2]+thic] for point in verts])
				if thic < 0.0:
					thic_verts.extend(verts)
					verts = thic_verts
				else:
					verts.extend(thic_verts)
				faces = []
				#print 'deb:len1:', len1  #-----------------------
				#print 'deb:verts:', verts  #remi--todo----- why is this List inhomogene? ----------
				faces = [[num, num+1, num+len1+1, num+len1] for num in xrange(len1 - 1)]

				me.verts.extend(verts)	# add vertices to mesh
				me.faces.extend(faces)	 # add faces to the mesh
			else:
				me.verts.extend(verts)	# add vertices to mesh
				me.edges.extend(edges)	 # add edges to the mesh

		ob.loc = tuple(center)
		ob.SizeY = self.ratio
		transform(self.extrusion, 0, ob)

		return ob



class Face:  #-----------------------------------------------------------------
	"""Class for objects representing dxf 3d faces.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type 3dfaceplot as input.
		"""
		if not obj.type == '3dface':
			raise TypeError, "Wrong type %s for 3dface object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj.data, 8, None)
		self.points = self.get_points(obj.data)


	def get_points(self, data):
		"""Gets 3-4 points for a 3d face type object.

		Faces have three or optionally four verts.
		"""
		a = [0, 0, 0]
		b = [0, 0, 0]
		c = [0, 0, 0]
		d = [0, 0, 0]
		a[0] = getit(data, 10, None) # 10 = x
		a[1] = getit(data, 20, None) # 20 = y
		a[2] = getit(data, 30,  0.0) # 30 = z
		b[0] = getit(data, 11, None)
		b[1] = getit(data, 21, None)
		b[2] = getit(data, 31,  0.0)
		c[0] = getit(data, 12, None)
		c[1] = getit(data, 22, None)
		c[2] = getit(data, 32,  0.0)
		out = [a,b,c]

		d[0] =  getit(data, 13, None)
		if d[0] != None:
			d[1] = getit(data, 23, None)
			d[2] = getit(data, 33,  0.0)
			out.append(d)

		#if len(out) < 4: print '3dface with only 3 vertices:\n',a,b,c,d #-----------------
		return out


	def __repr__(self):
		return "%s: layer - %s, points - %s" %(self.__class__.__name__, self.layer, self.points)


	def draw(self, settings):
		"""for 3DFACE: generate Blender_geometry.
		"""
		# Generate the geometery
		points = self.points

		global activObjectLayer
		global activObjectName
		#print 'deb:draw:face.ob IN activObjectName: ', activObjectName #---------------------

		if activObjectLayer == self.layer and settings.var['one_mesh_on']:
			obname = activObjectName
			#print 'deb:face.draw obname from activObjectName: ', obname #---------------------
			ob = Object.Get(obname)  # open an existing mesh_object
		else:
			obname = 'fa_%s' %self.layer  # create object name from layer name
			obname = obname[:MAX_NAMELENGTH]
			me = Mesh.New(obname)		   # create a new mesh
			ob = SCENE.objects.new(me) # create a new mesh_object
			activObjectName = ob.name
			activObjectLayer = self.layer
			#print ('deb:except. new face.ob+mesh:"%s" created!' %ob.name) #---------------------

		me = Mesh.Get(ob.name)	 # open objects mesh data
		faces, edges = [], []
		n = len(me.verts)
		if len(self.points) == 4:
			faces = [[0+n,1+n,2+n,3+n]]
		elif len(self.points) == 3:
			faces = [[0+n,1+n,2+n]]
		elif len(self.points) == 2:
			edges = [[0+n,1+n]]

		me.verts.extend(points) # add vertices to mesh
		if faces: me.faces.extend(faces)	   # add faces to the mesh
		if edges: me.edges.extend(edges)	   # add faces to the mesh
		if settings.var['vGroup_on']:
			# entities with the same color build one vertexGroup for easier material assignment ---------------------
			ob.link(me) # link mesh to that object
			vG_name = 'color_%s' %self.color_index
			if edges: faces = edges
			replace = Blender.Mesh.AssignModes.ADD  #or .AssignModes.REPLACE or ADD
			try:
				me.assignVertsToGroup(vG_name,  faces[0], 1.0, replace)
				#print 'deb: existed vGroup:', vG_name #---------------------
			except:
				me.addVertGroup(vG_name)
				me.assignVertsToGroup(vG_name,  faces[0], 1.0, replace)
				#print 'deb: create new vGroup:', vG_name #--------------------

		#print 'deb:draw:face.ob OUT activObjectName: ', activObjectName #---------------------
		return ob


#---------------------------------------------------------------------------------------
# type to object maping (sorted-dictionary for f_obiectify ONLY!, format={'key':Class} )
type_map = {
	'layer':Layer,
	'block_record':BlockRecord,
	'block':Block,
	'insert':Insert,
	'point':Point,
	'3dface':Face,
	'line':Line,
#	'mline':MLine,
	'polyline':Polyline,
	'lwpolyline':LWpolyline,
#	'region':Region,
	'trace':Solid,
	'solid':Solid,
	'text':Text,
	'mtext':Mtext,
	'circle':Circle,
	'ellipse':Ellipse,
	'arc':Arc
}



def objectify(data):  #-----------------------------------------------------------------
	"""Expects a section type object's data as input.

	Maps object data to the correct object type.
	"""
	#print 'deb:objectify start %%%%%%%%%%%' #---------------
	objects = [] # colector for finished objects
	known_types = type_map.keys() # so we don't have to call foo.keys() every iteration
	curves_on = GUI_A['curves_on'].val
	index = 0
	while index < len(data):
		item = data[index]
		#print 'deb:objectify item: \n', item #------------
		if type(item) != list and item.type == 'table':
			item.data = objectify(item.data) # tables have sub-objects
			objects.append(item)
		elif type(item) != list and item.type == 'polyline': #remi todo-----------
			#print 'deb:gosub Polyline\n' #-------------
			pline = Polyline(item)
			while 1:
				index += 1
				item = data[index]
				if item.type == 'vertex':
					#print 'deb:objectify gosub Vertex--------' #-------------
					v = Vertex(item)
					if pline.spline: # Bspline-curve
						# then for Blender-mesh  filter only additional_vertices
						# OR
						# then for Blender-curve filter only spline_control_vertices
						if (v.spline and not curves_on) or (curves_on and v.splin2): #correct for real NURBS-import
						#if (v.spline and not curves_on) or (curves_on and not v.splin2): #fake for Bezier-emulation of NURBS-import
							pline.points.append(v)
					elif pline.curved:  # Bezier-curve
						# then for Blender-mesh filter only curve_additional_vertices
						# OR
						# then for Blender-curve filter curve_control_vertices
						if not curves_on or (curves_on and not v.curved):
							pline.points.append(v)
					else:
						pline.points.append(v)
				elif item.type == 'seqend':
					#print 'deb:objectify it is seqEND ---------\n' #-------------
					break
				else:
					print "Error: non-vertex found before seqend!"
					index -= 1  #so go back one step
					break
			objects.append(pline)
		elif type(item) != list and item.type in known_types:
			# proccess the object and append the resulting object
			objects.append(type_map[item.type](item))
		else:
			# we will just let the data pass un-harrased
			objects.append(item)
		index += 1
	#print 'deb:objectify objects:\n', objects #------------
	#print 'deb:objectify END %%%%%%%%' #------------
	return objects



class MatColors:  #-----------------------------------------------------------------
	"""A smart container for dxf-color based materials.

	This class is a wrapper around a dictionary mapping dxf-color indicies to materials.
	When called with a color index it returns a material corrisponding to that index.
	Behind the scenes it checks if that index is in its keys, and if not it creates
	a new material.  It then adds the new index:material pair to its dict and returns
	the material.
	"""

	def __init__(self, map):
		"""Expects a map - a dictionary mapping layer names to layers.
		"""
		self.map = map  # a dictionary of layername:layer
		self.colMaterials = {}  # a dictionary of color_index:blender_material
		#print 'deb:init_MatColors argument.map: ', map #------------------


	def __call__(self, color=None):
		"""Return the material associated with color.

		If a layer name is provided, the color of that layer is used.
		"""
		if not color:
			color = 0
		if type(color) == str:
			#print 'deb:color is string:--------------: ', color #--todo---bug with ARC from ARC-T0.DXF layer="T-3DARC-1"-----
			try:
				color = self.map[color].color
				#print 'deb:color=self.map[color].color:', color #------------------
			except KeyError:
				layer = Layer(name=color, color=0, frozen=False)
				self.map[color] = layer
				color = 0
		color = abs(color)
		if color not in self.colMaterials.keys():
			self.add(color)
		return self.colMaterials[color]


	def add(self, color):
		"""Create a new material 'ColorNr-N' using the provided color index-N.
		"""
		global color_map
		mat = Material.New('ColorNr-%s' %color)
		mat.setRGBCol(color_map[color])
		try:
			mat.setMode('Shadeless', 'Wire') #work-around for 2.45rc-bug
		except: pass
		self.colMaterials[color] = mat



class MatLayers:  #-----------------------------------------------------------------
	"""A smart container for dxf-layer based materials.

	This class is a wrapper around a dictionary mapping dxf-layer names to materials.
	When called with a layer name it returns a material corrisponding to that.
	Behind the scenes it checks if that layername is in its keys, and if not it creates
	a new material.  It then adds the new layername:material pair to its dict and returns
	the material.
	"""

	def __init__(self, map):
		"""Expects a map - a dictionary mapping layer names to layers.
		"""
		self.map = map  # a dictionary of layername:layer
		self.layMaterials = {}  # a dictionary of layer_name:blender_material
		#print 'deb:init_MatLayers argument.map: ', map #------------------


	def __call__(self, layername=None):
		"""Return the material associated with dxf-layer.

		If a dxf-layername is not provided, create a new material
		"""
		if layername not in self.layMaterials.keys():
			self.add(layername)
		return self.layMaterials[layername]


	def add(self, layername):
		"""Create a new material 'layername'.
		"""
		try: mat = Material.Get('Lay-%s' %layername)
		except: mat = Material.New('Lay-%s' %layername)
		#print 'deb:MatLayers material: ', mat  #----------
		#print 'deb:MatLayers getMode: ', mat.getMode()  #----------
		global layersmap
		color = layersmap[layername].color
		#print 'deb:MatLayers layer_color: ', color  #-----------
		global color_map
		mat.setRGBCol(color_map[color])
		try:
			mat.setMode('Shadeless', 'Wire') #work-around for 2.45rc-bug
		except: pass
		self.layMaterials[layername] = mat




class Blocks:  #-----------------------------------------------------------------
	"""A smart container for blocks.

	This class is a wrapper around a dictionary mapping block names to Blender data blocks.
	When called with a name string it returns a block corresponding to that name.
	Behind the scenes it checks if that name is in its keys, and if not it creates
	a new data block.  It then adds the new name:block_data pair to its dict and returns
	the block.
	"""

	def __init__(self, blocksmap, settings):
		"""Expects a dictionary mapping block_name:block_data.
		"""
		self.blocksmap = blocksmap     #a dictionary mapping block_name:block_data
		self.settings = settings
		self.blocks = {}   #container for blocks


	def __call__(self, name=None):
		"""Return the data block associated with that block_name.

		If that name is not in its keys, it creates a new data block.
		If no name is provided return entire self.blocks container.
		"""
		if not name:
			return self.blocks
		if name not in self.blocks.keys():
			self.addBlock(name)
		return self.blocks[name]


	def addBlock(self, name):
		"""Create a new 'block group' for the block name.
		"""
		block_def = Group.New('bl_%s' %name)  # groupObject contains definition of BLOCK
		block = self.blocksmap[name]
		self.settings.write("\nDrawing block:\'%s\' ..." % name)
		drawEntities(block.entities, self.settings, block_def)
		self.settings.write("Drawing block:\'%s\' done!" %name)
		self.blocks[name] = block_def





class Settings:  #-----------------------------------------------------------------
	"""A container for all the import settings and objects used by the draw functions.

	This is like a collection of globally accessable persistant properties and functions.
	"""
	# Optimization constants
	MIN = 0
	MID = 1
	PRO = 2
	MAX = 3

	def __init__(self, keywords, drawTypes):
		"""initialize all the important settings used by the draw functions.
		"""
		self.obj_number = 1 #global object_number for progress_bar

		self.var = dict(keywords)	   #a dictionary of (key_variable:Value) control parameter
		self.drawTypes = dict(drawTypes) #a dictionary of (entity_type:True/False) = import on/off for this entity_type

		self.var['colorFilter_on'] = False   #deb:remi------------
		self.acceptedColors = [0,2,3,4,5,6,7,8,9,
							   10 ]

		self.var['layerFilter_on'] = False   #deb:remi------------
		self.acceptedLayers = ['3',
						   '0'
						  ]

		self.var['blockFilter_on'] = False   #deb:remi------------
		self.acceptedBlocks = ['BOX01',
						   'BOX02'
						  ]


	def update(self, keywords, drawTypes):
		"""update all the important settings used by the draw functions.
		"""

		for k, v in keywords.iteritems():
			self.var[k] = v
			#print 'deb:settings_update var %s= %s' %(k, self.var[k]) #--------------
		for t, v in drawTypes.iteritems():
			self.drawTypes[t] = v
			#print 'deb:settings_update drawType %s= %s' %(t, self.drawTypes[t]) #--------------

		#print 'deb:self.drawTypes', self.drawTypes #---------------


	def validate(self, drawing):
		"""Given the drawing, build dictionaries of Layers, Colors and Blocks.
		"""

		#de: 	paßt die distance parameter an globalScale
		if self.var['g_scale'] != 1:
			self.var['dist_min']  = self.var['dist_min'] / self.var['g_scale']
			self.var['thick_min'] = self.var['thick_min'] / self.var['g_scale']
			self.var['width_min'] = self.var['width_min'] / self.var['g_scale']

		# First sort out all the section_items
		sections = dict([(item.name, item) for item in drawing.data])

		# The section:header may be omited
		if 'header' in sections.keys():
			self.write("Found section:header!")
		else:
			self.write("File contains no section:header!")

		# The section:tables may be partialy or completely missing.
		self.layersTable = False
		self.colMaterials = MatColors({})
		self.layMaterials = MatLayers({})
		if 'tables' in sections.keys():
			self.write("Found section:tables!")
			# First sort out all the tables
			tables = dict([(item.name, item) for item in sections["tables"].data])
			if 'layer' in tables.keys():
				self.write("Found table:layers!")
				self.layersTable = True
				# Read the layers table and get the layer colors
				global layersmap
				layersmap = getLayersmap(drawing)
				self.colMaterials = MatColors(layersmap)
				self.layMaterials = MatLayers(layersmap)
			else:
				self.write("File contains no table:layers!")
		else:
			self.write("File contains no section:tables!")
			self.write("File contains no table:layers!")

		# The section:blocks may be omited
		if 'blocks' in sections.keys():
			self.write("Found section:blocks!")
			# Read the block definitions and build our block object
			if self.drawTypes['insert']:  #if drawing of type 'Insert' activated
				blocksmap, self.obj_number = getBlocksmap(drawing)  #Build a dictionary of blockname:block_data pairs
				self.blocks = Blocks(blocksmap, self) # initiates container for blocks_data

			#print 'deb: self.obj_number', self.obj_number #----------
		else:
			self.write("File contains no section:blocks!")
			self.drawTypes['insert'] = False

		# The section:entities
		if 'entities' in sections.keys():
			self.write("Found section:entities!")

			self.obj_number += len(drawing.entities.data)
			#print 'deb: self.obj_number', self.obj_number #----------
			self.obj_number = 1.0 / self.obj_number


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


	def layer_isOff(self, name):
		"""Given a layer name, and return its visible status.
		"""
		# colors are negative if layer is off
		try:
			#print 'deb:layer_isOff self.colMaterials.map:\n', self.colMaterials.map #--------------
			layer = self.colMaterials.map[name]
		except KeyError: return False
		if layer.color < 0: return True
		#print 'deb:layer_isOff: layer is ON' #---------------
		return False


	def layer_isFrozen(self, name):
		"""Given a layer name, and return its frozen status.
		"""
		# colors are negative if layer is off
		try:
			#print 'deb:layer_isFrozen self.colMaterials.map:\n', self.colMaterials.map #---------------
			layer = self.colMaterials.map[name]
		except KeyError: return False
		if layer.frozen: return True
		#print 'deb:layer_isFrozen: layer is not FROZEN' #---------------
		return False




def main():  #---------------#############################-----------
	#print 'deb:filename:', filename #--------------
	global SCENE
	editmode = Window.EditMode()	# are we in edit mode?  If so ...
	if editmode:
		Window.EditMode(0) # leave edit mode before

	#SCENE = Scene.GetCurrent()
	SCENE = bpy.data.scenes.active
	SCENE.objects.selected = [] # deselect all

	global cur_COUNTER  #counter for progress_bar
	cur_COUNTER = 0

	try:
		print "Getting settings..."
		global GUI_A, GUI_B
		if GUI_A['g_scale_on'].val:
			GUI_A['g_scale'].val = 10.0 ** int(GUI_A['g_scale_as'].val)
		else:
			GUI_A['g_scale'].val = 1.0

		keywords = {}
		drawTypes = {}
		for k, v in GUI_A.iteritems():
			keywords[k] = v.val
		for k, v in GUI_B.iteritems():
			drawTypes[k] = v.val
		#print 'deb:startUInew keywords: ', keywords #--------------
		#print 'deb:startUInew drawTypes: ', drawTypes #--------------

		# The settings object controls how dxf entities are drawn
		settings.update(keywords, drawTypes)
		#print 'deb:settings.var:\n', settings.var  #-----------------------

		if not settings:
			#Draw.PupMenu('DXF importer:  EXIT!%t')
			print '\nDXF Import: terminated by user!'
			return None

		dxfFile = dxfFileName.val
		#print 'deb: dxfFile file: ', dxfFile #----------------------
		if dxfFile.lower().endswith('.dxf') and sys.exists(dxfFile):
			Window.WaitCursor(True)   # Let the user know we are thinking
			print 'start	reading DXF file: %s.' % dxfFile
			time1 = Blender.sys.time()  #time marker1
			drawing = readDXF(dxfFile, objectify)
			print 'finished reading DXF file in %.4f sec.' % (Blender.sys.time()-time1)
			Window.WaitCursor(False)
		else:
			if UI_MODE: Draw.PupMenu('DXF importer:  EXIT----------!%t| no valid DXF-file selected!')
			print "DXF importer: error, no DXF-file selected. Abort!"
			return None

		settings.validate(drawing)

		Window.WaitCursor(True)   # Let the user know we are thinking
		settings.write("\n\nDrawing entities...")

		# Draw all the know entity types in the current scene
		global oblist
		oblist = []  # a list of all created AND linked objects for final f_globalScale
		time2 = Blender.sys.time()  #time marker2

		drawEntities(drawing.entities, settings)

		#print 'deb:drawEntities after: oblist:', oblist #-----------------------
		if oblist and settings.var['g_scale'] != 1:
			globalScale(oblist, settings.var['g_scale'])

		# Set the visable layers
		SCENE.setLayers([i+1 for i in range(18)])
		Blender.Redraw(-1)
		Window.WaitCursor(False)
		settings.write("Import DXF to Blender:  *** DONE ***")
		settings.progress(1.0/settings.obj_number, 'DXF import done!')
		print 'DXF importer: done in %.4f sec.' % (Blender.sys.time()-time2)
		if UI_MODE: Draw.PupMenu('DXF importer:	   Done!|finished in %.4f sec.' % (Blender.sys.time()-time2))

	finally:
		# restore state even if things didn't work
		Window.WaitCursor(False)
		if editmode: Window.EditMode(1) # and put things back how we fond them



def getOCS(az):  #-----------------------------------------------------------------
	"""An implimentation of the Arbitrary Axis Algorithm.
	"""
	#decide if we need to transform our coords
	if az[0] == 0 and az[1] == 0:
		return False
	#elif abs(az[0]) < 0.0001 and abs(az[1]) < 0.0001:
	#   return False

	az = Mathutils.Vector(az)

	cap = 0.015625 # square polar cap value (1/64.0)
	if abs(az.x) < cap and abs(az.y) < cap:
		ax = Mathutils.CrossVecs(WORLDY, az)
	else:
		ax = Mathutils.CrossVecs(WORLDZ, az)
	ax = ax.normalize()
	ay = Mathutils.CrossVecs(az, ax)
	ay = ay.normalize()
	return ax, ay, az



def transform(normal, rotation, obj):  #--------------------------------------------
	"""Use the calculated ocs to determine the objects location/orientation in space.

	Quote from dxf docs:
		The elevation value stored with an entity and output in DXF files is a sum
	of the Z-coordinate difference between the UCS XY plane and the OCS XY
	plane, and the elevation value that the user specified at the time the entity
	was drawn.
	"""
	ma = Mathutils.Matrix([1,0,0],[0,1,0],[0,0,1])
	o = Mathutils.Vector(obj.loc)
	ocs = getOCS(normal)
	if ocs:
		ma = Mathutils.Matrix(ocs[0], ocs[1], ocs[2])
		o = ma.invert() * o
		ma = Mathutils.Matrix(ocs[0], ocs[1], ocs[2])

	if rotation != 0:
		g = radians(-rotation)
		rmat = Mathutils.Matrix([cos(g), -sin(g), 0], [sin(g), cos(g), 0], [0, 0, 1])
		ma = rmat * ma

	obj.setMatrix(ma)
	obj.loc = o
	#print 'deb:new obj.matrix:\n', obj.getMatrix()   #--------------------



def rotXY_Vec(rotation, vec):  #----------------------------------------------------
	"""Rotate vector vec in XY-plane. vec must be in radians
	"""
	if rotation != 0:
		o = Mathutils.Vector(vec)
		g = radians(-rotation)
		vec = o * Mathutils.Matrix([cos(g), -sin(g), 0], [sin(g), cos(g), 0], [0, 0, 1])
	return vec



def getLayersmap(drawing):  #------------------------------------------------------
	"""Build a dictionary of layername:layer pairs for the given drawing.
	"""
	tables = drawing.tables
	for table in tables.data:
		if table.name == 'layer':
			layers = table
			break
	layersmap = {}
	for item in layers.data:
		if type(item) != list and item.type == 'layer':
			layersmap[item.name] = item
	return layersmap



def getBlocksmap(drawing):  #--------------------------------------------------------
	"""Build a dictionary of blockname:block_data pairs for the given drawing.
	"""
	blocksmap = {}
	obj_number = 0
	for item in drawing.blocks.data:
		#print 'deb:getBlocksmap item=' ,item #--------
		#print 'deb:getBlocksmap item.entities=' ,item.entities #--------
		#print 'deb:getBlocksmap item.entities.data=' ,item.entities.data #--------
		if type(item) != list and item.type == 'block':
			obj_number += len(item.entities.data)
			try:
				blocksmap[item.name] = item
			except KeyError:
				# annon block
				print 'Cannot map "%s" - "%s" as Block!' %(item.name, item)
	return blocksmap, obj_number





def drawEntities(entities, settings, block_def=None):  #----------------------------------------
	"""Draw every kind of thing in the entity list.

	If provided 'block_def': the entities are to be added to the Blender 'group'.
	"""
	for _type in type_map.keys():
		#print 'deb:drawEntities_type:', _type #------------------
		# for each known type get a list of that type and call the associated draw function
		drawer(_type, entities.get_type(_type), settings, block_def)


def drawer(_type, entities, settings, block_def):  #------------------------------------------
	"""Call with a list of entities and a settings object to generate Blender geometry.

	If 'block_def': the entities are to be added to the Blender 'group'.
	"""
	if entities:
		# Break out early if settings says we aren't drawing the current dxf-type
		global cur_COUNTER  #counter for progress_bar
		group = None
		#print 'deb:drawer.check:_type: ', _type  #--------------------
		if _type == '3dface':_type = 'face' # hack, while python_variable_name can not beginn with a nummber
		if not settings.drawTypes[_type] or _type == 'block_record':
			message = 'Skipping dxf\'%ss\' entities' %_type
			settings.write(message, True)
			cur_COUNTER += len(entities)
			settings.progress(cur_COUNTER, message)
			return
		#print 'deb:drawer.todo:_type:', _type  #-----------------------

		len_temp = len(entities)
		# filtering only model-space enitities (no paper-space enitities)
		entities = [entity for entity in entities if entity.space == 0]

		# filtering only objects with color from acceptedColorsList
		if settings.var['colorFilter_on']:
			entities = [entity for entity in entities if entity.color in settings.acceptedColors]

		# filtering only objects on layers from acceptedLayersList
		if settings.var['layerFilter_on']:
#			entities = [entity for entity in entities if entity.layer[0] in ['M','3','0'] and not entity.layer.endswith('H')]
			entities = [entity for entity in entities if entity.layer in settings.acceptedLayers]

		# filtering only objects on not-frozen layers
		entities = [entity for entity in entities if not settings.layer_isFrozen(entity.layer)]

		global activObjectLayer, activObjectName
		activObjectLayer = ''
		activObjectName = ''

		message = "Drawing dxf\'%ss\'..." %_type
		cur_COUNTER += len_temp - len(entities)
		settings.write(message, False)
		settings.progress(cur_COUNTER, message)
		if len(entities) > 0.1 / settings.obj_number:
			show_progress = int(0.03 / settings.obj_number)
		else: show_progress = 0
		cur_temp = 0

		#print 'deb:drawer cur_COUNTER: ', cur_COUNTER  #-----------------------

		for entity in entities:   #----loop-------------------------------------
			settings.write('\b.', False)
			cur_COUNTER += 1
			if show_progress:
				cur_temp += 1
				if cur_temp == show_progress:
					settings.progress(cur_COUNTER, message)
					cur_temp = 0
					#print 'deb:drawer show_progress=',show_progress  #-----------------------
				
			# get the layer group (just to make things a little cleaner)
			if settings.var['group_bylayer_on'] and not block_def:
				group = getGroup('l:%s' % entity.layer[:MAX_NAMELENGTH-2])

			if _type == 'insert':	#---- INSERT and MINSERT=array ------------------------
				#print 'deb:insert entity.loc:', entity.loc #----------------
				columns = entity.columns[0]
				coldist = entity.columns[1]
				rows	= entity.rows[0]
				rowdist = entity.rows[1]
				deltaloc = [0,0,0]
				#print 'deb:insert columns, rows:', columns, rows   #-----------
				for col in xrange(columns):
					deltaloc[0] =  col * coldist
					for row in xrange(rows):
						deltaloc[1] =  row * rowdist
						#print 'deb:insert col=%s, row=%s,deltaloc=%s' %(col, row, deltaloc) #------
						ob = entity.draw(settings, deltaloc)   #-----draw BLOCK----------
						setObjectProperties(ob, group, entity, settings, block_def)
						if ob:
							if settings.var['optimization'] <= settings.MIN:
								if settings.var['g_scale'] != 1: globalScaleOne(ob, True, settings.var['g_scale'])
								settings.redraw()
							else: oblist.append((ob, True))
					
			else:   #---draw entities except BLOCKs/INSERTs---------------------
				alt_obname = activObjectName
				ob = entity.draw(settings)
				if ob and ob.name != alt_obname:
					setObjectProperties(ob, group, entity, settings, block_def)
					if settings.var['optimization'] <= settings.MIN:
						if settings.var['g_scale'] != 1: globalScaleOne(ob, False, settings.var['g_scale'])
						settings.redraw()
					else: oblist.append((ob, False))

		#print 'deb:Finished drawing:', entities[0].type   #------------------------
		message = "\nDrawing dxf\'%ss\' done!" % _type
		settings.write(message, True)



def globalScale(oblist, SCALE):  #---------------------------------------------------------
	"""Global_scale for list of all imported objects.

	oblist is a list of pairs (ob, insertFlag), where insertFlag=True/False
	"""
	#print 'deb:globalScale.oblist: ---------%\n', oblist #---------------------
	for l in oblist:
		ob, insertFlag  = l[0], l[1]
		globalScaleOne(ob, insertFlag, SCALE)


def globalScaleOne(ob, insertFlag, SCALE):  #---------------------------------------------------------
	"""Global_scale imported object.
	"""
	#print 'deb:globalScaleOne  ob: ', ob #---------------------
	SCALE_MAT= Mathutils.Matrix([SCALE,0,0,0],[0,SCALE,0,0],[0,0,SCALE,0],[0,0,0,1])
	if insertFlag:  # by BLOCKs/INSERTs only insert-point must be scaled------------
		ob.loc = Mathutils.Vector(ob.loc) * SCALE_MAT
	else:   # entire scaling for all other imported objects ------------
		ob.setMatrix(ob.matrixWorld*SCALE_MAT)



def setObjectProperties(ob, group, entity, settings, block_def):  #-----------------------
	"""Link object to scene.
	"""

	if not ob:  #remi--todo-----------------------
		message = "\nObject \'%s\' not found!" %entity
		settings.write(message)
		return

	if group:
		setGroup(group, ob)  # if object belongs to group

	if block_def:   # if object belongs to BLOCK_def - Move it to layer nr19
		setGroup(block_def, ob)
		#print 'deb:setObjectProperties  \'%s\' set to block_def_group!' %ob.name #---------
		ob.layers = [19]
	else:
		#ob.layers = [i+1 for i in xrange(20)] #remi--todo------------
		ob.layers = [settings.var['target_layer']]

	# Set material for any objects except empties
	if ob.type != 'Empty':
		setMaterial_from(entity, ob, settings, block_def)

	# Set the visibility
	if settings.layer_isOff(entity.layer):
		#ob.layers = [20]  #remi--todo-------------
		ob.restrictDisplay = True
		ob.restrictRender = True

	#print 'deb:\n---------linking Object %s!' %ob.name #----------



def getGroup(name):  #-----------------------------------------------------------------
	"""Returns a Blender group-object.
	"""
	try:
		group = Group.Get(name)
	except: # What is the exception?
		group = Group.New(name)
	return group


def setGroup(group, ob):  #------------------------------------------------------------
	"""Assigns object to Blender group.
	"""
	try:
		group.objects.link(ob)
	except:
		group.objects.append(ob)  #remi?---------------



def setMaterial_from(entity, ob, settings, block_def):  #------------------------------------------------
	""" Set Blender-material for the object controled by item.

	Set Blender-material for the object
	 - controlled by settings.var['material_from']
	"""
	if settings.var['material_from'] == 1: # 1= material from color
		if entity.color_index == BYLAYER:
			mat = settings.colMaterials(entity.layer)
		else:
			mat = settings.colMaterials(entity.color_index)
	elif settings.var['material_from'] == 2: # 2= material from layer
		mat = settings.layMaterials(entity.layer)
#	elif settings.var['material_from'] == 3: # 3= material from layer+color
#		mat = settings.layMaterials(entity.layer)
#		color = entity.color_index
#		if type(color) == int:
#			mat.setRGBCol(color_map[abs(color)])
#	elif settings.var['material_from'] == 4: # 4= material from block
#	elif settings.var['material_from'] == 5: # 5= material from INI-file
	else:                       # set neutral material
		try:
			mat = Material.Get('dxf-neutral')
		except:
			mat = Material.New('dxf-neutral')
			try:
				mat.setMode('Shadeless', 'Wire') #work-around for 2.45rc-bug
			except: pass
	try:
		#print 'deb:material mat:', mat #-----------
		ob.setMaterials([mat])  #assigns Blender-material to object
	except ValueError:
		settings.write("material error - \'%s\'!" %mat)
	ob.colbits = 0x01 # Set OB materials.



def drawBulge(p1, p2, ARC_RESOLUTION=120):   #-------------------------------------------------
	"""return the center, radius, start angle, and end angle given two points.

	Needs to take into account bulge sign.
	negative = clockwise
	positive = counter-clockwise

	to find center given two points, and arc angle
	calculate radius
		Cord = sqrt(start^2 + end^2)
		S = (bulge*Cord)/2
		radius = ((Cord/2)^2+S^2)/2*S
	angle of arc = 4*atan( bulge )
	angle from p1 to center is (180-angle)/2
	get vector pointing from p1 to p2 (p2 - p1)
	normalize it and multiply by radius
	rotate around p1 by angle to center, point to center.
	start angle = angle between (center - p1) and worldX
	end angle = angle between (center - p2) and worldX
	"""

	bulge = p1.bulge
	p2 = Mathutils.Vector(p2.loc)
	p1 = Mathutils.Vector(p1.loc)
	cord = p2 - p1 # vector from p1 to p2
	clength = cord.length
	s = (bulge * clength)/2.0 # sagitta (height)
	radius = abs(((clength/2.0)**2.0 + s**2.0)/(2.0*s)) # magic formula
	angle = (degrees(4.0*atan(bulge))) # theta (included angle)

	pieces = int(abs(angle)/(360.0/ARC_RESOLUTION)) # set a fixed step of ARC_RESOLUTION
	if pieces < 3: pieces = 3  #bulge under arc_resolution
	#if pieces < 3: points = [p1, p2] ;return points
	step = angle/pieces  # set step so pieces * step = degrees in arc
	delta = (180.0 - abs(angle))/2.0 # the angle from cord to center
	if bulge < 0: delta = -delta
	radial = cord.normalize() * radius # a radius length vector aligned with cord
	rmat = Mathutils.RotationMatrix(-delta, 3, 'Z')
	center = p1 + (rmat * radial) # rotate radial by delta degrees, then add to p1 to find center
	#length = radians(abs(angle)) * radius
	#print 'deb:drawBulge:\n angle, delta: ', angle, delta  #----------------
	#print 'deb:center, radius: ', center, radius  #----------------------

	startpoint = p1 - center
	#endpoint = p2 - center
	stepmatrix = Mathutils.RotationMatrix(-step, 3, "Z")
	points = [startpoint]
	point = Mathutils.Vector(startpoint)
	for i in xrange(int(pieces)-1):  #fast (but not so acurate as: vector * RotMatrix(step * i)
		point = stepmatrix * point
		points.append(point)
	points = [[point[0]+center[0], point[1]+center[1], point[2]+center[2]] for point in points]
	return points



def drawArc(center, radius, start, end, ARC_RESOLUTION=120):  #-----------------------------------------
	"""Draw a mesh arc with the given parameters.
	"""
	# center is currently set by object
	# if start > end: start = start - 360
	if end > 360: end = end%360.0

	startmatrix = Mathutils.RotationMatrix(-start, 3, "Z")
	startpoint = startmatrix * Mathutils.Vector(radius, 0, 0)
	endmatrix = Mathutils.RotationMatrix(-end, 3, "Z")
	endpoint = endmatrix * Mathutils.Vector(radius, 0, 0)

	if end < start: end +=360.0
	angle = end - start
	length = radians(angle) * radius

	#if radius < MIN_DIST * 10: # if circumfence is too small
	pieces = int(abs(angle)/(360.0/ARC_RESOLUTION)) # set a fixed step of ARC_RESOLUTION
	if pieces < 3: pieces = 3  #cambo-----
	step = angle/pieces # set step so pieces * step = degrees in arc

	stepmatrix = Mathutils.RotationMatrix(-step, 3, "Z")
	points = [startpoint]
	point = Mathutils.Vector(startpoint)
	for i in xrange(int(pieces)):
		point = stepmatrix * point
		points.append(point)
	points.append(endpoint)

	if center:
		points = [[point[0]+center[0], point[1]+center[1], point[2]+center[2]] for point in points]
	edges = [[num, num+1] for num in xrange(len(points)-1)]

	return points, edges



def drawCurveCircle(circle):  #--- no more used --------------------------------------------
	"""Given a dxf circle object return a blender circle object using curves.
	"""
	c = Curve.New('circle')	# create new  curve data
	center = circle.loc
	radius = circle.radius

	p1 = (0, -radius, 0)
	p2 = (radius, 0, 0)
	p3 = (0, radius, 0)
	p4 = (-radius, 0, 0)

	p1 = BezTriple.New(p1)
	p2 = BezTriple.New(p2)
	p3 = BezTriple.New(p3)
	p4 = BezTriple.New(p4)

	curve = c.appendNurb(p1)
	curve.append(p2)
	curve.append(p3)
	curve.append(p4)
	for point in curve:
		point.handleTypes = [AUTO, AUTO]
	curve.flagU = 1 # Set curve cyclic
	c.update()

	ob = Object.New('Curve', 'circle')  # make curve object
	return ob


def drawCurveArc(self):  #---- only for ELLIPSE -------------------------------------------------------------
	"""Given a dxf ELLIPSE object return a blender_curve.
	"""
	center = self.loc
	radius = self.radius
	start = self.start_angle
	end = self.end_angle

	if start > end:
		start = start - 360.0
	startmatrix = Mathutils.RotationMatrix(start, 3, "Z")
	startpoint = startmatrix * Mathutils.Vector((radius, 0, 0))
	endmatrix = Mathutils.RotationMatrix(end, 3, "Z")
	endpoint = endmatrix * Mathutils.Vector((radius, 0, 0))
	# Note: handles must be tangent to arc and of correct length...

	a = Curve.New('arc')			 # create new  curve data

	p1 = (0, -radius, 0)
	p2 = (radius, 0, 0)
	p3 = (0, radius, 0)
	p4 = (-radius, 0, 0)

	p1 = BezTriple.New(p1)
	p2 = BezTriple.New(p2)
	p3 = BezTriple.New(p3)
	p4 = BezTriple.New(p4)

	curve = a.appendNurb(p1)
	curve.append(p2)
	curve.append(p3)
	curve.append(p4)
	for point in curve:
		point.handleTypes = [AUTO, AUTO]
	curve.flagU = 1 # Set curve cyclic
	a.update()

	ob = Object.New('Curve', 'arc') # make curve object
	return ob




# GUI STUFF -----#################################################-----------------
from Blender.BGL import *

EVENT_NONE = 1
EVENT_START = 2
EVENT_REDRAW = 3
EVENT_LOAD_INI = 4
EVENT_SAVE_INI = 5
EVENT_PRESET = 6
EVENT_CHOOSE_INI = 7
EVENT_CHOOSE_DXF = 8
EVENT_HELP = 9
EVENT_CONFIG = 10
EVENT_PRESET2D = 11
EVENT_EXIT = 100
GUI_EVENT = EVENT_NONE

GUI_A = {}  # GUI-buttons dictionary for parameter
GUI_B = {}  # GUI-buttons dictionary for drawingTypes

# settings default, initialize ------------------------

points_as_menu  = "convert to: %t|empty %x1|mesh.vertex %x2|thin sphere %x3|thin box %x4"
lines_as_menu   = "convert to: %t|*edge %x1|mesh %x2|*thin cylinder %x3|*thin box %x4"
mlines_as_menu  = "convert to: %t|*edge %x1|*mesh %x2|*thin cylinder %x3|*thin box %x4"
plines_as_menu  = "convert to: %t|*edge %x1|mesh %x2|*thin cylinder %x3|*thin box %x4"
plines3_as_menu = "convert to: %t|*edge %x1|mesh %x2|*thin cylinder %x3|*thin box %x4"
plmesh_as_menu  = "convert to: %t|mesh %x1"
solids_as_menu  = "convert to: %t|mesh %x1"
blocks_as_menu  = "convert to: %t|dupl.group %x1|*real.group %x2|*exploded %x3"
texts_as_menu   = "convert to: %t|text %x1|*mesh %x2"
material_from_menu= "material from: %t|COLOR %x1|LAYER %x2|*LAYER+COLOR %x3|*BLOCK %x4|*XDATA %x5|*INI-File %x6"
g_scale_list	= "scale factor: %t|x 1000 %x3|x 100 %x2|x 10 %x1|x 1 %x0|x 0.1 %x-1|x 0.01 %x-2|x 0.001 %x-3|x 0.0001 %x-4|x 0.00001 %x-5"

dxfFileName = Draw.Create("")
iniFileName = Draw.Create(INIFILE_DEFAULT_NAME)
config_UI = Draw.Create(0)   #switch_on/off extended config_UI

keywords_org = {
	'curves_on' : 0,
	'optimization': 2,
	'one_mesh_on': 1,
	'vGroup_on' : 1,
	'dummy_on' : 0,
	'target_layer' : TARGET_LAYER,
	'group_bylayer_on' : GROUP_BYLAYER,
	'g_scale'   : float(G_SCALE),
	'g_scale_as': int(log10(G_SCALE)), #   0,
	'g_scale_on': 1,
	'thick_on'  : 1,
	'thick_min' : float(MIN_THICK),
	'thick_force': 0,
	'width_on'  : 1,
	'width_min' : float(MIN_WIDTH),
	'width_force': 0,
	'dist_on'   : 1,
	'dist_min'  : float(MIN_DIST),
	'dist_force': 0,
	'material_on': 1,
	'material_from': 2,
	'pl_3d'	 : 1,
	'arc_res'   : ARC_RESOLUTION,
	'thin_res'  : THIN_RESOLUTION,
	'angle_cut' : ANGLECUT_LIMIT,
	'pl_section_on': 1,
	'points_as' : 2,
	'lines_as'  : 2,
	'mlines_as' : 2,
	'plines_as' : 2,
	'plines3_as': 2,
	'plmesh_as' : 1,
	'solids_as' : 1,
	'blocks_as' : 1,
	'texts_as'  : 1
	}

drawTypes_org = {
	'point' : 1,
	'line'  : 1,
	'arc'   : 1,
	'circle': 1,
	'ellipse': 0,
	'mline' : 0,
	'polyline': 1,
	'plmesh': 1,
	'pline3': 1,
	'lwpolyline': 1,
	'text'  : 1,
	'mtext' : 0,
	'block' : 1,
	'insert': 1,
	'face'  : 1,
	'solid' : 1,
	'trace' : 1
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
# initialize settings-object controls how dxf entities are drawn
settings = Settings(keywords_org, drawTypes_org)



def saveConfig():  #remi--todo-----------------------------------------------
	"""Save settings/config/materials from GUI to INI-file.

	Write all config data to INI-file.
	"""
	global iniFileName

	iniFile = iniFileName.val
	#print 'deb:saveConfig inifFile: ', inifFile #----------------------
	if iniFile.lower().endswith(INIFILE_EXTENSION):
		output = '[%s,%s]' %(GUI_A, GUI_B)
		if output =='None':
			Draw.PupMenu('DXF importer: INI-file:  Alert!%t|no config-data present to save!')
		else:
			#if BPyMessages.Warning_SaveOver(iniFile): #<- remi find it too abstarct
			if sys.exists(iniFile):
				f = file(iniFile, 'r');	header_str = f.readline(); f.close()
				if header_str.startswith(INIFILE_HEADER[0:12]):
				    if Draw.PupMenu('  OK ? %t|SAVE OVER: ' + '\'%s\'' %iniFile) == 1:
						save_ok = True
				elif Draw.PupMenu('  OK ? %t|SAVE OVER: ' + '\'%s\'' %iniFile +
					 '|Alert: this file has no valid ImportDXF-format| ! it may belong to another aplication !') == 1:
					save_ok = True
				else: save_ok = False
			else: save_ok = True

			if save_ok:
				try:
					f = file(iniFile, 'w')
					f.write(INIFILE_HEADER + '\n')
					f.write(output)
					f.close()
					Draw.PupMenu('DXF importer: INI-file: Done!%t|config-data saved in ' + '\'%s\'' %iniFile)
				except:
					Draw.PupMenu('DXF importer: INI-file: Error!%t|failure by writing to ' + '\'%s\'|no config-data saved!' %iniFile)

	else:
		Draw.PupMenu('DXF importer: INI-file:  Alert!%t|no valid name/extension for INI-file selected!')
		print "DXF importer: Alert!: no valid INI-file selected."
		if not iniFile:
			if dxfFileName.val.lower().endswith('.dxf'):
				iniFileName.val = dxfFileName.val[0:-4] + INIFILE_EXTENSION


def loadConfig():  #remi--todo-----------------------------------------------
	"""Load settings/config/materials from INI-file.

	Read material-assignements from config-file.
	"""
	#070724 buggy Window.FileSelector(loadConfigFile, 'Load config data from INI-file', inifilename)
	global iniFileName, GUI_A, GUI_B

	iniFile = iniFileName.val
	#print 'deb:loadConfig iniFile: ', iniFile #----------------------
	if iniFile.lower().endswith(INIFILE_EXTENSION) and sys.exists(iniFile):
		f = file(iniFile, 'r')
		header_str = f.readline()
		if not header_str.startswith(INIFILE_HEADER):
			f.close()
			Draw.PupMenu('DXF importer: INI-file:  Alert!%t|no valid header in INI-file: ' + '\'%s\'' %iniFile)
		else:
			data_str = f.read()
			f.close()
			#print 'deb:loadConfig data_str from %s: \n' %iniFile , data_str #-----------------------------------
			data = eval(data_str)
			for k, v in data[0].iteritems():
				try:
					GUI_A[k].val = v
				except:
					GUI_A[k] = Draw.Create(v)
			for k, v in data[1].iteritems():
				try:
					GUI_B[k].val = v
				except:
					GUI_B[k] = Draw.Create(v)
	else:
		Draw.PupMenu('DXF importer: INI-file:  Alert!%t|no valid INI-file selected!')
		print "DXF importer: Alert!: no valid INI-file selected."
		if not iniFileName:
			if dxfFileName.val.lower().endswith('.dxf'):
				iniFileName.val = dxfFileName.val[0:-4] + INIFILE_EXTENSION



def resetDefaultConfig():  #remi--todo-----------------------------------------------
	"""Resets settings/config/materials to defaults.

	"""
	global GUI_A, GUI_B
	#print 'deb:lresetDefaultConfig keywords_org: \n', keywords_org #-----------------------------------
	for k, v in keywords_org.iteritems():
		GUI_A[k].val = v
	for k, v in drawTypes_org.iteritems():
		GUI_B[k].val = v


def resetDefaultConfig_2D():  #remi--todo-----------------------------------------------
	"""Sets settings/config/materials to defaults 2D.

	"""
	resetDefaultConfig()
	global GUI_A, GUI_B
	drawTypes2d = {
		'point' : 1,
		'line'  : 1,
		'arc'   : 1,
		'circle': 1,
		'ellipse': 0,
		'mline' : 0,
		'polyline': 1,
		'plmesh': 0,
		'pline3': 0,
		'lwpolyline': 1,
		'text'  : 1,
		'mtext' : 0,
		'block' : 1,
		'insert': 1,
		'face'  : 0,
		'solid' : 1,
		'trace' : 1
		}

	keywords2d = {
		'curves_on' : 0,
		'one_mesh_on': 1,
		'vGroup_on' : 1,
		'dummy_on' : 0,
		'thick_on'  : 0,
		'thick_force': 0,
		'width_on'  : 0,
		'width_force': 0,
		'dist_on'   : 1,
		'dist_force': 0,
		'pl_3d'	 : 0,
		'pl_section_on': 1,
		'points_as' : 2,
		'lines_as'  : 2,
		'mlines_as' : 2,
		'plines_as' : 2,
		'solids_as' : 1,
		'blocks_as' : 1,
		'texts_as'  : 1
		}

	for k, v in keywords2d.iteritems():
		GUI_A[k].val = v
	for k, v in drawTypes2d.iteritems():
		GUI_B[k].val = v



def draw_UI():  #-----------------------------------------------------------------
	""" Draw startUI and setup Settings.
	"""
	global GUI_A, GUI_B #__version__
	global iniFileName, dxfFileName, config_UI

	# This is for easy layout changes
	but_1c = 140	#button 1.column width
	but_2c = 70  #button 2.column
	but_3c = 70  #button 3.column
	menu_margin = 10
	menu_w = but_1c + but_2c + but_3c  #menu width

	simlpe_menu_h = 110
	extend_menu_h = 370
	y = simlpe_menu_h         # y is menu upper.y
	if config_UI.val: y += extend_menu_h
	x = 10 #menu left.x
	but1c = x + menu_margin  #buttons 1.column position.x
	but2c = but1c + but_1c
	but3c = but2c + but_2c

	# Here starts menu -----------------------------------------------------
	#glClear(GL_COLOR_BUFFER_BIT)
	#glRasterPos2d(8, 125)

	colorbox(x, y+20, x+menu_w+menu_margin*2, menu_margin)
	Draw.Label("ImportDXF-3D v" + __version__, but1c, y, menu_w, 20)

	if config_UI.val:
		y -= 30
		Draw.BeginAlign()
		GUI_B['point'] = Draw.Toggle('POINT', EVENT_NONE, but1c, y, but_1c, 20, GUI_B['point'].val, "support dxf-POINT on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['points_as'] = Draw.Menu(points_as_menu, EVENT_NONE, but3c, y, but_3c, 20, GUI_A['points_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['line'] = Draw.Toggle('LINE.ARC.CIRCLE', EVENT_NONE, but1c, y, but_1c, 20, GUI_B['line'].val, "support dxf-LINE,ARC,CIRCLE,ELLIPSE on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['lines_as'] = Draw.Menu(lines_as_menu, EVENT_NONE, but3c, y, but_3c, 20, GUI_A['lines_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['mline'] = Draw.Toggle('*MLINE', EVENT_NONE, but1c, y, but_1c, 20, GUI_B['mline'].val, "(*wip)support dxf-MLINE on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['mlines_as'] = Draw.Menu(mlines_as_menu, EVENT_NONE, but3c, y, but_3c, 20, GUI_A['mlines_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['polyline'] = Draw.Toggle('2D-POLYLINE', EVENT_NONE, but1c, y, but_1c, 20, GUI_B['polyline'].val, "support dxf-2D-POLYLINE on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['plines_as'] = Draw.Menu(plines_as_menu,   EVENT_NONE, but3c, y, but_3c, 20, GUI_A['plines_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['pline3'] = Draw.Toggle('3D-POLYLINE', EVENT_NONE, but1c, y, but_1c, 20, GUI_B['pline3'].val, "support dxf-3D-POLYLINE on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['plines3_as'] = Draw.Menu(plines3_as_menu,   EVENT_NONE, but3c, y, but_3c, 20, GUI_A['plines3_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['plmesh'] = Draw.Toggle('POLYMESH/-FACE', EVENT_NONE, but1c, y, but_1c, 20, GUI_B['plmesh'].val, "support dxf-POLYMESH/POLYFACE on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['plmesh_as'] = Draw.Menu(plmesh_as_menu,   EVENT_NONE, but3c, y, but_3c, 20, GUI_A['plmesh_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['solid'] = Draw.Toggle('3DFACE.SOLID.TRACE', EVENT_NONE, but1c, y, but_1c, 20, GUI_B['solid'].val, "support dxf-3DFACE, SOLID and TRACE on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['solids_as'] = Draw.Menu(solids_as_menu, EVENT_NONE, but3c, y, but_3c, 20, GUI_A['solids_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['text'] = Draw.Toggle('TEXT', EVENT_NONE, but1c, y, but_1c/2, 20, GUI_B['text'].val, "support dxf-TEXT on/off")
		GUI_B['mtext'] = Draw.Toggle('*MTEXT', EVENT_NONE, but1c+but_1c/2, y, but_1c/2, 20, GUI_B['mtext'].val, "(*wip)support dxf-MTEXT on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['texts_as'] = Draw.Menu(texts_as_menu, EVENT_NONE, but3c, y, but_3c, 20, GUI_A['texts_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['block'] = Draw.Toggle('BLOCK / ARRAY', EVENT_NONE, but1c, y, but_1c, 20, GUI_B['block'].val, "support dxf-BLOCK and ARRAY on/off")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['blocks_as'] = Draw.Menu(blocks_as_menu, EVENT_NONE, but3c, y, but_3c, 20, GUI_A['blocks_as'].val, "select target Blender-object")
		Draw.EndAlign()


		y -= 20
		Draw.BeginAlign()
		GUI_A['material_from'] = Draw.Menu(material_from_menu,   EVENT_NONE, but1c, y, but_1c, 20, GUI_A['material_from'].val, "material assignment from?")
		Draw.Label('-->', but2c, y, but_2c, 20)
		GUI_A['material_on'] = Draw.Toggle('material', EVENT_NONE, but3c, y, but_3c, 20, GUI_A['material_on'].val, "support for material assignment on/off")
		Draw.EndAlign()


		y -= 30
		Draw.BeginAlign()
		GUI_A['group_bylayer_on'] = Draw.Toggle('oneGroup', EVENT_NONE, but1c, y, but_1c/2, 20, GUI_A['group_bylayer_on'].val, "group entities from the same layer on/off")
		GUI_A['curves_on'] = Draw.Toggle('to Curve', EVENT_NONE, but1c+ but_1c/2, y, but_1c/2, 20, GUI_A['curves_on'].val, "draw LINE/ARC/*PLINE into Blender-Curves instead of Meshes on/off")
		GUI_A['dummy_on'] = Draw.Toggle('-', EVENT_NONE, but2c, y, (but_2c+but_3c)/2, 20, GUI_A['dummy_on'].val, "dummy on/off")
		GUI_A['target_layer'] = Draw.Number('layer', EVENT_NONE, but2c+(but_2c+but_3c)/2, y, (but_2c+but_3c)/2, 20, GUI_A['target_layer'].val, 1, 18, "draw all into this Blender-layer (except <19> for block_definitios)")
		Draw.EndAlign()


		y -= 20
		Draw.BeginAlign()
		GUI_A['one_mesh_on'] = Draw.Toggle('oneMesh', EVENT_NONE, but1c, y, but_1c/2, 20, GUI_A['one_mesh_on'].val, "draw LINEs/3DFACEs from the same layer into one mesh-object on/off")
		GUI_A['vGroup_on'] = Draw.Toggle('vGroups', EVENT_NONE, but1c+ but_1c/2, y, but_1c/2, 20, GUI_A['vGroup_on'].val, "support Blender-VertexGroups on/off")
		GUI_A['g_scale_on'] = Draw.Toggle('glob.Scale', EVENT_NONE, but2c, y, (but_2c+but_3c)/2, 20, GUI_A['g_scale_on'].val, "scaling all DXF objects on/off")
		GUI_A['g_scale_as'] = Draw.Menu(g_scale_list, EVENT_NONE, but2c+(but_2c+but_3c)/2, y, (but_2c+but_3c)/2, 20, GUI_A['g_scale_as'].val, "10^ factor for scaling the DXFdata")
		Draw.EndAlign()


		y -= 20
		Draw.BeginAlign()
		Draw.Label('', but1c+but_1c/2, y, but_1c/2, 20)
		GUI_A['dist_on'] = Draw.Toggle('dist.:', EVENT_NONE, but2c, y, but_2c-20, 20, GUI_A['dist_on'].val, "support distance on/off")
		GUI_A['dist_force'] = Draw.Toggle('F', EVENT_NONE, but2c+but_2c-20, y,  20, 20, GUI_A['dist_force'].val, "force minimal distance on/off (double.vertex removing)")
		GUI_A['dist_min'] = Draw.Number('', EVENT_NONE, but2c+(but_2c+but_3c)/2, y, (but_2c+but_3c)/2, 20, GUI_A['dist_min'].val, 0, 10, "minimal distance for double.vertex removing")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_A['pl_section_on'] = Draw.Toggle('self.cut:', EVENT_NONE, but1c, y, but_1c/2, 20, GUI_A['pl_section_on'].val, "support POLYLINE-wide-segment-intersection on/off")
		GUI_A['angle_cut'] = Draw.Number('', EVENT_NONE, but1c+but_1c/2, y, but_1c/2, 20, GUI_A['angle_cut'].val, 1, 5, "it limits POLYLINE-wide-segment-intersection: 1.0-5.0")
		GUI_A['thick_on'] = Draw.Toggle('thick:', EVENT_NONE, but2c, y, but_2c-20, 20, GUI_A['thick_on'].val, "support thickness on/off")
		GUI_A['thick_force'] = Draw.Toggle('F', EVENT_NONE, but2c+but_2c-20, y,  20, 20, GUI_A['thick_force'].val, "force minimal thickness on/off")
		GUI_A['thick_min'] = Draw.Number('', EVENT_NONE, but2c+(but_2c+but_3c)/2, y, (but_2c+but_3c)/2, 20, GUI_A['thick_min'].val, 0, 10, "minimal thickness")
		Draw.EndAlign()


		y -= 20
		Draw.BeginAlign()
		GUI_A['arc_res'] = Draw.Number('arc:''', EVENT_NONE, but1c, y, but_1c/2, 20, GUI_A['arc_res'].val, 4, 500, "arc/circle resolution - number of segments")
		GUI_A['thin_res'] = Draw.Number('thin:', EVENT_NONE, but1c+but_1c/2, y, but_1c/2, 20, GUI_A['thin_res'].val, 4, 500, "thin cylinder resolution - number of segments")
		GUI_A['width_on'] = Draw.Toggle('width:', EVENT_NONE, but2c, y, but_2c-20, 20, GUI_A['width_on'].val, "support width on/off")
		GUI_A['width_force'] = Draw.Toggle('F', EVENT_NONE, but2c+but_2c-20, y, 20, 20, GUI_A['width_force'].val, "force minimal width on/off")
		GUI_A['width_min'] = Draw.Number('', EVENT_NONE, but2c+(but_2c+but_3c)/2, y, (but_2c+but_3c)/2, 20, GUI_A['width_min'].val, 0, 10, "minimal width")
		Draw.EndAlign()

		y -= 30
		Draw.BeginAlign()
		Draw.PushButton('Load', EVENT_LOAD_INI, but1c, y, but_1c/2, 20, '     Load configuration from ini-file: %s' % iniFileName.val)
		Draw.PushButton('Save', EVENT_SAVE_INI, but1c+but_1c/2, y, but_1c/2, 20, 'Save configuration to ini-file: %s' % iniFileName.val)
		GUI_A['optimization'] = Draw.Number('optim:', EVENT_NONE, but2c, y, but_2c, 20, GUI_A['optimization'].val, 0, 3, "Optimisation Level: 0=Debug/directDrawing, 1=Verbose, 2=ProgressBar, 3=silentMode/fastest")
		Draw.PushButton('3D', EVENT_PRESET, but3c, y, but_3c/2, 20, 'reset configuration to 3D-defaults')
		Draw.PushButton('2D', EVENT_PRESET2D, but3c+but_3c/2, y, but_3c/2, 20, 'reset configuration to 2D-defaults')
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		Draw.PushButton('INI file >', EVENT_CHOOSE_INI, but1c, y, but_1c/2, 20, 'Select INI-file from project directory')
		iniFileName = Draw.String(' : ', EVENT_NONE, but1c+(but_1c/2), y, but_1c/2+but_2c+but_3c, 20, iniFileName.val, FILENAME_MAX, "write here the name of the INI-file")
		Draw.EndAlign()

	y -= 30
	Draw.BeginAlign()
	Draw.PushButton('DXFfile >', EVENT_CHOOSE_DXF, but1c, y, but_1c/2, 20, 'Select DXF-file from project directory')
	dxfFileName = Draw.String(' :', EVENT_NONE, but1c+(but_1c/2), y, but_1c/2+but_2c+but_3c, 20, dxfFileName.val, FILENAME_MAX, "write here the name of the imported DXF-file")
	Draw.EndAlign()

	y -= 50
	Draw.BeginAlign()
	Draw.Label('  ', but1c-menu_margin, y, menu_margin, 40)
	Draw.PushButton('EXIT', EVENT_EXIT, but1c, y, but_1c/2, 40, '' )
	Draw.PushButton('HELP', EVENT_HELP, but1c+but_1c/2, y, but_1c/2, 20, 'manual-page on Blender-Wiki, support at www.blenderartists.org')
	Draw.PushButton('START IMPORT', EVENT_START, but2c, y, but_2c+but_3c, 40, 'Start the import procedure')
	Draw.Label('  ', but1c+menu_w, y, menu_margin, 40)
	Draw.EndAlign()

	config_UI = Draw.Toggle('CONFIG', EVENT_CONFIG, but1c+but_1c/2, y+20, but_1c/2, 20, config_UI.val, 'Advanced configuration on/off' )

	y -= 20
	Draw.BeginAlign()
	Draw.Label("*) parts under construction", but1c, y, menu_w, 20)
	Draw.EndAlign()

#-- END GUI Stuf-----------------------------------------------------

def colorbox(x,y,xright,bottom):
   glColor3f(0.75, 0.75, 0.75)
   glRecti(x + 1, y + 1, xright - 1, bottom - 1)

def dxf_callback(input_filename):
	global dxfFileName
	dxfFileName.val=input_filename

def ini_callback(input_texture):
	global iniFileName
	iniFileName.val=input_texture

def event(evt, val):
	if evt in (Draw.QKEY, Draw.ESCKEY) and not val:
		Blender.Draw.Exit()

def bevent(evt):
#	global EVENT_NONE,EVENT_LOAD_DXF,EVENT_LOAD_INI,EVENT_SAVE_INI,EVENT_EXIT
	global config_UI

	######### Manages GUI events
	if (evt==EVENT_EXIT):
		Blender.Draw.Exit()
	elif (evt==EVENT_CHOOSE_INI):
		Window.FileSelector(ini_callback, "INI-file Selection", '*.ini')
	elif (evt==EVENT_CONFIG):
		Draw.Redraw()
	elif (evt==EVENT_PRESET):
		resetDefaultConfig()
		Draw.Redraw()
	elif (evt==EVENT_PRESET2D):
		resetDefaultConfig_2D()
		Draw.Redraw()
	elif (evt==EVENT_HELP):
		try:
			import webbrowser
			webbrowser.open('http://wiki.blender.org/index.php?title=Scripts/Manual/Import/DXF-3D')
		except:
			Draw.PupMenu('DXF importer:	HELP Alert!%t|no connection to manual-page on Blender-Wiki!    try:|\
http://wiki.blender.org/index.php?title=Scripts/Manual/Import/DXF-3D')
		Draw.Redraw()
	elif (evt==EVENT_LOAD_INI):
		loadConfig()
		Draw.Redraw()
	elif (evt==EVENT_SAVE_INI):
		saveConfig()
		Draw.Redraw()
	elif (evt==EVENT_CHOOSE_DXF):
		Window.FileSelector(dxf_callback, "DXF-file Selection", '*.dxf')
	elif (evt==EVENT_START):
		dxfFile = dxfFileName.val
		#print 'deb: dxfFile file: ', dxfFile #----------------------
		if dxfFile.lower().endswith('.dxf') and sys.exists(dxfFile):
			main()
			#Blender.Draw.Exit()
			Draw.Redraw()
		else:
			Draw.PupMenu('DXF importer:	 Alert!%t|no valid DXF-file selected!')
			print "DXF importer: error, no valid DXF-file selected! try again"
			Draw.Redraw()


if __name__ == "__main__":
	UI_MODE = True
	Draw.Register(draw_UI, event, bevent)


"""
if 1:
	# DEBUG ONLY
	TIME= Blender.sys.time()
	#DIR = '/dxf_r12_testfiles/'
	DIR = '/metavr/'
	import os
	print 'Searching for files'
	os.system('find %s -iname "*.dxf" > /tmp/tempdxf_list' % DIR)
	# os.system('find /storage/ -iname "*.dxf" > /tmp/tempdxf_list')
	print '...Done'
	file= open('/tmp/tempdxf_list', 'r')
	lines= file.readlines()
	file.close()
	# sort by filesize for faster testing
	lines_size = [(os.path.getsize(f[:-1]), f[:-1]) for f in lines]
	lines_size.sort()
	lines = [f[1] for f in lines_size]



	for i, _dxf in enumerate(lines):
		if i >= 70:
			#if 1:
			print 'Importing', _dxf, '\nNUMBER', i, 'of', len(lines)
			_dxf_file= _dxf.split('/')[-1].split('\\')[-1]
			newScn= bpy.data.scenes.new(_dxf_file)
			bpy.data.scenes.active = newScn
			# load_dxf(_dxf, False)
			dxfFileName.val = _dxf
			main()

	print 'TOTAL TIME: %.6f' % (Blender.sys.time() - TIME)
"""