'''
Hierarchic 'Dummy' objects
'''

import hierobj, dummy

class HierobjDummy(hierobj.Hierobj,dummy.Dummy):
	'''
	An Hierarchic Dummy object, which provides direct access to its
	children through object[x] interfaces, allows "index" "count"
	etceteras by returning the corresponding attributes of the _base.
	'''
	def __init__(self, parent=None, childlist=None):
		hierobj.Hierobj.__init__(self, parent, childlist)
		self._base = self.__childlist__ #set by init function above

