#!BPY

"""
Name: 'OpenInventor (.iv)'
Blender: 236
Group: 'Export'
Tip: 'Export to OpenInventor file format. (.iv)'
"""
__author__ = ("Radek Barton")
__url__ = ["http://blackhex.no-ip.org/"]
__email__ = ["scripts"]
__version__ = "0.1"


__bpydoc__ = """\
This script exports to the Open Inventor format.

Usage:

Run this script from "File->Export" menu.

Note:
"""

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

import Blender
import os
import math

def WriteHeader(file):
  file.write("#Inventor V2.1 ascii\n\n")
  file.write("Separator\n")
  file.write("{\n")
  file.write("  ShapeHints\n")
  file.write("  {\n")
  file.write("    vertexOrdering COUNTERCLOCKWISE\n")
  file.write("  }\n")

def WriteFooter(file):
  file.write("}\n")

def WriteMesh(file, object):
  file.write("  Separator\n")
  file.write("  {\n")
  file.write("    # %s\n" % object.getName())
  WriteMatrix(file, object)
  mesh = object.getData()
  WriteMaterials(file, mesh)
  WriteTexture(file, mesh)
  WriteNormals(file, mesh)
  WriteVertices(file, mesh)
  WriteFaces(file, mesh)
  file.write("  }\n")

def WriteMatrix(file, object):
  matrix = object.getMatrix()
  file.write("    MatrixTransform\n")
  file.write("    {\n")
  file.write("      matrix\n")
  for line in matrix:
    file.write("      %s %s %s %s\n" % (line[0], line[1], line[2], line[3]))
  file.write("    }\n")

def WriteColors(file, mesh):
  file.write("      vertexProperty VertexProperty\n")
  file.write("      {\n")
  file.write("        orderedRGBA\n")
  file.write("        [\n")
  for face in mesh.faces:
    for I in range(len(face.v)):
      file.write("          0x%02x%02x%02x%02x,\n" % (face.col[I].r,
        face.col[I].g, face.col[I].b, face.col[I].a))
  file.write("        ]\n")
  file.write("        materialBinding PER_VERTEX\n")
  file.write("       }\n")

def WriteMaterials(file, mesh):
  if mesh.materials:
    file.write("    Material\n")
    file.write("    {\n")
    file.write("      ambientColor\n")
    file.write("      [\n")
    for mat in mesh.materials:
      file.write("        %s %s %s,\n" % (mat.mirCol[0], mat.mirCol[1],
        mat.mirCol[2]))
    file.write("      ]\n")
    file.write("      diffuseColor\n")
    file.write("      [\n")
    for mat in mesh.materials:
      file.write("        %s %s %s,\n" % (mat.rgbCol[0], mat.rgbCol[1],
        mat.rgbCol[2]))
    file.write("      ]\n")
    file.write("      specularColor\n")
    file.write("      [\n")
    for mat in mesh.materials:
      file.write("        %s %s %s,\n" % (mat.specCol[0] * mat.spec / 2.0,
        mat.specCol[1] * mat.spec / 2.0,  mat.specCol[2] * mat.spec / 2.0))
    file.write("      ]\n")
    file.write("      emissiveColor\n")
    file.write("      [\n")
    for mat in mesh.materials:
      file.write("        %s %s %s,\n" % (mat.rgbCol[0] * mat.emit,
        mat.rgbCol[1] * mat.emit, mat.rgbCol[0] * mat.emit))
    file.write("      ]\n")
    file.write("      shininess\n")
    file.write("      [\n")
    for mat in mesh.materials:
      file.write("        %s,\n" % (mat.hard / 255.0))
    file.write("      ]\n")
    file.write("      transparency\n")
    file.write("      [\n")
    for mat in mesh.materials:
      file.write("        %s,\n" % (1.0 - mat.alpha))
    file.write("      ]\n")
    file.write("    }\n")
    file.write("    MaterialBinding\n")
    file.write("    {\n")
    file.write("      value PER_FACE_INDEXED\n")
    file.write("    }\n")

def WriteTexture(file, mesh):
  texture = mesh.faces[0].image
  if texture:
    file.write("    Texture2\n")
    file.write("    {\n")
    file.write('      filename "%s"\n' % texture.getName())
    file.write("    }\n")
    file.write("    TextureCoordinate2\n")
    file.write("    {\n")
    file.write("      point\n")
    file.write("      [\n")
    if mesh.hasVertexUV():
      for vert in mesh.verts:
        file.write("        %s %s,\n" % (vert.uvco[0], vert.uvco[1]))
      file.write("      ]\n")
      file.write("    }\n")
      file.write("    TextureCoordinateBinding\n")
      file.write("    {\n")
      file.write("      value PER_VERTEX_INDEXED\n")
      file.write("    }\n")
    elif mesh.hasFaceUV():
      for face in mesh.faces:
        for uv in face.uv:
          file.write("        %s %s,\n" % (uv[0], uv[1]))
      file.write("      ]\n")
      file.write("    }\n")
      file.write("    TextureCoordinateBinding\n")
      file.write("    {\n")
      file.write("      value PER_VERTEX\n")
      file.write("    }\n")

def WriteVertices(file, mesh):
  file.write("    Coordinate3\n")
  file.write("    {\n")
  file.write("      point\n")
  file.write("      [\n")
  for vert in mesh.verts:
    file.write("        %s %s %s,\n" % (vert[0], vert[1], vert[2]))
  file.write("      ]\n")
  file.write("    }\n")

def WriteNormals(file, mesh):
  file.write("    Normal\n")
  file.write("    {\n")
  file.write("      vector\n")
  file.write("      [\n")

  # make copy of vertex normals
  normals = []
  for face in mesh.faces:
    if len(face.v) in [3, 4]:
      if face.smooth:
        for v in face.v:
          normals.append(v.no)
      else:
        for v in face.v:
          normals.append(face.no)

  # write normals
  for no in normals:
    file.write("        %s %s %s,\n" % (no[0], no[1], no[2]))
  file.write("      ]\n")
  file.write("    }\n")

  # write way how normals are binded
  file.write("    NormalBinding\n")
  file.write("    {\n")
  file.write("      value PER_VERTEX\n")
  file.write("    }\n")

def WriteFaces(file, mesh):
  file.write("    IndexedFaceSet\n")
  file.write("    {\n")

  # write vertex paint
  if mesh.hasVertexColours():
    WriteColors(file, mesh)

  # write material indexes
  file.write("      materialIndex\n")
  file.write("      [\n")
  for face in mesh.faces:
    file.write("        %s,\n" % (face.mat));
  file.write("      ]\n")

  # write faces with coordinate indexes
  file.write("      coordIndex\n")
  file.write("      [\n")
  for face in mesh.faces:
    if len(face.v) == 3:
      file.write("        %s, %s, %s, -1,\n" % (face.v[0].index,
        face.v[1].index, face.v[2].index))
    elif len(face.v) == 4:
      file.write("        %s, %s, %s, %s, -1,\n"% (face.v[0].index,
        face.v[1].index, face.v[2].index, face.v[3].index))
  file.write("      ]\n")
  file.write("    }\n")


def WriteCamera(file, object):
  camera = object.getData();
  # perspective camera
  if camera.type == 0:
    file.write("  PerspectiveCamera\n")
    file.write("  {\n")
    file.write("    nearDistance %s\n" % (camera.clipStart))
    file.write("    farDistance %s\n" % (camera.clipEnd))
    file.write("  }\n")
  # ortho camera
  else:
    print camera.type

def WriteLamp(file, object):
  lamp = object.getData();
  # spot lamp
  if lamp.type == 2:
    file.write("    SpotLight\n")
    file.write("    {\n")
    file.write("      intensity %s\n" % (lamp.energy / 10.0))
    file.write("      color %s %s %s\n" % (lamp.col[0], lamp.col[1],
      lamp.col[2]))
    #file.write("      location %s\n" % ())
    #file.write("      direction %s\n" % ())
    file.write("      dropOffRate %s\n" % (lamp.spotBlend))
    file.write("      cutOffAngle %s\n" % (lamp.spotSize * math.pi / 180.0))
    file.write("    }\n")

# script main function
def ExportToIv(file_name):
  scene = Blender.Scene.GetCurrent()
  file = open(file_name, "w")

  # make lists of individual object types
  meshes = []
  lamps = []
  cameras = []
  for object in Blender.Object.Get():
    if object.getType() == "Mesh":
      meshes.append(object);
    elif object.getType() == "Lamp":
      lamps.append(object);
    elif object.getType() == "Camera":
      cameras.append(object);
    else:
      print "Exporting %s objects isn't supported!" % object.getType()

  # write header, footer and groups of object types
  WriteHeader(file);
  #for camera in cameras:
  #  WriteCamera(file, camera);
  #for lamp in lamps:
  #  WriteLamp(file, lamp)
  for mesh in meshes:
    WriteMesh(file, mesh)
  WriteFooter(file)

  file.close()

def FileSelectorCB(file_name):
  if(file_name.find('.iv', -3) <= 0):
    file_name += '.iv'
  ExportToIv(file_name)

Blender.Window.FileSelector(FileSelectorCB, "Export IV")
