# This is the built in Blender emulation module for os.py
# not all features are implemented yet...

import Blender.sys as bsys

sep = bsys.dirsep # path separator ('/' or '\')

class Path:
	def dirname(self, name):
		return bsys.dirname(name)
	def join(self, a, *p):
		dirsep = bsys.dirsep
		path = a
		for b in p:
			if b[:1] == dirsep:
				path = b
			elif path == '' or path[-1:] == dirsep:
				path = path + b
			else:
				path = path + dirsep + b
		return path
	
path = Path()

