# Blender.sys module

"""
The Blender.sys submodule.

sys
===

B{New}: L{expandpath}.

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
  Split 'path' into (root, ext), where 'ext' is a file extension including the full stop.

  Example::

    import Blender
    file, ext= Blender.sys.splitext('/tmp/foobar.blend')
    print file, ext
    # ('/tmp/foobar', '.blend')

  @type path: string
  @param path: a path name
  @rtype: tuple of two strings
  @return: (root, ext)
  @note: This function will raise an error if the path is longer then 80 characters.
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
  @rtype: int
  @return:
      -  0: path does not exist;
      -  1: path is an existing filename;
      -  2: path is an existing dirname;
      - -1: path exists but is neither a regular file nor a dir.
  """

def time ():
  """
  Get the current time in seconds since a fixed value.  Successive calls to
  this function are guaranteed to return values greater than the previous call.
  @rtype: float
  @return: the elapsed time in seconds.
  """

def sleep (millisecs = 10):
  """
  Sleep for the specified amount of time.
  @type millisecs: int
  @param millisecs: the amount of time in milliseconds to sleep.  The default
      is 10 which is 0.1 seconds.
  """

def expandpath (path):
  """
  Expand the given Blender 'path' into an absolute and valid path.
  Internally, Blender recognizes two special character sequences in paths:
    - '//' (used at the beginning): means base path -- the current .blend file's
      dir;
    - '#' characters in the filename will be replaced by the frame number.
  The expanded string can be passed to generic python functions that don't
  understand Blender's internal relative paths.
  @note: this function is also useful for obtaining the name of the image
      that will be saved when rendered.
  @note: if the passed string doesn't contain the special characters it is
    returned unchanged.
  @type path: string
  @param path: a path name.
  @rtype: string
  @return: the expanded (if necessary) path.
  """

def cleanpath (path):
  """
  Clean the given 'path' by removing unneeded components such as "/./" and "/test/../"
  @type path: string
  @param path: a path name.
  @rtype: string
  @return: the cleaned (if necessary) path.
  """
