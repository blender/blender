#!BPY

"""
Name: 'Cal3D v0.9'
Blender: 235
Group: 'Export'
Tip: 'Export armature/bone/mesh/action data to the Cal3D format.'
"""

# blender2cal3D.py
# Copyright (C) 2003-2004 Jean-Baptiste LAMY -- jibalamy@free.fr
# Copyright (C) 2004 Matthias Braun -- matze@braunis.de
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


__version__ = "0.11"
__author__  = "Jean-Baptiste 'Jiba' Lamy"
__email__   = ["Author's email, jibalamy:free*fr"]
__url__     = ["Soya3d's homepage, http://home.gna.org/oomadness/en/soya/",
	"Cal3d, http://cal3d.sourceforge.net"]
__bpydoc__  = """\
This script is a Blender => Cal3D converter.
(See http://blender.org and http://cal3d.sourceforge.net)

USAGE:

To install it, place the script in your $HOME/.blender/scripts directory.

Then open the File->Export->Cal3d v0.9 menu. And select the filename of the .cfg file.
The exporter will create a set of other files with same prefix (ie. bla.cfg, bla.xsf,
bla_Action1.xaf, bla_Action2.xaf, ...).

You should be able to open the .cfg file in cal3d_miniviewer.


NOT (YET) SUPPORTED:

  - Rotation, translation, or stretching Blender objects is still quite
buggy, so AVOID MOVING / ROTATING / RESIZE OBJECTS (either mesh or armature) !
Instead, edit the object (with tab), select all points / bones (with "a"),
and move / rotate / resize them.<br>
  - no support for exporting springs yet<br>
  - no support for exporting material colors (most games should only use images
I think...)


KNOWN ISSUES:

  - Cal3D versions <=0.9.1 have a bug where animations aren't played when the root bone
is not animated;<br>
  - Cal3D versions <=0.9.1 have a bug where objects that aren't influenced by any bones
are not drawn (fixed in Cal3D CVS).


NOTES:

It requires a very recent version of Blender (>= 2.35).

Build a model following a few rules:<br>
  - Use only a single armature;<br>
  - Use only a single rootbone (Cal3D doesn't support floating bones);<br>
  - Use only locrot keys (Cal3D doesn't support bone's size change);<br>
  - Don't try to create child/parent constructs in blender object, that gets exported
incorrectly at the moment;<br>
  - Don't put "." in action or bone names, and do not start these names by a figure;<br>
  - Objects or animations whose names start by "_" are not exported (hidden object).

It can be run in batch mode, as following :<br>
    blender model.blend -P blender2cal3d.py --blender2cal3d FILENAME=model.cfg EXPORT_FOR_SOYA=1

You can pass as many parameters as you want at the end, "EXPORT_FOR_SOYA=1" is just an
example. The parameters are the same as below.
"""

# Parameters :

# Filename to export to (if "", display a file selector dialog).
FILENAME = ""

# True (=1) to export for the Soya 3D engine
#     (http://oomadness.tuxfamily.org/en/soya).
# (=> rotate meshes and skeletons so as X is right, Y is top and -Z is front)
EXPORT_FOR_SOYA = 0

# Enables LODs computation. LODs computation is quite slow, and the algo is
# surely not optimal :-(
LODS = 0

# Scale the model (not supported by Soya).
SCALE = 1.0

# Set to 1 if you want to prefix all filename with the model name
# (e.g. knight_walk.xaf instead of walk.xaf)
PREFIX_FILE_WITH_MODEL_NAME = 0

# Set to 0 to use Cal3D binary format
XML = 1


MESSAGES = ""

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


# transforms a blender to a cal3d quaternion notation (x,y,z,w)
def blender2cal3dquat(q):
  return [q.x, q.y, q.z, q.w]

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

# multiplies 2 quaternions in x,y,z,w notation
def quaternion_multiply(q1, q2):
  return [
    q2[3] * q1[0] + q2[0] * q1[3] + q2[1] * q1[2] - q2[2] * q1[1],
    q2[3] * q1[1] + q2[1] * q1[3] + q2[2] * q1[0] - q2[0] * q1[2],
    q2[3] * q1[2] + q2[2] * q1[3] + q2[0] * q1[1] - q2[1] * q1[0],
    q2[3] * q1[3] - q2[0] * q1[0] - q2[1] * q1[1] - q2[2] * q1[2],
    ]

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

def vector_add(v1, v2):
  return [v1[0]+v2[0], v1[1]+v2[1], v1[2]+v2[2]]

def vector_sub(v1, v2):
  return [v1[0]-v2[0], v1[1]-v2[1], v1[2]-v2[2]]
    
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
    if vector_dotproduct(target, nor) > 0.0: updown =  1.0
    else:                                    updown = -1.0
    
    # Quoted from Blender source : "I think this should work ..."
    bMatrix = [
      [updown, 0.0,    0.0, 0.0],
      [0.0,    updown, 0.0, 0.0],
      [0.0,    0.0,    1.0, 0.0],
      [0.0,    0.0,    0.0, 1.0],
      ]
  
  rMatrix = matrix_rotate(nor, roll)
  return matrix_multiply(rMatrix, bMatrix)


# Hack for having the model rotated right.
# Put in BASE_MATRIX your own rotation if you need some.

BASE_MATRIX = None


# Cal3D data structures

CAL3D_VERSION = 910

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
    
  # old cal3d format
  def to_cal3d(self):
    s = "CRF\0" + struct.pack("iBBBBBBBBBBBBfi", CAL3D_VERSION, self.ambient_r, self.ambient_g, self.ambient_b, self.ambient_a, self.diffuse_r, self.diffuse_g, self.diffuse_b, self.diffuse_a, self.specular_r, self.specular_g, self.specular_b, self.specular_a, self.shininess, len(self.maps_filenames))
    for map_filename in self.maps_filenames:
      s += struct.pack("i", len(map_filename) + 1)
      s += map_filename + "\0"
    return s
 
  # new xml format
  def to_cal3d_xml(self):
    s = "<?xml version=\"1.0\"?>\n"
    s += "<HEADER MAGIC=\"XRF\" VERSION=\"%i\"/>\n" % CAL3D_VERSION
    s += "<MATERIAL NUMMAPS=\"" + str(len(self.maps_filenames)) + "\">\n"
    s += "  <AMBIENT>" + str(self.ambient_r) + " " + str(self.ambient_g) + " " + str(self.ambient_b) + " " + str(self.ambient_a) + "</AMBIENT>\n";
    s += "  <DIFFUSE>" + str(self.diffuse_r) + " " + str(self.diffuse_g) + " " + str(self.diffuse_b) + " " + str(self.diffuse_a) + "</DIFFUSE>\n";
    s += "  <SPECULAR>" + str(self.specular_r) + " " + str(self.specular_g) + " " + str(self.specular_b) + " " + str(self.specular_a) + "</SPECULAR>\n";
    s += "  <SHININESS>" + str(self.shininess) + "</SHININESS>\n";
    for map_filename in self.maps_filenames:
      s += "  <MAP>" + map_filename + "</MAP>\n";
      
    s += "</MATERIAL>\n";
        
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

  def to_cal3d_xml(self):
    s = "<?xml version=\"1.0\"?>\n"
    s += "<HEADER MAGIC=\"XMF\" VERSION=\"%i\"/>\n" % CAL3D_VERSION
    s += "<MESH NUMSUBMESH=\"%i\">\n" % len(self.submeshes)
    s += "".join(map(SubMesh.to_cal3d_xml, self.submeshes))
    s += "</MESH>\n"                                                  
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
    self.bone   = bone
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
    return struct.pack("iiff", self.vertex1.id, self.vertex2.id, self.spring_coefficient, self.idlelength)

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
    s = "<?xml version=\"1.0\"?>\n"
    s += "<HEADER MAGIC=\"XSF\" VERSION=\"%i\"/>\n" % CAL3D_VERSION
    s += "<SKELETON NUMBONES=\"%i\">\n" % len(self.bones)
    s += "".join(map(Bone.to_cal3d_xml, self.bones))
    s += "</SKELETON>\n"
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

  def to_cal3d_xml(self):
    s = "  <BONE ID=\"%i\" NAME=\"%s\" NUMCHILD=\"%i\">\n" % \
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
  def __init__(self, name, duration = 0.0):
    self.name     = name
    self.duration = duration
    self.tracks   = {} # Map bone names to tracks
    
  def to_cal3d(self):
    s = "CAF\0" + struct.pack("ifi", CAL3D_VERSION, self.duration, len(self.tracks))
    s += "".join(map(Track.to_cal3d, self.tracks.values()))
    return s

  def to_cal3d_xml(self):
    s = "<?xml version=\"1.0\"?>\n"
    s += "<HEADER MAGIC=\"XAF\" VERSION=\"%i\"/>\n" % CAL3D_VERSION
    s += "<ANIMATION DURATION=\"%f\" NUMTRACKS=\"%i\">\n" % \
         (self.duration, len(self.tracks))                            
    s += "".join(map(Track.to_cal3d_xml, self.tracks.values()))
    s += "</ANIMATION>\n"
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

  def to_cal3d_xml(self):
    s = "  <TRACK BONEID=\"%i\" NUMKEYFRAMES=\"%i\">\n" % \
        (self.bone.id, len(self.keyframes))
    s += "".join(map(KeyFrame.to_cal3d_xml, self.keyframes))
    s += "  </TRACK>\n"
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

  def to_cal3d_xml(self):
    s = "    <KEYFRAME TIME=\"%f\">\n" % self.time
    s += "      <TRANSLATION>%f %f %f</TRANSLATION>\n" % \
         (self.loc[0], self.loc[1], self.loc[2])
    # We need to negate quaternion W value, but why ?
    s += "      <ROTATION>%f %f %f %f</ROTATION>\n" % \
         (self.rot[0], self.rot[1], self.rot[2], -self.rot[3])
    s += "    </KEYFRAME>\n"
    return s                                                      

def export(filename):
  global MESSAGES
  
  if EXPORT_FOR_SOYA:
    global BASE_MATRIX
    BASE_MATRIX = matrix_rotate_x(-math.pi / 2.0)
    
  # Get the scene
  scene = Blender.Scene.getCurrent()
  
  # ---- Export skeleton (=armature) ----------------------------------------

  skeleton = Skeleton()
  
  foundarmature = False
  for obj in Blender.Object.Get():
    data = obj.getData()
    if type(data) is not Blender.Types.ArmatureType:
      continue
    
    if foundarmature == True:
      MESSAGES += "Found multiple armatures! '" + obj.getName() + "' ignored.\n"
      continue

    foundarmature = True
    matrix = obj.getMatrix()
    if BASE_MATRIX:
      matrix = matrix_multiply(BASE_MATRIX, matrix)
    
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

        ploc = vector_add(head, b.getLoc())
        parentheadtotail = vector_sub(parent_tail, parent_head)
        # hmm this should be handled by the IPos, but isn't for non-animated
        # bones which are transformed in the pose mode...
        #loc = vector_add(ploc, parentheadtotail)
        #rot = quaternion_multiply(blender2cal3dquat(b.getQuat()), quat)
        loc = parentheadtotail
        rot = quat
        
        bone = Bone(skeleton, parent, b.getName(), loc, rot)
      else:
        # Apply the armature's matrix to the root bones
        head = point_by_matrix(head, matrix)
        tail = point_by_matrix(tail, matrix)
        quat = matrix2quaternion(matrix_multiply(matrix, quaternion2matrix(quat))) # Probably not optimal
        
        # loc = vector_add(head, b.getLoc())
        # rot = quaternion_multiply(blender2cal3dquat(b.getQuat()), quat)
        loc = head
        rot = quat
        
        # Here, the translation is simply the head vector
        bone = Bone(skeleton, None, b.getName(), loc, rot)
        
      bone.head = head
      bone.tail = tail
      
      for child in b.getChildren():
        treat_bone(child, bone)
     
    foundroot = False
    for b in data.getBones():
      # child bones are handled in treat_bone
      if b.getParent() != None:
        continue
      if foundroot == True:
        print "Warning: Found multiple root-bones, this may not be supported in cal3d."
        #print "Ignoring bone '" + b.getName() + "' and it's childs."
        #continue
        
      treat_bone(b)
      foundroot = True

  # ---- Export Mesh data ---------------------------------------------------
  
  meshes = []
  
  for obj in Blender.Object.Get():
    data = obj.getData()
    if (type(data) is Blender.Types.NMeshType) and data.faces:
      mesh_name = obj.getName()
      mesh = Mesh(mesh_name)
      meshes.append(mesh)
      
      matrix = obj.getMatrix()
      if BASE_MATRIX:
        matrix = matrix_multiply(BASE_MATRIX, matrix)
        
      faces = data.faces
      while faces:
        image          = faces[0].image
        image_filename = image and image.filename
        material       = MATERIALS.get(image_filename) or Material(image_filename)
        outputuv       = len(material.maps_filenames) > 0
        
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
                if face.smooth:
                  normal = vector_normalize(vector_by_matrix(face.v[i].no, matrix))
                vertex  = vertices[face.v[i].index] = Vertex(submesh, coord, normal)

                influences = data.getVertexInfluences(face.v[i].index)
                # should this really be a warning? (well currently enabled,
                # because blender has some bugs where it doesn't return
                # influences in python api though they are set, and because
                # cal3d<=0.9.1 had bugs where objects without influences
                # aren't drawn.
                if not influences:
                  MESSAGES += "A vertex of object '%s' has no influences.\n(This occurs on objects placed in an invisible layer, you can fix it by using a single layer)\n" \
                              % obj.getName()
                
                # sum of influences is not always 1.0 in Blender ?!?!
                sum = 0.0
                for bone_name, weight in influences:
                  sum += weight
                
                for bone_name, weight in influences:
                  if bone_name not in BONES:
                    MESSAGES += "Couldn't find bone '%s' which influences" \
                                "object '%s'.\n" % (bone_name, obj.getName())
                    continue
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
                if not vertex.maps:
                  if outputuv: vertex.maps.append(Map(*uv))
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
                    if outputuv: vertex.maps.append(Map(*uv))
                    old_vertex.clones.append(vertex)
                    
              face_vertices.append(vertex)
              
            # Split faces with more than 3 vertices
            for i in range(1, len(face.v) - 1):
              Face(submesh, face_vertices[0], face_vertices[i], face_vertices[i + 1])
              
        # Computes LODs info
        if LODS:
          submesh.compute_lods()
        
  # ---- Export animations --------------------------------------------------
  ANIMATIONS = {}

  for a in Blender.Armature.NLA.GetActions().iteritems():
    animation_name = a[0]
    animation = Animation(animation_name)
    animation.duration = 0.0

    for b in a[1].getAllChannelIpos().iteritems():
      bone_name = b[0]
      if bone_name not in BONES:
        MESSAGES += "No Bone '" + bone_name + "' defined (from Animation '" \
            + animation_name + "' ?!?\n"
        continue                                            

      bone = BONES[bone_name]

      track = Track(animation, bone)
      track.finished = 0
      animation.tracks[bone_name] = track

      ipo = b[1]
      
      times = []
      
      # SideNote: MatzeB: Ipo.getCurve(curvename) is broken in blender 2.33 and
      # below if the Ipo comes from an Action, so only use Ipo.getCurves()!
      # also blender upto 2.33a had a bug where IpoCurve.evaluate was not
      # exposed to the python interface :-/
      
      #run 1: we need to find all time values where we need to produce keyframes
      for curve in ipo.getCurves():
        curve_name = curve.getName()

        if curve_name not in ["QuatW", "QuatX", "QuatY", "QuatZ", "LocX", "LocY", "LocZ"]:
          MESSAGES += "Curve type %s not supported in Action '%s' Bone '%s'.\n"\
                    % (curve_name, animation_name, bone_name)
        
        for p in curve.getPoints():
          time = p.getPoints() [0]
          if time not in times:
            times.append(time)
      
      times.sort()

      # run2: now create keyframes
      for time in times:
        cal3dtime = (time-1) / 25.0 # assume 25FPS by default
        if cal3dtime > animation.duration:
          animation.duration = cal3dtime
        trans = [0, 0, 0]
        quat  = [0, 0, 0, 0]
        
        for curve in ipo.getCurves():
          val = curve.evaluate(time)
          if curve.getName() == "LocX": trans[0] = val
          if curve.getName() == "LocY": trans[1] = val
          if curve.getName() == "LocZ": trans[2] = val
          if curve.getName() == "QuatW": quat[3] = val
          if curve.getName() == "QuatX": quat[0] = val
          if curve.getName() == "QuatY": quat[1] = val
          if curve.getName() == "QuatZ": quat[2] = val
          
        transt = vector_by_matrix(trans, bone.matrix)
        loc = vector_add(bone.loc, transt)
        rot = quaternion_multiply(quat, bone.rot)
        rot = quaternion_normalize(rot)
        
        KeyFrame(track, cal3dtime, loc, rot)
        
    if animation.duration <= 0:
      MESSAGES += "Ignoring Animation '" + animation_name + \
                  "': duration is 0.\n"
      continue
    ANIMATIONS[animation_name] = animation
    
  # Save all data
  if filename.endswith(".cfg"):
    filename = os.path.splitext(filename)[0]
  BASENAME = os.path.basename(filename)         
  DIRNAME  = os.path.dirname(filename)
  if PREFIX_FILE_WITH_MODEL_NAME: PREFIX = BASENAME + "_"
  else:                           PREFIX = ""
  if XML: FORMAT_PREFIX = "x"; encode = lambda x: x.to_cal3d_xml()
  else:   FORMAT_PREFIX = "c"; encode = lambda x: x.to_cal3d()
  print DIRNAME + " - " + BASENAME
  
  cfg = open(os.path.join(DIRNAME, BASENAME + ".cfg"), "wb")
  print >> cfg, "# Cal3D model exported from Blender with blender2cal3d.py"
  print >> cfg

  if SCALE != 1.0:
    print >> cfg, "scale=%s" % SCALE
    print >> cfg
    
  filename = BASENAME + "." + FORMAT_PREFIX + "sf"
  open(os.path.join(DIRNAME, filename), "wb").write(encode(skeleton))
  print >> cfg, "skeleton=%s" % filename
  print >> cfg
  
  for animation in ANIMATIONS.values():
    if not animation.name.startswith("_"):
      if animation.duration: # Cal3D does not support animation with only one state
        filename = PREFIX + animation.name + "." + FORMAT_PREFIX + "af"
        open(os.path.join(DIRNAME, filename), "wb").write(encode(animation))
        print >> cfg, "animation=%s" % filename
        
  print >> cfg
  
  for mesh in meshes:
    if not mesh.name.startswith("_"):
      filename = PREFIX + mesh.name + "." + FORMAT_PREFIX + "mf"
      open(os.path.join(DIRNAME, filename), "wb").write(encode(mesh))
      print >> cfg, "mesh=%s" % filename
  print >> cfg
  
  materials = MATERIALS.values()
  materials.sort(lambda a, b: cmp(a.id, b.id))
  for material in materials:
    if material.maps_filenames:
      filename = PREFIX + os.path.splitext(os.path.basename(material.maps_filenames[0]))[0] + "." + FORMAT_PREFIX + "rf"
    else:
      filename = PREFIX + "plain." + FORMAT_PREFIX + "rf"
    open(os.path.join(DIRNAME, filename), "wb").write(encode(material))
    print >> cfg, "material=%s" % filename
  print >> cfg
  
  MESSAGES += "Saved to '%s.cfg'\n" % BASENAME
  MESSAGES += "Done."
  
  # show messages
  print MESSAGES

# some (ugly) gui to show the error messages - no scrollbar or other luxury,
# please improve this if you know how
def gui():
  global MESSAGES
  button = Blender.Draw.Button("Ok", 1, 0, 0, 50, 20, "Close Window")
    
  lines = MESSAGES.split("\n")
  if len(lines) > 15:
    lines.append("Please also take a look at your console")
  pos = len(lines) * 15 + 20
  for line in lines:
    Blender.BGL.glRasterPos2i(0, pos)
    Blender.Draw.Text(line)
    pos -= 15

def event(evt, val):
  if evt == Blender.Draw.ESCKEY:
    Blender.Draw.Exit()
    return

def button_event(evt):
  if evt == 1:
    Blender.Draw.Exit()
    return

# Main script
def fs_callback(filename):
  export(filename)
  Blender.Draw.Register(gui, event, button_event)


# Check for batch mode
if "--blender2cal3d" in sys.argv:
  args = sys.argv[sys.argv.index("--blender2cal3d") + 1:]
  for arg in args:
    attr, val = arg.split("=")
    try: val = int(val)
    except:
      try: val = float(val)
      except: pass
    globals()[attr] = val
  export(FILENAME)
  Blender.Quit()
  
else:
  if FILENAME: fs_callback(FILENAME)
  else:
    defaultname = Blender.Get("filename")
    if defaultname.endswith(".blend"):
      defaultname = defaultname[0:len(defaultname)-len(".blend")] + ".cfg"
    Blender.Window.FileSelector(fs_callback, "Cal3D Export", defaultname)


