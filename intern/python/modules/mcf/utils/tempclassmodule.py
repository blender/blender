'''
Generate module for holding temporary classes which
will be reconstructed into the same module to allow
cPickle and the like to properly import them.

Note: You _must_ pickle a reference to the tempclassmodule
_before_ you pickle any instances which use the classes stored
in the module!  Also, the classes cannot reference anything
in their dictionary or bases tuples which are not normally
pickleable (in particular, you can't subclass a class in the
same tempclassmodule or a tempclassmodule which you cannot
guarantee will be loaded before the dependent classes. (i.e.
by guaranteeing they will be pickled first)
'''
import new, time, string, sys, types

def buildModule(packagename, basename, rebuild=None, initialcontents=None):
	'''
	Dynamically build a module or rebuild one, generates
	a persistent ID/name if not rebuilding.  The persistent
	ID is the value of basename+`time.time()` with the decimal 
	point removed (i.e. a long string of digits).  Packagename
	must be an importable package!  Will raise an ImportError
	otherwise.  Also, for easy reconstitution, basename must not
	include any decimal points.
	
	initialcontents is a dictionary (or list) of elements which will be
	added to the new module.
	'''
	if rebuild == None:
		timestamp = `time.time()`
		decpos = string.find(timestamp,'.')
		basename = basename+timestamp[:decpos]+timestamp[decpos+1:]
	name = string.join((packagename, basename), '.')
	a = {}
	b = {}
	try: # see if we've already loaded this module...
		mod = __import__( name, {},{}, string.split( name, '.'))
		if initialcontents:
			_updateFrom(mod, initialcontents)
		return mod.__name__, mod
	except ImportError:
		pass
	mod = new.module(name)
	sys.modules[name] = mod
	# following is just to make sure the package is loaded before attempting to alter it...
	__import__( packagename, {}, {}, string.split(packagename) )
##	exec 'import %s'%(packagename) in a, b ### Security Risk!
	setattr(sys.modules[ packagename ], basename, mod)
	# now do the update if there were initial contents...
	if initialcontents:
		_updateFrom(mod, initialcontents)
	return name, mod

def buildClassIn(module, *classargs, **namedclassargs):
	'''
	Build a new class and register it in the module
	as if it were really defined there.
	'''
	print module, classargs, namedclassargs
	namedclassargs["__temporary_class__"] = 1
	newclass = new.classobj(classargs[0], classargs[1], namedclassargs)
	newclass.__module__ = module.__name__
	setattr(module, newclass.__name__, newclass)
	return newclass

def addClass(module, classobj):
	'''
	Insert a classobj into the tempclassmodule, setting the
	class' __module__ attribute to point to this tempclassmodule
	'''
	classobj.__module__ = module.__name__
	setattr(module, classobj.__name__, classobj)
	setattr( classobj, "__temporary_class__", 1)

def delClass(module, classobj):
	'''
	Remove this class from the module, Note: after running this
	the classobj is no longer able to be pickled/unpickled unless
	it is subsequently added to another module.  This is because
	it's __module__ attribute is now pointing to a module which
	is no longer going to save its definition!
	'''
	try:
		delattr(module, classobj.__name__)
	except AttributeError:
		pass

def _packageName(modulename):
	decpos = string.rfind(modulename, '.')
	return modulename[:decpos], modulename[decpos+1:]

def _updateFrom(module, contentsource):
	'''
	For dealing with unknown datatypes (those passed in by the user),
	we want to check and make sure we're building the classes correctly.
	'''
	# often will pass in a protoNamespace from which to update (during cloning)
	if type(contentsource) in ( types.DictType, types.InstanceType):
		contentsource = contentsource.values()
	# contentsource should now be a list of classes or class-building tuples
	for val in contentsource:
		if type(val) is types.ClassType:
			try:
				addClass(module, val)
			except:
				pass
		elif type(val) is types.TupleType:
			try:
				apply(buildClassIn, (module,)+val)
			except:
				pass

def deconstruct(templatemodule):
	'''
	Return a tuple which can be passed to reconstruct
	in order to get a rebuilt version of the module
	after pickling. i.e. apply(reconstruct, deconstruct(tempmodule))
	is the equivalent of doing a deepcopy on the tempmodule.
	'''
##	import pdb
##	pdb.set_trace()
	classbuilder = []
	for name, classobj in templatemodule.__dict__.items():
		if type(classobj) is types.ClassType: # only copy class objects, could do others, but these are special-purpose modules, not general-purpose ones.
			classbuilder.append( deconstruct_class( classobj) )
##		import pdb
##		pdb.set_trace()
	return (templatemodule.__name__, classbuilder)
##	except AttributeError:
##		print templatemodule
##		print classbuilder
	
def deconstruct_class( classobj ):
	'''
	Pull apart a class into a tuple of values which can be used
	to reconstruct it through a call to buildClassIn
	'''
	if not hasattr( classobj, "__temporary_class__"):
		# this is a regular class, re-import on load...
		return (classobj.__module__, classobj.__name__)
	else:
		# this is a temporary class which can be deconstructed
		bases = []
		for classobject in classobj.__bases__:
			bases.append( deconstruct_class (classobject) )
		return (classobj.__name__, tuple (bases), classobj.__dict__)
			

def reconstruct(modulename, classbuilder):
	'''
	Rebuild a temporary module and all of its classes
	from the structure created by deconstruct.
	i.e. apply(reconstruct, deconstruct(tempmodule))
	is the equivalent of doing a deepcopy on the tempmodule.
	'''
##	import pdb
##	pdb.set_trace()
	mname, newmod = apply(buildModule, _packageName(modulename)+(1,) ) # 1 signals reconstruct
	reconstruct_classes( newmod, classbuilder )
	return newmod

def reconstruct_classes( module, constructors ):
	'''
	Put a class back together from the tuple of values
	created by deconstruct_class.
	'''
	classes = []
	import pprint
	pprint.pprint( constructors)
	for constructor in constructors:
		if len (constructor) == 2:
			module, name = constructor
			# this is a standard class, re-import
			temporarymodule = __import__(
				module,
				{},{},
				string.split(module)+[name]
			)
			classobject =getattr (temporarymodule, name)
		else:
			# this is a class which needs to be re-constructed
			(name, bases,namedarguments) = constructor
			bases = tuple( reconstruct_classes( module, bases ))
			classobject = apply (
				buildClassIn,
				(module, name, bases), # name and bases are the args to the class constructor along with the dict contents in namedarguments
				namedarguments,
			)
		classes.append (classobject)
	return classes
	
	
def destroy(tempmodule):
	'''
	Destroy the module to allow the system to do garbage collection
	on it.  I'm not sure that the system really does do gc on modules,
	but one would hope :)
	'''
	name = tempmodule.__name__
	tempmodule.__dict__.clear() # clears references to the classes
	try:
		del(sys.modules[name])
	except KeyError:
		pass
	packagename, modname = _packageName(name)
	try:
		delattr(sys.modules[ packagename ], modname)
	except AttributeError:
		pass
	del( tempmodule ) # no, I don't see any reason to do it...
	return None
	
	
def deepcopy(templatemodule, packagename=None, basename=None):
	'''
	Rebuild the whole Module and it's included classes
	(just the classes).  Note: This will _not_ make instances
	based on the old classes point to the new classes!
	The value of this function is likely to be minimal given
	this restriction.  For pickling use deconstruct/reconstruct
	for simple copying just return the module.
	'''
	name, classbuilder = deconstruct( templatemodule )
	if packagename is None:
		tp, tb = _packageName( name )
		if packagename is None:
			packagename = tp
		if basename is None:
			basename = tb
	newmod = buildModule(packagename, basename, initialcontents=classbuilder )
	return newmod

if __name__ == "__main__":
	def testPickle ():
		import mcf.vrml.prototype
		name, module = buildModule( 'mcf.vrml.temp', 'scenegraph' )
		buildClassIn( module, 'this', () )
		buildClassIn( module, 'that', (mcf.vrml.prototype.ProtoTypeNode,) )
##		import pdb
##		pdb.set_trace()
		import pprint
		pprint.pprint( deconstruct( module ))
		name,builder = deconstruct( module )
		destroy( module)
		return reconstruct(name, builder)
	t = testPickle()
	print t


	