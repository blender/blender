'''
Really simplistic walker-processable hierobjects, doesn't
have parent attributes, every element has an __attrDict__
item and a childlist.  This is different from the mechanisms
we'll want to use for multi-tree systems, but it's fairly
close.  Should be fairly simply worked with.
'''
class WalkerAble:
	'''
	Simple hierarchic objects with the following elements
	
	__attrDict__ -- app-specific attributes
	__childlist__ -- childen of this node
	__gi__ -- "type" or Generic Indicator of this node
	__childlist__append__ -- as you'd expect, method on childlist to add an element
	'''
	def __init__(self, childlist=None, attrDict=None, gi=None):
		self.__dict__['__attrDict__'] = attrDict or {}
		self.__dict__['__childlist__'] = childlist or []
		self.__dict__['__gi__'] = gi or ''
		self.__dict__['__childlist__append__'] = self.__childlist__.append

	def __getattr__(self, attrName):
		'''
		Note: you can store attributes with the same names as
		the reserved names, but to get them back, you'll need
		to read it directly out of the attrDict
		'''
		if attrName != '__attrDict__':
			try:
				return self.__attrDict__[attrName]
			except KeyError:
				pass
		raise AttributeError, attrName

	def __setattr__(self, attrName, attrVal):
		self.__attrDict__[attrName] = attrVal
	def __setGI__(self, gi):
		self.__dict__['__gi__'] = gi
	def __repr__(self):
		return '''<WalkerAble %(__gi__)s %(__attrDict__)s %(__childlist__)s>'''%self.__dict__

	# copy functions
#	def __getinitargs__(self):
#		return (self.__childlist__, self.__attrDict__, self.__gi__)

