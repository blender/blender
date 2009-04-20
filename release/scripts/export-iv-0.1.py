#!BPY

"""
Name: 'OpenInventor (.iv)...'
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
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Radek Barton
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
#

import Blender
math_pi= 3.1415926535897931

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

def WriteMesh(file, ob):
	file.write("  Separator\n")
	file.write("  {\n")
	file.write("    # %s\n" % ob.name)
	WriteMatrix(file, ob)
	mesh = ob.getData()
	WriteMaterials(file, mesh)
	WriteTexture(file, mesh)
	WriteNormals(file, mesh)
	WriteVertices(file, mesh)
	WriteFaces(file, mesh)
	file.write("  }\n")

def WriteMatrix(file, ob):
	matrix = ob.getMatrix()
	file.write("    MatrixTransform\n")
	file.write("    {\n")
	file.write("      matrix\n")
	for line in matrix:
		file.write("      %.6f %.6f %.6f %.6f\n" % (line[0], line[1], line[2], line[3]))
	file.write("    }\n")

def WriteColors(file, mesh):
	file.write("      vertexProperty VertexProperty\n")
	file.write("      {\n")
	file.write("        orderedRGBA\n")
	file.write("        [\n")
	for face in mesh.faces:
		for I in xrange(len(face)):
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
			file.write("        %.6f %.6f %.6f,\n" % (mat.mirCol[0], mat.mirCol[1],
				mat.mirCol[2]))
		file.write("      ]\n")
		file.write("      diffuseColor\n")
		file.write("      [\n")
		for mat in mesh.materials:
			file.write("        %.6f %.6f %.6f,\n" % (mat.rgbCol[0], mat.rgbCol[1],
				mat.rgbCol[2]))
		file.write("      ]\n")
		file.write("      specularColor\n")
		file.write("      [\n")
		for mat in mesh.materials:
			file.write("        %.6f %.6f %.6f,\n" % (mat.specCol[0] * mat.spec / 2.0,
				mat.specCol[1] * mat.spec / 2.0,  mat.specCol[2] * mat.spec / 2.0))
		file.write("      ]\n")
		file.write("      emissiveColor\n")
		file.write("      [\n")
		for mat in mesh.materials:
			file.write("        %.6f %.6f %.6f,\n" % (mat.rgbCol[0] * mat.emit,
				mat.rgbCol[1] * mat.emit, mat.rgbCol[0] * mat.emit))
		file.write("      ]\n")
		file.write("      shininess\n")
		file.write("      [\n")
		for mat in mesh.materials:
			file.write("        %.6f,\n" % (mat.hard / 255.0))
		file.write("      ]\n")
		file.write("      transparency\n")
		file.write("      [\n")
		for mat in mesh.materials:
			file.write("        %.6f,\n" % (1.0 - mat.alpha))
		file.write("      ]\n")
		file.write("    }\n")
		file.write("    MaterialBinding\n")
		file.write("    {\n")
		file.write("      value PER_FACE_INDEXED\n")
		file.write("    }\n")

def WriteTexture(file, mesh):
	texture = mesh.faces[0].image # BAD Ju Ju
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
					file.write("        %.6f %.6f,\n" % (uv[0], uv[1]))
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
		file.write("        %.6f %.6f %.6f,\n" % (vert[0], vert[1], vert[2]))
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
		file.write("        %.6f %.6f %.6f,\n" % (no[0], no[1], no[2]))
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
		file.write("        %i,\n" % face.mat);
	file.write("      ]\n")

	# write faces with coordinate indexes
	file.write("      coordIndex\n")
	file.write("      [\n")
	for face in mesh.faces:
		face_v= face.v
		if len(face_v) == 3:
			file.write("        %i, %i, %i, -1,\n" % (face_v[0].index,
				face_v[1].index, face_v[2].index))
		elif len(face_v) == 4:
			file.write("        %i, %i, %i, %i, -1,\n" % (face_v[0].index,
				face_v[1].index, face_v[2].index, face_v[3].index))
	file.write("      ]\n")
	file.write("    }\n")


def WriteCamera(file, ob):
	camera = ob.getData();
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

def WriteLamp(file, ob):
	lamp = ob.getData();
	# spot lamp
	if lamp.type == 2:
		file.write("    SpotLight\n")
		file.write("    {\n")
		file.write("      intensity %s\n" % (lamp.energy / 10.0))
		file.write("      color %s %s %s\n" % (lamp.col[0], lamp.col[1], lamp.col[2]))
		#file.write("      location %s\n" % ())
		#file.write("      direction %s\n" % ())
		file.write("      dropOffRate %s\n" % (lamp.spotBlend))
		file.write("      cutOffAngle %s\n" % (lamp.spotSize * math_pi / 180.0))
		file.write("    }\n")

# script main function
def ExportToIv(file_name):
	scene = Blender.Scene.GetCurrent()
	file = open(file_name, "w")

	# make lists of individual ob types
	meshes = []
	lamps = []
	cameras = []
	for ob in scene.objects:
		obtype= ob.type
		if obtype == "Mesh":
			meshes.append(ob);
		#elif obtype == "Lamp":
		#	lamps.append(ob);
		#elif obtype == "Camera":
		#	cameras.append(ob);
		#else:
		#	print "Exporting %s objects isn't supported!" % ob.type

	# write header, footer and groups of ob types
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
	if not file_name.lower().endswith('.iv'):
		file_name += '.iv'
	ExportToIv(file_name)

if __name__ == '__main__':
	Blender.Window.FileSelector(FileSelectorCB, "Export IV", Blender.sys.makename(ext='.iv'))
