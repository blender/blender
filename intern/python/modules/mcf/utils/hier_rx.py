'''
Simple Hierarchic Walking functions for use with hierobj-type objects.

Provide for recurse-safe processing.  Currently only provide depth-first
processing, and don't provide means for ignoring branches of the tree
during processing.  For an example of breadth-first processing, see
mcf.pars.int.index.indutils.  For more complex hierarchic processing,
see the mcf.walker package.

Originally these functions were only methods of the hierobj class (they
still are methods of it).  I've split them out to allow them to be
imported selectively by other classes (some classes will only want
the simple walking functions, and not want to be bothered with the
methods which hierobj uses to keep track of its particular internal
structures.
'''

def hier_rapply(self, function,arglist=None,argdict={},moreattr = '__childlist__'):
	'''
	Safely apply a function to self and all children for
	the function's side effects. Discard the return values
	that function returns.

	function
		function to apply
	arglist
		(self,)+arglist is the set of arguments passed to function
	argdict
		passed as namedargs to the function
	moreattr
		the attribute representing the children of a node
	'''
	alreadydone = {}
	tobedone = [self]
	if arglist or argdict:
		if not arglist: arglist=[self]
		else:
			arglist.insert(0,self) # we could insert anything... self is convenient
		while tobedone:
			object = tobedone[0]
			try:
				alreadydone[id(object)]
				# We've already processed this object
			except KeyError:
				# We haven't processed this object
				alreadydone[id(object)]=1
				arglist[0]=object
				apply(function,tuple(arglist),argdict)
				try:
					tobedone[1:1]=getattr(object,moreattr)
				except AttributeError:
					# if the object isn't a hierobj, we don't need to recurse into it.
					pass
			del(tobedone[0])
	else: # no arglist or argdict
		while tobedone:
			object = tobedone[0]
			try:
				alreadydone[id(object)]
				# We've already processed this object
			except KeyError:
				# We haven't processed this object
				alreadydone[id(object)]=1
				function(object)
				try:
					tobedone[1:1]=getattr(object,moreattr)
				except AttributeError:
					# if the object isn't a hierobj, we don't need to recurse into it.
					pass
			del(tobedone[0])
def hier_rreturn(self, function,arglist=None,argdict={},moreattr = '__childlist__'):
	'''
	Safely apply a function to self and all children,
	collect the results in a list and return.

	function
		function to apply
	arglist
		(self,)+arglist is the set of arguments passed to function
	argdict
		passed as namedargs to the function
	moreattr
		the attribute representing the children of a node
	'''
	alreadydone = {}
	tobedone = [self]
	results = []
	if arglist or argdict:
		if not arglist: arglist=[self]
		else:
			arglist.insert(0,self) # or anything you feel like
		while tobedone:
			object = tobedone[0]
			try:
				alreadydone[id(object)]
				# We've already processed this object
			except KeyError:
				# We haven't processed this object
				alreadydone[id(object)]=1
				arglist[0]=object
				results.append(apply(function,tuple(arglist),argdict))
				try:
					tobedone[1:1]=getattr(object,moreattr)
				except AttributeError:
					# if the object isn't a hierobj, we don't need to recurse into it.
					pass
			del(tobedone[0])
	else:
		while tobedone:
			object = tobedone[0]
			try:
				alreadydone[id(object)]
				# We've already processed this object
			except KeyError:
				# We haven't processed this object
				alreadydone[id(object)]=1
				results.append(function(object))
				try:
					tobedone[1:1]=getattr(object,moreattr)
				except AttributeError:
					# if the object isn't a hierobj, we don't need to recurse into it.
					pass
			del(tobedone[0])
	return results
def hier_rgetattr(self, attrname, multiple=1, moreattr = '__childlist__'):
	'''
	Recursively collect the values for attrname and
	return as a list.

	attrname
		attribute to collect
	arglist
		(self,)+arglist is the set of arguments passed to function
	argdict
		passed as namedargs to the function
	moreattr
		the attribute representing the children of a node
	'''
	alreadydone = {}
	tobedone = [self]
	results = []
	while tobedone:
		object = tobedone[0]
		try:
			alreadydone[id(object)]
			# We've already processed this object
		except KeyError:
			# We haven't processed this object
			alreadydone[id(object)]=1
			try:
				if multiple:
					results.append(getattr(object, attrname))
				else:
					return getattr(object, attrname)
			except AttributeError:
				pass
			try:
				tobedone[1:1]=getattr(object,moreattr)
			except AttributeError:
				# if the object isn't a hierobj, we don't need to recurse into it.
				pass
		del(tobedone[0])
	return results
def hier_rmethod(self, methodname,arglist=(),argdict={},moreattr = '__childlist__'):
	'''
	return the result of calling every object's method methodname,
	as for hier_rreturn otherwise.

	methodname
		method to call
	arglist
		(self,)+arglist is the set of arguments passed to function
	argdict
		passed as namedargs to the function
	moreattr
		the attribute representing the children of a node
	'''

	alreadydone = {}
	tobedone = [self]
	results = []
	while tobedone:
		object = tobedone[0]
		try:
			alreadydone[id(object)]
			# We've already processed this object
		except KeyError:
			# We haven't processed this object
			alreadydone[id(object)]=1
			try:
				results.append(apply(getattr(object,methodname),arglist,argdict))
			except:
				pass
			try:
				tobedone[1:1]=getattr(object,moreattr)
			except AttributeError:
				# if the object isn't a hierobj, we don't need to recurse into it.
				pass
		del(tobedone[0])
	return results

