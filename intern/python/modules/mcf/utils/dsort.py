nullval = (1,)

class DSort:
	'''
	A "dependency" sorting class, used to order elements
	according to declared "dependencies" (many-to-one relationships)
	Is not a beautiful algo, but it works (or seems to)
	Requires hashable values for all elements.
	
	This is a quick hack, use at your own risk!
	
	Basic usage:
		Create a DSort mysorter
		for each element q which is part of the set to sort, call:
			mysorter.rule( dsort.nullval, q)
			# this is not strictly necessary for elements which are
			# dependent on other objects, but it is necessary for
			# those which are not.  Generally it's easiest to call
			# the null rule for each element.
		for each rule x depends on y, call:
			mysorter.rule( x, y)
		when _all_ rules are entered, call
		try:
			sortedlist = mysorter.sort()
		except ValueError:
			handle recursive dependencies here...
		
		
	For an example of real-life use, see the VRML lineariser.
	
	'''
	def __init__(self, recurseError=None ):
		self.dependon = {nullval:[0]}
		self.recurseError = recurseError
	def rule( self, depon, deps):
		'''
		Register a "rule".  Both elements must be hashable values.
		See the class' documentation for usage.
		'''
#		print '''registering rule:''', depon, deps
		if self.dependon.has_key( deps ) and depon is not nullval:
			self.dependon[ deps ].append( depon )
		elif depon is not nullval:
			self.dependon[ deps ] = [-1, depon]
		elif not self.dependon.has_key( deps ):
			self.dependon[ deps ] = [-1 ]
	def sort( self ):
		'''
		Get the sorted results as a list
		'''
		for key, value in self.dependon.items():
			self._dsort( key, value)
		temp = []
		for key, value in self.dependon.items():
			temp.append( (value[0], key) )
		temp.sort()
		temp.reverse()
		temp2 = []
		for x,y in temp:
			temp2.append( y )
		# following adds the elements with no dependencies
		temp2[len(temp2):] = self.dependon[ nullval ][1:]
		return temp2
	def _dsort( self, key, value ):
		if value[0] == -2:
			if self.recurseError:
				raise ValueError, '''Dependencies were recursive!'''
			else:
				if __debug__:
					print '''Recursive dependency discovered and ignored in dsort.Dsort._dsort on %s:%s'''%(key, value)
				return 1 # we know it has at least one reference...
		elif value[0] == -1: # haven't yet calculated this rdepth
			value[0] = -2
			tempval = [0]
			for x in value[1:]:
				try:
					tempval.append( 1 + self._dsort( x, self.dependon[x]) )
				except KeyError:
					self.dependon[ nullval ].append( x ) # is an unreferenced element
					tempval.append( 1 )
			value[0] = max( tempval )
			return value[0]
		else:
			return value[0]
'''
from mcf.utils import dsort
>>> x = dsort.DSort()
>>> map( x.rule, [1,2,2,4,5,4], [2,3,4,5,6,3] )
[None, None, None, None, None, None]
>>> x.sort()
'''