#!BPY

"""
Name: 'Cal3D Exporter V0.7'
Blender: 234
Group: 'Export'
Tip: 'Export armature/bone data to the Cal3D library.'
"""

__author__ = ["Jean-Baptiste Lamy (Jiba)", "Chris Montijin", "Damien McGinnes"]
__url__ = ("blender", "elysiun", "Cal3D, http://cal3d.sf.net")
__version__ = "0.7"

__bpydoc__ = """\
This script exports armature / bone data to the well known open source Cal3D
library.

Usage:

Simply run the script to export available armatures.

Supported:<br>
    Cal3D versions 0.7 -> 0.9.

Known issues:<br>
    Material color is not supported yet;<br>
    Cal3D springs (for clothes and hair) are not supported yet;<br>
    Cal3d has a bug in that a cycle that doesn't have a root bone channel
will segfault cal3d.  Until cal3d supports this, add a keyframe for the
root bone;<br>
    When you finish an animation and run the script you can get an error
(something with KeyError). Just save your work and reload the model. This is
usually caused by deleted items hanging around;<br>
    If a vertex is assigned to one or more bones, but has for each bone a
weight of zero, there used to be a subdivision by zero error somewhere.  As a
workaround, if sum is 0.0 then sum becomes 1.0.  It's recommended that you give
weights to all bones to avoid problems.

Notes:<br>
    Objects/bones/actions whose names start by "_" are not exported so call IK
and null bones _LegIK, for example;<br>
    All your armature's exported bones must be connected to another bone
    (except for the root bone). Contrary to Blender, Cal3D doesn't support
"floating" bones.<br>
    Actions that start with '@' will be exported as actions, others will be
exported as cycles.
"""

# $Id$
#
# Copyright (C) 2003 Jean-Baptiste LAMY -- jiba@tuxfamily.org
# Copyright (C) 2004 Chris Montijin
# Copyright (C) 2004 Damien McGinnes
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


# This script is a Blender 2.34 => Cal3D 0.7/0.8/0.9 converter.
# (See http://blender.org and http://cal3d.sourceforge.net)
#

# This script was written by Jiba, modified by Chris and later modified by Damien

# Changes:
#
# 0.7 Damien McGinnes <mcginnes at netspeed com au>
#    Added NLA functionality for IPOs - this simplifies
#      the animation export and speeds it up significantly
#      it also removes the constraints on channel names -
#      they no longer have to match the bone or action and
#      .L .R etc are supported
#    bones starting with _ are not exported
#    textures no longer flipped vertically
#    fixed a filename bug for .csf and .cfg
#    actions that are prefixed with '@' go into the cfg file
#      as actions rather than cycles    
#    works with baked IK actions, unbaked ones wont work well
#      because you wont have the constraints evaluated
#    added an FPS slider into the gui
#    added registry saving for gui state. 
#    
# 0.6 Chris Montjin
#    Updated for Blender 2.32, 2.33
#    added basic GUI
#    generally improved flexibility
#
# 0.5 Jiba <jiba@tuxfamily.org>
#    Initial Release for Blender 2.28



# HOW TO USE :
# 1 - load the script in Blender's text editor
# 2 - type M-P (meta/alt + P) and wait until script execution is finished
# or install it in .scripts and access from the export menu

# ADVICE
# - Objects/bones/actions whose names start by "_" are not exported
#   so call IK and null bones _LegIK for example
# - All your armature's exported bones must be connected to another bone (except
#   for the rootbone). Contrary to Blender, Cal3D doesn't support "floating" bones.
# - Actions that start with '@' will be exported as actions, others will be
#   exported as cycles

# BUGS / TODO :
# - Material color is not supported yet
# - Cal3D springs (for clothes and hair) are not supported yet
# - Cal3d has a bug in that a cycle that doesnt have as rootbone channel
#    will segfault cal3d. until cal3d supports this, add a keyframe for the rootbone


# REMARKS
# 1. When you finished an animation and run the script
#    you can get an error (something with KeyError). Just save your work,
#    and reload the model. This is usualy caused by deleted items hanging around
# 2. If a vertex is assigned to one or more bones, but is has a for each
#    bone a weight of zero, there was a subdivision by zero somewhere
#    Made a workaround (if sum is 0.0 then sum becomes 1.0).
#    I have not checked what the outcome of that is, so you better nail 'm,
#    and give it some weight...


# Parameters :

# The directory where the data are saved.
SAVE_TO_DIR = "/tmp/tutorial/"

# Delete all existing Cal3D files in directory?
DELETE_ALL_FILES = 0

# What do you wanna export? If all are true then a .cfg file is created,
# otherwise no .cfg file is made. You have to make one by hand.
EXPORT_SKELETON = 1
EXPORT_ANIMATION = 0
EXPORT_MESH = 1
EXPORT_MATERIAL = 0

# Prefix for all created files
FILE_PREFIX = "Test"

# Remove path from imagelocation
REMOVE_PATH_FROM_IMAGE = 0

# prefix or subdir for imagepathname (if you place your textures in a
# subdir or just need a prefix or something). Only used when
# REMOVE_PATH_FROM_IMAGE = 1. Set to "" if none.
IMAGE_PREFIX = "textures/"

# Export to new (>= 900) Cal3D XML-format
EXPORT_TO_XML = 0

# Set scalefactor for model
SCALE = 0.5

# frames per second - used to convert blender frames to times
FPS = 25

# Use this dictionary to rename animations, as their name is lost at the 
# exportation.
RENAME_ANIMATIONS = {
  # "OldName" : "NewName",
  
  }

# True (=1) to export for the Soya 3D engine 
# (http://oomadness.tuxfamily.org/en/soya).
# (=> rotate meshes and skeletons so as X is right, Y is top and -Z is front)
EXPORT_FOR_SOYA = 0

# See also BASE_MATRIX below, if you want to rotate/scale/translate the model at
# the exportation.


# Enables LODs computation. LODs computation is quite slow, and the algo is 
# surely not optimal :-(
LODS = 0

#remove the word '.BAKED' from exported baked animations
REMOVE_BAKED = 1


################################################################################
# Code starts here.
# The script should be quite re-useable for writing another Blender animation 
# exporter. Most of the hell of it is to deal with Blender's head-tail-roll 
# bone's definition.

import sys, os, os.path, struct, math, string
import Blender
from Blender.BGL import *
from Blender.Draw import *
from Blender.Armature import *
from Blender.Registry import *

# HACK -- it seems that some Blender versions don't define sys.argv,
# which may crash Python if a warning occurs.

if not hasattr(sys, "argv"): sys.argv = ["???"]


# Math stuff

def quaternion2matrix(q):
  xx = q[0] * q[0]
  yy = q[1] * q[1]
  zz = q[2] * q[2]
  xy = q[0] * q[1]
  xz = q[0] * q[2]
  yz = q[1] * q[2]
  wx = q[3] * q[0]
  wy = q[3] * q[1]
  wz = q[3] * q[2]
  return [
    [1.0 - 2.0 * (yy + zz), 2.0 * (xy + wz), 2.0 * (xz - wy), 0.0],
    [2.0 * (xy - wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz + wx), 0.0],
    [2.0 * (xz + wy), 2.0 * (yz - wx), 1.0 - 2.0 * (xx + yy), 0.0],
    [0.0, 0.0, 0.0, 1.0]
    ]

def matrix2quaternion(m):
  s = math.sqrt(abs(m[0][0] + m[1][1] + m[2][2] + m[3][3]))
  if s == 0.0:
    x = abs(m[2][1] - m[1][2])
    y = abs(m[0][2] - m[2][0])
    z = abs(m[1][0] - m[0][1])
    if   (x >= y) and (x >= z): return 1.0, 0.0, 0.0, 0.0
    elif (y >= x) and (y >= z): return 0.0, 1.0, 0.0, 0.0
    else:                       return 0.0, 0.0, 1.0, 0.0
  return quaternion_normalize([
    -(m[2][1] - m[1][2]) / (2.0 * s),
    -(m[0][2] - m[2][0]) / (2.0 * s),
    -(m[1][0] - m[0][1]) / (2.0 * s),
    0.5 * s,
    ])

def quaternion_normalize(q):
  l = math.sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3])
  return q[0] / l, q[1] / l, q[2] / l, q[3] / l

def quaternion_multiply(q1, q2):
  r = [
    q2[3] * q1[0] + q2[0] * q1[3] + q2[1] * q1[2] - q2[2] * q1[1],
    q2[3] * q1[1] + q2[1] * q1[3] + q2[2] * q1[0] - q2[0] * q1[2],
    q2[3] * q1[2] + q2[2] * q1[3] + q2[0] * q1[1] - q2[1] * q1[0],
    q2[3] * q1[3] - q2[0] * q1[0] - q2[1] * q1[1] - q2[2] * q1[2],
    ]
  d = math.sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3])
  if d == 0:
    r[0] = d
    r[1] = d
    r[2] = d
    r[3] = d
  else:
    r[0] /= d
    r[1] /= d
    r[2] /= d
    r[3] /= d
  return r

def matrix_translate(m, v):
  m[3][0] += v[0]
  m[3][1] += v[1]
  m[3][2] += v[2]
  return m

def matrix_multiply(b, a):
  return [ [
    a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0],
    a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1],
    a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2],
    0.0,
    ], [
    a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0],
    a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1],
    a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2],
    0.0,
    ], [
    a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0],
    a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1],
    a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2],
     0.0,
    ], [
    a[3][0] * b[0][0] + a[3][1] * b[1][0] + a[3][2] * b[2][0] + b[3][0],
    a[3][0] * b[0][1] + a[3][1] * b[1][1] + a[3][2] * b[2][1] + b[3][1],
    a[3][0] * b[0][2] + a[3][1] * b[1][2] + a[3][2] * b[2][2] + b[3][2],
    1.0,
    ] ]

def matrix_invert(m):
  det = (m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
       - m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2])
       + m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]))
  if det == 0.0: return None
  det = 1.0 / det
  r = [ [
      det * (m[1][1] * m[2][2] - m[2][1] * m[1][2]),
    - det * (m[0][1] * m[2][2] - m[2][1] * m[0][2]),
      det * (m[0][1] * m[1][2] - m[1][1] * m[0][2]),
      0.0,
    ], [
    - det * (m[1][0] * m[2][2] - m[2][0] * m[1][2]),
      det * (m[0][0] * m[2][2] - m[2][0] * m[0][2]),
    - det * (m[0][0] * m[1][2] - m[1][0] * m[0][2]),
      0.0
    ], [
      det * (m[1][0] * m[2][1] - m[2][0] * m[1][1]),
    - det * (m[0][0] * m[2][1] - m[2][0] * m[0][1]),
      det * (m[0][0] * m[1][1] - m[1][0] * m[0][1]),
      0.0,
    ] ]
  r.append([
    -(m[3][0] * r[0][0] + m[3][1] * r[1][0] + m[3][2] * r[2][0]),
    -(m[3][0] * r[0][1] + m[3][1] * r[1][1] + m[3][2] * r[2][1]),
    -(m[3][0] * r[0][2] + m[3][1] * r[1][2] + m[3][2] * r[2][2]),
    1.0,
    ])
  return r

def matrix_rotate_x(angle):
  cos = math.cos(angle)
  sin = math.sin(angle)
  return [
    [1.0,  0.0, 0.0, 0.0],
    [0.0,  cos, sin, 0.0],
    [0.0, -sin, cos, 0.0],
    [0.0,  0.0, 0.0, 1.0],
    ]

def matrix_rotate_y(angle):
  cos = math.cos(angle)
  sin = math.sin(angle)
  return [
    [cos, 0.0, -sin, 0.0],
    [0.0, 1.0,  0.0, 0.0],
    [sin, 0.0,  cos, 0.0],
    [0.0, 0.0,  0.0, 1.0],
    ]

def matrix_rotate_z(angle):
  cos = math.cos(angle)
  sin = math.sin(angle)
  return [
    [ cos, sin, 0.0, 0.0],
    [-sin, cos, 0.0, 0.0],
    [ 0.0, 0.0, 1.0, 0.0],
    [ 0.0, 0.0, 0.0, 1.0],
    ]

def matrix_rotate(axis, angle):
  vx  = axis[0]
  vy  = axis[1]
  vz  = axis[2]
  vx2 = vx * vx
  vy2 = vy * vy
  vz2 = vz * vz
  cos = math.cos(angle)
  sin = math.sin(angle)
  co1 = 1.0 - cos
  return [
    [vx2 * co1 + cos, vx * vy * co1 + vz * sin, vz * vx * co1 - vy * sin, 0.0],
    [vx * vy * co1 - vz * sin, vy2 * co1 + cos, vy * vz * co1 + vx * sin, 0.0],
    [vz * vx * co1 + vy * sin, vy * vz * co1 - vx * sin, vz2 * co1 + cos, 0.0],
    [0.0, 0.0, 0.0, 1.0],
    ]

def matrix_scale(fx, fy, fz):
  return [
    [ fx, 0.0, 0.0, 0.0],
    [0.0,  fy, 0.0, 0.0],
    [0.0, 0.0,  fz, 0.0],
    [0.0, 0.0, 0.0, 1.0],
    ]
  
def point_by_matrix(p, m):
  return [p[0] * m[0][0] + p[1] * m[1][0] + p[2] * m[2][0] + m[3][0],
          p[0] * m[0][1] + p[1] * m[1][1] + p[2] * m[2][1] + m[3][1],
          p[0] * m[0][2] + p[1] * m[1][2] + p[2] * m[2][2] + m[3][2]]

def point_distance(p1, p2):
  return math.sqrt((p2[0] - p1[0]) ** 2 + \
         (p2[1] - p1[1]) ** 2 + (p2[2] - p1[2]) ** 2)

def vector_by_matrix(p, m):
  return [p[0] * m[0][0] + p[1] * m[1][0] + p[2] * m[2][0],
          p[0] * m[0][1] + p[1] * m[1][1] + p[2] * m[2][1],
          p[0] * m[0][2] + p[1] * m[1][2] + p[2] * m[2][2]]

def vector_length(v):
  return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])

def vector_normalize(v):
  l = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
  return v[0] / l, v[1] / l, v[2] / l

def vector_dotproduct(v1, v2):
  return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2]

def vector_crossproduct(v1, v2):
  return [
    v1[1] * v2[2] - v1[2] * v2[1],
    v1[2] * v2[0] - v1[0] * v2[2],
    v1[0] * v2[1] - v1[1] * v2[0],
    ]

def vector_angle(v1, v2):
  s = vector_length(v1) * vector_length(v2)
  f = vector_dotproduct(v1, v2) / s
  if f >=  1.0: return 0.0
  if f <= -1.0: return math.pi / 2.0
  return math.atan(-f / math.sqrt(1.0 - f * f)) + math.pi / 2.0

def blender_bone2matrix(head, tail, roll):
  # Convert bone rest state (defined by bone.head, bone.tail and bone.roll)
  # to a matrix (the more standard notation).
  # Taken from blenkernel/intern/armature.c in Blender source.
  # See also DNA_armature_types.h:47.
  
  target = [0.0, 1.0, 0.0]
  delta  = [tail[0] - head[0], tail[1] - head[1], tail[2] - head[2]]
  nor    = vector_normalize(delta)
  axis   = vector_crossproduct(target, nor)
  
  if vector_dotproduct(axis, axis) > 0.0000000000001:
    axis    = vector_normalize(axis)
    theta   = math.acos(vector_dotproduct(target, nor))
    bMatrix = matrix_rotate(axis, theta)
    
  else:
    if vector_crossproduct(target, nor) > 0.0: updown =  1.0
    else:                                      updown = -1.0
    
    # Quoted from Blender source : "I think this should work ..."
    bMatrix = [
      [updown, 0.0, 0.0, 0.0],
      [0.0, updown, 0.0, 0.0],
      [0.0, 0.0, 1.0, 0.0],
      [0.0, 0.0, 0.0, 1.0],
      ]
  
  rMatrix = matrix_rotate(nor, roll)
  return matrix_multiply(rMatrix, bMatrix)


# Cal3D data structures

CAL3D_VERSION = 700
CAL3D_XML_VERSION = 900

NEXT_MATERIAL_ID = 0
class Material:
  def __init__(self, map_filename = None):
    self.ambient_r = 255
    self.ambient_g = 255
    self.ambient_b = 255
    self.ambient_a = 255
    self.diffuse_r = 255
    self.diffuse_g = 255
    self.diffuse_b = 255
    self.diffuse_a = 255
    self.specular_r = 255
    self.specular_g = 255
    self.specular_b = 255
    self.specular_a = 255
    self.shininess = 1.0
    if map_filename:
      self.maps_filenames = [map_filename]
    else:
      self.maps_filenames = []
    
    MATERIALS[map_filename] = self
    
    global NEXT_MATERIAL_ID
    self.id = NEXT_MATERIAL_ID
    NEXT_MATERIAL_ID += 1
    
  def to_cal3d(self):
    s = "CRF\0" + struct.pack("iBBBBBBBBBBBBfi", CAL3D_VERSION,
      self.ambient_r, self.ambient_g, self.ambient_b, self.ambient_a,
      self.diffuse_r, self.diffuse_g, self.diffuse_b, self.diffuse_a,
      self.specular_r, self.specular_g, self.specular_b, self.specular_a,
      self.shininess, len(self.maps_filenames))
    for map_filename in self.maps_filenames:
      s += struct.pack("i", len(map_filename) + 1)
      s += map_filename + "\0"
    return s
  
  def to_cal3d_xml(self):
    s = "<HEADER MAGIC=\"XRF\" VERSION=\"%i\"/>\n" % CAL3D_XML_VERSION
    s += "  <MATERIAL NUMMAPS=\"%i\">\n" % len(self.maps_filenames)
    s += "  <AMBIENT>%f %f %f %f</AMBIENT>\n" % \
         (self.ambient_r, self.ambient_g, self.ambient_b, self.ambient_a)   
    s += "  <DIFFUSE>%f %f %f %f</DIFFUSE>\n" % \
         (self.diffuse_r, self.diffuse_g, self.diffuse_b, self.diffuse_a)
    s += "  <SPECULAR>%f %f %f %f</SPECULAR>\n" % \
         (self.specular_r, self.specular_g, self.specular_b, self.specular_a)
    s += "  <SHININESS>%f</SHININESS>\n" % self.shininess
    for map_filename in self.maps_filenames:
      s += "  <MAP>%s</MAP>\n" % map_filename
    s += "</MATERIAL>\n"
    return s
  
MATERIALS = {}

class Mesh:
  def __init__(self, name):
    self.name = name
    self.submeshes = []
    
    self.next_submesh_id = 0
    
  def to_cal3d(self):
    s = "CMF\0" + struct.pack("ii", CAL3D_VERSION, len(self.submeshes))
    s += "".join(map(SubMesh.to_cal3d, self.submeshes))
    return s
  
  def to_cal3d_xml(self):
    s = "<HEADER MAGIC=\"XMF\" VERSION=\"%i\"/>\n" % CAL3D_XML_VERSION
    s += "<MESH NUMSUBMESH=\"%i\">\n" % len(self.submeshes)
    s += "".join(map(SubMesh.to_cal3d_xml, self.submeshes))
    s += "</MESH>\n"
    return s
  
class SubMesh:
  def __init__(self, mesh, material):
    self.material = material
    self.vertices = []
    self.faces = []
    self.nb_lodsteps = 0
    self.springs = []
    
    self.next_vertex_id = 0
    
    self.mesh = mesh
    self.id = mesh.next_submesh_id
    mesh.next_submesh_id += 1
    mesh.submeshes.append(self)
    
  def compute_lods(self):
    """Computes LODs info for Cal3D (there's no Blender related stuff here)."""
    
    print "Start LODs computation..."
    vertex2faces = {}
    for face in self.faces:
      for vertex in (face.vertex1, face.vertex2, face.vertex3):
        l = vertex2faces.get(vertex)
        if not l:
          vertex2faces[vertex] = [face]
        else:
          l.append(face)
        
    couple_treated = {}
    couple_collapse_factor = []
    for face in self.faces:
      for a, b in ((face.vertex1, face.vertex2), (face.vertex1, face.vertex3),
                  (face.vertex2, face.vertex3)):
        a = a.cloned_from or a
        b = b.cloned_from or b
        if a.id > b.id:
          a, b = b, a
        if not couple_treated.has_key((a, b)):
          # The collapse factor is simply the distance between the 2 points :-(
          # This should be improved !!
          if vector_dotproduct(a.normal, b.normal) < 0.9:
            continue
          couple_collapse_factor.append((point_distance(a.loc, b.loc), a, b))
          couple_treated[a, b] = 1
      
    couple_collapse_factor.sort()
    
    collapsed = {}
    new_vertices = []
    new_faces = []
    for factor, v1, v2 in couple_collapse_factor:
      # Determines if v1 collapses to v2 or v2 to v1.
      # We choose to keep the vertex which is on the 
      # smaller number of faces, since
      # this one has more chance of being in an extrimity of the body.
      # Though heuristic, this rule yields very good results in practice.
      if len(vertex2faces[v1]) <  len(vertex2faces[v2]):
        v2, v1 = v1, v2
      elif len(vertex2faces[v1]) == len(vertex2faces[v2]):
        if collapsed.get(v1, 0): v2, v1 = v1, v2 # v1 already collapsed, try v2
        
      if (not collapsed.get(v1, 0)) and (not collapsed.get(v2, 0)):
        collapsed[v1] = 1
        collapsed[v2] = 1
        
        # Check if v2 is already collapsed
        while v2.collapse_to:
          v2 = v2.collapse_to
        
        common_faces = filter(vertex2faces[v1].__contains__, vertex2faces[v2])
        
        v1.collapse_to = v2
        v1.face_collapse_count = len(common_faces)
        
        for clone in v1.clones:
          # Find the clone of v2 that correspond to this clone of v1
          possibles = []
          for face in vertex2faces[clone]:
            possibles.append(face.vertex1)
            possibles.append(face.vertex2)
            possibles.append(face.vertex3)
          clone.collapse_to = v2
          for vertex in v2.clones:
            if vertex in possibles:
              clone.collapse_to = vertex
              break
            
          clone.face_collapse_count = 0
          new_vertices.append(clone)

        # HACK -- all faces get collapsed with v1 
        # (and no faces are collapsed with v1's
        # clones). This is why we add v1 in new_vertices after v1's clones.
        # This hack has no other incidence that consuming 
        # a little few memory for the
        # extra faces if some v1's clone are collapsed but v1 is not.
        new_vertices.append(v1)
        
        self.nb_lodsteps += 1 + len(v1.clones)
        
        new_faces.extend(common_faces)
        for face in common_faces:
          face.can_collapse = 1
          
          # Updates vertex2faces
          vertex2faces[face.vertex1].remove(face)
          vertex2faces[face.vertex2].remove(face)
          vertex2faces[face.vertex3].remove(face)
        vertex2faces[v2].extend(vertex2faces[v1])
        
    new_vertices.extend(filter(lambda vertex: not vertex.collapse_to, 
      self.vertices))
    new_vertices.reverse() # Cal3D want LODed vertices at the end
    for i in range(len(new_vertices)): new_vertices[i].id = i
    self.vertices = new_vertices
    
    new_faces.extend(filter(lambda face: not face.can_collapse, self.faces))
    new_faces.reverse() # Cal3D want LODed faces at the end
    self.faces = new_faces
    
    print "LODs computed : %s vertices can be removed (from a total of %s)." % \
          (self.nb_lodsteps, len(self.vertices))
    
  def rename_vertices(self, new_vertices):
    """Rename (change ID) of all vertices, such as self.vertices == 
       new_vertices.
    """
    for i in range(len(new_vertices)):
      new_vertices[i].id = i
    self.vertices = new_vertices
    
  def to_cal3d(self):
    s =  struct.pack("iiiiii", self.material.id, len(self.vertices), 
         len(self.faces), self.nb_lodsteps, len(self.springs),
         len(self.material.maps_filenames))
    s += "".join(map(Vertex.to_cal3d, self.vertices))
    s += "".join(map(Spring.to_cal3d, self.springs))
    s += "".join(map(Face.to_cal3d, self.faces))
    return s

  def to_cal3d_xml(self):
    s = "  <SUBMESH NUMVERTICES=\"%i\" NUMFACES=\"%i\" MATERIAL=\"%i\" " % \
        (len(self.vertices), len(self.faces), self.material.id)
    s += "NUMLODSTEPS=\"%i\" NUMSPRINGS=\"%i\" NUMTEXCOORDS=\"%i\">\n" % \
         (self.nb_lodsteps, len(self.springs),
         len(self.material.maps_filenames))
    s += "".join(map(Vertex.to_cal3d_xml, self.vertices))
    s += "".join(map(Spring.to_cal3d_xml, self.springs))
    s += "".join(map(Face.to_cal3d_xml, self.faces))
    s += "  </SUBMESH>\n"
    return s

class Vertex:
  def __init__(self, submesh, loc, normal):
    self.loc = loc
    self.normal = normal
    self.collapse_to = None
    self.face_collapse_count = 0
    self.maps = []
    self.influences = []
    self.weight = None
    
    self.cloned_from = None
    self.clones = []
    
    self.submesh = submesh
    self.id = submesh.next_vertex_id
    submesh.next_vertex_id += 1
    submesh.vertices.append(self)
    
  def to_cal3d(self):
    if self.collapse_to:
      collapse_id = self.collapse_to.id
    else:
      collapse_id = -1
    s = struct.pack("ffffffii", self.loc[0], self.loc[1], self.loc[2], 
        self.normal[0], self.normal[1], self.normal[2], collapse_id,
        self.face_collapse_count)
    s += "".join(map(Map.to_cal3d, self.maps))
    s += struct.pack("i", len(self.influences))
    s += "".join(map(Influence.to_cal3d, self.influences))
    if not self.weight is None:
      s += struct.pack("f", len(self.weight))
    return s
  
  def to_cal3d_xml(self):
    if self.collapse_to:
      collapse_id = self.collapse_to.id
    else:
      collapse_id = -1
    s = "    <VERTEX ID=\"%i\" NUMINFLUENCES=\"%i\">\n" % \
        (self.id, len(self.influences))
    s += "      <POS>%f %f %f</POS>\n" % (self.loc[0], self.loc[1], self.loc[2])
    s += "      <NORM>%f %f %f</NORM>\n" % \
         (self.normal[0], self.normal[1], self.normal[2])
    if collapse_id != -1:
      s += "      <COLLAPSEID>%i</COLLAPSEID>\n" % collapse_id
      s += "      <COLLAPSECOUNT>%i</COLLAPSECOUNT>\n" % \
           self.face_collapse_count
    s += "".join(map(Map.to_cal3d_xml, self.maps))
    s += "".join(map(Influence.to_cal3d_xml, self.influences))
    if not self.weight is None:
      s += "      <PHYSIQUE>%f</PHYSIQUE>\n" % len(self.weight)
    s += "    </VERTEX>\n"
    return s
  
class Map:
  def __init__(self, u, v):
    self.u = u
    self.v = v
    
  def to_cal3d(self):
    return struct.pack("ff", self.u, self.v)
    
  def to_cal3d_xml(self):
    return "      <TEXCOORD>%f %f</TEXCOORD>\n" % (self.u, self.v)
    
class Influence:
  def __init__(self, bone, weight):
    self.bone = bone
    self.weight = weight
    
  def to_cal3d(self):
    return struct.pack("if", self.bone.id, self.weight)
    
  def to_cal3d_xml(self):
    return "      <INFLUENCE ID=\"%i\">%f</INFLUENCE>\n" % \
           (self.bone.id, self.weight)
    
class Spring:
  def __init__(self, vertex1, vertex2):
    self.vertex1 = vertex1
    self.vertex2 = vertex2
    self.spring_coefficient = 0.0
    self.idlelength = 0.0
    
  def to_cal3d(self):
    return struct.pack("iiff", self.vertex1.id, self.vertex2.id,
           self.spring_coefficient, self.idlelength)

  def to_cal3d_xml(self):
    return "    <SPRING VERTEXID=\"%i %i\" COEF=\"%f\" LENGTH=\"%f\"/>\n" % \
           (self.vertex1.id, self.vertex2.id, self.spring_coefficient,
           self.idlelength)

class Face:
  def __init__(self, submesh, vertex1, vertex2, vertex3):
    self.vertex1 = vertex1
    self.vertex2 = vertex2
    self.vertex3 = vertex3
    
    self.can_collapse = 0
    
    self.submesh = submesh
    submesh.faces.append(self)
    
  def to_cal3d(self):
    return struct.pack("iii", self.vertex1.id, self.vertex2.id, self.vertex3.id)
    
  def to_cal3d_xml(self):
    return "    <FACE VERTEXID=\"%i %i %i\"/>\n" % \
           (self.vertex1.id, self.vertex2.id, self.vertex3.id)
    
class Skeleton:
  def __init__(self):
    self.bones = []
    
    self.next_bone_id = 0
    
  def to_cal3d(self):
    s = "CSF\0" + struct.pack("ii", CAL3D_VERSION, len(self.bones))
    s += "".join(map(Bone.to_cal3d, self.bones))
    return s

  def to_cal3d_xml(self):
    s = "<HEADER MAGIC=\"XSF\" VERSION=\"%i\"/>\n" % CAL3D_XML_VERSION
    s += "<SKELETON NUMBONES=\"%i\">\n" % len(self.bones)
    s += "".join(map(Bone.to_cal3d_xml, self.bones))
    s += "</SKELETON>\n"
    return s

BONES = {}

class Bone:
  def __init__(self, skeleton, parent, name, loc, rot):
    self.parent = parent
    self.name = name
    self.loc = loc
    self.rot = rot
    self.children = []
    
    self.matrix = matrix_translate(quaternion2matrix(rot), loc)
    if parent:
      self.matrix = matrix_multiply(parent.matrix, self.matrix)
      parent.children.append(self)
    
    # lloc and lrot are the bone => model space transformation 
    # (translation and rotation). They are probably specific to Cal3D.
    m = matrix_invert(self.matrix)
    self.lloc = m[3][0], m[3][1], m[3][2]
    self.lrot = matrix2quaternion(m)
    
    self.skeleton = skeleton
    self.id = skeleton.next_bone_id
    skeleton.next_bone_id += 1
    skeleton.bones.append(self)
    
    BONES[name] = self
    
  def to_cal3d(self):
    s = struct.pack("i", len(self.name) + 1) + self.name + "\0"
    
    # We need to negate quaternion W value, but why ?
    s += struct.pack("ffffffffffffff", self.loc[0], self.loc[1], self.loc[2],
         self.rot[0], self.rot[1], self.rot[2], -self.rot[3],
         self.lloc[0], self.lloc[1], self.lloc[2],
         self.lrot[0], self.lrot[1], self.lrot[2], -self.lrot[3])
    if self.parent:
      s += struct.pack("i", self.parent.id)
    else:
      s += struct.pack("i", -1)
    s += struct.pack("i", len(self.children))
    s += "".join(map(lambda bone: struct.pack("i", bone.id), self.children))
    return s
  
  def to_cal3d_xml(self):
    s = "  <BONE ID=\"%i\" NAME=\"%s\" NUMCHILDS=\"%i\">\n" % \
        (self.id, self.name, len(self.children))
    # We need to negate quaternion W value, but why ?
    s += "    <TRANSLATION>%f %f %f</TRANSLATION>\n" % \
         (self.loc[0], self.loc[1], self.loc[2])
    s += "    <ROTATION>%f %f %f %f</ROTATION>\n" % \
         (self.rot[0], self.rot[1], self.rot[2], -self.rot[3])
    s += "    <LOCALTRANSLATION>%f %f %f</LOCALTRANSLATION>\n" % \
         (self.lloc[0], self.lloc[1], self.lloc[2])
    s += "    <LOCALROTATION>%f %f %f %f</LOCALROTATION>\n" % \
         (self.lrot[0], self.lrot[1], self.lrot[2], -self.lrot[3])
    if self.parent:
      s += "    <PARENTID>%i</PARENTID>\n" % self.parent.id
    else:
      s += "    <PARENTID>%i</PARENTID>\n" % -1
    s += "".join(map(lambda bone: "    <CHILDID>%i</CHILDID>\n" % bone.id,
         self.children))
    s += "  </BONE>\n"
    return s
  
class Animation:
  def __init__(self, name, action, duration = 0.0):
    self.name = name
    self.action = action
    self.duration = duration
    self.tracks = {} # Map bone names to tracks
    
  def to_cal3d(self):
    s = "CAF\0" + struct.pack("ifi", 
        CAL3D_VERSION, self.duration, len(self.tracks))
    s += "".join(map(Track.to_cal3d, self.tracks.values()))
    return s
    
  def to_cal3d_xml(self):
    s = "<HEADER MAGIC=\"XAF\" VERSION=\"%i\"/>\n" % CAL3D_XML_VERSION
    s += "<ANIMATION DURATION=\"%f\" NUMTRACKS=\"%i\">\n" % \
         (self.duration, len(self.tracks))
    s += "".join(map(Track.to_cal3d_xml, self.tracks.values()))
    s += "</ANIMATION>\n"
    return s
    
class Track:
  def __init__(self, animation, bone):
    self.bone = bone
    self.keyframes = []
    
    self.animation = animation
    animation.tracks[bone.name] = self
    
  def to_cal3d(self):
    s = struct.pack("ii", self.bone.id, len(self.keyframes))
    s += "".join(map(KeyFrame.to_cal3d, self.keyframes))
    return s
    
  def to_cal3d_xml(self):
    s = "  <TRACK BONEID=\"%i\" NUMKEYFRAMES=\"%i\">\n" % \
        (self.bone.id, len(self.keyframes))
    s += "".join(map(KeyFrame.to_cal3d_xml, self.keyframes))
    s += "  </TRACK>\n"
    return s
    
class KeyFrame:
  def __init__(self, track, time, loc, rot):
    self.time = time
    self.loc = loc
    self.rot = rot
    
    self.track = track
    track.keyframes.append(self)
    
  def to_cal3d(self):
    # We need to negate quaternion W value, but why ?
    return struct.pack("ffffffff", self.time, self.loc[0], self.loc[1], 
           self.loc[2], self.rot[0], self.rot[1], self.rot[2], -self.rot[3])

  def to_cal3d_xml(self):
    s = "    <KEYFRAME TIME=\"%f\">\n" % self.time
    s += "      <TRANSLATION>%f %f %f</TRANSLATION>\n" % \
         (self.loc[0], self.loc[1], self.loc[2])
    # We need to negate quaternion W value, but why ?
    s += "      <ROTATION>%f %f %f %f</ROTATION>\n" % \
         (self.rot[0], self.rot[1], self.rot[2], -self.rot[3])
    s += "    </KEYFRAME>\n"
    return s


def export():
  global STATUS
  STATUS = "Start export..."
  Draw()

  # Hack for having the model rotated right.
  # Put in BASE_MATRIX your own rotation if you need some.

  if EXPORT_FOR_SOYA:
    BASE_MATRIX = matrix_rotate_x(-math.pi / 2.0)
  else:
    BASE_MATRIX = None

  # Get the scene
  
  scene = Blender.Scene.getCurrent()
  
  
  # Export skeleton (=armature)
  
  STATUS = "Calculate skeleton"
  Draw()

  skeleton = Skeleton()
  
  for obj in Blender.Object.Get():
    data = obj.getData()
    if type(data) is Blender.Types.ArmatureType:
      matrix = obj.getMatrix()
      if BASE_MATRIX: matrix = matrix_multiply(BASE_MATRIX, matrix)
      
      def treat_bone(b, parent = None):
        #skip bones that start with _
        #also skips children of that bone so be careful
        if b.getName()[0] == '_' : return
        head = b.getHead()
        tail = b.getTail()
        
        # Turns the Blender's head-tail-roll notation into a quaternion
        quat = matrix2quaternion(blender_bone2matrix(head, tail, b.getRoll()))
        
        if parent:
          # Compute the translation from the parent bone's head to the child
          # bone's head, in the parent bone coordinate system.
          # The translation is parent_tail - parent_head + child_head,
          # but parent_tail and parent_head must be converted from the parent's
          # parent system coordinate into the parent system coordinate.
          
          parent_invert_transform = matrix_invert(quaternion2matrix(parent.rot))
          parent_head = vector_by_matrix(parent.head, parent_invert_transform)
          parent_tail = vector_by_matrix(parent.tail, parent_invert_transform)
          
          bone = Bone(skeleton, parent, b.getName(), 
                 [parent_tail[0] - parent_head[0] + head[0], 
                  parent_tail[1] - parent_head[1] + head[1],
                  parent_tail[2] - parent_head[2] + head[2]], quat)
        else:
          # Apply the armature's matrix to the root bones
          head = point_by_matrix(head, matrix)
          tail = point_by_matrix(tail, matrix)
          quat = matrix2quaternion(matrix_multiply(matrix, 
                 quaternion2matrix(quat))) # Probably not optimal
          
          # Here, the translation is simply the head vector
          bone = Bone(skeleton, parent, b.getName(), head, quat)
          
        bone.head = head
        bone.tail = tail
        
        for child in b.getChildren():
          treat_bone(child, bone)
        
      for b in data.getBones():
        # treat this bone if not already treated as a child bone
        if not BONES.has_key(b.getName()):
        	treat_bone(b)
      
      # Only one armature / skeleton
      break
    
    
  # Export Mesh data
  
  if EXPORT_MESH or EXPORT_MATERIAL:

      STATUS = "Calculate mesh and materials"
      Draw()

      meshes = []
      
      for obj in Blender.Object.Get():
        data = obj.getData()
        if (type(data) is Blender.Types.NMeshType) and data.faces and EXPORT_MESH:
          mesh = Mesh(obj.name)

          if mesh.name[0] == '_' :
              print "skipping object ", mesh.name
              continue

          meshes.append(mesh)
          
          matrix = obj.getMatrix()
          if BASE_MATRIX:
            matrix = matrix_multiply(BASE_MATRIX, matrix)
          
          faces = data.faces
          while faces:
            image = faces[0].image
            image_filename = image and image.filename
            # for windows
            image_filename_t = str(image_filename)
            #print image_filename_t
            # end for windows
            if REMOVE_PATH_FROM_IMAGE:
              if image_filename_t == "None":
                print "Something wrong with material (is none), set name to none..."
                image_file = "none.tga"
              else:
                # for windows
                if image_filename_t[0] == "/":
                  tmplist = image_filename_t.split("/")
                else:
                  tmplist = image_filename_t.split("\\")
                #print "tmplist: " + repr(tmplist)
                image_file = IMAGE_PREFIX + tmplist[-1]          
                # end for windows
                # for linux
                # image_file = IMAGE_PREFIX + os.path.basename(image_filename)
            else:
              image_file = image_filename
            material = MATERIALS.get(image_file) or Material(image_file)
            
            # TODO add material color support here
            
            submesh = SubMesh(mesh, material)
            vertices = {}
            for face in faces[:]:
              if (face.image and face.image.filename) == image_filename:
                faces.remove(face)
                
                if not face.smooth:
                  #if len(face.v) < 3 :
                  #   print "mesh contains a dodgy face, skipping it"
                  #   continue
                  p1 = face.v[0].co
                  p2 = face.v[1].co
                  p3 = face.v[2].co
                  normal = vector_normalize(vector_by_matrix(vector_crossproduct(
                    [p3[0] - p2[0], p3[1] - p2[1], p3[2] - p2[2]],
                    [p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]],
                    ), matrix))
                  
                face_vertices = []
                for i in range(len(face.v)):
                  vertex = vertices.get(face.v[i].index)
                  if not vertex:
                    coord = point_by_matrix (face.v[i].co, matrix)
                    if face.smooth:
                      normal = vector_normalize(vector_by_matrix(face.v[i].no, 
                               matrix))
                    vertex = vertices[face.v[i].index] = Vertex(submesh, coord, 
                             normal)
                    
                    influences = data.getVertexInfluences(face.v[i].index)
                    if not influences:
                      print "Warning: vertex %i (%i) has no influence !" % \
                            (face.v[i].index, face.v[i].sel)
                    
                    # sum of influences is not always 1.0 in Blender ?!?!
                    sum = 0.0
                    for bone_name, weight in influences:
                      sum += weight
                    
                    # Select vertex with no weight at all (sum = 0.0).
                    # To find out which one it is, select part of vertices in mesh,
                    # exit editmode and see if value between brackets is 1. If so,
                    # the vertex is in selection. You can narrow the selection
                    # this way, to find the offending vertex...
                    if sum == 0.0:
                      print "Warning: vertex %i in mesh %s (selected: %i) has influence sum of 0.0!" % \
                            (face.v[i].index, mesh.name, face.v[i].sel)
                      print "Set the sum to 1.0, otherwise there will " + \
                            "be a division by zero. Better find the offending " + \
                            "vertex..."
                      # face.v[i].sel = 1 # does not work???
                      sum = 1.0
                    if face.v[i].sel:
                      print "Vertex %i is selected" % (face.v[i].index)

                    for bone_name, weight in influences:
                      #print "bone: %s, weight: %f, sum: %f" % (bone_name, weight, sum)
                      vertex.influences.append(Influence(BONES[bone_name], 
                                               weight / sum))
                      
                  elif not face.smooth:
                    # We cannot share vertex for non-smooth faces, 
                    # since Cal3D does not support vertex sharing 
                    # for 2 vertices with different normals.
                    # => we must clone the vertex.
                    
                    old_vertex = vertex
                    vertex = Vertex(submesh, vertex.loc, normal)
                    vertex.cloned_from = old_vertex
                    vertex.influences = old_vertex.influences
                    old_vertex.clones.append(vertex)
                    
                  if data.hasFaceUV():
                    uv = [face.uv[i][0], face.uv[i][1]] #1.0 - face.uv[i][1]]
                    if not vertex.maps:
                      vertex.maps.append(Map(*uv))
                    elif (vertex.maps[0].u != uv[0]) or (vertex.maps[0].v != uv[1]):
                      # This vertex can be shared for Blender, but not for Cal3D !!!
                      # Cal3D does not support vertex sharing for 2 vertices with
                      # different UV texture coodinates.
                      # => we must clone the vertex.
                      
                      for clone in vertex.clones:
                        if (clone.maps[0].u == uv[0]) and \
                           (clone.maps[0].v == uv[1]):
                          vertex = clone
                          break
                      else: # Not yet cloned...
                        old_vertex = vertex
                        vertex = Vertex(submesh, vertex.loc, vertex.normal)
                        vertex.cloned_from = old_vertex
                        vertex.influences = old_vertex.influences
                        vertex.maps.append(Map(*uv))
                        old_vertex.clones.append(vertex)
                        
                  face_vertices.append(vertex)
                  
                # Split faces with more than 3 vertices
                for i in range(1, len(face.v) - 1):
                  Face(submesh, face_vertices[0], face_vertices[i],
                       face_vertices[i + 1])
                  
            # Computes LODs info
            if LODS:
              submesh.compute_lods()
            
  # Export animations
              
  if EXPORT_ANIMATION:

      ipoCurveType = ['LocX', 'LocY', 'LocZ', 'QuatX', 'QuatY', 'QuatZ', 'QuatW']
      
      STATUS = "Calculate animations"
      Draw()

      ANIMATIONS = {}
      
      actions = Blender.Armature.NLA.GetActions()

      for a in actions:

        #skip actions beginning with _
        if a[0] == '_' : continue

        #create the animation object
        animation_name = a

        #if the name starts with @ then it is a oneshot action otherwise its a cycle
        if a[0] == '@':
           animation_name = a.split("@")[1]
           aact = 1
        else:
           aact = 0

        print "Animationname: %s" % (animation_name)

        if REMOVE_BAKED:
           tmp = animation_name.split('.BAKED')
           animation_name = "".join(tmp)

        #check for duplicate animation names and work around
        test = animation_name
        suffix = 1
        while ANIMATIONS.get(test):
           print "Warning %s already exists!! renaming" % animation_name
           test = "%s__%i" % (animation_name, suffix)
           suffix += 1
        animation_name = test
           
        animation = ANIMATIONS[animation_name] = Animation(animation_name, aact)

        ipos = actions[a].getAllChannelIpos()
        for bone_name in ipos:
          #skip bones that start with _
          if bone_name[0] == '_' :
             continue

          ipo = ipos[bone_name]
          try: nbez = ipo.getNBezPoints(0)
          except TypeError:
            print "No key frame for action %s, ipo %s, skipping..." % (a, bone_name)
            nbez = 0

          bone = BONES[bone_name]
          track = animation.tracks.get(bone_name)
          if not track:
            track = animation.tracks[bone_name] = Track(animation, bone)
            track.finished = 0
      
          curve = []
          for ctype in ipoCurveType:
            curve.append(ipo.getCurve(ctype))

          for bez in range(nbez):
            time1 = ipo.getCurveBeztriple(0, bez)[3]           
            time = (time1 - 1.0) / FPS
   
            if animation.duration < time:
              animation.duration = time

            loc = bone.loc
            rot = bone.rot
           
            if (curve[0]):
              trans = vector_by_matrix((
                curve[0].evaluate(time1),
                curve[1].evaluate(time1),
                curve[2].evaluate(time1)), bone.matrix)

              loc = [
                bone.loc[0] + trans[0],
                bone.loc[1] + trans[1],
                bone.loc[2] + trans[2]]

            if (curve[3]):
             
              ipo_rot = [
                curve[3].evaluate(time1),
                curve[4].evaluate(time1),
                curve[5].evaluate(time1),
                curve[6].evaluate(time1)]
              
              # We need to blend the rotation from the bone rest state 
              # (=bone.rot) with ipo_rot.
              
              rot = quaternion_multiply(ipo_rot, bone.rot)

            KeyFrame(track, time, loc, rot)
        
          
  # Save all data

  STATUS = "Save files"
  Draw()

  EXPORT_ALL = EXPORT_SKELETON and EXPORT_ANIMATION and \
               EXPORT_MESH and EXPORT_MATERIAL
  cfg_buffer = ""

  if FILE_PREFIX == "":
    std_fname = "cal3d"
  else:
    std_fname = ""
  
  if not os.path.exists(SAVE_TO_DIR):
    os.makedirs(SAVE_TO_DIR)
  else:
    if DELETE_ALL_FILES:
      for file in os.listdir(SAVE_TO_DIR):
        if file.endswith(".cfg") or file.endswith(".caf") or \
           file.endswith(".cmf") or file.endswith(".csf") or \
           file.endswith(".crf") or file.endswith(".xsf") or \
           file.endswith(".xaf") or file.endswith(".xmf") or \
           file.endswith(".xrf"):
           os.unlink(os.path.join(SAVE_TO_DIR, file))
        
  cfg_buffer += "# Cal3D model exported from Blender with blender2cal3d.py\n\n"
  if EXPORT_ALL:
    cfg_buffer += "# --- Scale of model ---\n"
    cfg_buffer += "scale=%f\n\n" % SCALE
  else:
    cfg_buffer += "# Append this file to the model configuration file\n\n"
  
  if EXPORT_SKELETON:
    cfg_buffer += "# --- Skeleton ---\n"
    if EXPORT_TO_XML:  
      open(os.path.join(SAVE_TO_DIR, FILE_PREFIX + std_fname + \
           os.path.basename(SAVE_TO_DIR) +".xsf"),
          "wb").write(skeleton.to_cal3d_xml())
      cfg_buffer += "skeleton=%s.xsf\n" %  (FILE_PREFIX + std_fname +\
               os.path.basename(SAVE_TO_DIR))
    else:
      open(os.path.join(SAVE_TO_DIR,  FILE_PREFIX + std_fname + \
           os.path.basename(SAVE_TO_DIR) + ".csf"),
          "wb").write(skeleton.to_cal3d())
      cfg_buffer += "skeleton=%s.csf\n" %  (FILE_PREFIX + std_fname +\
               os.path.basename(SAVE_TO_DIR))
    cfg_buffer += "\n"
  
  if EXPORT_ANIMATION:
    cfg_buffer += "# --- Animations ---\n"
    for animation in ANIMATIONS.values():
      # Cal3D does not support animation with only one state
      if animation.duration:
        animation.name = RENAME_ANIMATIONS.get(animation.name) or animation.name

        action_suffix=""
        if animation.action:
           action_suffix = "_action"

        if EXPORT_TO_XML:
          open(os.path.join(SAVE_TO_DIR, FILE_PREFIX + \
               animation.name + ".xaf"), "wb").write(animation.to_cal3d_xml())
          cfg_buffer += "animation%s=%s.xaf\n" % (action_suffix, (FILE_PREFIX + animation.name))
        else:
          open(os.path.join(SAVE_TO_DIR, FILE_PREFIX + \
               animation.name + ".caf"), "wb").write(animation.to_cal3d())
          cfg_buffer += "animation%s=%s.caf\n" % (action_suffix, (FILE_PREFIX + animation.name))

        # Prints animation names and durations
        print animation.name, "duration", animation.duration * FPS + 1.0
    cfg_buffer += "\n"

  if EXPORT_MESH:
    cfg_buffer += "# --- Meshes ---\n"
    for mesh in meshes:
      if not mesh.name.startswith("_"):
        if EXPORT_TO_XML:
          open(os.path.join(SAVE_TO_DIR, FILE_PREFIX + mesh.name + ".xmf"),
              "wb").write(mesh.to_cal3d_xml())
          cfg_buffer += "mesh=%s.xmf\n" % (FILE_PREFIX + mesh.name)
        else:
          open(os.path.join(SAVE_TO_DIR, FILE_PREFIX + mesh.name + ".cmf"),
              "wb").write(mesh.to_cal3d())
          cfg_buffer += "mesh=%s.cmf\n" % (FILE_PREFIX + mesh.name)
    cfg_buffer += "\n"
  
  if EXPORT_MATERIAL:
    cfg_buffer += "# --- Materials ---\n"
    materials = MATERIALS.values()
    materials.sort(lambda a, b: cmp(a.id, b.id))
    for material in materials:
      if material.maps_filenames:
        fname = os.path.splitext(os.path.basename(material.maps_filenames[0]))[0]
      else:
        fname = "plain"
      if EXPORT_TO_XML:
        open(os.path.join(SAVE_TO_DIR, FILE_PREFIX + fname + ".xrf"),
            "wb").write(material.to_cal3d_xml())
        cfg_buffer += "material=%s.xrf\n" % (FILE_PREFIX + fname)
      else:
        open(os.path.join(SAVE_TO_DIR, FILE_PREFIX + fname + ".crf"),
            "wb").write(material.to_cal3d())
        cfg_buffer += "material=%s.crf\n" % (FILE_PREFIX + fname)
    cfg_buffer += "\n"

  if EXPORT_ALL:
    cfg_prefix = ""
  else:
    cfg_prefix = "append_to_"

  cfg = open(os.path.join(SAVE_TO_DIR, cfg_prefix + FILE_PREFIX + std_fname +\
             os.path.basename(SAVE_TO_DIR) + ".cfg"), "wb")
  print >> cfg, cfg_buffer
  cfg.close()

  print "Saved to", SAVE_TO_DIR
  print "Done."

  STATUS = "Export finished."
  Draw()


# ::: GUI around the whole thing, not very clean, but it works for me...

_save_dir = Create(SAVE_TO_DIR)
_file_prefix = Create(FILE_PREFIX)
_image_prefix = Create(IMAGE_PREFIX)
_scale = Create(SCALE)
_framepsec = Create(FPS)
STATUS = "Done nothing yet"

def gui():
  global EXPORT_TO_XML, EXPORT_SKELETON, EXPORT_ANIMATION, EXPORT_MESH, \
         EXPORT_MATERIAL, SAVE_TO_DIR, _save_dir, _scale, SCALE, \
         EXPORT_FOR_SOYA, REMOVE_PATH_FROM_IMAGE, LODS, _file_prefix, \
         FILE_PREFIX, _image_prefix, IMAGE_PREFIX, DELETE_ALL_FILES, STATUS, \
         _framepsec, FPS

  glRasterPos2i(8, 14)
  Text("Status: %s" % STATUS)

  _export_button = Button("Export (E)", 1, 8, 36, 100, 20, 
                          "Start export to Cal3D format")
  _quit_button = Button("Quit (Q)", 5, 108, 36, 100, 20, "Exit from script")

  _delete_toggle = Toggle("X", 15, 8, 64, 20, 20, DELETE_ALL_FILES, 
         "Delete all existing Cal3D files in export directory")
  _SF_toggle = Toggle("_SF", 6, 28, 64, 45, 20, EXPORT_SKELETON, 
         "Export skeleton (CSF/XSF)")
  _AF_toggle = Toggle("_AF", 7, 73, 64, 45, 20, EXPORT_ANIMATION, 
         "Export animations (CAF/XAF)")
  _MF_toggle = Toggle("_MF", 8, 118, 64, 45, 20, EXPORT_MESH, 
         "Export mesh (CMF/XMF)")
  _RF_toggle = Toggle("_RF", 9, 163, 64, 45, 20, EXPORT_MATERIAL, 
         "Export materials (CRF/XRF)")

  _XML_toggle = Toggle("Export to XML", 2, 8, 84, 100, 20, EXPORT_TO_XML, 
         "Export to Cal3D XML or binary fileformat")
  _soya_toggle = Toggle("Export for Soya", 10, 108, 84, 100, 20, 
                        EXPORT_FOR_SOYA, "Export for Soya 3D Engine")

  _imagepath_toggle = Toggle("X imagepath", 11, 8, 104, 100, 20, 
                        REMOVE_PATH_FROM_IMAGE, "Remove path from imagename")
  _lods_toggle = Toggle("Calculate LODS", 12, 108, 104, 100, 20, 
                        LODS, "Calculate LODS, quit slow and not optimal")

  _scale = Slider("S:", 4, 8, 132, 100, 20, SCALE, 0.00, 10.00, 0, \
                  "Sets the scale of the model (small number will scale up)")

  _framepsec = Slider("F:", 16, 108, 132, 100, 20, FPS, 0.00, 100.0, 0, \
                  "Sets the export framerate (FPS)")

  _image_prefix = String("Image prefix: ", 13, 8, 160, 200, 20, IMAGE_PREFIX, \
                         256, "Prefix used for imagename (if you have the " + \
                         "textures in a subdirectory called textures, " + \
                         "the prefix would be \"textures\\\\\")")

  _file_prefix = String("File prefix: ", 14, 8, 180, 200, 20, FILE_PREFIX, \
                        256, "Prefix to all exported Cal3D files "+ \
                        "(f.e. \"model_\")")

  _save_dir = String("Export to: ", 3, 8, 200, 200, 20, _save_dir.val, 256, \
                     "Directory to save files to")



def event(evt, val):
  global STATUS

  if (evt == QKEY or evt == ESCKEY):
    Exit()
    return
  if evt == EKEY:
    update_reg()
    export()

def bevent(evt):
  global EXPORT_TO_XML, EXPORT_SKELETON, EXPORT_ANIMATION, EXPORT_MESH, \
         EXPORT_MATERIAL, _save_dir, SAVE_TO_DIR, _scale, SCALE, \
         EXPORT_FOR_SOYA, REMOVE_PATH_FROM_IMAGE, LODS, _file_prefix, \
         FILE_PREFIX, _image_prefix, IMAGE_PREFIX, DELETE_ALL_FILES, STATUS, \
         _framepsec, FPS

  if evt == 1:
    update_reg()
    export()
  if evt == 2:
    EXPORT_TO_XML = 1 - EXPORT_TO_XML
  if evt == 3:
    SAVE_TO_DIR = _save_dir.val
  if evt == 4:
    SCALE = _scale.val
  if evt == 5:
    Exit()
    return
  if evt == 6:
    EXPORT_SKELETON = 1 - EXPORT_SKELETON
  if evt == 7:
    EXPORT_ANIMATION = 1 - EXPORT_ANIMATION
  if evt == 8:
    EXPORT_MESH = 1 - EXPORT_MESH
  if evt == 9:
    EXPORT_MATERIAL = 1 - EXPORT_MATERIAL
  if evt == 10:
    EXPORT_FOR_SOYA = 1 - EXPORT_FOR_SOYA
  if evt == 11:
    REMOVE_PATH_FROM_IMAGE = 1 - REMOVE_PATH_FROM_IMAGE
  if evt == 12:
    LODS = 1 - LODS
  if evt == 13:
    IMAGE_PREFIX = _image_prefix.val
  if evt == 14:
    FILE_PREFIX = _file_prefix.val
  if evt == 15:
    DELETE_ALL_FILES = 1 - DELETE_ALL_FILES
  if evt == 16:
     FPS = _framepsec.val
  Draw()

def update_reg():
   x = {}
   x['sd'] = SAVE_TO_DIR
   x['da'] = DELETE_ALL_FILES
   x['es'] = EXPORT_SKELETON
   x['ea'] = EXPORT_ANIMATION
   x['em'] = EXPORT_MESH
   x['emat'] = EXPORT_MATERIAL
   x['fp'] = FILE_PREFIX
   x['rp'] = REMOVE_PATH_FROM_IMAGE
   x['ip'] = IMAGE_PREFIX
   x['ex'] = EXPORT_TO_XML
   x['sc'] = SCALE
   x['fps'] = FPS
   x['soya'] = EXPORT_FOR_SOYA
   x['lod'] = LODS
   Blender.Registry.SetKey('Cal3dExporter', x)

def get_from_reg():
   global SAVE_TO_DIR, DELETE_ALL_FILES, EXPORT_SKELETON, \
     EXPORT_ANIMATION, EXPORT_MESH, EXPORT_MATERIAL, FILE_PREFIX,  \
     REMOVE_PATH_FROM_IMAGE, IMAGE_PREFIX, EXPORT_TO_XML, SCALE, \
     FPS, EXPORT_FOR_SOYA, LODS

   tmp = Blender.Registry.GetKey("Cal3dExporter")
   if tmp: 
     SAVE_TO_DIR = tmp['sd']
     #DELETE_ALL_FILES = tmp['da']
     EXPORT_SKELETON = tmp['es']
     EXPORT_ANIMATION = tmp['ea']
     EXPORT_MESH = tmp['em']
     EXPORT_MATERIAL = tmp['emat']
     FILE_PREFIX = tmp['fp']
     REMOVE_PATH_FROM_IMAGE = tmp['rp']
     IMAGE_PREFIX = tmp['ip']
     EXPORT_TO_XML = tmp['ex']
     SCALE = tmp['sc']
     FPS = tmp['fps']
     EXPORT_FOR_SOYA = tmp['soya']
     LODS = tmp['lod']

get_from_reg()
Register(gui, event, bevent)
