# the clean target

import os
import shutil

def DoClean(dir2clean):
	"""
	Do a removal of the root_build_dir the fast way
	"""
	
	print "start the clean"
	dirs = os.listdir(dir2clean)
	for dir in dirs:
		if os.path.isdir(dir2clean + "/" + dir) == 1:
			print "clean dir %s"%(dir2clean+"/" + dir)
			shutil.rmtree(dir2clean+"/" + dir)
	print "done"