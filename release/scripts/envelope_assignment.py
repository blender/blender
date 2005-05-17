#!BPY

"""
Name: 'Envelope Assignment'
Blender: 234
Group: 'Animation'
Tooltip: 'Assigns weights to vertices via envelopes'
"""

__author__ = "Jonas Petersen"
__url__ = ("blender", "elysiun", "Script's homepage, http://www.mindfloaters.de/blender/", "thread at blender.org, http://www.blender.org/modules.php?op=modload&name=phpBB2&file=viewtopic&t=4858")
__version__ = "0.9 2004-11-10"
__doc__ = """\
This script creates vertex groups from a set of envelopes.

"Envelopes" are Mesh objects with names following this naming convention:

<bone name>:<float value>

Notes:<br>
  - All existing vertex groups of the target Mesh will be deleted.

Please check the script's homepage and the thread at blender.org (last link button above) for more info.
"""

# --------------------------------------------------------------------------
# "Armature Symmetry" by Jonas Petersen
# Version 0.9 - 10th November 2004 - first public release
# --------------------------------------------------------------------------
#
# A script that creates vertex groups from a set of envelopes.
#
# Envelopes are Mesh objects with names that follow the following
# naming convention (syntax):
#
#   <bone name>:<float value>
#
# All existing vertex groups of the target Mesh will be deleted.
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

# SEPARATOR is the character used to delimit the bone name and the weight
# in the envelope name.
SEPARATOR = ":"

# --------------------------------------------------------------------------
# END OF CONFIGURATION
# --------------------------------------------------------------------------

import Blender, math, sys, string
from Blender import Mathutils
from Blender.Mathutils import *

def run(target_obj):
	target_mesh = target_obj.getData()

	num_verts = len(target_mesh.verts)
	warn_count = 0
	main_obj_loc = Vector(list(target_obj.getLocation()))

	Blender.Window.EditMode(0)

	def vertGroupExist(group):
		global vert_group_names;
		for name in target_mesh.getVertGroupNames():
			if group == name:
				return True
		return False

	def isInside(point, envl_data):
		for i in range(len(envl_data['normals'])):
			vec = point - envl_data['points'][i]
			if DotVecs(envl_data['normals'][i], vec) > 0.0:
				return False
		return True

	envls = {}

	Blender.Window.DrawProgressBar(0.0, "Parsing Zones")

	# go through all envelopes and fill the 'envls' dict with points, normals
	# and weights of the box faces
	for obj in Blender.Object.Get():
		if obj.getType() == "Mesh":
			name = obj.getName()
			pos = name.find(SEPARATOR)
			if (pos > -1):
				mesh = obj.getData()
				loc = Vector(list(obj.getLocation()))

				bone_name = name[0:pos]
				try:
					weight = float(name[pos+1:len(name)])
				except ValueError:
					print "WARNING: invalid syntax in envelope name \"%s\" - syntax: \"<bone name>:<float value>\""%(obj.getName())
					warn_count += 1
					weight = 0.0

				envl_data = {'points': [], 'normals': [], 'weight': weight}
				for face in mesh.faces:
					envl_data['normals'].append(Vector(list(face.normal)))
					envl_data['points'].append(Vector(list(face.v[0].co)) + loc)

				if not envls.has_key(bone_name):
					# add as first data set
					envls[bone_name] = [envl_data]
				else:
					# add insert in sorted list of data sets
					inserted = False
					for i in range(len(envls[bone_name])):
						if envl_data['weight'] > envls[bone_name][i]['weight']:
							envls[bone_name].insert(i, envl_data)
							inserted = True
					if not inserted:
						envls[bone_name].append(envl_data)

	Blender.Window.DrawProgressBar(0.33, "Parsing Vertices")
	
	assign_count = 0
	vert_groups = {}

	# go throug all vertices of the target mesh
	for vert in target_mesh.verts:
		point = Vector(list(vert.co)) + main_obj_loc

		vert.sel = 1
		counted = False

		for bone_name in envls.keys():
			for envl_data in envls[bone_name]:

				if (isInside(point, envl_data)):

					if (not vert_groups.has_key(bone_name)):
						vert_groups[bone_name] = {}

					if (not vert_groups[bone_name].has_key(envl_data['weight'])):
							vert_groups[bone_name][envl_data['weight']] = []

					vert_groups[bone_name][envl_data['weight']].append(vert.index)

					vert.sel = 0

					if not counted:
						assign_count += 1
						counted = True

					break


	Blender.Window.DrawProgressBar(0.66, "Writing Groups")

	vert_group_names = target_mesh.getVertGroupNames()

	# delete all vertices in vertex groups
	for group in vert_group_names:
		try:
			v = target_mesh.getVertsFromGroup(group)
		except:
			pass
		else:
			# target_mesh.removeVertsFromGroup(group) without second argument doesn't work
			#print "removing", len(v), "vertices from group \"",group,"\""
			target_mesh.removeVertsFromGroup(group, v)

	# delete all vertex groups
	for group in vert_group_names:
		target_mesh.removeVertGroup(group)

	# create new vertex groups and fill them
	if 1:
		for bone_name in vert_groups.keys():
			# add vertex group
			target_mesh.addVertGroup(bone_name)

			for weight in vert_groups[bone_name]:
				print "name: ", bone_name, ": ", weight, "len: ", len(vert_groups[bone_name][weight])
				index_list = vert_groups[bone_name][weight]
				target_mesh.assignVertsToGroup(bone_name, index_list, weight, 'replace')

	target_mesh.update(0)

	Blender.Window.DrawProgressBar(1.0, "")

	if assign_count < num_verts:
		Blender.Window.EditMode(1)
		print '\a'
		if warn_count: warn_msg =  " There is also %d warning(s) in the console."%(warn_count)
		else: warn_msg = ""
		Blender.Draw.PupMenu("Envelope Assignment%%t|%d vertices were not assigned.%s"%(num_verts-assign_count, warn_msg))
	elif warn_count:
		print '\a'
		Blender.Draw.PupMenu("Envelope Assignment%%t|There is %d warning(s) in the console."%(warn_count))

sel_objs = Blender.Object.GetSelected()
if len(sel_objs) != 1 or sel_objs[0].getType() != "Mesh":
	print '\a'
	Blender.Draw.PupMenu("Envelope Assignment%t|Please select 1 Mesh object to assign vertex groups to!")
else:
	if string.find(sel_objs[0].getName(), SEPARATOR) > -1:
		print '\a'
		Blender.Draw.PupMenu("Envelope Assignment%t|Don't use the command on the envelopes themselves!")
	else:
		run(sel_objs[0])
