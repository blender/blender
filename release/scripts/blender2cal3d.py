#!BPY

"""
Name: 'Cal3D v0.5'
Blender: 232
Group: 'Export'
Tip: 'Export armature/bone data to the Cal3D library.'
"""

# blender2cal3D.py version 0.5
# Copyright (C) 2003 Jean-Baptiste LAMY -- jiba@tuxfamily.org
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


# This script is a Blender 2.28 => Cal3D 0.7/0.8 converter.
# (See http://blender.org and http://cal3d.sourceforge.net)
#
# Grab the latest version here :
# http://oomadness.tuxfamily.org/en/blender2cal3d

# HOW TO USE :
# 1 - load the script in Blender's text editor
# 2 - modify the parameters below (e.g. the file name)
# 3 - type M-P (meta/alt + P) and wait until script execution is finished

# ADVICES :
# - Use only locrot keys in Blender's action
# - Do not put "." in action or bone names, and do not start these names by a figure
# - Objects whose names start by "_" are not exported (hidden object)
# - All your armature's bones must be connected to another bone (except for the root
#   bone). Contrary to Blender, Cal3D doesn't support "floating" bones.
# - Only Linux has been tested

# BUGS / TODO :
# - Animation names ARE LOST when exporting (this is due to Blender Python API and cannot
#   be fixed until the API change). See parameters for how to rename your animations
# - Rotation, translation, or stretch (size changing) of Blender object is still quite
#   bugged, so AVOID MOVING / ROTATING / RESIZE OBJECTS (either mesh or armature) !
#   Instead, edit the object (with tab), select all points / bones (with "a"),
#   and move / rotate / resize them.
# - Material color is not supported yet
# - Cal3D springs (for clothes and hair) are not supported yet
# - Optimization tips : almost all the time is spent on scene.makeCurrent(), called for
#   updating the IPO curve's values. Updating a single IPO and not the whole scene
#   would speed up a lot.

# Questions and comments are welcome at jiba@tuxfamily.org


# Parameters :

# The directory where the data are saved.
# blender2cal3d.py will create all files in this directory,
# including a .cfg file.
# WARNING: As Cal3D stores model in directory and not in a single file,
# you MUST avoid putting other file in this directory !
# Please give an empty directory (or an unexistant one).
# Files may be deleted from this directoty !
SAVE_TO_DIR = "cal3d"

# Use this dictionary to rename animations, as their name is lost at the exportation.
RENAME_ANIMATIONS = {
  # "OldName" : "NewName",
  
  }

# True (=1) to export for the Soya 3D engine (http://oomadness.tuxfamily.org/en/soya).
# (=> rotate meshes and skeletons so as X is right, Y is top and -Z is front)
EXPORT_FOR_SOYA = 0

# Enables LODs computation. LODs computation is quite slow, and the algo is surely
# not optimal :-(
LODS = 0

# See also BASE_MATRIX below, if you want to rotate/scale/translate the model at
# the exportation.


#########################################################################################
# Code starts here.
# The script should be quite re-useable for writing another Blender animation exporter.
# Most of the hell of it is to deal with Blender's head-tail-roll bone's definition.

import sys, os, os.path, struct, math, string
import Blender

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
  return [[1.0 - 2.0 * (yy + zz),       2.0 * (xy + wz),       2.0 * (xz - wy), 0.0],
          [      2.0 * (xy - wz), 1.0 - 2.0 * (xx + zz),       2.0 * (yz + wx), 0.0],
          [      2.0 * (xz + wy),       2.0 * (yz - wx), 1.0 - 2.0 * (xx + yy), 0.0],
          [0.0                  , 0.0                  , 0.0                  , 1.0]]

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
    [vx2 * co1 + cos,          vx * vy * co1 + vz * sin, vz * vx * co1 - vy * sin, 0.0],
    [vx * vy * co1 - vz * sin, vy2 * co1 + cos,          vy * vz * co1 + vx * sin, 0.0],
    [vz * vx * co1 + vy * sin, vy * vz * co1 - vx * sin, vz2 * co1 + cos,          0.0],
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
  return math.sqrt((p2[0] - p1[0]) ** 2 + (p2[1] - p1[1]) ** 2 + (p2[2] - p1[2]) ** 2)

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


# Hack for having the model rotated right.
# Put in BASE_MATRIX your own rotation if you need some.

if EXPORT_FOR_SOYA:
  BASE_MATRIX = matrix_rotate_x(-math.pi / 2.0)
  
else:
  BASE_MATRIX = None


# Cal3D data structures

CAL3D_VERSION = 700

NEXT_MATERIAL_ID = 0
class Material:
  def __init__(self, map_filename = None):
    self.ambient_r  = 255
    self.ambient_g  = 255
    self.ambient_b  = 255
    self.ambient_a  = 255
    self.diffuse_r  = 255
    self.diffuse_g  = 255
    self.diffuse_b  = 255
    self.diffuse_a  = 255
    self.specular_r = 255
    self.specular_g = 255
    self.specular_b = 255
    self.specular_a = 255
    self.shininess = 1.0
    if map_filename: self.maps_filenames = [map_filename]
    else:            self.maps_filenames = []
    
    MATERIALS[map_filename] = self
    
    global NEXT_MATERIAL_ID
    self.id = NEXT_MATERIAL_ID
    NEXT_MATERIAL_ID += 1
    
  def to_cal3d(self):
    s = "CRF\0" + struct.pack("iBBBBBBBBBBBBfi", CAL3D_VERSION, self.ambient_r, self.ambient_g, self.ambient_b, self.ambient_a, self.diffuse_r, self.diffuse_g, self.diffuse_b, self.diffuse_a, self.specular_r, self.specular_g, self.specular_b, self.specular_a, self.shininess, len(self.maps_filenames))
    for map_filename in self.maps_filenames:
      s += struct.pack("i", len(map_filename) + 1)
      s += map_filename + "\0"
    return s
  
MATERIALS = {}

class Mesh:
  def __init__(self, name):
    self.name      = name
    self.submeshes = []
    
    self.next_submesh_id = 0
    
  def to_cal3d(self):
    s = "CMF\0" + struct.pack("ii", CAL3D_VERSION, len(self.submeshes))
    s += "".join(map(SubMesh.to_cal3d, self.submeshes))
    return s
  
class SubMesh:
  def __init__(self, mesh, material):
    self.material   = material
    self.vertices   = []
    self.faces      = []
    self.nb_lodsteps = 0
    self.springs    = []
    
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
        if not l: vertex2faces[vertex] = [face]
        else: l.append(face)
        
    couple_treated         = {}
    couple_collapse_factor = []
    for face in self.faces:
      for a, b in ((face.vertex1, face.vertex2), (face.vertex1, face.vertex3), (face.vertex2, face.vertex3)):
        a = a.cloned_from or a
        b = b.cloned_from or b
        if a.id > b.id: a, b = b, a
        if not couple_treated.has_key((a, b)):
          # The collapse factor is simply the distance between the 2 points :-(
          # This should be improved !!
          if vector_dotproduct(a.normal, b.normal) < 0.9: continue
          couple_collapse_factor.append((point_distance(a.loc, b.loc), a, b))
          couple_treated[a, b] = 1
      
    couple_collapse_factor.sort()
    
    collapsed    = {}
    new_vertices = []
    new_faces    = []
    for factor, v1, v2 in couple_collapse_factor:
      # Determines if v1 collapses to v2 or v2 to v1.
      # We choose to keep the vertex which is on the smaller number of faces, since
      # this one has more chance of being in an extrimity of the body.
      # Though heuristic, this rule yields very good results in practice.
      if   len(vertex2faces[v1]) <  len(vertex2faces[v2]): v2, v1 = v1, v2
      elif len(vertex2faces[v1]) == len(vertex2faces[v2]):
        if collapsed.get(v1, 0): v2, v1 = v1, v2 # v1 already collapsed, try v2
        
      if (not collapsed.get(v1, 0)) and (not collapsed.get(v2, 0)):
        collapsed[v1] = 1
        collapsed[v2] = 1
        
        # Check if v2 is already colapsed
        while v2.collapse_to: v2 = v2.collapse_to
        
        common_faces = filter(vertex2faces[v1].__contains__, vertex2faces[v2])
        
        v1.collapse_to         = v2
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

        # HACK -- all faces get collapsed with v1 (and no faces are collapsed with v1's
        # clones). This is why we add v1 in new_vertices after v1's clones.
        # This hack has no other incidence that consuming a little few memory for the
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
        
    new_vertices.extend(filter(lambda vertex: not vertex.collapse_to, self.vertices))
    new_vertices.reverse() # Cal3D want LODed vertices at the end
    for i in range(len(new_vertices)): new_vertices[i].id = i
    self.vertices = new_vertices
    
    new_faces.extend(filter(lambda face: not face.can_collapse, self.faces))
    new_faces.reverse() # Cal3D want LODed faces at the end
    self.faces = new_faces
    
    print "LODs computed : %s vertices can be removed (from a total of %s)." % (self.nb_lodsteps, len(self.vertices))
    
  def rename_vertices(self, new_vertices):
    """Rename (change ID) of all vertices, such as self.vertices == new_vertices."""
    for i in range(len(new_vertices)): new_vertices[i].id = i
    self.vertices = new_vertices
    
  def to_cal3d(self):
    s =  struct.pack("iiiiii", self.material.id, len(self.vertices), len(self.faces), self.nb_lodsteps, len(self.springs), len(self.material.maps_filenames))
    s += "".join(map(Vertex.to_cal3d, self.vertices))
    s += "".join(map(Spring.to_cal3d, self.springs))
    s += "".join(map(Face  .to_cal3d, self.faces))
    return s

class Vertex:
  def __init__(self, submesh, loc, normal):
    self.loc    = loc
    self.normal = normal
    self.collapse_to         = None
    self.face_collapse_count = 0
    self.maps       = []
    self.influences = []
    self.weight = None
    
    self.cloned_from = None
    self.clones      = []
    
    self.submesh = submesh
    self.id = submesh.next_vertex_id
    submesh.next_vertex_id += 1
    submesh.vertices.append(self)
    
  def to_cal3d(self):
    if self.collapse_to: collapse_id = self.collapse_to.id
    else:                collapse_id = -1
    s =  struct.pack("ffffffii", self.loc[0], self.loc[1], self.loc[2], self.normal[0], self.normal[1], self.normal[2], collapse_id, self.face_collapse_count)
    s += "".join(map(Map.to_cal3d, self.maps))
    s += struct.pack("i", len(self.influences))
    s += "".join(map(Influence.to_cal3d, self.influences))
    if not self.weight is None: s += struct.pack("f", len(self.weight))
    return s
  
class Map:
  def __init__(self, u, v):
    self.u = u
    self.v = v
    
  def to_cal3d(self):
    return struct.pack("ff", self.u, self.v)
    
class Influence:
  def __init__(self, bone, weight):
    self.bone   = bone
    self.weight = weight
    
  def to_cal3d(self):
    return struct.pack("if", self.bone.id, self.weight)
    
class Spring:
  def __init__(self, vertex1, vertex2):
    self.vertex1 = vertex1
    self.vertex2 = vertex2
    self.spring_coefficient = 0.0
    self.idlelength = 0.0
    
  def to_cal3d(self):
    return struct.pack("iiff", self.vertex1.id, self.vertex2.id, self.spring_coefficient, self.idlelength)

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
    
class Skeleton:
  def __init__(self):
    self.bones = []
    
    self.next_bone_id = 0
    
  def to_cal3d(self):
    s = "CSF\0" + struct.pack("ii", CAL3D_VERSION, len(self.bones))
    s += "".join(map(Bone.to_cal3d, self.bones))
    return s

BONES = {}

class Bone:
  def __init__(self, skeleton, parent, name, loc, rot):
    self.parent = parent
    self.name   = name
    self.loc = loc
    self.rot = rot
    self.children = []
    
    self.matrix = matrix_translate(quaternion2matrix(rot), loc)
    if parent:
      self.matrix = matrix_multiply(parent.matrix, self.matrix)
      parent.children.append(self)
    
    # lloc and lrot are the bone => model space transformation (translation and rotation).
    # They are probably specific to Cal3D.
    m = matrix_invert(self.matrix)
    self.lloc = m[3][0], m[3][1], m[3][2]
    self.lrot = matrix2quaternion(m)
    
    self.skeleton = skeleton
    self.id = skeleton.next_bone_id
    skeleton.next_bone_id += 1
    skeleton.bones.append(self)
    
    BONES[name] = self
    
  def to_cal3d(self):
    s =  struct.pack("i", len(self.name) + 1) + self.name + "\0"
    
    # We need to negate quaternion W value, but why ?
    s += struct.pack("ffffffffffffff", self.loc[0], self.loc[1], self.loc[2], self.rot[0], self.rot[1], self.rot[2], -self.rot[3], self.lloc[0], self.lloc[1], self.lloc[2], self.lrot[0], self.lrot[1], self.lrot[2], -self.lrot[3])
    if self.parent: s += struct.pack("i", self.parent.id)
    else:           s += struct.pack("i", -1)
    s += struct.pack("i", len(self.children))
    s += "".join(map(lambda bone: struct.pack("i", bone.id), self.children))
    return s
  
class Animation:
  def __init__(self, name, duration = 0.0):
    self.name     = name
    self.duration = duration
    self.tracks   = {} # Map bone names to tracks
    
  def to_cal3d(self):
    s = "CAF\0" + struct.pack("ifi", CAL3D_VERSION, self.duration, len(self.tracks))
    s += "".join(map(Track.to_cal3d, self.tracks.values()))
    return s
    
class Track:
  def __init__(self, animation, bone):
    self.bone      = bone
    self.keyframes = []
    
    self.animation = animation
    animation.tracks[bone.name] = self
    
  def to_cal3d(self):
    s = struct.pack("ii", self.bone.id, len(self.keyframes))
    s += "".join(map(KeyFrame.to_cal3d, self.keyframes))
    return s
    
class KeyFrame:
  def __init__(self, track, time, loc, rot):
    self.time = time
    self.loc  = loc
    self.rot  = rot
    
    self.track = track
    track.keyframes.append(self)
    
  def to_cal3d(self):
    # We need to negate quaternion W value, but why ?
    return struct.pack("ffffffff", self.time, self.loc[0], self.loc[1], self.loc[2], self.rot[0], self.rot[1], self.rot[2], -self.rot[3])


def export():
  # Get the scene
  
  scene = Blender.Scene.getCurrent()
  
  
  # Export skeleton (=armature)
  
  skeleton = Skeleton()
  
  for obj in Blender.Object.Get():
    data = obj.getData()
    if type(data) is Blender.Types.ArmatureType:
      matrix = obj.getMatrix()
      if BASE_MATRIX: matrix = matrix_multiply(BASE_MATRIX, matrix)
      
      def treat_bone(b, parent = None):
        head = b.getHead()
        tail = b.getTail()
        
        # Turns the Blender's head-tail-roll notation into a quaternion
        quat = matrix2quaternion(blender_bone2matrix(head, tail, b.getRoll()))
        
        if parent:
          # Compute the translation from the parent bone's head to the child
          # bone's head, in the parent bone coordinate system.
          # The translation is parent_tail - parent_head + child_head,
          # but parent_tail and parent_head must be converted from the parent's parent
          # system coordinate into the parent system coordinate.
          
          parent_invert_transform = matrix_invert(quaternion2matrix(parent.rot))
          parent_head = vector_by_matrix(parent.head, parent_invert_transform)
          parent_tail = vector_by_matrix(parent.tail, parent_invert_transform)
          
          bone = Bone(skeleton, parent, b.getName(), [parent_tail[0] - parent_head[0] + head[0], parent_tail[1] - parent_head[1] + head[1], parent_tail[2] - parent_head[2] + head[2]], quat)
        else:
          # Apply the armature's matrix to the root bones
          head = point_by_matrix(head, matrix)
          tail = point_by_matrix(tail, matrix)
          quat = matrix2quaternion(matrix_multiply(matrix, quaternion2matrix(quat))) # Probably not optimal
          
          # Here, the translation is simply the head vector
          bone = Bone(skeleton, parent, b.getName(), head, quat)
          
        bone.head = head
        bone.tail = tail
        
        for child in b.getChildren(): treat_bone(child, bone)
        
      for b in data.getBones(): treat_bone(b)
      
      # Only one armature / skeleton
      break
    
    
  # Export Mesh data
  
  meshes = []
  
  for obj in Blender.Object.Get():
    data = obj.getData()
    if (type(data) is Blender.Types.NMeshType) and data.faces:
      mesh = Mesh(obj.name)
      meshes.append(mesh)
      
      matrix = obj.getMatrix()
      if BASE_MATRIX: matrix = matrix_multiply(BASE_MATRIX, matrix)
      
      faces = data.faces
      while faces:
        image          = faces[0].image
        image_filename = image and image.filename
        material       = MATERIALS.get(image_filename) or Material(image_filename)
        
        # TODO add material color support here
        
        submesh  = SubMesh(mesh, material)
        vertices = {}
        for face in faces[:]:
          if (face.image and face.image.filename) == image_filename:
            faces.remove(face)
            
            if not face.smooth:
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
                coord  = point_by_matrix (face.v[i].co, matrix)
                if face.smooth: normal = vector_normalize(vector_by_matrix(face.v[i].no, matrix))
                vertex  = vertices[face.v[i].index] = Vertex(submesh, coord, normal)
                
                influences = data.getVertexInfluences(face.v[i].index)
                if not influences: print "Warning !  A vertex has no influence !"
                
                # sum of influences is not always 1.0 in Blender ?!?!
                sum = 0.0
                for bone_name, weight in influences: sum += weight
                
                for bone_name, weight in influences:
                  vertex.influences.append(Influence(BONES[bone_name], weight / sum))
                  
              elif not face.smooth:
                # We cannot share vertex for non-smooth faces, since Cal3D does not
                # support vertex sharing for 2 vertices with different normals.
                # => we must clone the vertex.
                
                old_vertex = vertex
                vertex = Vertex(submesh, vertex.loc, normal)
                vertex.cloned_from = old_vertex
                vertex.influences = old_vertex.influences
                old_vertex.clones.append(vertex)
                
              if data.hasFaceUV():
                uv = [face.uv[i][0], 1.0 - face.uv[i][1]]
                if not vertex.maps: vertex.maps.append(Map(*uv))
                elif (vertex.maps[0].u != uv[0]) or (vertex.maps[0].v != uv[1]):
                  # This vertex can be shared for Blender, but not for Cal3D !!!
                  # Cal3D does not support vertex sharing for 2 vertices with
                  # different UV texture coodinates.
                  # => we must clone the vertex.
                  
                  for clone in vertex.clones:
                    if (clone.maps[0].u == uv[0]) and (clone.maps[0].v == uv[1]):
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
              Face(submesh, face_vertices[0], face_vertices[i], face_vertices[i + 1])
              
        # Computes LODs info
        if LODS: submesh.compute_lods()
        
  # Export animations
              
  ANIMATIONS = {}
  
  for ipo in Blender.Ipo.Get():
    name = ipo.getName()
    
    # Try to extract the animation name and the bone name from the IPO name.
    # THIS MAY NOT WORK !!!
    # The animation name extracted here is usually NOT the name of the action in Blender
    
    splitted = name.split(".")
    if len(splitted) == 2:
      animation_name, bone_name = splitted
      animation_name += ".000"
    elif len(splitted) == 3:
      animation_name, a, b = splitted
      if   a[0] in string.digits:
        animation_name += "." + a
        bone_name = b
      elif b[0] in string.digits:
        animation_name += "." + b
        bone_name = a
      else:
        print "Un-analysable IPO name :", name
        continue
    else:
      print "Un-analysable IPO name :", name
      continue
    
    animation = ANIMATIONS.get(animation_name)
    if not animation:
      animation = ANIMATIONS[animation_name] = Animation(animation_name)

    bone  = BONES[bone_name]
    track = animation.tracks.get(bone_name)
    if not track:
      track = animation.tracks[bone_name] = Track(animation, bone)
      track.finished = 0

    nb_curve = ipo.getNcurves()
    has_loc = nb_curve in (3, 7)
    has_rot = nb_curve in (4, 7)
    
    # TODO support size here
    # Cal3D does not support it yet.
    
    try: nb_bez_pts = ipo.getNBezPoints(0)
    except TypeError:
      print "No key frame for animation %s, bone %s, skipping..." % (animation_name, bone_name)
      nb_bez_pts = 0
      
    for bez in range(nb_bez_pts): # WARNING ! May not work if not loc !!!
      time = ipo.getCurveBeztriple(0, bez)[3]
      scene.currentFrame(int(time))

      # Needed to update IPO's value, but probably not the best way for that...
      scene.makeCurrent()

      # Convert time units from Blender's frame (starting at 1) to second
      # (using default FPS of 25)
      time = (time - 1.0) / 25.0

      if animation.duration < time: animation.duration = time
      
      loc = bone.loc
      rot = bone.rot
      
      curves = ipo.getCurves()
      print curves
      curve_id = 0
      while curve_id < len(curves):
        curve_name = curves[curve_id].getName()
        if curve_name == "LocX":
          # Get the translation
          # We need to blend the translation from the bone rest state (=bone.loc) with
          # the translation due to IPO.
          trans = vector_by_matrix((
            ipo.getCurveCurval(curve_id),
            ipo.getCurveCurval(curve_id + 1),
            ipo.getCurveCurval(curve_id + 2),
            ), bone.matrix)
          loc = [
            bone.loc[0] + trans[0],
            bone.loc[1] + trans[1],
            bone.loc[2] + trans[2],
            ]
          curve_id += 3
          
        elif curve_name == "RotX":
          # Get the rotation of the IPO
          ipo_rot = [
            ipo.getCurveCurval(curve_id),
            ipo.getCurveCurval(curve_id + 1),
            ipo.getCurveCurval(curve_id + 2),
            ipo.getCurveCurval(curve_id + 3),
            ]
          curve_id += 3 # XXX Strange !!!
          # We need to blend the rotation from the bone rest state (=bone.rot) with
          # ipo_rot.
          rot = quaternion_multiply(ipo_rot, bone.rot)
          
        else:
          print "Unknown IPO curve : %s" % curve_name
          break #Unknown curves
      
      KeyFrame(track, time, loc, rot)
        
      
  # Save all data
  
  if not os.path.exists(SAVE_TO_DIR): os.makedirs(SAVE_TO_DIR)
  else:
    for file in os.listdir(SAVE_TO_DIR):
      if file.endswith(".cfg") or file.endswith(".caf") or file.endswith(".cmf") or file.endswith(".csf") or file.endswith(".crf"):
        os.unlink(os.path.join(SAVE_TO_DIR, file))
        
  cfg = open(os.path.join(SAVE_TO_DIR, os.path.basename(SAVE_TO_DIR) + ".cfg"), "wb")
  print >> cfg, "# Cal3D model exported from Blender with blender2cal3d.py"
  print >> cfg
  
  open(os.path.join(SAVE_TO_DIR, os.path.basename(SAVE_TO_DIR) + ".csf"), "wb").write(skeleton.to_cal3d())
  print >> cfg, "skeleton=%s.csf" % os.path.basename(SAVE_TO_DIR)
  print >> cfg
  
  for animation in ANIMATIONS.values():
    if animation.duration: # Cal3D does not support animation with only one state
      animation.name = RENAME_ANIMATIONS.get(animation.name) or animation.name
      open(os.path.join(SAVE_TO_DIR, animation.name + ".caf"), "wb").write(animation.to_cal3d())
      print >> cfg, "animation=%s.caf" % animation.name
      
      # Prints animation names and durations, in order to help identifying animation
      # (since their name are lost).
      print animation.name, "duration", animation.duration * 25.0 + 1.0
      
  print >> cfg

  for mesh in meshes:
    if not mesh.name.startswith("_"):
      open(os.path.join(SAVE_TO_DIR, mesh.name + ".cmf"), "wb").write(mesh.to_cal3d())
      print >> cfg, "mesh=%s.cmf" % mesh.name
  print >> cfg
  
  materials = MATERIALS.values()
  materials.sort(lambda a, b: cmp(a.id, b.id))
  for material in materials:
    if material.maps_filenames: filename = os.path.splitext(os.path.basename(material.maps_filenames[0]))[0]
    else:                       filename = "plain"
    open(os.path.join(SAVE_TO_DIR, filename + ".crf"), "wb").write(material.to_cal3d())
    print >> cfg, "material=%s.crf" % filename
  print >> cfg
  
  print "Saved to", SAVE_TO_DIR
  print "Done."

export()
