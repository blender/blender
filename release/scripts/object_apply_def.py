#!BPY

"""
Name: 'Apply Deformation'
Blender: 242
Group: 'Object'
Tooltip: 'Make copys of all the selected objects with modifiers, softbodies and fluid baked into a mesh'
""" 

__author__ = "Martin Poirier (theeth), Jean-Michel Soler (jms), Campbell Barton (ideasman)"
# This script is the result of merging the functionalities of two other:
# Martin Poirier's Apply_Def.py and
# Jean-Michel Soler's Fix From Everything

__url__ = ("http://www.blender.org", "http://blenderartists.org", "http://jmsoler.free.fr")
__version__ = "1.6 07/07/2006"

__bpydoc__ = """\
This script creates "raw" copies of deformed meshes.

Usage:

Select any number of Objects and run this script.  A fixed copy of each selected object
will be created, with the word "_def" appended to its name. If an object with
the same name already exists, it appends a number at the end as Blender itself does.

Objects in Blender can be deformed by armatures, lattices, curve objects and subdivision,
but this will only change its appearance on screen and rendered
images -- the actual mesh data is still simpler, with vertices in an original
"rest" position and less vertices than the subdivided version.

Use this script if you want a "real" version of the deformed mesh, so you can
directly manipulate or export its data.

This script will work with object types: Mesh, Metaballs, Text3d, Curves and Nurbs Surface.
"""


# $Id$
#
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2003: Martin Poirier, theeth@yahoo.com
#
# Thanks to Jonathan Hudson for help with the vertex groups part
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


import Blender
import bpy
import BPyMesh

def copy_vgroups(source_ob, target_ob):
	
	source_me = source_ob.getData(mesh=1)
	
	vgroups= source_me.getVertGroupNames()
	if vgroups:
		ADD= Blender.Mesh.AssignModes.ADD
		target_me = target_ob.getData(mesh=1)
		for vgroupname in vgroups:
			target_me.addVertGroup(vgroupname)
			if len(target_me.verts) == len(source_me.verts):
				try: # in rare cases this can raise an 'no deform groups assigned to mesh' error
					vlist = source_me.getVertsFromGroup(vgroupname, True)
				except:
					vlist = []
				
				try:
					for vpair in vlist:
						target_me.assignVertsToGroup(vgroupname, [vpair[0]], vpair[1], ADD)
				except:
					pass


def apply_deform():
	scn= bpy.data.scenes.active
	#Blender.Window.EditMode(0)
	
	NAME_LENGTH = 19
	SUFFIX = "_def"
	SUFFIX_LENGTH = len(SUFFIX)
	# Get all object and mesh names
	

	ob_list = list(scn.objects.context)
	ob_act = scn.objects.active
	
	# Assume no soft body
	has_sb= False
	
	# reverse loop so we can remove objects (metaballs in this case)
	for ob_idx in xrange(len(ob_list)-1, -1, -1):
		ob= ob_list[ob_idx]
		
		ob.sel = 0 # deselect while where checking the metaballs
		
		# Test for a softbody
		if not has_sb and ob.isSB():
			has_sb= True
		
		# Remove all numbered metaballs because their disp list is only on the main metaball (un numbered)
		if ob.type == 'MBall':
			name= ob.name
			# is this metaball numbered?
			dot_idx= name.rfind('.') + 1
			if name[dot_idx:].isdigit():
				# Not the motherball, ignore it.
				del ob_list[ob_idx]
			
	
	if not ob_list:
		Blender.Draw.PupMenu('No objects selected, nothing to do.')
		return
	
	
	if has_sb:
		curframe=Blender.Get('curframe')
		for f in xrange(curframe):
			Blender.Set('curframe',f+1)
			Blender.Window.RedrawAll()

	used_names = [ob.name for ob in Blender.Object.Get()]
	used_names.extend(Blender.NMesh.GetNames())
	
	
	deformedList = []
	for ob in ob_list:
		
		# Get the mesh data
		new_me= BPyMesh.getMeshFromObject(ob, vgroups=False)
		
		if not new_me:
			continue # Object has no display list
		
		
		name = ob.name
		new_name = "%s_def" % name[:NAME_LENGTH-SUFFIX_LENGTH]
		num = 0
		
		while new_name in used_names:
			new_name = "%s_def.%.3i" % (name[:NAME_LENGTH-(SUFFIX_LENGTH+SUFFIX_LENGTH)], num)
			num += 1
		used_names.append(new_name)
		
		new_me.name= new_name
		
		new_ob= scn.objects.new(new_me)
		new_ob.setMatrix(ob.matrixWorld)
		
		# Make the active duplicate also active
		if ob == ob_act:
			scn.objects.active = new_ob
		
		# Original object was a mesh? see if we can copy any vert groups.
		if ob.type =='Mesh':
			copy_vgroups(ob, new_ob)
	
	Blender.Window.RedrawAll()

if __name__=='__main__':
	apply_deform()
