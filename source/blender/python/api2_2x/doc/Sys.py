# Blender.sys module

"""
The Blender.sys submodule.

sys
===

B{New}: L{exists}, L{makename}, L{join}.

This module provides a minimal set of helper functions and data.  Its purpose
is to avoid the need for the standard Python module 'os', in special 'os.path',
though it is only meant for the simplest cases.

Example::

  import Blender

  filename = ""
  def f(name): # file selector callback
    global filename
    filename = name

  Blender.Window.FileSelector(f)

  if filename:
    print 'basename:', Blender.sys.basename(filename)
    print 'dirname:',  Blender.sys.dirname(filename)
    print 'splitext:', Blender.sys.splitext(filename)

  # what would basename(splitext(filename)[0]) print?

@type sep: char
@var sep: the platform-specific dir separator for this Blender: '/'
    everywhere, except on Win systems, that use '\\'. 
@type dirsep: char
@var dirsep: same as L{sep}.
@type progname: string
@var progname: the Blender executable (argv[0]).

@attention: The module is called sys, not Sys.
"""

def basename (path):
  """
  Get the base name (filename stripped from dir info) of 'path'.
  @type path: string
  @param path: a path name
  @rtype: string
  @return: the base name
  """

def dirname (path):
  """
  Get the dir name (dir path stripped from filename) of 'path'.
  @type path: string
  @param path: a path name
  @rtype: string
  @return: the dir name
  """

def join (dir, file):
  """
  Join the given dir and file paths, using the proper separator for each
  platform.
  @type dir: string
  @type file: string
  @param dir: the dir name, like returned from L{dirname}.
  @param file: the bare filename, like returned from L{basename}.
  @rtype: string
  @return: the resulting filename.
  @warn: this simple function isn't intended to be a complete replacement for
     the standard os.path.join() one, which handles more general cases.
  """

def splitext (path):
  """
  Split 'path' into (root, ext), where 'ext' is a file extension.
  @type path: string
  @param path: a path name
  @rtype: list with two strings
  @return: (root, ext)
  """

def makename (path = "Blender.Get('filename')", ext = "", strip = 0):
  """
  Remove extension from 'path', append extension 'ext' (if given)
  to the result and return it.  If 'strip' is non-zero, also remove
  dirname from path.

  Example::
    import Blender
    from Blender.sys import *
    print makename('/path/to/myfile.txt','.abc', 1) # returns 'myfile.abc'

    print makename('/path/to/myfile.obj', '-01.obj') # '/path/to/myfile-01.obj'

    print makename('/path/to/myfile.txt', strip = 1) # 'myfile'

    # note that:
    print makename(ext = '.txt')
    # is equivalent to:
    print sys.splitext(Blender.Get('filename'))[0]) + '.txt'

  @type path: string
  @param path: a path name or Blender.Get('filename'), if not given.
  @type ext: string
  @param ext: an extension to append.  For flexibility, a dot ('.') is
      not automatically included.
  @rtype: string
  @return: the resulting string
  """

def exists(path):
  """
  Tell if the given pathname (file or dir) exists.
  @rtype: bool
  @return: 1 if 'path' exists, 0 otherwise.
  """

def time ():
  """
  Get the current time in seconds since a fixed value.  Successive calls to
  this function are garanteed to return values greater than the previous call.
  @rtype: float
  @return: the elapsed time in seconds.
  """
