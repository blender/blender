#!BPY
""" 
Name: 'MilkShape3D (.ms3d)...'
Blender: 245
Group: 'Import'
Tooltip: 'Import from MilkShape3D file format (.ms3d)'
"""
# 
# Author: Markus Ilmola
# Email: markus.ilmola@pp.inet.fi
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

# import needed stuff
import os.path
import math
from math import *
import struct
import Blender
from Blender import Mathutils
from Blender.Mathutils import *


# trims a string by removing ending 0 and everything after it
def uku(s):
	try:
		return s[:s.index('\0')]
	except:
		return s


# Converts ms3d euler angles to a rotation matrix
def RM(a):
	sy = sin(a[2])
	cy = cos(a[2])
	sp = sin(a[1])
	cp = cos(a[1])
	sr = sin(a[0])
	cr = cos(a[0])
	return Matrix([cp*cy, sr*sp*cy+cr*-sy, cr*sp*cy+-sr*-sy],[cp*sy, sr*sp*sy+cr*cy, cr*sp*sy+-sr*cy], [-sp, sr*cp, cr*cp])


# Converts ms3d euler angles to a quaternion
def RQ(a):
	angle = a[2] * 0.5;
	sy = sin(angle);
	cy = cos(angle);
	angle = a[1] * 0.5;
	sp = sin(angle);
	cp = cos(angle);
	angle = a[0] * 0.5;
	sr = sin(angle);
	cr = cos(angle);
	return Quaternion(cr*cp*cy+sr*sp*sy, sr*cp*cy-cr*sp*sy, cr*sp*cy+sr*cp*sy, cr*cp*sy-sr*sp*cy)


# takes a texture filename and tries to load it
def loadImage(path, filename):
	image = None
	try:
		image = Blender.Image.Load(os.path.abspath(filename))
	except IOError:
		print "Warning: Failed to load image: " + filename + ". Trying short path instead...\n"
		try:
			image = Blender.Image.Load(os.path.dirname(path) + "/" + os.path.basename(filename))
		except IOError:
			print "Warning: Failed to load image: " + os.path.basename(filename) + "!\n"
	return image


# imports a ms3d file to the current scene
def import_ms3d(path):
	# get scene
	scn = Blender.Scene.GetCurrent()
	if scn == None:
		return "No scene to import to!"

	# open the file
	try:
		file = open(path, 'rb')
	except IOError:
		return "Failed to open the file!"

	# read id
	id = file.read(10)
	if id!="MS3D000000":
		return "The file is not a MS3D file!"

	# read version
	version = struct.unpack("i", file.read(4))[0]
	if version!=4:
		return "The file has invalid version!"

	# Create the mesh
	scn.objects.selected = []
	mesh = Blender.Mesh.New("MilkShape3D Mesh")
	meshOb = scn.objects.new(mesh)

	# read the number of vertices
	numVertices = struct.unpack("H", file.read(2))[0]

	# read vertices
	coords = []
	boneIds = []
	for i in xrange(numVertices):
		# skip flags
		file.read(1)

		# read coords
		coords.append(struct.unpack("fff", file.read(3*4)))

		# read bone ids 
		boneIds.append(struct.unpack("B", file.read(1))[0])

		# skip refcount		
		file.read(1)

	# add the vertices to the mesh
	mesh.verts.extend(coords)

	# read number of triangles
	numTriangles = struct.unpack("H", file.read(2))[0]
	
	# read triangles
	faces = []
	uvs = []
	for i in xrange(numTriangles):
		# skip flags
		file.read(2)

		# read indices (faces)
		faces.append(struct.unpack("HHH", file.read(3*2)))

		# read normals
		normals = struct.unpack("fffffffff", file.read(3*3*4))

		# read texture coordinates
		s = struct.unpack("fff", file.read(3*4))
		t = struct.unpack("fff", file.read(3*4))

		# store texture coordinates
		uvs.append([[s[0], 1-t[0]], [s[1], 1-t[1]], [s[2], 1-t[2]]])
		
		if faces[-1][2] == 0: # Cant have zero at the third index
			faces[-1] = faces[-1][1], faces[-1][2], faces[-1][0]
			uvs[-1] = uvs[-1][1], uvs[-1][2], uvs[-1][0]
		
		# skip smooth group
		file.read(1)

		# skip group
		file.read(1)

	# add the faces to the mesh
	mesh.faces.extend(faces)

	# set texture coordinates
	for i in xrange(numTriangles):
		mesh.faces[i].uv = [Vector(uvs[i][0]), Vector(uvs[i][1]), Vector(uvs[i][2])]

	# read number of groups
	numGroups = struct.unpack("H", file.read(2))[0]

	# read groups
	for i in xrange(numGroups):
		# skip flags
		file.read(1)

		# skip name
		file.read(32)

		# read the number of triangles in the group
		numGroupTriangles = struct.unpack("H", file.read(2))[0]

		# read the group triangles
		if numGroupTriangles > 0:
			triangleIndices = struct.unpack(str(numGroupTriangles) + "H", file.read(2*numGroupTriangles));

		# read material
		material = struct.unpack("B", file.read(1))[0]
		for j in xrange(numGroupTriangles):
			mesh.faces[triangleIndices[j]].mat = material

	# read the number of materials
	numMaterials = struct.unpack("H", file.read(2))[0]

	# read materials
	for i in xrange(numMaterials):
		# read name
		name = uku(file.read(32))

		# create the material
		mat = Blender.Material.New(name)
		mesh.materials += [mat]

		# read ambient color
		ambient = struct.unpack("ffff", file.read(4*4))[0:3]
		mat.setAmb((ambient[0]+ambient[1]+ambient[2])/3)

		# read diffuse color
		diffuse = struct.unpack("ffff", file.read(4*4))[0:3]
		mat.setRGBCol(diffuse)

		# read specular color
		specular = struct.unpack("ffff", file.read(4*4))[0:3]
		mat.setSpecCol(specular)

		# read emissive color
		emissive = struct.unpack("ffff", file.read(4*4))[0:3]
		mat.setEmit((emissive[0]+emissive[1]+emissive[2])/3)

		# read shininess
		shininess = struct.unpack("f", file.read(4))[0]
		print "Shininess: " + str(shininess)

		# read transparency		
		transparency = struct.unpack("f", file.read(4))[0]
		mat.setAlpha(transparency)
		if transparency < 1:
			 mat.mode |= Blender.Material.Modes.ZTRANSP

		# read mode
		mode = struct.unpack("B", file.read(1))[0]

		# read texturemap
		texturemap = uku(file.read(128))
		if len(texturemap)>0:
			colorTexture = Blender.Texture.New(name + "_texture")
			colorTexture.setType('Image')
			colorTexture.setImage(loadImage(path, texturemap))
			mat.setTexture(0, colorTexture, Blender.Texture.TexCo.UV, Blender.Texture.MapTo.COL)

		# read alphamap
		alphamap = uku(file.read(128))
		if len(alphamap)>0:
			alphaTexture = Blender.Texture.New(name + "_alpha")
			alphaTexture.setType('Image') 
			alphaTexture.setImage(loadImage(path, alphamap))
			mat.setTexture(1, alphaTexture, Blender.Texture.TexCo.UV, Blender.Texture.MapTo.ALPHA)		

	# read animation
	fps = struct.unpack("f", file.read(4))[0]
	time = struct.unpack("f", file.read(4))[0]
	frames = struct.unpack("i", file.read(4))[0]

	# read the number of joints
	numJoints = struct.unpack("H", file.read(2))[0]

	# create the armature
	armature = 0
	armOb = 0
	if numJoints > 0:
		armOb = Blender.Object.New('Armature', "MilkShape3D Skeleton")
		armature = Blender.Armature.New("MilkShape3D Skeleton")
		armature.drawType = Blender.Armature.STICK
		armOb.link(armature)
		scn.objects.link(armOb)
		armOb.makeParentDeform([meshOb])
		armature.makeEditable()

	# read joints
	rotKeys = {}
	posKeys = {}
	for i in xrange(numJoints):
		# skip flags
		file.read(1)

		# read name
		name = uku(file.read(32))

		# create the bone
		bone = Blender.Armature.Editbone()
		armature.bones[name] = bone

		# read parent
		parent = uku(file.read(32))
		if len(parent)>0:
			bone.parent = armature.bones[parent]

		# read orientation
		rot = struct.unpack("fff", file.read(3*4))

		# read position
		pos = struct.unpack("fff", file.read(3*4))

		# set head
		if bone.hasParent():
			bone.head = bone.parent.matrix * Vector(pos) + bone.parent.head
			bone.matrix = bone.parent.matrix * RM(rot)
		else:
			bone.head = Vector(pos)
			bone.matrix = RM(rot)

		# set tail
		bvec = bone.tail - bone.head
		bvec.normalize()
		bone.tail = bone.head + 0.01 * bvec

		# Create vertex group for this bone
		mesh.addVertGroup(name)
		vgroup = []
		for index, v in enumerate(boneIds):
			if v==i:
				vgroup.append(index)
		mesh.assignVertsToGroup(name, vgroup, 1.0, 1)
	
		# read the number of rotation keys
		numKeyFramesRot = struct.unpack("H", file.read(2))[0]
			
		# read the number of postions keys
		numKeyFramesPos = struct.unpack("H", file.read(2))[0]

		# read rotation keys
		rotKeys[name] = []		
		for j in xrange(numKeyFramesRot):
			# read time
			time = fps * struct.unpack("f", file.read(4))[0]
			# read data
			rotKeys[name].append([time, struct.unpack("fff", file.read(3*4))])

		# read position keys
		posKeys[name] = []
		for j in xrange(numKeyFramesPos):
			# read time
			time = fps * struct.unpack("f", file.read(4))[0]
			# read data
			posKeys[name].append([time, struct.unpack("fff", file.read(3*4))])
	
	# create action and pose
	action = 0
	pose = 0
	if armature!=0:
		armature.update()
		pose = armOb.getPose()
		action = armOb.getAction()
		if not action:
			action = Blender.Armature.NLA.NewAction()
			action.setActive(armOb)

	# create animation key frames
	for name, pbone in pose.bones.items():
		# create position keys
		for key in posKeys[name]:
			pbone.loc = Vector(key[1])
			pbone.insertKey(armOb, int(key[0]), Blender.Object.Pose.LOC, True)

		# create rotation keys
		for key in rotKeys[name]:
			pbone.quat = RQ(key[1])
			pbone.insertKey(armOb, int(key[0]), Blender.Object.Pose.ROT, True)
	
	Blender.Redraw()
	
	# close the file
	file.close()

	# succes return empty error string
	return ""


# load the model
def fileCallback(filename):
	error = import_ms3d(filename)
	if error!="":
		Blender.Draw.PupMenu("An error occured during import: " + error + "|Not all data might have been imported succesfully.", 2)

Blender.Window.FileSelector(fileCallback, 'Import')

