class SingletonList:
	'''
	A SingletonList always has a length of one or 0,
	appends overwrite the single element, iteration will
	return precisely one element.  Attempts to get any item
	other than 0 will raise an IndexError or return the single
	item depending on whether the 'raiseIndexError' flag is
	true or false (generally it should be true except if the
	for x in SingletonList: construct is known never to be
	used, since this construct will create an infinite loop
	if we never raise an IndexError).
	'''
	def __init__(self,base=None,raiseIndexError=1):
		self._base = base
		self.raiseIndexError = raiseIndexError
	def __len__(self):
		'''
		The length is 0 if no _base, 1 if a base
		'''
		if hasattr(self, '_base'):
			return 1
		else:
			return 0
	def __getitem__(self,ind):
		'''
		Get the item if ind == 0, else raise an IndexError or return
		the item, depending on the raiseIndexError flag
		'''
		if ind == 0:
			try:
				return self._base
			except AttributeError:
				raise IndexError, ind
		elif self.raiseIndexError:
			raise IndexError, ind
		else:
			return self._base
	def __setitem__(self,ind, item):
		'''
		The item is to become the base
		'''
		self._base = item
	def __delitem__(self,ind):
		'''
		Delete the base, regardless of the index used
		'''
		try:
			del(self._base)
		except AttributeError:
			raise IndexError, ind
	def append(self,item):
		'''
		Replace the base with the item
		'''
		self._base = item
	def index(self,item):
		'''
		if the item is the base, return the only valid index (0)
		'''
		try:
			if item == self._base:
				return 0
		except:
			pass
		raise ValueError, item
	def count(self, item):
		'''
		If the item is the base, we have one, else 0
		'''
		try:
			if item == self._base:
				return 1
		except:
			pass
		return 0
	insert = __setitem__
	def remove(self, item):
		'''
		if the item is the base, delete the base, else ValueError
		'''
		try:
			if item == self._base:
				del(self._base)
				return
		except:
			pass
		raise ValueError, item
	def reverse(self):
		pass
	def sort(self):
		pass
	def __repr__(self):
		try:
			return '[%s]'%`self._base`
		except AttributeError:
			return '[]'
	# store and copy functions
#	def __getinitargs__(self):
#		return (self._base,self.raiseIndexError)
#	def __getstate__(self,*args,**namedargs):
#		pass
#	def __setstate__(self,*args,**namedargs):
#		pass

