#!BPY

"""
Name: 'Envelope Symmetry'
Blender: 234
Group: 'Animation'
Tooltip: 'Make envelope symmetrical'
"""

__author__ = "Jonas Petersen"
__url__ = ("blender", "blenderartists.org", "Script's homepage, http://www.mindfloaters.de/blender/", "thread at blender.org, http://www.blender.org/modules.php?op=modload&name=phpBB2&file=viewtopic&t=4858 ")
__version__ = "0.9 2004-11-10"
__doc__ = """\
This script creates perfectly symmetrical envelope sets.  It is part of the
envelop assignment tools.

"Envelopes" are Mesh objects with names following this naming convention:

<bone name>:<float value>

Please check the script's homepage and the thread at blender.org (last link button above) for more info.

For this version users need to edit the script code to change default options.
"""

# --------------------------------------------------------------------------
# "Envelope Symmetry" by Jonas Petersen
# Version 0.9 - 10th November 2004 - first public release
# --------------------------------------------------------------------------
#
# A script for creating perfectly symmetrical envelope sets. It is
# part of the envelope assignment tool.
#
# It is available in Object Mode via the menu item:
#
#   Object -> Scripts -> Envelope Symmetry
#
# With default settings it will:
#
# - Look for bones
#
# Find the latest version at: http://www.mindfloaters.de/blender/
#
# --------------------------------------------------------------------------
# $Id$
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004: Jonas Petersen, jonas at mindfloaters dot de
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
# CONFIGURATION
# --------------------------------------------------------------------------

# Note: Theses values will later be editable via a gui interface
# within Blender.

# The suffix for the reference and opposite envelope.
# The configuration of of the opposite envelope will be overwritten by
# the configuration of the reference envelope (shape, position, bone, weight).
# The default is REF_SUFFIX = '.L' and OPP_SUFFIX = '.R'.
REF_SUFFIX = '.R'
OPP_SUFFIX = '.L'

# MIRROR_AXIS defines the axis in which bones are mirrored/aligned.
# Values:
#   0 for X (default)
#   1 for Y
#   2 for Z
MIRROR_AXIS = 0

# SEPARATOR is the character used to delimit the bone name and the weight
# in the envelope name.
SEPARATOR = ":"

# --------------------------------------------------------------------------
# END OF CONFIGURATION
# --------------------------------------------------------------------------

import Blender, math, sys
from Blender import Mathutils
from BPyNMesh import *

def flipFace(v):
	if len(v) == 3: v[0], v[1], v[2] = v[2], v[1], v[0]
	elif len(v) == 4: v[0], v[1], v[2], v[3] = v[3], v[2], v[1], v[0]

# return object with given object name (with variable parts) and mesh name
def getObjectByName(obj_name, mesh_name):
	for obj in Blender.Scene.GetCurrent().objects:
		if obj.type == "Mesh":
#			if obj.getName()[0:len(obj_name)] == obj_name and obj.getData().name == mesh_name:
			# use only mesh_name so bone name and weight (in the envelope name)
			# can be changed by the user and mirrored by the script.
			if obj.getData(name_only=1) == mesh_name:
				return obj
	return False

SUFFIX_LEN = len(REF_SUFFIX);

Blender.Window.EditMode(0)

count = 0
for obj in Blender.Scene.GetCurrent().objects:
	if obj.type != 'Mesh':
		continue

	count += 1
	name = obj.name
	pos = name.find(SEPARATOR)
	if (pos > -1):
		ApplySizeAndRotation(obj)

		base_name = name[0:pos-SUFFIX_LEN]
		suffix = name[pos-SUFFIX_LEN:pos]
		weight = name[pos:len(name)] # the SEPARATOR following a float value

		if suffix == REF_SUFFIX:
			mesh = obj.getData()
			mirror_name = base_name +  OPP_SUFFIX + weight
			mirror_mesh_name = mesh.name + ".mirror"

			mirror_obj = getObjectByName(base_name + OPP_SUFFIX, mirror_mesh_name)

			if mirror_obj:

				# update vertices

				mirror_mesh = mirror_obj.getData()
				for i in xrange(len(mesh.verts)):
					org = mesh.verts[i]
					mir = mirror_mesh.verts[i]
					mir.co[0], mir.co[1], mir.co[2] = org.co[0], org.co[1], org.co[2]
					mir.co[MIRROR_AXIS] *= -1

				mirror_mesh.update()
			else:

				# create mirror object

				mirror_mesh = obj.data
				for face in mirror_mesh.faces:
					flipFace(face.v)
				for vert in mirror_mesh.verts:
					vert.co[MIRROR_AXIS] *= -1

				mirror_obj = Blender.NMesh.PutRaw(mirror_mesh, mirror_mesh_name)

			# update name, drawType and location
			
			mirror_obj.setName(mirror_name)
			mirror_obj.drawType = obj.drawType

			loc = [obj.LocX, obj.LocY, obj.LocZ]
			loc[MIRROR_AXIS] *= -1
			mirror_obj.setLocation(loc)

Blender.Window.EditMode(0)
