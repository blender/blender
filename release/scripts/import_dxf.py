#!BPY

"""
Name: 'Autodesk DXF (.dxf .dwg)'
Blender: 249
Group: 'Import'
Tooltip: 'Import for DWG/DXF geometry data.'
"""
__author__ = 'Kitsu(Ed Blake) & migius(Remigiusz Fiedler)'
__version__ = '1.12 - 2009.06.16 by migius'
__url__ = ["http://blenderartists.org/forum/showthread.php?t=84319",
	 "http://wiki.blender.org/index.php/Scripts/Manual/Import/DXF-3D"]
__email__ = ["migius(at)4d-vectors.de","Kitsune_e(at)yahoo.com"]
__bpydoc__ = """\
This script imports objects from DWG/DXF (2d/3d) into Blender.

This script imports 2d and 3d geometery from DXF files.
It supports DWG format too, with help of an external converter.
Supported DXF format versions: from (r2.5) r12 up to r2008.
Enhanced features are:
- configurable object filtering and geometry manipulation,
- configurable material pre-processing,
- DXF-code analyze and reporting.

Supported DXF r12 objects:
LINE,
POINT,
SOLID,
TRACE,
TEXT,
INSERT (=block),
MINSERT (=array of blocks),
CIRCLE,
ARC,
3DFACE,
2d-POLYLINE (=in plane, incl. arc, variable-width, curve, spline),
3d-POLYLINE (=non-plane),
3d-POLYMESH,
3d-POLYFACE,
VIEW, VPORT
XREF (External Reference).

Supported DXF>r12 objects:
ELLIPSE,
LWPOLYLINE (LightWeight Polyline),
SPLINE,
(todo v1.13) MLINE,
(todo v1.13) MTEXT

Unsupported objects:
DXF r12: DIMENSION.
DXF>r12: GROUP, RAY/XLINE, LEADER, 3DSOLID, BODY, REGION, dynamic BLOCK

Supported geometry: 2d and 3d DXF-objects.
Curves imported as Blender curves or meshes optionally.

Supported layout modes:
"model space" is default,
"paper space" as option (= "layout views")

Supported scene definition objects produced with AVE_RENDER:
scene: selection of lights assigned to the camera,
lights: DIRECT, OVERHEAD, SH_SPOT,
(wip v1.13 import of AVE_RENDER material definitions)

Hierarchy:
Entire DXF BLOCK hierarchy is preserved after import into Blender
(BLOCKs as groups on layer19, INSERTs as dupliGroups on target layer).

Supported properties:
visibility status,
frozen status,
thickness,
width,
color,
layer,
(todo v1.13: XDATA, grouped status)
It is recommended to use DXF-object properties for assign Blender materials.

Notes:
- Recommend that you run 'RemoveDoubles' on each imported mesh after using this script
- Blocks are created on layer 19 then referenced at each insert point.
- support for DXF-files up to 160MB on systems with 1GB RAM
- DXF-files with over 1500 objects decrease import performance.
The problem is not the inefficiency of python-scripting but Blenders performance
in creating new objects in scene database - probably a database management problem.

"""

"""
History:
 v1.0 - 2007/2008/2009 by migius
 planned tasks:
 -- (to see more, search for "--todo--" in script code)
 -- command-line-mode/batch-mode
 -- in-place-editing for dupliGroups
 -- support for MLINE (is exported to r12 as BLOCK*Unnamed with LINEs)
 -- support for MTEXT (is exported to r12 as TEXT???)
 -- blender_object.properties['dxf_layer_name']
 -- better support for long dxf-layer-names 
 -- add configuration file.ini handles multiple material setups
 -- added f_layerFilter
 -- to-check: obj/mat/group/_mapping-idea from ideasman42
 -- curves: added "fill/non-fill" option for closed curves: CIRCLEs,ELLIPSEs,POLYLINEs
 -- "normalize Z" option to correct non-planar figures
 -- LINEs need "width" in 3d-space incl vGroups
 -- support width_force for LINEs/ELLIPSEs = "solidify"
 -- add better support for color_index BYLAYER=256, BYBLOCK=0 
 -- bug: "oneMesh" produces irregularly errors
 -- bug: Registry recall from hd_cache ?? only win32 bug??
 -- support DXF-definitions of autoshade: scene, lights and cameras
 -- support ortho mode for VIEWs and VPORTs as cameras 

 v1.12 - 2009.06.16 by migius
 d7 fix for ignored BLOCKs (e.g. *X) which are members of other BLOCKs
 v1.12 - 2009.05.27 by migius
 d6 bugfix negative scaled INSERTs - isLeftHand(Matrix) check
 v1.12 - 2009.05.26 by migius
 d5 changed to the new 2.49 method Vector.cross()
 d5 bugfix WORLDY(1,1,0) to (0,1,0)
 v1.12 - 2009.04.11 by migius
 d4 added DWG support, Stani Michiels idea for binding an extern DXF-DWG-converter 
 v1.12 - 2009.03.14 by migius
 d3 removed all set()functions (problem with osx/python<2.4 reported by Blinkozo)
 d3 code-cleaning
 v1.12 - 2009.01.14 by migius
 d2 temp patch for noname BLOCKS (*X,*U,*D)
 v1.12 - 2008.11.16 by migius
 d1 remove try_finally: cause not supported in python <2.5
 d1 add Bezier curves bevel radius support (default 1.0)
 v1.12 - 2008.08.03 by migius
 c2 warningfix: relocating of globals: layersmap, oblist 
 c2 modif UI: buttons newScene+targetLayer moved to start panel
 v1.12 - 2008.07.04 by migius
 c1 added control Curve's OrderU parameter
 c1 modif UI: preset buttons X-2D-3D moved to start panel
 b6 added handling exception of not registered LAYERs (Hammer-HL-editor DXF output)
 b5 rebuild UI: global preset 2D for Curve-Import
 b5 added UI-options: PL-MESH N+N plmesh_flip and normals_out 
 b5 added support for SPLINEs, added control OrderU parameter
 b5 rewrote draw module for NURBS_curve and Bezier_curve
 v1.12 - 2008.06.22 by migius
 b4 change versioning system 1.0.12 -> 1.12
 b4 print at start version-info to console
 b3 bugfix: ob.name conflict with existing meshes (different ob.name/mesh.name)
 v1.0.12: 2008.05.24 by migius
 b2 added support for LWPOLYLINEs
 b2 added support for ProE in readerDXF.py
 v1.0.12: 2008.02.08 by migius
 b1 update: object = Object.Get(obname) -> f_getSceChild().getChildren()
 a9 bugfix by non-existing tables views, vports, layers (Kai reported)
 v1.0.12: 2008.01.17 by migius
 a8 lately used INI-dir/filename persistently stored in Registry
 a8 lately used DXF-dir/filename persistently stored in Registry
 a7 fix missing layersmap{} for dxf-files without "section:layer"
 a6 added support for XREF external referenced BLOCKs
 a6 check for bug in AutoCAD2002:DXFr12export: ELLIPSE->POLYLINE_ARC fault angles
 a6 support VIEWs and VPORTs as cameras: ortho and perspective mode
 a6 save resources through ignoring unused BLOCKs (not-inserted or on frozen/blocked layers)
 a6 added try_finally: f.close() for all IO-files
 a6 added handling for TypeError raise
 a5 bugfix f_getOCS for (0,0,z!=1.0) (ellipse in Kai's dxf)
 a4 added to analyzeTool: report about VIEWs, VPORTs, unused/xref BLOCKs
 a4 bugfix: individual support for 2D/3DPOLYLINE/POLYMESH
 a4 added to UI: (*wip)BLOCK-(F): name filtering for BLOCKs
 a4 added to UI: BLOCK-(n): filter noname/hatch BLOCKs *X...
 a2 g_scale_as is no more GUI_A-variable
 a2 bugfix "material": negative sign color_index
 a2 added support for BLOCKs defined with origin !=(0,0,0)
 a1 added 'global.reLocation-vector' option

 v1.0.11: 2007.11.24 by migius
 c8 added 'curve_resolution_U' option 
 c8 added context_sensitivity for some UI-buttons
 c8 bugfix ELLIPSE rotation, added closed_variant and caps
 c7 rebuild UI: new layout, grouping and meta-buttons
 c6 rewritten support for ELLIPSE mesh & curve representation
 c6 restore selector-buttons for DXF-drawTypes: LINE & Co
 c6 change header of INI/INF-files: # at begin
 c6 apply scale(1,1,1) after glob.Scale for all mesh objects, not for curve objects.
 c5 fixing 'material_on' option
 c4 added "analyze DXF-file" UI-option: print LAYER/BLOCK-dependences into a textfile
 c3 human-formating of data in INI-Files
 c2 added "caps" for closed Bezier-curves
 c2 added "set elevation" UI-option
 c1 rewrite POLYLINE2d-arc-segments Bezier-interpreter
 b9 many bugs fixed
 b9 rewrite POLYLINE2d-arc-segments trimming (clean-trim)
 b8 added "import from frozen layers" UI-option
 b8 added "import from paper space" UI-option
 b8 support Bezier curves for LINEs incl.thickness(0.0-10.0)
 b8 added meshSmooth_on for circle/arc/polyline 
 b8 added vertexGroups for circle/arc 
 b7 added width_force for ARCs/CIRCLEs = "thin_box" option
 b3 cleanup code, rename f_drawArc/Bulg->f_calcArc/Bulg
 b2 fixing material assignment by LAYER+COLOR
 b1 fixing Bezier curves representation of POLYLINEs-arc-segments
 b0 added global_scale_presets: "yard/feet/inch to meter"

 v1.0.10: 2007.10.18 by migius
 a6 bugfix CircleDrawCaps for OSX 
 a5 added two "curve_res" UI-buttons for Bezier curves representation
 a5 improved Bezier curves representation of circles/arcs: correct handlers
 a4 try to fix malformed endpoints of Blender curves of ARC/POLYLINE-arc segments. 
 a3 bugfix: open-POLYLINEs with end_point.loc==start_point.loc
 a2 bugfix: f_transform for OCS=(0,0,-1) oriented objects
 a1 added "fill_on=caps" option to draw top and bottom sides of CIRCLEs and ELLIPSEs
 a1 rewrite f_CIRCLE.Draw: from Mesh.Primitive to Mesh
 a1 bugfix "newScene"-mode: all Cylinders/Arcs were drawn at <0,0,0>location

 v1.0.beta09: 2007.09.02 by migius
 g5 redesign UI: grouping of buttons
 g3 update multi-import-mode: <*.*> button
 g- added multi-import-mode: (path/*) for importing many dxf-files at once
 g- added import into newScene
 g- redesign UI: user presets, into newScene-import  
 f- cleanup code
 f- bugfix: thickness for Bezier/Bsplines into Blender-curves
 f- BlenderWiki documentation, on-line Manual
 f- added import POLYLINE-Bsplines into Blender-NURBSCurves
 f- added import POLYLINE-arc-segments into Blender-BezierCurves
 f- added import POLYLINE-Bezier-curves into Blender-Curves
 d5 rewrite: Optimization Levels, added 'directDrawing'
 d4 added: f_set_thick(controlled by ini-parameters)
 d4 bugfix: face-normals in objects with minus thickness
 d4 added: placeholder'Empty'-size in f_Insert.draw
 d3 rewrite f_Text.Draw: added support for all Text's parameters
 d2 redesign: progressbar 
 e- tuning by ideasman42: better use of the Py API.
 c- tuning by ideasman42
 b- rewrite f_Text.Draw rotation/transform
 b- bugfix: POLYLINE-segment-intersection more reliable now
 b- bugfix: circle:_thic, 'Empties':no material_assignment
 b- added material assignment (from layer and/or color)
 a- added empty, cylinder and UVsphere for POINTs
 a- added support for 2d-POLYLINE: splines, fitted curves, fitted surfaces
 a- redesign f_Drawer for block_definitions
 a- rewrite import into Blender-Curve-Object

 v1.0.beta08 - 2007.07.27 by migius: "full 3d"-release
 l- bugfix: solid_vgroups, clean:scene.objects.new()
 l- redesign UI to standard Draw.Register+FileSelector, advanced_config_option
 k- bugfix UI:fileSelect() for MacOSX os.listdir()
 k- added reset/save/load for config-data
 k- redesign keywords/drawTypes/Draw.Create_Buttons
 j- new UI using UIBlock() with own FileSelector, cause problem Window.FileSelector()
 i- rewritten Class:Settings for better config-parameter management
 h- bugfix: face-normals in objects with minus thickness
 h- added Vertex-Groups in POLYLINE and SOLID meshes, for easy material assignment
 h- beautify code, whitespace->tabs
 h- added settings.thic_force switch for forcing thickness
 h- added "one Mesh" option for all entities from the same Layer, sorted in<br>
 Vertex-Groups(color_name)  (fewer objects = better import performance)
 g- rewrote: insert-point-handle-object is a small tetrahedron
 e- bugfix: closed-polymesh3d
 - rewrote: UI, type_map.keys, f_drawer, all class_f_draw(added "settings" as attribut)
 - added 2d/3d-support for Polyline_Width incl. angle intersection
 beta07: 2007.06.19 by migius
 - added 3d-support for LWPolylines
 - added 2d/3d-support for Points
 beta06: 2007.06.15 by migius
 - cleanup code
 - added 2d/3d-support for MINSERT=BlockArray in f_drawer, added f_rotXY_Vec
 beta05: 2007.06.14 by migius
 - added 2d/3d-support for 3d-PolyLine, PolyMesh and PolyFace
 - added Global-Scale for size control of imported scenes
 beta04: 2007.06.12 by migius
 - rewrote the f_drawBulge for correct import the arc-segments of Polylines
 beta03: 2007.06.10 by migius
 - rewrote interface
 beta02: 2007.06.09 by migius
 - added 3d-support for Arcs and Circles
 - added support for Object_Thickness(=height)
 beta01: 2007.06.08 by migius
 - added 3d-support for Blocks/Inserts within nested-structures
 - rewrote f_transform for correct 3d-location/3d-rotation
 - added 3d-support Lines, 3dFaces
 - added 2d+3d-support for Solids and Traces

 v0.9 - 2007.01 by kitsu: (for 2.43)
 - first draft of true POLYLINE import
 -

 v0.8 - 2006.12 by kitsu:
 - first draft of object space coordinates OCS import
 -

 v0.5b - 2006.10 by kitsu: (for 2.42a)
 - dxfReader.py
 - color_map.py

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
from Blender import Mathutils, BezTriple, Draw, Registry, sys,\
Text3d, Window, Mesh, Material, Group, Curve
#from Blender.Mathutils import Vector, Matrix
#import bpy #not used yet
#import BPyMessages

from dxfReader import readDXF
#from dxfReader import get_name, get_layer
from dxfReader import Object as dxfObject
from dxfColorMap import color_map
from math import log10, sqrt, radians, degrees, atan, cos, sin

# osx-patch by Blinkozo
#todo: avoid additional modules, prefer Blender-build-in test routines
#import platform
#if platform.python_version() < '2.4':
#    from sets import Set as set
#from sys import version_info
#ver = '%s.%s' % version_info[0:2]
# end osx-patch

import subprocess
import os
if os.name != 'mac':
	try:
		import psyco
		psyco.log(Blender.Get('tempdir')+"/blender.log-psyco")
		#psyco.log()
		psyco.full(memory=100)
		psyco.profile(0.05, memory=100)
		psyco.profile(0.2)
		#print 'psyco imported'
	except ImportError:
		print 'psyco not imported'

print '\n\n\n'
print 'DXF/DWG-Importer v%s *** start ***' %(__version__)   #---------------------

SCENE = None
WORLDX = Mathutils.Vector((1,0,0))
WORLDY = Mathutils.Vector((0,1,0))
WORLDZ = Mathutils.Vector((0,0,1))

G_SCALE = 1.0	   #(0.0001-1000) global scaling factor for all dxf data
G_ORIGIN_X = 0.0   #global translation-vector (x,y,z) in DXF units
G_ORIGIN_Y = 0.0
G_ORIGIN_Z = 0.0
MIN_DIST = 0.001	#cut-off value for sort out short-distance polyline-"duoble_vertex"
ARC_RESOLUTION = 64   #(4-500) arc/circle resolution - number of segments
ARC_RADIUS = 1.0   #(0.01-100) arc/circle radius for number of segments algorithm
CURV_RESOLUTION = 12 #(1-128) Bezier curves U-resolution
CURVARC_RESOLUTION = 4 #(3-32) resolution of circle represented as Bezier curve 
THIN_RESOLUTION = 8   #(4-64) thin_cylinder arc_resolution - number of segments
MIN_THICK = MIN_DIST * 10.0  #minimal thickness by forced thickness
MIN_WIDTH = MIN_DIST * 10.0  #minimal width by forced width
TRIM_LIMIT = 3.0	 #limit for triming of polylines-wide-segments (values:0.0 - 5.0)
ELEVATION = 0.0 #standard elevation = coordinate Z value

BYBLOCK = 0
BYLAYER = 256
TARGET_LAYER = 3	#target blender_layer
GROUP_BYLAYER = 0   #(0/1) all entities from same layer import into one blender-group
LAYER_DEF_NAME = 'AAAA' #default layer name
LAYER_DEF_COLOR = 4 #default layer color
E_M = 0
LAB = ". wip   .. todo" #"*) parts under construction"
M_OBJ = 0

FILENAME_MAX = 180	#max length of path+file_name string  (FILE_MAXDIR + FILE_MAXFILE)
MAX_NAMELENGTH = 17   #max_effective_obnamelength in blender =21=17+(.001)
INIFILE_DEFAULT_NAME = 'importDXF'
INIFILE_EXTENSION = '.ini'
INIFILE_HEADER = '#ImportDXF.py ver.1.0 config data'
INFFILE_HEADER = '#ImportDXF.py ver.1.0 analyze of DXF-data'

AUTO = BezTriple.HandleTypes.AUTO
FREE = BezTriple.HandleTypes.FREE
VECT = BezTriple.HandleTypes.VECT
ALIGN = BezTriple.HandleTypes.ALIGN

UI_MODE = True #activates UI-popup-print, if not multiple files imported

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
	
	
#-------- DWG support ------------------------------------------
extCONV_OK = True
extCONV = 'DConvertCon.exe'
extCONV_PATH = os.path.join(Blender.Get('scriptsdir'),extCONV)
if not os.path.isfile(extCONV_PATH):
	extCONV_OK = False
	extCONV_TEXT = 'DWG-Importer cant find external DWG-converter (%s) in Blender script directory.|\
More details in online Help.' %extCONV
else:
	if not os.sys.platform.startswith('win'):
		# check if Wine installed:   
		if subprocess.Popen(('which', 'winepath'), stdout=subprocess.PIPE).stdout.read().strip():
			extCONV_PATH    = 'wine %s'%extCONV_PATH
		else: 
			extCONV_OK = False
			extCONV_TEXT = 'The external DWG-converter (%s) needs Wine installed on your system.|\
More details in online Help.' %extCONV
#print 'extCONV_PATH = ', extCONV_PATH



class View:  #-----------------------------------------------------------------
	"""Class for objects representing dxf VIEWs.
	"""
	def __init__(self, obj, active=None):
		"""Expects an object of type VIEW as input.
		"""
		if not obj.type == 'view':
			raise TypeError, "Wrong type %s for VIEW object!" %obj.type

		self.type = obj.type
		self.name = obj.get_type(2)[0]
#		self.data = obj.data[:]


		self.centerX = getit(obj, 10, 0.0) #view center pointX (in DCS)
		self.centerY = getit(obj, 20, 0.0) #view center pointY (in DCS)
		self.height = obj.get_type(40)[0] #view height (in DCS)
		self.width = obj.get_type(41)[0] #view width (in DCS)

		self.dir = [0,0,0]
		self.dir[0] = getit(obj, 11, 0.0) #view directionX from target (in WCS)
		self.dir[1] = getit(obj, 21, 0.0) #
		self.dir[2] = getit(obj, 31, 0.0) #

		self.target = [0,0,0]
		self.target[0] = getit(obj, 12, 0.0) #target pointX(in WCS)
		self.target[1] = getit(obj, 22, 0.0) #
		self.target[2] = getit(obj, 32, 0.0) #

		self.length = obj.get_type(42)[0] #Lens length
		self.clip_front = getit(obj, 43) #Front clipping plane (offset from target point)
		self.clip_back = getit(obj, 44) #Back clipping plane (offset from target point)
		self.twist  = obj.get_type(50)[0] #view twist angle in degrees

		self.flags = getit(obj, 70, 0)
		self.paperspace = self.flags & 1 #

		self.mode = obj.get_type(71)[0] #view mode (VIEWMODE system variable)

	def __repr__(self):
		return "%s: name - %s, focus length - %s" %(self.__class__.__name__, self.name, self.length)


	def draw(self, settings):
		"""for VIEW: generate Blender_camera.
		"""
		obname = 'vw_%s' %self.name  # create camera object name
		#obname = 'ca_%s' %self.name  # create camera object name
		obname = obname[:MAX_NAMELENGTH]

		if self.target == [0,0,0] and Mathutils.Vector(self.dir).length == 1.0:
			cam= Camera.New('ortho', obname)
			ob= SCENE.objects.new(cam)
			cam.type = 'ortho'
			cam.scale = 1.0  # for ortho cameras
		else:
			cam= Camera.New('persp', obname)
			ob= SCENE.objects.new(cam)
			cam.type = 'persp'
			cam.angle = 60.0  # for persp cameras
			if self.length:
				#cam.angle = 2 * atan(17.5/self.length) * 180/pi
				cam.lens = self.length #for persp cameras
			# hack to update Camera>Lens setting (inaccurate as a focal length) 
			#curLens = cam.lens; cam.lens = curLens
			# AutoCAD gets clip distance from target:
			dist = Mathutils.Vector(self.dir).length
			cam.clipEnd = dist - self.clip_back
			cam.clipStart = dist - self.clip_front
	
		cam.drawLimits = 1 
		cam.drawSize = 10
		
		v = Mathutils.Vector(self.dir)
#		print 'deb:view cam:', cam #------------
#		print 'deb:view self.target:', self.target #------------
#		print 'deb:view self.dir:', self.dir #------------
#		print 'deb:view self.twist:', self.twist #------------
#		print 'deb:view self.clip_front=%s, self.clip_back=%s, dist=%s' %(self.clip_front, self.clip_back, dist) #------------
		transform(v.normalize(), -self.twist, ob)
		ob.loc =  Mathutils.Vector(self.target) + Mathutils.Vector(self.dir)
		return ob


class Vport:  #-----------------------------------------------------------------
	"""Class for objects representing dxf VPORTs.
	"""
	def __init__(self, obj, active=None):
		"""Expects an object of type VPORT as input.
		"""
		if not obj.type == 'vport':
			raise TypeError, "Wrong type %s for VPORT object!" %obj.type

		self.type = obj.type
		self.name = obj.get_type(2)[0]
#		self.data = obj.data[:]
		#print 'deb:vport name, data:', self.name #-------
		#print 'deb:vport data:', self.data #-------

		self.height = obj.get_type(40)[0] #vport height (in DCS)
		self.centerX = getit(obj, 12, 0.0) #vport center pointX (in DCS)
		self.centerY = getit(obj, 22, 0.0) #vport center pointY (in DCS)
		self.width = self.height * obj.get_type(41)[0] #vport aspect ratio - width (in DCS)

		self.dir = [0,0,0]
		self.dir[0] = getit(obj, 16, 0.0) #vport directionX from target (in WCS)
		self.dir[1] = getit(obj, 26, 0.0) #
		self.dir[2] = getit(obj, 36, 0.0) #

		self.target = [0,0,0]
		self.target[0] = getit(obj, 17, 0.0) #target pointX(in WCS)
		self.target[1] = getit(obj, 27, 0.0) #
		self.target[2] = getit(obj, 37, 0.0) #

		self.length = obj.get_type(42)[0] #Lens length
		self.clip_front = getit(obj, 43) #Front clipping plane (offset from target point)
		self.clip_back = getit(obj, 44) #Back clipping plane (offset from target point)
		self.twist  = obj.get_type(51)[0] #view twist angle

		self.flags = getit(obj, 70, 0)
		self.paperspace = self.flags & 1 #

		self.mode = obj.get_type(71)[0] #view mode (VIEWMODE system variable)

	def __repr__(self):
		return "%s: name - %s, focus length - %s" %(self.__class__.__name__, self.name, self.length)

	def draw(self, settings):
		"""for VPORT: generate Blender_camera.
		"""
		obname = 'vp_%s' %self.name  # create camera object name
		#obname = 'ca_%s' %self.name  # create camera object name
		obname = obname[:MAX_NAMELENGTH]

		if self.target == [0,0,0] and Mathutils.Vector(self.dir).length == 1.0:
			cam= Camera.New('ortho', obname)
			ob= SCENE.objects.new(cam)
			cam.type = 'ortho'
			cam.scale = 1.0  # for ortho cameras
		else:
			cam= Camera.New('persp', obname)
			ob= SCENE.objects.new(cam)
			cam.type = 'persp'
			cam.angle = 60.0  # for persp cameras
			if self.length:
				#cam.angle = 2 * atan(17.5/self.length) * 180/pi
				cam.lens = self.length #for persp cameras
			# hack to update Camera>Lens setting (inaccurate as a focal length) 
			#curLens = cam.lens; cam.lens = curLens
			# AutoCAD gets clip distance from target:
			dist = Mathutils.Vector(self.dir).length
			cam.clipEnd = dist - self.clip_back
			cam.clipStart = dist - self.clip_front
	
		cam.drawLimits = 1 
		cam.drawSize = 10
		
		v = Mathutils.Vector(self.dir)
#		print 'deb:view cam:', cam #------------
#		print 'deb:view self.target:', self.target #------------
#		print 'deb:view self.dir:', self.dir #------------
#		print 'deb:view self.twist:', self.twist #------------
#		print 'deb:view self.clip_front=%s, self.clip_back=%s, dist=%s' %(self.clip_front, self.clip_back, dist) #------------
		transform(v.normalize(), -self.twist, ob)
		ob.loc =  Mathutils.Vector(self.target) + Mathutils.Vector(self.dir)
		return ob



class Layer:  #-----------------------------------------------------------------
	"""Class for objects representing dxf LAYERs.
	"""
	def __init__(self, obj, name=None, color=None, frozen=None):
		"""Expects an dxfobject of type layer as input.
			if no dxfobject - creates surogate layer with default parameters
		"""

		if obj==None:
			self.type = 'layer'
			if name: self.name = name
			else: self.name = LAYER_DEF_NAME

			if color: self.color = color
			else: self.color = LAYER_DEF_COLOR

			if frozen!=None: self.frozen = frozen
			else: self.frozen = 0
		else:	
			if obj.type=='layer':
				self.type = obj.type
				#self.data = obj.data[:]
				if name: self.name = name
					#self.bfname = name  #--todo---see layernamesmap in f_getLayersmap ---
				else: self.name = obj.get_type(2)[0] #layer name of object
		
				if color: self.color = color
				else: self.color = obj.get_type(62)[0]  #color of object
		
				if frozen!=None: self.frozen = frozen
				else:
					self.flags = obj.get_type(70)[0]
					self.frozen = self.flags & 1
	
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
				# --todo-- I found one case where item was a text instance
				# that failed with no __getitem__
				pass
	else:	#else searching in Object with get_type-Methode
		item = obj.get_type(typ)
		if item:
			it = item[0]
	#print 'deb:getit:typ, it', typ, it #----------
	return it



def get_extrusion(data):	 #-------------------------------------------------
	"""Find the axis of extrusion.

	Used to get from object_data the objects Object_Coordinate_System (ocs).
	"""
	#print 'deb:get_extrusion: data: \n', data  #---------------
	vec = [0,0,1]
	vec[0] = getit(data, 210, 0) # 210 = x
	vec[1] = getit(data, 220, 0) # 220 = y
	vec[2] = getit(data, 230, 1) # 230 = z
	#print 'deb:get_extrusion: vec: ', vec  #---------------
	return vec


#------------------------------------------
def getSceneChild(name):
	dudu = [i for i in SCENE.objects if i.name==name]
#	dudu = [i for i in SCENE.getChildren() if i.name==name]
	#print 'deb:getSceneChild %s -result: %s:' %(name,dudu) #-----------------
	if dudu!=[]: return dudu[0]
	return None


class Solid:  #-----------------------------------------------------------------
	"""Class for objects representing dxf SOLID or TRACE.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type solid or trace as input.
		"""
		if obj.type == 'trace':
			obj.type = 'solid'
		if not obj.type == 'solid':
			raise TypeError, "Wrong type \'%s\' for solid/trace object!" %obj.type

		self.type = obj.type
#		self.data = obj.data[:]

		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj, 8, None)
		self.extrusion = get_extrusion(obj)
		self.points = self.get_points(obj)



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
		a[2] = getit(data, 30,  0)   # 30 = z
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
		
		if M_OBJ: obname, me, ob = makeNewObject()
		else: 
			me = Mesh.New(obname)		# create a new mesh
			ob = SCENE.objects.new(me) # create a new mesh_object
		me.verts.extend(points)		# add vertices to mesh
		if faces: me.faces.extend(faces)		   # add faces to the mesh
		if edges: me.edges.extend(edges)		   # add faces to the mesh

		if settings.var['vGroup_on'] and not M_OBJ:
			# each MeshSide becomes vertexGroup for easier material assignment ---------------------
			replace = Mesh.AssignModes.ADD  #or .AssignModes.ADD/REPLACE
			if vg_left: me.addVertGroup('side.left')  ; me.assignVertsToGroup('side.left',  vg_left, 1.0, replace)
			if vg_right:me.addVertGroup('side.right') ; me.assignVertsToGroup('side.right', vg_right, 1.0, replace)
			if vg_top:  me.addVertGroup('side.top')   ; me.assignVertsToGroup('side.top',   vg_top, 1.0, replace)
			if vg_bottom:me.addVertGroup('side.bottom'); me.assignVertsToGroup('side.bottom',vg_bottom, 1.0, replace)
			if vg_start:me.addVertGroup('side.start') ; me.assignVertsToGroup('side.start', vg_start, 1.0, replace)
			if vg_end:  me.addVertGroup('side.end')   ; me.assignVertsToGroup('side.end',   vg_end,   1.0, replace)

		transform(self.extrusion, 0, ob)

		return ob

class Line:  #-----------------------------------------------------------------
	"""Class for objects representing dxf LINEs.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type line as input.
		"""
		if not obj.type == 'line':
			raise TypeError, "Wrong type \'%s\' for line object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		#print 'deb:self.thic: ', self.thic #---------------------
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj, 8, None)
		self.extrusion = get_extrusion(obj)
		self.points = self.get_points(obj)


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
		thic = set_thick(self.thic, settings)
		width = 0.0
		if settings.var['lines_as'] == 4: # as thin_box
			thic = settings.var['thick_min']
			width = settings.var['width_min']
		elif settings.var['lines_as'] == 3: # as thin cylinder
			cyl_rad = 0.5 * settings.var['width_min']

		elif settings.var['lines_as'] == 5: # LINE curve representation-------------------------
			obname = 'li_%s' %self.layer  # create object name from layer name
			obname = obname[:MAX_NAMELENGTH]

			c = Curve.New(obname) # create new curve data
			curve = c.appendNurb(BezTriple.New(points[0]))
			curve.append(BezTriple.New(points[1]))
			for point in curve:
				point.handleTypes = [VECT, VECT]
				point.radius = 1.0
			curve.flagU = 0 # 0 sets the curve not cyclic=open
			c.setResolu(settings.var['curve_res'])
			c.update() #important for handles calculation

			ob = SCENE.objects.new(c) # create a new curve_object

			#if False:  # --todo-- better support for 210-group
			if thic != 0.0: #hack: Blender2.45 curve-extrusion
				t = thic * 0.5
				if abs(t) > 5.0: t = 5.0 * cmp(t,0) # Blender2.45 accepts only (0.0 - 5.0)
				e = self.extrusion
				c.setExt1(abs(t))  # curve-extrusion
				ob.LocX += t * e[0]
				ob.LocY += t * e[1]
				ob.LocZ += t * e[2]
				#c.setExt1(1.0)  # curve-extrusion: Blender2.45 accepts only (0.0 - 5.0)
				#ob.LocZ = t + self.loc[2]
				#ob.SizeZ *= abs(t)
			return ob

		else:  # LINE mesh representation ------------------------------
			global activObjectLayer
			global activObjectName
			#print 'deb:draw:line.ob IN activObjectName: ', activObjectName #---------------------
	
			if M_OBJ: obname, me, ob = makeNewObject()
			else: 
				if activObjectLayer == self.layer and settings.var['one_mesh_on']:
					obname = activObjectName
					#print 'deb:line.draw obname from activObjectName: ', obname #---------------------
					ob = getSceneChild(obname)  # open an existing mesh_object
					#ob = SCENE.getChildren(obname)  # open an existing mesh_object
					#me = Mesh.Get(ob.name)   # open objects mesh data
					me = ob.getData(name_only=False, mesh=True)
				else:
					obname = 'li_%s' %self.layer  # create object name from layer name
					obname = obname[:MAX_NAMELENGTH]
					me = Mesh.New(obname)		  # create a new mesh
					ob = SCENE.objects.new(me) # create a new mesh_object
					activObjectName = ob.name
					activObjectLayer = self.layer
					#print ('deb:line.draw new line.ob+mesh:"%s" created!' %ob.name) #---------------------
	
			faces, edges = [], []
			n = len(me.verts)

			#if settings.var['width_force']: #--todo-----------

			if thic != 0:
				t, e = thic, self.extrusion
				#print 'deb:thic, extr: ', t, e #---------------------
				points.extend([[v[0]+t*e[0], v[1]+t*e[1], v[2]+t*e[2]] for v in points[:]])
				faces = [[0+n, 1+n, 3+n, 2+n]]
			else:
				edges = [[0+n, 1+n]]
	
			me.verts.extend(points) # adds vertices to global mesh
			if faces: me.faces.extend(faces)	   # add faces to the mesh
			if edges: me.edges.extend(edges)	   # add faces to the mesh
	
			if settings.var['vGroup_on'] and not M_OBJ:
				# entities with the same color build one vertexGroup for easier material assignment ----
				ob.link(me) # link mesh to that object
				vG_name = 'color_%s' %self.color_index
				if edges: faces = edges
				replace = Mesh.AssignModes.ADD  #or .AssignModes.REPLACE or ADD
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
	"""Class for objects representing dxf POINTs.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type point as input.
		"""
		if not obj.type == 'point':
			raise TypeError, "Wrong type %s for point object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		#print 'deb:self.thic: ', self.thic #---------------------
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj, 8, None)
		self.extrusion = get_extrusion(obj)
		self.points = self.get_points(obj)


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

		if points_as in [1,3,4,5]:
			if points_as in [1,5]: # as 'empty'
				c = 'Empty'
			elif points_as == 3: # as 'thin sphere'
				res = settings.var['thin_res']
				c = Mesh.Primitives.UVsphere(res,res,thic)
			elif points_as == 4: # as 'thin box'
				c = Mesh.Primitives.Cube(thic)
			ob = SCENE.objects.new(c, obname) # create a new object
			transform(self.extrusion, 0, ob)
			ob.loc = tuple(points[0])

		elif points_as == 2: # as 'vertex'
			global activObjectLayer
			global activObjectName
			#print 'deb:draw:point.ob IN activObjectName: ', activObjectName #---------------------
			if M_OBJ: obname, me, ob = makeNewObject()
			else: 
				if activObjectLayer == self.layer and settings.var['one_mesh_on']:
					obname = activObjectName
					#print 'deb:draw:point.ob obname from activObjectName: ', obname #---------------------
					ob = getSceneChild(obname)  # open an existing mesh_object
					#ob = SCENE.getChildren(obname)  # open an existing mesh_object
					me = ob.getData(name_only=False, mesh=True)
					#me = Mesh.Get(ob.name)   # open objects mesh data
				else:
					me = Mesh.New(obname)		  # create a new mesh
					ob = SCENE.objects.new(me) # create a new mesh_object
					activObjectName = ob.name
					activObjectLayer = self.layer
					#print ('deb:draw:point new point.ob+mesh:"%s" created!' %ob.name) #---------------------
			me.verts.extend(points) # add vertices to mesh

		return ob



class Polyline:  #-----------------------------------------------------------------
	"""Class for objects representing dxf POLYLINEs.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type polyline as input.
		"""
		#print 'deb:polyline.init.START:----------------' #------------------------
		if not obj.type == 'polyline':
			raise TypeError, "Wrong type %s for polyline object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		self.space = getit(obj, 67, 0)
		self.elevation =  getit(obj, 30, 0)
		#print 'deb:elevation: ', self.elevation #---------------
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.flags = getit(obj, 70, 0)
		self.closed = self.flags & 1   # closed in the M direction
		self.curved = self.flags & 2   # Bezier-curve-fit vertices have been added
		self.spline = self.flags & 4   # NURBS-curve-fit vertices have been added
		self.poly3d = self.flags & 8   # 3D-polyline
		self.plmesh = self.flags & 16  # 3D-polygon mesh
		self.closeN = self.flags & 32  # closed in the N direction
		self.plface = self.flags & 64  # 3D-polyface mesh
		self.contin = self.flags & 128 # the linetype pattern is generated continuously

		self.pltype='poly2d'   # default is a 2D-polyline
		if self.poly3d: self.pltype='poly3d'
		elif self.plface: self.pltype='plface'
		elif self.plmesh: self.pltype='plmesh'

		self.swidth =  getit(obj, 40, 0) # default start width
		self.ewidth =  getit(obj, 41, 0) # default end width
		#self.bulge  =  getit(obj, 42, None) # bulge of the segment
		self.vectorsM =  getit(obj, 71, None) # PolyMesh: expansion in M-direction / PolyFace: number of the vertices
		self.vectorsN =  getit(obj, 72, None) # PolyMesh: expansion in M-direction / PolyFace: number of faces
		#self.resolM =  getit(obj, 73, None) # resolution of surface in M direction
		#self.resolN =  getit(obj, 74, None) # resolution of surface in N direction
		self.curvNoFitted = False
		self.curvQuadrati = False
		self.curvCubicBsp = False
		self.curvBezier = False
		curvetype =  getit(obj, 75, 0) # type of curve/surface: 0=None/5=Quadric/6=Cubic/8=Bezier
		if   curvetype == 0: self.curvNoFitted = True
		elif curvetype == 5: self.curvQuadrati = True
		elif curvetype == 6: self.curvCubicBsp = True
		elif curvetype == 8: self.curvBezier = True

		self.layer = getit(obj, 8, None)
		self.extrusion = get_extrusion(obj)

		self.points = []  #list with vertices coordinats
		self.faces  = []  #list with vertices assigment to faces
		#print 'deb:polyline.init.ENDinit:----------------' #------------



	def __repr__(self):
		return "%s: layer - %s, points - %s" %(self.__class__.__name__, self.layer, self.points)



	def doubles_out(self, settings, d_points):
		"""routine to sort out of double.vertices-----------------------------
		"""
		minimal_dist =  settings.var['dist_min'] * 0.1
		dv_count = 0
		temp_points = []
		for i in xrange(len(d_points)-1):
			point = d_points[i]
			point2 = d_points[i+1]
			#print 'deb:double.vertex p1,p2', point, point2 #------------------------
			delta = Mathutils.Vector(point2.loc) - Mathutils.Vector(point.loc)
			if delta.length > minimal_dist:
				 temp_points.append(point)
			else:
				dv_count+=1
		#print 'deb:drawPoly2d double.vertex sort out! count=', dv_count #------------------------
		temp_points.append(d_points[-1])  #------ incl. last vertex -------------
		#if self.closed: temp_points.append(d_points[1])  #------ loop start vertex -------------
		d_points = temp_points   #-----vertex.list without "double.vertices"
		#print 'deb:drawPoly2d d_pointsList =after DV-outsorting=====:\n ', d_points #------------------------
		return d_points


	def tribles_out(self, settings, d_points):
		"""routine to sort out of three_in_place.vertices-----------------------------
		"""
		minimal_dist = settings.var['dist_min'] * 0.1
		dv_count = 0
		temp_points = []
		for i in xrange(len(d_points)-2):
			point1 = d_points[i]
			point2 = d_points[i+1]
			point3 = d_points[i+2]
			#print 'deb:double.vertex p1,p2', point, point2 #------------------------
			delta12 = Mathutils.Vector(point2.loc) - Mathutils.Vector(point1.loc)
			delta23 = Mathutils.Vector(point3.loc) - Mathutils.Vector(point2.loc)
			if delta12.length < minimal_dist and delta23.length < minimal_dist:
				dv_count+=1
			else:
				temp_points.append(point1)
		#print 'deb:drawPoly2d double.vertex sort out! count=', dv_count #------------------------
		point1 = d_points[-2]
		point2 = d_points[-1]
		delta12 = Mathutils.Vector(point2.loc) - Mathutils.Vector(point1.loc)
		if delta12.length > minimal_dist:
			temp_points.append(d_points[-2])  #------ incl. 2last vertex -------------
		temp_points.append(d_points[-1])  #------ incl. 1last vertex -------------
		#if self.closed: temp_points.append(d_points[1])  #------ loop start vertex -------------
		d_points = temp_points   #-----vertex.list without "double.vertices"
		#print 'deb:drawPoly2d d_pointsList =after DV-outsorting=====:\n ', d_points #------------------------
		return d_points


	def draw(self, settings):   #-------------%%%% DRAW POLYLINE %%%---------------
		"""for POLYLINE: generate Blender_geometry.
		"""
		#print 'deb:drawPOLYLINE.START:----------------' #------------------------
		#print 'deb:POLYLINEdraw self.pltype:', self.pltype #------------------------
		#print 'deb:POLYLINEdraw self.points:\n', self.points #------------------------
		ob = []
		#---- 3dPolyFace - mesh with free topology
		if self.pltype=='plface' and settings.drawTypes['plmesh']:
			ob = self.drawPlFace(settings)
		#---- 3dPolyMesh - mesh with ortogonal topology
		elif self.pltype=='plmesh' and settings.drawTypes['plmesh']:
			ob = self.drawPlMesh(settings)

		#---- 2dPolyline - plane polyline with arc/wide/thic segments
		elif self.pltype=='poly2d' and settings.drawTypes['polyline']:
			if settings.var['plines_as'] in [5,6]: # and self.spline:
				ob = self.drawPolyCurve(settings)
			else:
				ob = self.drawPoly2d(settings)

		#---- 3dPolyline - non-plane polyline (thin segments = without arc/wide/thic)
		elif self.pltype=='poly3d' and settings.drawTypes['pline3']:
			if settings.var['plines3_as'] in [5,6]: # and self.spline:
				ob = self.drawPolyCurve(settings)
			else:
				ob = self.drawPoly2d(settings)

		#---- Spline - curved polyline (thin segments = without arc/wide/thic)
		elif self.pltype=='spline' and settings.drawTypes['spline']:
			if settings.var['splines_as'] in [5,6]:
				ob = self.drawPolyCurve(settings)
			else:
				ob = self.drawPoly2d(settings)
		return ob


	def drawPlFace(self, settings):  #---- 3dPolyFace - mesh with free topology
		"""Generate the geometery of polyface.
		"""
		#print 'deb:drawPlFace.START:----------------' #------------------------
		points = []
		faces = []
		#print 'deb:len of pointsList ====== ', len(self.points) #------------------------
		for point in self.points:
			if point.face:
				faces.append(point.face)
			else:
				points.append(point.loc)

		if settings.var['plmesh_flip']:  # ----------------------
			for face in faces:
				face.reverse()
				face = [face[-1]] + face[:-1]

		#print 'deb:drawPlFace: len of points_list:\n', len(points)  #-----------------------
		#print 'deb:drawPlFace: len of faces_list:\n', len(faces)  #-----------------------
		#print 'deb:drawPlFace: points_list:\n', points  #-----------------------
		#print 'deb:drawPlFace: faces_list:\n', faces  #-----------------------
		obname = 'pf_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]
		me = Mesh.New(obname)		  # create a new mesh
		ob = SCENE.objects.new(me) # create a new mesh_object
		me.verts.extend(points) # add vertices to mesh
		me.faces.extend(faces)   # add faces to the mesh
		if settings.var['normals_out']:  # ----------------------
			#me.flipNormals()
			me.recalcNormals(0)
			#me.update()
		#print 'deb:drawPlFace: len of me.faces:\n', len(me.faces)  #-----------------------

		if settings.var['meshSmooth_on']:  # ----------------------
			for i in xrange(len(me.faces)):
				me.faces[i].smooth = True
			#me.Mode(AUTOSMOOTH)
		transform(self.extrusion, 0, ob)
		#print 'deb:drawPlFace.END:----------------' #------------------------
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
		me = Mesh.New(obname)		  # create a new mesh
		ob = SCENE.objects.new(me) # create a new mesh_object
		me.verts.extend([point.loc for point in self.points]) # add vertices to mesh
		me.faces.extend(faces)   # add faces to the mesh
		if settings.var['normals_out']:  # ----------------------
			#me.flipNormals()
			me.recalcNormals(0)
			#me.update()
		if settings.var['meshSmooth_on']:  # ----------------------
			for i in xrange(len(faces)):
				me.faces[i].smooth = True
			#me.Mode(AUTOSMOOTH)

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

		if self.spline: pline_typ = 'ps'	# Polyline-NURBSpline
		elif self.curved: pline_typ = 'pc'	# Polyline-BezierCurve
		else: pline_typ = 'pl'				# Polyline classic
		obname = '%s_%s' %(pline_typ, self.layer)  # create object_name from layer name
		obname = obname[:MAX_NAMELENGTH]
		d_points = []

		if settings.var['Z_force_on']:
			self.elevation = settings.var['Z_elev']
			for point in self.points:
				point.loc[2] = self.elevation
				d_points.append(point)
		else: #for DXFr10-format: update all points[].loc[2] == None -> 0.0 
			for point in self.points:
				if point.loc[2] == None:
					point.loc[2] = self.elevation
				d_points.append(point)

		#d_points = self.tribles_out(settings, d_points)
		#d_points = self.doubles_out(settings, d_points)
		#print 'deb:drawPolyCurve d_pointsList =after DV-outsorting=====:\n ', d_points #------------------------

		thic = set_thick(self.thic, settings)
		if thic != 0.0:   #hack: Blender<2.45 curve-extrusion
			LocZ = d_points[0].loc[2]
			temp_points = []
			for point in d_points:
				point.loc[2] = 0.0
				temp_points.append(point)
			d_points = temp_points
		
		#print 'deb:polyline2dCurve.draw d_points=', d_points  #---------------
		pline = Curve.New(obname)   # create new curve data
		#pline.setResolu(24) #--todo-----						

		if self.spline:  # NURBSplines-----OK-----
			#print 'deb:polyline2dCurve.draw self.spline!' #---------------
			nurbs_points = []
			for d in d_points:
				pkt = d.loc
				pkt.append(d.weight)
				nurbs_points.append(pkt)
			firstpoint = nurbs_points[0]
			curve = pline.appendNurb(firstpoint)
			curve.setType(4) # set curve_type NURBS
			print 'deb: dir(curve):', dir(curve[-1]) #----------------
			for point in nurbs_points[1:]:
				curve.append(point)
				#TODO: what is the trick for bevel radius? curve[-1].radius = 1.0
			if self.closed:
				curve.flagU = 1+0 # Set curve cyclic=close and uni
			else:
				curve.flagU = 0+2 # Set curve not cyclic=open
			try: curve.orderU = 5 # works only with >2.46svn080625
			except AttributeError: pass
			#print 'deb: dir(curve):', dir(curve) #----------------

		elif  self.curved:  #--SPLINE as Bezier-curves---wip------
			#print 'deb:polyline2dCurve.draw self.curved!' #---------------
			begtangent, endtangent = None, None
			if d_points[0].tangent:
				begtangent = d_points[0]
				d_points = d_points[1:]
			if d_points[-1].tangent:
				endtangent = d_points[-1]
				d_points = d_points[:-1]
			curve = pline.appendNurb(BezTriple.New(d_points[0]))
			for p in d_points[1:]:
				curve.append(BezTriple.New(p))
			for point in curve:
				point.handleTypes = [AUTO, AUTO]
				point.radius = 1.0
			#curve.setType(1) #Bezier curve
			if self.closed:
				curve.flagU = 5 #1 # Set curve cyclic=close
			else:
				curve.flagU = 4 #0 # Set curve not cyclic=open
				if begtangent:
					#print 'deb:polyline2dCurve.draw curve[0].vec:', curve[0].vec #-----
					#print 'deb:polyline2dCurve.draw begtangent:', begtangent #-----
					p0h1,p0,p0h2 = curve[0].vec 
					p0h1 = [p0h1[i]+begtangent[i] for i in range(3)]
					curve.__setitem__(0,BezTriple.New(p0h1+p0+p0h2))
				curve[0].handleTypes = [FREE, ALIGN]   #remi--todo-----
				curve[0].radius = 1.0
				if endtangent:
					#print 'deb:polyline2dCurve.draw curve[-1].vec:', curve[-1].vec #-----
					#print 'deb:polyline2dCurve.draw endtangent:', endtangent #-----
					p0h1,p0,p0h2 = curve[-1].vec 
					p0h2 = [p0h2[i]+endtangent[i] for i in range(3)]
					#print 'deb:drawPlineCurve: p0h2:', p0h2 #----------
					curve.__setitem__(-1,BezTriple.New(p0h1+p0+p0h2))
					#print 'deb:polyline2dCurve.draw curve[-1].vec:', curve[-1].vec #-----
				curve[-1].handleTypes = [ALIGN, FREE]   #remi--todo-----
				curve[-1].radius = 1.0



		else:	#-- only straight line- and arc-segments----OK------
			#print 'deb:polyline2dCurve.draw curve:', curve #-----
			points = []
			arc_res = settings.var['curve_arc']
			prevHandleType = VECT
			#d_points.append(d_points[0])  #------ first vertex added at the end of list --------
			#curve.setType(0) #polygon_type of Blender_curve
			for i in xrange(len(d_points)):
				point1 = d_points[i]
				#point2 = d_points[i+1]
				#----- optimised Bezier-Handles calculation --------------------------------
				#print 'deb:drawPlineCurve: i:', i #---------
				if point1.bulge and not (i == len(d_points)-1 and point1.bulge and not self.closed):
					if i == len(d_points)-1: point2 = d_points[0]
					else: point2 = d_points[i+1]


					# calculate additional points for bulge
					VectorTriples = calcBulge(point1, point2, arc_res, triples=True)

					if prevHandleType == FREE:
						#print 'deb:drawPlineCurve: VectorTriples[0]:', VectorTriples[0] #---------
						VectorTriples[0][:3] = prevHandleVect
						#print 'deb:drawPlineCurve: VectorTriples[0]:', VectorTriples[0] #---------

					if i == 0: curve = pline.appendNurb(BezTriple.New(VectorTriples[0]))
					else: curve.append(BezTriple.New(VectorTriples[0]))
					curve[-1].handleTypes = [prevHandleType, FREE]
					curve[-1].radius = 1.0

					for p in VectorTriples[1:-1]:
						curve.append(BezTriple.New(p))
						curve[-1].handleTypes = [FREE, FREE]
						curve[-1].radius = 1.0

					prevHandleVect = VectorTriples[-1][:3]
					prevHandleType = FREE
					#print 'deb:drawPlineCurve: prevHandleVect:', prevHandleVect #---------
				else:
					#print 'deb:drawPlineCurve: else' #----------
					if prevHandleType == FREE:
						VectorTriples = prevHandleVect + list(point1) + list(point1)
						#print 'deb:drawPlineCurve: VectorTriples:', VectorTriples #---------
						curve.append(BezTriple.New(VectorTriples))
						curve[-1].handleTypes = [FREE, VECT]
						prevHandleType = VECT
						curve[-1].radius = 1.0
					else:
						if i == 0: curve = pline.appendNurb(BezTriple.New(point1.loc))
						else: curve.append(BezTriple.New(point1.loc))
						curve[-1].handleTypes = [VECT, VECT]
						curve[-1].radius = 1.0
					#print 'deb:drawPlineCurve: curve[-1].vec[0]', curve[-1].vec[0] #----------

			if self.closed:
				curve.flagU = 1 # Set curve cyclic=close
				if prevHandleType == FREE:
					#print 'deb:drawPlineCurve:closed curve[0].vec:', curve[0].vec #----------
					#print 'deb:drawPlineCurve:closed curve[0].handleTypes:', curve[0].handleTypes #----------
					prevHandleType2 = curve[0].handleTypes[1]
					p0h1,p0,p0h2 = curve[0].vec 
					#print 'deb:drawPlineCurve:closed p0h1:', p0h1 #----------
					p0h1 = prevHandleVect
					#p0h1 = [0,0,0]
					#print 'deb:drawPlineCurve:closed p0h1:', p0h1 #----------
					#curve[0].vec = [p0h1,p0,p0h2]
					curve.__setitem__(0,BezTriple.New(p0h1+p0+p0h2))

					curve[0].handleTypes = [FREE,prevHandleType2]
					curve[0].radius = 1.0
					#print 'deb:drawPlineCurve:closed curve[0].vec:', curve[0].vec #----------
					#print 'deb:drawPlineCurve:closed curve[0].handleTypes:', curve[0].handleTypes #----------
				else: 
					curve[0].handleTypes[0] = VECT
					curve[0].radius = 1.0
			else: 
				curve.flagU = 0 # Set curve not cyclic=open

		if settings.var['fill_on']:
			pline.setFlag(6) # 2+4 set top and button caps
		else:
			pline.setFlag(pline.getFlag() & ~6) # dont set top and button caps

		pline.setResolu(settings.var['curve_res'])
		pline.update()
		ob = SCENE.objects.new(pline) # create a new curve_object

		if thic != 0.0: #hack: Blender<2.45 curve-extrusion
			thic = thic * 0.5
			pline.setExt1(1.0)  # curve-extrusion accepts only (0.0 - 2.0)
			ob.LocZ = thic + LocZ

		transform(self.extrusion, 0, ob)
		if thic != 0.0:
			ob.SizeZ *= abs(thic)

		#print 'deb:polyline2dCurve.draw.END:----------------' #-----
		return ob


	def drawPoly2d(self, settings):  #---- 2dPolyline - plane lines/arcs with wide/thic
		"""Generate the geometery of regular polyline.
		"""
		#print 'deb:polyline2d.draw.START:----------------' #------------------------
		points = []
		d_points = []
		swidths = []
		ewidths = []
		swidth_default = self.swidth #default start width of POLYLINEs segments
		ewidth_default = self.ewidth #default end width of POLYLINEs segments
		#print 'deb:drawPoly2d self.swidth=', self.swidth #------------------------
		thic = set_thick(self.thic, settings)
		if self.spline: pline_typ = 'ps'
		elif self.curved: pline_typ = 'pc'
		else: pline_typ = 'pl'
		obname = '%s_%s' %(pline_typ, self.layer)  # create object_name from layer name
		obname = obname[:MAX_NAMELENGTH]

		if len(self.points) < 2:
			#print 'deb:drawPoly2d exit, cause POLYLINE has less than 2 vertices' #---------
			return
		
		if settings.var['Z_force_on']:
			self.elevation = settings.var['Z_elev']
			for point in self.points:
				point.loc[2] = self.elevation
				d_points.append(point)
		else: #for DXFr10-format: update all non-existing LocZ points[].loc[2] == None -> 0.0 elevation
			for point in self.points:
				if point.loc[2] == None:
					point.loc[2] = self.elevation
				d_points.append(point)
		#print 'deb:drawPoly2d len of d_pointsList ====== ', len(d_points) #------------------------
		#print 'deb:drawPoly2d d_pointsList ======:\n ', d_points #------------------------


		#if closed polyline, add duplic of the first vertex at the end of pointslist
		if self.closed:  #new_b8
			if d_points[-1].loc != d_points[0].loc: # if not equal, then set the first at the end of pointslist
				d_points.append(d_points[0])
		else:
			if d_points[-1].loc == d_points[0].loc: # if equal, then set to closed, and modify the last point
				d_points[-1] = d_points[0]
				self.closed = True
		#print 'deb:drawPoly2d len of d_pointsList ====== ', len(d_points) #------------------------
		#print 'deb:drawPoly2d d_pointsList ======:\n ', d_points #------------------------

		d_points = self.doubles_out(settings, d_points)
		#print 'deb:drawPolyCurve d_pointsList =after DV-outsorting=====:\n ', d_points #------------------------

		#print 'deb:drawPoly2d len of d_pointsList ====== ', len(d_points) #------------------------
		if len(d_points) < 2:  #if too few vertex, then return
			#print 'deb:drawPoly2d corrupted Vertices' #---------
			return

		# analyze of straight- and bulge-segments
		# generation of additional points for bulge segments
		arc_res = settings.var['arc_res']/sqrt(settings.var['arc_rad'])
		wide_segment_exist = False
		bulg_points = []  # for each point set None (or center for arc-subPoints)
		for i in xrange(len(d_points)-1):
			point1 = d_points[i]
			point2 = d_points[i+1]
			#print 'deb:drawPoly2d_bulg tocalc.point1:', point1 #------------------------
			#print 'deb:drawPoly2d_bulg tocalc.point2:', point2 #------------------------

			swidth = point1.swidth
			ewidth = point1.ewidth
			#print 'deb:drawPoly2d point1.swidth=', swidth #------------------------
			if swidth == None: swidth = swidth_default
			if ewidth == None: ewidth = ewidth_default
			if swidth != 0.0 or ewidth != 0.0: wide_segment_exist = True
			#print 'deb:drawPoly2d vertex_swidth=', swidth #------------------------

			if settings.var['width_force']:  # force minimal width for thin segments
				width_min = settings.var['width_min']
				if swidth < width_min: swidth = width_min
				if ewidth < width_min: ewidth = width_min
				if not settings.var['width_on']:  # then force minimal width for all segments
					swidth = width_min
					ewidth = width_min

			#if point1.bulge and (i < (len(d_points)-1) or self.closed):
			if point1.bulge and i < (len(d_points)-1): #10_b8
				verts, center = calcBulge(point1, point2, arc_res) #calculate additional points for bulge
				points.extend(verts)
				delta_width = (ewidth - swidth) / len(verts)
				width_list = [swidth + (delta_width * ii) for ii in xrange(len(verts)+1)]
				swidths.extend(width_list[:-1])
				ewidths.extend(width_list[1:])
				bulg_list = [center for ii in xrange(len(verts))]
				#the last point in bulge has index False for better indexing of bulg_end!
				bulg_list[-1] = None
				bulg_points.extend(bulg_list)

			else:
				points.append(point1.loc)
				swidths.append(swidth)
				ewidths.append(ewidth)
				bulg_points.append(None)
		points.append(d_points[-1].loc)


		#--calculate width_vectors: left-side- and right-side-points ----------------
		# 1.level:IF width  ---------------------------------------
		if (settings.var['width_on'] and wide_segment_exist) or settings.var['width_force']:
			#new_b8 points.append(d_points[0].loc)  #temporarly add first vertex at the end (for better loop)
			dist_min05 = 0.5 * settings.var['dist_min'] #minimal width for zero_witdh
			
			pointsLs = []   # list of left-start-points
			pointsLe = []   # list of left-end-points
			pointsRs = []   # list of right-start-points
			pointsRe = []   # list of right-end-points
			pointsW  = []   # list of all border-points
			#rotMatr90 = Mathutils.Matrix(rotate 90 degree around Z-axis) = normalvectorXY
			rotMatr90 = Mathutils.Matrix([0, -1, 0], [1, 0, 0], [0, 0, 1])
			bulg_in = False
			last_bulg_point = False
			for i in xrange(len(points)-1):
				point1 = points[i]
				point2 = points[i+1]
				point1vec = Mathutils.Vector(point1)
				point2vec = Mathutils.Vector(point2)
				swidth05 = swidths[i] * 0.5
				ewidth05 = ewidths[i] * 0.5
				if swidth05 == 0: swidth05 = dist_min05
				if ewidth05 == 0: ewidth05 = dist_min05
				normal_vector = rotMatr90 * (point2vec-point1vec).normalize()
				if last_bulg_point:
					last_bulg_point = False
					bulg_in = True
				elif bulg_points[i] != None:
					centerVec = Mathutils.Vector(bulg_points[i])
					if bulg_points[i+1] == None: last_bulg_point = True
					bulg_in = True
				else: bulg_in = False

				if bulg_in:
					#makes clean intersections for arc-segments
					radius1vec = point1vec - centerVec
					radius2vec = point2vec - centerVec
					angle = Mathutils.AngleBetweenVecs(normal_vector, radius1vec)
					if angle < 90.0:
						normal_vector1 = radius1vec.normalize()
						normal_vector2 = radius2vec.normalize()
					else:	
						normal_vector1 = - radius1vec.normalize()
						normal_vector2 = - radius2vec.normalize()

					swidth05vec = swidth05 * normal_vector1
					ewidth05vec = ewidth05 * normal_vector2
					pointsLs.append(point1vec + swidth05vec) #vertex left start
					pointsRs.append(point1vec - swidth05vec) #vertex right start
					pointsLe.append(point2vec + ewidth05vec) #vertex left end
					pointsRe.append(point2vec - ewidth05vec) #vertex right end

				else:
					swidth05vec = swidth05 * normal_vector
					ewidth05vec = ewidth05 * normal_vector
					pointsLs.append(point1vec + swidth05vec) #vertex left start
					pointsRs.append(point1vec - swidth05vec) #vertex right start
					pointsLe.append(point2vec + ewidth05vec) #vertex left end
					pointsRe.append(point2vec - ewidth05vec) #vertex right end
	
			# additional last point is also calculated
			#pointsLs.append(pointsLs[0])
			#pointsRs.append(pointsRs[0])
			#pointsLe.append(pointsLe[0])
			#pointsRe.append(pointsRe[0])

			pointsLc, pointsRc = [], [] # lists Left/Right corners = intersection points

			# 2.level:IF width and corner-trim
			if settings.var['pl_trim_on']:  #optional clean corner-intersections
				# loop preset
				# set STARTpoints of the first point points[0]
				if not self.closed:
					pointsLc.append(pointsLs[0])
					pointsRc.append(pointsRs[0])
				else:
					pointsLs.append(pointsLs[0])
					pointsRs.append(pointsRs[0])
					pointsLe.append(pointsLe[0])
					pointsRe.append(pointsRe[0])
					points.append(points[0])
				vecL3, vecL4 = pointsLs[0], pointsLe[0]
				vecR3, vecR4 = pointsRs[0], pointsRe[0]
				lenL = len(pointsLs)-1
				#print 'deb:drawPoly2d pointsLs():\n',  pointsLs  #----------------
				#print 'deb:drawPoly2d lenL, len.pointsLs():', lenL,',', len(pointsLs)  #----------------
				bulg_in = False
				last_bulg_point = False

				# LOOP: makes (ENDpoints[i],STARTpoints[i+1])
				for i in xrange(lenL):
					if bulg_points[i] != None:
						if bulg_points[i+1] == None: #makes clean intersections for arc-segments
							last_bulg_point = True
						if not bulg_in:
							bulg_in = True
							#pointsLc.extend((points[i], pointsLs[i]))
							#pointsRc.extend((points[i], pointsRs[i]))
					vecL1, vecL2 = vecL3, vecL4
					vecR1, vecR2 = vecR3, vecR4
					vecL3, vecL4 = pointsLs[i+1], pointsLe[i+1]
					vecR3, vecR4 = pointsRs[i+1], pointsRe[i+1]
					#compute left- and right-cornerpoints
					#cornerpointL = Geometry.LineIntersect2D(vec1, vec2, vec3, vec4)
					cornerpointL = Mathutils.LineIntersect(vecL1, vecL2, vecL3, vecL4)
					cornerpointR = Mathutils.LineIntersect(vecR1, vecR2, vecR3, vecR4)
					#print 'deb:drawPoly2d cornerpointL: ', cornerpointL  #-------------
					#print 'deb:drawPoly2d cornerpointR: ', cornerpointR  #-------------

					# IF not cornerpoint THEN check if identic start-endpoints (=collinear segments)
					if cornerpointL == None or cornerpointR == None:
						if vecL2 == vecL3 and vecR2 == vecR3:
							#print 'deb:drawPoly2d pointVec: ####### identic ##########' #----------------
							pointsLc.append(pointsLe[i])
							pointsRc.append(pointsRe[i])
						else:
							pointsLc.extend((pointsLe[i],points[i+1],pointsLs[i+1]))
							pointsRc.extend((pointsRe[i],points[i+1],pointsRs[i+1]))
					else:
						cornerpointL = cornerpointL[0] # because Mathutils.LineIntersect() -> (pkt1,pkt2)
						cornerpointR = cornerpointR[0]
						#print 'deb:drawPoly2d cornerpointL: ', cornerpointL  #-------------
						#print 'deb:drawPoly2d cornerpointR: ', cornerpointR  #-------------
						pointVec0 = Mathutils.Vector(points[i])
						pointVec = Mathutils.Vector(points[i+1])
						pointVec2 = Mathutils.Vector(points[i+2])
						#print 'deb:drawPoly2d pointVec0: ', pointVec0  #-------------
						#print 'deb:drawPoly2d pointVec: ', pointVec  #-------------
						#print 'deb:drawPoly2d pointVec2: ', pointVec2  #-------------
						# if diststance(cornerL-center-cornerR) < limiter * (seg1_endWidth + seg2_startWidth)
						max_cornerDist = (vecL2 - vecR2).length + (vecL3 - vecR3).length
						is_cornerDist = (cornerpointL - pointVec).length + (cornerpointR - pointVec).length
						#corner_angle = Mathutils.AngleBetweenVecs((pointVec0 - pointVec),(pointVec - pointVec2))
						#print 'deb:drawPoly2d corner_angle: ', corner_angle  #-------------
						#print 'deb:drawPoly2d max_cornerDist, is_cornerDist: ', max_cornerDist, is_cornerDist  #-------------
						#if abs(corner_angle) < 90.0:
						# intersection --------- limited by TRIM_LIMIT (1.0 - 5.0)
						if is_cornerDist < max_cornerDist * settings.var['pl_trim_max']:
							# clean corner intersection
							pointsLc.append(cornerpointL)
							pointsRc.append(cornerpointR)
						else:
							pointsLc.extend((pointsLe[i],points[i+1],pointsLs[i+1]))
							pointsRc.extend((pointsRe[i],points[i+1],pointsRs[i+1]))
				if not self.closed:
					pointsLc.append(pointsLe[-1])
					pointsRc.append(pointsRe[-1])

			# 2.level:IF width but no-trim
			else:
				# loop preset
				# set STARTpoints of the first point points[0]
				if not self.closed:
					pointsLc.append(pointsLs[0])
					pointsRc.append(pointsRs[0])
				else:
					pointsLs.append(pointsLs[0])
					pointsRs.append(pointsRs[0])
					pointsLe.append(pointsLe[0])
					pointsRe.append(pointsRe[0])
					points.append(points[0])
				vecL3, vecL4 = pointsLs[0], pointsLe[0]
				vecR3, vecR4 = pointsRs[0], pointsRe[0]
				lenL = len(pointsLs)-1
				#print 'deb:drawPoly2d pointsLs():\n',  pointsLs  #----------------
				#print 'deb:drawPoly2d lenL, len.pointsLs():', lenL,',', len(pointsLs)  #----------------
				bulg_in = False
				last_bulg_point = False

				# LOOP: makes (ENDpoints[i],STARTpoints[i+1])
				for i in xrange(lenL):
					vecL1, vecL2 = vecL3, vecL4
					vecR1, vecR2 = vecR3, vecR4
					vecL3, vecL4 = pointsLs[i+1], pointsLe[i+1]
					vecR3, vecR4 = pointsRs[i+1], pointsRe[i+1]
					if bulg_points[i] != None:
						#compute left- and right-cornerpoints
						cornerpointL = Mathutils.LineIntersect(vecL1, vecL2, vecL3, vecL4)
						cornerpointR = Mathutils.LineIntersect(vecR1, vecR2, vecR3, vecR4)
						pointsLc.append(cornerpointL[0])
						pointsRc.append(cornerpointR[0])
					else: # IF non-bulg
						pointsLc.extend((pointsLe[i],points[i+1],pointsLs[i+1]))
						pointsRc.extend((pointsRe[i],points[i+1],pointsRs[i+1]))
				if not self.closed:
					pointsLc.append(pointsLe[-1])
					pointsRc.append(pointsRe[-1])

			len1 = len(pointsLc)
			#print 'deb:drawPoly2d len1:', len1  #-----------------------
			#print 'deb:drawPoly2d len1 len(pointsLc),len(pointsRc):', len(pointsLc),len(pointsRc)  #-----------------------
			pointsW = pointsLc + pointsRc  # all_points_List = left_side + right_side
			#print 'deb:drawPoly2d pointsW():\n',  pointsW  #----------------

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

				faces = f_left + f_right + f_bottom + f_top + f_start + f_end
				#faces = f_bottom + f_top
				#faces = f_left + f_right + f_start + f_end
				#print 'deb:faces_list:\n', faces  #-----------------------
				if M_OBJ: obname, me, ob = makeNewObject()
				else: 
					me = Mesh.New(obname)		# create a new mesh
					ob = SCENE.objects.new(me) # create a new mesh_object
				me.verts.extend(pointsW)		# add vertices to mesh
				me.faces.extend(faces)  # add faces to the mesh

				# each MeshSide becomes vertexGroup for easier material assignment ---------------------
				# The mesh must first be linked to an object so the method knows which object to update.
				# This is because vertex groups in Blender are stored in the object -- not in the mesh,
				# which may be linked to more than one object.
				if settings.var['vGroup_on'] and not M_OBJ:
					# each MeshSide becomes vertexGroup for easier material assignment ---------------------
					replace = Mesh.AssignModes.REPLACE  #or .AssignModes.ADD
					vg_left, vg_right, vg_top, vg_bottom = [], [], [], []
					for v in f_left: vg_left.extend(v)
					for v in f_right: vg_right.extend(v)
					for v in f_top: vg_top.extend(v)
					for v in f_bottom: vg_bottom.extend(v)
					me.addVertGroup('side.left')  ; me.assignVertsToGroup('side.left',  vg_left, 1.0, replace)
					me.addVertGroup('side.right') ; me.assignVertsToGroup('side.right', vg_right, 1.0, replace)
					me.addVertGroup('side.top')   ; me.assignVertsToGroup('side.top',   vg_top, 1.0, replace)
					me.addVertGroup('side.bottom'); me.assignVertsToGroup('side.bottom',vg_bottom, 1.0, replace)
					if not self.closed:
						me.addVertGroup('side.start'); me.assignVertsToGroup('side.start', f_start[0], 1.0, replace)
						me.addVertGroup('side.end')  ; me.assignVertsToGroup('side.end',   f_end[0],   1.0, replace)

				if settings.var['meshSmooth_on']:  # left and right side become smooth ----------------------
					#if self.spline or self.curved:
					smooth_len = len(f_left) + len(f_right)
					for i in xrange(smooth_len):
						me.faces[i].smooth = True
					#me.Modes(AUTOSMOOTH)

			# 2.level:IF width, but no-thickness  ---------------------
			else:
				faces = []
				faces = [[num, len1+num, len1+num+1, num+1] for num in xrange(len1 - 1)]
				if self.closed:
					faces.append([len1, 0, len1-1, len1+len1-1])
				if M_OBJ: obname, me, ob = makeNewObject()
				else: 
					me = Mesh.New(obname)		# create a new mesh
					ob = SCENE.objects.new(me) # create a new mesh_object
				me.verts.extend(pointsW)		# add vertices to mesh
				me.faces.extend(faces)  # add faces to the mesh


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
			if M_OBJ: obname, me, ob = makeNewObject()
			else: 
				me = Mesh.New(obname)		# create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
			me.verts.extend(points)   # add vertices to mesh
			me.faces.extend(faces)  # add faces to the mesh

			if settings.var['meshSmooth_on']:  # left and right side become smooth ----------------------
				#if self.spline or self.curved:
				for i in xrange(len(faces)):
					me.faces[i].smooth = True
				#me.Modes(AUTOSMOOTH)

		# 1.level:IF no-width and no-thickness  ---------------------
		else:
			edges = [[num, num+1] for num in xrange(len(points)-1)]
			if self.closed:
				edges.append([len(points)-1, 0])
			if M_OBJ: obname, me, ob = makeNewObject()
			else: 
				me = Mesh.New(obname)		# create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
			me.verts.extend(points)   # add vertices to mesh
			me.edges.extend(edges)  # add edges to the mesh

		transform(self.extrusion, 0, ob)
		#print 'deb:polyline.draw.END:----------------' #-----------------------
		return ob




class Vertex(object):  #-----------------------------------------------------------------
	"""Generic vertex object used by POLYLINEs, (and maybe others).
	also used by class_LWPOLYLINEs but without obj-parameter
	"""

	def __init__(self, obj=None):
		"""Initializes vertex data.

		The optional obj arg is an entity object of type vertex.
		"""
		#print 'deb:Vertex.init.START:----------------' #-----------------------
		self.loc = [0,0,0]
		self.face = []
		self.swidth = None #0
		self.ewidth = None #0
		self.bulge = 0
		self.tangent = False
		self.weight =  1.0
		if obj is not None:
			if not obj.type == 'vertex':
				raise TypeError, "Wrong type %s for vertex object!" %obj.type
			self.type = obj.type
#			self.data = obj.data[:]
			self.get_props(obj)
		else:
			pass
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
		self.curved_t = self.flags&2   # Bezier-curve-fit:tangent exists
		self.spline = self.flags&8   # NURBSpline-fit:additional-vertex
		self.spline_c = self.flags&16  # NURBSpline-fit:control-vertex
		self.poly3d = self.flags&32  # polyline3d:control-vertex
		self.plmesh = self.flags&64  # polymesh3d:control-vertex
		self.plface = self.flags&128 # polyface

		# if PolyFace.Vertex with Face_definition
		if self.curved_t:
			self.curve_tangent =  getit(data, 50, None) # curve_tangent
			if not self.curve_tangent==None:
				self.tangent = True
		#elif self.spline_c: # NURBSpline:control-vertex
		#	self.weight =  getit(data, 41, 1.0) # weight od control point

		elif self.plface and not self.plmesh:
			v1 = getit(data, 71, 0) # polyface:Face.vertex 1.
			v2 = getit(data, 72, 0) # polyface:Face.vertex 2.
			v3 = getit(data, 73, 0) # polyface:Face.vertex 3.
			v4 = getit(data, 74, None) # polyface:Face.vertex 4.
			self.face = [abs(v1)-1,abs(v2)-1,abs(v3)-1]
			if v4 != None:
				if abs(v4) != abs(v1):
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



class Spline(Polyline):  #-----------------------------------------------------------------
	"""Class for objects representing dxf SPLINEs.
	"""
	"""Expects an entity object of type spline as input.
100 - Subclass marker (AcDbSpline)
210,220, 230  - Normal vector (omitted if the spline is nonplanar) X,Y,Z values of normal vector
70 - Spline flag (bit coded):
  1 = Closed spline
  2 = Periodic spline
  4 = Rational spline
  8 = Planar
 16 = Linear (planar bit is also set)
71 - Degree of the spline curve
72 - Number of knots
73 - Number of control points
74 - Number of fit points (if any)
42 - Knot tolerance (default = 0.0000001)
43 - Control-point tolerance (default = 0.0000001)
44 - Fit tolerance (default = 0.0000000001)
12,22,32 - Start tangent--may be omitted (in WCS). X,Y,Z values of start tangent--may be omitted (in WCS).
13,23, 33 - End tangent--may be omitted (in WCS). X,Y,Z values of end tangent--may be omitted (in WCS)
40 - Knot value (one entry per knot)
41 - Weight (if not 1); with multiple group pairs, are present if all are not 1
10,20, 30  - Control points (in WCS) one entry per control point.
DXF: X value; APP: 3D point, Y and Z values of control points (in WCS) (one entry per control point)
11,21, 31 - Fit points (in WCS) one entry per fit point.
 X,Y,Z values of fit points (in WCS) (one entry per fit point)
	"""
	def __init__(self, obj):
		#print 'deb:Spline.START:----------------' #------------------------
		if not obj.type == 'spline':
			raise TypeError, "Wrong type %s for spline object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		# required data
		self.num_points = obj.get_type(73)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)

		self.color_index = getit(obj, 62, BYLAYER)

		#self.elevation =  getit(obj, 30, 0)
		self.thic = 0 # getit(obj, 39, 0)

		width = 0
		self.swidth =  width # default start width
		self.ewidth =  width # default end width

		self.flags = getit(obj, 70, 0)
		self.closed = self.flags & 1   # closed spline
		self.period = self.flags & 2   # Periodic spline
		self.ration = self.flags & 4   # Rational spline
		self.planar = self.flags & 8   # Planar
		self.linear = self.flags & 16  # Linear (and Planar)

		self.curvNoFitted = False
		self.curvQuadrati = False
		self.curvCubicBsp = False
		self.curvBezier = False
		self.degree = getit(obj, 71, 0) # Degree of the spline curve
		if   self.degree == 0: self.curvNoFitted = True
		elif self.degree == 1: self.curvQuadrati = True
		elif self.degree == 2: self.curvCubicBsp = True
		#elif self.degree == 3: self.curvBezier = True
		#elif self.degree == 3: self.spline = True
	
		self.knotpk_len = getit(obj, 72, 0) # Number of knots
		self.ctrlpk_len = getit(obj, 73, 0) # Number of control points
		self.fit_pk_len = getit(obj, 74, 0) # Number of fit points (if any)

		#TODO: import SPLINE as Bezier curve directly, possible?
		#print 'deb:Spline self.fit_pk_len=', self.fit_pk_len #------------------------
		#self.fit_pk_len = 0 # temp for debug
		if self.fit_pk_len and settings.var['splines_as']==5:
			self.spline = False
			self.curved = True
		else:
			self.spline = True
			self.curved = False

		self.knotpk_tol = getit(obj, 42, 0.0000001) # Knot tolerance (default = 0.0000001)
		self.ctrlpk_tol = getit(obj, 43, 0.0000001) # Control-point tolerance (default = 0.0000001)
		self.fit_pk_tol = getit(obj, 44, 0.0000000001) # Fit tolerance (default = 0.0000000001)

		self.layer = getit(obj, 8, None)
		self.extrusion = get_extrusion(obj)

		self.pltype = 'spline'   # spline is a 2D- or 3D-polyline

		self.points = self.get_points(obj.data)
		#self.knots_val = self.get_knots_val(obj.data) # 40 - Knot value (one entry per knot)
		#self.knots_wgh = self.get_knots_wgh(obj.data) # 41 - Weight (default 1)

		#print 'deb:Spline obj.data:\n', obj.data #------------------------
		#print 'deb:Spline self.points:\n', self.points #------------------------
		#print 'deb:Spline.ENDinit:----------------' #------------------------


	def get_points(self, data):
		"""Gets points for a spline type object.

		Splines have fixed number of verts, and
		each vert can have a number of properties.
		Verts should be coded as
		10:xvalue
		20:yvalue
		for each vert
		"""
		point = None
		points = []
		pointend = None
		#point = Vertex()
		if self.spline: # NURBSpline definition
			for item in data:
				#print 'deb:Spline.get_points spilne_item:', item #------------------------
				if item[0] == 10:   # control point
					if point: points.append(point)
					point = Vertex()
					point.curved = True
					point.x = item[1]
				elif item[0] == 20: # 20 = y
					point.y = item[1]
				elif item[0] == 30: # 30 = z
					point.z = item[1]
				elif item[0] == 41: # 41 = weight
					point.weight = item[1]
					#print 'deb:Spline.get_points control point:', point #------------------------

		elif self.curved: # Bezier definition
			for item in data:
				#print 'deb:Spline.get_points curved_item:', item #------------------------
				if item[0] == 11:   # fit point
					if point: points.append(point)
					point = Vertex()
					point.tangent = False
					point.x = item[1]
				elif item[0] == 21: # 20 = y
					point.y = item[1]
				elif item[0] == 31: # 30 = z
					point.z = item[1]
					#print 'deb:Spline.get_points fit point:', point #------------------------

				elif item[0] == 12:   # start tangent
					if point: points.append(point)
					point = Vertex()
					point.tangent = True
					point.x = item[1]
				elif item[0] == 22: # = y
					point.y = item[1]
				elif item[0] == 32: # = z
					point.z = item[1]
					#print 'deb:Spline.get_points fit begtangent:', point #------------------------

				elif item[0] == 13:   # end tangent
					if point: points.append(point)
					pointend = Vertex()
					pointend.tangent = True
					pointend.x = item[1]
				elif item[0] == 23: # 20 = y
					pointend.y = item[1]
				elif item[0] == 33: # 30 = z
					pointend.z = item[1]
					#print 'deb:Spline.get_points fit endtangent:', pointend #------------------------
		points.append(point)
		if self.curved and pointend:
			points.append(pointend)
		#print 'deb:Spline points:\n', points #------------------------
		return points

	def __repr__(self):
		return "%s: layer - %s, points - %s" %(self.__class__.__name__, self.layer, self.points)

	

class LWpolyline(Polyline):  #-------------------------------------------------------------
	"""Class for objects representing dxf LWPOLYLINEs.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type lwpolyline as input.
		"""
		#print 'deb:LWpolyline.START:----------------' #------------------------
		if not obj.type == 'lwpolyline':
			raise TypeError, "Wrong type %s for polyline object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		# required data
		self.num_points = obj.get_type(90)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.elevation =  getit(obj, 38, 0)
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)
		width =  getit(obj, 43, 0)
		self.swidth =  width # default start width
		self.ewidth =  width # default end width
		#print 'deb:LWpolyline width=', width #------------------------
		#print 'deb:LWpolyline elevation=', self.elevation #------------------------
	
		self.flags = getit(obj, 70, 0)
		self.closed = self.flags&1 # byte coded, 1 = closed, 128 = plinegen

		self.layer = getit(obj, 8, None)
		self.extrusion = get_extrusion(obj)

		self.points = self.get_points(obj.data)

		self.pltype = 'poly2d'   # LW-polyline is a 2D-polyline
		self.spline = False
		self.curved = False

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
				point.z = self.elevation
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


class Text:  #-----------------------------------------------------------------
	"""Class for objects representing dxf TEXT.
	"""
	def __init__(self, obj):
		"""Expects an entity object of type text as input.
		"""
		if not obj.type == 'text':
			raise TypeError, "Wrong type %s for text object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

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

		#self.style = getit(obj, 7, 'STANDARD') # --todo---- Text style name (optional, default = STANDARD)

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

		self.layer = getit(obj, 8, None)
		self.loc1, self.loc2 = self.get_loc(obj)
		if self.loc2[0] != None and self.halignment != 5: 
			self.loc = self.loc2
		else:
			self.loc = self.loc1
		self.extrusion = get_extrusion(obj)


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
		if thic != 0.0: ob.SizeZ *= abs(thic)
		return ob


	
def set_thick(thickness, settings):
	"""Set thickness relative to settings variables.
	
	Set thickness relative to settings variables:
	'thick_on','thick_force','thick_min'.
	Accepted also minus values of thickness
	python trick: sign(x)=cmp(x,0)
	"""
	if settings.var['thick_force']:
		if settings.var['thick_on']:
			if abs(thickness) <  settings.var['thick_min']:
				thic = settings.var['thick_min'] * cmp(thickness,0)
			else: thic = thickness
		else: thic = settings.var['thick_min']
	else: 
		if settings.var['thick_on']: thic = thickness
		else: thic = 0.0
	return thic




class Mtext:  #-----------------------------------------------------------------
	"""Class for objects representing dxf MTEXT.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type mtext as input.
		"""
		if not obj.type == 'mtext':
			raise TypeError, "Wrong type %s for mtext object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		# required data
		self.height = obj.get_type(40)[0]
		self.width = obj.get_type(41)[0]
		self.alignment = obj.get_type(71)[0] # alignment 1=TL, 2=TC, 3=TR, 4=ML, 5=MC, 6=MR, 7=BL, 8=BC, 9=BR
		self.value = self.get_text(obj) # The text string value

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.color_index = getit(obj, 62, BYLAYER)
		self.rotation = getit(obj, 50, 0)  # radians

		self.width_factor = getit(obj, 42, 1) # Scaling factor along local x axis
		self.line_space = getit(obj, 44, 1) # percentage of default

		self.layer = getit(obj, 8, None)
		self.loc = self.get_loc(obj)
		self.extrusion = get_extrusion(obj)


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
	"""Class for objects representing dxf CIRCLEs.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type circle as input.
		"""
		if not obj.type == 'circle':
			raise TypeError, "Wrong type %s for circle object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		# required data
		self.radius = obj.get_type(40)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj, 8, None)
		self.loc = self.get_loc(obj)
		self.extrusion = get_extrusion(obj)



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
		width = 0.0
		if settings.var['lines_as'] == 4: # as thin_box
			thic = settings.var['thick_min']
			width = settings.var['width_min']
		if settings.var['lines_as'] == 3: # as thin cylinder
			cyl_rad = 0.5 * settings.var['width_min']

		if settings.var['lines_as'] == 5:  # draw CIRCLE as curve -------------
			arc_res = settings.var['curve_arc']
			#arc_res = 3
			start, end = 0.0, 360.0
			VectorTriples = calcArc(None, radius, start, end, arc_res, True)
			c = Curve.New(obname) # create new curve data
			curve = c.appendNurb(BezTriple.New(VectorTriples[0]))
			for p in VectorTriples[1:-1]:
				curve.append(BezTriple.New(p))
			for point in curve:
				point.handleTypes = [FREE, FREE]
				point.radius = 1.0
			curve.flagU = 1	 # 1 sets the curve cyclic=closed
			if settings.var['fill_on']:
				c.setFlag(6) # 2+4 set top and button caps
			else:
				c.setFlag(c.getFlag() & ~6) # dont set top and button caps

			c.setResolu(settings.var['curve_res'])
			c.update()

			#--todo-----to check---------------------------
			ob = SCENE.objects.new(c) # create a new curve_object
			ob.loc = tuple(self.loc)
			if thic != 0.0: #hack: Blender<2.45 curve-extrusion
				thic = thic * 0.5
				c.setExt1(1.0)  # curve-extrusion accepts only (0.0 - 2.0)
				ob.LocZ = thic + self.loc[2]
			transform(self.extrusion, 0, ob)
			if thic != 0.0:
				ob.SizeZ *= abs(thic)
			return ob

		else:  # draw CIRCLE as mesh -----------------------------------------------
			if M_OBJ: obname, me, ob = makeNewObject()
			else: 
				me = Mesh.New(obname)		# create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
			# set a number of segments in entire circle
			arc_res = settings.var['arc_res'] * sqrt(radius) / sqrt(settings.var['arc_rad'])
			start, end = 0.0 , 360.0
			verts = calcArc(None, radius, start, end, arc_res, False)
			verts = verts[:-1] #list without last point/edge (cause by circle it is equal to the first point)
			#print 'deb:circleDraw: verts:', verts  #--------------- 

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
				f_band = [[num, num+1, num+len1+1, num+len1] for num in xrange(len1 - 1)]
				#f_band = [[num, num+1, num+len1+1, num+len1] for num in xrange(len1)]
				f_band.append([len1 - 1, 0, len1, len1 + len1 -1])
				faces = f_band
				smooth_len = len(f_band)
				if settings.var['fill_on']:
					if thic < 0.0:
						verts.append([0,0,thic])  #center of top side
						verts.append([0,0,0])  #center of bottom side
					else:
						verts.append([0,0,0])  #center of bottom side
						verts.append([0,0,thic])  #center of top side
					center1 = len(verts)-2
					center2 = len(verts)-1
					f_bottom = [[num+1, num, center1] for num in xrange(len1 - 1)]
					f_bottom.append([0, len1 - 1, center1])
					f_top = [[num+len1, num+1+len1, center2] for num in xrange(len1 - 1)]
					f_top.append([len1-1+len1, 0+len1, center2])
					#print 'deb:circleDraw:verts:', verts  #---------------
					faces = f_band + f_bottom + f_top
					#print 'deb:circleDraw:faces:', faces  #---------------
				me.verts.extend(verts) # add vertices to mesh
				me.faces.extend(faces)  # add faces to the mesh

				if settings.var['meshSmooth_on']:  # left and right side become smooth ----------------------
					for i in xrange(smooth_len):
						me.faces[i].smooth = True
				# each MeshSide becomes vertexGroup for easier material assignment ---------------------
				if settings.var['vGroup_on'] and not M_OBJ:
					# each MeshSide becomes vertexGroup for easier material assignment ---------------------
					replace = Mesh.AssignModes.REPLACE  #or .AssignModes.ADD
					vg_band, vg_top, vg_bottom = [], [], []
					for v in f_band: vg_band.extend(v)
					me.addVertGroup('side.band')  ; me.assignVertsToGroup('side.band',  vg_band, 1.0, replace)

					if settings.var['fill_on']:
						for v in f_top: vg_top.extend(v)
						for v in f_bottom: vg_bottom.extend(v)
						me.addVertGroup('side.top')   ; me.assignVertsToGroup('side.top',   vg_top, 1.0, replace)
						me.addVertGroup('side.bottom'); me.assignVertsToGroup('side.bottom',vg_bottom, 1.0, replace)

			else: # if thic == 0
				if settings.var['fill_on']:
					len1 = len(verts)
					verts.append([0,0,0])  #center of circle
					center1 = len1
					faces = []
					faces.extend([[num, num+1, center1] for num in xrange(len1)])
					faces.append([len1-1, 0, center1])
					#print 'deb:circleDraw:verts:', verts  #---------------
					#print 'deb:circleDraw:faces:', faces  #---------------
					me.verts.extend(verts) # add vertices to mesh
					me.faces.extend(faces)  # add faces to the mesh
				else:
					me.verts.extend(verts) # add vertices to mesh
					edges = [[num, num+1] for num in xrange(len(verts))]
					edges[-1][1] = 0   # it points the "new" last edge to the first vertex
					me.edges.extend(edges)  # add edges to the mesh

			ob.loc = tuple(self.loc)
			transform(self.extrusion, 0, ob)
			return ob
			

class Arc:  #-----------------------------------------------------------------
	"""Class for objects representing dxf ARCs.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type arc as input.
		"""
		if not obj.type == 'arc':
			raise TypeError, "Wrong type %s for arc object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		# required data
		self.radius = obj.get_type(40)[0]
		self.start_angle = obj.get_type(50)[0]
		self.end_angle = obj.get_type(51)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj, 8, None)
		self.loc = self.get_loc(obj)
		self.extrusion = get_extrusion(obj)
		#print 'deb:Arc__init__: center, radius, start, end:\n', self.loc, self.radius, self.start_angle, self.end_angle  #---------



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
		#print 'deb:calcArcPoints:\n center, radius, start, end:\n', center, radius, start, end  #---------
		thic = set_thick(self.thic, settings)
		width = 0.0
		if settings.var['lines_as'] == 4: # as thin_box
			thic = settings.var['thick_min']
			width = settings.var['width_min']
		if settings.var['lines_as'] == 3: # as thin cylinder
			cyl_rad = 0.5 * settings.var['width_min']

		if settings.var['lines_as'] == 5:  # draw ARC as curve -------------
			arc_res = settings.var['curve_arc']
			triples = True
			VectorTriples = calcArc(None, radius, start, end, arc_res, triples)
			arc = Curve.New(obname) # create new curve data
			curve = arc.appendNurb(BezTriple.New(VectorTriples[0]))
			for p in VectorTriples[1:]:
				curve.append(BezTriple.New(p))
			for point in curve:
				point.handleTypes = [FREE, FREE]
				point.radius = 1.0
			curve.flagU = 0 # 0 sets the curve not cyclic=open
			arc.setResolu(settings.var['curve_res'])

			arc.update() #important for handles calculation

			ob = SCENE.objects.new(arc) # create a new curve_object
			ob.loc = tuple(self.loc)
			if thic != 0.0: #hack: Blender<2.45 curve-extrusion
				thic = thic * 0.5
				arc.setExt1(1.0)  # curve-extrusion: Blender2.45 accepts only (0.0 - 5.0)
				ob.LocZ = thic + self.loc[2]
			transform(self.extrusion, 0, ob)
			if thic != 0.0:
				ob.SizeZ *= abs(thic)
			return ob

		else:  # draw ARC as mesh --------------------
			if M_OBJ: obname, me, ob = makeNewObject()
			else: 
				me = Mesh.New(obname)		# create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
			# set a number of segments in entire circle
			arc_res = settings.var['arc_res'] * sqrt(radius) / sqrt(settings.var['arc_rad'])

			verts = calcArc(None, radius, start, end, arc_res, False)
			#verts = [list(point) for point in verts]
			len1 = len(verts)
			#print 'deb:len1:', len1  #-----------------------
			if width != 0:
				radius_out = radius + (0.5 * width)
				radius_in  = radius - (0.5 * width)
				if radius_in <= 0.0:
					radius_in = settings.var['dist_min']
					#radius_in = 0.0
				verts_in = []
				verts_out = []
				for point in verts:
					pointVec = Mathutils.Vector(point)
					pointVec = pointVec.normalize()
					verts_in.append(list(radius_in * pointVec))   #vertex inside
					verts_out.append(list(radius_out * pointVec)) #vertex outside
				verts = verts_in + verts_out

				#print 'deb:verts:', verts  #---------------------
				if thic != 0:
					thic_verts = []
					thic_verts.extend([[point[0], point[1], point[2]+thic] for point in verts])
					if thic < 0.0:
						thic_verts.extend(verts)
						verts = thic_verts
					else:
						verts.extend(thic_verts)
					f_bottom = [[num, num+1, len1+num+1, len1+num] for num in xrange(len1-1)]
					f_top   = [[num, len1+num, len1+num+1, num+1] for num in xrange(len1+len1, len1+len1+len1-1)]
					f_left   = [[num, len1+len1+num, len1+len1+num+1, num+1] for num in xrange(len1-1)]
					f_right  = [[num, num+1, len1+len1+num+1, len1+len1+num] for num in xrange(len1, len1+len1-1)]
					f_start = [[0, len1, len1+len1+len1, len1+len1]]
					f_end   = [[len1+len1-1, 0+len1-1, len1+len1+len1-1, len1+len1+len1+len1-1]]
					faces = f_left + f_right + f_bottom + f_top + f_start + f_end
	
					me.verts.extend(verts) # add vertices to mesh
					me.faces.extend(faces)  # add faces to the mesh

					if settings.var['meshSmooth_on']:  # left and right side become smooth ----------------------
						smooth_len = len(f_left) + len(f_right)
						for i in xrange(smooth_len):
							me.faces[i].smooth = True
					# each MeshSide becomes vertexGroup for easier material assignment ---------------------
					if settings.var['vGroup_on'] and not M_OBJ:
						# each MeshSide becomes vertexGroup for easier material assignment ---------------------
						replace = Mesh.AssignModes.REPLACE  #or .AssignModes.ADD
						vg_left, vg_right, vg_top, vg_bottom = [], [], [], []
						for v in f_left: vg_left.extend(v)
						for v in f_right: vg_right.extend(v)
						for v in f_top: vg_top.extend(v)
						for v in f_bottom: vg_bottom.extend(v)
						me.addVertGroup('side.left')  ; me.assignVertsToGroup('side.left',  vg_left, 1.0, replace)
						me.addVertGroup('side.right') ; me.assignVertsToGroup('side.right', vg_right, 1.0, replace)
						me.addVertGroup('side.top')   ; me.assignVertsToGroup('side.top',   vg_top, 1.0, replace)
						me.addVertGroup('side.bottom'); me.assignVertsToGroup('side.bottom',vg_bottom, 1.0, replace)
						me.addVertGroup('side.start'); me.assignVertsToGroup('side.start', f_start[0], 1.0, replace)
						me.addVertGroup('side.end')  ; me.assignVertsToGroup('side.end',   f_end[0],   1.0, replace)
					

				else:  # if thick=0 - draw only flat ring
					faces = [[num, len1+num, len1+num+1, num+1] for num in xrange(len1 - 1)]
					me.verts.extend(verts) # add vertices to mesh
					me.faces.extend(faces)  # add faces to the mesh
	
			elif thic != 0:
				thic_verts = []
				thic_verts.extend([[point[0], point[1], point[2]+thic] for point in verts])
				if thic < 0.0:
					thic_verts.extend(verts)
					verts = thic_verts
				else:
					verts.extend(thic_verts)
				faces = []
				#print 'deb:len1:', len1  #-----------------------
				#print 'deb:verts:', verts  #---------------------
				faces = [[num, num+1, num+len1+1, num+len1] for num in xrange(len1 - 1)]

				me.verts.extend(verts) # add vertices to mesh
				me.faces.extend(faces)  # add faces to the mesh
				if settings.var['meshSmooth_on']:  # left and right side become smooth ----------------------
					for i in xrange(len(faces)):
						me.faces[i].smooth = True

			else:
				edges = [[num, num+1] for num in xrange(len(verts)-1)]
				me.verts.extend(verts) # add vertices to mesh
				me.edges.extend(edges)  # add edges to the mesh

			#me.update()
			#ob = SCENE.objects.new(me) # create a new arc_object
			#ob.link(me)
			ob.loc = tuple(center)
			#ob.loc = Mathutils.Vector(ob.loc)
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
#		self.data = obj.data[:]

		# required data
		self.name =  getit(obj, 2, None)

		# optional data (with defaults)
		self.insertion_units =  getit(obj, 70, None)
		self.insert_units = getit(obj, 1070, None)
		"""code 1070 Einfuegeeinheiten:
		0 = Keine Einheiten; 1 = Zoll; 2 = Fuss; 3 = Meilen; 4 = Millimeter;
		5 = Zentimeter; 6 = Meter; 7 = Kilometer; 8 = Mikrozoll;
		9 = Mils; 10 = Yard; 11 = Angstrom; 12 = Nanometer;
		13 = Mikrons; 14 = Dezimeter; 15 = Dekameter;
		16 = Hektometer; 17 = Gigameter; 18 = Astronomische Einheiten;
		19 = Lichtjahre; 20 = Parsecs
		"""


	def __repr__(self):
		return "%s: name - %s, insert units - %s" %(self.__class__.__name__, self.name, self.insertion_units)




class Block:  #-----------------------------------------------------------------
	"""Class for objects representing dxf BLOCKs.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type block as input.
		"""
		if not obj.type == 'block':
			raise TypeError, "Wrong type %s for block object!" %obj.type

		self.type = obj.type
		self.name = obj.name
		self.data = obj.data[:]

		# required data
		self.flags = getit(obj, 70, 0)
		self.anonim = self.flags & 1 #anonymous block generated by hatching, associative dimensioning, other
		self.atrib  = self.flags & 2 # has attribute definitions
		self.xref = self.flags & 4 # is an external reference (xref)
		self.xref_lay = self.flags & 8 # is an xref overlay 
		self.dep_ext = self.flags & 16 #  is externally dependent
		self.dep_res = self.flags & 32 # resolved external reference
		self.xref_ext = self.flags & 64 # is a referenced external reference xref
		#--todo--- if self.flag > 4: self.xref = True

		# optional data (with defaults)
		self.path = getit(obj, 1, '') # Xref path name
		self.discription = getit(obj, 4, '')

		self.entities = dxfObject('block_contents') #creates empty entities_container for this block
		self.entities.data = objectify([ent for ent in obj.data if type(ent) != list])

		self.layer = getit(obj, 8, None)
		self.loc = self.get_loc(obj)

		#print 'deb:Block %s data:\n%s' %(self.name, self.data) #------------
		#print 'deb:Block %s self.entities.data:\n%s' %(self.name, self.entities.data) #------------
			
			

	def get_loc(self, data):
		"""Gets the insert point of the block.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 10, 0.0) # 10 = x
		loc[1] = getit(data, 20, 0.0) # 20 = y
		loc[2] = getit(data, 30, 0.0) # 30 = z
		return loc


	def __repr__(self):
		return "%s: name - %s, description - %s, xref-path - %s" %(self.__class__.__name__, self.name, self.discription, self.path)




class Insert:  #-----------------------------------------------------------------
	"""Class for objects representing dxf INSERTs.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type insert as input.
		"""
		if not obj.type == 'insert':
			raise TypeError, "Wrong type %s for insert object!" %obj.type
		self.type = obj.type
		self.data = obj.data[:]
		#print 'deb:Insert_init_ self.data:\n', self.data #-----------

		# required data
		self.name = obj.get_type(2)[0]

		# optional data (with defaults)
		self.rotation =  getit(obj, 50, 0)
		self.space = getit(obj, 67, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj, 8, None)
		self.loc = self.get_loc(obj)
		self.scale = self.get_scale(obj)
		self.rows, self.columns = self.get_array(obj)
		self.extrusion = get_extrusion(obj)

		#self.flags = getit(obj.data, 66, 0) #
		#self.attrib = self.flags & 1


	def get_loc(self, data):
		"""Gets the origin location of the insert.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 10, 0.0)
		loc[1] = getit(data, 20, 0.0)
		loc[2] = getit(data, 30, 0.0)
		return loc


	def get_scale(self, data):
		"""Gets the x/y/z scale factors of the insert.
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


	def get_target(self, data):
		"""Gets the origin location of the insert.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 1011, 0.0)
		loc[1] = getit(data, 1021, 0.0)
		loc[2] = getit(data, 1031, 0.0)
		return loc


	def get_color(self, data):
		"""Gets the origin location of the insert.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 1010, 0.0)
		loc[1] = getit(data, 1020, 0.0)
		loc[2] = getit(data, 1030, 0.0)
		return loc


	def get_ave_render(self, data):
		"""Gets the origin location of the insert.
		"""
		loc = [0, 0, 0]
		loc[0] = getit(data, 1010, 0.0)
		loc[1] = getit(data, 1020, 0.0)
		loc[2] = getit(data, 1030, 0.0)
		return loc


	def __repr__(self):
		return "%s: layer - %s, name - %s" %(self.__class__.__name__, self.layer, self.name)


	def draw(self, settings, deltaloc):
		"""for INSERT(block): draw empty-marker for duplicated Blender_Group.

		Blocks are made of three objects:
			the block_record in the tables section
			the block in the blocks section
			the insert object (one or more) in the entities section
		block_record gives the insert units,
		block provides the objects drawn in the	block,
		insert object gives the location/scale/rotation of the block instances.
		"""

		name = self.name.lower()
		if name == 'ave_render':
			if settings.var['lights_on']:  #if lights support activated
				a_data = get_ave_data(self.data)
				# AVE_RENDER objects:
				# 7:'Pref', 0:'Full Opt', 0:'Quick Opt', 1:'Scanl Opt', 2:'Raytr Opt', 0:'RFile Opt'
				# 0:'Fog Opt', 0:'BG Opt', 0:'SCENE1','','','','','','','','','',
				# '','','','','','','','','','','','',

				if a_data.key == 'SCENE': # define set of lights as blender group
					scene_lights = 1
				return
		elif name == 'ave_global':
			if settings.var['lights_on']:  #if lights support activated
				return
		elif name == 'sh_spot':
			if settings.var['lights_on']:  #if lights support activated
				obname = settings.blocknamesmap[self.name]
				obname = 'sp_%s' %obname  # create object name from block name
				#obname = obname[:MAX_NAMELENGTH]
				# blender: 'Lamp', 'Sun', 'Spot', 'Hemi', 'Area', or 'Photon'
				li = Lamp.New('Spot', obname)
				ob = SCENE.objects.new(li)
				intensity = 2.0 #--todo-- -----------
				li.setEnergy(intensity)
				target = self.get_target(self.data)
				color = self.get_color(self.data)
				li.R = color[0]
				li.G = color[1]
				li.B = color[2]

				ob.loc = tuple(self.loc)
				transform(self.extrusion, 0, ob)
				return ob

		elif name == 'overhead':
			if settings.var['lights_on']:  #if lights support activated
				obname = settings.blocknamesmap[self.name]
				obname = 'la_%s' %obname  # create object name from block name
				#obname = obname[:MAX_NAMELENGTH]
				# blender: 'Lamp', 'Sun', 'Spot', 'Hemi', 'Area', or 'Photon'
				li = Lamp.New('Lamp', obname)
				ob = SCENE.objects.new(li)
				intensity = 2.0 #--todo-- -----------
				li.setEnergy(intensity)
				target = self.get_target(self.data)
				color = self.get_color(self.data)
				li.R = color[0]
				li.G = color[1]
				li.B = color[2]

				ob.loc = tuple(self.loc)
				transform(self.extrusion, 0, ob)
				return ob

		elif name == 'direct':
			if settings.var['lights_on']:  #if lights support activated
				obname = settings.blocknamesmap[self.name]
				obname = 'su_%s' %obname  # create object name from block name
				#obname = obname[:MAX_NAMELENGTH]
				# blender: 'Lamp', 'Sun', 'Spot', 'Hemi', 'Area', or 'Photon'
				li = Lamp.New('Sun', obname)
				ob = SCENE.objects.new(li)
				intensity = 2.0 #--todo-- -----------
				li.setEnergy(intensity)
				color = self.get_color(self.data)
				li.R = color[0]
				li.G = color[1]
				li.B = color[2]

				ob.loc = tuple(self.loc)
				transform(self.extrusion, 0, ob)
				return ob

		elif settings.drawTypes['insert']:  #if insert_drawType activated
			#print 'deb:draw.  settings.blocknamesmap:', settings.blocknamesmap #--------------------
			obname = settings.blocknamesmap[self.name]
			obname = 'in_%s' %obname  # create object name from block name
			#obname = obname[:MAX_NAMELENGTH]

			# if material BYBLOCK def needed: use as placeholder a mesh-vertex instead of empty
			ob = SCENE.objects.new('Empty', obname) # create a new empty_object
			empty_size = 1.0 * settings.var['g_scale']
			if   empty_size < 0.01:  empty_size = 0.01 #Blender limits (0.01-10.0)
			elif empty_size > 10.0:  empty_size = 10.0
			ob.drawSize = empty_size

			# get our block_def-group
			block = settings.blocks(self.name)
			ob.DupGroup = block
			ob.enableDupGroup = True

			if block.name.startswith('xr_'):
				ob.name = 'xb_' + ob.name[3:]
		
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
	"""Class for objects representing dxf ELLIPSEs.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type ellipse as input.
		"""
		if not obj.type == 'ellipse':
			raise TypeError, "Wrong type %s for ellipse object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		# required data
		self.ratio = obj.get_type(40)[0] # Ratio of minor axis to major axis
		self.start_angle = obj.get_type(41)[0]  # in radians
		self.end_angle = obj.get_type(42)[0]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.thic =  getit(obj, 39, 0.0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj, 8, None)
		self.loc = self.get_loc(obj)
		self.major = self.get_major(obj)
		self.extrusion = get_extrusion(obj)


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
		obname = 'el_%s' %self.layer  # create object name from layer name
		obname = obname[:MAX_NAMELENGTH]

		center = self.loc
		start = degrees(self.start_angle)
		end = degrees(self.end_angle)
		if abs(end - 360.0) < 0.00001: end = 360.0
		ellipse_closed = False
		if end - start == 360.0: ellipse_closed = True
	
		# rotation = Angle between major and WORLDX
		# doesnt work, couse produces always positive value: rotation = Mathutils.AngleBetweenVecs(major, WORLDX)
		if self.major[0] == 0:
			rotation = 90.0
			if self.major[1] < 0: rotation += 180
		else:
			rotation = degrees(atan(self.major[1] / self.major[0]))
			if self.major[0] < 0:
				rotation += 180.0

		major = Mathutils.Vector(self.major)
		#radius = sqrt(self.major[0]**2 + self.major[1]**2 + self.major[2]**2)
		radius = major.length
		#print 'deb:calcEllipse:\n center, radius, start, end:\n', center, radius, start, end  #---------

		thic = set_thick(self.thic, settings)
		width = 0.0
		if settings.var['lines_as'] == 4: # as thin_box
			thic = settings.var['thick_min']
			width = settings.var['width_min']
		elif settings.var['lines_as'] == 3: # as thin cylinder
			cyl_rad = 0.5 * settings.var['width_min']

		elif settings.var['lines_as'] == 5:  # draw ELLIPSE as curve -------------
			arc_res = settings.var['curve_arc']
			triples = True
			VectorTriples = calcArc(None, radius, start, end, arc_res, triples)
			arc = Curve.New(obname) # create new curve data
			curve = arc.appendNurb(BezTriple.New(VectorTriples[0]))
			if ellipse_closed:
				for p in VectorTriples[1:-1]:
					curve.append(BezTriple.New(p))
				for point in curve:
					point.handleTypes = [FREE, FREE]
					point.radius = 1.0
				curve.flagU = 1 # 0 sets the curve not cyclic=open
				if settings.var['fill_on']:
					arc.setFlag(6) # 2+4 set top and button caps
				else:
					arc.setFlag(arc.getFlag() & ~6) # dont set top and button caps
			else:
				for p in VectorTriples[1:]:
					curve.append(BezTriple.New(p))
				for point in curve:
					point.handleTypes = [FREE, FREE]
					point.radius = 1.0
				curve.flagU = 0 # 0 sets the curve not cyclic=open

			arc.setResolu(settings.var['curve_res'])
			arc.update() #important for handles calculation

			ob = SCENE.objects.new(arc) # create a new curve_object
			ob.loc = tuple(self.loc)
			if thic != 0.0: #hack: Blender<2.45 curve-extrusion
				thic = thic * 0.5
				arc.setExt1(1.0)  # curve-extrusion: Blender2.45 accepts only (0.0 - 5.0)
				ob.LocZ = thic + self.loc[2]
			transform(self.extrusion, rotation, ob)
			ob.SizeY *= self.ratio
			if thic != 0.0:
				ob.SizeZ *= abs(thic)
			return ob


		else: # draw ELLIPSE as mesh --------------------------------------
			if M_OBJ: obname, me, ob = makeNewObject()
			else: 
				me = Mesh.New(obname)		# create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
			# set a number of segments in entire circle
			arc_res = settings.var['arc_res'] * sqrt(radius) / sqrt(settings.var['arc_rad'])

			verts = calcArc(None, radius, start, end, arc_res, False)
			#verts = [list(point) for point in verts]
			len1 = len(verts)
			#print 'deb:len1:', len1  #-----------------------
			if width != 0:
				radius_out = radius + (0.5 * width)
				radius_in  = radius - (0.5 * width)
				if radius_in <= 0.0:
					radius_in = settings.var['dist_min']
					#radius_in = 0.0
				verts_in = []
				verts_out = []
				for point in verts:
					pointVec = Mathutils.Vector(point)
					pointVec = pointVec.normalize()
					verts_in.append(list(radius_in * pointVec))   #vertex inside
					verts_out.append(list(radius_out * pointVec)) #vertex outside
				verts = verts_in + verts_out

				#print 'deb:verts:', verts  #---------------------
				if thic != 0:
					thic_verts = []
					thic_verts.extend([[point[0], point[1], point[2]+thic] for point in verts])
					if thic < 0.0:
						thic_verts.extend(verts)
						verts = thic_verts
					else:
						verts.extend(thic_verts)
					f_bottom = [[num, num+1, len1+num+1, len1+num] for num in xrange(len1-1)]
					f_top   = [[num, len1+num, len1+num+1, num+1] for num in xrange(len1+len1, len1+len1+len1-1)]
					f_left   = [[num, len1+len1+num, len1+len1+num+1, num+1] for num in xrange(len1-1)]
					f_right  = [[num, num+1, len1+len1+num+1, len1+len1+num] for num in xrange(len1, len1+len1-1)]
					f_start = [[0, len1, len1+len1+len1, len1+len1]]
					f_end   = [[len1+len1-1, 0+len1-1, len1+len1+len1-1, len1+len1+len1+len1-1]]
					faces = f_left + f_right + f_bottom + f_top + f_start + f_end
	
					me.verts.extend(verts) # add vertices to mesh
					me.faces.extend(faces)  # add faces to the mesh

					if settings.var['meshSmooth_on']:  # left and right side become smooth ----------------------
						smooth_len = len(f_left) + len(f_right)
						for i in xrange(smooth_len):
							me.faces[i].smooth = True
					if settings.var['vGroup_on'] and not M_OBJ:
						# each MeshSide becomes vertexGroup for easier material assignment ---------------------
						replace = Mesh.AssignModes.REPLACE  #or .AssignModes.ADD
						vg_left, vg_right, vg_top, vg_bottom = [], [], [], []
						for v in f_left: vg_left.extend(v)
						for v in f_right: vg_right.extend(v)
						for v in f_top: vg_top.extend(v)
						for v in f_bottom: vg_bottom.extend(v)
						me.addVertGroup('side.left')  ; me.assignVertsToGroup('side.left',  vg_left, 1.0, replace)
						me.addVertGroup('side.right') ; me.assignVertsToGroup('side.right', vg_right, 1.0, replace)
						me.addVertGroup('side.top')   ; me.assignVertsToGroup('side.top',   vg_top, 1.0, replace)
						me.addVertGroup('side.bottom'); me.assignVertsToGroup('side.bottom',vg_bottom, 1.0, replace)
						me.addVertGroup('side.start'); me.assignVertsToGroup('side.start', f_start[0], 1.0, replace)
						me.addVertGroup('side.end')  ; me.assignVertsToGroup('side.end',   f_end[0],   1.0, replace)
					

				else:  # if thick=0 - draw only flat ring
					faces = [[num, len1+num, len1+num+1, num+1] for num in xrange(len1 - 1)]
					me.verts.extend(verts) # add vertices to mesh
					me.faces.extend(faces)  # add faces to the mesh
	
			elif thic != 0:
				thic_verts = []
				thic_verts.extend([[point[0], point[1], point[2]+thic] for point in verts])
				if thic < 0.0:
					thic_verts.extend(verts)
					verts = thic_verts
				else:
					verts.extend(thic_verts)
				faces = []
				#print 'deb:len1:', len1  #-----------------------
				#print 'deb:verts:', verts  #---------------------
				faces = [[num, num+1, num+len1+1, num+len1] for num in xrange(len1 - 1)]

				me.verts.extend(verts) # add vertices to mesh
				me.faces.extend(faces)  # add faces to the mesh
				if settings.var['meshSmooth_on']:  # left and right side become smooth ----------------------
					for i in xrange(len(faces)):
						me.faces[i].smooth = True

			else:
				edges = [[num, num+1] for num in xrange(len(verts)-1)]
				me.verts.extend(verts) # add vertices to mesh
				me.edges.extend(edges)  # add edges to the mesh

			#print 'deb:calcEllipse transform rotation: ', rotation  #---------
			ob.loc = tuple(center)
			#old ob.SizeY = self.ratio
			transform(self.extrusion, rotation, ob)
			#old transform(self.extrusion, 0, ob)
			ob.SizeY *= self.ratio
	
			return ob



class Face:  #-----------------------------------------------------------------
	"""Class for objects representing dxf 3DFACEs.
	"""

	def __init__(self, obj):
		"""Expects an entity object of type 3dfaceplot as input.
		"""
		if not obj.type == '3dface':
			raise TypeError, "Wrong type %s for 3dface object!" %obj.type
		self.type = obj.type
#		self.data = obj.data[:]

		# optional data (with defaults)
		self.space = getit(obj, 67, 0)
		self.color_index = getit(obj, 62, BYLAYER)

		self.layer = getit(obj, 8, None)
		self.points = self.get_points(obj)


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

		if M_OBJ: obname, me, ob = makeNewObject()
		else: 
			if activObjectLayer == self.layer and settings.var['one_mesh_on']:
				obname = activObjectName
				#print 'deb:face.draw obname from activObjectName: ', obname #---------------------
				ob = getSceneChild(obname)  # open an existing mesh_object
				#ob = SCENE.getChildren(obname)  # open an existing mesh_object
				me = ob.getData(name_only=False, mesh=True)
			else:
				obname = 'fa_%s' %self.layer  # create object name from layer name
				obname = obname[:MAX_NAMELENGTH]
				me = Mesh.New(obname)		  # create a new mesh
				ob = SCENE.objects.new(me) # create a new mesh_object
				activObjectName = ob.name
				activObjectLayer = self.layer
				#print ('deb:except. new face.ob+mesh:"%s" created!' %ob.name) #---------------------
	
		#me = Mesh.Get(ob.name)   # open objects mesh data
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
		if settings.var['vGroup_on'] and not M_OBJ:
			# entities with the same color build one vertexGroup for easier material assignment ---------------------
			ob.link(me) # link mesh to that object
			vG_name = 'color_%s' %self.color_index
			if edges: faces = edges
			replace = Mesh.AssignModes.ADD  #or .AssignModes.REPLACE or ADD
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
	'vport':Vport,
	'view':View,
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
	'spline':Spline,
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
		elif type(item) != list and item.type == 'polyline': #remi --todo-----------
			#print 'deb:gosub Polyline\n' #-------------
			pline = Polyline(item)
			while 1:
				index += 1
				item = data[index]
				if item.type == 'vertex':
					#print 'deb:objectify gosub Vertex--------' #-------------
					v = Vertex(item)
					if pline.spline: # if NURBSpline-curve
						# then for Blender-mesh  filter only additional_vertices
						# OR
						# then for Blender-curve filter only spline_control_vertices
						if (v.spline and not curves_on) or (curves_on and v.spline_c): #correct for real NURBS-import
						#if (v.spline and not curves_on) or (curves_on and not v.spline_c): #fake for Bezier-emulation of NURBS-import
							pline.points.append(v)
					elif pline.curved:  # if Bezier-curve
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
		elif type(item) != list and item.type in ['block', 'insert']:
			if not settings.var['block_nn'] and item.name.startswith('*X'):
				#print 'deb:objectify item.type:"%s", item.name:"%s"' %(item.type, item.name) #------------
				pass
			elif settings.var['blockFilter_on'] and not settings.accepted_block(item.name):
				pass
			else:
				try:
					objects.append(type_map[item.type](item))
				except TypeError:
					pass
		elif type(item) != list and item.type in known_types:
			# proccess the object and append the resulting object
			try:
				objects.append(type_map[item.type](item))
			except TypeError:
				pass
		else:
			#we will just let the data pass un-harrased
			#objects.append(item)
			pass
		index += 1
	#print 'deb:objectify objects:\n', objects #------------
	#print 'deb:objectify END %%%%%%%%' #------------
	return objects



class MatColors:  #-----------------------------------------------------------------
	"""A smart container for dxf-color based materials.

	This class is a wrapper around a dictionary mapping dxf-color indicies to materials.
	When called with a color_index
	it returns a material corresponding to that index.
	Behind the scenes it checks if that index is in its keys, and if not it creates
	a new material.  It then adds the new index:material pair to its dict and returns
	the material.
	"""

	def __init__(self):
		"""Expects a map - a dictionary mapping layer names to layers.
		"""
		#self.layersmap = layersmap  # a dictionary of layername:layerobject
		self.colMaterials = {}  # a dictionary of color_index:blender_material
		#print 'deb:init_MatColors argument.map: ', map #------------------


	def __call__(self, color=None):
		"""Return the material associated with color.

		If a layer name is provided, the color of that layer is used.
		"""
		if color == None: color = 256  # color 256=BYLAYER
		if type(color) == str: # looking for color of LAYER named "color"
			#--todo---bug with ARC from ARC-T0.DXF layer="T-3DARC-1"-----
			#print 'deb:color is string:--------: ', color
			#try:
				#color = layersmap[color].color
				#print 'deb:color=self.map[color].color:', color #------------------
			#except KeyError:
				#layer = Layer(name=color, color=256, frozen=False)
				#layersmap[color] = layer
				#color = 0
			if color in layersmap.keys():
				color = layersmap[color].color
		if color == 256:  # color 256 = BYLAYER
			#--todo-- should looking for color of LAYER
			#if layersmap: color = layersmap[color].color
			color = 3
		if color == 0:  # color 0 = BYBLOCK
			#--todo-- should looking for color of paret-BLOCK
			#if layersmap: color = layersmap[color].color
			color = 3
		color = abs(color)  # cause the value could be nagative = means the layer is turned off

		if color not in self.colMaterials.keys():
			self.add(color)
		return self.colMaterials[color]


	def add(self, color):
		"""Create a new material 'ColorNr-N' using the provided color index-N.
		"""
		#global color_map    #--todo-- has not to be global?
		mat = Material.New('ColorNr-%s' %color)
		mat.setRGBCol(color_map[color])
		#mat.mode |= Material.Modes.SHADELESS  #--todo--
		#mat.mode |= Material.Modes.WIRE
#		try: mat.setMode('Shadeless', 'Wire') #work-around for 2.45rc-bug
#		except: pass
		self.colMaterials[color] = mat



class MatLayers:  #-----------------------------------------------------------------
	"""A smart container for dxf-layer based materials.

	This class is a wrapper around a dictionary mapping dxf-layer names to materials.
	When called with a layer name it returns a material corrisponding to that.
	Behind the scenes it checks if that layername is in its keys, and if not it creates
	a new material.  It then adds the new layername:material pair to its dict and returns
	the material.
	"""

	def __init__(self):
		"""Expects a map - a dictionary mapping layer names to layers.
		"""
		#self.layersmap = layersmap  # a dictionary of layername:layer
		self.layMaterials = {}  # a dictionary of layer_name:blender_material
		#print 'deb:init_MatLayers argument.map: ', map #------------------


	def __call__(self, layername=None, color=None):
		"""Return the material associated with dxf-layer.

		If a dxf-layername is not provided, create a new material
		"""
		#global layernamesmap
		layername_short = layername
		if layername in layernamesmap.keys():
			layername_short = layernamesmap[layername]
		colorlayername = layername_short
		if color: colorlayername = str(color) + colorlayername
		if colorlayername not in self.layMaterials.keys():
			self.add(layername, color, colorlayername)
		return self.layMaterials[colorlayername]


	def add(self, layername, color, colorlayername):
		"""Create a new material 'layername'.
		"""
		try: mat = Material.Get('L-%s' %colorlayername)
		except: mat = Material.New('L-%s' %colorlayername)
		#print 'deb:MatLayers material: ', mat  #----------
		#global settings
		#print 'deb:MatLayers material_from: ', settings.var['material_from']  #----------
		if settings.var['material_from'] == 3 and color:
			if color == 0 or color == 256: mat_color = 3
			else: mat_color = color
		elif layersmap and layername:
			mat_color = layersmap[layername].color
		else: mat_color = 3
		#print 'deb:MatLayers color: ', color  #-----------
		#print 'deb:MatLayers mat_color: ', mat_color  #-----------
		mat.setRGBCol(color_map[abs(mat_color)])
		#mat.mode |= Material.Modes.SHADELESS
		#mat.mode |= Material.Modes.WIRE
#		try: mat.setMode('Shadeless', 'Wire') #work-around for 2.45rc-bug
#		except: pass
		self.layMaterials[colorlayername] = mat




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
		self.blocksmap = blocksmap	 #a dictionary mapping block_name:block_data
		self.settings = settings
		self.blocks = {}   #container for blender groups representing blocks


	def __call__(self, name=None):
		"""Return the data block associated with that block_name.

		If that name is not in its keys, it creates a new data block.
		If no name is provided return entire self.blocks container.
		"""
		if name == None:
			return self.blocks
		if name not in self.blocks.keys():
			self.addBlock(name)
		return self.blocks[name]


	def addBlock(self, name):
		"""Create a new 'block group' for the block name.
		"""
		block = self.blocksmap[name]
		prefix = 'bl'
		if block.xref: prefix = 'xr'
		blender_group = Group.New('%s_%s' %(prefix,name))  # Blender groupObject contains definition of BLOCK
		block_def = [blender_group, block.loc]
		self.settings.write("\nDrawing block:\'%s\' ..." % name)

		if block.xref:
			obname = 'xr_%s' %name  # create object name from xref block name
			#obname = obname[:MAX_NAMELENGTH]
			# if material BYBLOCK def needed: use as placeholder a mesh-vertex instead of empty
			ob = SCENE.objects.new('Empty', obname) # create a new empty_object
			empty_size = 1.0 * settings.var['g_scale']
			if   empty_size < 0.01:  empty_size = 0.01 #Blender limits (0.01-10.0)
			elif empty_size > 10.0:  empty_size = 10.0
			ob.drawSize = empty_size
			ob.loc = tuple(block.loc)
			ob.properties['xref_path'] = block.path
			ob.layers = [19]
			insertFlag=True; blockFlag=True
			global oblist
			oblist.append((ob, insertFlag, blockFlag))
		else:		
			if M_OBJ:
				car_end()
				car_start()
			drawEntities(block.entities, self.settings, block_def)
			if M_OBJ: car_end()
		self.settings.write("Drawing block:\'%s\' done!" %name)
		self.blocks[name] = blender_group





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

		self.var = dict(keywords)	  #a dictionary of (key_variable:Value) control parameter
		self.drawTypes = dict(drawTypes) #a dictionary of (entity_type:True/False) = import on/off for this entity_type

		self.var['colorFilter_on'] = False   #deb:remi------------
		self.acceptedColors = [0,2,3,4,5,6,7,8,9,
							   10 ]

		self.var['layerFilter_on'] = False   #deb:remi------------
		self.acceptedLayers = ['3',
						   '0'
						  ]

		self.var['groupFilter_on'] = False   #deb:remi------------
		self.acceptedLayers = ['3',
						   '0'
						  ]

		#self.var['blockFilter_on'] = 0   #deb:remi------------
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

		self.drawTypes['arc'] = self.drawTypes['line']
		self.drawTypes['circle'] = self.drawTypes['line']
		self.drawTypes['ellipse'] = self.drawTypes['line']
		self.drawTypes['trace'] = self.drawTypes['solid']
		self.drawTypes['insert'] = self.drawTypes['block']
		#self.drawTypes['vport'] = self.drawTypes['view']

		#print 'deb:self.drawTypes', self.drawTypes #---------------


	def validate(self, drawing):
		"""Given the drawing, build dictionaries of Layers, Colors and Blocks.
		"""

		global oblist
		#adjust the distance parameter to globalScale
		if self.var['g_scale'] != 1.0:
			self.var['dist_min']  = self.var['dist_min'] / self.var['g_scale']
			self.var['thick_min'] = self.var['thick_min'] / self.var['g_scale']
			self.var['width_min'] = self.var['width_min'] / self.var['g_scale']
			self.var['arc_rad'] =  self.var['arc_rad'] / self.var['g_scale']

		self.g_origin = Mathutils.Vector(self.var['g_originX'], self.var['g_originY'], self.var['g_originZ'])

		# First sort out all the section_items
		sections = dict([(item.name, item) for item in drawing.data])

		# The section:header may be omited
		if 'header' in sections.keys():
			self.write("found section:header")
		else:
			self.write("File contains no section:header!")

		if self.var['optimization'] == 0: self.var['one_mesh_on'] = 0
		# The section:tables may be partialy or completely missing.
		self.layersTable = False
		self.colMaterials = MatColors() #A container for dxf-color based materials
		self.layMaterials = MatLayers() #A container for dxf-layer based materials
		#self.collayMaterials = MatColLayers({}) #A container for dxf-color+layer based materials
		global layersmap, layernamesmap
		layersmap, layernamesmap = {}, {}
		if 'tables' in sections.keys():
			self.write("found section:tables")
			views, vports, layers = False, False, False
			for table in drawing.tables.data:
				if table.name == 'layer':
					self.write("found table:layers")
					layers = table
				elif table.name == 'view':
					print "found table:view"
					views = table
				elif table.name == 'vport':
					print "found table:vport"
					vports = table
			if layers: #----------------------------------
				# Read the layers table and get the layer colors
				layersmap, layernamesmap = getLayersmap(layers)
				#self.colMaterials = MatColors()
				#self.layMaterials = MatLayers()
			else:
				self.write("File contains no table:layers!")


			if views: #----------------------------------
				if self.var['views_on']:
					for item in views.data:
						if type(item) != list and item.type == 'view':
							#print 'deb:settings_valid views dir(item)=', dir(item) #-------------
							#print 'deb:settings_valid views item=', item #-------------
							ob = item.draw(self)
							#viewsmap[item.name] = [item.length]
							#--todo-- add to obj_list for global.Scaling
							insertFlag, blockFlag = False, False
							oblist.append((ob, insertFlag, blockFlag))

			else:
				self.write("File contains no table:views!")


			if vports: #----------------------------------
				if self.var['views_on']:
					for item in vports.data:
						if type(item) != list and item.type == 'vport':
							#print 'deb:settings_valid views dir(item)=', dir(item) #-------------
							#print 'deb:settings_valid views item=', item #-------------
							ob = item.draw(self)
							#viewsmap[item.name] = [item.length]
							#--todo-- add to obj_list for global.Scaling
							insertFlag, blockFlag = False, False
							oblist.append((ob, insertFlag, blockFlag))
			else:
				self.write("File contains no table:vports!")


		else:
			self.write("File contains no section:tables!")
			self.write("File contains no table:layers!")


		# The section:blocks may be omited
		if 'blocks' in sections.keys():
			self.write("found section:blocks")
			# Read the block definitions and build our block object
			if self.drawTypes['insert']: #if support for entity type 'Insert' is activated
				#Build a dictionary of blockname:block_data pairs
				blocksmap, obj_number = getBlocksmap(drawing, layersmap, self.var['layFrozen_on'])
				self.obj_number += obj_number
				self.blocknamesmap = getBlocknamesmap(blocksmap)
				self.blocks = Blocks(blocksmap, self) # initiates container for blocks_data
				self.usedBlocks = blocksmap.keys()
				#print 'deb:settings_valid self.usedBlocks', self.usedBlocks #----------
			else:
				self.write("ignored, because support for BLOCKs is turn off!")
			#print 'deb:settings_valid self.obj_number', self.obj_number #----------
		else:
			self.write("File contains no section:blocks!")
			self.drawTypes['insert'] = False

		# The section:entities
		if 'entities' in sections.keys():
			self.write("found section:entities")
			self.obj_number += len(drawing.entities.data)
			self.obj_number = 1.0 / self.obj_number


	def accepted_block(self, name):
		if name not in self.usedBlocks: return False
		if name in self.unwantedBlocks: return False
		elif name in self.acceptedBlocks: return True
		#elif (name.find('*X')+1): return False
		#elif name.startswith('3'): return True
		#elif name.endswith('H'): return False
		return True


	def write(self, text, newline=True):
		"""Wraps the built-in print command in a optimization check.
		"""
		if self.var['optimization'] <= self.MID:
			if newline: print text
			else: print text,


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

	def layer_isOff(self, layername):  # no more used -------
		"""Given a layer name, and return its visible status.
		"""
		# if layer is off then color_index is negative 
		if layersmap and layersmap[layername].color < 0: return True
		#print 'deb:layer_isOff: layer is ON' #---------------
		return False


	def layer_isFrozen(self, layername):  # no more used -------
		"""Given a layer name, and return its frozen status.
		"""
		if layersmap and layersmap[layername].frozen: return True
		#print 'deb:layer_isFrozen: layer is not FROZEN' #---------------
		return False



def	analyzeDXF(dxfFile): #---------------------------------------
	"""list statistics about LAYER and BLOCK dependences into textfile.INF
	
	"""
	Window.WaitCursor(True)   # Let the user know we are thinking
	print 'reading DXF file: %s.' % dxfFile
	time1 = sys.time()  #time marker1
	drawing = readDXF(dxfFile, objectify)
	print 'finish reading in %.4f sec.' % (sys.time()-time1)

	# First sort out all the section_items
	sections = dict([(item.name, item) for item in drawing.data])

	# The section:header may be omited
	if 'header' in sections.keys():	print "found section:header"
	else: print "File contains no section:header!"

	# The section:tables may be partialy or completely missing.
	layersTable = False
	global layersmap
	layersmap = {}
	viewsmap = {}
	vportsmap = {}
	layersmap_str = '#File contains no table:layers!'
	viewsmap_str = '#File contains no table:views!'
	vportsmap_str = '#File contains no table:vports!'
	if 'tables' in sections.keys():
		print "found section:tables"
		views, vports, layers = False, False, False
		for table in drawing.tables.data:
			if table.name == 'layer':
				print "found table:layers"
				layers = table
			elif table.name == 'view':
				print "found table:view"
				views = table
			elif table.name == 'vport':
				print "found table:vport"
				vports = table
		if layers: #----------------------------------
			for item in layers.data:
				if type(item) != list and item.type == 'layer':
					#print dir(item)
					layersmap[item.name] = [item.color, item.frozen]
			#print 'deb:analyzeDXF: layersmap=' , layersmap #-------------
			layersmap_str = '#list of LAYERs: name, color, frozen_status  ---------------------------\n'
			key_list = layersmap.keys()
			key_list.sort()
			for key in key_list:
			#for layer_name, layer_data in layersmap.iteritems():
				layer_name, layer_data = key, layersmap[key]
				layer_str = '\'%s\': col=%s' %(layer_name,layer_data[0])#-------------
				if layer_data[1]: layer_str += ', frozen'
				layersmap_str += layer_str + '\n'
			#print 'deb:analyzeDXF: layersmap_str=\n' , layersmap_str #-------------
		else:
			print "File contains no table:layers!"

		if views: #----------------------------------
			for item in views.data:
				if type(item) != list and item.type == 'view':
					#print dir(item)
					viewsmap[item.name] = [item.length]
			#print 'deb:analyzeDXF: viewsmap=' , viewsmap #-------------
			viewsmap_str = '#list of VIEWs: name, focus_length  ------------------------------------\n'
			key_list = viewsmap.keys()
			key_list.sort()
			for key in key_list:
			#for view_name, view_data in viewsmap.iteritems():
				view_name, view_data = key, viewsmap[key]
				view_str = '\'%s\': length=%s' %(view_name,view_data[0])#-------------
				#if view_data[1]: view_str += ', something'
				viewsmap_str += view_str + '\n'
			#print 'deb:analyzeDXF: layersmap_str=\n' , layersmap_str #-------------
		else:
			print "File contains no table:views!"

		if vports: #----------------------------------
			for item in vports.data:
				if type(item) != list and item.type == 'vport':
					#print dir(item)
					vportsmap[item.name] = [item.length]
			#print 'deb:analyzeDXF: vportsmap=' , vportsmap #-------------
			vportsmap_str = '#list of VPORTs: name, focus_length  -----------------------------------\n'
			key_list = vportsmap.keys()
			key_list.sort()
			for key in key_list:
			#for vport_name, vport_data in vportsmap.iteritems():
				vport_name, vport_data = key, vportsmap[key]
				vport_str = '\'%s\': length=%s' %(vport_name,vport_data[0])#-------------
				#if vport_data[1]: vport_str += ', something'
				vportsmap_str += vport_str + '\n'
			#print 'deb:analyzeDXF: vportsmap_str=\n' , vportsmap_str #-------------
		else:
			print "File contains no table:vports!"

	else:
		print "File contains no section:tables!"
		print "File contains no tables:layers,views,vports!"

	# The section:blocks may be omited
	if 'blocks' in sections.keys():
		print "found section:blocks"
		blocksmap = {}
		for item in drawing.blocks.data:
			#print 'deb:getBlocksmap item=' ,item #--------
			#print 'deb:getBlocksmap item.entities=' ,item.entities #--------
			#print 'deb:getBlocksmap item.entities.data=' ,item.entities.data #--------
			if type(item) != list and item.type == 'block':
				xref = False
				if item.xref: xref = True
				childList = []
				used = False
				for item2 in item.entities.data:
					if type(item2) != list and item2.type == 'insert':
						#print 'deb:getBlocksmap dir(item2)=', dir(item2) #----------
						item2str = [item2.name, item2.layer, item2.color_index, item2.scale, item2.space]
						childList.append(item2str)
				try: blocksmap[item.name] = [used, childList, xref]
				except KeyError: print 'Cannot map "%s" - "%s" as Block!' %(item.name, item)
		#print 'deb:analyzeDXF: blocksmap=' , blocksmap #-------------

		for item2 in drawing.entities.data:
			if type(item2) != list and item2.type == 'insert':
				if item2.name in blocksmap.keys():
					if not layersmap or (layersmap and not layersmap[item2.layer][1]): #if insert_layer is not frozen
						blocksmap[item2.name][0] = True # marked as world used BLOCK

		key_list = blocksmap.keys()
		key_list.reverse()
		for key in key_list:
			if blocksmap[key][0]: #if used
				for child in blocksmap[key][1]:
					if not layersmap or (layersmap and not layersmap[child[1]][1]): #if insert_layer is not frozen
						blocksmap[child[0]][0] = True # marked as used BLOCK

		blocksmap_str = '#list of BLOCKs: name:(unused)(xref) -[child_name, layer, color, scale, space]-------\n'
		key_list = blocksmap.keys()
		key_list.sort()
		for key in key_list:
		#for block_name, block_data in blocksmap.iteritems():
			block_name, block_data = key, blocksmap[key]
			block_str = '\'%s\': ' %(block_name) #-------------
			used = '(unused)'
			if block_data[0]: used = ''
#			else: used = '(unused)'
			xref = ''
			if block_data[2]: xref = '(xref)'
 			blocksmap_str += block_str + used + xref +'\n'
			if block_data:
				for block_item in block_data[1]:
					block_data_str = ' - %s\n' %block_item
					blocksmap_str += block_data_str
		#print 'deb:analyzeDXF: blocksmap_str=\n' , blocksmap_str #-------------
	else:
		blocksmap_str = '#File contains no section:blocks!'
		print "File contains no section:blocks!"

	Window.WaitCursor(False)
	output_str = '%s\n%s\n%s\n%s' %(viewsmap_str, vportsmap_str, layersmap_str, blocksmap_str)
	infFile = dxfFile[:-4] + '_DXF.INF'  # replace last char:'.dxf' with '_DXF.inf'
	try:
		f = file(infFile, 'w')
		f.write(INFFILE_HEADER + '\n# this is a comment line\n\n')
		f.write(output_str)
		f.close()
		Draw.PupMenu('DXF importer: report saved in INF-file:%t|' + '\'%s\'' %infFile)
	except:
		Draw.PupMenu('DXF importer: ERROR by writing report in INF-file:%t|' + '\'%s\'' %infFile)
	#finally: f.close()




def main(dxfFile):  #---------------#############################-----------
	#print 'deb:filename:', filename #--------------
	global SCENE
	global oblist
	editmode = Window.EditMode()	# are we in edit mode?  If so ...
	if editmode:
		Window.EditMode(0) # leave edit mode before

	#SCENE = bpy.data.scenes.active
	#SCENE.objects.selected = [] # deselect all
	
	global cur_COUNTER  #counter for progress_bar
	cur_COUNTER = 0

	#try:
	if 1:
		#print "Getting settings..."
		global GUI_A, GUI_B, g_scale_as
		if not GUI_A['g_scale_on'].val:
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
			#print '\nDXF Import: terminated by user!'
			print '\nDXF Import: terminated, cause settings failure!'
			Window.WaitCursor(False)
			if editmode: Window.EditMode(1) # and put things back how we fond them
			return None

		#no more used dxfFile = dxfFileName.val
		#print 'deb: dxfFile file: ', dxfFile #----------------------
		if dxfFile.lower().endswith('.dxf') and sys.exists(dxfFile):
			Window.WaitCursor(True)   # Let the user know we are thinking
			print 'reading file: %s.' % dxfFile
			time1 = sys.time()  #time marker1
			drawing = readDXF(dxfFile, objectify)
			print 'reading finished in %.4f sec.' % (sys.time()-time1)
			Window.WaitCursor(False)
		elif dxfFile.lower().endswith('.dwg') and sys.exists(dxfFile):
			if not extCONV_OK:
				Draw.PupMenu(extCONV_TEXT)
				Window.WaitCursor(False)
				if editmode: Window.EditMode(1) # and put things back how we fond them
				return None
			else:
				Window.WaitCursor(True)   # Let the user know we are thinking
				#todo: issue: in DConvertCon.exe the output filename is fixed to dwg_name.dxf
				
				if 0: # works only for Windows
					dwgTemp = 'temp_01.dwg'
					dxfTemp = 'temp_01.dxf'
					os.system('copy %s %s' %(dxfFile,dwgTemp))
				else:
					dwgTemp = dxfFile
					dxfTemp = dxfFile[:-3]+'dxf'
				#print 'temp. converting: %s\n              to: %s' %(dxfFile, dxfTemp)
				#os.system('%s %s  -acad11 -dxf' %(extCONV_PATH, dxfFile))
				os.system('%s %s  -dxf' %(extCONV_PATH, dwgTemp))
				#os.system('%s %s  -dxf' %(extCONV_PATH, dxfFile_temp))
				if sys.exists(dxfTemp):
					print 'reading file: %s.' % dxfTemp
					time1 = sys.time()  #time marker1
					drawing = readDXF(dxfTemp, objectify)
					#os.remove(dwgTemp)
					os.remove(dxfTemp) # clean up
					print 'reading finished in %.4f sec.' % (sys.time()-time1)
					Window.WaitCursor(False)
				else:
					if UI_MODE: Draw.PupMenu('DWG importer:  nothing imported!%t|No valid DXF-representation found!')
					print 'DWG importer:  nothing imported. No valid DXF-representation found.'
					Window.WaitCursor(False)
					if editmode: Window.EditMode(1) # and put things back how we fond them
					return None
		else:
			if UI_MODE: Draw.PupMenu('DXF importer:  Alert!%t| no valid DXF-file selected!')
			print "DXF importer: Alert! - no valid DXF-file selected."
			Window.WaitCursor(False)
			if editmode: Window.EditMode(1) # and put things back how we fond them
			return None

		# Draw all the know entity types in the current scene
		oblist = []  # a list of all created AND linked objects for final f_globalScale
		time2 = sys.time()  #time marker2

		Window.WaitCursor(True)   # Let the user know we are thinking
		settings.write("\n\nDrawing entities...")

		settings.validate(drawing)

		global activObjectLayer, activObjectName
		activObjectLayer, activObjectName = None, None

		if M_OBJ: car_init()

		drawEntities(drawing.entities, settings)

		#print 'deb:drawEntities after: oblist:', oblist #-----------------------
		if M_OBJ: car_end()
		if oblist: # and settings.var['g_scale'] != 1:
			globalScale(oblist, settings.var['g_scale'])

		# Set visibility for all layers on all View3d
		#Window.ViewLayers([i+1 for i in range(18)]) # for 2.45
		SCENE.setLayers([i+1 for i in range(18)])
		SCENE.update(1)
		SCENE.objects.selected = [i[0] for i in oblist] #select only the imported objects		   
		#SCENE.objects.selected = SCENE.objects   #select all objects in current scene		  
		Blender.Redraw()

		time_text = sys.time() - time2
		Window.WaitCursor(False)
		if settings.var['paper_space_on']: space = 'from paper space'
		else: space = 'from model space'
		ob_len = len(oblist)
		message = '  %s objects imported %s in %.4f sec.  -----DONE-----' % (ob_len, space, time_text)
		settings.progress(1.0/settings.obj_number, 'DXF import done!')
		print message
		#settings.write(message)
		if UI_MODE: Draw.PupMenu('DXF importer:	Done!|finished in %.4f sec.' % time_text)

	#finally:
		# restore state even if things didn't work
		#print 'deb:drawEntities finally!' #-----------------------
		Window.WaitCursor(False)
		if editmode: Window.EditMode(1) # and put things back how we fond them



def getOCS(az):  #-----------------------------------------------------------------
	"""An implimentation of the Arbitrary Axis Algorithm.
	"""
	#decide if we need to transform our coords
	#if az[0] == 0 and az[1] == 0: 
	if abs(az[0]) < 0.00001 and abs(az[1]) < 0.00001:
		if az[2] > 0.0:
			return False
		elif az[2] < 0.0:
			ax = Mathutils.Vector(-1.0, 0, 0)
			ay = Mathutils.Vector(0, 1.0, 0)
			az = Mathutils.Vector(0, 0, -1.0)
			return ax, ay, az 

	az = Mathutils.Vector(az)

	cap = 0.015625 # square polar cap value (1/64.0)
	if abs(az.x) < cap and abs(az.y) < cap:
		ax = M_CrossVecs(WORLDY,az)
	else:
		ax = M_CrossVecs(WORLDZ,az)
	ax = ax.normalize()
	ay = M_CrossVecs(az, ax)
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



def getLayersmap(dxflayers):  #------------------------------------------------------
	"""Build two dictionaries: 1.layername:layer object, and 2.layername:layername_short
	gets set of layers from TABLES SECTION LAYERS
	"""
	layersmap = {}
	layernamesmap = {}
	for item in dxflayers.data:
		if type(item) != list and item.type == 'layer':
			layersmap[item.name] = item
			layername_short = item.name[:MAX_NAMELENGTH-1]
			i = 0  #sufix for layernames cause Blender-objectnames-limits
			while layername_short in layernamesmap.keys():
				i += 1
				suffix = str(i) #--todo--set zero-leading number format
				layername_short = layername_short[:-2] + suffix
			layernamesmap[item.name] = layername_short

	#print 'deb:getLayersmap layersmap:\n', layersmap #------------
	#print 'deb:getLayersmap layernamesmap:\n', layernamesmap #------------
	return layersmap, layernamesmap


	
def getBlocksmap(drawing, layersmap, layFrozen_on=False):  #--------------------------------------------------------
	"""Build a dictionary of blockname:block_data pairs
	"""
	usedblocks = {}
	for item in drawing.blocks.data:
		#print 'deb:getBlocksmap item=%s\n i.entities=%s\n i.data=%s' %(item,item.entities,item.entities.data)
		if type(item) != list and item.type == 'block':
			childList = []
			used = False
			for item2 in item.entities.data:
				if type(item2) != list and item2.type == 'insert':
					#print 'deb:getBlocksmap dir(item2)=', dir(item2) #----------
					item2str = [item2.name, item2.layer]
					childList.append(item2str)
			try: usedblocks[item.name] = [used, childList]
			except KeyError: print 'Cannot find "%s" Block!' %(item.name)
	#print 'deb:getBlocksmap: usedblocks=' , usedblocks #-------------
	#print 'deb:getBlocksmap:  layersmap=' , layersmap #-------------

	for item in drawing.entities.data:
		if type(item) != list and item.type == 'insert':
			if not layersmap or (not layersmap[item.layer].frozen or layFrozen_on): #if insert_layer is not frozen
				try: usedblocks[item.name][0] = True 
				except KeyError: print 'Cannot find "%s" Block!' %(item.name)
				
	key_list = usedblocks.keys()
	key_list.reverse()
	for key in key_list:
		if usedblocks[key][0]: #if parent used, then set used also all child blocks
			for child in usedblocks[key][1]:
				if not layersmap or (layersmap and not layersmap[child[1]].frozen): #if insert_layer is not frozen
					try: usedblocks[child[0]][0] = True # marked as used BLOCK
					except KeyError: print 'Cannot find "%s" Block!' %(child[0])

	usedblocks = [i for i in usedblocks.keys() if usedblocks[i][0]]
	#print 'deb:getBlocksmap: usedblocks=' , usedblocks #-------------
	obj_number = 0
	blocksmap = {}
	for item in drawing.blocks.data:
		if type(item) != list and item.type == 'block' and item.name in usedblocks:
			#if item.name.startswith('*X'): #--todo--
			obj_number += len(item.entities.data)
			try: blocksmap[item.name] = item
			except KeyError: print 'Cannot map "%s" - "%s" as Block!' %(item.name, item)
	

	#print 'deb:getBlocksmap: blocksmap:\n', blocksmap #------------
	return blocksmap, obj_number


def getBlocknamesmap(blocksmap):  #--------------------------------------------------------
	"""Build a dictionary of blockname:blockname_short pairs
	"""
	#print 'deb:getBlocknamesmap blocksmap:\n', blocksmap #------------
	blocknamesmap = {}
	for n in blocksmap.keys():
		blockname_short = n[:MAX_NAMELENGTH-1]
		i = 0  #sufix for blockname cause Blender-objectnamelength-limit
		while blockname_short in blocknamesmap.keys():
			i += 1
			suffix = str(i)
			blockname_short = blockname_short[:-2] + suffix
		blocknamesmap[n] = blockname_short
	#print 'deb:getBlocknamesmap blocknamesmap:\n', blocknamesmap #------------
	return blocknamesmap


def drawEntities(entities, settings, block_def=None):  #----------------------------------------
	"""Draw every kind of thing in the entity list.

	If provided 'block_def': the entities are to be added to the Blender 'group'.
	"""
	for _type in type_map.keys():
		#print 'deb:drawEntities_type:', _type #------------------
		# for each known type get a list of that type and call the associated draw function
		entities_type = entities.get_type(_type)
		if entities_type: drawer(_type, entities_type, settings, block_def)


def drawer(_type, entities, settings, block_def):  #------------------------------------------
	"""Call with a list of entities and a settings object to generate Blender geometry.

	If 'block_def': the entities are to be added to the Blender 'group'.
	"""
	global layersmap, layersmapshort
	#print 'deb:drawer _type, entities:\n ', _type, entities  #-----------------------

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
		#print 'deb:drawer entities:\n ', entities  #-----------------------

		len_temp = len(entities)
		# filtering only model-space enitities (no paper-space enitities)
		if settings.var['paper_space_on']:
			entities = [entity for entity in entities if entity.space != 0]
		else:
			entities = [entity for entity in entities if entity.space == 0]

		# filtering only objects with color from acceptedColorsList
		if settings.var['colorFilter_on']:
			entities = [entity for entity in entities if entity.color in settings.acceptedColors]

		# filtering only objects on layers from acceptedLayersList
		if settings.var['layerFilter_on']:
			#entities = [entity for entity in entities if entity.layer[0] in ['M','3','0'] and not entity.layer.endswith('H')]
			entities = [entity for entity in entities if entity.layer in settings.acceptedLayers]

		# patch for incomplete layer table in HL2-DXF-files 
		if layersmap:
			for entity in entities:
				oblayer = entity.layer
				if oblayer not in layersmap.keys():
					layer_obj = Layer(None, name=oblayer)
					layersmap[oblayer] = layer_obj
					layername_short = oblayer[:MAX_NAMELENGTH-1]
					i = 0  #sufix for layernames cause Blender-objectnames-limits
					while layername_short in layernamesmap.keys():
						i += 1
						suffix = str(i) #--todo--set zero-leading number format
						layername_short = layername_short[:-2] + suffix
					layernamesmap[oblayer] = layername_short

		# filtering only objects on not-frozen layers
		if layersmap and not settings.var['layFrozen_on']:
			entities = [entity for entity in entities if not layersmap[entity.layer].frozen]

		global activObjectLayer, activObjectName
		activObjectLayer = ''
		activObjectName = ''

		message = "Drawing dxf \'%ss\'..." %_type
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
					#print 'deb:drawer show_progress=',show_progress  #----------------
				
			# get the layer group (just to make things a little cleaner)
			if settings.var['group_bylayer_on'] and not block_def:
				group = getGroup('l:%s' % layernamesmap[entity.layer])

			if _type == 'insert':   #---- INSERT and MINSERT=array --------------------
				if not settings.var['block_nn']:  #----turn off support for noname BLOCKs
					prefix = entity.name[:2]
					if prefix in ('*X', '*U', '*D'):
						#print 'deb:drawer entity.name:', entity.name #------------
						continue
				if settings.var['blockFilter_on'] and not settings.accepted_block(entity.name):
					continue

				#print 'deb:insert entity.loc:', entity.loc #----------------
				insertFlag = True
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
						if block_def:
							blockFlag = True
							bl_loc = block_def[1]
							ob.loc = [ob.loc[0]-bl_loc[0],ob.loc[1]-bl_loc[1],ob.loc[2]-bl_loc[2]]
						else: blockFlag = False
						setObjectProperties(ob, group, entity, settings, block_def)
						if ob:
							if settings.var['optimization'] <= settings.MIN:
								#if settings.var['g_origin_on'] and not block_def: ob.loc = Mathutils.Vector(ob.loc) + settings.g_origin
								if settings.var['g_scale_on']: globalScaleOne(ob, insertFlag, blockFlag, settings.var['g_scale'])
								settings.redraw()
							else: oblist.append((ob, insertFlag, blockFlag))
					
			else:   #---draw entities except BLOCKs/INSERTs---------------------
				insertFlag = False
				alt_obname = activObjectName
				ob = entity.draw(settings)
				if ob:
					if M_OBJ and ob.type=='Mesh': #'Curve', 'Text'
						if block_def:
							blockFlag = True
							bl_loc = block_def[1]
							ob.loc = [ob.loc[0]-bl_loc[0],ob.loc[1]-bl_loc[1],ob.loc[2]-bl_loc[2]]
						car_nr()
	
					elif ob.name != alt_obname:
						if block_def:
							blockFlag = True
							bl_loc = block_def[1]
							ob.loc = [ob.loc[0]-bl_loc[0],ob.loc[1]-bl_loc[1],ob.loc[2]-bl_loc[2]]
						else: blockFlag = False
						setObjectProperties(ob, group, entity, settings, block_def)
						if settings.var['optimization'] <= settings.MIN:
							#if settings.var['g_origin_on'] and not block_def: ob.loc = Mathutils.Vector(ob.loc) + settings.g_origin
							if settings.var['g_scale_on']: globalScaleOne(ob, insertFlag, blockFlag, settings.var['g_scale'])
							settings.redraw()
						else: oblist.append((ob, insertFlag, blockFlag))
	
		#print 'deb:Finished drawing:', entities[0].type   #------------------------
		message = "\nDrawing dxf\'%ss\' done!" % _type
		settings.write(message, True)



def globalScale(oblist, SCALE):  #---------------------------------------------------------
	"""Global_scale for list of all imported objects.

	oblist is a list of pairs (ob, insertFlag), where insertFlag=True/False
	"""
	#print 'deb:globalScale.oblist: ---------%\n', oblist #---------------------
	for l in oblist:
		ob, insertFlag, blockFlag  = l[0], l[1], l[2]
		globalScaleOne(ob, insertFlag, blockFlag, SCALE)


def globalScaleOne(ob, insertFlag, blockFlag, SCALE):  #---------------------------------------------------------
	"""Global_scale imported object.
	"""
	#print 'deb:globalScaleOne  ob: ', ob #---------------------
	if settings.var['g_origin_on'] and not blockFlag:
		ob.loc = Mathutils.Vector(ob.loc) + settings.g_origin

	SCALE_MAT= Mathutils.Matrix([SCALE,0,0,0],[0,SCALE,0,0],[0,0,SCALE,0],[0,0,0,1])
	if insertFlag:  # by BLOCKs/INSERTs only insert-point coords must be scaled------------
		ob.loc = Mathutils.Vector(ob.loc) * SCALE_MAT
	else:   # entire scaling for all other imported objects ------------
		if ob.type == 'Mesh':		
			me = ob.getData(name_only=False, mesh=True)
			#me = Mesh.Get(ob.name)
			# set centers of all objects in (0,0,0)
			#me.transform(ob.matrixWorld*SCALE_MAT) 
			#ob.loc = Mathutils.Vector([0,0,0])
			# preseve centers of all objects
			me.transform(SCALE_MAT) 
			ob.loc = Mathutils.Vector(ob.loc) * SCALE_MAT
		else: #--todo-- also for curves: neutral scale factor after import
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
		setGroup(block_def[0], ob)
		#print 'deb:setObjectProperties  \'%s\' set to block_def_group!' %ob.name #---------
		ob.layers = [19]
	else:
		#ob.layers = [i+1 for i in xrange(20)] #remi--todo------------
		ob.layers = [settings.var['target_layer']]

	# Set material for any objects except empties
	if ob.type != 'Empty' and settings.var['material_on']:
		setMaterial_from(entity, ob, settings, block_def)

	# Set the visibility
	#if settings.layer_isOff(entity.layer):
	if layersmap and layersmap[entity.layer].color < 0: # color is negative if layer is off
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
		if entity.color_index == BYLAYER or entity.color_index == 256:
			mat = settings.colMaterials(entity.layer)
		elif entity.color_index == BYBLOCK or entity.color_index == 0:
			#--todo-- looking for block.color_index
			#mat = settings.colMaterials(block.color_index)
			#if block_def: mat = settings.colMaterials(block_def[2])
			mat = settings.colMaterials(3)
		else:
			mat = settings.colMaterials(entity.color_index)

	elif settings.var['material_from'] == 2: # 2= material from layer_name
		mat = settings.layMaterials(layername=entity.layer)

	elif settings.var['material_from'] == 3: # 3= material from layer+color
		mat = settings.layMaterials(layername=entity.layer, color=entity.color_index)

#	elif settings.var['material_from'] == 4: # 4= material from block_name

#	elif settings.var['material_from'] == 5: # 5= material from XDATA

#	elif settings.var['material_from'] == 6: # 6= material from INI-file

	else:					   # set neutral material
		try:
			mat = Material.Get('dxf-neutral')
		except:
			mat = Material.New('dxf-neutral')
			mat.setRGBCol(color_map[3])
			try:mat.setMode('Shadeless', 'Wire') #work-around for 2.45rc1-bug
			except:
			 mat.mode |= Material.Modes.SHADELESS #
			 mat.mode |= Material.Modes.WIRE
	try:
		#print 'deb:material mat:', mat #-----------
		ob.setMaterials([mat])  #assigns Blender-material to object
	except ValueError:
		settings.write("material error - \'%s\'!" %mat)
	ob.colbits = 0x01 # Set OB materials.



def calcBulge(p1, p2, arc_res, triples=False):   #-------------------------------------------------
	"""given startpoint, endpoint and bulge of arc, returns points/segments of its representation.

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

	calculate the center, radius, start angle, and end angle
	returns points/segments of its mesh representation
	incl.startpoint, without endpoint
	"""

	bulge = p1.bulge
	p1 = Mathutils.Vector(p1.loc)
	p2 = Mathutils.Vector(p2.loc)
	cord = p2 - p1 # vector from p1 to p2
	clength = cord.length
	s = (bulge * clength)/2.0 # sagitta (height)
	radius = abs(((clength/2.0)**2.0 + s**2.0)/(2.0*s)) # magic formula
	angle = (degrees(4.0*atan(bulge))) # theta (included angle)
	radial = cord.normalize() * radius # a radius length vector aligned with cord
	delta = (180.0 - abs(angle))/2.0 # the angle from cord to center
	if bulge < 0: delta = -delta
	rmat = Mathutils.RotationMatrix(-delta, 3, 'Z')
	center = p1 + (rmat * radial) # rotate radial by delta degrees, then add to p1 to find center
	#length = radians(abs(angle)) * radius
	#print 'deb:calcBulge:\n angle, delta: ', angle, delta  #----------------
	#print 'deb:center, radius: ', center, radius  #----------------------
	startpoint = p1 - center
	endpoint = p2 - center
	#print 'deb:calcBulg: startpoint:', startpoint  #---------
	#print 'deb:calcBulg: endpoint:', endpoint  #---------

	if not triples: #IF mesh-representation -----------
		if arc_res > 1024: arc_res = 1024 
		elif arc_res < 4: arc_res = 4 
		pieces = int(abs(angle)/(360.0/arc_res)) # set a fixed step of ARC_RESOLUTION
		if pieces < 3: pieces = 3
	else:  #IF curve-representation -------------------------------
		if arc_res > 32: arc_res = 32
		elif arc_res < 3: arc_res = 3 
		pieces = int(abs(angle)/(360.0/arc_res)) # set a fixed step of ARC_RESOLUTION
		if pieces < 2: pieces = 2

	step = angle/pieces  # set step so pieces * step = degrees in arc
	stepmatrix = Mathutils.RotationMatrix(-step, 3, "Z")

	if not triples: #IF mesh-representation -----------
		points = [startpoint]
		point = startpoint
		for i in xrange(int(pieces)-1):  #fast (but not so acurate as: vector * RotMatrix(-step*i,3,"Z")
			point = stepmatrix * point
			points.append(point)
		points = [ point+center for point in points]
		# vector to point convertion:
		points = [list(point) for point in points]
		return points, list(center)

	else:  #IF curve-representation -------------------------------
		# correct Bezier curves representation for free segmented circles/arcs
		step2 = radians(step * 0.5)
		bulg = radius * (1 - cos(step2))
		deltaY = 4.0 * bulg / (3.0 * sin(step2) )
		#print 'deb:calcArcCurve: bulg, deltaY:\n',  bulg, deltaY  #---------
		#print 'deb:calcArcCurve: step:\n',  step  #---------

		#org handler0 = Mathutils.Vector(0.0, -deltaY, 0.0)
		#handler = startmatrix * handler0
		#endhandler = endmatrix * handler0
		rotMatr90 = Mathutils.Matrix([0, -1, 0], [1, 0, 0], [0, 0, 1])
		handler = rotMatr90 * startpoint
		handler = - deltaY * handler.normalize()
		endhandler = rotMatr90 * endpoint
		endhandler = - deltaY * endhandler.normalize()
	
		points = [startpoint]
		handlers1 = [startpoint + handler]
		handlers2 = [startpoint - handler]
		point = Mathutils.Vector(startpoint)
		for i in xrange(int(pieces)-1):
			point = stepmatrix * point
			handler = stepmatrix * handler
			handler1 = point + handler
			handler2 = point - handler
			points.append(point)
			handlers1.append(handler1)
			handlers2.append(handler2)
		points.append(endpoint)
		handlers1.append(endpoint + endhandler)
		handlers2.append(endpoint - endhandler)

		points = [point + center for point in points]
		handlers1 = [point + center for point in handlers1]
		handlers2 = [point + center for point in handlers2]

		VectorTriples = [list(h1)+list(p)+list(h2) for h1,p,h2 in zip(handlers1, points, handlers2)]
		#print 'deb:calcBulgCurve: handlers1:\n', handlers1  #---------
		#print 'deb:calcBulgCurve: points:\n', points  #---------
		#print 'deb:calcBulgCurve: handlers2:\n', handlers2  #---------
		#print 'deb:calcBulgCurve: VectorTriples:\n', VectorTriples  #---------
		return VectorTriples


	

def calcArc(center, radius, start, end, arc_res, triples):  #-----------------------------------------
	"""calculate Points (or BezierTriples) for ARC/CIRCLEs representation.
	
	Given parameters of the ARC/CIRCLE,
	returns points/segments (or BezierTriples) and centerPoint
	"""
	# center is currently set by object
	# if start > end: start = start - 360
	if end > 360: end = end % 360.0

	startmatrix = Mathutils.RotationMatrix(-start, 3, "Z")
	startpoint = startmatrix * Mathutils.Vector(radius, 0, 0)
	endmatrix = Mathutils.RotationMatrix(-end, 3, "Z")
	endpoint = endmatrix * Mathutils.Vector(radius, 0, 0)

	if end < start: end +=360.0
	angle = end - start
	#length = radians(angle) * radius

	if not triples: #IF mesh-representation -----------
		if arc_res > 1024: arc_res = 1024 
		elif arc_res < 4: arc_res = 4 
		pieces = int(abs(angle)/(360.0/arc_res)) # set a fixed step of ARC_RESOLUTION
		if pieces < 3: pieces = 3
		step = angle/pieces # set step so pieces * step = degrees in arc
		stepmatrix = Mathutils.RotationMatrix(-step, 3, "Z")

		points = [startpoint]
		point = startpoint
		for i in xrange(int(pieces)-1):
			point = stepmatrix * point
			points.append(point)
		points.append(endpoint)
	
		if center:
			centerVec = Mathutils.Vector(center)
			#points = [point + centerVec for point in points()]
			points = [point + centerVec for point in points]
		# vector to point convertion:
		points = [list(point) for point in points]
		return points

	else:  #IF curve-representation ---------------
		if arc_res > 32: arc_res = 32
		elif arc_res < 3: arc_res = 3 
		pieces = int(abs(angle)/(360.0/arc_res)) # set a fixed step of ARC_RESOLUTION
		if pieces < 2: pieces = 2
		step = angle/pieces # set step so pieces * step = degrees in arc
		stepmatrix = Mathutils.RotationMatrix(-step, 3, "Z")

		# correct Bezier curves representation for free segmented circles/arcs
		step2 = radians(step * 0.5)
		bulg = radius * (1 - cos(step2))
		deltaY = 4.0 * bulg / (3.0 * sin(step2) )
		#print 'deb:calcArcCurve: bulg, deltaY:\n',  bulg, deltaY  #---------
		#print 'deb:calcArcCurve: step:\n',  step  #---------
		handler0 = Mathutils.Vector(0.0, -deltaY, 0.0)
	
		points = [startpoint]
		handler = startmatrix * handler0
		endhandler = endmatrix * handler0
		handlers1 = [startpoint + handler]
		handlers2 = [startpoint - handler]
		point = Mathutils.Vector(startpoint)
		for i in xrange(int(pieces)-1):
			point = stepmatrix * point
			handler = stepmatrix * handler
			handler1 = point + handler
			handler2 = point - handler
			points.append(point)
			handlers1.append(handler1)
			handlers2.append(handler2)
		points.append(endpoint)
		handlers1.append(endpoint + endhandler)
		handlers2.append(endpoint - endhandler)
		VectorTriples = [list(h1)+list(p)+list(h2) for h1,p,h2 in zip(handlers1, points, handlers2)]
		#print 'deb:calcArcCurve: handlers1:\n', handlers1  #---------
		#print 'deb:calcArcCurve: points:\n', points  #---------
		#print 'deb:calcArcCurve: handlers2:\n', handlers2  #---------
		#print 'deb:calcArcCurve: VectorTriples:\n', VectorTriples  #---------
		return VectorTriples


def drawCurveCircle(circle):  #--- no more used --------------------------------------------
	"""Given a dxf circle object return a blender circle object using curves.
	"""
	c = Curve.New('circle') # create new  curve data
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
		point.radius = 1.0
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
		point.radius = 1.0
	curve.flagU = 1 # Set curve cyclic
	a.update()

	ob = Object.New('Curve', 'arc') # make curve object
	return ob




# GUI STUFF -----#################################################-----------------
from Blender.BGL import glColor3f, glRecti, glClear, glRasterPos2d

EVENT_NONE = 1
EVENT_START = 2
EVENT_REDRAW = 3
EVENT_LOAD_INI = 4
EVENT_SAVE_INI = 5
EVENT_RESET = 6
EVENT_CHOOSE_INI = 7
EVENT_CHOOSE_DXF = 8
EVENT_HELP = 9
EVENT_PRESETCURV = 10
EVENT_PRESETS = 11
EVENT_DXF_DIR = 12
#         = 13
EVENT_LIST = 14
EVENT_ORIGIN = 15
EVENT_SCALE = 16
EVENT_PRESET2D = 20
EVENT_PRESET3D = 21
EVENT_EXIT = 100
GUI_EVENT = EVENT_NONE

GUI_A = {}  # GUI-buttons dictionary for parameter
GUI_B = {}  # GUI-buttons dictionary for drawingTypes

# settings default, initialize ------------------------

points_as_menu  = "convert to: %t|empty %x1|mesh.vertex %x2|thin sphere %x3|thin box %x4|..curve.vertex %x5"
lines_as_menu   = "convert to: %t|..edge %x1|mesh %x2|..thin cylinder %x3|thin box %x4|Bezier-curve %x5|..NURBS-curve %x6"
mlines_as_menu  = "convert to: %t|..edge %x1|..mesh %x2|..thin cylinder %x3|..thin box %x|..curve %x5"
plines_as_menu  = "convert to: %t|..edge %x1|mesh %x2|..thin cylinder %x3|..thin box %x4|Bezier-curve %x5|NURBS-curve %x6"
splines_as_menu = "convert to: %t|mesh %x2|..thin cylinder %x3|..thin box %x4|Bezier-curve %x5|NURBS-curve %x6"
plines3_as_menu = "convert to: %t|..edge %x1|mesh %x2|..thin cylinder %x3|..thin box %x4|Bezier-curve %x5|NURBS-curve %x6"
plmesh_as_menu  = "convert to: %t|..edge %x1|mesh %x2|..NURBS-surface %x6"
solids_as_menu  = "convert to: %t|..edge %x1|mesh %x2"
blocks_as_menu  = "convert to: %t|dupliGroup %x1|..real.Group %x2|..exploded %x3"
texts_as_menu   = "convert to: %t|text %x1|..mesh %x2|..curve %x5"
material_from_menu= "material from: %t|..LINESTYLE %x7|COLOR %x1|LAYER %x2|..LAYER+COLOR %x3|..BLOCK %x4|..XDATA %x5|..INI-File %x6"
g_scale_list	= ''.join((
	'scale factor: %t',
	'|user def. %x12',
	'|yard to m %x8',
	'|feet to m %x7',
	'|inch to m %x6',
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
	'curves_on' : 0,
	'optimization': 2,
	'one_mesh_on': 1,
	'vGroup_on' : 1,
	'dummy_on' : 0,
	'views_on' : 0,
	'cams_on'  : 0,
	'lights_on' : 0,
	'xref_on' : 1,
	'block_nn': 0,
	'blockFilter_on': 0,
	'layerFilter_on': 0,
	'colorFilter_on': 0,
	'groupFilter_on': 0,
	'newScene_on' : 1,
	'target_layer' : TARGET_LAYER,
	'group_bylayer_on' : GROUP_BYLAYER,
	'g_originX'   : G_ORIGIN_X,
	'g_originY'   : G_ORIGIN_Y,
	'g_originZ'   : G_ORIGIN_Z,
	'g_origin_on': 0,
	'g_scale'   : float(G_SCALE),
#	'g_scale_as': int(log10(G_SCALE)), #   0,
	'g_scale_on': 0,
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
	'fill_on'	: 1,
	'meshSmooth_on': 1,
	'curve_res' : CURV_RESOLUTION,
	'curve_arc' : CURVARC_RESOLUTION,
	'arc_res'   : ARC_RESOLUTION,
	'arc_rad'   : ARC_RADIUS,
	'thin_res'  : THIN_RESOLUTION,
	'pl_trim_max' : TRIM_LIMIT,
	'pl_trim_on': 1,
	'plmesh_flip': 0,
	'normals_out': 0,
	'paper_space_on': 0,
	'layFrozen_on': 0,
	'Z_force_on': 0,
	'Z_elev': float(ELEVATION),
	'points_as' : 2,
	'lines_as'  : 2,
	'mlines_as' : 2,
	'plines_as' : 2,
	'splines_as' : 5,
	'plines3_as': 2,
	'plmesh_as' : 2,
	'solids_as' : 2,
	'blocks_as' : 1,
	'texts_as'  : 1
	}

drawTypes_org = {
	'point' : 1,
	'line'  : 1,
	'arc'   : 1,
	'circle': 1,
	'ellipse': 1,
	'mline' : 0,
	'polyline': 1,
	'spline': 1,
	'plmesh': 1,
	'pline3': 1,
	'lwpolyline': 1,
	'text'  : 1,
	'mtext' : 0,
	'block' : 1,
	'insert': 1,
	'solid' : 1,
	'trace' : 1,
	'face'  : 1,
#	'view' : 0,
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
	rdict = Registry.GetKey('DXF_Importer', cache)
	if not rdict: rdict = {}
	if item:
		rdict[key] = item
		Registry.SetKey('DXF_Importer', rdict, cache)
		#print  'deb:update_RegistryKey rdict', rdict #---------------


def check_RegistryKey(key):
	""" check if the key is already there (saved on a previous execution of this script)
	"""
	cache = True # data is also saved to a file
	rdict = Registry.GetKey('DXF_Importer', cache)
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
		#	l_name, l_data = key, GUI_A[key].val
		#	list_A

		output_str = '[%s,%s]' %(GUI_A, GUI_B)
		if output_str =='None':
			Draw.PupMenu('DXF importer: INI-file:  Alert!%t|no config-data present to save!')
		else:
			#if BPyMessages.Warning_SaveOver(iniFile): #<- remi find it too abstarct
			if sys.exists(iniFile):
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
					#Draw.PupMenu('DXF importer: INI-file: Done!%t|config-data saved in ' + '\'%s\'' %iniFile)
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
	update_RegistryKey('iniFileName', iniFile)
	#print 'deb:loadConfig iniFile: ', iniFile #----------------------
	if iniFile.lower().endswith(INIFILE_EXTENSION) and sys.exists(iniFile):
		f = file(iniFile, 'r')
		header_str = f.readline()
		if header_str.startswith(INIFILE_HEADER):
			data_str = f.read()
			f.close()
			#print 'deb:loadConfig data_str from %s: \n' %iniFile , data_str #-----------------
			data = eval(data_str)
			for k, v in data[0].iteritems():
				try: GUI_A[k].val = v
				except:	GUI_A[k] = Draw.Create(v)
			for k, v in data[1].iteritems():
				try: GUI_B[k].val = v
				except:	GUI_B[k] = Draw.Create(v)
		else:
			f.close()
			Draw.PupMenu('DXF importer: INI-file:  Alert!%t|no valid header in INI-file: ' + '\'%s\'' %iniFile)
	else:
		Draw.PupMenu('DXF importer: INI-file:  Alert!%t|no valid INI-file selected!')
		print "DXF importer: Alert!: no valid INI-file selected."
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


def presetConfig_curv(activate):  #-----------------------------------------------
	"""Sets settings/config/materials for curve representation.

	"""
	global GUI_A
	if activate:
		GUI_A['curves_on'].val = 1
		GUI_A['points_as'].val = 5
		GUI_A['lines_as'].val  = 5
		GUI_A['mlines_as'].val = 5
		GUI_A['plines_as'].val = 5
		GUI_A['splines_as'].val = 5
		GUI_A['plines3_as'].val = 5
	else:
		GUI_A['curves_on'].val = 0
		GUI_A['points_as'].val = 2
		GUI_A['lines_as'].val  = 2
		GUI_A['mlines_as'].val = 2
		GUI_A['plines_as'].val = 2
		GUI_A['splines_as'].val = 6
		GUI_A['plines3_as'].val = 2

	
def resetDefaultConfig_2D():  #-----------------------------------------------
	"""Sets settings/config/materials to defaults 2D.

	"""
	presetConfig_curv(1)
	keywords2d = {
		'views_on' : 0,
		'cams_on'  : 0,
		'lights_on' : 0,
		'vGroup_on' : 1,
		'thick_on'  : 0,
		'thick_force': 0,
		'width_on'  : 1,
		'width_force': 0,
		'dist_on'   : 1,
		'dist_force': 0,
		'fill_on'	: 0,
		'pl_trim_on': 1,
		'Z_force_on': 0,
		'meshSmooth_on': 0,
		'solids_as' : 2,
		'blocks_as' : 1,
		'texts_as'  : 1
		}

	drawTypes2d = {
		'point' : 1,
		'line'  : 1,
		'arc'   : 1,
		'circle': 1,
		'ellipse': 1,
		'mline' : 0,
		'polyline': 1,
		'spline': 1,
		'plmesh': 0,
		'pline3': 1,
		'lwpolyline': 1,
		'text'  : 1,
		'mtext' : 0,
		'block' : 1,
		'insert': 1,
		'solid' : 1,
		'trace' : 1,
		'face'  : 0,
#		'view' : 0,
		}

	updateConfig(keywords2d, drawTypes2d)

def resetDefaultConfig_3D():  #-----------------------------------------------
	"""Sets settings/config/materials to defaults 3D.

	"""
	presetConfig_curv(0)
	keywords3d = {
#		'views_on' : 1,
#		'cams_on'  : 1,
#		'lights_on' : 1,
		'vGroup_on' : 1,
		'thick_on'  : 1,
		'thick_force': 0,
		'width_on'  : 1,
		'width_force': 0,
		'dist_on'   : 1,
		'dist_force': 0,
		'fill_on'	: 1,
		'pl_trim_on': 1,
		'Z_force_on': 0,
		'meshSmooth_on': 1,
		'solids_as' : 2,
		'blocks_as' : 1,
		'texts_as'  : 1
		}

	drawTypes3d = {
		'point' : 1,
		'line'  : 1,
		'arc'   : 1,
		'circle': 1,
		'ellipse': 1,
		'mline' : 0,
		'polyline': 1,
		'spline': 1,
		'plmesh': 1,
		'pline3': 1,
		'lwpolyline': 1,
		'text'  : 0,
		'mtext' : 0,
		'block' : 1,
		'insert': 1,
		'solid' : 1,
		'trace' : 1,
		'face'  : 1,
#		'view' : 0,
		}

	updateConfig(keywords3d, drawTypes3d)


def	inputGlobalScale():
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

	
def	inputOriginVector():
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


def draw_UI():  #-----------------------------------------------------------------
	""" Draw startUI and setup Settings.
	"""
	global GUI_A, GUI_B #__version__
	global user_preset, iniFileName, dxfFileName, config_UI, g_scale_as
	global model_space_on

	# This is for easy layout changes
	but_0c = 70  #button 1.column width
	but_1c = 70  #button 1.column width
	but_2c = 70  #button 2.column
	but_3c = 70  #button 3.column
	menu_margin = 10
	butt_margin = 10
	menu_w = (3 * butt_margin) + but_0c + but_1c + but_2c + but_3c  #menu width

	simple_menu_h = 100
	extend_menu_h = 350
	y = simple_menu_h		 # y is menu upper.y
	if config_UI.val: y += extend_menu_h
	x = 20 #menu left.x
	but0c = x + menu_margin  #buttons 0.column position.x
	but1c = but0c + but_0c + butt_margin
	but2c = but1c + but_1c + butt_margin
	but3c = but2c + but_2c + butt_margin
	but4c = but3c + but_3c

	# Here starts menu -----------------------------------------------------
	#glClear(GL_COLOR_BUFFER_BIT)
	#glRasterPos2d(8, 125)

	y += 30
	colorbox(x, y+20, x+menu_w+menu_margin*2, menu_margin)
	Draw.Label("DXF/DWG-Importer v" + __version__, but0c, y, menu_w, 20)

	if config_UI.val:
		b0, b0_ = but0c, but_0c + butt_margin
		b1, b1_ = but1c, but_1c
		y_top = y

		y -= 10
		y -= 20
		Draw.BeginAlign()
		GUI_B['point'] = Draw.Toggle('POINT', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['point'].val, "support dxf-POINT on/off")
		if GUI_B['point'].val:
			GUI_A['points_as'] = Draw.Menu(points_as_menu, EVENT_NONE, b1, y, b1_, 20, GUI_A['points_as'].val, "select target Blender-object")
#		Draw.Label('-->', but2c, y, but_2c, 20)
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['line'] = Draw.Toggle('LINE...etc', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['line'].val, "support dxf-LINE,ARC,CIRCLE,ELLIPSE on/off")
		if GUI_B['line'].val:
			GUI_A['lines_as'] = Draw.Menu(lines_as_menu, EVENT_NONE, but1c, y, but_1c, 20, GUI_A['lines_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['mline'] = Draw.Toggle('..MLINE', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['mline'].val, "(*todo)support dxf-MLINE on/off")
		if GUI_B['mline'].val:
			GUI_A['mlines_as'] = Draw.Menu(mlines_as_menu, EVENT_NONE, but1c, y, but_1c, 20, GUI_A['mlines_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['spline'] = Draw.Toggle('SPLINE', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['spline'].val, "support dxf-SPLINE on/off")
		if GUI_B['spline'].val:
			GUI_A['splines_as'] = Draw.Menu(splines_as_menu, EVENT_NONE, but1c, y, but_1c, 20, GUI_A['splines_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['polyline'] = Draw.Toggle('2D/LWPLINE', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['polyline'].val, "support dxf-2D-POLYLINE on/off")
		if GUI_B['polyline'].val:
			GUI_A['plines_as'] = Draw.Menu(plines_as_menu, EVENT_NONE, but1c, y, but_1c, 20, GUI_A['plines_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_B['pline3'] = Draw.Toggle('3D-PLINE', EVENT_REDRAW, b0, y, b0_, 20, GUI_B['pline3'].val, "support dxf-3D-POLYLINE on/off")
		if GUI_B['pline3'].val:
			GUI_A['plines3_as'] = Draw.Menu(plines3_as_menu, EVENT_NONE, but1c, y, but_1c, 20, GUI_A['plines3_as'].val, "select target Blender-object")
		Draw.EndAlign()

		y_down = y
		# -----------------------------------------------

		y = y_top
		b0, b0_ = but2c, but_2c + butt_margin
		b1, b1_ = but3c, but_3c

		y -= 10
		y -= 20
		Draw.BeginAlign()
		GUI_B['plmesh'] = Draw.Toggle('PL-MESH/FACE', EVENT_NONE, b0, y, b0_+b1_-40, 20, GUI_B['plmesh'].val, "support dxf-POLYMESH/POLYFACE on/off")
#		GUI_A['plmesh_as'] = Draw.Menu(plmesh_as_menu, EVENT_NONE, but1c, y, but_1c, 20, GUI_A['plmesh_as'].val, "select target Blender-object")
		GUI_A['plmesh_flip'] = Draw.Toggle('N', EVENT_NONE, b1+b1_-40, y, 20, 20, GUI_A['plmesh_flip'].val, "flip DXF normals on/off")
		GUI_A['normals_out'] = Draw.Toggle('N', EVENT_NONE, b1+b1_-20, y, 20, 20, GUI_A['normals_out'].val, "force Blender normals to outside on/off")
		Draw.EndAlign()

		y -= 20
		GUI_B['solid'] = Draw.Toggle('SOLID', EVENT_NONE, b0, y, b0_, 20, GUI_B['solid'].val, "support dxf-SOLID and TRACE on/off")
		GUI_B['face'] = Draw.Toggle('3DFACE', EVENT_NONE, b1, y, b1_, 20, GUI_B['face'].val, "support dxf-3DFACE on/off")
#		GUI_A['solids_as'] = Draw.Menu(solids_as_menu, EVENT_NONE, but3c, y, but_3c, 20, GUI_A['solids_as'].val, "select target Blender-object")
		#print 'deb:support solid, trace', GUI_B['trace'].val, GUI_B['solid'].val # ------------


		y -= 20
		GUI_B['text'] = Draw.Toggle('TEXT', EVENT_NONE, b0, y, b0_, 20, GUI_B['text'].val, "support dxf-TEXT on/off")
		GUI_B['mtext'] = Draw.Toggle('..MTEXT', EVENT_NONE, b1, y, b1_, 20, GUI_B['mtext'].val, "(*todo)support dxf-MTEXT on/off")
#		GUI_A['texts_as'] = Draw.Menu(texts_as_menu, EVENT_NONE, but3c, y, but_3c, 20, GUI_A['texts_as'].val, "select target Blender-object")

		y -= 20
		Draw.BeginAlign()
		GUI_B['block'] = Draw.Toggle('BLOCK', EVENT_REDRAW, b0, y, b0_-30, 20, GUI_B['block'].val, "support dxf-BLOCK and ARRAY on/off")
		GUI_B['insert'].val = GUI_B['block'].val
		if GUI_B['block'].val:
			GUI_A['block_nn'] = Draw.Toggle('n', EVENT_NONE, b1-30, y, 15, 20, GUI_A['block_nn'].val, "support hatch/noname BLOCKs *X... on/off")
			GUI_A['xref_on'] = Draw.Toggle('Xref', EVENT_NONE, b1-15, y, 35, 20, GUI_A['xref_on'].val, "support for XREF-BLOCKs (place holders) on/off")
			GUI_A['blocks_as'] = Draw.Menu(blocks_as_menu, EVENT_NONE, b1+20, y, b1_-20, 20, GUI_A['blocks_as'].val, "select target representation for imported BLOCKs")
		Draw.EndAlign()


		y -= 20
		y -= 20
		
		Draw.BeginAlign()
		GUI_A['views_on'] = Draw.Toggle('views', EVENT_NONE, b0, y, b0_-25, 20, GUI_A['views_on'].val, "imports VIEWs and VIEWPORTs as cameras on/off")
		GUI_A['cams_on'] = Draw.Toggle('..cams', EVENT_NONE, b1-25, y, b1_-25, 20, GUI_A['cams_on'].val, "(*todo) support ASHADE cameras on/off")
		GUI_A['lights_on'] = Draw.Toggle('..lights', EVENT_NONE, b1+25, y, b1_-25, 20, GUI_A['lights_on'].val, "(*todo) support AVE_RENDER lights on/off")
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
		GUI_A['paper_space_on'] = Draw.Toggle('paper', EVENT_NONE, b0+but_*0, y, but_, 20, GUI_A['paper_space_on'].val, "import only from Paper-Space on/off")
		GUI_A['layFrozen_on'] = Draw.Toggle ('frozen', EVENT_NONE, b0+but_*1, y, but_, 20, GUI_A['layFrozen_on'].val, "import also from frozen LAYERs on/off")
		GUI_A['layerFilter_on'] = Draw.Toggle('..layer', EVENT_NONE, b0+but_*2, y, but_, 20, GUI_A['layerFilter_on'].val, "(*todo) LAYER filtering on/off")
		GUI_A['colorFilter_on'] = Draw.Toggle('..color', EVENT_NONE, b0+but_*3, y, but_, 20, GUI_A['colorFilter_on'].val, "(*todo) COLOR filtering on/off")
		GUI_A['groupFilter_on'] = Draw.Toggle('..group', EVENT_NONE, b0+but_*4, y, but_, 20, GUI_A['groupFilter_on'].val, "(*todo) GROUP filtering on/off")
		GUI_A['blockFilter_on'] = Draw.Toggle('..block', EVENT_NONE, b0+but_*5, y, but_, 20, GUI_A['blockFilter_on'].val, "(*todo) BLOCK filtering on/off")
		#GUI_A['dummy_on'] = Draw.Toggle('-', EVENT_NONE, but3c, y, but_3c, 20, GUI_A['dummy_on'].val, "dummy on/off")
		Draw.EndAlign()

		# -----end filters--------------------------------------

		b0, b0_ = but0c, but_0c + butt_margin
		b1, b1_ = but1c, but_1c

		y -= 10
		y -= 20
		Draw.BeginAlign()
		GUI_A['g_origin_on'] = Draw.Toggle('glob.reLoc', EVENT_REDRAW, b0, y, b0_, 20, GUI_A['g_origin_on'].val, "global relocate all DXF objects on/off")
		if GUI_A['g_origin_on'].val:
			tmp = Draw.PushButton('=', EVENT_ORIGIN, b1, y, 20, 20, "edit relocation-vector (x,y,z in DXF units)")
			origin_str = '(%.4f, %.4f, %.4f)'  % (
				GUI_A['g_originX'].val,
				GUI_A['g_originY'].val,
				GUI_A['g_originZ'].val
				)
			tmp = Draw.Label(origin_str, b1+20, y, 300, 20)
			#GUI_A['g_origin'] = Draw.String('', EVENT_ORIGIN, b1, y, b1_, 20, GUI_A['g_origin'].val, "global translation-vector (x,y,z) in DXF units")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_A['g_scale_on'] = Draw.Toggle('glob.Scale', EVENT_REDRAW, b0, y, b0_, 20, GUI_A['g_scale_on'].val, "global scale all DXF objects on/off")
		if GUI_A['g_scale_on'].val:
			g_scale_as = Draw.Menu(g_scale_list, EVENT_SCALE, b1, y, 45, 20, g_scale_as.val, "factor for scaling the DXFdata")
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

		y_down = y
		# -----end material,translate,scale------------------------------------------

		b0, b0_ = but0c, but_0c + butt_margin
		b1, b1_ = but1c, but_1c

		y_top = y_down
		y = y_top
		y -= 10
		y -= 20
		Draw.BeginAlign()
		GUI_A['meshSmooth_on'] = Draw.Toggle('smooth', EVENT_NONE, b0, y, b0_-20, 20, GUI_A['meshSmooth_on'].val, "mesh smooth for circles/arc-segments on/off")
		GUI_A['pl_trim_on'] = Draw.Toggle('trim', EVENT_NONE, b1-20, y, 32, 20, GUI_A['pl_trim_on'].val, "clean intersection of POLYLINE-wide-segments on/off")
		GUI_A['pl_trim_max'] = Draw.Number('', EVENT_NONE, b1+12, y,  b1_-12, 20, GUI_A['pl_trim_max'].val, 0, 5, "threshold intersection of POLYLINE-wide-segments: 0.0-5.0")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
#		GUI_A['thin_res'] = Draw.Number('thin:', EVENT_NONE, but0c, y, but_0c, 20, GUI_A['thin_res'].val, 4, 64, "thin cylinder resolution - number of segments (4-64)")
		GUI_A['arc_rad'] = Draw.Number('bR:', EVENT_NONE, b0, y, b0_, 20, GUI_A['arc_rad'].val, 0.01, 100, "basis radius for arc/circle resolution (0.01-100)")
		GUI_A['arc_res'] = Draw.Number('', EVENT_NONE, b1, y, b1_/2, 20, GUI_A['arc_res'].val, 3, 500, "arc/circle resolution - number of segments (3-500)")
		GUI_A['fill_on'] = Draw.Toggle('caps', EVENT_NONE, b1+b1_/2, y, b1_/2, 20, GUI_A['fill_on'].val, "draws top and bottom caps of CYLINDERs/closed curves on/off")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_A['curve_arc'] = Draw.Number('', EVENT_NONE, b0, y, b0_/2, 20, GUI_A['curve_arc'].val, 3, 32, "Bezier circle: amount of segments: 3-32")
		GUI_A['curve_res'] = Draw.Number('', EVENT_NONE, b0+b0_/2, y,  b0_/2, 20, GUI_A['curve_res'].val, 1, 128, "Set the Curve's U-resolution value: 1-128")
		GUI_A['curves_on'] = Draw.Toggle('to Curves', EVENT_PRESETCURV, b1, y, b1_, 20, GUI_A['curves_on'].val, "set Curve as target object type on/off")
		Draw.EndAlign()

		y -= 20
		GUI_A['group_bylayer_on'] = Draw.Toggle('Layer', EVENT_NONE, b0, y, 30, 20, GUI_A['group_bylayer_on'].val, "DXF-entities group by layer on/off")
		GUI_A['vGroup_on'] = Draw.Toggle('vGroups', EVENT_NONE, b0+30, y, b1_-10, 20, GUI_A['vGroup_on'].val, "sort faces into VertexGroups on/off")
		GUI_A['one_mesh_on'] = Draw.Toggle('oneMesh', EVENT_NONE, b1+10, y, b1_-10, 20, GUI_A['one_mesh_on'].val, "draw DXF-entities into one mesh-object. Recommended for big DXF-files. on/off")

		y -= 30
		Draw.BeginAlign()
		GUI_A['material_on'] = Draw.Toggle('material', EVENT_REDRAW, b0, y, b0_-20, 20, GUI_A['material_on'].val, "support for material assignment on/off")
		if GUI_A['material_on'].val:
			GUI_A['material_from'] = Draw.Menu(material_from_menu,   EVENT_NONE, b1-20, y, b1_+20, 20, GUI_A['material_from'].val, "material assignment from?")
		Draw.EndAlign()

		y_down = y
		# -----------------------------------------------

		b0, b0_ = but2c, but_2c + butt_margin
		b1, b1_ = but3c, but_3c

		y = y_top
		y -= 10
		y -= 20
		Draw.BeginAlign()
		GUI_A['Z_force_on'] = Draw.Toggle('.elevation', EVENT_REDRAW, b0, y, b0_, 20, GUI_A['Z_force_on'].val, ".set objects Z-coordinates to elevation on/off")
		if GUI_A['Z_force_on'].val:
			GUI_A['Z_elev'] = Draw.Number('', EVENT_NONE, b1, y, b1_, 20, GUI_A['Z_elev'].val, -1000, 1000, "set default elevation(Z-coordinate)")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_A['dist_on'] = Draw.Toggle('dist.:', EVENT_NONE, b0, y, b0_-20, 20, GUI_A['dist_on'].val, "support distance on/off")
		GUI_A['dist_force'] = Draw.Toggle('F', EVENT_NONE, b0+b0_-20, y,  20, 20, GUI_A['dist_force'].val, "force minimal distance on/off")
		GUI_A['dist_min'] = Draw.Number('', EVENT_NONE, b1, y, b1_, 20, GUI_A['dist_min'].val, 0, 10, "minimal length/distance (double.vertex removing)")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_A['thick_on'] = Draw.Toggle('thick:', EVENT_NONE, b0, y, b0_-20, 20, GUI_A['thick_on'].val, "support thickness on/off")
		GUI_A['thick_force'] = Draw.Toggle('F', EVENT_REDRAW, b0+b0_-20, y,  20, 20, GUI_A['thick_force'].val, "force for thickness at least limiter value on/off")
		if GUI_A['thick_force'].val:
			GUI_A['thick_min'] = Draw.Number('', EVENT_NONE, b1, y, b1_, 20, GUI_A['thick_min'].val, 0, 10, "minimal value for thickness")
		Draw.EndAlign()

		y -= 20
		Draw.BeginAlign()
		GUI_A['width_on'] = Draw.Toggle('width:', EVENT_NONE, b0, y, b0_-20, 20, GUI_A['width_on'].val, "support width on/off")
		GUI_A['width_force'] = Draw.Toggle('F', EVENT_REDRAW, b0+b0_-20, y, 20, 20, GUI_A['width_force'].val, "force for width at least limiter value on/off")
		if GUI_A['width_force'].val:
			GUI_A['width_min'] = Draw.Number('', EVENT_NONE, b1, y, b1_, 20, GUI_A['width_min'].val, 0, 10, "minimal value for width")
		Draw.EndAlign()

		y -= 30
		but, but_ = but2c, 25
		Draw.BeginAlign()
		Draw.EndAlign()

		if y < y_down: y_down = y
		# -----end options --------------------------------------


		#--------------------------------------
		y_top = y_down
		y = y_top
		#GUI_A['dummy_on'] = Draw.Toggle(' - ', EVENT_NONE, but0c, y, but_0c, 20, GUI_A['dummy_on'].val, "reserved")
		y -= 30
		Draw.BeginAlign()
		Draw.PushButton('INI file >', EVENT_CHOOSE_INI, but0c, y, but_0c, 20, 'Select INI-file from project directory')
		iniFileName = Draw.String(' :', EVENT_NONE, but1c, y, menu_w-but_1c-60, 20, iniFileName.val, FILENAME_MAX, "write here the name of the INI-file")
		but = but4c-60
		Draw.PushButton('#', EVENT_PRESETS, but, y, 20, 20, "toggle Preset-INI-files")
		Draw.PushButton('L', EVENT_LOAD_INI, but+20, y, 20, 20, 'Loads configuration from ini-file: %s' % iniFileName.val)
		Draw.PushButton('S', EVENT_SAVE_INI, but+40, y, 20, 20, 'Saves configuration to ini-file: %s' % iniFileName.val)
		Draw.EndAlign()


	b0, b0_ = but2c, but_2c + butt_margin
	b1, b1_ = but3c, but_3c

	y =	simple_menu_h
	bm = butt_margin/2

	#y -= 10
	Draw.BeginAlign()
	Draw.PushButton('DXFfile >', EVENT_CHOOSE_DXF, but0c, y, but_0c, 20, 'Select DXF/DWG-file for import')
	dxfFileName = Draw.String(' :', EVENT_NONE, but1c, y, but_1c+but_2c+but_3c-20, 20, dxfFileName.val, FILENAME_MAX, "type the name of DXF/DWG-file or type *.dxf/*.dwg for multiple files")
	Draw.PushButton('*.*', EVENT_DXF_DIR, but3c+but_3c-20, y, 20, 20, 'set filter for import all files from this directory')
	Draw.EndAlign()

	y -= 30
	config_UI = Draw.Toggle('CONFIG', EVENT_REDRAW, but0c, y, but_0c+bm, 20, config_UI.val, 'Advanced configuration on/off' )
	Draw.BeginAlign()
	but, but_ = but1c, but_1c+bm
	but_ /= 3
	Draw.PushButton('X', EVENT_RESET, but, y, 15, 20, "reset configuration to defaults")
	Draw.PushButton('2D', EVENT_PRESET2D, but+but_, y, but_, 20, 'set configuration for 2D import')
	Draw.PushButton('3D', EVENT_PRESET3D, but+(but_*2), y, but_, 20, 'set configuration for 3D import')
	Draw.EndAlign()

	Draw.BeginAlign()
	GUI_A['newScene_on'] = Draw.Toggle('newScene', EVENT_NONE, but2c, y, but_2c, 20, GUI_A['newScene_on'].val, "create new Scene for each imported dxf file on/off")
	GUI_A['target_layer'] = Draw.Number('layer', EVENT_NONE, but3c, y, but_3c, 20, GUI_A['target_layer'].val, 1, 18, "target Blender-layer (<19> reserved for block_definitions)")
	Draw.EndAlign()

	y -= 40
	Draw.PushButton('EXIT', EVENT_EXIT, but0c, y, but_0c+bm, 20, '' )
	Draw.PushButton('HELP', EVENT_HELP, but1c, y, but_1c+bm, 20, 'calls DXF-Importer Manual Page on Wiki.Blender.org')
	Draw.BeginAlign()
	GUI_A['optimization'] = Draw.Number('', EVENT_NONE, but2c, y+20, 40, 20, GUI_A['optimization'].val, 0, 3, "Optimization Level: 0=Debug/directDrawing, 1=Verbose, 2=ProgressBar, 3=SilentMode")
	Draw.EndAlign()
	Draw.BeginAlign()
	Draw.PushButton('TEST', EVENT_LIST, but2c, y, 40, 20, 'DXF-Analyze-Tool: reads data from selected dxf file and writes report in project_directory/dxf_blendname.INF')
	Draw.PushButton('START IMPORT', EVENT_START, but2c+40, y, but_2c-40+but_3c+butt_margin, 40, 'Start the import process. For Cancel go to console and hit Ctrl-C')
	Draw.EndAlign()




	y -= 20
	Draw.BeginAlign()
	Draw.Label(' ', but0c-menu_margin, y, menu_margin, 20)
	Draw.Label(LAB, but0c, y, menu_w, 20)
	Draw.Label(' ', but0c+menu_w, y, menu_margin, 20)
	Draw.EndAlign()

#-- END GUI Stuf-----------------------------------------------------

def colorbox(x,y,xright,bottom):
   glColor3f(0.75, 0.75, 0.75)
   glRecti(x + 1, y + 1, xright - 1, bottom - 1)

def dxf_callback(input_filename):
	global dxfFileName
	if input_filename.lower()[-3:] in ('dwg','dxf'):
		dxfFileName.val=input_filename
#	dirname == sys.dirname(Blender.Get('filename'))
#	update_RegistryKey('DirName', dirname)
#	update_RegistryKey('dxfFileName', input_filename)
	
def ini_callback(input_filename):
	global iniFileName
	iniFileName.val=input_filename

def event(evt, val):
	if evt in (Draw.QKEY, Draw.ESCKEY) and not val:
		Draw.Exit()

def bevent(evt):
#   global EVENT_NONE,EVENT_LOAD_DXF,EVENT_LOAD_INI,EVENT_SAVE_INI,EVENT_EXIT
	global config_UI, user_preset
	global GUI_A, UI_MODE

	######### Manages GUI events
	if (evt==EVENT_EXIT):
		Draw.Exit()
		print 'DXF/DWG-Importer  *** exit ***'   #---------------------
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
	elif (evt==EVENT_SCALE):
		if g_scale_as.val == 12:
			inputGlobalScale()
		if GUI_A['g_scale'].val < 0.00000001:
			GUI_A['g_scale'].val = 0.00000001
		Draw.Redraw()
	elif (evt==EVENT_ORIGIN):
		inputOriginVector()
		Draw.Redraw()
	elif (evt==EVENT_PRESET3D):
		resetDefaultConfig_3D()
		Draw.Redraw()
	elif (evt==EVENT_PRESETCURV):
		presetConfig_curv(GUI_A['curves_on'].val)
		Draw.Redraw()
	elif (evt==EVENT_PRESETS):
		user_preset += 1
		index = str(user_preset)
		if user_preset > 5: user_preset = 0; index = ''
		iniFileName.val = INIFILE_DEFAULT_NAME + index + INIFILE_EXTENSION
		Draw.Redraw()
	elif (evt==EVENT_LIST):
		dxfFile = dxfFileName.val
		update_RegistryKey('dxfFileName', dxfFileName.val)
		if dxfFile.lower().endswith('.dxf') and sys.exists(dxfFile):
			analyzeDXF(dxfFile)
		else:
			Draw.PupMenu('DXF importer:  Alert!%t|no valid DXF-file selected!')
			print "DXF importer: error, no valid DXF-file selected! try again"
		Draw.Redraw()
	elif (evt==EVENT_HELP):
		try:
			import webbrowser
			webbrowser.open('http://wiki.blender.org/index.php?title=Scripts/Manual/Import/DXF-3D')
		except:
			Draw.PupMenu('DXF importer: HELP Alert!%t|no connection to manual-page on Blender-Wiki!	try:|\
http://wiki.blender.org/index.php?title=Scripts/Manual/Import/DXF-3D')
		Draw.Redraw()
	elif (evt==EVENT_LOAD_INI):
		loadConfig()
		Draw.Redraw()
	elif (evt==EVENT_SAVE_INI):
		saveConfig()
		Draw.Redraw()
	elif (evt==EVENT_DXF_DIR):
		dxfFile = dxfFileName.val
		dxfFileExt = '*'+dxfFile.lower()[-4:]  #can be .dxf or .dwg
		dxfPathName = ''
		if '/' in dxfFile:
			dxfPathName = '/'.join(dxfFile.split('/')[:-1]) + '/'
		elif '\\' in dxfFile:
			dxfPathName = '\\'.join(dxfFile.split('\\')[:-1]) + '\\'
		dxfFileName.val = dxfPathName + dxfFileExt 
#		dirname == sys.dirname(Blender.Get('filename'))
#		update_RegistryKey('DirName', dirname)
#		update_RegistryKey('dxfFileName', dxfFileName.val)
		GUI_A['newScene_on'].val = 1
		Draw.Redraw()
	elif (evt==EVENT_CHOOSE_DXF):
		filename = '' # '*.dxf'
		if dxfFileName.val:	filename = dxfFileName.val
		Window.FileSelector(dxf_callback, "DXF/DWG-file Selection", filename)
	elif (evt==EVENT_START):
		dxfFile = dxfFileName.val
		#print 'deb: dxfFile file: ', dxfFile #----------------------
		if E_M: dxfFileName.val, dxfFile = e_mode(dxfFile) #evaluation mode
		update_RegistryKey('dxfFileName', dxfFileName.val)
		if dxfFile.lower().endswith('*.dxf'):
			if Draw.PupMenu('DXF importer will import all DXF-files from:|%s|OK?' % dxfFile) != -1:
				UI_MODE = False
				multi_import(dxfFile)
				UI_MODE = True
				Draw.Redraw()

		elif dxfFile.lower().endswith('*.dwg'):
			if not extCONV_OK: Draw.PupMenu(extCONV_TEXT)
			elif Draw.PupMenu('DWG importer will import all DWG-files from:|%s|OK?' % dxfFile) != -1:
			#elif Draw.PupMenu('DWG importer will import all DWG-files from:|%s|Caution! overwrites existing DXF-files!| OK?' % dxfFile) != -1:
				UI_MODE = False
				multi_import(dxfFile)
				UI_MODE = True
				Draw.Redraw()
				
		elif sys.exists(dxfFile) and dxfFile.lower()[-4:] in ('.dxf','.dwg'):
			if dxfFile.lower().endswith('.dwg') and (not extCONV_OK):
				Draw.PupMenu(extCONV_TEXT)
			else:
				#print '\nStandard Mode: active'
				if GUI_A['newScene_on'].val:
					_dxf_file = dxfFile.split('/')[-1].split('\\')[-1]
					_dxf_file = _dxf_file[:-4]  # cut last char:'.dxf'
					_dxf_file = _dxf_file[:MAX_NAMELENGTH]  #? [-MAX_NAMELENGTH:])
					global SCENE
					SCENE = Blender.Scene.New(_dxf_file)
					SCENE.makeCurrent()
					Blender.Redraw()
					#or so? Blender.Scene.makeCurrent(_dxf_file)
					#sce = bpy.data.scenes.new(_dxf_file)
					#bpy.data.scenes.active = sce
				else:
					SCENE = Blender.Scene.GetCurrent()
					SCENE.objects.selected = [] # deselect all
				main(dxfFile)
				#SCENE.objects.selected = SCENE.objects		 
				#Window.RedrawAll()
				#Blender.Redraw()
				#Draw.Redraw()
		else:
			Draw.PupMenu('DXF importer: nothing imported!%t|no valid DXF-file selected!')
			print "DXF importer: nothing imported, no valid DXF-file selected! try again"
			Draw.Redraw()




def multi_import(DIR):
	"""Imports all DXF-files from directory DIR.
	
	"""
	global SCENE
	batchTIME = sys.time()
	#if #DIR == "": DIR = os.path.curdir
	if DIR == "":
		DIR = sys.dirname(Blender.Get('filename'))
		EXT = '.dxf'
	else:
		EXT = DIR[-4:]  # get last 4 characters '.dxf'
		DIR = DIR[:-5]  # cut last 5 characters '*.dxf'
	print 'importing multiple %s files from %s' %(EXT,DIR)
	files = \
		[sys.join(DIR, f) for f in os.listdir(DIR) if f.lower().endswith(EXT)] 
	if not files:
		print '...None %s-files found. Abort!' %EXT
		return
	
	i = 0
	for dxfFile in files:
		i += 1
		print '\n%s-file' %EXT, i, 'of', len(files) #,'\nImporting', dxfFile
		if GUI_A['newScene_on'].val:
			_dxf_file = dxfFile.split('/')[-1].split('\\')[-1]
			_dxf_file = _dxf_file[:-4]  # cut last char:'.dxf'
			_dxf_file = _dxf_file[:MAX_NAMELENGTH]  #? [-MAX_NAMELENGTH:])
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

	print 'TOTAL TIME: %.6f' % (sys.time() - batchTIME)
	print '\a\r', # beep when done
	Draw.PupMenu('DXF importer:	Done!|finished in %.4f sec.' % (sys.time() - batchTIME))


if __name__ == "__main__":
	#Draw.PupMenu('DXF importer: Abort%t|This script version works for Blender up 2.49 only!')
	UI_MODE = True
	# recall last used DXF-file and INI-file names
	dxffilename = check_RegistryKey('dxfFileName')
	#print 'deb:start dxffilename:', dxffilename #----------------
	if dxffilename: dxfFileName.val = dxffilename
	else:
		dirname = sys.dirname(Blender.Get('filename'))
		#print 'deb:start dirname:', dirname #----------------
		dxfFileName.val = sys.join(dirname, '')
	inifilename = check_RegistryKey('iniFileName')
	if inifilename: iniFileName.val = inifilename

	Draw.Register(draw_UI, event, bevent)


"""
if 1:
	# DEBUG ONLY
	UI_MODE = False
	TIME= sys.time()
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
			if True:
				_dxf_file= _dxf.split('/')[-1].split('\\')[-1]
				_dxf_file = _dxf_file[:-4]  # cut last char:'.dxf'
				_dxf_file = _dxf_file[:MAX_NAMELENGTH]  #? [-MAX_NAMELENGTH:])
				sce = bpy.data.scenes.new(_dxf_file)
				bpy.data.scenes.active = sce
			dxfFileName.val = _dxf
			main(_dxf)

	print 'TOTAL TIME: %.6f' % (sys.time() - TIME)
"""