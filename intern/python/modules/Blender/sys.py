from _Blender.sys import *

sep = dirsep # path separator ('/' or '\')

class Path:
	def dirname(self, name):
		return dirname(name)
	def join(self, a, *p):
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

