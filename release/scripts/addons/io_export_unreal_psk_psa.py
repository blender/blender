# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#  All rights reserved.
#
# ##### END GPL LICENSE BLOCK #####

bl_info = {
    "name": "Export Unreal Engine Format(.psk/.psa)",
    "author": "Darknet/Optimus_P-Fat/Active_Trash/Sinsoft/VendorX/Spoof",
    "version": (2, 7, 1),
    "blender": (2, 65, 4),
    "location": "File > Export > Skeletal Mesh/Animation Data (.psk/.psa)",
    "description": "Export Skeleletal Mesh/Animation Data",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/Unreal_psk_psa",
    "category": "Import-Export",
}

"""
-- Unreal Skeletal Mesh and Animation Export (.psk  and .psa) export script v0.0.1 --<br>

- NOTES:
- This script Exports To Unreal's PSK and PSA file formats for Skeletal Meshes and Animations. <br>
- This script DOES NOT support vertex animation! These require completely different file formats. <br>

- v0.0.1
- Initial version

- v0.0.2
- This version adds support for more than one material index!

[ - Edit by: Darknet
- v0.0.3 - v0.0.12
- This will work on UT3 and it is a stable version that work with vehicle for testing.
- Main Bone fix no dummy needed to be there.
- Just bone issues position, rotation, and offset for psk.
- The armature bone position, rotation, and the offset of the bone is fix.
  It was to deal with skeleton mesh export for psk.
- Animation is fix for position, offset, rotation bone support one rotation direction when armature build.
- It will convert your mesh into triangular when exporting to psk file.
- Did not work with psa export yet.

- v0.0.13
- The animatoin will support different bone rotations when export the animation.

- v0.0.14
- Fixed Action set keys frames when there is no pose keys and it will ignore it.

- v0.0.15
- Fixed multiple objects when exporting to psk. Select one mesh to export to psk.
- ]

- v0.1.1
- Blender 2.50 svn (Support)

Credit to:
- export_cal3d.py (Position of the Bones Format)
- blender2md5.py (Animation Translation Format)
- export_obj.py (Blender 2.5/Pyhton 3.x Format)

- freenode #blendercoder -> user -> ideasman42

- Give Credit to those who work on this script.

- http://sinsoft.com
"""


# ===========================================================================
"""
NOTES for Jan 2012 refactor (Spoof)

    * THIS IS A WORK IN PROGRESS. These modifications were originally
    intended for internal use and are incomplete. Use at your own risk! *

TODO

- (Blender 2.62) changes to Matrix math
- (Blender 2.62) check for long names
- option to manually set the root bone for export

CHANGES

- new bone parsing to allow advanced rigging
- identification of armature and mesh
- removed the need to apply an action to the armature
- fixed anim rate to work correctly in UDK (no more FPS fudging)
- progress reporting while processing smooth groups
- more informative logging
- code refactor for clarity and modularity
    - naming conventions unified to use lowercase_with_underscore
    - C++ datatypes and PSK/PSA classes remain CamelCaseStyle for clarity
    - names such as 'ut' and 'unreal' unified to 'udk'
    - simplification of code structure
    - removed legacy code paths

USAGE

This version of the exporter is more selective over which bones are considered
part of the UDK skeletal mesh, and allows greater flexibility for adding
control bones to aid in animation.

Taking advantage of this script requires the following methodology:

    * Place all exportable bones into a bone hierarchy extending from a single
    root. This root bone must have use_deform enabled. All other root bones
    in the armature must disable use_deform. *

The script searches for a root bone with use_deform set true and considers all
bones parented to it as part of the UDK skeletal mesh. Thus only these bones
are exported and all other bones are ignored.

This removes many restrictions on the rigger/animator, who can add control
bone hierarchies to the rig, and keyframe any element into actions. With this
approach you can build complex animation rigs in a similar vein to the Rigify
add-on, by Nathan Vegdahl. However...

    * Rigify is incompatible with this script *

Rigify interlaces deformer bones within a single hierarchy making it difficult
to deconstruct for export. It also splits some meta-rig bones into multiple
deformer bones (bad for optimising a game character). I had partial success
writing a parser for the structure, but it was taking too much time and,
considering the other issues with Rigify, it was abandoned.
"""
# ===========================================================================

import bmesh
import os
import time
import bpy
import mathutils
import math
import operator
import sys
from bpy.props import (
        BoolProperty,
        CollectionProperty,
        EnumProperty,
        FloatProperty,
        IntProperty,
        StringProperty,
        )
from bpy.types import (
        Operator,
        Panel,
        UIList,
        PropertyGroup,
        AddonPreferences,
        )
from struct import pack


# REFERENCE MATERIAL JUST IN CASE:
#
# U = x / sqrt(x^2 + y^2 + z^2)
# V = y / sqrt(x^2 + y^2 + z^2)
#
# Triangles specifed counter clockwise for front face
#
# defines for sizeofs
SIZE_FQUAT = 16
SIZE_FVECTOR = 12
SIZE_VJOINTPOS = 44
SIZE_ANIMINFOBINARY = 168
SIZE_VCHUNKHEADER = 32
SIZE_VMATERIAL = 88
SIZE_VBONE = 120
SIZE_FNAMEDBONEBINARY = 120
SIZE_VRAWBONEINFLUENCE = 12
SIZE_VQUATANIMKEY = 32
SIZE_VVERTEX = 16
SIZE_VPOINT = 12
SIZE_VTRIANGLE = 12

MaterialName = []


# Python 3 doesn't have cmp anymore
def cmp(a, b):
    return (a > b) - (a < b)


# ===========================================================================
# Custom exception class
# ===========================================================================
class Error(Exception):

    def __init__(self, message):
        self.message = message


# ===========================================================================
# Verbose logging with loop truncation
# ===========================================================================
def verbose(msg, iteration=-1, max_iterations=4, msg_truncated="..."):

    if bpy.context.scene.udk_option_verbose is True:
        # limit the number of times a loop can output messages
        if iteration > max_iterations:
            return
        elif iteration == max_iterations:
            print(msg_truncated)
            return

        print(msg)


# ===========================================================================
# Log header/separator
# ===========================================================================
def header(msg, justify='LEFT', spacer='_', cols=78):

    if justify == 'LEFT':
        s = '{:{spacer}<{cols}}'.format(msg + " ", spacer=spacer, cols=cols)

    elif justify == 'RIGHT':
        s = '{:{spacer}>{cols}}'.format(" " + msg, spacer=spacer, cols=cols)

    else:
        s = '{:{spacer}^{cols}}'.format(" " + msg + " ", spacer=spacer, cols=cols)

    return "\n" + s + "\n"


# ===========================================================================
# Generic Object->Integer mapping
# the object must be usable as a dictionary key
# ===========================================================================
class ObjMap:

    def __init__(self):
        self.dict = {}
        self.next = 0

    def get(self, obj):
        if obj in self.dict:
            return self.dict[obj]
        else:
            id = self.next
            self.next = self.next + 1
            self.dict[obj] = id
            return id

    def items(self):
        getval = operator.itemgetter(0)
        getkey = operator.itemgetter(1)
        return map(getval, sorted(self.dict.items(), key=getkey))


# ===========================================================================
# RG - UNREAL DATA STRUCTS - CONVERTED FROM C STRUCTS GIVEN ON UDN SITE
# provided here: http://udn.epicgames.com/Two/BinaryFormatSpecifications.html
# updated UDK (Unreal Engine 3): http://udn.epicgames.com/Three/BinaryFormatSpecifications.html
# ===========================================================================
class FQuat:

    def __init__(self):
        self.X = 0.0
        self.Y = 0.0
        self.Z = 0.0
        self.W = 1.0

    def dump(self):
        return pack('ffff', self.X, self.Y, self.Z, self.W)

    def __cmp__(self, other):
        return cmp(self.X, other.X) or \
               cmp(self.Y, other.Y) or \
               cmp(self.Z, other.Z) or \
               cmp(self.W, other.W)

    def __hash__(self):
        return hash(self.X) ^ hash(self.Y) ^ hash(self.Z) ^ hash(self.W)

    def __str__(self):
        return "[%f,%f,%f,%f](FQuat)" % (self.X, self.Y, self.Z, self.W)


class FVector(object):

    def __init__(self, X=0.0, Y=0.0, Z=0.0):
        self.X = X
        self.Y = Y
        self.Z = Z

    def dump(self):
        return pack('fff', self.X, self.Y, self.Z)

    def __cmp__(self, other):
        return cmp(self.X, other.X) or \
               cmp(self.Y, other.Y) or \
               cmp(self.Z, other.Z)

    def _key(self):
        return (type(self).__name__, self.X, self.Y, self.Z)

    def __hash__(self):
        return hash(self._key())

    def __eq__(self, other):
        if not hasattr(other, '_key'):
            return False
        return self._key() == other._key()

    def dot(self, other):
        return self.X * other.X + self.Y * other.Y + self.Z * other.Z

    def cross(self, other):
        return FVector(self.Y * other.Z - self.Z * other.Y,
                self.Z * other.X - self.X * other.Z,
                self.X * other.Y - self.Y * other.X)

    def sub(self, other):
        return FVector(self.X - other.X,
            self.Y - other.Y,
            self.Z - other.Z)


class VJointPos:

    def __init__(self):
        self.Orientation = FQuat()
        self.Position = FVector()
        self.Length = 0.0
        self.XSize = 0.0
        self.YSize = 0.0
        self.ZSize = 0.0

    def dump(self):
        return self.Orientation.dump() + self.Position.dump() + \
               pack('4f', self.Length, self.XSize, self.YSize, self.ZSize)


class AnimInfoBinary:

    def __init__(self):
        self.Name = ""      # length=64
        self.Group = ""     # length=64
        self.TotalBones = 0
        self.RootInclude = 0
        self.KeyCompressionStyle = 0
        self.KeyQuotum = 0
        self.KeyPrediction = 0.0
        self.TrackTime = 0.0
        self.AnimRate = 0.0
        self.StartBone = 0
        self.FirstRawFrame = 0
        self.NumRawFrames = 0

    def dump(self):
        return pack('64s64siiiifffiii', str.encode(self.Name), str.encode(self.Group),
                    self.TotalBones, self.RootInclude, self.KeyCompressionStyle, self.KeyQuotum,
                    self.KeyPrediction, self.TrackTime, self.AnimRate, self.StartBone,
                    self.FirstRawFrame, self.NumRawFrames)


class VChunkHeader:

    def __init__(self, name, type_size):
        self.ChunkID = str.encode(name)  # length=20
        self.TypeFlag = 1999801           # special value
        self.DataSize = type_size
        self.DataCount = 0

    def dump(self):
        return pack('20siii', self.ChunkID, self.TypeFlag, self.DataSize, self.DataCount)


class VMaterial:

    def __init__(self):
        self.MaterialName = ""    # length=64
        self.TextureIndex = 0
        self.PolyFlags = 0        # DWORD
        self.AuxMaterial = 0
        self.AuxFlags = 0         # DWORD
        self.LodBias = 0
        self.LodStyle = 0

    def dump(self):
        # print("DATA MATERIAL:",self.MaterialName)
        return pack('64siLiLii', str.encode(self.MaterialName), self.TextureIndex,
                    self.PolyFlags, self.AuxMaterial, self.AuxFlags, self.LodBias, self.LodStyle)


class VBone:

    def __init__(self):
        self.Name = ""    # length = 64
        self.Flags = 0     # DWORD
        self.NumChildren = 0
        self.ParentIndex = 0
        self.BonePos = VJointPos()

    def dump(self):
        return pack('64sLii', str.encode(self.Name), self.Flags,
                    self.NumChildren, self.ParentIndex) + self.BonePos.dump()


# same as above - whatever - this is how Epic does it...
class FNamedBoneBinary:

    def __init__(self):
        self.Name = ""    # length = 64
        self.Flags = 0    # DWORD
        self.NumChildren = 0
        self.ParentIndex = 0
        self.BonePos = VJointPos()
        self.IsRealBone = 0  # this is set to 1 when the bone is actually a bone in the mesh and not a dummy

    def dump(self):
        return pack('64sLii', str.encode(self.Name), self.Flags,
                    self.NumChildren, self.ParentIndex) + self.BonePos.dump()


class VRawBoneInfluence:

    def __init__(self):
        self.Weight = 0.0
        self.PointIndex = 0
        self.BoneIndex = 0

    def dump(self):
        return pack('fii', self.Weight, self.PointIndex, self.BoneIndex)


class VQuatAnimKey:

    def __init__(self):
        self.Position = FVector()
        self.Orientation = FQuat()
        self.Time = 0.0

    def dump(self):
        return self.Position.dump() + self.Orientation.dump() + pack('f', self.Time)


class VVertex(object):

    def __init__(self):
        self.PointIndex = 0     # WORD
        self.U = 0.0
        self.V = 0.0
        self.MatIndex = 0       # BYTE
        self.Reserved = 0       # BYTE
        self.SmoothGroup = 0

    def dump(self):
        return pack('HHffBBH', self.PointIndex, 0, self.U, self.V, self.MatIndex, self.Reserved, 0)

    def __cmp__(self, other):
        return cmp(self.PointIndex, other.PointIndex) or\
               cmp(self.U, other.U) or \
               cmp(self.V, other.V) or \
               cmp(self.MatIndex, other.MatIndex) or \
               cmp(self.Reserved, other.Reserved) or \
               cmp(self.SmoothGroup, other.SmoothGroup)

    def _key(self):
        return (type(self).__name__, self.PointIndex, self.U, self.V, self.MatIndex, self.Reserved)

    def __hash__(self):
        return hash(self._key())

    def __eq__(self, other):
        if not hasattr(other, '_key'):
            return False
        return self._key() == other._key()


class VPointSimple:

    def __init__(self):
        self.Point = FVector()

    def __cmp__(self, other):
        return cmp(self.Point, other.Point)

    def __hash__(self):
        return hash(self._key())

    def _key(self):
        return (type(self).__name__, self.Point)

    def __eq__(self, other):
        if not hasattr(other, '_key'):
            return False
        return self._key() == other._key()


class VPoint(object):

    def __init__(self):
        self.Point = FVector()
        self.SmoothGroup = 0

    def dump(self):
        return self.Point.dump()

    def __cmp__(self, other):
        return cmp(self.Point, other.Point) \
            or cmp(self.SmoothGroup, other.SmoothGroup)

    def _key(self):
        return (type(self).__name__, self.Point, self.SmoothGroup)

    def __hash__(self):
        return hash(self._key()) \
            ^ hash(self.SmoothGroup)

    def __eq__(self, other):
        if not hasattr(other, '_key'):
            return False
        return self._key() == other._key()


class VTriangle:

    def __init__(self):
        self.WedgeIndex0 = 0        # WORD
        self.WedgeIndex1 = 0        # WORD
        self.WedgeIndex2 = 0        # WORD
        self.MatIndex = 0           # BYTE
        self.AuxMatIndex = 0        # BYTE
        self.SmoothingGroups = 0    # DWORD

    def dump(self):
        return pack('HHHBBL', self.WedgeIndex0, self.WedgeIndex1, self.WedgeIndex2,
                    self.MatIndex, self.AuxMatIndex, self.SmoothingGroups)
        """
        print("smooth",self.SmoothingGroups)
        return pack('HHHBBI', self.WedgeIndex0, self.WedgeIndex1, self.WedgeIndex2,
                     self.MatIndex, self.AuxMatIndex, self.SmoothingGroups)
        """
# END UNREAL DATA STRUCTS
# ===========================================================================


# ===========================================================================
# RG - helper class to handle the normal way the UT files are stored
# as sections consisting of a header and then a list of data structures
# ===========================================================================
class FileSection:

    def __init__(self, name, type_size):
        self.Header = VChunkHeader(name, type_size)
        self.Data = []  # list of datatypes

    def dump(self):
        data = self.Header.dump()
        for i in range(len(self.Data)):
            data = data + self.Data[i].dump()
        return data

    def UpdateHeader(self):
        self.Header.DataCount = len(self.Data)


# ===========================================================================
# PSK
# ===========================================================================
class PSKFile:

    def __init__(self):
        self.GeneralHeader = VChunkHeader("ACTRHEAD", 0)
        self.Points = FileSection("PNTS0000", SIZE_VPOINT)              # VPoint
        self.Wedges = FileSection("VTXW0000", SIZE_VVERTEX)             # VVertex
        self.Faces = FileSection("FACE0000", SIZE_VTRIANGLE)            # VTriangle
        self.Materials = FileSection("MATT0000", SIZE_VMATERIAL)        # VMaterial
        self.Bones = FileSection("REFSKELT", SIZE_VBONE)                # VBone
        self.Influences = FileSection("RAWWEIGHTS", SIZE_VRAWBONEINFLUENCE)  # VRawBoneInfluence

        # RG - this mapping is not dumped, but is used internally to store the new point indices
        # for vertex groups calculated during the mesh dump, so they can be used again
        # to dump bone influences during the armature dump
        #
        # the key in this dictionary is the VertexGroup/Bone Name, and the value
        # is a list of tuples containing the new point index and the weight, in that order
        #
        # Layout:
        # { groupname : [ (index, weight), ... ], ... }
        #
        # example:
        # {'MyVertexGroup' : [ (0, 1.0), (5, 1.0), (3, 0.5) ] , 'OtherGroup' : [(2, 1.0)]}

        self.VertexGroups = {}

    def AddPoint(self, p):
        self.Points.Data.append(p)

    def AddWedge(self, w):
        self.Wedges.Data.append(w)

    def AddFace(self, f):
        self.Faces.Data.append(f)

    def AddMaterial(self, m):
        self.Materials.Data.append(m)

    def AddBone(self, b):
        self.Bones.Data.append(b)

    def AddInfluence(self, i):
        self.Influences.Data.append(i)

    def UpdateHeaders(self):
        self.Points.UpdateHeader()
        self.Wedges.UpdateHeader()
        self.Faces.UpdateHeader()
        self.Materials.UpdateHeader()
        self.Bones.UpdateHeader()
        self.Influences.UpdateHeader()

    def dump(self):
        self.UpdateHeaders()
        data = self.GeneralHeader.dump() + self.Points.dump() + self.Wedges.dump() + \
               self.Faces.dump() + self.Materials.dump() + self.Bones.dump() + self.Influences.dump()
        return data

    def GetMatByIndex(self, mat_index):
        if mat_index >= 0 and len(self.Materials.Data) > mat_index:
            return self.Materials.Data[mat_index]
        else:
            m = VMaterial()
            # modified by VendorX
            m.MaterialName = MaterialName[mat_index]
            self.AddMaterial(m)
            return m

    def PrintOut(self):
        print("{:>16} {:}".format("Points", len(self.Points.Data)))
        print("{:>16} {:}".format("Wedges", len(self.Wedges.Data)))
        print("{:>16} {:}".format("Faces", len(self.Faces.Data)))
        print("{:>16} {:}".format("Materials", len(self.Materials.Data)))
        print("{:>16} {:}".format("Bones", len(self.Bones.Data)))
        print("{:>16} {:}".format("Influences", len(self.Influences.Data)))


# ===========================================================================
# PSA
#
# Notes from UDN:
#   The raw key array holds all the keys for all the bones in all the specified sequences,
#   organized as follows:
#   For each AnimInfoBinary's sequence there are [Number of bones] times [Number of frames keys]
#   in the VQuatAnimKeys, laid out as tracks of [numframes] keys for each bone in the order of
#   the bones as defined in the array of FnamedBoneBinary in the PSA.
#
#   Once the data from the PSK (now digested into native skeletal mesh) and PSA (digested into
#   a native animation object containing one or more sequences) are associated together at runtime,
#   bones are linked up by name. Any bone in a skeleton (from the PSK) that finds no partner in
#   the animation sequence (from the PSA) will assume its reference pose stance ( as defined in
#   the offsets & rotations that are in the VBones making up the reference skeleton from the PSK)
# ===========================================================================
class PSAFile:

    def __init__(self):
        self.GeneralHeader = VChunkHeader("ANIMHEAD", 0)
        self.Bones = FileSection("BONENAMES", SIZE_FNAMEDBONEBINARY)        # FNamedBoneBinary
        self.Animations = FileSection("ANIMINFO", SIZE_ANIMINFOBINARY)      # AnimInfoBinary
        self.RawKeys = FileSection("ANIMKEYS", SIZE_VQUATANIMKEY)           # VQuatAnimKey
        # this will take the format of key=Bone Name, value = (BoneIndex, Bone Object)
        # THIS IS NOT DUMPED
        self.BoneLookup = {}

    def AddBone(self, b):
        self.Bones.Data.append(b)

    def AddAnimation(self, a):
        self.Animations.Data.append(a)

    def AddRawKey(self, k):
        self.RawKeys.Data.append(k)

    def UpdateHeaders(self):
        self.Bones.UpdateHeader()
        self.Animations.UpdateHeader()
        self.RawKeys.UpdateHeader()

    def GetBoneByIndex(self, bone_index):
        if bone_index >= 0 and len(self.Bones.Data) > bone_index:
            return self.Bones.Data[bone_index]

    def IsEmpty(self):
        return (len(self.Bones.Data) == 0 or len(self.Animations.Data) == 0)

    def StoreBone(self, b):
        self.BoneLookup[b.Name] = [-1, b]

    def UseBone(self, bone_name):
        if bone_name in self.BoneLookup:
            bone_data = self.BoneLookup[bone_name]

            if bone_data[0] == -1:
                bone_data[0] = len(self.Bones.Data)
                self.AddBone(bone_data[1])
                # self.Bones.Data.append(bone_data[1])

            return bone_data[0]

    def GetBoneByName(self, bone_name):
        if bone_name in self.BoneLookup:
            bone_data = self.BoneLookup[bone_name]
            return bone_data[1]

    def GetBoneIndex(self, bone_name):
        if bone_name in self.BoneLookup:
            bone_data = self.BoneLookup[bone_name]
            return bone_data[0]

    def dump(self):
        self.UpdateHeaders()
        return self.GeneralHeader.dump() + self.Bones.dump() + self.Animations.dump() + self.RawKeys.dump()

    def PrintOut(self):
        print("{:>16} {:}".format("Bones", len(self.Bones.Data)))
        print("{:>16} {:}".format("Animations", len(self.Animations.Data)))
        print("{:>16} {:}".format("Raw keys", len(self.RawKeys.Data)))


# ===========================================================================
#  Helpers to create bone structs
# ===========================================================================
def make_vbone(name, parent_index, child_count, orientation_quat, position_vect):
    bone = VBone()
    bone.Name = name
    bone.ParentIndex = parent_index
    bone.NumChildren = child_count
    bone.BonePos.Orientation = orientation_quat
    bone.BonePos.Position.X = position_vect.x
    bone.BonePos.Position.Y = position_vect.y
    bone.BonePos.Position.Z = position_vect.z
    # these values seem to be ignored?
    # bone.BonePos.Length = tail.length
    # bone.BonePos.XSize = tail.x
    # bone.BonePos.YSize = tail.y
    # bone.BonePos.ZSize = tail.z
    return bone


def make_namedbonebinary(name, parent_index, child_count, orientation_quat, position_vect, is_real):
    bone = FNamedBoneBinary()
    bone.Name = name
    bone.ParentIndex = parent_index
    bone.NumChildren = child_count
    bone.BonePos.Orientation = orientation_quat
    bone.BonePos.Position.X = position_vect.x
    bone.BonePos.Position.Y = position_vect.y
    bone.BonePos.Position.Z = position_vect.z
    bone.IsRealBone = is_real
    return bone


def make_fquat(bquat):
    quat = FQuat()
    # flip handedness for UT = set x,y,z to negative (rotate in other direction)
    quat.X = -bquat.x
    quat.Y = -bquat.y
    quat.Z = -bquat.z
    quat.W = bquat.w

    return quat


def make_fquat_default(bquat):
    quat = FQuat()
    # print(dir(bquat))
    quat.X = bquat.x
    quat.Y = bquat.y
    quat.Z = bquat.z
    quat.W = bquat.w

    return quat


# ===========================================================================
# RG - check to make sure face isnt a line
# ===========================================================================
def is_1d_face(face, mesh):
    # ID Vertex of id point
    v0 = face.vertices[0]
    v1 = face.vertices[1]
    v2 = face.vertices[2]

    return (mesh.vertices[v0].co == mesh.vertices[v1].co or
            mesh.vertices[v1].co == mesh.vertices[v2].co or
            mesh.vertices[v2].co == mesh.vertices[v0].co)
    return False


# ===========================================================================
# Smoothing group
# (renamed to seperate it from VVertex.SmoothGroup)
# ===========================================================================
class SmoothingGroup:

    static_id = 1

    def __init__(self):
        self.faces = []
        self.neighboring_faces = []
        self.neighboring_groups = []
        self.id = -1
        self.local_id = SmoothingGroup.static_id
        SmoothingGroup.static_id += 1

    def __cmp__(self, other):
        if isinstance(other, SmoothingGroup):
            return cmp(self.local_id, other.local_id)
        return -1

    def __hash__(self):
        return hash(self.local_id)

    # searches neighboring faces to determine which smoothing group ID can be used
    def get_valid_smoothgroup_id(self):
        temp_id = 1
        for group in self.neighboring_groups:
            if group is not None and group.id == temp_id:
                if temp_id < 0x80000000:
                    temp_id = temp_id << 1
                else:
                    raise Error("Smoothing Group ID Overflowed, "
                                "Smoothing Group evidently has more than 31 neighboring groups")

        self.id = temp_id
        return self.id

    def make_neighbor(self, new_neighbor):
        if new_neighbor not in self.neighboring_groups:
            self.neighboring_groups.append(new_neighbor)

    def contains_face(self, face):
        return (face in self.faces)

    def add_neighbor_face(self, face):
        if face not in self.neighboring_faces:
            self.neighboring_faces.append(face)

    def add_face(self, face):
        if face not in self.faces:
            self.faces.append(face)


def determine_edge_sharing(mesh):

    edge_sharing_list = dict()

    for edge in mesh.edges:
        edge_sharing_list[edge.key] = []

    for face in mesh.tessfaces:
        for key in face.edge_keys:
            if face not in edge_sharing_list[key]:
                edge_sharing_list[key].append(face)  # mark this face as sharing this edge

    return edge_sharing_list


def find_edges(mesh, key):
    """ Temp replacement for mesh.findEdges().
        This is painfully slow.
    """
    for edge in mesh.edges:
        v = edge.vertices
        if key[0] == v[0] and key[1] == v[1]:
            return edge.index


def add_face_to_smoothgroup(mesh, face, edge_sharing_list, smoothgroup):

    if face in smoothgroup.faces:
        return

    smoothgroup.add_face(face)

    for key in face.edge_keys:

        edge_id = find_edges(mesh, key)

        if edge_id is not None:

            # not sharp
            if not (mesh.edges[edge_id].use_edge_sharp):

                for shared_face in edge_sharing_list[key]:
                    if shared_face != face:
                        # recursive
                        add_face_to_smoothgroup(mesh, shared_face, edge_sharing_list, smoothgroup)
            # sharp
            else:
                for shared_face in edge_sharing_list[key]:
                    if shared_face != face:
                        smoothgroup.add_neighbor_face(shared_face)


def determine_smoothgroup_for_face(mesh, face, edge_sharing_list, smoothgroup_list):

    for group in smoothgroup_list:
        if (face in group.faces):
            return

    smoothgroup = SmoothingGroup()
    add_face_to_smoothgroup(mesh, face, edge_sharing_list, smoothgroup)

    if smoothgroup not in smoothgroup_list:
        smoothgroup_list.append(smoothgroup)


def build_neighbors_tree(smoothgroup_list):

    for group in smoothgroup_list:
        for face in group.neighboring_faces:
            for neighbor_group in smoothgroup_list:
                if neighbor_group.contains_face(face) and neighbor_group not in group.neighboring_groups:
                    group.make_neighbor(neighbor_group)
                    neighbor_group.make_neighbor(group)


# ===========================================================================
# parse_smooth_groups
# ===========================================================================
def parse_smooth_groups(mesh):

    print("Parsing smooth groups...")

    t = time.clock()
    smoothgroup_list = []
    edge_sharing_list = determine_edge_sharing(mesh)

    # print("faces:",len(mesh.tessfaces))
    interval = math.floor(len(mesh.tessfaces) / 100)

    if interval == 0:  # if the faces are few do this
        interval = math.floor(len(mesh.tessfaces) / 10)
    # print("FACES:",len(mesh.tessfaces),"//100 =" "interval:",interval)

    for face in mesh.tessfaces:
        # print(dir(face))
        determine_smoothgroup_for_face(mesh, face, edge_sharing_list, smoothgroup_list)
        # progress indicator, writes to console without scrolling
        if face.index > 0 and (face.index % interval) == 0:
            print("Processing... {}%\r".format(int(face.index / len(mesh.tessfaces) * 100)), end='')
            sys.stdout.flush()
    print("Completed", ' ' * 20)

    verbose("len(smoothgroup_list)={}".format(len(smoothgroup_list)))

    build_neighbors_tree(smoothgroup_list)

    for group in smoothgroup_list:
        group.get_valid_smoothgroup_id()

    print("Smooth group parsing completed in {:.2f}s".format(time.clock() - t))
    return smoothgroup_list


# ===========================================================================
# http://en.wikibooks.org/wiki/Blender_3D:_Blending_Into_Python/Cookbook#Triangulate_NMesh
# blender 2.50 format using the Operators/command convert the mesh to tri mesh
# ===========================================================================
def triangulate_mesh(object):

    verbose(header("triangulateNMesh"))
    # print(type(object))
    scene = bpy.context.scene

    me_ob = object.copy()
    me_ob.data = object.to_mesh(bpy.context.scene, True, 'PREVIEW')  # write data object
    bpy.context.scene.objects.link(me_ob)
    bpy.context.scene.update()
    bpy.ops.object.mode_set(mode='OBJECT')

    for i in scene.objects:
        i.select = False  # deselect all objects

    me_ob.select = True
    scene.objects.active = me_ob

    print("Copy and Convert mesh just incase any way...")

    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')  # select all the face/vertex/edge
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.quads_convert_to_tris()
    bpy.context.scene.update()

    bpy.ops.object.mode_set(mode='OBJECT')

    bpy.context.scene.udk_option_triangulate = True

    verbose("Triangulated mesh")

    me_ob.data = me_ob.to_mesh(bpy.context.scene, True, 'PREVIEW')  # write data object
    bpy.context.scene.update()
    return me_ob


# copy mesh data and then merge them into one object
def meshmerge(selectedobjects):
    bpy.ops.object.mode_set(mode='OBJECT')  # object mode and not edit mode
    cloneobjects = []  # object holder for copying object data

    if len(selectedobjects) > 1:
        print("selectedobjects:", len(selectedobjects))  # print select object
        count = 0  # reset count

        for count in range(len(selectedobjects)):
            # print("Index:",count)
            if selectedobjects[count] is not None:
                me_da = selectedobjects[count].data.copy()  # copy data
                me_ob = selectedobjects[count].copy()  # copy object
                # note two copy two types else it will use the current data or mesh
                me_ob.data = me_da  # assign the data
                bpy.context.scene.objects.link(me_ob)  # link the object to the scene #current object location
                print("Index:", count, "clone object", me_ob.name)  # print clone object
                cloneobjects.append(me_ob)  # add object to the array

        for i in bpy.data.objects:
            i.select = False  # deselect all objects
        count = 0  # reset count
        # begin merging the mesh together as one
        for count in range(len(cloneobjects)):
            if count == 0:
                bpy.context.scene.objects.active = cloneobjects[count]
                print("Set Active Object:", cloneobjects[count].name)
            cloneobjects[count].select = True
        bpy.ops.object.join()  # join object together
        if len(cloneobjects) > 1:
            bpy.types.Scene.udk_copy_merge = True
    return cloneobjects[0]


# sort the mesh center top list and not center at the last array.
# Base on order while select to merge mesh to make them center.
def sortmesh(selectmesh):
    print("MESH SORTING...")
    centermesh = []
    notcentermesh = []
    for countm in range(len(selectmesh)):
        # if object are center add here
        if selectmesh[countm].location.x == 0 and \
                    selectmesh[countm].location.y == 0 and \
                    selectmesh[countm].location.z == 0:
            centermesh.append(selectmesh[countm])
        else:  # if not add here for not center
            notcentermesh.append(selectmesh[countm])
    selectmesh = []
    # add mesh object in order for merge object
    for countm in range(len(centermesh)):
        selectmesh.append(centermesh[countm])
    for countm in range(len(notcentermesh)):
        selectmesh.append(notcentermesh[countm])
    if len(selectmesh) == 1:  # if there one mesh just do some here
        return selectmesh[0]  # return object mesh
    else:
        return meshmerge(selectmesh)  # return merge object mesh


import binascii


# ===========================================================================
# parse_mesh
# ===========================================================================
def parse_mesh(mesh, psk):
    # bpy.ops.object.mode_set(mode='OBJECT')
    # error ? on commands for select object?
    print(header("MESH", 'RIGHT'))
    print("Mesh object:", mesh.name)
    scene = bpy.context.scene

    for i in scene.objects:
        i.select = False  # deselect all objects

    scene.objects.active = mesh
    setmesh = mesh
    mesh = triangulate_mesh(mesh)

    if bpy.types.Scene.udk_copy_merge is True:
        bpy.context.scene.objects.unlink(setmesh)

    # print("FACES----:",len(mesh.data.tessfaces))
    verbose("Working mesh object: {}".format(mesh.name))

    # collect a list of the material names
    print("Materials...")

    mat_slot_index = 0

    for slot in mesh.material_slots:

        print("  Material {} '{}'".format(mat_slot_index, slot.name))
        MaterialName.append(slot.name)

        """
        if slot.material.texture_slots[0] is not None:
            if slot.material.texture_slots[0].texture.image.filepath is not None:
                print("    Texture path {}".format(slot.material.texture_slots[0].texture.image.filepath))
        """

        # create the current material
        v_material = psk.GetMatByIndex(mat_slot_index)
        v_material.MaterialName = slot.name
        v_material.TextureIndex = mat_slot_index
        v_material.AuxMaterial = mat_slot_index
        mat_slot_index += 1
        verbose("    PSK index {}".format(v_material.TextureIndex))

    # END slot in mesh.material_slots

    # object_mat = mesh.materials[0]
    # object_material_index = mesh.active_material_index
    # FIXME ^ this is redundant due to "= face.material_index" in face loop

    wedges = ObjMap()
    points = ObjMap()  # vertex
    points_linked = {}

    discarded_face_count = 0
    sys.setrecursionlimit(1000000)
    smoothgroup_list = parse_smooth_groups(mesh.data)

    print("{} faces".format(len(mesh.data.tessfaces)))

    print("Smooth groups active:", bpy.context.scene.udk_option_smoothing_groups)

    for face in mesh.data.tessfaces:

        smoothgroup_id = 0x80000000

        for smooth_group in smoothgroup_list:
            if smooth_group.contains_face(face):
                smoothgroup_id = smooth_group.id
                break

        # modified by VendorX
        object_material_index = face.material_index

        if len(face.vertices) != 3:
            raise Error("Non-triangular face (%i)" % len(face.vertices))

        # RG - apparently blender sometimes has problems when you do quad to triangle
        #   conversion, and ends up creating faces that have only TWO points -
        #   one of the points is simply in the vertex list for the face twice.
        #   This is bad, since we can't get a real face normal for a LINE, we need
        #   a plane for this. So, before we add the face to the list of real faces,
        #   ensure that the face is actually a plane, and not a line. If it is not
        #   planar, just discard it and notify the user in the console after we're
        #   done dumping the rest of the faces

        if not is_1d_face(face, mesh.data):

            wedge_list = []
            vect_list = []

            # get or create the current material
            psk.GetMatByIndex(object_material_index)

            face_index = face.index
            has_uv = False
            face_uv = None

            if len(mesh.data.uv_textures) > 0:
                has_uv = True
                uv_layer = mesh.data.tessface_uv_textures.active
                face_uv = uv_layer.data[face_index]
                # size(data) is number of texture faces. Each face has UVs
                # print("DATA face uv: ",len(faceUV.uv), " >> ",(faceUV.uv[0][0]))

            for i in range(3):
                vert_index = face.vertices[i]
                vert = mesh.data.vertices[vert_index]
                uv = []
                # assumes 3 UVs Per face (for now)
                if (has_uv):
                    if len(face_uv.uv) != 3:
                        print("WARNING: face has more or less than 3 UV coordinates - writing 0,0...")
                        uv = [0.0, 0.0]
                    else:
                        uv = [face_uv.uv[i][0], face_uv.uv[i][1]]  # OR bottom works better # 24 for cube
                else:
                    # print ("No UVs?")
                    uv = [0.0, 0.0]

                # flip V coordinate because UEd requires it and DOESN'T flip it on its own like it
                # does with the mesh Y coordinates. this is otherwise known as MAGIC-2
                uv[1] = 1.0 - uv[1]

                # clamp UV coords if udk_option_clamp_uv is True
                if bpy.context.scene.udk_option_clamp_uv:
                    if (uv[0] > 1):
                        uv[0] = 1
                    if (uv[0] < 0):
                        uv[0] = 0
                    if (uv[1] > 1):
                        uv[1] = 1
                    if (uv[1] < 0):
                        uv[1] = 0

                # RE - Append untransformed vector (for normal calc below)
                # TODO: convert to Blender.Mathutils
                vect_list.append(FVector(vert.co.x, vert.co.y, vert.co.z))

                # Transform position for export
                # vpos = vert.co * object_material_index

                # should fixed this!!
                vpos = mesh.matrix_local * vert.co
                if bpy.context.scene.udk_option_scale < 0 or bpy.context.scene.udk_option_scale > 1:
                    # print("OK!")
                    vpos.x = vpos.x * bpy.context.scene.udk_option_scale
                    vpos.y = vpos.y * bpy.context.scene.udk_option_scale
                    vpos.z = vpos.z * bpy.context.scene.udk_option_scale

                # print("scale pos:", vpos)
                # Create the point
                p = VPoint()
                p.Point.X = vpos.x
                p.Point.Y = vpos.y
                p.Point.Z = vpos.z
                if bpy.context.scene.udk_option_smoothing_groups:  # is this necessary?
                    p.SmoothGroup = smoothgroup_id

                lPoint = VPointSimple()
                lPoint.Point.X = vpos.x
                lPoint.Point.Y = vpos.y
                lPoint.Point.Z = vpos.z

                if lPoint in points_linked:
                    if not(p in points_linked[lPoint]):
                        points_linked[lPoint].append(p)
                else:
                    points_linked[lPoint] = [p]

                # Create the wedge
                w = VVertex()
                w.MatIndex = object_material_index
                w.PointIndex = points.get(p)  # store keys
                w.U = uv[0]
                w.V = uv[1]
                if bpy.context.scene.udk_option_smoothing_groups:  # is this necessary?
                    w.SmoothGroup = smoothgroup_id
                index_wedge = wedges.get(w)
                wedge_list.append(index_wedge)

                # print results
                # print("result PointIndex={}, U={:.6f}, V={:.6f}, wedge_index={}".format(
                #   w.PointIndex,
                #   w.U,
                #   w.V,
                #   index_wedge))

            # END for i in range(3)

            # Determine face vertex order

            # TODO: convert to Blender.Mathutils
            # get normal from blender
            no = face.normal
            # convert to FVector
            norm = FVector(no[0], no[1], no[2])
            # Calculate the normal of the face in blender order
            tnorm = vect_list[1].sub(vect_list[0]).cross(vect_list[2].sub(vect_list[1]))
            # RE - dot the normal from blender order against the blender normal
            # this gives the product of the two vectors' lengths along the blender normal axis
            # all that matters is the sign
            dot = norm.dot(tnorm)

            tri = VTriangle()
            # RE - magic: if the dot product above > 0, order the vertices 2, 1, 0
            #      if the dot product above < 0, order the vertices 0, 1, 2
            #      if the dot product is 0, then blender's normal is coplanar with the face
            #      and we cannot deduce which side of the face is the outside of the mesh
            if dot > 0:
                (tri.WedgeIndex2, tri.WedgeIndex1, tri.WedgeIndex0) = wedge_list
            elif dot < 0:
                (tri.WedgeIndex0, tri.WedgeIndex1, tri.WedgeIndex2) = wedge_list
            else:
                dindex0 = face.vertices[0]
                dindex1 = face.vertices[1]
                dindex2 = face.vertices[2]

                mesh.data.vertices[dindex0].select = True
                mesh.data.vertices[dindex1].select = True
                mesh.data.vertices[dindex2].select = True

                raise Error("Normal coplanar with face! points: %s, %s, %s" % (str(mesh.data.vertices[dindex0].co),
                                                                               str(mesh.data.vertices[dindex1].co),
                                                                               str(mesh.data.vertices[dindex2].co)))

            face.select = True
            if face.use_smooth is True:
                tri.SmoothingGroups = 1
            else:
                tri.SmoothingGroups = 0
            tri.MatIndex = object_material_index

            if bpy.context.scene.udk_option_smoothing_groups:
                tri.SmoothingGroups = smoothgroup_id
                print("Bool Smooth")

            psk.AddFace(tri)

        # END if not is_1d_face(current_face, mesh.data)

        else:
            discarded_face_count += 1

    # END face in mesh.data.faces

    print("{} points".format(len(points.dict)))

    for point in points.items():
        psk.AddPoint(point)

    if len(points.dict) > 32767:
        raise Error("Mesh vertex limit exceeded! {} > 32767".format(len(points.dict)))

    print("{} wedges".format(len(wedges.dict)))

    for wedge in wedges.items():
        psk.AddWedge(wedge)

    # alert the user to degenerate face issues
    if discarded_face_count > 0:
        print("WARNING: Mesh contained degenerate faces (non-planar)")
        print("      Discarded {} faces".format(discarded_face_count))

    # RG - walk through the vertex groups and find the indexes into the PSK points array
    # for them, then store that index and the weight as a tuple in a new list of
    # verts for the group that we can look up later by bone name, since Blender matches
    # verts to bones for influences by having the VertexGroup named the same thing as
    # the bone

    # [print(x, len(points_linked[x])) for x in points_linked]
    # print("pointsindex length ",len(points_linked))
    # vertex group

    # all vertex groups of the mesh (obj)...
    for obj_vertex_group in mesh.vertex_groups:

        # print("  bone group build:",obj_vertex_group.name)#print bone name
        # print(dir(obj_vertex_group))
        verbose("obj_vertex_group.name={}".format(obj_vertex_group.name))

        vertex_list = []

        # all vertices in the mesh...
        for vertex in mesh.data.vertices:
            # print(dir(vertex))
            # all groups this vertex is a member of...
            for vgroup in vertex.groups:
                if vgroup.group == obj_vertex_group.index:
                    vertex_weight = vgroup.weight
                    p = VPointSimple()
                    vpos = mesh.matrix_local * vertex.co

                    if bpy.context.scene.udk_option_scale < 0 or bpy.context.scene.udk_option_scale > 1:
                        vpos.x = vpos.x * bpy.context.scene.udk_option_scale
                        vpos.y = vpos.y * bpy.context.scene.udk_option_scale
                        vpos.z = vpos.z * bpy.context.scene.udk_option_scale
                    p.Point.X = vpos.x
                    p.Point.Y = vpos.y
                    p.Point.Z = vpos.z
                    # print(p)
                    # print(len(points_linked[p]))
                    try:  # check if point doesn't give error
                        for point in points_linked[p]:
                            point_index = points.get(point)  # point index
                            v_item = (point_index, vertex_weight)
                            vertex_list.append(v_item)
                    except Exception:  # if get error ignore them # not safe I think
                        print("Error link points!")
                        pass

        # bone name, [point id and wieght]
        # print("Add Vertex Group:",obj_vertex_group.name, " No. Points:",len(vertex_list))
        psk.VertexGroups[obj_vertex_group.name] = vertex_list

    # remove the temporary triangulated mesh
    if bpy.context.scene.udk_option_triangulate is True:
        verbose("Removing temporary triangle mesh: {}".format(mesh.name))
        bpy.ops.object.mode_set(mode='OBJECT')    # OBJECT mode
        mesh.parent = None                        # unparent to avoid phantom links
        bpy.context.scene.objects.unlink(mesh)    # unlink


# ===========================================================================
# Collate bones that belong to the UDK skeletal mesh
# ===========================================================================
def parse_armature(armature, psk, psa):

    print(header("ARMATURE", 'RIGHT'))
    verbose("Armature object: {} Armature data: {}".format(armature.name, armature.data.name))

    # generate a list of root bone candidates
    root_candidates = [b for b in armature.data.bones if b.parent is None and b.use_deform is True]

    # should be a single, unambiguous result
    if len(root_candidates) == 0:
        raise Error("Cannot find root for UDK bones. The root bone must use deform.")

    if len(root_candidates) > 1:
        raise Error("Ambiguous root for UDK. More than one root bone is using deform.")

    # prep for bone collection
    udk_root_bone = root_candidates[0]
    udk_bones = []
    BoneUtil.static_bone_id = 0  # replaces global

    # traverse bone chain
    print("{: <3} {: <48} {: <20}".format("ID", "Bone", "Status"))
    print()
    recurse_bone(udk_root_bone, udk_bones, psk, psa, 0, armature.matrix_local)

    # final validation
    if len(udk_bones) < 3:
        raise Error("Less than three bones may crash UDK (legacy issue?)")

    # return a list of bones making up the entire udk skel
    # this is passed to parse_animation instead of working from keyed bones in the action
    return udk_bones


# ===========================================================================
# bone              current bone
# bones             bone list
# psk               the PSK file object
# psa               the PSA file object
# parent_id
# parent_matrix
# indent            text indent for recursive log
# ===========================================================================
def recurse_bone(bone, bones, psk, psa, parent_id, parent_matrix, indent=""):

    status = "Ok"
    bones.append(bone)

    if not bone.use_deform:
        status = "No effect"

    # calc parented bone transform
    if bone.parent is not None:
        quat = make_fquat(bone.matrix.to_quaternion())
        quat_parent = bone.parent.matrix.to_quaternion().inverted()
        parent_head = quat_parent * bone.parent.head
        parent_tail = quat_parent * bone.parent.tail
        translation = (parent_tail - parent_head) + bone.head

    # calc root bone transform
    else:
        translation = parent_matrix * bone.head             # ARMATURE OBJECT Location
        rot_matrix = bone.matrix * parent_matrix.to_3x3()   # ARMATURE OBJECT Rotation
        quat = make_fquat_default(rot_matrix.to_quaternion())

    # udk_option_scale bones here?
    if bpy.context.scene.udk_option_scale < 0 or bpy.context.scene.udk_option_scale > 1:
        translation.x = translation.x * bpy.context.scene.udk_option_scale
        translation.y = translation.y * bpy.context.scene.udk_option_scale
        translation.z = translation.z * bpy.context.scene.udk_option_scale
    bone_id = BoneUtil.static_bone_id       # ALT VERS
    BoneUtil.static_bone_id += 1            # ALT VERS

    child_count = len(bone.children)

    psk.AddBone(make_vbone(bone.name, parent_id, child_count, quat, translation))
    psa.StoreBone(make_namedbonebinary(bone.name, parent_id, child_count, quat, translation, 1))

    # RG - dump influences for this bone - use the data we collected
    # in the mesh dump phase to map our bones to vertex groups
    if bone.name in psk.VertexGroups:
        vertex_list = psk.VertexGroups[bone.name]
        # print("vertex list:", len(vertex_list), " of >" ,bone.name)

        for vertex_data in vertex_list:
            point_index = vertex_data[0]
            vertex_weight = vertex_data[1]
            influence = VRawBoneInfluence()
            influence.Weight = vertex_weight
            influence.BoneIndex = bone_id
            influence.PointIndex = point_index
            # print ("   AddInfluence to vertex {}, weight={},".format(point_index, vertex_weight))
            psk.AddInfluence(influence)
    else:
        status = "No vertex group"
        # FIXME overwriting previous status error?

    print("{:<3} {:<48} {:<20}".format(bone_id, indent + bone.name, status))

    # bone.matrix_local
    # recursively dump child bones

    for child_bone in bone.children:
        recurse_bone(child_bone, bones, psk, psa, bone_id, parent_matrix, " " + indent)


# FIXME rename? remove?
class BoneUtil:
    static_bone_id = 0  # static property to replace global


# ===========================================================================
# armature          the armature
# udk_bones         list of bones to be exported
# actions_to_export list of actions to process for export
# psa               the PSA file object
# ===========================================================================
def parse_animation(armature, udk_bones, actions_to_export, psa):

    print(header("ANIMATION", 'RIGHT'))

    context = bpy.context
    anim_rate = context.scene.render.fps

    verbose("Armature object: {}".format(armature.name))
    print("Scene: {} FPS: {} Frames: {} to {}".format(context.scene.name, anim_rate,
                                                      context.scene.frame_start, context.scene.frame_end)
        )
    print("Processing {} action(s)\n".format(len(actions_to_export)))

    # if animation data was not create for the armature it will skip the exporting action set(s)
    if armature.animation_data is None:
        print("None Actions Set! skipping...")
        return
    restoreAction = armature.animation_data.action    # Q: is animation_data always valid?
    # we already do this in export_proxy, but we'll do it here too for now
    restoreFrame = context.scene.frame_current
    raw_frame_index = 0  # used to set FirstRawFrame, seperating actions in the raw keyframe array

    # action loop...
    for action in actions_to_export:

        # removed: check for armature with no animation; all it did was force you to add one

        if not len(action.fcurves):
            print("{} has no keys, skipping".format(action.name))
            continue

        # apply action to armature and update scene
        # note if loop all actions that is not armature it will override and will break armature animation
        armature.animation_data.action = action
        context.scene.update()

        # min/max frames define range
        framemin, framemax = action.frame_range
        start_frame = int(framemin)
        end_frame = int(framemax)
        scene_range = range(start_frame, end_frame + 1)
        frame_count = len(scene_range)

        # create the AnimInfoBinary
        anim = AnimInfoBinary()
        anim.Name = action.name
        anim.Group = ""  # unused?
        anim.NumRawFrames = frame_count
        anim.AnimRate = anim_rate
        anim.FirstRawFrame = raw_frame_index

        print("{}, frames {} to {} ({} frames)".format(action.name, start_frame, end_frame, frame_count))

        # removed: bone lookup table

        # build a list of pose bones relevant to the collated udk_bones
        # fixme: could be done once, prior to loop?
        udk_pose_bones = []
        for b in udk_bones:
            for pb in armature.pose.bones:
                if b.name == pb.name:
                    udk_pose_bones.append(pb)
                    break

        # sort in the order the bones appear in the PSA file
        ordered_bones = {}
        ordered_bones = sorted([(psa.UseBone(b.name), b) for b in udk_pose_bones], key=operator.itemgetter(0))

        # NOTE: posebone.bone references the obj/edit bone
        # REMOVED: unique_bone_indexes is redundant?

        # frame loop...
        for i in range(frame_count):

            frame = scene_range[i]

            # verbose("FRAME {}".format(i), i) # test loop sampling

            # advance to frame (automatically updates the pose)
            context.scene.frame_set(frame)

            # compute the key for each bone
            for bone_data in ordered_bones:

                bone_index = bone_data[0]
                pose_bone = bone_data[1]
                pose_bone_matrix = mathutils.Matrix(pose_bone.matrix)

                if pose_bone.parent is not None:
                    pose_bone_parent_matrix = mathutils.Matrix(pose_bone.parent.matrix)
                    pose_bone_matrix = pose_bone_parent_matrix.inverted() * pose_bone_matrix

                head = pose_bone_matrix.to_translation()
                quat = pose_bone_matrix.to_quaternion().normalized()

                if pose_bone.parent is not None:
                    quat = make_fquat(quat)
                else:
                    quat = make_fquat_default(quat)

                # scale animation position here?
                if bpy.context.scene.udk_option_scale < 0 or bpy.context.scene.udk_option_scale > 1:
                    head.x = head.x * bpy.context.scene.udk_option_scale
                    head.y = head.y * bpy.context.scene.udk_option_scale
                    head.z = head.z * bpy.context.scene.udk_option_scale

                vkey = VQuatAnimKey()
                vkey.Position.X = head.x
                vkey.Position.Y = head.y
                vkey.Position.Z = head.z
                vkey.Orientation = quat

                # frame delta = 1.0 / fps
                vkey.Time = 1.0 / anim_rate  # according to C++ header this is "disregarded"

                psa.AddRawKey(vkey)

            # END for bone_data in ordered_bones

            raw_frame_index += 1

        # END for i in range(frame_count)

        # REMOVED len(unique_bone_indexes)
        anim.TotalBones = len(ordered_bones)
        # frame_count/anim.AnimRate makes more sense, but this is what actually works in UDK
        anim.TrackTime = float(frame_count)

        verbose("anim.TotalBones={}, anim.TrackTime={}".format(anim.TotalBones, anim.TrackTime))

        psa.AddAnimation(anim)

    # END for action in actions

    # restore
    armature.animation_data.action = restoreAction
    context.scene.frame_set(restoreFrame)


# ===========================================================================
# Collate actions to be exported
# Modify this to filter for one, some or all actions. For now use all.
# RETURNS list of actions
# ===========================================================================
def collate_actions():
    verbose(header("collate_actions"))
    actions_to_export = []

    for action in bpy.data.actions:
        if bpy.context.scene.udk_option_selectanimations:  # check if needed to select actions set for exporting it
            print("Action Set is selected!")
            bready = False
            for actionlist in bpy.context.scene.udkas_list:  # list the action set from the list
                if actionlist.name == action.name and actionlist.bmatch is True and actionlist.bexport is True:
                    bready = True
                    print("Added Action Set:", action.name)
                    break
            if bready is False:  # don't export it
                print("Skipping Action Set:", action.name)
                continue
        verbose(" + {}".format(action.name))  # action set name
        actions_to_export.append(action)  # add to the action array

    return actions_to_export


# ===========================================================================
# Locate the target armature and mesh for export
# RETURNS armature, mesh
# ===========================================================================
def find_armature_and_mesh():
    verbose(header("find_armature_and_mesh", 'LEFT', '<', 60))

    context = bpy.context
    active_object = context.active_object
    armature = None
    mesh = None

    # TODO:
    # this could be more intuitive
    # bpy.ops.object.mode_set(mode='OBJECT')

    if bpy.context.scene.udk_option_selectobjects:  # if checked select object true do list object on export
        print("select mode:")
        if len(bpy.context.scene.udkArm_list) > 0:
            print("Armature Name:", bpy.context.scene.udkArm_list[bpy.context.scene.udkArm_list_idx].name)
            for obj in bpy.context.scene.objects:
                if obj.name == bpy.context.scene.udkArm_list[bpy.context.scene.udkArm_list_idx].name:
                    armature = obj
                    break
        else:
            raise Error("There is no Armature in the list!")
        meshselected = []
        # parented_meshes = [obj for obj in armature.children if obj.type == 'MESH']
        meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
        for obj in meshes:
            # print(dir(obj))
            if obj.type == 'MESH':
                bexportmesh = False
                # print("PARENT MESH:",obj.name)
                for udkmeshlist in bpy.context.scene.udkmesh_list:
                    if obj.name == udkmeshlist.name and udkmeshlist.bexport is True:
                        bexportmesh = True
                        break
                if bexportmesh is True:
                    print("Mesh Name:", obj.name, " < SELECT TO EXPORT!")
                    meshselected.append(obj)

        print("MESH COUNT:", len(meshselected))
        # try the active object
        if active_object and active_object.type == 'MESH' and len(meshselected) == 0:
            if active_object.parent == armature:
                mesh = active_object
            else:
                raise Error("The selected mesh is not parented to the armature")

        # otherwise, expect a single mesh parented to the armature (other object types are ignored)
        else:
            print("Number of meshes:", len(meshes))
            print("Number of meshes (selected):", len(meshes))
            if len(meshes) == 1:
                mesh = meshes[0]

            elif len(meshes) > 1:
                if len(meshselected) >= 1:
                    mesh = sortmesh(meshselected)
                else:
                    raise Error("More than one mesh(s) parented to armature. Select object(s)!")
            else:
                raise Error("No mesh parented to armature")
    else:  # if not check for select function from the list work the code here
        print("normal mode:")
        # try the active object
        if active_object and active_object.type == 'ARMATURE':
            armature = active_object
            bpy.ops.object.mode_set(mode='OBJECT')
        # otherwise, try for a single armature in the scene
        else:
            # bpy.ops.object.mode_set(mode='OBJECT')
            all_armatures = [obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE']

            if len(all_armatures) == 1:  # if armature has one scene just assign it
                armature = all_armatures[0]
            elif len(all_armatures) > 1:  # if there more armature then find the select armature
                barmselect = False
                for _armobj in all_armatures:
                    if _armobj.select:
                        armature = _armobj
                        barmselect = True
                        break
                if barmselect is False:
                    raise Error("Please select an armatures in the scene")
            else:
                raise Error("No armatures in scene")

        verbose("Found armature: {}".format(armature.name))

        meshselected = []
        parented_meshes = [obj for obj in armature.children if obj.type == 'MESH']

        if len(armature.children) == 0:
            raise Error("The selected Armature has no mesh parented to the Armature Object!")

        for obj in armature.children:
            # print(dir(obj))
            if obj.type == 'MESH' and obj.select is True:
                meshselected.append(obj)
        # try the active object
        if active_object and active_object.type == 'MESH' and len(meshselected) == 0:
            if active_object.parent == armature:
                mesh = active_object
            else:
                raise Error("The selected mesh is not parented to the armature")

        # otherwise, expect a single mesh parented to the armature (other object types are ignored)
        else:
            print("Number of meshes:", len(parented_meshes))
            print("Number of meshes (selected):", len(meshselected))
            if len(parented_meshes) == 1:
                mesh = parented_meshes[0]

            elif len(parented_meshes) > 1:
                if len(meshselected) >= 1:
                    mesh = sortmesh(meshselected)
                else:
                    raise Error("More than one mesh(s) parented to armature. Select object(s)!")
            else:
                raise Error("No mesh parented to armature")

        verbose("Found mesh: {}".format(mesh.name))
    if mesh is None or armature is None:
        raise Error("Check Mesh and Armature are list!")

    """
    if len(armature.pose.bones) == len(mesh.vertex_groups):
        print("Armature and Mesh Vertex Groups matches Ok!")
    else:
        raise Error("Armature bones:" + str(len(armature.pose.bones)) +
                     " Mesh Vertex Groups:" + str(len(mesh.vertex_groups)) +" doesn't match!")
    """
    # this will check if object need to be rebuild
    if bpy.context.scene.udk_option_rebuildobjects:
        # print("INIT... REBUILDING...")
        print("REBUILDING ARMATURE...")
        # if deform mesh
        # rebuild the armature to raw. If there IK constraint it will ignore it
        armature = rebuildarmature(armature)
        print("REBUILDING MESH...")
        mesh = rebuildmesh(mesh)  # rebuild the mesh to raw data format.

    return armature, mesh


# ===========================================================================
# Returns a list of vertex groups in the mesh. Can be modified to filter
# groups as necessary.
# UNUSED
# ===========================================================================
def collate_vertex_groups(mesh):
    verbose("collate_vertex_groups")
    groups = []

    for group in mesh.vertex_groups:

        groups.append(group)
        verbose("  " + group.name)

    return groups


# ===========================================================================
# Main
# ===========================================================================
def export(filepath):
    print(header("Export", 'RIGHT'))
    bpy.types.Scene.udk_copy_merge = False  # in case fail to export set this to default
    t = time.clock()
    context = bpy.context

    print("Blender Version {}.{}.{}".format(bpy.app.version[0], bpy.app.version[1], bpy.app.version[2]))
    print("Filepath: {}".format(filepath))

    verbose("PSK={}, PSA={}".format(context.scene.udk_option_export_psk, context.scene.udk_option_export_psa))

    # find armature and mesh
    # [change this to implement alternative methods; raise Error() if not found]
    udk_armature, udk_mesh = find_armature_and_mesh()

    # check misc conditions
    if not (udk_armature.scale.x == udk_armature.scale.y == udk_armature.scale.z == 1):
        raise Error("bad armature scale: armature object should have uniform scale of 1 (ALT-S)")

    if not (udk_mesh.scale.x == udk_mesh.scale.y == udk_mesh.scale.z == 1):
        raise Error("bad mesh scale: mesh object should have uniform scale of 1 (ALT-S)")

    if not (udk_armature.location.x == udk_armature.location.y == udk_armature.location.z == 0):
        raise Error("bad armature location: armature should be located at origin (ALT-G)")

    if not (udk_mesh.location.x == udk_mesh.location.y == udk_mesh.location.z == 0):
        raise Error("bad mesh location: mesh should be located at origin (ALT-G)")

    # prep
    psk = PSKFile()
    psa = PSAFile()

    # step 1
    parse_mesh(udk_mesh, psk)

    # step 2
    udk_bones = parse_armature(udk_armature, psk, psa)

    # step 3
    if context.scene.udk_option_export_psa is True:
        actions = collate_actions()
        parse_animation(udk_armature, udk_bones, actions, psa)

    # write files
    print(header("Exporting", 'CENTER'))

    psk_filename = filepath + '.psk'
    psa_filename = filepath + '.psa'

    if context.scene.udk_option_export_psk is True:
        print("Skeletal mesh data...")
        psk.PrintOut()
        file = open(psk_filename, "wb")
        file.write(psk.dump())
        file.close()
        print("Exported: " + psk_filename)
        print()

    if context.scene.udk_option_export_psa is True:
        print("Animation data...")
        if not psa.IsEmpty():
            psa.PrintOut()
            file = open(psa_filename, "wb")
            file.write(psa.dump())
            file.close()
            print("Exported: " + psa_filename)
        else:
            print("No Animation (.psa file) to export")

        print()

    # if objects are rebuild do the unlink
    if bpy.context.scene.udk_option_rebuildobjects:
        print("Unlinking Objects")
        print("Armature Object Name:", udk_armature.name)  # display object name
        bpy.context.scene.objects.unlink(udk_armature)     # remove armature from the scene
        print("Mesh Object Name:", udk_mesh.name)          # display object name
        bpy.context.scene.objects.unlink(udk_mesh)         # remove mesh from the scene

    print("Export completed in {:.2f} seconds".format((time.clock() - t)))


# ===========================================================================
# Operator
# ===========================================================================
class Operator_UDKExport(Operator):
    """Export to UDK"""
    bl_idname = "object.udk_export"
    bl_label = "Export now"

    def execute(self, context):
        print("\n" * 8)
        scene = bpy.context.scene

        scene.udk_option_export_psk = (scene.udk_option_export == '0' or scene.udk_option_export == '2')
        scene.udk_option_export_psa = (scene.udk_option_export == '1' or scene.udk_option_export == '2')

        filepath = get_dst_path()

        # cache settings
        restore_frame = scene.frame_current
        message = "Object(s) exported to: {}".format(filepath)
        try:
            export(filepath)

        except Error as err:
            print(err.message)
            message = err.message

        # restore settings
        scene.frame_set(restore_frame)

        def draw(self, context):
            self.layout.label(text="Export Finished")
        try:
            context.window_manager.popup_menu(draw, title=message, icon="INFO")
        except:
            pass

        self.report({'INFO'}, message)

        # restore settings
        scene.frame_set(restore_frame)

        return {'FINISHED'}


# ===========================================================================
# Operator
# ===========================================================================
class Operator_ToggleConsole(Operator):
    """Show or hide the console"""
    bl_idname = "object.toggle_console"
    bl_label = "Toggle console"

    def execute(self, context):
        bpy.ops.wm.console_toggle()
        return {'FINISHED'}


# ===========================================================================
# Get filepath for export
# ===========================================================================
def get_dst_path():
    if bpy.context.scene.udk_option_filename_src == '0':
        if bpy.context.active_object:
            path = os.path.split(bpy.data.filepath)[0] + "\\" + bpy.context.active_object.name  # + ".psk"
        else:
            # path = os.path.split(bpy.data.filepath)[0] + "\\" + "Unknown";
            path = os.path.splitext(bpy.data.filepath)[0]  # + ".psk"
    else:
        path = os.path.splitext(bpy.data.filepath)[0]  # + ".psk"
    return path


# ===========================================================================
# User interface
# ===========================================================================
class OBJECT_OT_UTSelectedFaceSmooth(Operator):
    """It will only select smooth faces that is select mesh"""
    bl_idname = "object.utselectfacesmooth"  # XXX, name???
    bl_label = "Select Smooth Faces"         # "Select Smooth faces"

    def invoke(self, context, event):
        print("----------------------------------------")
        print("Init Select Face(s):")
        bselected = False
        for obj in bpy.data.objects:
            if obj.type == 'MESH' and obj.select is True:
                smoothcount = 0
                flatcount = 0
                bpy.ops.object.mode_set(mode='OBJECT')  # it need to go into object mode to able to select the faces

                for i in bpy.context.scene.objects:
                    i.select = False  # deselect all objects

                obj.select = True  # set current object select
                bpy.context.scene.objects.active = obj  # set active object
                mesh = bmesh.new()
                mesh.from_mesh(obj.data)

                for face in mesh.faces:
                    face.select = False

                for face in mesh.faces:
                    if face.smooth is True:
                        face.select = True
                        smoothcount += 1
                    else:
                        flatcount += 1
                        face.select = False
                mesh.to_mesh(obj.data)
                bpy.context.scene.update()
                bpy.ops.object.mode_set(mode='EDIT')
                print("Select Smooth Count(s):", smoothcount, " Flat Count(s):", flatcount)
                bselected = True
                break
        if bselected:
            self.report({'INFO'}, "Selected Face(s) Executed")
        else:
            self.report({'INFO'}, "Mesh Object is not selected")
        print("----------------------------------------")

        return{'FINISHED'}


class OBJECT_OT_MeshClearWeights(Operator):
    """Remove all mesh vertex groups weights for the bones"""
    bl_idname = "object.meshclearweights"  # XXX, name???
    bl_label = "Remove Vertex Weights"     # "Remove Mesh vertex weights"

    def invoke(self, context, event):
        for obj in bpy.data.objects:
            if obj.type == 'MESH' and obj.select is True:
                for vg in obj.vertex_groups:
                    obj.vertex_groups.remove(vg)
                self.report({'INFO'}, "Mesh Vertex Groups Removed")
                break
        return{'FINISHED'}


def unpack_list(list_of_tuples):
    l = []
    for t in list_of_tuples:
        l.extend(t)
    return l


def rebuildmesh(obj):
    # make sure it in object mode
    print("Mesh Object Name:", obj.name)
    bpy.ops.object.mode_set(mode='OBJECT')

    for i in bpy.context.scene.objects:
        i.select = False  # deselect all objects
    obj.select = True
    bpy.context.scene.objects.active = obj

    me_ob = bpy.data.meshes.new(("Re_" + obj.name))
    mesh = obj.data
    faces = []
    verts = []
    smoothings = []
    uvfaces = []
    # print("creating array build mesh...")
    mmesh = obj.to_mesh(bpy.context.scene, True, 'PREVIEW')
    uv_layer = mmesh.tessface_uv_textures.active

    for face in mmesh.tessfaces:
        smoothings.append(face.use_smooth)  # smooth or flat in boolean
        if uv_layer is not None:            # check if there texture data exist
            faceUV = uv_layer.data[face.index]
            uvs = []
            for uv in faceUV.uv:
                uvs.append((uv[0], uv[1]))
            uvfaces.append(uvs)
        # print((face.vertices[:]))
        if len(face.vertices) == 3:
            faces.extend([(face.vertices[0], face.vertices[1], face.vertices[2], 0)])
        else:
            faces.extend([(face.vertices[0], face.vertices[1], face.vertices[2], face.vertices[3])])

    # vertex positions
    for vertex in mesh.vertices:
        verts.append(vertex.co.to_tuple())
    # vertices weight groups into array
    vertGroups = {}  # array in strings

    for vgroup in obj.vertex_groups:
        vlist = []
        for v in mesh.vertices:
            for vg in v.groups:
                if vg.group == vgroup.index:
                    vlist.append((v.index, vg.weight))
                    # print((v.index,vg.weight))
        vertGroups[vgroup.name] = vlist

    # print("creating mesh object...")
    # me_ob.from_pydata(verts, [], faces)
    me_ob.vertices.add(len(verts))
    me_ob.tessfaces.add(len(faces))
    me_ob.vertices.foreach_set("co", unpack_list(verts))
    me_ob.tessfaces.foreach_set("vertices_raw", unpack_list(faces))
    me_ob.tessfaces.foreach_set("use_smooth", smoothings)  # smooth array from face

    # check if there is uv faces
    if len(uvfaces) > 0:
        uvtex = me_ob.tessface_uv_textures.new(name="retex")
        for i, face in enumerate(me_ob.tessfaces):
            blender_tface = uvtex.data[i]  # face
            mfaceuv = uvfaces[i]
            if len(mfaceuv) == 3:
                blender_tface.uv1 = mfaceuv[0]
                blender_tface.uv2 = mfaceuv[1]
                blender_tface.uv3 = mfaceuv[2]
            if len(mfaceuv) == 4:
                blender_tface.uv1 = mfaceuv[0]
                blender_tface.uv2 = mfaceuv[1]
                blender_tface.uv3 = mfaceuv[2]
                blender_tface.uv4 = mfaceuv[3]

    me_ob.update()  # need to update the information to able to see into the secne
    obmesh = bpy.data.objects.new(("Re_" + obj.name), me_ob)
    bpy.context.scene.update()

    # Build tmp materials
    materialname = "ReMaterial"
    for matcount in mesh.materials:
        matdata = bpy.data.materials.new(materialname)
        me_ob.materials.append(matdata)

    # assign face to material id
    for face in mesh.tessfaces:
        me_ob.faces[face.index].material_index = face.material_index

    # vertices weight groups
    for vgroup in vertGroups:
        group = obmesh.vertex_groups.new(vgroup)
        for v in vertGroups[vgroup]:
            group.add([v[0]], v[1], 'ADD')  # group.add(array[vertex id],weight,add)
    bpy.context.scene.objects.link(obmesh)
    # print("Mesh Material Count:",len(me_ob.materials))
    matcount = 0
    # print("MATERIAL ID OREDER:")
    for mat in me_ob.materials:
        # print("-Material:",mat.name,"INDEX:",matcount)
        matcount += 1

    print("Mesh Object Name:", obmesh.name)
    bpy.context.scene.update()

    return obmesh


class OBJECT_OT_UTRebuildMesh(Operator):
    """It rebuild the mesh from scrape from the selected mesh object. """ \
    """Note the scale will be 1:1 for object mode. To keep from deforming"""
    bl_idname = "object.utrebuildmesh"  # XXX, name???
    bl_label = "Rebuild Mesh"           # "Rebuild Mesh"

    def invoke(self, context, event):
        print("----------------------------------------")
        print("Init Mesh Bebuild...")
        bselected = False
        bpy.ops.object.mode_set(mode='OBJECT')

        for obj in bpy.data.objects:
            if obj.type == 'MESH' and obj.select is True:
                rebuildmesh(obj)

        self.report({'INFO'}, "Rebuild Mesh Finished!")
        print("Finish Mesh Build...")
        print("----------------------------------------")
        return{'FINISHED'}


def rebuildarmature(obj):
    currentbone = []  # select armature for roll copy
    print("Armature Name:", obj.name)
    objectname = "ArmatureDataPSK"
    meshname = "ArmatureObjectPSK"
    armdata = bpy.data.armatures.new(objectname)
    ob_new = bpy.data.objects.new(meshname, armdata)
    bpy.context.scene.objects.link(ob_new)
    # bpy.ops.object.mode_set(mode='OBJECT')

    for i in bpy.context.scene.objects:
        i.select = False  # deselect all objects

    ob_new.select = True
    bpy.context.scene.objects.active = obj

    bpy.ops.object.mode_set(mode='EDIT')
    for bone in obj.data.edit_bones:
        if bone.parent is not None:
            currentbone.append([bone.name, bone.roll])
        else:
            currentbone.append([bone.name, bone.roll])
    bpy.ops.object.mode_set(mode='OBJECT')

    for i in bpy.context.scene.objects:
        i.select = False  # deselect all objects

    bpy.context.scene.objects.active = ob_new
    bpy.ops.object.mode_set(mode='EDIT')

    for bone in obj.data.bones:
        bpy.ops.object.mode_set(mode='EDIT')
        newbone = ob_new.data.edit_bones.new(bone.name)
        newbone.head = bone.head_local
        newbone.tail = bone.tail_local
        for bonelist in currentbone:
            if bone.name == bonelist[0]:
                newbone.roll = bonelist[1]
                break
        if bone.parent is not None:
            parentbone = ob_new.data.edit_bones[bone.parent.name]
            newbone.parent = parentbone

    ob_new.animation_data_create()      # create animation data
    if obj.animation_data is not None:  # check for animation
        # just make sure it here to do the animations if exist
        ob_new.animation_data.action = obj.animation_data.action

    print("Armature Object Name:", ob_new.name)
    return ob_new


class OBJECT_OT_UTRebuildArmature(Operator):
    """If mesh is deform when importing to unreal engine try this. """ \
    """It rebuild the bones one at the time by select one armature object scrape to raw setup build. """ \
    """Note the scale will be 1:1 for object mode. To keep from deforming"""
    bl_idname = "object.utrebuildarmature"  # XXX, name???
    bl_label = "Rebuild Armature"           # Rebuild Armature

    def invoke(self, context, event):
        print("----------------------------------------")
        print("Init Rebuild Armature...")
        bselected = False
        for obj in bpy.data.objects:
            if obj.type == 'ARMATURE' and obj.select is True:
                rebuildarmature(obj)
        self.report({'INFO'}, "Rebuild Armature Finish!")
        print("End of Rebuild Armature.")
        print("----------------------------------------")
        return{'FINISHED'}


class UDKActionSetListPG(PropertyGroup):
    bool = BoolProperty(default=False)
    string = StringProperty()
    actionname = StringProperty()
    bmatch = BoolProperty(
            default=False,
            name="Match",
            options={"HIDDEN"},
            description="This check against bone names and action group "
                        "names matches and override boolean if true"
            )
    bexport = BoolProperty(
            default=False,
            name="Export",
            description="Check this to export the animation"
            )


class UL_UDKActionSetList(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(item.name)
        layout.prop(item, "bmatch", text="Match")
        layout.prop(item, "bexport", text="Export")


class UDKObjListPG(PropertyGroup):
    bool = BoolProperty(default=False)
    string = StringProperty()
    bexport = BoolProperty(
            default=False,
            name="Export",
            options={"HIDDEN"},
            description="This will be ignore when exported"
            )
    bselect = BoolProperty(
            default=False,
            name="Select",
            options={"HIDDEN"},
            description="This will be ignore when exported"
            )
    otype = StringProperty(
            name="Type",
            description="This will be ignore when exported"
            )


class UL_UDKObjList(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(item.name)
        layout.prop(item, "otype", text="")
        layout.prop(item, "bselect", text="")


class UDKMeshListPG(PropertyGroup):
    bool = BoolProperty(
            default=False
            )
    string = StringProperty()
    bexport = BoolProperty(
            default=False,
            name="Export",
            options={"HIDDEN"},
            description="This object will be export when true"
            )
    bselect = BoolProperty(
            default=False,
            name="Select",
            options={"HIDDEN"},
            description="Make sure you have Mesh is parent to Armature"
            )
    otype = StringProperty(
            name="Type",
            description="This will be ignore when exported"
            )


class UL_UDKMeshList(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(item.name)
        # layout.prop(item, "bselect", text="Select")
        layout.prop(item, "bexport", text="Export")


class UDKArmListPG(PropertyGroup):
    bool = BoolProperty(default=False)
    string = StringProperty()
    bexport = BoolProperty(
            default=False,
            name="Export",
            options={"HIDDEN"},
            description="This will be ignore when exported"
            )
    bselect = BoolProperty(
            default=False,
            name="Select",
            options={"HIDDEN"},
            description="This will be ignore when exported"
            )
    otype = StringProperty(
            name="Type",
            description="This will be ignore when exported"
            )


class UL_UDKArmList(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(item.name)


class Panel_UDKExport(Panel):
    bl_label = "UDK Export"
    bl_idname = "OBJECT_PT_udk_tools"
    bl_category = "File I/O"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = "objectmode"

    """
    def draw_header(self, context):
        layout = self.layout
        obj = context.object
        layout.prop(obj, "select", text="")

    @classmethod
    def poll(cls, context):
        return context.active_object
    """

    def draw(self, context):
        layout = self.layout
        path = get_dst_path()

        object_name = ""
        """
        if context.object:
            object_name = context.object.name
        """
        if context.active_object:
            object_name = context.active_object.name
        row10 = layout.row()
        row10.prop(context.scene, "udk_option_smoothing_groups")
        row10.prop(context.scene, "udk_option_clamp_uv")
        row10.prop(context.scene, "udk_option_verbose")

        row = layout.row()
        row.label(text="Active object: " + object_name)
        layout.prop(context.scene, "udk_option_filename_src")
        row = layout.row()
        row.label(text=path)

        layout.prop(context.scene, "udk_option_export")
        layout.prop(context.scene, "udk_option_selectobjects")

        if context.scene.udk_option_selectobjects:
            layout.operator("object.selobjectpdate")
            layout.label(text="ARMATURE - Index")
            layout.template_list("UL_UDKArmList", "udk_armatures", context.scene, "udkArm_list",
                                 context.scene, "udkArm_list_idx", rows=3)
            layout.label(text="MESH - Export")
            layout.template_list("UL_UDKMeshList", "", context.scene, "udkmesh_list",
                                 context.scene, "udkmesh_list_idx", rows=5)
        layout.prop(context.scene, "udk_option_selectanimations")

        if context.scene.udk_option_selectanimations:
            layout.operator("action.setanimupdate")
            layout.label(text="Action Set(s) - Match / Export")
            layout.template_list("UL_UDKActionSetList", "", context.scene, "udkas_list",
                                 context.scene, "udkas_list_idx", rows=5)
        layout.separator()
        layout.prop(context.scene, "udk_option_scale")
        layout.prop(context.scene, "udk_option_rebuildobjects")
        # layout.prop(context.scene, "udk_option_ignoreactiongroupnames")

        row11 = layout.row()
        row11.operator("object.udk_export")
        row11.operator("object.toggle_console")
        layout.operator(OBJECT_OT_UTRebuildArmature.bl_idname)
        layout.label(text="Mesh")
        layout.operator(OBJECT_OT_MeshClearWeights.bl_idname)
        layout.operator(OBJECT_OT_UTSelectedFaceSmooth.bl_idname)
        layout.operator(OBJECT_OT_UTRebuildMesh.bl_idname)
        layout.operator(OBJECT_OT_UDKCheckMeshLines.bl_idname)


def udkupdateobjects():
    my_objlist = bpy.context.scene.udkArm_list
    objectl = []
    for objarm in bpy.context.scene.objects:  # list and filter only mesh and armature
        if objarm.type == 'ARMATURE':
            objectl.append(objarm)

    for _objd in objectl:  # check if list has in udk list
        bfound_obj = False
        for _obj in my_objlist:
            if _obj.name == _objd.name and _obj.otype == _objd.type:
                _obj.bselect = _objd.select
                bfound_obj = True
                break

        if bfound_obj is False:
            # print("ADD ARMATURE...")
            my_item = my_objlist.add()
            my_item.name = _objd.name
            my_item.bselect = _objd.select
            my_item.otype = _objd.type
    removeobject = []
    for _udkobj in my_objlist:
        bfound_objv = False

        for _objd in bpy.context.scene.objects:  # check if there no existing object from sense to remove it
            if _udkobj.name == _objd.name and _udkobj.otype == _objd.type:
                bfound_objv = True
                break

        if bfound_objv is False:
            removeobject.append(_udkobj)
    # print("remove check...")
    for _item in removeobject:  # loop remove object from udk list object
        count = 0
        for _obj in my_objlist:
            if _obj.name == _item.name and _obj.otype == _item.otype:
                my_objlist.remove(count)
                break
            count += 1

    my_objlist = bpy.context.scene.udkmesh_list
    objectl = []
    for objarm in bpy.context.scene.objects:  # list and filter only mesh and armature
        if objarm.type == 'MESH':
            objectl.append(objarm)
    for _objd in objectl:  # check if list has in udk list
        bfound_obj = False
        for _obj in my_objlist:
            if _obj.name == _objd.name and _obj.otype == _objd.type:
                _obj.bselect = _objd.select
                bfound_obj = True
                break
        if bfound_obj is False:
            my_item = my_objlist.add()
            my_item.name = _objd.name
            my_item.bselect = _objd.select
            my_item.otype = _objd.type
    removeobject = []
    for _udkobj in my_objlist:
        bfound_objv = False
        for _objd in bpy.context.scene.objects:  # check if there no existing object from sense to remove it
            if _udkobj.name == _objd.name and _udkobj.otype == _objd.type:
                bfound_objv = True
                break
        if bfound_objv is False:
            removeobject.append(_udkobj)
    # print("remove check...")
    for _item in removeobject:  # loop remove object from udk list object
        count = 0
        for _obj in my_objlist:
            if _obj.name == _item.name and _obj.otype == _item.otype:
                my_objlist.remove(count)
                break
            count += 1


class OBJECT_OT_UDKObjUpdate(Operator):
    """This will update the filter of the mesh and armature"""
    bl_idname = "object.selobjectpdate"
    bl_label = "Update Object(s)"

    actionname = bpy.props.StringProperty()

    def execute(self, context):
        udkupdateobjects()
        return{'FINISHED'}


def udkcheckmeshline():
    objmesh = None
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH' and obj.select is True:
            objmesh = obj

    objmesh = triangulate_mesh(objmesh)  # create a copy of the mesh
    bpy.ops.object.mode_set(mode='OBJECT')

    for i in bpy.context.scene.objects:
        i.select = False  # deselect all objects

    objmesh.select = True
    bpy.context.scene.objects.active = objmesh  # set active mesh
    wedges = ObjMap()
    points = ObjMap()
    bpy.ops.object.mode_set(mode='EDIT')  # set in edit mode
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.context.tool_settings.mesh_select_mode = (True, False, False)  # select vertices

    if objmesh is not None:
        print("found mesh")
        print(objmesh)
        print(objmesh.data.tessfaces)
        vertex_list = []
        for face in objmesh.data.tessfaces:
            wedge_list = []
            vect_list = []
            for i in range(3):
                vert_index = face.vertices[i]
                vert = objmesh.data.vertices[vert_index]
                vect_list.append(FVector(vert.co.x, vert.co.y, vert.co.z))
                vpos = objmesh.matrix_local * vert.co
                p = VPoint()
                p.Point.X = vpos.x
                p.Point.Y = vpos.y
                p.Point.Z = vpos.z
                w = VVertex()
                w.PointIndex = points.get(p)  # store keys
                index_wedge = wedges.get(w)
                wedge_list.append(index_wedge)
            no = face.normal
            norm = FVector(no[0], no[1], no[2])
            tnorm = vect_list[1].sub(vect_list[0]).cross(vect_list[2].sub(vect_list[1]))
            dot = norm.dot(tnorm)

            tri = VTriangle()
            if dot > 0:
                (tri.WedgeIndex2, tri.WedgeIndex1, tri.WedgeIndex0) = wedge_list
            elif dot < 0:
                (tri.WedgeIndex0, tri.WedgeIndex1, tri.WedgeIndex2) = wedge_list
            else:
                dindex0 = face.vertices[0]
                dindex1 = face.vertices[1]
                dindex2 = face.vertices[2]
                vertex_list.append(dindex0)
                vertex_list.append(dindex1)
                vertex_list.append(dindex2)

        bpy.ops.object.mode_set(mode='OBJECT')
        for vertex in objmesh.data.vertices:  # loop all vertex in the mesh list
            for vl in vertex_list:      # loop for error vertex
                if vertex.index == vl:  # if match set to select
                    vertex.select = True
                    break
        bpy.ops.object.mode_set(mode='EDIT')  # set in edit mode to see the select vertex
        objmesh.data.update()       # update object
        bpy.context.scene.update()  # update scene
        message = "MESH PASS"
        if len(vertex_list) > 0:
            message = "MESH FAIL"
    return message


class OBJECT_OT_UDKCheckMeshLines(Operator):
    """Select the mesh for export test. This will create dummy mesh to see which area are broken. """ \
    """If the vertices share the same position it will cause a bug"""
    bl_idname = "object.udkcheckmeshline"
    bl_label = "Check Mesh Vertices"

    def execute(self, context):
        message = udkcheckmeshline()
        self.report({'ERROR'}, message)
        return{'FINISHED'}


class OBJECT_OT_ActionSetAnimUpdate(Operator):
    """Select Armture to match the action set groups. """ \
    """All bones keys must be set to match with number of bones"""
    bl_idname = "action.setanimupdate"
    bl_label = "Update Action Set(s)"

    actionname = bpy.props.StringProperty()

    def execute(self, context):
        my_sett = bpy.context.scene.udkas_list

        bones = []
        armature = None
        armatures = []
        armatureselected = []
        for objarm in bpy.context.scene.objects:
            if objarm.type == 'ARMATURE':
                # print("ADDED ARMATURE...")
                armatures.append(objarm)
                if objarm.select is True:
                    armatureselected.append(objarm)

        if len(armatureselected) == len(armatures) == 1:
            armature = armatures[0]
        if len(armatures) == 1:
            armature = armatures[0]
        if len(armatureselected) == 1:
            armature = armatureselected[0]

        if armature is not None:
            for bone in armature.pose.bones:
                bones.append(bone.name)

        for action in bpy.data.actions:  # action list
            bfound = False
            count = 0
            for actionbone in action.groups:
                # print("Pose bone name: ",actionbone.name)
                for b in bones:
                    if b == actionbone.name:
                        count += 1
                        # print(b," : ",actionbone.name)
                        break
            for actionlist in my_sett:
                if action.name == actionlist.name:
                    bactionfound = True
                    if len(bones) == len(action.groups) == count:
                        actionlist.bmatch = True
                    else:
                        actionlist.bmatch = False
                    bfound = True
                    break
            if bfound is not True:
                my_item = my_sett.add()
                # print(dir(my_item.bmatch))
                my_item.name = action.name
                # my_item.template_list_controls = "bmatch:bexport"
                if len(bones) == len(action.groups) == count:
                    my_item.bmatch = True
                else:
                    my_item.bmatch = False
        removeactions = []
        # check action list and data actions
        for actionlist in bpy.context.scene.udkas_list:
            bfind = False
            notfound = 0
            for act in bpy.data.actions:
                if actionlist.name == act.name:
                    bfind = True
                else:
                    notfound += 1
            # print("ACT NAME:",actionlist.name," COUNT",notfound)
            if notfound == len(bpy.data.actions):
                # print("remove :",actionlist.name)
                removeactions.append(actionlist.name)
        # print("Not in the action data list:",len(removeactions))
        # remove list or chnages in the name the template list
        for actname in removeactions:
            actioncount = 0
            for actionlist in my_sett:
                # print("action name:",actionlist.name)
                if actionlist.name == actname:
                    my_sett.remove(actioncount)
                    break
                actioncount += 1
        return{'FINISHED'}


class ExportUDKAnimData(Operator):
    """Export Skeleton Mesh / Animation Data file(s). """ \
    """One mesh and one armature else select one mesh or armature to be exported"""
    bl_idname = "export_anim.udk"  # this is important since its how bpy.ops.export.udk_anim_data is constructed
    bl_label = "Export PSK/PSA"

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_glob = StringProperty(
            default="*.psk;*.psa",
            options={'HIDDEN'},
            )
    udk_option_scale = FloatProperty(
            name="UDK Scale",
            description="In case you don't want to scale objects manually - "
                        "This will just scale position when on export for the skeleton mesh and animation data",
            default=1
            )
    udk_option_rebuildobjects = BoolProperty(
            name="Rebuild Objects",
            description="In case of deform skeleton mesh and animations data - "
                        "This will rebuild objects from raw format on export when checked",
            default=False
            )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        scene = bpy.context.scene
        scene.udk_option_export_psk = (scene.udk_option_export == '0' or scene.udk_option_export == '2')
        scene.udk_option_export_psa = (scene.udk_option_export == '1' or scene.udk_option_export == '2')
        bpy.context.scene.udk_option_scale = self.udk_option_scale
        bpy.context.scene.udk_option_rebuildobjects = self.udk_option_rebuildobjects

        filepath = get_dst_path()

        # cache settings
        restore_frame = scene.frame_current

        message = "Finish Export!"
        try:
            export(filepath)

        except Error as err:
            print(err.message)
            message = err.message

        # restore settings
        scene.frame_set(restore_frame)

        self.report({'WARNING', 'INFO'}, message)
        return {'FINISHED'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.prop(scene, "udk_option_smoothing_groups")
        layout.prop(scene, "udk_option_clamp_uv")
        layout.prop(scene, "udk_option_verbose")
        layout.prop(scene, "udk_option_filename_src")
        layout.prop(scene, "udk_option_export")
        layout.prop(self, "udk_option_scale")
        layout.prop(self, "udk_option_rebuildobjects")

    def invoke(self, context, event):
        self.udk_option_scale = bpy.context.scene.udk_option_scale
        self.udk_option_rebuildobjects = bpy.context.scene.udk_option_rebuildobjects

        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


def menu_func(self, context):
    default_path = os.path.splitext(bpy.data.filepath)[0] + ".psk"
    self.layout.operator(ExportUDKAnimData.bl_idname,
                         text="Skeleton Mesh / Animation Data (.psk/.psa)").filepath = default_path


# Add-ons Preferences Update Panel

# Define Panel classes for updating
panels = (
        Panel_UDKExport,
        )


def update_panel(self, context):
    message = "Export Unreal Engine Format(.psk/.psa): Updating Panel locations has failed"
    try:
        for panel in panels:
            if "bl_rna" in panel.__dict__:
                bpy.utils.unregister_class(panel)

        for panel in panels:
            panel.bl_category = context.user_preferences.addons[__name__].preferences.category
            bpy.utils.register_class(panel)

    except Exception as e:
        print("\n[{}]\n{}\n\nError:\n{}".format(__name__, message, e))
        pass


class PskAddonPreferences(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    category = StringProperty(
            name="Tab Category",
            description="Choose a name for the category of the panel",
            default="File I/O",
            update=update_panel
            )

    def draw(self, context):
        layout = self.layout

        row = layout.row()
        col = row.column()
        col.label(text="Tab Category:")
        col.prop(self, "category", text="")


# ===========================================================================
# Entry
# ===========================================================================
def register():

    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_export.append(menu_func)
    update_panel(None, bpy.context)

    # Added by [MGVS]
    bpy.types.Scene.udk_option_filename_src = EnumProperty(
            name="Filename",
            description="Sets the name for the files",
            items=[
                  ('0', "From object", "Name will be taken from object name"),
                  ('1', "From Blend", "Name will be taken from .blend file name")
                  ],
            default='0'
            )
    bpy.types.Scene.udk_option_export_psk = BoolProperty(
            name="bool export psa",
            description="Boolean for exporting psk format (Skeleton Mesh)",
            default=True
            )
    bpy.types.Scene.udk_option_export_psa = BoolProperty(
            name="bool export psa",
            description="Boolean for exporting psa format (Animation Data)",
            default=True
            )
    bpy.types.Scene.udk_option_clamp_uv = BoolProperty(
            name="Clamp UV",
            description="True is to limit Clamp UV co-ordinates to [0-1]. False is unrestricted (x,y)",
            default=False
            )
    bpy.types.Scene.udk_copy_merge = BoolProperty(
            name="Merge Mesh",
            description="This will copy the mesh(s) and merge the object together "
                        "and unlink the mesh to be remove while exporting the object",
            default=False
            )
    bpy.types.Scene.udk_option_export = EnumProperty(
            name="Export",
            description="What to export",
            items=[
                  ('0', "Mesh only", "Exports the PSK file for the Skeletal Mesh"),
                  ('1', "Animation only", "Export the PSA file for Action Set(s)(Animations Data)"),
                  ('2', "Mesh & Animation", "Export both PSK and PSA files(Skeletal Mesh/Animation(s) Data)")
                  ],
            default='2'
            )
    bpy.types.Scene.udk_option_verbose = BoolProperty(
            name="Verbose",
            description="Verbose console output",
            default=False
            )
    bpy.types.Scene.udk_option_smoothing_groups = BoolProperty(
            name="Smooth Groups",
            description="Activate hard edges as smooth groups",
            default=True
            )
    bpy.types.Scene.udk_option_triangulate = BoolProperty(
            name="Triangulate Mesh",
            description="Convert Quads to Triangles",
            default=False
            )
    bpy.types.Scene.udk_option_selectanimations = BoolProperty(
            name="Select Animation(s)",
            description="Select animation(s) for export to psa file",
            default=False
            )
    bpy.types.Scene.udk_option_selectobjects = BoolProperty(
            name="Select Object(s)",
            description="Select Armature and Mesh(s). Just make sure mesh(s) is parent to armature",
            default=False
            )
    bpy.types.Scene.udk_option_rebuildobjects = BoolProperty(
            name="Rebuild Objects",
            description="In case of deform skeleton mesh and animations data - "
                        "This will rebuild objects from raw format on export when checked",
            default=False
            )
    bpy.types.Scene.udk_option_ignoreactiongroupnames = BoolProperty(
            name="Ignore Action Group Names",
            description="This will Ignore Action Set Group Names Check With Armature Bones. "
                        "It will override armature to set action set",
            default=False
            )
    bpy.types.Scene.udk_option_scale = FloatProperty(
            name="UDK Scale",
            description="In case you don't want to scale objects manually - "
                        "This will just scale position when on export for the skeleton mesh and animation data",
            default=1
            )
    bpy.types.Scene.udkas_list = CollectionProperty(
                                        type=UDKActionSetListPG
                                        )
    bpy.types.Scene.udkas_list_idx = IntProperty()
    bpy.types.Scene.udkobj_list = CollectionProperty(
                                        type=UDKObjListPG
                                        )
    bpy.types.Scene.udkobj_list_idx = IntProperty()
    bpy.types.Scene.udkmesh_list = CollectionProperty(
                                        type=UDKMeshListPG
                                        )
    bpy.types.Scene.udkmesh_list_idx = IntProperty()
    bpy.types.Scene.udkArm_list = CollectionProperty(
                                        type=UDKArmListPG
                                        )
    bpy.types.Scene.udkArm_list_idx = IntProperty()


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_export.remove(menu_func)

    del bpy.types.Scene.udk_option_filename_src
    del bpy.types.Scene.udk_option_export_psk
    del bpy.types.Scene.udk_option_export_psa
    del bpy.types.Scene.udk_option_clamp_uv
    del bpy.types.Scene.udk_copy_merge
    del bpy.types.Scene.udk_option_export
    del bpy.types.Scene.udk_option_verbose
    del bpy.types.Scene.udk_option_smoothing_groups
    del bpy.types.Scene.udk_option_triangulate
    del bpy.types.Scene.udk_option_selectanimations
    del bpy.types.Scene.udk_option_selectobjects
    del bpy.types.Scene.udk_option_rebuildobjects
    del bpy.types.Scene.udk_option_ignoreactiongroupnames
    del bpy.types.Scene.udk_option_scale
    del bpy.types.Scene.udkas_list
    del bpy.types.Scene.udkas_list_idx
    del bpy.types.Scene.udkobj_list
    del bpy.types.Scene.udkobj_list_idx
    del bpy.types.Scene.udkmesh_list
    del bpy.types.Scene.udkmesh_list_idx
    del bpy.types.Scene.udkArm_list
    del bpy.types.Scene.udkArm_list_idx


if __name__ == "__main__":
    print(header("UDK Export PSK/PSA 2.6", 'CENTER'))
    register()
