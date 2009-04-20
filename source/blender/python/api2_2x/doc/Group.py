# Blender.Group module and the Group PyType object

"""
The Blender.Group submodule.

Group
=====

This module provides access to B{Group} data in Blender.

Example::

	# Make Dupli's Real, as a python script.

	from Blender import *

	scn= Scene.GetCurrent()
	for ob in scn.objects:
		print 'Object Group Settings'
		print ob.name, ob.type
		print 'enableDupVerts:', ob.enableDupVerts
		print 'enableDupFrames:', ob.enableDupFrames
		print 'enableDupGroup:', ob.enableDupGroup
		print 'DupGroup:', ob.DupGroup
		dupe_obs= ob.DupObjects
		print 'num dup obs:', len(dupe_obs)

		for dup_ob, dup_matrix in dupe_obs:
			print '\tDupOb', dup_ob.name
			scn.objects.new(dup_ob.data)
			new_ob.setMatrix(dup_matrix)
			new_ob.sel= 1 # select all real instances.

		ob.sel=0 # Desel the original object

	Window.RedrawAll()

Example::

	# Make a new group with the selected objects, and add an instance of this group.

	from Blender import *
	
	scn= Scene.GetCurrent()
	
	# New Group
	grp= Group.New('mygroup')
	grp.objects= scn.objects
	
	# Instance the group at an empty using dupligroups
	ob= scn.objects.new(None)
	ob.enableDupGroup= True
	ob.DupGroup= grp
	Window.RedrawAll()
	
	
Example::

	# Remove all non mesh objects from a group.

	from Blender import *
	
	scn= Scene.GetCurrent()
	
	# New Group
	grp= Group.Get('mygroup')
	for ob in list(grp.objects): # Convert to a list before looping because we are removing items
		if ob.type != 'Mesh':
			grp.objects.unlink(ob)
"""

def New (name = None):
	"""
	Make a new empty group, name optional, default is "Group"
	@type name: string
	@param name: The name of the new group.
	@rtype:  Blender Group
	@return: A Empty Blender Group object
	"""

def Get (name = None):
	"""
	Get the Group object(s) from Blender.
	@type name: string
	@param name: The name of the Group object.
	@rtype: Blender Group or a list of Blender Groups
	@return: It depends on the I{name} parameter:
			- (name): The Group object called I{name}, Exception if it is not found.
			- (): A list with all Group objects in the current blend file.
	"""

def Unlink (group):
	"""
	Unlink (delete) this group from Blender.
	@Note: No objects will be removed, just the group that references them.
	@type group: group
	@param group: A group to remove from this blend file, does not remove objects that this group uses.
	"""


class Group:
	"""
	The Group object
	================
		This object gives access to Groups in Blender.
	@ivar layers: Layer bitmask for this group.
	@type layers: int
	@ivar dupliOffset: Object offset when instanced as a dupligroup
	@type dupliOffset: vector
	@ivar objects: Objects that this group uses.
		This is a sequence with-list like access so use list(grp.objects) if you need to use a list (where grp is a group).
		The groups objects can be set by assigning a list or iterator of objects to the groups objects.
		objects.link() and objects.unlink() also work with the the objects iterator just like with lists.

		B{Note}: append() and remove() have been deprecated and replaced by link() and unlink(),
		after Blender 2.43 append() and remove() will not be available.
	@type objects: custom object sequence
	"""

	def __copy__ ():
		"""
		Make a copy of this group
		@rtype: Group
		@return:  a copy of this group
		"""
	def copy ():
		"""
		Make a copy of this group
		@rtype: Group
		@return:  a copy of this group
		"""

import id_generics
Group.__doc__ += id_generics.attributes 

