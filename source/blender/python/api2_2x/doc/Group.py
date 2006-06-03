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
  for ob in Object.GetSelected():
    print 'Object Group Settings'
    print ob.name, ob.getType()
    print 'enableDupVerts:', ob.enableDupVerts
    print 'enableDupFrames:', ob.enableDupFrames
    print 'enableDupGroup:', ob.enableDupGroup
    print 'DupGroup:', ob.DupGroup
    dupe_obs= ob.DupObjects
    print 'num dup obs:', len(dupe_obs)

    for dup_ob, dup_matrix in dupe_obs:
      print '\tDupOb', dup_ob.name
      new_ob= Object.New(dup_ob.getType())
      new_ob.shareFrom(dup_ob)
      scn.link(new_ob)
      new_ob.setMatrix(dup_matrix)
      new_ob.sel= 1 # select all real instances.

    ob.sel=0 # Desel the original object

  Window.RedrawAll()

Example::

  # Make a new group with the selected objects, and add an instance of this group.

  from Blender import *
  
  scn= Scene.GetCurrent()
  
  # New Group
  gp= Group.New('mygroup')
  gp.objects= Object.GetSelected()
  
  # Instance the group at an empty using dupligroups
  ob= Object.New('Empty')
  scn.link(ob)
  ob.enableDupGroup= True
  ob.DupGroup= gp
  Window.RedrawAll()
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
  @type group: group
  @param group: A group to remove from this blend file, does not remove objects that this group uses.
  """


class Group:
  """
  The Group object
  ================
    This object gives access to Groups in Blender.
  @ivar name: The name of this Group object.
  @ivar users: Number of users this group has (read only)
  @ivar objects: Objects that this group uses.
  This is an iterator with list like access so use list(gp.objects) if you need to use a list. (where gp is a group object).
  The groups objects can be set by assigning a list or iterator of objects to the groups objects.
  objects.append() and objects.remove() also work with the the objects iterator just like with lists.
  """

