'''
Generic Hierarchic Objects Module
Hierobj's store their children (which can be anything) in their
__childlist__ attribute, and provide methods for walking the 
hierarchy, either collecting results or not.

The index function returns an index of the objects (effectively a
flattened copy of the hierarchy)

97-03-17 Added ability to pass arguments to hier_rapply and hier_rreturn.
97-10-31 Removed dependencies on mcf.store
'''
#import copy,types
import types
import singletonlist, hier_rx

class Hierobj: 
	'''
	An abstract class which handles hierarchic functions and information
	# remade as a DAG 97-04-02, also reduced memory overhead for
	hier-r* functions by using while-del-IndexError construct versus
	for loop (probably makes it slower though)
	If you require a true hierarchy, use the TrueHierobj class below...
	'''
	def __init__(self, parent=None, childlist=None):
		if parent is None: # passed no parents
			self.__dict__['__parent__'] = []
		elif type(parent) == types.ListType: # passed a list of parents
			self.__dict__['__parent__'] = parent
		else: # passed a single parent
			self.__dict__['__parent__'] = [parent]
		self.__dict__['__childlist__'] = childlist or []
		for child in self.__childlist__:
			try:
				child.__parent__.append(self)
			except:
				pass
	# import simple hierarchic processing methods
	hier_rapply = hier_rx.hier_rapply
	hier_rreturn = hier_rx.hier_rreturn
	hier_rgetattr = hier_rx.hier_rgetattr
	hier_rmethod = hier_rx.hier_rmethod


	def hier_addchild(self, child):
		'''
		Add a single child to the childlist
		'''
		self.__childlist__.append(child)
		try:
			# Hierobj-aware child
			child.__parent__.append(self) # raises error if not hier_obj aware
		except (TypeError, AttributeError):
			# Non Hierobj-aware child
			pass
	append = hier_addchild
	def hier_remchild(self, child):
		'''
		Breaks the child relationship with child, including the
		reciprocal parent relationship
		'''
		try:
			self.__childlist__.remove(child)
			try:
				child.hier_remparent(self) # if this fails, no problem
			except AttributeError: pass
		except (AttributeError,ValueError):
			return 0 # didn't manage to remove the child
		return 1 # succeeded
	def hier_remparent(self, parent):
		'''
		Normally only called by hier_remchild of the parent,
		just removes the parent from the child's parent list,
		but leaves child in parent's childlist
		'''
		try:
			self.__parent__.remove(parent)
		except (AttributeError,ValueError):
			return 0
		return 1
	def hier_replacewith(self,newel):
		'''
		As far as the hierarchy is concerned, the new element
		is exactly the same as the old element, it has all
		the same children, all the same parents.  The old
		element becomes completely disconnected from the hierarchy,
		but it still retains all of its references
		
		For every parent, replace this as a child
		For every child, replace this as the parent
		'''
		for parent in self.__parent__:
			try:
				parent.hier_replacechild(self, newel)
			except AttributeError:
				pass
		for child in self.__childlist__:
			try:
				child.hier_replaceparent(self,parent)
			except AttributeError:
				pass
	def hier_replaceparent(self, oldparent, newparent):
		ind = self.__parent__.index(oldparent)
		self.__parent__[ind] = newparent
	def hier_replacechild(self, oldchild, newchild):
		ind = self.__childlist__.index(oldchild)
		self.__childlist__[ind] = newchild
		
class TrueHierobj(Hierobj):
	'''
	An inefficient implementation of an Hierobj which limits the
	__parent__ attribute to a single element.  This will likely be
	_slower_ than an equivalent Hierobj.  That will have to be fixed
	eventually.
	'''
	def __init__(self, parent=None, childlist=[]):
		if parent is None: # passed no parents
			self.__dict__['__parent__'] = singletonlist.SingletonList()
		else: # passed a single parent
			self.__dict__['__parent__'] = singletonlist.SingletonList(parent)
		self.__dict__['__childlist__'] = copy.copy(childlist)
		for child in self.__childlist__:
			try:
				child.__parent__.append(self)
			except:
				pass

def index(grove):
	'''
	Returns a flattened version of the grove
	'''
	return grove.hier_rreturn(lambda x: x)


