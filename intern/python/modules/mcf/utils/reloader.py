import sys, string

class Reloader:
	'''
	Class allows for reloading all modules imported
	after the instance is created. Normally you will
	use this by doing:
		import <anything you don't want reloaded>
		from mcf.utils import reloader
		<do testing and rewriting>
		reloader.go()
	'''
	def __init__(self):
		self.keys = sys.modules.keys()
	def __call__(self, *args, **namedargs):
		done = []
		for key, val in sys.modules.items():
			if key not in self.keys:
				try:
					reload( val )
					done.append( key )
				except (ImportError):
					print '''Couldn't reload module:''', key
				except (TypeError): # for None's
					# is a flag to prevent reloading
					pass
		if done:
			print '''Reloaded:''', string.join( done, ', ')
		else:
			print '''No modules reloaded'''

# the default reloader...
go = Reloader()