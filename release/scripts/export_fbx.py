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
import BPyMesh
import BPyMessages
import time

# Used to add the scene name into the filename without using odd chars
sane_name_mapping_ob = {}
sane_name_mapping_mat = {}
sane_name_mapping_tex = {}

def strip_path(p):
	return p.split('\\')[-1].split('/')[-1]

def sane_name(name, dct):
	
	try:		return dct[name]
	except:		pass
	
	orig_name = name
	for ch in ' /\\~!@#$%^&*()+=[];\':",./<>?\t\r\n':
		name = name.replace(ch, '_')
	dct[orig_name] = name
	return name

def sane_obname(name):
	return sane_name(name, sane_name_mapping_ob)

def sane_matname(name):
	return sane_name(name, sane_name_mapping_mat)

def sane_texname(name):
	return sane_name(name, sane_name_mapping_tex)

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
}
''' % (curtime))
	
	file.write('CreationTime: "%.4i-%.2i-%.2i %.2i:%.2i:%.2i:000"\n' % curtime)
	file.write('Creator: "Blender3D version %.2f"\n' % Blender.Get('version'))




def write_scene(file):
	
	def write_camera_switch():
		file.write('''
	Model: "Model::Camera Switcher", "CameraSwitcher" {
		Version: 232
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",0
			Property: "Lcl Translation", "Lcl Translation", "A+",0,0,0
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "Show", "bool", "",0
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0
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
	
	def write_cameras():
		# This sucks but to match FBX converter its easier to
		# write the cameras though they are not needed.
		file.write('''
	Model: "Model::Producer Perspective", "Camera" {
		Version: 232
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",0
			Property: "Lcl Translation", "Lcl Translation", "A+",0,71.3,287.5
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "Show", "bool", "",0
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0
			Property: "Color", "Color", "A",0.8,0.8,0.8
			Property: "Roll", "Roll", "A+",0
			Property: "FieldOfView", "FieldOfView", "A+",40
			Property: "FieldOfViewX", "FieldOfView", "A+",1
			Property: "FieldOfViewY", "FieldOfView", "A+",1
			Property: "OpticalCenterX", "Real", "A+",0
			Property: "OpticalCenterY", "Real", "A+",0
			Property: "BackgroundColor", "Color", "A+",0.63,0.63,0.63
			Property: "TurnTable", "Real", "A+",0
			Property: "DisplayTurnTableIcon", "bool", "",1
			Property: "Motion Blur Intensity", "Real", "A+",1
			Property: "UseMotionBlur", "bool", "",0
			Property: "UseRealTimeMotionBlur", "bool", "",1
			Property: "ResolutionMode", "enum", "",0
			Property: "ApertureMode", "enum", "",2
			Property: "GateFit", "enum", "",0
			Property: "FocalLength", "Real", "A+",21.3544940948486
			Property: "CameraFormat", "enum", "",0
			Property: "AspectW", "double", "",320
			Property: "AspectH", "double", "",200
			Property: "PixelAspectRatio", "double", "",1
			Property: "UseFrameColor", "bool", "",0
			Property: "FrameColor", "ColorRGB", "",0.3,0.3,0.3
			Property: "ShowName", "bool", "",1
			Property: "ShowGrid", "bool", "",1
			Property: "ShowOpticalCenter", "bool", "",0
			Property: "ShowAzimut", "bool", "",1
			Property: "ShowTimeCode", "bool", "",0
			Property: "NearPlane", "double", "",10
			Property: "FarPlane", "double", "",4000
			Property: "FilmWidth", "double", "",0.816
			Property: "FilmHeight", "double", "",0.612
			Property: "FilmAspectRatio", "double", "",1.33333333333333
			Property: "FilmSqueezeRatio", "double", "",1
			Property: "FilmFormatIndex", "enum", "",4
			Property: "ViewFrustum", "bool", "",1
			Property: "ViewFrustumNearFarPlane", "bool", "",0
			Property: "ViewFrustumBackPlaneMode", "enum", "",2
			Property: "BackPlaneDistance", "double", "",100
			Property: "BackPlaneDistanceMode", "enum", "",0
			Property: "ViewCameraToLookAt", "bool", "",1
			Property: "LockMode", "bool", "",0
			Property: "LockInterestNavigation", "bool", "",0
			Property: "FitImage", "bool", "",0
			Property: "Crop", "bool", "",0
			Property: "Center", "bool", "",1
			Property: "KeepRatio", "bool", "",1
			Property: "BackgroundMode", "enum", "",0
			Property: "BackgroundAlphaTreshold", "double", "",0.5
			Property: "ForegroundTransparent", "bool", "",1
			Property: "DisplaySafeArea", "bool", "",0
			Property: "SafeAreaDisplayStyle", "enum", "",1
			Property: "SafeAreaAspectRatio", "double", "",1.33333333333333
			Property: "Use2DMagnifierZoom", "bool", "",0
			Property: "2D Magnifier Zoom", "Real", "A+",100
			Property: "2D Magnifier X", "Real", "A+",50
			Property: "2D Magnifier Y", "Real", "A+",50
			Property: "CameraProjectionType", "enum", "",0
			Property: "UseRealTimeDOFAndAA", "bool", "",0
			Property: "UseDepthOfField", "bool", "",0
			Property: "FocusSource", "enum", "",0
			Property: "FocusAngle", "double", "",3.5
			Property: "FocusDistance", "double", "",200
			Property: "UseAntialiasing", "bool", "",0
			Property: "AntialiasingIntensity", "double", "",0.77777
			Property: "UseAccumulationBuffer", "bool", "",0
			Property: "FrameSamplingCount", "int", "",7
		}
		MultiLayer: 0
		MultiTake: 0
		Hidden: "True"
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Camera"
		GeometryVersion: 124
		Position: 0,71.3,287.5
		Up: 0,1,0
		LookAt: 0,0,0
		ShowInfoOnMoving: 1
		ShowAudio: 0
		AudioColor: 0,1,0
		CameraOrthoZoom: 1
	}
	Model: "Model::Producer Top", "Camera" {
		Version: 232
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",0
			Property: "Lcl Translation", "Lcl Translation", "A+",0,4000,0
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "Show", "bool", "",0
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0
			Property: "Color", "Color", "A",0.8,0.8,0.8
			Property: "Roll", "Roll", "A+",0
			Property: "FieldOfView", "FieldOfView", "A+",40
			Property: "FieldOfViewX", "FieldOfView", "A+",1
			Property: "FieldOfViewY", "FieldOfView", "A+",1
			Property: "OpticalCenterX", "Real", "A+",0
			Property: "OpticalCenterY", "Real", "A+",0
			Property: "BackgroundColor", "Color", "A+",0.63,0.63,0.63
			Property: "TurnTable", "Real", "A+",0
			Property: "DisplayTurnTableIcon", "bool", "",1
			Property: "Motion Blur Intensity", "Real", "A+",1
			Property: "UseMotionBlur", "bool", "",0
			Property: "UseRealTimeMotionBlur", "bool", "",1
			Property: "ResolutionMode", "enum", "",0
			Property: "ApertureMode", "enum", "",2
			Property: "GateFit", "enum", "",0
			Property: "FocalLength", "Real", "A+",21.3544940948486
			Property: "CameraFormat", "enum", "",0
			Property: "AspectW", "double", "",320
			Property: "AspectH", "double", "",200
			Property: "PixelAspectRatio", "double", "",1
			Property: "UseFrameColor", "bool", "",0
			Property: "FrameColor", "ColorRGB", "",0.3,0.3,0.3
			Property: "ShowName", "bool", "",1
			Property: "ShowGrid", "bool", "",1
			Property: "ShowOpticalCenter", "bool", "",0
			Property: "ShowAzimut", "bool", "",1
			Property: "ShowTimeCode", "bool", "",0
			Property: "NearPlane", "double", "",1
			Property: "FarPlane", "double", "",30000
			Property: "FilmWidth", "double", "",0.816
			Property: "FilmHeight", "double", "",0.612
			Property: "FilmAspectRatio", "double", "",1.33333333333333
			Property: "FilmSqueezeRatio", "double", "",1
			Property: "FilmFormatIndex", "enum", "",4
			Property: "ViewFrustum", "bool", "",1
			Property: "ViewFrustumNearFarPlane", "bool", "",0
			Property: "ViewFrustumBackPlaneMode", "enum", "",2
			Property: "BackPlaneDistance", "double", "",100
			Property: "BackPlaneDistanceMode", "enum", "",0
			Property: "ViewCameraToLookAt", "bool", "",1
			Property: "LockMode", "bool", "",0
			Property: "LockInterestNavigation", "bool", "",0
			Property: "FitImage", "bool", "",0
			Property: "Crop", "bool", "",0
			Property: "Center", "bool", "",1
			Property: "KeepRatio", "bool", "",1
			Property: "BackgroundMode", "enum", "",0
			Property: "BackgroundAlphaTreshold", "double", "",0.5
			Property: "ForegroundTransparent", "bool", "",1
			Property: "DisplaySafeArea", "bool", "",0
			Property: "SafeAreaDisplayStyle", "enum", "",1
			Property: "SafeAreaAspectRatio", "double", "",1.33333333333333
			Property: "Use2DMagnifierZoom", "bool", "",0
			Property: "2D Magnifier Zoom", "Real", "A+",100
			Property: "2D Magnifier X", "Real", "A+",50
			Property: "2D Magnifier Y", "Real", "A+",50
			Property: "CameraProjectionType", "enum", "",1
			Property: "UseRealTimeDOFAndAA", "bool", "",0
			Property: "UseDepthOfField", "bool", "",0
			Property: "FocusSource", "enum", "",0
			Property: "FocusAngle", "double", "",3.5
			Property: "FocusDistance", "double", "",200
			Property: "UseAntialiasing", "bool", "",0
			Property: "AntialiasingIntensity", "double", "",0.77777
			Property: "UseAccumulationBuffer", "bool", "",0
			Property: "FrameSamplingCount", "int", "",7
		}
		MultiLayer: 0
		MultiTake: 0
		Hidden: "True"
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Camera"
		GeometryVersion: 124
		Position: 0,4000,0
		Up: 0,0,-1
		LookAt: 0,0,0
		ShowInfoOnMoving: 1
		ShowAudio: 0
		AudioColor: 0,1,0
		CameraOrthoZoom: 1
	}
	Model: "Model::Producer Bottom", "Camera" {
		Version: 232
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",0
			Property: "Lcl Translation", "Lcl Translation", "A+",0,-4000,0
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "Show", "bool", "",0
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0
			Property: "Color", "Color", "A",0.8,0.8,0.8
			Property: "Roll", "Roll", "A+",0
			Property: "FieldOfView", "FieldOfView", "A+",40
			Property: "FieldOfViewX", "FieldOfView", "A+",1
			Property: "FieldOfViewY", "FieldOfView", "A+",1
			Property: "OpticalCenterX", "Real", "A+",0
			Property: "OpticalCenterY", "Real", "A+",0
			Property: "BackgroundColor", "Color", "A+",0.63,0.63,0.63
			Property: "TurnTable", "Real", "A+",0
			Property: "DisplayTurnTableIcon", "bool", "",1
			Property: "Motion Blur Intensity", "Real", "A+",1
			Property: "UseMotionBlur", "bool", "",0
			Property: "UseRealTimeMotionBlur", "bool", "",1
			Property: "ResolutionMode", "enum", "",0
			Property: "ApertureMode", "enum", "",2
			Property: "GateFit", "enum", "",0
			Property: "FocalLength", "Real", "A+",21.3544940948486
			Property: "CameraFormat", "enum", "",0
			Property: "AspectW", "double", "",320
			Property: "AspectH", "double", "",200
			Property: "PixelAspectRatio", "double", "",1
			Property: "UseFrameColor", "bool", "",0
			Property: "FrameColor", "ColorRGB", "",0.3,0.3,0.3
			Property: "ShowName", "bool", "",1
			Property: "ShowGrid", "bool", "",1
			Property: "ShowOpticalCenter", "bool", "",0
			Property: "ShowAzimut", "bool", "",1
			Property: "ShowTimeCode", "bool", "",0
			Property: "NearPlane", "double", "",1
			Property: "FarPlane", "double", "",30000
			Property: "FilmWidth", "double", "",0.816
			Property: "FilmHeight", "double", "",0.612
			Property: "FilmAspectRatio", "double", "",1.33333333333333
			Property: "FilmSqueezeRatio", "double", "",1
			Property: "FilmFormatIndex", "enum", "",4
			Property: "ViewFrustum", "bool", "",1
			Property: "ViewFrustumNearFarPlane", "bool", "",0
			Property: "ViewFrustumBackPlaneMode", "enum", "",2
			Property: "BackPlaneDistance", "double", "",100
			Property: "BackPlaneDistanceMode", "enum", "",0
			Property: "ViewCameraToLookAt", "bool", "",1
			Property: "LockMode", "bool", "",0
			Property: "LockInterestNavigation", "bool", "",0
			Property: "FitImage", "bool", "",0
			Property: "Crop", "bool", "",0
			Property: "Center", "bool", "",1
			Property: "KeepRatio", "bool", "",1
			Property: "BackgroundMode", "enum", "",0
			Property: "BackgroundAlphaTreshold", "double", "",0.5
			Property: "ForegroundTransparent", "bool", "",1
			Property: "DisplaySafeArea", "bool", "",0
			Property: "SafeAreaDisplayStyle", "enum", "",1
			Property: "SafeAreaAspectRatio", "double", "",1.33333333333333
			Property: "Use2DMagnifierZoom", "bool", "",0
			Property: "2D Magnifier Zoom", "Real", "A+",100
			Property: "2D Magnifier X", "Real", "A+",50
			Property: "2D Magnifier Y", "Real", "A+",50
			Property: "CameraProjectionType", "enum", "",1
			Property: "UseRealTimeDOFAndAA", "bool", "",0
			Property: "UseDepthOfField", "bool", "",0
			Property: "FocusSource", "enum", "",0
			Property: "FocusAngle", "double", "",3.5
			Property: "FocusDistance", "double", "",200
			Property: "UseAntialiasing", "bool", "",0
			Property: "AntialiasingIntensity", "double", "",0.77777
			Property: "UseAccumulationBuffer", "bool", "",0
			Property: "FrameSamplingCount", "int", "",7
		}
		MultiLayer: 0
		MultiTake: 0
		Hidden: "True"
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Camera"
		GeometryVersion: 124
		Position: 0,-4000,0
		Up: 0,0,-1
		LookAt: 0,0,0
		ShowInfoOnMoving: 1
		ShowAudio: 0
		AudioColor: 0,1,0
		CameraOrthoZoom: 1
	}
	Model: "Model::Producer Front", "Camera" {
		Version: 232
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",0
			Property: "Lcl Translation", "Lcl Translation", "A+",0,0,4000
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "Show", "bool", "",0
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0
			Property: "Color", "Color", "A",0.8,0.8,0.8
			Property: "Roll", "Roll", "A+",0
			Property: "FieldOfView", "FieldOfView", "A+",40
			Property: "FieldOfViewX", "FieldOfView", "A+",1
			Property: "FieldOfViewY", "FieldOfView", "A+",1
			Property: "OpticalCenterX", "Real", "A+",0
			Property: "OpticalCenterY", "Real", "A+",0
			Property: "BackgroundColor", "Color", "A+",0.63,0.63,0.63
			Property: "TurnTable", "Real", "A+",0
			Property: "DisplayTurnTableIcon", "bool", "",1
			Property: "Motion Blur Intensity", "Real", "A+",1
			Property: "UseMotionBlur", "bool", "",0
			Property: "UseRealTimeMotionBlur", "bool", "",1
			Property: "ResolutionMode", "enum", "",0
			Property: "ApertureMode", "enum", "",2
			Property: "GateFit", "enum", "",0
			Property: "FocalLength", "Real", "A+",21.3544940948486
			Property: "CameraFormat", "enum", "",0
			Property: "AspectW", "double", "",320
			Property: "AspectH", "double", "",200
			Property: "PixelAspectRatio", "double", "",1
			Property: "UseFrameColor", "bool", "",0
			Property: "FrameColor", "ColorRGB", "",0.3,0.3,0.3
			Property: "ShowName", "bool", "",1
			Property: "ShowGrid", "bool", "",1
			Property: "ShowOpticalCenter", "bool", "",0
			Property: "ShowAzimut", "bool", "",1
			Property: "ShowTimeCode", "bool", "",0
			Property: "NearPlane", "double", "",1
			Property: "FarPlane", "double", "",30000
			Property: "FilmWidth", "double", "",0.816
			Property: "FilmHeight", "double", "",0.612
			Property: "FilmAspectRatio", "double", "",1.33333333333333
			Property: "FilmSqueezeRatio", "double", "",1
			Property: "FilmFormatIndex", "enum", "",4
			Property: "ViewFrustum", "bool", "",1
			Property: "ViewFrustumNearFarPlane", "bool", "",0
			Property: "ViewFrustumBackPlaneMode", "enum", "",2
			Property: "BackPlaneDistance", "double", "",100
			Property: "BackPlaneDistanceMode", "enum", "",0
			Property: "ViewCameraToLookAt", "bool", "",1
			Property: "LockMode", "bool", "",0
			Property: "LockInterestNavigation", "bool", "",0
			Property: "FitImage", "bool", "",0
			Property: "Crop", "bool", "",0
			Property: "Center", "bool", "",1
			Property: "KeepRatio", "bool", "",1
			Property: "BackgroundMode", "enum", "",0
			Property: "BackgroundAlphaTreshold", "double", "",0.5
			Property: "ForegroundTransparent", "bool", "",1
			Property: "DisplaySafeArea", "bool", "",0
			Property: "SafeAreaDisplayStyle", "enum", "",1
			Property: "SafeAreaAspectRatio", "double", "",1.33333333333333
			Property: "Use2DMagnifierZoom", "bool", "",0
			Property: "2D Magnifier Zoom", "Real", "A+",100
			Property: "2D Magnifier X", "Real", "A+",50
			Property: "2D Magnifier Y", "Real", "A+",50
			Property: "CameraProjectionType", "enum", "",1
			Property: "UseRealTimeDOFAndAA", "bool", "",0
			Property: "UseDepthOfField", "bool", "",0
			Property: "FocusSource", "enum", "",0
			Property: "FocusAngle", "double", "",3.5
			Property: "FocusDistance", "double", "",200
			Property: "UseAntialiasing", "bool", "",0
			Property: "AntialiasingIntensity", "double", "",0.77777
			Property: "UseAccumulationBuffer", "bool", "",0
			Property: "FrameSamplingCount", "int", "",7
		}
		MultiLayer: 0
		MultiTake: 0
		Hidden: "True"
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Camera"
		GeometryVersion: 124
		Position: 0,0,4000
		Up: 0,1,0
		LookAt: 0,0,0
		ShowInfoOnMoving: 1
		ShowAudio: 0
		AudioColor: 0,1,0
		CameraOrthoZoom: 1
	}
	Model: "Model::Producer Back", "Camera" {
		Version: 232
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",0
			Property: "Lcl Translation", "Lcl Translation", "A+",0,0,-4000
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "Show", "bool", "",0
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0
			Property: "Color", "Color", "A",0.8,0.8,0.8
			Property: "Roll", "Roll", "A+",0
			Property: "FieldOfView", "FieldOfView", "A+",40
			Property: "FieldOfViewX", "FieldOfView", "A+",1
			Property: "FieldOfViewY", "FieldOfView", "A+",1
			Property: "OpticalCenterX", "Real", "A+",0
			Property: "OpticalCenterY", "Real", "A+",0
			Property: "BackgroundColor", "Color", "A+",0.63,0.63,0.63
			Property: "TurnTable", "Real", "A+",0
			Property: "DisplayTurnTableIcon", "bool", "",1
			Property: "Motion Blur Intensity", "Real", "A+",1
			Property: "UseMotionBlur", "bool", "",0
			Property: "UseRealTimeMotionBlur", "bool", "",1
			Property: "ResolutionMode", "enum", "",0
			Property: "ApertureMode", "enum", "",2
			Property: "GateFit", "enum", "",0
			Property: "FocalLength", "Real", "A+",21.3544940948486
			Property: "CameraFormat", "enum", "",0
			Property: "AspectW", "double", "",320
			Property: "AspectH", "double", "",200
			Property: "PixelAspectRatio", "double", "",1
			Property: "UseFrameColor", "bool", "",0
			Property: "FrameColor", "ColorRGB", "",0.3,0.3,0.3
			Property: "ShowName", "bool", "",1
			Property: "ShowGrid", "bool", "",1
			Property: "ShowOpticalCenter", "bool", "",0
			Property: "ShowAzimut", "bool", "",1
			Property: "ShowTimeCode", "bool", "",0
			Property: "NearPlane", "double", "",1
			Property: "FarPlane", "double", "",30000
			Property: "FilmWidth", "double", "",0.816
			Property: "FilmHeight", "double", "",0.612
			Property: "FilmAspectRatio", "double", "",1.33333333333333
			Property: "FilmSqueezeRatio", "double", "",1
			Property: "FilmFormatIndex", "enum", "",4
			Property: "ViewFrustum", "bool", "",1
			Property: "ViewFrustumNearFarPlane", "bool", "",0
			Property: "ViewFrustumBackPlaneMode", "enum", "",2
			Property: "BackPlaneDistance", "double", "",100
			Property: "BackPlaneDistanceMode", "enum", "",0
			Property: "ViewCameraToLookAt", "bool", "",1
			Property: "LockMode", "bool", "",0
			Property: "LockInterestNavigation", "bool", "",0
			Property: "FitImage", "bool", "",0
			Property: "Crop", "bool", "",0
			Property: "Center", "bool", "",1
			Property: "KeepRatio", "bool", "",1
			Property: "BackgroundMode", "enum", "",0
			Property: "BackgroundAlphaTreshold", "double", "",0.5
			Property: "ForegroundTransparent", "bool", "",1
			Property: "DisplaySafeArea", "bool", "",0
			Property: "SafeAreaDisplayStyle", "enum", "",1
			Property: "SafeAreaAspectRatio", "double", "",1.33333333333333
			Property: "Use2DMagnifierZoom", "bool", "",0
			Property: "2D Magnifier Zoom", "Real", "A+",100
			Property: "2D Magnifier X", "Real", "A+",50
			Property: "2D Magnifier Y", "Real", "A+",50
			Property: "CameraProjectionType", "enum", "",1
			Property: "UseRealTimeDOFAndAA", "bool", "",0
			Property: "UseDepthOfField", "bool", "",0
			Property: "FocusSource", "enum", "",0
			Property: "FocusAngle", "double", "",3.5
			Property: "FocusDistance", "double", "",200
			Property: "UseAntialiasing", "bool", "",0
			Property: "AntialiasingIntensity", "double", "",0.77777
			Property: "UseAccumulationBuffer", "bool", "",0
			Property: "FrameSamplingCount", "int", "",7
		}
		MultiLayer: 0
		MultiTake: 0
		Hidden: "True"
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Camera"
		GeometryVersion: 124
		Position: 0,0,-4000
		Up: 0,1,0
		LookAt: 0,0,0
		ShowInfoOnMoving: 1
		ShowAudio: 0
		AudioColor: 0,1,0
		CameraOrthoZoom: 1
	}
	Model: "Model::Producer Right", "Camera" {
		Version: 232
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",0
			Property: "Lcl Translation", "Lcl Translation", "A+",4000,0,0
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "Show", "bool", "",0
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0
			Property: "Color", "Color", "A",0.8,0.8,0.8
			Property: "Roll", "Roll", "A+",0
			Property: "FieldOfView", "FieldOfView", "A+",40
			Property: "FieldOfViewX", "FieldOfView", "A+",1
			Property: "FieldOfViewY", "FieldOfView", "A+",1
			Property: "OpticalCenterX", "Real", "A+",0
			Property: "OpticalCenterY", "Real", "A+",0
			Property: "BackgroundColor", "Color", "A+",0.63,0.63,0.63
			Property: "TurnTable", "Real", "A+",0
			Property: "DisplayTurnTableIcon", "bool", "",1
			Property: "Motion Blur Intensity", "Real", "A+",1
			Property: "UseMotionBlur", "bool", "",0
			Property: "UseRealTimeMotionBlur", "bool", "",1
			Property: "ResolutionMode", "enum", "",0
			Property: "ApertureMode", "enum", "",2
			Property: "GateFit", "enum", "",0
			Property: "FocalLength", "Real", "A+",21.3544940948486
			Property: "CameraFormat", "enum", "",0
			Property: "AspectW", "double", "",320
			Property: "AspectH", "double", "",200
			Property: "PixelAspectRatio", "double", "",1
			Property: "UseFrameColor", "bool", "",0
			Property: "FrameColor", "ColorRGB", "",0.3,0.3,0.3
			Property: "ShowName", "bool", "",1
			Property: "ShowGrid", "bool", "",1
			Property: "ShowOpticalCenter", "bool", "",0
			Property: "ShowAzimut", "bool", "",1
			Property: "ShowTimeCode", "bool", "",0
			Property: "NearPlane", "double", "",1
			Property: "FarPlane", "double", "",30000
			Property: "FilmWidth", "double", "",0.816
			Property: "FilmHeight", "double", "",0.612
			Property: "FilmAspectRatio", "double", "",1.33333333333333
			Property: "FilmSqueezeRatio", "double", "",1
			Property: "FilmFormatIndex", "enum", "",4
			Property: "ViewFrustum", "bool", "",1
			Property: "ViewFrustumNearFarPlane", "bool", "",0
			Property: "ViewFrustumBackPlaneMode", "enum", "",2
			Property: "BackPlaneDistance", "double", "",100
			Property: "BackPlaneDistanceMode", "enum", "",0
			Property: "ViewCameraToLookAt", "bool", "",1
			Property: "LockMode", "bool", "",0
			Property: "LockInterestNavigation", "bool", "",0
			Property: "FitImage", "bool", "",0
			Property: "Crop", "bool", "",0
			Property: "Center", "bool", "",1
			Property: "KeepRatio", "bool", "",1
			Property: "BackgroundMode", "enum", "",0
			Property: "BackgroundAlphaTreshold", "double", "",0.5
			Property: "ForegroundTransparent", "bool", "",1
			Property: "DisplaySafeArea", "bool", "",0
			Property: "SafeAreaDisplayStyle", "enum", "",1
			Property: "SafeAreaAspectRatio", "double", "",1.33333333333333
			Property: "Use2DMagnifierZoom", "bool", "",0
			Property: "2D Magnifier Zoom", "Real", "A+",100
			Property: "2D Magnifier X", "Real", "A+",50
			Property: "2D Magnifier Y", "Real", "A+",50
			Property: "CameraProjectionType", "enum", "",1
			Property: "UseRealTimeDOFAndAA", "bool", "",0
			Property: "UseDepthOfField", "bool", "",0
			Property: "FocusSource", "enum", "",0
			Property: "FocusAngle", "double", "",3.5
			Property: "FocusDistance", "double", "",200
			Property: "UseAntialiasing", "bool", "",0
			Property: "AntialiasingIntensity", "double", "",0.77777
			Property: "UseAccumulationBuffer", "bool", "",0
			Property: "FrameSamplingCount", "int", "",7
		}
		MultiLayer: 0
		MultiTake: 0
		Hidden: "True"
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Camera"
		GeometryVersion: 124
		Position: 4000,0,0
		Up: 0,1,0
		LookAt: 0,0,0
		ShowInfoOnMoving: 1
		ShowAudio: 0
		AudioColor: 0,1,0
		CameraOrthoZoom: 1
	}
	Model: "Model::Producer Left", "Camera" {
		Version: 232
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",0
			Property: "Lcl Translation", "Lcl Translation", "A+",-4000,0,0
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "Show", "bool", "",0
			Property: "NegativePercentShapeSupport", "bool", "",1
			Property: "DefaultAttributeIndex", "int", "",0
			Property: "Color", "Color", "A",0.8,0.8,0.8
			Property: "Roll", "Roll", "A+",0
			Property: "FieldOfView", "FieldOfView", "A+",40
			Property: "FieldOfViewX", "FieldOfView", "A+",1
			Property: "FieldOfViewY", "FieldOfView", "A+",1
			Property: "OpticalCenterX", "Real", "A+",0
			Property: "OpticalCenterY", "Real", "A+",0
			Property: "BackgroundColor", "Color", "A+",0.63,0.63,0.63
			Property: "TurnTable", "Real", "A+",0
			Property: "DisplayTurnTableIcon", "bool", "",1
			Property: "Motion Blur Intensity", "Real", "A+",1
			Property: "UseMotionBlur", "bool", "",0
			Property: "UseRealTimeMotionBlur", "bool", "",1
			Property: "ResolutionMode", "enum", "",0
			Property: "ApertureMode", "enum", "",2
			Property: "GateFit", "enum", "",0
			Property: "FocalLength", "Real", "A+",21.3544940948486
			Property: "CameraFormat", "enum", "",0
			Property: "AspectW", "double", "",320
			Property: "AspectH", "double", "",200
			Property: "PixelAspectRatio", "double", "",1
			Property: "UseFrameColor", "bool", "",0
			Property: "FrameColor", "ColorRGB", "",0.3,0.3,0.3
			Property: "ShowName", "bool", "",1
			Property: "ShowGrid", "bool", "",1
			Property: "ShowOpticalCenter", "bool", "",0
			Property: "ShowAzimut", "bool", "",1
			Property: "ShowTimeCode", "bool", "",0
			Property: "NearPlane", "double", "",1
			Property: "FarPlane", "double", "",30000
			Property: "FilmWidth", "double", "",0.816
			Property: "FilmHeight", "double", "",0.612
			Property: "FilmAspectRatio", "double", "",1.33333333333333
			Property: "FilmSqueezeRatio", "double", "",1
			Property: "FilmFormatIndex", "enum", "",4
			Property: "ViewFrustum", "bool", "",1
			Property: "ViewFrustumNearFarPlane", "bool", "",0
			Property: "ViewFrustumBackPlaneMode", "enum", "",2
			Property: "BackPlaneDistance", "double", "",100
			Property: "BackPlaneDistanceMode", "enum", "",0
			Property: "ViewCameraToLookAt", "bool", "",1
			Property: "LockMode", "bool", "",0
			Property: "LockInterestNavigation", "bool", "",0
			Property: "FitImage", "bool", "",0
			Property: "Crop", "bool", "",0
			Property: "Center", "bool", "",1
			Property: "KeepRatio", "bool", "",1
			Property: "BackgroundMode", "enum", "",0
			Property: "BackgroundAlphaTreshold", "double", "",0.5
			Property: "ForegroundTransparent", "bool", "",1
			Property: "DisplaySafeArea", "bool", "",0
			Property: "SafeAreaDisplayStyle", "enum", "",1
			Property: "SafeAreaAspectRatio", "double", "",1.33333333333333
			Property: "Use2DMagnifierZoom", "bool", "",0
			Property: "2D Magnifier Zoom", "Real", "A+",100
			Property: "2D Magnifier X", "Real", "A+",50
			Property: "2D Magnifier Y", "Real", "A+",50
			Property: "CameraProjectionType", "enum", "",1
			Property: "UseRealTimeDOFAndAA", "bool", "",0
			Property: "UseDepthOfField", "bool", "",0
			Property: "FocusSource", "enum", "",0
			Property: "FocusAngle", "double", "",3.5
			Property: "FocusDistance", "double", "",200
			Property: "UseAntialiasing", "bool", "",0
			Property: "AntialiasingIntensity", "double", "",0.77777
			Property: "UseAccumulationBuffer", "bool", "",0
			Property: "FrameSamplingCount", "int", "",7
		}
		MultiLayer: 0
		MultiTake: 0
		Hidden: "True"
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Camera"
		GeometryVersion: 124
		Position: -4000,0,0
		Up: 0,1,0
		LookAt: 0,0,0
		ShowInfoOnMoving: 1
		ShowAudio: 0
		AudioColor: 0,1,0
		CameraOrthoZoom: 1
	}''')
	
	
	def write_object_props(ob):
		# if the type is 0 its an empty otherwise its a mesh
		# only difference at the moment is one has a color
		file.write(\
'''
		Properties60:  {
			Property: "QuaternionInterpolate", "bool", "",0
			Property: "Visibility", "Visibility", "A+",1
			Property: "Lcl Translation", "Lcl Translation", "A+",0,0,0
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
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
			Property: "RotationOrder", "enum", "",0
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
			Property: "DefaultAttributeIndex", "int", "",0
''')
		if ob:
			# Only mesh objects have color 
			file.write('\t\t\tProperty: "Color", "Color", "A",0.8,0.8,0.8\n')
		
		file.write('\t\t}\n')
	
	
	
	# Material Settings
	world = Blender.World.GetCurrent()
	if world:
		world_amb = world.getAmb()
	else:
		world_amb = (0,0,0) # Default value
	
	
	def write_material(matname, mat):
		file.write('\n	Material: "Material::%s", "" {' % matname)
		
		# Todo, add more material Properties.
		if mat:
			mat_cold = tuple(mat.rgbCol)
			mat_cols = tuple(mat.rgbCol)
			mat_amb = tuple([c for c in world_amb])
		else:
			mat_cols = mat_cold = 0.8, 0.8, 0.8
			mat_amb = 0.0,0.0,0.0
		
		file.write('''
		Version: 102
		ShadingModel: "phong"
		MultiLayer: 0
		Properties60:  {
			Property: "ShadingModel", "KString", "", "Phong"
			Property: "MultiLayer", "bool", "",0
			Property: "EmissiveColor", "ColorRGB", "",0,0,0
			Property: "EmissiveFactor", "double", "",1
''')
		file.write('\t\t\tProperty: "AmbientColor", "ColorRGB", "",%.1f,%.1f,%.1f\n' % mat_amb)
		file.write('\t\t\tProperty: "AmbientFactor", "double", "",1\n')
		file.write('\t\t\tProperty: "DiffuseColor", "ColorRGB", "",%.1f,%.1f,%.1f\n' % mat_cold)
		file.write('\t\t\tProperty: "DiffuseFactor", "double", "",1\n')
		file.write('\t\t\tProperty: "Bump", "Vector3D", "",0,0,0\n')
		file.write('\t\t\tProperty: "TransparentColor", "ColorRGB", "",1,1,1\n')
		file.write('\t\t\tProperty: "TransparencyFactor", "double", "",0\n')
		file.write('\t\t\tProperty: "SpecularColor", "ColorRGB", "",%.1f,%.1f,%.1f' % mat_cols)
		
		file.write('''
			Property: "SpecularFactor", "double", "",1
			Property: "ShininessExponent", "double", "",80.0
			Property: "ReflectionColor", "ColorRGB", "",0,0,0
			Property: "ReflectionFactor", "double", "",1
			Property: "Emissive", "Vector3D", "",0,0,0
			Property: "Ambient", "Vector3D", "",0,0,0
			Property: "Diffuse", "Vector3D", "",0,0.8,0
			Property: "Specular", "Vector3D", "",0.5,0.5,0.5
			Property: "Shininess", "double", "",80.0
			Property: "Opacity", "double", "",1
			Property: "Reflectivity", "double", "",0
		}
	}''')
	
	
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
	
	
	scn = Blender.Scene.GetCurrent()
	objects = []
	materials = {}
	textures = {}
	for ob in scn.objects.context:
		me = BPyMesh.getMeshFromObject(ob)
		if me:
			
			for mat in me.materials:
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
			
			me.transform(ob.matrixWorld)
			#### High Quality, not realy needed for now.
			#BPyMesh.meshCalcNormals(me) # high quality normals nice for realtime engines.
			objects.append( (sane_obname(ob.name), ob, me) )
	
	materials = [(sane_matname(mat.name), mat) for mat in materials.itervalues()]
	textures = [(sane_texname(img.name), img) for img in textures.itervalues()]
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
	file.write(\
'''
; Object definitions
;------------------------------------------------------------------

Definitions:  {
	Version: 100
	Count: %i''' % (1+1+camera_count+len(objects)+len(materials)+(len(textures)*2))) # add 1 for the root model 1 for global settings
	
	file.write('''
	ObjectType: "Model" {
		Count: %i
	}''' % (1+camera_count+len(objects))) # add 1 for the root model
	
	file.write('''
	ObjectType: "Geometry" {
		Count: %i
	}''' % len(objects))
	
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
	
	file.write('''
	ObjectType: "GlobalSettings" {
		Count: 1
	}
}
''')
	
	file.write(\
'''
; Object properties
;------------------------------------------------------------------

Objects:  {''')
	
	# To comply with other FBX FILES
	write_camera_switch()
	
	# Write the null object
	file.write('''
	Model: "Model::blend_root", "Null" {
		Version: 232''')
	write_object_props(None)
	file.write(\
'''		MultiLayer: 0
		MultiTake: 1
		Shading: Y
		Culling: "CullingOff"
		TypeFlags: "Null"
	}''')

	
	for obname, ob, me in objects:
		file.write('\n\tModel: "Model::%s", "Mesh" {\n' % sane_obname(ob.name))
		file.write('\t\tVersion: 232') # newline is added in write_object_props
		write_object_props(ob)
		
		file.write('\t\tMultiLayer: 0\n')
		file.write('\t\tMultiTake: 1\n')
		file.write('\t\tShading: Y\n')
		file.write('\t\tCulling: "CullingOff"')
		
		# Write the Real Mesh data here
		file.write('\n\t\tVertices: ')
		i=-1
		for v in me.verts:
			if i==-1:
				file.write('%.6f,%.6f,%.6f' % tuple(v.co))
				i=0
			else:
				if i==7:
					file.write('\n		')
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
					file.write('\n		')
					i=0
				if len(f) == 3:		file.write(',%i,%i,%i' % fi )
				else:				file.write(',%i,%i,%i,%i' % fi )
			i+=1
		
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
								file.write('\n			 ')
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
							file.write('\n			 ')
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
			for i, mat in enumerate(me.materials):
				if mat:
					material_mapping_local[i] = material_mapping[mat.name]
				else:
					material_mapping_local[i] = 0 # None material is zero for now.
			
			if not material_mapping_local:
				material_mapping_local[0] = 0
			
			len_material_mapping_local = len(material_mapping_local)
			
			i=-1
			for f in me.faces:
				if i==-1:
					i=0
					f_mat = f.mat
					if f_mat >= len_material_mapping_local:
						f_mat = 0
					
					file.write( '%s' % material_mapping_local[f_mat])
				else:
					if i==55:
						file.write('\n			 ')
						i=0
					
					file.write(',%s' % material_mapping_local[f_mat])
				i+=1
			
			file.write('\n		}')
		
		
		
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
			
			
		
	write_cameras()
	
	for matname, mat in materials:
		write_material(matname, mat)
	
	# each texture uses a video, odd
	for texname, tex in textures:
		write_video(texname, tex)
	i = 0
	for texname, tex in textures:
		write_texture(texname, tex, i)
		i+=1
	
	
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
	file.write('}\n\n')
	
	file.write(\
'''; Object relations
;------------------------------------------------------------------

Relations:  {
''')

	file.write('\tModel: "Model::blend_root", "Null" {\n\t}\n')
	
	for obname, ob, me in objects:
		file.write('\tModel: "Model::%s", "Mesh" {\n\t}\n' % obname)
	
	file.write('''	Model: "Model::Producer Perspective", "Camera" {
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
	}
''')
	
	for matname, mat in materials:
		file.write('\tMaterial: "Material::%s", "" {\n\t}\n' % matname)


	if textures:
		for texname, tex in textures:
			file.write('\tTexture: "Texture::%s", "TextureVideoClip" {\n\t}\n' % texname)
		for texname, tex in textures:
			file.write('\tVideo: "Video::%s", "Clip" {\n\t}\n' % texname)		

	file.write('}\n')
	file.write(\
'''
; Object connections
;------------------------------------------------------------------

Connections:  {
''')

	# write the fake root node
	file.write('\tConnect: "OO", "Model::blend_root", "Model::Scene"\n')
	
	for obname, ob, me in objects:
		file.write('\tConnect: "OO", "Model::%s", "Model::blend_root"\n' % obname)
	
	for obname, ob, me in objects:
		# Connect all materials to all objects, not good form but ok for now.
		for matname, mat in materials:
			file.write('	Connect: "OO", "Material::%s", "Model::%s"\n' % (matname, obname))
	
	if textures:
		for obname, ob, me in objects:
			for texname, tex in textures:
				file.write('\tConnect: "OO", "Texture::%s", "Model::%s"\n' % (texname, obname))
		
		for texname, tex in textures:
			file.write('\tConnect: "OO", "Video::%s", "Texture::%s"\n' % (texname, texname))
	
	file.write('}\n')
	
	
	# Clear mesh data
	for obname, ob, me in objects:
		me.verts = None


def write_footer(file):
	file.write(\
''';Takes and animation section
;----------------------------------------------------

Takes:  {
	Current: ""
}
;Version 5 settings
;------------------------------------------------------------------

Version5:  {
	AmbientRenderSettings:  {
		Version: 101
		AmbientLightColor: 0.4,0.4,0.4,0
	}
	FogOptions:  {
		FlogEnable: 0
		FogMode: 0
		FogDensity: 0.002
		FogStart: 0.3
		FogEnd: 1000
		FogColor: 1,1,1,1
	}
	Settings:  {
		FrameRate: "30"
		TimeFormat: 1
		SnapOnFrames: 0
		ReferenceTimeIndex: -1
		TimeLineStartTime: 0
		TimeLineStopTime: 46186158000
	}
	RendererSetting:  {
		DefaultCamera: "Producer Perspective"
		DefaultViewingMode: 0
	}
}
''')


def write_ui(filename):
	if not BPyMessages.Warning_SaveOver(filename):
		return
	
	Blender.Window.WaitCursor(1)
	file = open(filename, 'w')
	write_header(file)
	write_scene(file)
	write_footer(file)
	Blender.Window.WaitCursor(0)

if __name__ == '__main__':
	Blender.Window.FileSelector(write_ui, 'Export FBX', Blender.sys.makename(ext='.fbx'))
