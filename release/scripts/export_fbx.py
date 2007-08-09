#!BPY
"""
Name: 'Autodesk FBX (.fbx)...'
Blender: 243
Group: 'Export'
Tooltip: 'Selection to an ASCII Autodesk FBX '
"""
__author__ = "Campbell Barton"
__url__ = ['www.blender.org', 'blenderartists.org']
__version__ = "1.1"

__bpydoc__ = """\
This script is an exporter to the FBX file format.

Usage:

Select the objects you wish to export and run this script from "File->Export" menu.
All objects that can be represented as a mesh (mesh, curve, metaball, surface, text3d)
will be exported as mesh data.
"""

# --------------------------------------------------------------------------
# FBX Export v0.1 by Campbell Barton (AKA Ideasman)
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
import BPyObject
import BPyMesh
import BPySys
import BPyMessages
import time
from math import degrees, atan, pi
from Blender.Mathutils import Matrix, Vector, Euler, RotationMatrix



# Change the order rotation is applied.
'''
ROT_ORDER = [\
(0,1,2),\
(1,2,0),\
(2,0,1),\
(2,1,0),\
(1,0,2),\
(0,2,1),\
]
MATRIX_IDENTITY_3x3 = Matrix([1,0,0],[0,1,0],[0,0,1])
MATRIX_IDENTITY_4x4 = Matrix([1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1])

def eulerRotate(x,y,z, rot_order): 
	# Clamp all values between 0 and 360, values outside this raise an error.
	mats=[RotationMatrix(x%360,3,'x'), RotationMatrix(y%360,3,'y'), RotationMatrix(z%360,3,'z')]
	# print rot_order
	# Standard BVH multiplication order, apply the rotation in the order Z,X,Y
	return (mats[rot_order[2]]*(mats[rot_order[1]]* (mats[rot_order[0]]* MATRIX_IDENTITY_3x3))).toEuler()
	
def eulerMat(mat, rot_order): 
	x,y,z = tuple(mat.toEuler())
	# Clamp all values between 0 and 360, values outside this raise an error.
	mats=[RotationMatrix(x%360,3,'x'), RotationMatrix(y%360,3,'y'), RotationMatrix(z%360,3,'z')]
	# print rot_order
	# Standard BVH multiplication order, apply the rotation in the order Z,X,Y
	return mats[rot_order[2]]*(mats[rot_order[1]]* (mats[rot_order[0]]* MATRIX_IDENTITY_3x3))
'''



# Used to add the scene name into the filename without using odd chars

sane_name_mapping_ob = {}
sane_name_mapping_mat = {}
sane_name_mapping_tex = {}

def strip_path(p):
	return p.split('\\')[-1].split('/')[-1]

# todo - Disallow the name 'Scene' and 'blend_root' - it will bugger things up.
def sane_name(data, dct):
	if not data: return None
	name = data.name
	try:		return dct[name]
	except:		pass
	
	orig_name = name
	name = BPySys.cleanName(name)
	dct[orig_name] = name
	return name

def sane_obname(data):		return sane_name(data, sane_name_mapping_ob)
def sane_matname(data):		return sane_name(data, sane_name_mapping_mat)
def sane_texname(data):		return sane_name(data, sane_name_mapping_tex)

def mat4x4str(mat):
	return '%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f' % tuple([ f for v in mat for f in v ])
	
# May use this later
"""
# Auto class, use for datastorage only, a like a dictionary but with limited slots
def auto_class(slots):
	exec('class container_class(object): __slots__=%s' % slots)
	return container_class
"""


header_comment = \
'''; FBX 6.1.0 project file
; Created by Blender FBX Exporter
; for support mail cbarton@metavr.com
; ----------------------------------------------------

'''

def write_header(file):
	file.write(header_comment)
	curtime = time.localtime()[0:6]
	# 
	file.write(\
'''FBXHeaderExtension:  {
	FBXHeaderVersion: 1003
	FBXVersion: 6100
	CreationTimeStamp:  {
		Version: 1000
		Year: %.4i
		Month: %.2i
		Day: %.2i
		Hour: %.2i
		Minute: %.2i
		Second: %.2i
		Millisecond: 0
	}
	Creator: "FBX SDK/FBX Plugins build 20070228"
	OtherFlags:  {
		FlagPLE: 0
	}
}''' % (curtime))
	
	file.write('\nCreationTime: "%.4i-%.2i-%.2i %.2i:%.2i:%.2i:000"' % curtime)
	file.write('\nCreator: "Blender3D version %.2f"' % Blender.Get('version'))



mtx_z90 = RotationMatrix(90, 3, 'z')
mtx_x90 = RotationMatrix(90, 3, 'x')

# testing
mtx_x90		= RotationMatrix( 90, 3, 'x')
mtx_x90n	= RotationMatrix(-90, 3, 'x')
mtx_y90		= RotationMatrix( 90, 3, 'y')
mtx_y90n	= RotationMatrix(-90, 3, 'y')
mtx_z90		= RotationMatrix( 90, 3, 'z')
mtx_z90n	= RotationMatrix(-90, 3, 'z')


mtx4_x90	= RotationMatrix( 90, 4, 'x')
mtx4_x90n	= RotationMatrix(-90, 4, 'x')
mtx4_y90	= RotationMatrix( 90, 4, 'y')
mtx4_y90n	= RotationMatrix(-90, 4, 'y')
mtx4_z90	= RotationMatrix( 90, 4, 'z')
mtx4_z90n	= RotationMatrix(-90, 4, 'z')

XVEC  = Vector(1,  0, 0)
XVECN = Vector(-1, 0, 0)
YVEC  = Vector(0,  1, 0)
YVECN = Vector(0, -1, 0)
ZVEC  = Vector(0, 0,  1)
ZVECN = Vector(0, 0, -1)

def write_scene(file, sce, world):
	
	def object_tx(ob, loc, matrix, matrix_mod = None):
		'''
		Matrix mod is so armature objects can modify their bone matricies
		'''
		if isinstance(ob, Blender.Types.BoneType):
			
			# we know we have a matrix
			matrix = mtx4_z90 * (matrix_mod * ob.matrix['ARMATURESPACE'])
			
			parent = ob.parent
			if parent:
				par_matrix = mtx4_z90 * (matrix_mod * parent.matrix['ARMATURESPACE'].copy())
				matrix = matrix * par_matrix.copy().invert()
				
			matrix_rot =	matrix.rotationPart()
			
			loc =			tuple(matrix.translationPart())
			scale =			tuple(matrix.scalePart())
			rot =			tuple(matrix_rot.toEuler())
			
		else:
			if ob and not matrix:	matrix = ob.matrixWorld
			matrix_rot = matrix
			#if matrix:
			#	matrix = matrix_scale * matrix
			
			if matrix:
				loc = tuple(matrix.translationPart())
				scale = tuple(matrix.scalePart())
				
				matrix_rot = matrix.rotationPart()
				# Lamps need to be rotated
				if ob and ob.type =='Lamp':
					matrix_rot = mtx_x90 * matrix.rotationPart()
					rot = tuple(matrix_rot.toEuler())
				elif ob and ob.type =='Camera':
					y = Vector(0,1,0) * matrix_rot
					matrix_rot = matrix_rot * RotationMatrix(90, 3, 'r', y)
					rot = tuple(matrix_rot.toEuler())
				else:
					rot = tuple(matrix_rot.toEuler())
			else:
				if not loc:
					loc = 0,0,0
				scale = 1,1,1
				rot = 0,0,0
		
		return loc, rot, scale, matrix, matrix_rot
	
	def write_object_tx(ob, loc, matrix, matrix_mod= None):
		'''
		We have loc to set the location if non blender objects that have a location
		
		matrix_mod is only used for bones at the moment
		'''
		loc, rot, scale, matrix, matrix_rot = object_tx(ob, loc, matrix, matrix_mod)
		
		# print rot
		file.write('\n\t\t\tProperty: "Lcl Translation", "Lcl Translation", "A+",%.15f,%.15f,%.15f' % loc)
		file.write('\n\t\t\tProperty: "Lcl Rotation", "Lcl Rotation", "A+",%.15f,%.15f,%.15f' % rot)
		file.write('\n\t\t\tProperty: "Lcl Scaling", "Lcl Scaling", "A+",%.15f,%.15f,%.15f' % scale)
		return loc, rot, scale, matrix, matrix_rot
	
	def write_object_props(ob=None, loc=None, matrix=None, matrix_mod=None):
		# if the type is 0 its an empty otherwise its a mesh
		# only difference at the moment is one has a color
		file.write('''
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",1''')
		
		loc, rot, scale, matrix, matrix_rot = write_object_tx(ob, loc, matrix, matrix_mod)
		
		# Rotation order
		# eEULER_XYZ
		# eEULER_XZY
		# eEULER_YZX
		# eEULER_YXZ
		# eEULER_ZXY
		# eEULER_ZYX 
		
		
		file.write('''
			Property: "RotationOffset", "Vector3D", "",0,0,0
			Property: "RotationPivot", "Vector3D", "",0,0,0
			Property: "ScalingOffset", "Vector3D", "",0,0,0
			Property: "ScalingPivot", "Vector3D", "",0,0,0
			Property: "TranslationActive", "bool", "",0
			Property: "TranslationMin", "Vector3D", "",0,0,0
			Property: "TranslationMax", "Vector3D", "",0,0,0
			Property: "TranslationMinX", "bool", "",0
			Property: "TranslationMinY", "bool", "",0
			Property: "TranslationMinZ", "bool", "",0
			Property: "TranslationMaxX", "bool", "",0
			Property: "TranslationMaxY", "bool", "",0
			Property: "TranslationMaxZ", "bool", "",0
			Property: "RotationOrder", "enum", "",1
			Property: "RotationSpaceForLimitOnly", "bool", "",0
			Property: "AxisLen", "double", "",10
			Property: "PreRotation", "Vector3D", "",0,0,0
			Property: "PostRotation", "Vector3D", "",0,0,0
			Property: "RotationActive", "bool", "",0
			Property: "RotationMin", "Vector3D", "",0,0,0
			Property: "RotationMax", "Vector3D", "",0,0,0
			Property: "RotationMinX", "bool", "",0
			Property: "RotationMinY", "bool", "",0
			Property: "RotationMinZ", "bool", "",0
			Property: "RotationMaxX", "bool", "",0
			Property: "RotationMaxY", "bool", "",0
			Property: "RotationMaxZ", "bool", "",0
			Property: "RotationStiffnessX", "double", "",0
			Property: "RotationStiffnessY", "double", "",0
			Property: "RotationStiffnessZ", "double", "",0
			Property: "MinDampRangeX", "double", "",0
			Property: "MinDampRangeY", "double", "",0
			Property: "MinDampRangeZ", "double", "",0
			Property: "MaxDampRangeX", "double", "",0
			Property: "MaxDampRangeY", "double", "",0
			Property: "MaxDampRangeZ", "double", "",0
			Property: "MinDampStrengthX", "double", "",0
			Property: "MinDampStrengthY", "double", "",0
			Property: "MinDampStrengthZ", "double", "",0
			Property: "MaxDampStrengthX", "double", "",0
			Property: "MaxDampStrengthY", "double", "",0
			Property: "MaxDampStrengthZ", "double", "",0
			Property: "PreferedAngleX", "double", "",0
			Property: "PreferedAngleY", "double", "",0
			Property: "PreferedAngleZ", "double", "",0
			Property: "InheritType", "enum", "",0
			Property: "ScalingActive", "bool", "",0
			Property: "ScalingMin", "Vector3D", "",1,1,1
			Property: "ScalingMax", "Vector3D", "",1,1,1
			Property: "ScalingMinX", "bool", "",0
			Property: "ScalingMinY", "bool", "",0
			Property: "ScalingMinZ", "bool", "",0
			Property: "ScalingMaxX", "bool", "",0
			Property: "ScalingMaxY", "bool", "",0
			Property: "ScalingMaxZ", "bool", "",0
			Property: "GeometricTranslation", "Vector3D", "",0,0,0
			Property: "GeometricRotation", "Vector3D", "",0,0,0
			Property: "GeometricScaling", "Vector3D", "",1,1,1
			Property: "LookAtProperty", "object", ""
			Property: "UpVectorProperty", "object", ""
			Property: "Show", "bool", "",1
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0''')
		if ob and type(ob) != Blender.Types.BoneType:
			# Only mesh objects have color 
			file.write('\n\t\t\tProperty: "Color", "Color", "A",0.8,0.8,0.8')
			file.write('\n\t\t\tProperty: "Size", "double", "",100')
			file.write('\n\t\t\tProperty: "Look", "enum", "",1')
		
		return loc, rot, scale, matrix, matrix_rot
	
	
	# -------------------------------------------- Armatures
	def write_bone(bone, name, matrix_mod):
		file.write('\n\tModel: "Model::%s", "LimbNode" {' % name)
		file.write('\n\t\tVersion: 232')
		
		write_object_props(bone, None, None, matrix_mod)
		
		file.write('\n\t\t\tProperty: "Size", "double", "",%.6f' % ((bone.head['ARMATURESPACE']-bone.tail['ARMATURESPACE']) * matrix_mod).length)
		#file.write('\n\t\t\tProperty: "Size", "double", "",1')
		#file.write('\n\t\t\tProperty: "LimbLength", "double", "",%.6f' % (bone.head['ARMATURESPACE']-bone.tail['ARMATURESPACE']).length)
		file.write('\n\t\t\tProperty: "LimbLength", "double", "",1')
		file.write('\n\t\t\tProperty: "Color", "ColorRGB", "",0.8,0.8,0.8')
		file.write('\n\t\t\tProperty: "Color", "Color", "A",0.8,0.8,0.8')
		file.write('\n\t\t}')
		file.write('\n\t\tMultiLayer: 0')
		file.write('\n\t\tMultiTake: 1')
		file.write('\n\t\tShading: Y')
		file.write('\n\t\tCulling: "CullingOff"')
		file.write('\n\t\tTypeFlags: "Skeleton"')
		file.write('\n\t}')
	
	def write_camera_switch():
		file.write('''
	Model: "Model::Camera Switcher", "CameraSwitcher" {
		Version: 232''')
		
		write_object_props()
		file.write('''
			Property: "Color", "Color", "A",0.8,0.8,0.8
			Property: "Camera Index", "Integer", "A+",100
		}
		MultiLayer: 0
		MultiTake: 1
		Hidden: "True"
		Shading: W
		Culling: "CullingOff"
		Version: 101
		Name: "Model::Camera Switcher"
		CameraId: 0
		CameraName: 100
		CameraIndexName: 
	}''')
	
	def write_camera_dummy(name, loc, near, far, proj_type, up):
		file.write('\n\tModel: "Model::%s", "Camera" {' % name )
		file.write('\n\t\tVersion: 232')
		write_object_props(None, loc)
		
		file.write('\n\t\t\tProperty: "Color", "Color", "A",0.8,0.8,0.8')
		file.write('\n\t\t\tProperty: "Roll", "Roll", "A+",0')
		file.write('\n\t\t\tProperty: "FieldOfView", "FieldOfView", "A+",40')
		file.write('\n\t\t\tProperty: "FieldOfViewX", "FieldOfView", "A+",1')
		file.write('\n\t\t\tProperty: "FieldOfViewY", "FieldOfView", "A+",1')
		file.write('\n\t\t\tProperty: "OpticalCenterX", "Real", "A+",0')
		file.write('\n\t\t\tProperty: "OpticalCenterY", "Real", "A+",0')
		file.write('\n\t\t\tProperty: "BackgroundColor", "Color", "A+",0.63,0.63,0.63')
		file.write('\n\t\t\tProperty: "TurnTable", "Real", "A+",0')
		file.write('\n\t\t\tProperty: "DisplayTurnTableIcon", "bool", "",1')
		file.write('\n\t\t\tProperty: "Motion Blur Intensity", "Real", "A+",1')
		file.write('\n\t\t\tProperty: "UseMotionBlur", "bool", "",0')
		file.write('\n\t\t\tProperty: "UseRealTimeMotionBlur", "bool", "",1')
		file.write('\n\t\t\tProperty: "ResolutionMode", "enum", "",0')
		file.write('\n\t\t\tProperty: "ApertureMode", "enum", "",2')
		file.write('\n\t\t\tProperty: "GateFit", "enum", "",0')
		file.write('\n\t\t\tProperty: "FocalLength", "Real", "A+",21.3544940948486')
		file.write('\n\t\t\tProperty: "CameraFormat", "enum", "",0')
		file.write('\n\t\t\tProperty: "AspectW", "double", "",320')
		file.write('\n\t\t\tProperty: "AspectH", "double", "",200')
		file.write('\n\t\t\tProperty: "PixelAspectRatio", "double", "",1')
		file.write('\n\t\t\tProperty: "UseFrameColor", "bool", "",0')
		file.write('\n\t\t\tProperty: "FrameColor", "ColorRGB", "",0.3,0.3,0.3')
		file.write('\n\t\t\tProperty: "ShowName", "bool", "",1')
		file.write('\n\t\t\tProperty: "ShowGrid", "bool", "",1')
		file.write('\n\t\t\tProperty: "ShowOpticalCenter", "bool", "",0')
		file.write('\n\t\t\tProperty: "ShowAzimut", "bool", "",1')
		file.write('\n\t\t\tProperty: "ShowTimeCode", "bool", "",0')
		file.write('\n\t\t\tProperty: "NearPlane", "double", "",%.6f' % near)
		file.write('\n\t\t\tProperty: "FarPlane", "double", "",%.6f' % far)
		file.write('\n\t\t\tProperty: "FilmWidth", "double", "",0.816')
		file.write('\n\t\t\tProperty: "FilmHeight", "double", "",0.612')
		file.write('\n\t\t\tProperty: "FilmAspectRatio", "double", "",1.33333333333333')
		file.write('\n\t\t\tProperty: "FilmSqueezeRatio", "double", "",1')
		file.write('\n\t\t\tProperty: "FilmFormatIndex", "enum", "",4')
		file.write('\n\t\t\tProperty: "ViewFrustum", "bool", "",1')
		file.write('\n\t\t\tProperty: "ViewFrustumNearFarPlane", "bool", "",0')
		file.write('\n\t\t\tProperty: "ViewFrustumBackPlaneMode", "enum", "",2')
		file.write('\n\t\t\tProperty: "BackPlaneDistance", "double", "",100')
		file.write('\n\t\t\tProperty: "BackPlaneDistanceMode", "enum", "",0')
		file.write('\n\t\t\tProperty: "ViewCameraToLookAt", "bool", "",1')
		file.write('\n\t\t\tProperty: "LockMode", "bool", "",0')
		file.write('\n\t\t\tProperty: "LockInterestNavigation", "bool", "",0')
		file.write('\n\t\t\tProperty: "FitImage", "bool", "",0')
		file.write('\n\t\t\tProperty: "Crop", "bool", "",0')
		file.write('\n\t\t\tProperty: "Center", "bool", "",1')
		file.write('\n\t\t\tProperty: "KeepRatio", "bool", "",1')
		file.write('\n\t\t\tProperty: "BackgroundMode", "enum", "",0')
		file.write('\n\t\t\tProperty: "BackgroundAlphaTreshold", "double", "",0.5')
		file.write('\n\t\t\tProperty: "ForegroundTransparent", "bool", "",1')
		file.write('\n\t\t\tProperty: "DisplaySafeArea", "bool", "",0')
		file.write('\n\t\t\tProperty: "SafeAreaDisplayStyle", "enum", "",1')
		file.write('\n\t\t\tProperty: "SafeAreaAspectRatio", "double", "",1.33333333333333')
		file.write('\n\t\t\tProperty: "Use2DMagnifierZoom", "bool", "",0')
		file.write('\n\t\t\tProperty: "2D Magnifier Zoom", "Real", "A+",100')
		file.write('\n\t\t\tProperty: "2D Magnifier X", "Real", "A+",50')
		file.write('\n\t\t\tProperty: "2D Magnifier Y", "Real", "A+",50')
		file.write('\n\t\t\tProperty: "CameraProjectionType", "enum", "",%i' % proj_type)
		file.write('\n\t\t\tProperty: "UseRealTimeDOFAndAA", "bool", "",0')
		file.write('\n\t\t\tProperty: "UseDepthOfField", "bool", "",0')
		file.write('\n\t\t\tProperty: "FocusSource", "enum", "",0')
		file.write('\n\t\t\tProperty: "FocusAngle", "double", "",3.5')
		file.write('\n\t\t\tProperty: "FocusDistance", "double", "",200')
		file.write('\n\t\t\tProperty: "UseAntialiasing", "bool", "",0')
		file.write('\n\t\t\tProperty: "AntialiasingIntensity", "double", "",0.77777')
		file.write('\n\t\t\tProperty: "UseAccumulationBuffer", "bool", "",0')
		file.write('\n\t\t\tProperty: "FrameSamplingCount", "int", "",7')
		file.write('\n\t\t}')
		file.write('\n\t\tMultiLayer: 0')
		file.write('\n\t\tMultiTake: 0')
		file.write('\n\t\tHidden: "True"')
		file.write('\n\t\tShading: Y')
		file.write('\n\t\tCulling: "CullingOff"')
		file.write('\n\t\tTypeFlags: "Camera"')
		file.write('\n\t\tGeometryVersion: 124')
		file.write('\n\t\tPosition: %.6f,%.6f,%.6f' % loc)
		file.write('\n\t\tUp: %i,%i,%i' % up)
		file.write('\n\t\tLookAt: 0,0,0')
		file.write('\n\t\tShowInfoOnMoving: 1')
		file.write('\n\t\tShowAudio: 0')
		file.write('\n\t\tAudioColor: 0,1,0')
		file.write('\n\t\tCameraOrthoZoom: 1')
		file.write('\n\t}')
	
	def write_camera_default():
		# This sucks but to match FBX converter its easier to
		# write the cameras though they are not needed.
		write_camera_dummy('Producer Perspective',	(0,71.3,287.5), 10, 4000, 0, (0,1,0))
		write_camera_dummy('Producer Top',			(0,4000,0), 1, 30000, 1, (0,0,-1))
		write_camera_dummy('Producer Bottom',			(0,-4000,0), 1, 30000, 1, (0,0,-1))
		write_camera_dummy('Producer Front',			(0,0,4000), 1, 30000, 1, (0,1,0))
		write_camera_dummy('Producer Back',			(0,0,-4000), 1, 30000, 1, (0,1,0))
		write_camera_dummy('Producer Right',			(4000,0,0), 1, 30000, 1, (0,1,0))
		write_camera_dummy('Producer Left',			(-4000,0,0), 1, 30000, 1, (0,1,0))
	
	def write_camera(ob, name):
		'''
		Write a blender camera
		'''
		render = sce.render
		width	= render.sizeX
		height	= render.sizeY
		aspect	= float(width)/height
		
		data = ob.data
		
		file.write('\n\tModel: "Model::%s", "Camera" {' % name )
		file.write('\n\t\tVersion: 232')
		loc, rot, scale, matrix, matrix_rot = write_object_props(ob)
		
		file.write('\n\t\t\tProperty: "Roll", "Roll", "A+",0')
		file.write('\n\t\t\tProperty: "FieldOfView", "FieldOfView", "A+",%.6f' % data.angle)
		file.write('\n\t\t\tProperty: "FieldOfViewX", "FieldOfView", "A+",1')
		file.write('\n\t\t\tProperty: "FieldOfViewY", "FieldOfView", "A+",1')
		file.write('\n\t\t\tProperty: "FocalLength", "Real", "A+",14.0323972702026')
		file.write('\n\t\t\tProperty: "OpticalCenterX", "Real", "A+",%.6f' % data.shiftX) # not sure if this is in the correct units?
		file.write('\n\t\t\tProperty: "OpticalCenterY", "Real", "A+",%.6f' % data.shiftY) # ditto 
		file.write('\n\t\t\tProperty: "BackgroundColor", "Color", "A+",0,0,0')
		file.write('\n\t\t\tProperty: "TurnTable", "Real", "A+",0')
		file.write('\n\t\t\tProperty: "DisplayTurnTableIcon", "bool", "",1')
		file.write('\n\t\t\tProperty: "Motion Blur Intensity", "Real", "A+",1')
		file.write('\n\t\t\tProperty: "UseMotionBlur", "bool", "",0')
		file.write('\n\t\t\tProperty: "UseRealTimeMotionBlur", "bool", "",1')
		file.write('\n\t\t\tProperty: "ResolutionMode", "enum", "",0')
		file.write('\n\t\t\tProperty: "ApertureMode", "enum", "",2')
		file.write('\n\t\t\tProperty: "GateFit", "enum", "",0')
		file.write('\n\t\t\tProperty: "CameraFormat", "enum", "",0')
		file.write('\n\t\t\tProperty: "AspectW", "double", "",%i' % width)
		file.write('\n\t\t\tProperty: "AspectH", "double", "",%i' % height)
		
		'''Camera aspect ratio modes.
			0 If the ratio mode is eWINDOW_SIZE, both width and height values aren't relevant.
			1 If the ratio mode is eFIXED_RATIO, the height value is set to 1.0 and the width value is relative to the height value.
			2 If the ratio mode is eFIXED_RESOLUTION, both width and height values are in pixels.
			3 If the ratio mode is eFIXED_WIDTH, the width value is in pixels and the height value is relative to the width value.
			4 If the ratio mode is eFIXED_HEIGHT, the height value is in pixels and the width value is relative to the height value. 
		
		Definition at line 234 of file kfbxcamera.h. '''
		
		file.write('\n\t\t\tProperty: "PixelAspectRatio", "double", "",2')
		
		file.write('\n\t\t\tProperty: "UseFrameColor", "bool", "",0')
		file.write('\n\t\t\tProperty: "FrameColor", "ColorRGB", "",0.3,0.3,0.3')
		file.write('\n\t\t\tProperty: "ShowName", "bool", "",1')
		file.write('\n\t\t\tProperty: "ShowGrid", "bool", "",1')
		file.write('\n\t\t\tProperty: "ShowOpticalCenter", "bool", "",0')
		file.write('\n\t\t\tProperty: "ShowAzimut", "bool", "",1')
		file.write('\n\t\t\tProperty: "ShowTimeCode", "bool", "",0')
		file.write('\n\t\t\tProperty: "NearPlane", "double", "",%.6f' % data.clipStart)
		file.write('\n\t\t\tProperty: "FarPlane", "double", "",%.6f' % data.clipStart)
		file.write('\n\t\t\tProperty: "FilmWidth", "double", "",1.0')
		file.write('\n\t\t\tProperty: "FilmHeight", "double", "",1.0')
		file.write('\n\t\t\tProperty: "FilmAspectRatio", "double", "",%.6f' % aspect)
		file.write('\n\t\t\tProperty: "FilmSqueezeRatio", "double", "",1')
		file.write('\n\t\t\tProperty: "FilmFormatIndex", "enum", "",0')
		file.write('\n\t\t\tProperty: "ViewFrustum", "bool", "",1')
		file.write('\n\t\t\tProperty: "ViewFrustumNearFarPlane", "bool", "",0')
		file.write('\n\t\t\tProperty: "ViewFrustumBackPlaneMode", "enum", "",2')
		file.write('\n\t\t\tProperty: "BackPlaneDistance", "double", "",100')
		file.write('\n\t\t\tProperty: "BackPlaneDistanceMode", "enum", "",0')
		file.write('\n\t\t\tProperty: "ViewCameraToLookAt", "bool", "",1')
		file.write('\n\t\t\tProperty: "LockMode", "bool", "",0')
		file.write('\n\t\t\tProperty: "LockInterestNavigation", "bool", "",0')
		file.write('\n\t\t\tProperty: "FitImage", "bool", "",0')
		file.write('\n\t\t\tProperty: "Crop", "bool", "",0')
		file.write('\n\t\t\tProperty: "Center", "bool", "",1')
		file.write('\n\t\t\tProperty: "KeepRatio", "bool", "",1')
		file.write('\n\t\t\tProperty: "BackgroundMode", "enum", "",0')
		file.write('\n\t\t\tProperty: "BackgroundAlphaTreshold", "double", "",0.5')
		file.write('\n\t\t\tProperty: "ForegroundTransparent", "bool", "",1')
		file.write('\n\t\t\tProperty: "DisplaySafeArea", "bool", "",0')
		file.write('\n\t\t\tProperty: "SafeAreaDisplayStyle", "enum", "",1')
		file.write('\n\t\t\tProperty: "SafeAreaAspectRatio", "double", "",%.6f' % aspect)
		file.write('\n\t\t\tProperty: "Use2DMagnifierZoom", "bool", "",0')
		file.write('\n\t\t\tProperty: "2D Magnifier Zoom", "Real", "A+",100')
		file.write('\n\t\t\tProperty: "2D Magnifier X", "Real", "A+",50')
		file.write('\n\t\t\tProperty: "2D Magnifier Y", "Real", "A+",50')
		file.write('\n\t\t\tProperty: "CameraProjectionType", "enum", "",0')
		file.write('\n\t\t\tProperty: "UseRealTimeDOFAndAA", "bool", "",0')
		file.write('\n\t\t\tProperty: "UseDepthOfField", "bool", "",0')
		file.write('\n\t\t\tProperty: "FocusSource", "enum", "",0')
		file.write('\n\t\t\tProperty: "FocusAngle", "double", "",3.5')
		file.write('\n\t\t\tProperty: "FocusDistance", "double", "",200')
		file.write('\n\t\t\tProperty: "UseAntialiasing", "bool", "",0')
		file.write('\n\t\t\tProperty: "AntialiasingIntensity", "double", "",0.77777')
		file.write('\n\t\t\tProperty: "UseAccumulationBuffer", "bool", "",0')
		file.write('\n\t\t\tProperty: "FrameSamplingCount", "int", "",7')
		
		file.write('\n\t\t}')
		file.write('\n\t\tMultiLayer: 0')
		file.write('\n\t\tMultiTake: 0')
		file.write('\n\t\tShading: Y')
		file.write('\n\t\tCulling: "CullingOff"')
		file.write('\n\t\tTypeFlags: "Camera"')
		file.write('\n\t\tGeometryVersion: 124')
		file.write('\n\t\tPosition: %.6f,%.6f,%.6f' % loc)
		file.write('\n\t\tUp: %.6f,%.6f,%.6f' % tuple(Vector(0,1,0) * matrix_rot) )
		file.write('\n\t\tLookAt: %.6f,%.6f,%.6f' % tuple(Vector(0,0,-1)*matrix_rot) )
		
		#file.write('\n\t\tUp: 0,0,0' )
		#file.write('\n\t\tLookAt: 0,0,0' )
		
		file.write('\n\t\tShowInfoOnMoving: 1')
		file.write('\n\t\tShowAudio: 0')
		file.write('\n\t\tAudioColor: 0,1,0')
		file.write('\n\t\tCameraOrthoZoom: 1')
		file.write('\n\t}')
	
	def write_light(ob, name):
		light = ob.data
		file.write('\n\tModel: "Model::%s", "Light" {' % name)
		file.write('\n\t\tVersion: 232')
		
		write_object_props(ob)
		
		# Why are these values here twice?????? - oh well, follow the holy sdk's output
		
		# Blender light types match FBX's, funny coincidence, we just need to
		# be sure that all unsupported types are made into a point light
		#ePOINT, 
		#eDIRECTIONAL
		#eSPOT
		light_type = light.type
		if light_type > 3: light_type = 0
			
		file.write('\n\t\t\tProperty: "LightType", "enum", "",%i' % light_type)
		file.write('\n\t\t\tProperty: "CastLightOnObject", "bool", "",1')
		file.write('\n\t\t\tProperty: "DrawVolumetricLight", "bool", "",1')
		file.write('\n\t\t\tProperty: "DrawGroundProjection", "bool", "",1')
		file.write('\n\t\t\tProperty: "DrawFrontFacingVolumetricLight", "bool", "",0')
		file.write('\n\t\t\tProperty: "GoboProperty", "object", ""')
		file.write('\n\t\t\tProperty: "Color", "Color", "A+",1,1,1')
		file.write('\n\t\t\tProperty: "Intensity", "Intensity", "A+",%.2f' % (light.energy*100))
		file.write('\n\t\t\tProperty: "Cone angle", "Cone angle", "A+",%.2f' % light.spotSize)
		file.write('\n\t\t\tProperty: "Fog", "Fog", "A+",50')
		file.write('\n\t\t\tProperty: "Color", "Color", "A",%.2f,%.2f,%.2f' % tuple(light.col))
		file.write('\n\t\t\tProperty: "Intensity", "Intensity", "A+",%.2f' % (light.energy*100))
		file.write('\n\t\t\tProperty: "Cone angle", "Cone angle", "A+",%.2f' % light.spotSize)
		file.write('\n\t\t\tProperty: "Fog", "Fog", "A+",50')
		file.write('\n\t\t\tProperty: "LightType", "enum", "",%i' % light_type)
		file.write('\n\t\t\tProperty: "CastLightOnObject", "bool", "",1')
		file.write('\n\t\t\tProperty: "DrawGroundProjection", "bool", "",1')
		file.write('\n\t\t\tProperty: "DrawFrontFacingVolumetricLight", "bool", "",0')
		file.write('\n\t\t\tProperty: "DrawVolumetricLight", "bool", "",1')
		file.write('\n\t\t\tProperty: "GoboProperty", "object", ""')
		file.write('\n\t\t\tProperty: "DecayType", "enum", "",0')
		file.write('\n\t\t\tProperty: "DecayStart", "double", "",%.2f' % light.dist)
		file.write('\n\t\t\tProperty: "EnableNearAttenuation", "bool", "",0')
		file.write('\n\t\t\tProperty: "NearAttenuationStart", "double", "",0')
		file.write('\n\t\t\tProperty: "NearAttenuationEnd", "double", "",0')
		file.write('\n\t\t\tProperty: "EnableFarAttenuation", "bool", "",0')
		file.write('\n\t\t\tProperty: "FarAttenuationStart", "double", "",0')
		file.write('\n\t\t\tProperty: "FarAttenuationEnd", "double", "",0')
		file.write('\n\t\t\tProperty: "CastShadows", "bool", "",0')
		file.write('\n\t\t\tProperty: "ShadowColor", "ColorRGBA", "",0,0,0,1')
		file.write('\n\t\t}')
		file.write('\n\t\tMultiLayer: 0')
		file.write('\n\t\tMultiTake: 0')
		file.write('\n\t\tShading: Y')
		file.write('\n\t\tCulling: "CullingOff"')
		file.write('\n\t\tTypeFlags: "Light"')
		file.write('\n\t\tGeometryVersion: 124')
		file.write('\n\t}')
	
	def write_null(ob, name):
		# ob can be null
		file.write('\n\tModel: "Model::%s", "Null" {' % name)
		file.write('\n\t\tVersion: 232')
		write_object_props(ob)
		file.write('''
		}
		MultiLayer: 0
		MultiTake: 1
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Null"
	}''')
		
		
	
	
	
	# Material Settings
	if world:
		world_amb = world.getAmb()
	else:
		world_amb = (0,0,0) # Default value
	
	def write_material(matname, mat):
		file.write('\n\tMaterial: "Material::%s", "" {' % matname)
		
		# Todo, add more material Properties.
		if mat:
			mat_cold = tuple(mat.rgbCol)
			mat_cols = tuple(mat.specCol)
			#mat_colm = tuple(mat.mirCol) # we wont use the mirror color
			mat_colamb = tuple([c for c in world_amb])
			
			mat_dif = mat.ref
			mat_amb = mat.amb
			mat_hard = (float(mat.hard)-1)/5.10
			mat_spec = mat.spec/2.0
			mat_alpha = mat.alpha
			mat_shadeless = mat.mode & Blender.Material.Modes.SHADELESS
			if mat_shadeless:
				mat_shader = 'Lambert'
			else:
				if mat.diffuseShader == Blender.Material.Shaders.DIFFUSE_LAMBERT:
					mat_shader = 'Lambert'
				else:
					mat_shader = 'Phong'
		else:
			mat_cols = mat_cold = 0.8, 0.8, 0.8
			mat_colamb = 0.0,0.0,0.0
			# mat_colm 
			mat_dif = 1.0
			mat_amb = 0.5
			mat_hard = 20.0
			mat_spec = 0.2
			mat_alpha = 1.0
			mat_shadeless = False
			mat_shader = 'Phong'
		
		file.write('\n\t\tVersion: 102')
		file.write('\n\t\tShadingModel: "%s"' % mat_shader.lower())
		file.write('\n\t\tMultiLayer: 0')
		
		file.write('\n\t\tProperties60:  {')
		file.write('\n\t\t\tProperty: "ShadingModel", "KString", "", "%s"' % mat_shader)
		file.write('\n\t\t\tProperty: "MultiLayer", "bool", "",0')
		file.write('\n\t\t\tProperty: "EmissiveColor", "ColorRGB", "",0,0,0')
		file.write('\n\t\t\tProperty: "EmissiveFactor", "double", "",1')
		
		file.write('\n\t\t\tProperty: "AmbientColor", "ColorRGB", "",%.1f,%.1f,%.1f' % mat_colamb)
		file.write('\n\t\t\tProperty: "AmbientFactor", "double", "",%.1f' % mat_amb)
		file.write('\n\t\t\tProperty: "DiffuseColor", "ColorRGB", "",%.1f,%.1f,%.1f' % mat_cold)
		file.write('\n\t\t\tProperty: "DiffuseFactor", "double", "",%.1f' % mat_dif)
		file.write('\n\t\t\tProperty: "Bump", "Vector3D", "",0,0,0')
		file.write('\n\t\t\tProperty: "TransparentColor", "ColorRGB", "",1,1,1')
		file.write('\n\t\t\tProperty: "TransparencyFactor", "double", "",0')
		if not mat_shadeless:
			file.write('\n\t\t\tProperty: "SpecularColor", "ColorRGB", "",%.1f,%.1f,%.1f' % mat_cols)
			file.write('\n\t\t\tProperty: "SpecularFactor", "double", "",%.1f' % mat_spec)
			file.write('\n\t\t\tProperty: "ShininessExponent", "double", "",80.0')
			file.write('\n\t\t\tProperty: "ReflectionColor", "ColorRGB", "",0,0,0')
			file.write('\n\t\t\tProperty: "ReflectionFactor", "double", "",1')
		file.write('\n\t\t\tProperty: "Emissive", "Vector3D", "",0,0,0')
		file.write('\n\t\t\tProperty: "Ambient", "Vector3D", "",%.1f,%.1f,%.1f' % mat_colamb)
		file.write('\n\t\t\tProperty: "Diffuse", "Vector3D", "",%.1f,%.1f,%.1f' % mat_cold)
		if not mat_shadeless:
			file.write('\n\t\t\tProperty: "Specular", "Vector3D", "",%.1f,%.1f,%.1f' % mat_cols)
			file.write('\n\t\t\tProperty: "Shininess", "double", "",%.1f' % mat_hard)
		file.write('\n\t\t\tProperty: "Opacity", "double", "",%.1f' % mat_alpha)
		if not mat_shadeless:
			file.write('\n\t\t\tProperty: "Reflectivity", "double", "",0')

		file.write('\n\t\t}')
		file.write('\n\t}')
	
	def write_video(texname, tex):
		# Same as texture really!
		file.write('\n\tVideo: "Video::%s", "Clip" {' % texname)
		
		file.write('''
		Type: "Clip"
		Properties60:  {
			Property: "FrameRate", "double", "",0
			Property: "LastFrame", "int", "",0
			Property: "Width", "int", "",0
			Property: "Height", "int", "",0''')
		if tex:
			fname = tex.filename
			fname_strip = strip_path(fname)
		else:
			fname = fname_strip = ''
		
		file.write('\n\t\t\tProperty: "Path", "charptr", "", "%s"' % fname_strip)
		
		
		file.write('''
			Property: "StartFrame", "int", "",0
			Property: "StopFrame", "int", "",0
			Property: "PlaySpeed", "double", "",1
			Property: "Offset", "KTime", "",0
			Property: "InterlaceMode", "enum", "",0
			Property: "FreeRunning", "bool", "",0
			Property: "Loop", "bool", "",0
			Property: "AccessMode", "enum", "",0
		}
		UseMipMap: 0''')
		
		file.write('\n\t\tFilename: "%s"' % fname_strip)
		if fname_strip: fname_strip = '/' + fname_strip
		file.write('\n\t\tRelativeFilename: "fbx%s"' % fname_strip) # make relative
		file.write('\n\t}')

	
	def write_texture(texname, tex, num):
		# if tex == None then this is a dummy tex
		file.write('\n\tTexture: "Texture::%s", "TextureVideoClip" {' % texname)
		file.write('\n\t\tType: "TextureVideoClip"')
		file.write('\n\t\tVersion: 202')
		# TODO, rare case _empty_ exists as a name.
		file.write('\n\t\tTextureName: "Texture::%s"' % texname)
		
		file.write('''
		Properties60:  {
			Property: "Translation", "Vector", "A+",0,0,0
			Property: "Rotation", "Vector", "A+",0,0,0
			Property: "Scaling", "Vector", "A+",1,1,1''')
		file.write('\n\t\t\tProperty: "Texture alpha", "Number", "A+",%i' % num)
		file.write('''
			Property: "TextureTypeUse", "enum", "",0
			Property: "CurrentTextureBlendMode", "enum", "",1
			Property: "UseMaterial", "bool", "",0
			Property: "UseMipMap", "bool", "",0
			Property: "CurrentMappingType", "enum", "",0
			Property: "UVSwap", "bool", "",0
			Property: "WrapModeU", "enum", "",0
			Property: "WrapModeV", "enum", "",0
			Property: "TextureRotationPivot", "Vector3D", "",0,0,0
			Property: "TextureScalingPivot", "Vector3D", "",0,0,0
			Property: "VideoProperty", "object", ""
		}''')
		
		file.write('\n\t\tMedia: "Video::%s"' % texname)
		if tex:
			fname = tex.filename
			file.write('\n\t\tFileName: "%s"' % strip_path(fname))
			file.write('\n\t\tRelativeFilename: "fbx/%s"' % strip_path(fname)) # need some make relative command
		else:
			file.write('\n\t\tFileName: ""')
			file.write('\n\t\tRelativeFilename: "fbx"')
		
		file.write('''
		ModelUVTranslation: 0,0
		ModelUVScaling: 1,1
		Texture_Alpha_Source: "None"
		Cropping: 0,0,0,0
	}''')
	
	
	def write_deformer_skin(obname):
		file.write('\n\tDeformer: "Deformer::Skin %s", "Skin" {' % obname)
		file.write('''
	Version: 100
		MultiLayer: 0
		Type: "Skin"
		Properties60:  {
		}
		Link_DeformAcuracy: 50
	}''')
	
	# in the example was 'Bip01 L Thigh_2'
	def write_sub_deformer_skin(obname, group_name, bone, me, matrix_mod):
		file.write('\n\tDeformer: "SubDeformer::Cluster %s", "Cluster" {' % group_name)
		file.write('''
		Version: 100
		MultiLayer: 0
		Type: "Cluster"
		Properties60:  {
			Property: "SrcModel", "object", ""
			Property: "SrcModelReference", "object", ""
		}
		UserData: "", ""''')
		
		try:
			vgroup_data = me.getVertsFromGroup(bone.name, 1)
		except:
			vgroup_data = []
		
		file.write('\n\t\tIndexes: ')
		
		i = -1
		for vg in vgroup_data:
			if i == -1:
				file.write('%i'  % vg[0])
				i=0
			else:
				if i==38:
					file.write('\n\t\t')
					i=0
				file.write(',%i' % vg[0])
			i+=1
		
		file.write('\n\t\tWeights: ')
		i = -1
		for vg in vgroup_data:
			if i == -1:
				file.write('%.8f'  % vg[1])
				i=0
			else:
				if i==38:
					file.write('\n\t\t')
					i=0
				file.write(',%.8f' % vg[1])
			i+=1
		
		m = mtx4_z90 * (matrix_mod * bone.matrix['ARMATURESPACE'])
		matstr = mat4x4str(m)
		matstr_i = mat4x4str(m.invert())
		#matstr = mat4x4str(Matrix())
		
		# It seems fine to have these matricies the same! - worldspace bone or pose locations?
		file.write('\n\t\tTransform: %s' % matstr_i) # THIS IS __NOT__ THE GLOBAL MATRIX AS DOCUMENTED :/
		file.write('\n\t\tTransformLink: %s' % matstr)
		file.write('\n\t}')
	
	ob_meshes = []
	ob_lights = []
	ob_cameras = []
	# in fbx we export bones as children of the mesh
	# armatures not a part of a mesh, will be added to ob_arms
	ob_bones = [] 
	ob_arms = []
	ob_null = [] # emptys
	materials = {}
	textures = {}
	
	ob_type = None # incase no objects are exported, so as not to raise an error
	
	for ob_base in sce.objects.context:
		for ob, mtx in BPyObject.getDerivedObjects(ob_base):
			#for ob in [ob_base,]:
			ob_type = ob.type
			if ob_type == 'Camera':
				ob_cameras.append((sane_obname(ob), ob))
			elif ob_type == 'Lamp':
				ob_lights.append((sane_obname(ob), ob))
			elif ob_type == 'Armature':
				#ob_arms.append(sane_obname(ob), ob)
				ob_arms.append(ob) # replace later.
			elif ob_type == 'Empty':
				ob_null.append((sane_obname(ob), ob))
			else:
				if ob_type == 'Mesh':	me = ob.getData(mesh=1)
				else:					me = BPyMesh.getMeshFromObject(ob)
				
				if me:
					mats = me.materials
					for mat in mats:
						# 2.44 use mat.lib too for uniqueness
						if mat: materials[mat.name] = mat
					
					if me.faceUV:
						uvlayer_orig = me.activeUVLayer
						for uvlayer in me.getUVLayerNames():
							me.activeUVLayer = uvlayer
							for f in me.faces:
								img = f.image
								if img: textures[img.name] = img
							
							me.activeUVLayer = uvlayer_orig
					
					obname = sane_obname(ob)
					
					armob = BPyObject.getObjectArmature(ob)
					
					if armob:
						armname = sane_obname(armob)
						bones = armob.data.bones.values()
						# armatures.append((arm, armname, bones))
						# arm_name = BPySys.cleanName(arm.name)
						
						for bone in bones:
							#name = sane_obname(arm_name + ' ' + b.name)
							ob_bones.append( (sane_obname(bone), bone, obname, me, armob) )
					else:
						armname = None
					
					#### me.transform(ob.matrixWorld) # Export real ob coords.
					#### High Quality, not realy needed for now.
					#BPyMesh.meshCalcNormals(me) # high quality normals nice for realtime engines.
					ob_meshes.append( (obname, ob, mtx, me, mats, armob, armname) )
	
	del ob_type
	#print ob_bones
	
	
	# this sucks a bit, remove all armatures that are used by a mesh.
	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		if arm:
			try:	ob_arms.remove(arm)
			except:	pass
	
	# now we have removed, get the unique names
	for i, ob in enumerate(ob_arms):
		name = sane_obname(ob)
		ob_arms[i] = name, ob
		
		for bone in ob.data.bones.values():
			ob_bones.append( (sane_obname(bone), bone, name, None, ob) )
	
	materials = [(sane_matname(mat), mat) for mat in materials.itervalues()]
	textures = [(sane_texname(img), img) for img in textures.itervalues()]
	materials.sort() # sort by name
	textures.sort()
	
	if not materials:
		materials = [('null', None)]
	
	material_mapping = {} # blen name : index
	if textures:
		texture_mapping_local = {None:0} # ditto
		i = 0
		for texname, tex in textures:
			texture_mapping_local[tex.name] = i
			i+=1
		textures.insert(0, ('_empty_', None))
	
	i = 0
	for matname, mat in materials:
		if mat: mat = mat.name
		material_mapping[mat] = i
		i+=1
	
	camera_count = 8
	file.write('''

; Object definitions
;------------------------------------------------------------------

Definitions:  {
	Version: 100
	Count: %i''' % (\
		1+1+camera_count+\
		len(ob_meshes)+\
		len(ob_lights)+\
		len(ob_cameras)+\
		len(ob_arms)+\
		len(ob_null)+\
		len(ob_bones)+\
		len(materials)+\
		(len(textures)*2))) # add 1 for the root model 1 for global settings
	
	file.write('''
	ObjectType: "Model" {
		Count: %i
	}''' % (\
		1+camera_count+\
		len(ob_meshes)+\
		len(ob_lights)+\
		len(ob_cameras)+\
		len(ob_arms)+\
		len(ob_null)+\
		len(ob_bones))) # add 1 for the root model
	
	file.write('''
	ObjectType: "Geometry" {
		Count: %i
	}''' % len(ob_meshes))
	
	if materials:
		file.write('''
	ObjectType: "Material" {
		Count: %i
	}''' % len(materials))
	
	if textures:
		file.write('''
	ObjectType: "Texture" {
		Count: %i
	}''' % len(textures)) # add 1 for an empty tex
		file.write('''
	ObjectType: "Video" {
		Count: %i
	}''' % len(textures)) # add 1 for an empty tex
	
	tmp = 0
	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		if armname:
			tmp+=1
	
	for bonename, bone, obname, me, armob in ob_bones:
		if me:
			tmp += 1
	
	if tmp:
		file.write('''
	ObjectType: "Deformer" {
		Count: %i
	}''' % tmp)
	del tmp
	
	# we could avoid writing this possibly but for now just write it
	"""
	file.write('''
	ObjectType: "Pose" {
		Count: 1
	}''')
	"""
	
	file.write('''
	ObjectType: "GlobalSettings" {
		Count: 1
	}
}''')
	
	file.write('''

; Object properties
;------------------------------------------------------------------

Objects:  {''')
	
	# To comply with other FBX FILES
	write_camera_switch()
	
	# Write the null object
	write_null(None, 'blend_root')
	
	for obname, ob in ob_null:
		write_null(ob, obname)
	
	for obname, ob in ob_arms:
		write_null(ob, obname) # armatures are just null's with bone children.
	
	for obname, ob in ob_cameras:
		write_camera(ob, obname)

	for obname, ob in ob_lights:
		write_light(ob, obname)
	
	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		file.write('\n\tModel: "Model::%s", "Mesh" {' % sane_obname(ob))
		file.write('\n\t\tVersion: 232') # newline is added in write_object_props
		write_object_props(ob, None, mtx)
		file.write('\n\t\t}')
		file.write('\n\t\tMultiLayer: 0')
		file.write('\n\t\tMultiTake: 1')
		file.write('\n\t\tShading: Y')
		file.write('\n\t\tCulling: "CullingOff"')
		
		# Write the Real Mesh data here
		file.write('\n\t\tVertices: ')
		i=-1
		for v in me.verts:
			if i==-1:
				file.write('%.6f,%.6f,%.6f' % tuple(v.co))
				i=0
			else:
				if i==7:
					file.write('\n\t\t')
					i=0
				file.write(',%.6f,%.6f,%.6f'% tuple(v.co))
			i+=1
		file.write('\n\t\tPolygonVertexIndex: ')
		i=-1
		for f in me.faces:
			fi = [v.index for v in f]
			# flip the last index, odd but it looks like
			# this is how fbx tells one face from another
			fi[-1] = -(fi[-1]+1)
			fi = tuple(fi)
			if i==-1:
				if len(f) == 3:		file.write('%i,%i,%i' % fi )
				else:				file.write('%i,%i,%i,%i' % fi )
				i=0
			else:
				if i==13:
					file.write('\n\t\t')
					i=0
				if len(f) == 3:		file.write(',%i,%i,%i' % fi )
				else:				file.write(',%i,%i,%i,%i' % fi )
			i+=1
		
		ed_val = [None, None]
		LOOSE = Blender.Mesh.EdgeFlags.LOOSE
		for ed in me.edges:
			if ed.flag & LOOSE:
				ed_val[0] = ed.v1.index
				ed_val[1] = -(ed.v2.index+1)
				if i==-1:
					file.write('%i,%i' % tuple(ed_val) )
					i=0
				else:
					if i==13:
						file.write('\n\t\t')
						i=0
					file.write(',%i,%i' % tuple(ed_val) )
				i+=1
		del LOOSE
		
		file.write('\n\t\tGeometryVersion: 124')
		
		file.write('''
		LayerElementNormal: 0 {
			Version: 101
			Name: ""
			MappingInformationType: "ByVertice"
			ReferenceInformationType: "Direct"
			Normals: ''')

		i=-1
		for v in me.verts:
			if i==-1:
				file.write('%.15f,%.15f,%.15f' % tuple(v.no))
				i=0
			else:
				if i==2:
					file.write('\n			 ')
					i=0
				file.write(',%.15f,%.15f,%.15f' % tuple(v.no))
			i+=1
		file.write('\n\t\t}')
		
		
		# Write VertexColor Layers
		collayers = []
		if me.vertexColors:
			collayers = me.getColorLayerNames()
			collayer_orig = me.activeColorLayer
			for colindex, collayer in enumerate(collayers):
				me.activeColorLayer = collayer
				file.write('\n\t\tLayerElementColor: %i {' % colindex)
				file.write('\n\t\t\tVersion: 101')
				file.write('\n\t\t\tName: "%s"' % collayer)
				
				file.write('''
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "IndexToDirect"
			Colors: ''')
			
				i = -1
				ii = 0 # Count how many Colors we write
				
				for f in me.faces:
					for col in f.col:
						if i==-1:
							file.write('%i,%i,%i' % (col[0], col[1], col[2]))
							i=0
						else:
							if i==7:
								file.write('\n\t\t\t\t')
								i=0
							file.write(',%i,%i,%i' % (col[0], col[1], col[2]))
						i+=1
						ii+=1 # One more Color
				
				file.write('\n\t\t\tColorIndex: ')
				i = -1
				for j in xrange(ii):
					if i == -1:
						file.write('%i' % j)
						i=0
					else:
						if i==55:
							file.write('\n\t\t\t\t')
							i=0
						file.write(',%i' % j)
					i+=1
				
				file.write('\n\t\t}')
		
		
		
		# Write UV and texture layers.
		uvlayers = []
		if me.faceUV:
			uvlayers = me.getUVLayerNames()
			uvlayer_orig = me.activeUVLayer
			for uvindex, uvlayer in enumerate(uvlayers):
				me.activeUVLayer = uvlayer
				file.write('\n\t\tLayerElementUV: %i {' % uvindex)
				file.write('\n\t\t\tVersion: 101')
				file.write('\n\t\t\tName: "%s"' % uvlayer)
				
				file.write('''
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "IndexToDirect"
			UV: ''')
			
				i = -1
				ii = 0 # Count how many UVs we write
				
				for f in me.faces:
					for uv in f.uv:
						if i==-1:
							file.write('%.6f,%.6f' % tuple(uv))
							i=0
						else:
							if i==7:
								file.write('\n			 ')
								i=0
							file.write(',%.6f,%.6f' % tuple(uv))
						i+=1
						ii+=1 # One more UV
				
				file.write('\n\t\t\tUVIndex: ')
				i = -1
				for j in xrange(ii):
					if i == -1:
						file.write('%i'  % j)
						i=0
					else:
						if i==55:
							file.write('\n\t\t\t\t')
							i=0
						file.write(',%i' % j)
					i+=1
				
				file.write('\n\t\t}')
				
				if textures:
					file.write('\n\t\tLayerElementTexture: %i {' % uvindex)
					file.write('\n\t\t\tVersion: 101')
					file.write('\n\t\t\tName: "%s"' % uvlayer)
					
					file.write('''
			MappingInformationType: "ByPolygon"
			ReferenceInformationType: "IndexToDirect"
			BlendMode: "Translucent"
			TextureAlpha: 1
			TextureId: ''')
					i=-1
					for f in me.faces:
						img_key = f.image
						if img_key: img_key = img_key.name
						
						if i==-1:
							i=0
							file.write( '%s' % texture_mapping_local[img_key])
						else:
							if i==55:
								file.write('\n			 ')
								i=0
							
							file.write(',%s' % texture_mapping_local[img_key])
						i+=1
				else:
					file.write('''
		LayerElementTexture: 0 {
			Version: 101
			Name: ""
			MappingInformationType: "NoMappingInformation"
			ReferenceInformationType: "IndexToDirect"
			BlendMode: "Translucent"
			TextureAlpha: 1
			TextureId: ''')
				file.write('\n\t\t}')
			
			me.activeUVLayer = uvlayer_orig
			
		# Done with UV/textures.
		
		if materials:
			file.write('''
		LayerElementMaterial: 0 {
			Version: 101
			Name: ""
			MappingInformationType: "ByPolygon"
			ReferenceInformationType: "IndexToDirect"
			Materials: ''')
			
			# Build a material mapping for this 
			material_mapping_local = {} # local-index : global index.
			for i, mat in enumerate(mats):
				if mat:
					material_mapping_local[i] = material_mapping[mat.name]
				else:
					material_mapping_local[i] = 0 # None material is zero for now.
			
			if not material_mapping_local:
				material_mapping_local[0] = 0
			
			len_material_mapping_local = len(material_mapping_local)
			
			i=-1
			for f in me.faces:
				f_mat = f.mat
				if f_mat >= len_material_mapping_local:
					f_mat = 0
				
				if i==-1:
					i=0
					file.write( '%s' % material_mapping_local[f_mat])
				else:
					if i==55:
						file.write('\n\t\t\t\t')
						i=0
					
					file.write(',%s' % material_mapping_local[f_mat])
				i+=1
			
			file.write('\n\t\t}')
		
		file.write('''
		Layer: 0 {
			Version: 100
			LayerElement:  {
				Type: "LayerElementNormal"
				TypedIndex: 0
			}''')
		
		if materials:
			file.write('''
			LayerElement:  {
				Type: "LayerElementMaterial"
				TypedIndex: 0
			}''')
			
		# Always write this
		if textures:
			file.write('''
			LayerElement:  {
				Type: "LayerElementTexture"
				TypedIndex: 0
			}''')
		
		if me.vertexColors:
			file.write('''
			LayerElement:  {
				Type: "LayerElementColor"
				TypedIndex: 0
			}''')
		
		if me.faceUV:
			file.write('''
			LayerElement:  {
				Type: "LayerElementUV"
				TypedIndex: 0
			}''')
		
		
		file.write('\n\t\t}')
		
		if len(uvlayers) > 1:
			for i in xrange(1, len(uvlayers)):
				
				file.write('\n\t\tLayer: %i {' % i)
				file.write('\n\t\t\tVersion: 100')
				
				file.write('''
			LayerElement:  {
				Type: "LayerElementUV"''')
				
				file.write('\n\t\t\t\tTypedIndex: %i' % i)
				file.write('\n\t\t\t}')
				
				if textures:
					
					file.write('''
			LayerElement:  {
				Type: "LayerElementTexture"''')
					
					file.write('\n\t\t\t\tTypedIndex: %i' % i)
					file.write('\n\t\t\t}')
				
				file.write('\n\t\t}')
		
		if len(collayers) > 1:
			# Take into account any UV layers
			layer_offset = 0
			if uvlayers: layer_offset = len(uvlayers)-1
			
			for i in xrange(layer_offset, len(collayers)+layer_offset):
				file.write('\n\t\tLayer: %i {' % i)
				file.write('\n\t\t\tVersion: 100')
				
				file.write('''
			LayerElement:  {
				Type: "LayerElementColor"''')
				
				file.write('\n\t\t\t\tTypedIndex: %i' % i)
				file.write('\n\t\t\t}')
				file.write('\n\t\t}')
		file.write('\n\t}')
	
	for bonename, bone, obname, me, armob in ob_bones:
		write_bone(bone, bonename, armob.matrixWorld)
	
	write_camera_default()
	
	for matname, mat in materials:
		write_material(matname, mat)
	
	# each texture uses a video, odd
	for texname, tex in textures:
		write_video(texname, tex)
	i = 0
	for texname, tex in textures:
		write_texture(texname, tex, i)
		i+=1
	
	# Write armature modifiers
	# TODO - add another MODEL? - because of this skin definition.
	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		if armname:
			write_deformer_skin(obname)
		
		for bonename, bone, obname, bone_mesh, armob in ob_bones:
			if bone_mesh == me:
				write_sub_deformer_skin(obname, bonename, bone, me, armob.matrixWorld)
	
	# Write pose's really weired, only needed when an armature and mesh are used together
	# each by themselves dont need pose data. for now only pose meshes and bones
	"""
	file.write('''
	Pose: "Pose::BIND_POSES", "BindPose" {
		Type: "BindPose"
		Version: 100
		Properties60:  {
		}
		NbPoseNodes: ''')
		
	file.write(str(\
	 len(ob_meshes)+\
	 len(ob_bones)
	))
	
	for tmp in 	(ob_meshes, ob_bones):
		for ob in tmp:
			file.write('\n\t\tPoseNode:  {')
			file.write('\n\t\t\tNode: "Model::%s"' % ob[0] )					# the first item is the fbx-name
			file.write('\n\t\t\tMatrix: %s' % mat4x4str(object_tx(ob[1], None, None)[3]))	# second item is the object or bone
			file.write('\n\t\t}')
	
	file.write('\n\t}')
	"""
	
	
	
	# Finish Writing Objects
	# Write global settings
	file.write('''
	GlobalSettings:  {
		Version: 1000
		Properties60:  {
			Property: "UpAxis", "int", "",1
			Property: "UpAxisSign", "int", "",1
			Property: "FrontAxis", "int", "",2
			Property: "FrontAxisSign", "int", "",1
			Property: "CoordAxis", "int", "",0
			Property: "CoordAxisSign", "int", "",1
			Property: "UnitScaleFactor", "double", "",1
		}
	}
''')	
	file.write('}')
	
	file.write('''

; Object relations
;------------------------------------------------------------------

Relations:  {''')

	file.write('\n\tModel: "Model::blend_root", "Null" {\n\t}')

	for obname, ob in ob_null:
		file.write('\n\tModel: "Model::%s", "Null" {\n\t}' % obname)

	for obname, ob in ob_arms:
		file.write('\n\tModel: "Model::%s", "Null" {\n\t}' % obname)

	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		file.write('\n\tModel: "Model::%s", "Mesh" {\n\t}' % obname)

	# TODO - limbs can have the same name for multiple armatures, should prefix.
	for bonename, bone, obname, me, armob in ob_bones:
		file.write('\n\tModel: "Model::%s", "Limb" {\n\t}' % bonename)
	
	for obname, ob in ob_cameras:
		file.write('\n\tModel: "Model::%s", "Camera" {\n\t}' % obname)
	
	for obname, ob in ob_lights:
		file.write('\n\tModel: "Model::%s", "Light" {\n\t}' % obname)
	
	file.write('''
	Model: "Model::Producer Perspective", "Camera" {
	}
	Model: "Model::Producer Top", "Camera" {
	}
	Model: "Model::Producer Bottom", "Camera" {
	}
	Model: "Model::Producer Front", "Camera" {
	}
	Model: "Model::Producer Back", "Camera" {
	}
	Model: "Model::Producer Right", "Camera" {
	}
	Model: "Model::Producer Left", "Camera" {
	}
	Model: "Model::Camera Switcher", "CameraSwitcher" {
	}''')
	
	for matname, mat in materials:
		file.write('\n\tMaterial: "Material::%s", "" {\n\t}' % matname)

	if textures:
		for texname, tex in textures:
			file.write('\n\tTexture: "Texture::%s", "TextureVideoClip" {\n\t}' % texname)
		for texname, tex in textures:
			file.write('\n\tVideo: "Video::%s", "Clip" {\n\t}' % texname)

	# deformers - modifiers
	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		if arm:
			file.write('\n\tDeformer: "Deformer::Skin %s", "Skin" {\n\t}' % obname)
	
	for bonename, bone, obname, me, armob in ob_bones:
		if me: # is this bone effecting a mesh?
			file.write('\n\tDeformer: "SubDeformer::Cluster %s", "Cluster" {\n\t}' % bonename)
	
	
	# This should be at the end
	# file.write('\n\tPose: "Pose::BIND_POSES", "BindPose" {\n\t}')
	
	file.write('\n}')
	file.write('''

; Object connections
;------------------------------------------------------------------

Connections:  {''')

	#ob_bones.reverse()
	#print ob_bones[0]

	# write the fake root node
	file.write('\n\tConnect: "OO", "Model::blend_root", "Model::Scene"')
	
	for obname, ob in ob_null:
		if ob.parent:
			file.write('\n\tConnect: "OO", "Model::%s", "Model::%s"' % (obname, sane_matname(ob.parent)))
		else:
			file.write('\n\tConnect: "OO", "Model::%s", "Model::blend_root"' % obname)

	for obname, ob in ob_arms:
		file.write('\n\tConnect: "OO", "Model::%s", "Model::blend_root"' % obname)

	for obname, ob in ob_cameras:
		file.write('\n\tConnect: "OO", "Model::%s", "Model::blend_root"' % obname)

	for obname, ob in ob_cameras:
		file.write('\n\tConnect: "OO", "Model::%s", "Model::blend_root"' % obname)
	
	for obname, ob in ob_lights:
		file.write('\n\tConnect: "OO", "Model::%s", "Model::blend_root"' % obname)
	
	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		# Connect all materials to all objects, not good form but ok for now.
		for mat in mats:
			file.write('\n\tConnect: "OO", "Material::%s", "Model::%s"' % (sane_matname(mat), obname))
	
	if textures:
		for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
			for texname, tex in textures:
				file.write('\n\tConnect: "OO", "Texture::%s", "Model::%s"' % (texname, obname))
		
		for texname, tex in textures:
			file.write('\n\tConnect: "OO", "Video::%s", "Texture::%s"' % (texname, texname))
	
	
	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		if arm:
			file.write('\n\tConnect: "OO", "Deformer::Skin %s", "Model::%s"' % (obname, obname))
	
	for bonename, bone, obname, me, armob in ob_bones:
		if me:
			file.write('\n\tConnect: "OO", "SubDeformer::Cluster %s", "Deformer::Skin %s"' % (bonename, obname))
	
	
	# limbs -> deformers
	for bonename, bone, obname, me, armob in ob_bones:
		if me:
			file.write('\n\tConnect: "OO", "Model::%s", "SubDeformer::Cluster %s"' % (bonename, bonename))
	
	
	
	
	for bonename, bone, obname, me, armob in ob_bones:
		blend_parent = bone.parent
		if blend_parent:
			file.write('\n\tConnect: "OO", "Model::%s", "Model::%s"' % (bonename, sane_obname(blend_parent)) )
		else:
			# NOTE, when 'me' is None, the obname is the armature-object,
			# the armature object is written as an empty and all root level bones connect to it
			file.write('\n\tConnect: "OO", "Model::%s", "Model::%s"' % (bonename, obname) )
	
	for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
		file.write('\n\tConnect: "OO", "Model::%s", "Model::blend_root"' % obname)
	
	file.write('\n}')
	
	
	# Clear mesh data Only when writing with modifiers applied
	#for obname, ob, me, mats, arm, armname in objects:
	#	me.verts = None


def write_footer(file, sce, world):
	
	tuple(world.hor)
	tuple(world.amb)
	
	has_mist = world.mode & 1
	
	mist_intense, mist_start, mist_end, mist_height = world.mist
	
	render = sce.render
	
	file.write('\n;Takes and animation section')
	file.write('\n;----------------------------------------------------')
	file.write('\n')
	file.write('\nTakes:  {')
	file.write('\n\tCurrent: ""')
	file.write('\n}')
	file.write('\n;Version 5 settings')
	file.write('\n;------------------------------------------------------------------')
	file.write('\n')
	file.write('\nVersion5:  {')
	file.write('\n\tAmbientRenderSettings:  {')
	file.write('\n\t\tVersion: 101')
	file.write('\n\t\tAmbientLightColor: %.1f,%.1f,%.1f,0' % tuple(world.amb))
	file.write('\n\t}')
	file.write('\n\tFogOptions:  {')
	file.write('\n\t\tFlogEnable: %i' % has_mist)
	file.write('\n\t\tFogMode: 0')
	file.write('\n\t\tFogDensity: %.3f' % mist_intense)
	file.write('\n\t\tFogStart: %.3f' % mist_start)
	file.write('\n\t\tFogEnd: %.3f' % mist_end)
	file.write('\n\t\tFogColor: %.1f,%.1f,%.1f,1' % tuple(world.hor))
	file.write('\n\t}')
	file.write('\n\tSettings:  {')
	file.write('\n\t\tFrameRate: "%i"' % render.fps)
	file.write('\n\t\tTimeFormat: 1')
	file.write('\n\t\tSnapOnFrames: 0')
	file.write('\n\t\tReferenceTimeIndex: -1')
	file.write('\n\t\tTimeLineStartTime: %i' % render.sFrame)
	file.write('\n\t\tTimeLineStopTime: %i' % render.eFrame)
	file.write('\n\t}')
	file.write('\n\tRendererSetting:  {')
	file.write('\n\t\tDefaultCamera: "Producer Perspective"')
	file.write('\n\t\tDefaultViewingMode: 0')
	file.write('\n\t}')
	file.write('\n}')
	file.write('\n')
	
	# Incase sombody imports this, clean up by clearing global dicts
	sane_name_mapping_ob.clear()
	sane_name_mapping_mat.clear()
	sane_name_mapping_tex.clear()

import bpy
def write_ui(filename):
	if not filename.lower().endswith('.fbx'):
		filename += '.fbx'
	
	#if not BPyMessages.Warning_SaveOver(filename):
	#	return
	sce = bpy.data.scenes.active
	world = sce.world
	
	Blender.Window.WaitCursor(1)
	file = open(filename, 'w')
	write_header(file)
	write_scene(file, sce, world)
	write_footer(file, sce, world)
	Blender.Window.WaitCursor(0)

if __name__ == '__main__':
	Blender.Window.FileSelector(write_ui, 'Export FBX', Blender.sys.makename(ext='.fbx'))
	#write_ui('/scratch/test.fbx')
