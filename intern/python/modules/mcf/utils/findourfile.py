'''
This utility allows a python system to find a file in it's
directory.  To do this, you need to pass it a function object from
a module in the correct directory.  I know there must be a better
way to do this, but I haven't seen it yet.  Incidentally, the
current directory should be _different_ from the module in which
the function is contained, otherwise this function will go off into
the root directory.

Currently this has to be called with the current directory a directory
other than the directory we're trying to find... need a better solution
for this kind of thing... a python registry would be great :)

NOTE: as of Python 1.5, this module should be obsolete!  As soon as I
have verified that all of my code is fixed, it will be moved to the unused
directories.
'''
import os,sys

def findourfile(function, filename):
	'''
	Given the function, return a path to the a file in the
	same directory with 'filename'.  We also let the caller
	know if the file already exists.
	'''
	ourfilename = os.path.split(function.func_code.co_filename)[0]+os.sep+filename
	exists = os.path.exists(ourfilename)
	return (exists,ourfilename)


