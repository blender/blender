# Blender.Library submodule

"""
The Blender.Library submodule.

Library
=======

This module provides access to objects stored in .blend files.  With it scripts
can append from Blender files to the current scene, like the File->Append
menu entry in Blender does.  It allows programmers to use .blend files as
data files for their scripts.

@warn: This is a new, still experimental module.

Example::
  import Blender
  from Blender import Library

  def f(name):
    open_library(name)

  def open_library(name):
    Library.Open(name)
    groups = Library.LinkableGroups()

    for db in groups:
      print "DATABLOCK %s:" % db
      for obname in Library.Datablocks(db):
        print obname
 
    if 'Object' in groups:
      for obname in Library.Datablocks('Object'):
        Library.Load(obname, 'Object', 0) # note the 0...
      Library.Update()

    Library.Close()
    b.Redraw()

  b.Window.FileSelector(f, "Choose Library", "*.blend")

"""

def Open (filename):
  """
  Open an existing .blend file.  If there was already one open file, it is
  closed first.
  @type filename: string
  @param filename: The filename of a Blender file.
  @rtype: bool
  @return: 1 if successful, 0 otherwise.
  """

def Close ():
  """
  Close the currently open library file, if any.
  """

def getName ():
  """
  Get the filename of the currently open library file.
  @rtype: string
  @return: The open library filename.
  """

def LinkableGroups ():
  """
  Get all the linkable group names from the currently open library file.  These
  are the available groups for linking with the current scene.  Ex: 'Object',
  'Mesh', 'Material', 'Text', etc.
  @rtype: list of strings
  @return: the list of linkable groups.
  """

def Datablocks (group):
  """
  Get all datablock objects of the given 'group' available in the currently
  open library file.
  @type group: string
  @param group: datablock group, see L{LinkableGroups}.
  """

def Load (datablock, group, update = 1):
  """
  Load the given datablock object from the current library file
  @type datablock: string
  @type group: string
  @type update: bool
  @param datablock: an available object name, as returned by L{Datablocks}.
  @param group: an available group name, as returned by L{LinkableGroups}.
  @param update: defines if Blender should be updated after loading this
      object.  This means linking all objects and remaking all display lists,
      so it is potentially very slow.

  @warn: If you plan to load more than one object in sequence, it is
     B{definitely recommended} to set 'update' to 0 in all calls to this
     function and after them call L{Update}.
  """

def Update ():
  """
  Update all links and display lists in Blender.  This function should be
  called after a series of L{Load}(datablock, group, B{0}) calls to make
  everything behave nicely.
  @warn: to use this function, remember to set the third L{Load} parameter to
     zero or each loading will automatically update Blender, which will slow
     down your script and make you look like a lousy programmer.
     Enough warnings :)?
  """

