# VRML node prototype class (SGbuilder)
# Wed Oct 31 16:18:35 CET 2001

'''Prototype2 -- VRML 97 sceneGraph/Node/Script/ROUTE/IS implementations'''
#import copy, types # extern
import types # extern
import strop as string # builtin
from utils import typeclasses, err, namespace # XXX
## TODO: namespace must go


class baseProto:
	def __vrmlStr__( self, **namedargs ):
		'''Generate a VRML 97-syntax string representing this Prototype
		**namedargs -- key:value
			passed arguments for the linearisation object
		see lineariser4.Lineariser
		'''
#		import lineariser4 
		lineariser = apply( lineariser4.Lineariser, (), namedargs )
		return apply( lineariser.linear, ( self, ), namedargs )

	toString = __vrmlStr__ 
	# added stuff for linking support for target scenegraph
	def setTargetnode(self, node):
		self.__dict__['_targetnode'] = node
	def getTargetnode(self):
		try:
			return self.__dict__['_targetnode']
		except:
			return None

class Prototype(baseProto):
	''' A VRML 97 Prototype object
	
	A Prototype is a callable object which produces Node instances
	the Node uses a pointer to its Prototype to provide much of the
	Node's standard functionality.

	Prototype's are often stored in a sceneGraph's protoTypes namespace,
	where you can access them as sceneGraph.protoTypes.nodeGI . They are
	also commonly found in Nodes' PROTO attributes.
	
	Attributes:
		__gi__ -- constant string "PROTO"
		nodeGI -- string gi
			The "generic identifier" of the node type, i.e. the name of the node
		fieldDictionary -- string name: (string name, string dataType, boolean exposed)
		defaultDictionary -- string name: object defaultValue
			Will be blank for EXTERNPROTO's and Script prototypes
		eventDictionary -- string name: (string name, string dataType, boolean eventOut)
		sceneGraph -- object sceneGraph
		MFNodeNames -- list of field name strings
			Allows for easy calculation of "children" nodes
		SFNodeNames -- list of field name strings
			Allows for easy calculation of "children" nodes
	'''
	__gi__ = "PROTO"
	def __init__(self, gi, fieldDict=None, defaultDict=None, eventDict=None, sGraph=None):
		'''
		gi -- string gi
			see attribute nodeGI
		fieldDict -- string name: (string name, string dataType, boolean exposed)
			see attribute fieldDictionary
		defaultDict -- string name: object defaultValue
			see attribute defaultDictionary
		eventDict -- string name: (string name, string dataType, boolean eventOut)
			see attribute eventDictionary
		sceneGraph -- object sceneGraph
			see attribute sceneGraph
		'''
		self.nodeGI = checkName( gi )
		self.fieldDictionary = {}
		self.defaultDictionary = {}
		self.eventDictionary = {}
		self.SFNodeNames = []
		self.MFNodeNames = []
		self.sceneGraph = sGraph

		# setup the fields/events
		for definition in (fieldDict or {}).values():
			self.addField( definition, (defaultDict or {}).get( definition[0]))
		for definition in (eventDict or {}).values():
			self.addEvent( definition )
			
	def getSceneGraph( self ):
		''' Retrieve the sceneGraph object (may be None object)
		see attribute sceneGraph'''
		return self.sceneGraph
	def setSceneGraph( self, sceneGraph ):
		''' Set the sceneGraph object (may be None object)
		see attribute sceneGraph'''
		self.sceneGraph = sceneGraph
	def getChildren(self, includeSceneGraph=None, includeDefaults=1, *args, **namedargs):
		''' Calculate the current children of the PROTO and return as a list of nodes
		if includeDefaults:
			include those default values which are node values
		if includeSceneGraph:
			include the sceneGraph object if it is not None

		see attribute MFNodeNames
		see attribute SFNodeNames
		see attribute sceneGraph
		'''
		temp = []
		if includeDefaults:
			for attrname in self.SFNodeNames:
				try:
					temp.append( self.defaultDictionary[attrname] )
				except KeyError: # sceneGraph object is not copied...
					pass
			for attrname in self.MFNodeNames:
				try:
					temp[len(temp):] = self.defaultDictionary[attrname]
				except KeyError:
					pass
		if includeSceneGraph and self.sceneGraph:
			temp.append( self.getSceneGraph() )
		return temp
	def addField (self, definition, default = None):
		''' Add a single field definition to the Prototype
		definition -- (string name, string dataType, boolean exposed)
		default -- object defaultValue
		
		see attribute fieldDictionary
		see attribute defaultDictionary
		'''
		if type (definition) == types.InstanceType:
			definition = definition.getDefinition()
			default = definition.getDefault ()
		self.removeField( definition[0] )
		self.fieldDictionary[definition [0]] = definition
		if default is not None:
			default = fieldcoercian.FieldCoercian()( default, definition[1] )
			self.defaultDictionary [definition [0]] = default
		if definition[1] == 'SFNode':
			self.SFNodeNames.append(definition[0])
		elif definition[1] == 'MFNode':
			self.MFNodeNames.append(definition[0])
	def removeField (self, key):
		''' Remove a single field from the Prototype
		key -- string fieldName
			The name of the field to remove
		'''
		if self.fieldDictionary.has_key (key):
			del self.fieldDictionary [key]
		if self.defaultDictionary.has_key (key):
			del self.defaultDictionary [key]
		for attribute in (self.SFNodeNames, self.MFNodeNames):
			while key in attribute:
				attribute.remove(key)
	def addEvent(self, definition):
		''' Add a single event definition to the Prototype
		definition -- (string name, string dataType, boolean eventOut)
		
		see attribute eventDictionary
		'''
		if type (definition) == types.InstanceType:
			definition = definition.getDefinition()
		self.eventDictionary[definition [0]] = definition
	def removeEvent(self, key):
		''' Remove a single event from the Prototype
		key -- string eventName
			The name of the event to remove
		'''
		if self.eventDictionary.has_key (key):
			del self.eventDictionary [key]
	def getField( self, key ):
		'''Return a Field or Event object representing a given name
		key -- string name
			The name of the field or event to retrieve
			will attempt to match key, key[4:], and key [:-8]
			corresponding to key, set_key and key_changed

		see class Field
		see class Event
		'''
#		print self.fieldDictionary, self.eventDictionary
		for tempkey in (key, key[4:], key[:-8]):
			if self.fieldDictionary.has_key( tempkey ):
				return Field( self.fieldDictionary[tempkey], self.defaultDictionary.get(tempkey) )
			elif self.eventDictionary.has_key( tempkey ):
				return Event( self.eventDictionary[tempkey] )
		raise AttributeError, key
	def getDefault( self, key ):
		'''Return the default value for the given field
		key -- string name
			The name of the field
			Will attempt to match key, key[4:], and key [:-8]
			corresponding to key, set_key and key_changed

		see attribute defaultDictionary
		'''
		for key in (key, key[4:], key[:-8]):
			if self.defaultDictionary.has_key( key ):
				val = self.defaultDictionary[key]
				if type(val) in typeclasses.MutableTypes:
					val = copy.deepcopy( val )
				return val
			elif self.fieldDictionary.has_key( key ):
				'''We have the field, but we don't have a default, we are likely an EXTERNPROTO'''
				return None
		raise AttributeError, key
	def setDefault (self, key, value):
		'''Set the default value for the given field
		key -- string name
			The name of the field to set
		value -- object defaultValue
			The default value, will be checked for type and coerced if necessary
		'''
		field = self.getField (key)
		self.defaultDictionary [field.name]= field.coerce (value)
	def clone( self, children = 1, sceneGraph = 1 ):
		'''Return a copy of this Prototype
		children -- boolean
			if true, copy the children of the Prototype, otherwise include them
		sceneGraph -- boolean
			if true, copy the sceneGraph of the Prototype
		'''
		if sceneGraph:
			sceneGraph = self.sceneGraph
		else:
			sceneGraph = None
		# defaults should always be copied before modification, but this is still dangerous...
		defaultDictionary = self.defaultDictionary.copy()
		if not children:
			for attrname in self.SFNodeNames+self.MFNodeNames:
				try:
					del defaultDictionary[attrname]
				except KeyError: # sceneGraph object is not copied...
					pass
		# now make a copy
		if self.__gi__ == "PROTO":
			newNode = self.__class__(
				self.nodeGI,
				self.fieldDictionary,
				defaultDictionary,
				self.eventDictionary,
				sceneGraph,
			)
		else:
			newNode = self.__class__(
				self.nodeGI,
				self.url,
				self.fieldDictionary,
				self.eventDictionary,
			)
		return newNode
	def __call__(self, *args, **namedargs):
		'''Create a new Node instance associated with this Prototype
		*args, **namedargs -- passed to the Node.__init__
		see class Node
		'''
		node = apply( Node, (self, )+args, namedargs )
		return node
	def __repr__ ( self ):
		'''Create a simple Python representation'''
		return '''%s( %s )'''%( self.__class__.__name__, self.nodeGI )

class ExternalPrototype( Prototype ):
	'''Sub-class of Prototype

	The ExternalPrototype is a minor sub-classing of the Prototype
	it does not have any defaults, nor a sceneGraph
	
	Attributes:
		__gi__ -- constant string "EXTERNPROTO"
		url -- string list urls
			implementation source for the ExternalPrototype
	'''
	__gi__ = "EXTERNPROTO"
	def __init__(self, gi, url=None, fieldDict=None, eventDict=None):
		'''
		gi -- string gi
			see attribute nodeGI
		url -- string list url
			MFString-compatible list of url's for EXTERNPROTO
		fieldDict -- string name: (string name, string dataType, boolean exposed)
			see attribute fieldDictionary
		eventDict -- string name: (string name, string dataType, boolean eventOut)
			see attribute eventDictionary
		'''
		if url is None:
			url = []
		self.url = url
		Prototype.__init__( self, gi, fieldDict=fieldDict, eventDict=eventDict)
	

from vrml import fieldcoercian # XXX
class Field:
	''' Representation of a Prototype Field
	The Field object is a simple wrapper to provide convenient
	access to field coercian and meta- information
	'''
	def __init__( self, specification, default=None ):
		self.name, self.type, self.exposure = specification
		self.default = default
	def getDefinition (self):
		return self.name, self.type, self.exposure
	def getDefault (self):
		return self.default
	def coerce( self, value ):
		''' Coerce value to the appropriate dataType for this Field '''
		return fieldcoercian.FieldCoercian()( value,self.type,  )
	def __repr__( self ):
		if hasattr (self, "default"):
			return '%s( (%s,%s,%s), %s)'%( self.__class__.__name__, self.name, self.type, self.exposure, self.default)
		else:
			return '%s( (%s,%s,%s),)'%( self.__class__.__name__, self.name, self.type, self.exposure)
	def __str__( self ):
		if self.exposure:
			exposed = "exposedField"
		else:
			exposed = field
		if hasattr (self, "default"):
			default = ' ' + str( self.default)
		else:
			default = ""
		return '%s %s %s%s'%(exposed, self.type, self.name, default)

class Event (Field):
	def __str__( self ):
		if self.exposure:
			exposed = "eventOut"
		else:
			exposed = "eventIn"
		return '%s %s %s'%(exposed, self.type, self.name)


### Translation strings for VRML node names...
translationstring = '''][0123456789{}"'#,.\\ \000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023'''
NAMEFIRSTCHARTRANSLATOR = string.maketrans( translationstring, '_'*len(translationstring) )
translationstring = '''][{}"'#,.\\ \000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023'''
NAMERESTCHARTRANSLATOR = string.maketrans( translationstring, '_'*len(translationstring) )
del translationstring
def checkName( name ):
	'''Convert arbitrary string to a valid VRML id'''
	if type(name) is types.StringType:
		if not name:
			return name
		return string.translate( name[:1], NAMEFIRSTCHARTRANSLATOR) + string.translate( name[1:], NAMERESTCHARTRANSLATOR)
	else:
		raise TypeError, "VRML Node Name must be a string, was a %s: %s"%(type(name), name)

class Node(baseProto):
	''' A VRML 97 Node object
	
	A Node object represents a VRML 97 node.  Attributes of the Node
	can be set/retrieved with standard python setattr/getattr syntax.
	VRML 97 attributes may be passed to the constructor as named
	arguments.
	
	Attributes:
		__gi__ -- string PROTOname
		DEF -- string DEFName
			The DEF name of the node, will be coerced to be a valid
			identifier (with "" being considered valid)
		PROTO -- Prototype PROTO
			The node's Prototype object
		attributeDictionary -- string name: object value
			Dictionary in which VRML 97 attributes are stored
	'''
	DEF = '' # the default name for all nodes (arbitrary)
	def __init__(self, PROTO, name='', attrDict=None, *args, **namedargs):
		'''Normally this method is only called indirectly via the Prototype() interface
		PROTO -- Prototype PROTO
			see attribute PROTO
		name -- string DEFName
			see attribute DEF
		attrDict -- string name: object value
			see attribute attributeDictionary
		**namedargs -- string name: object value
			added to attrDict to create attributeDictionary
		'''
		self.__dict__["PROTO"] = PROTO
		self.DEF = name
		self.__dict__["attributeDictionary"] = {}
##		print attrDict, namedargs
		for dict in (attrDict or {}), namedargs:
			if dict:
				for key, value in dict.items ():
					self.__setattr__( key, value, check=1 )

	def __setattr__( self, key, value, check=1, raw=0 ):
		'''Set attribute on Node
		key -- string attributeName
		value -- object attributeValue
		check -- boolean check
			if false, put values for unrecognized keys into __dict__
			otherwise, raise an  AttributeError
		'''
		if key == "DEF":
			self.__dict__["DEF"] = checkName( value )
			return None
		elif key == "PROTO":
			self.__dict__["PROTO"] = value
		try:
			field = self.PROTO.getField( key )
			if (hasattr( value, "__gi__") and value.__gi__ == "IS") or raw:
				self.attributeDictionary[ field.name] = value
			else:
				self.attributeDictionary[ field.name] = field.coerce( value )
		except ValueError, x:
			raise ValueError( "Could not coerce value %s into value of VRML type %s for %s node %s's field %s"%( value, field.type, self.__gi__, self.DEF, key), x.args)
		except (AttributeError), x:
			if check:
				raise AttributeError("%s is not a known field for node %s"%(key, repr(self)))
			else:
				self.__dict__[key] = value
	def __getattr__( self, key, default = 1 ):
		''' Retrieve an attribute when standard lookup fails
		key -- string attributeName
		default -- boolean default
			if true, return the default value if the node does not have local value
			otherwise, raise AttributeError
		'''
		if key != "attributeDictionary":
			if self.__dict__.has_key( key):
				return self.__dict__[ key ]
			elif self.attributeDictionary.has_key( key):
				return self.attributeDictionary[key]
			if key != "PROTO":
				if key == "__gi__":
					return self.PROTO.nodeGI
				elif default:
					try:
						default = self.PROTO.getDefault( key )
						if type( default ) in typeclasses.MutableTypes:
							# we need a copy, not the original
							default = copy.deepcopy( default )
							self.__setattr__( key, default, check=0, raw=1 )
						return default
					except AttributeError:
						pass
		raise AttributeError, key
	def __delattr__( self, key ):
		''' Delete an attribute from the Node
		key -- string attributeName
		'''
		if key != "attributeDictionary":
			if self.attributeDictionary.has_key( key):
				del self.attributeDictionary[key]
			elif self.__dict__.has_key( key):
				del self.__dict__[ key ]
		raise AttributeError, key
			
	def __repr__(self):
		''' Create simple python representation '''
		return '<%s(%s): %s>'%(self.__gi__, `self.DEF`, self.attributeDictionary.keys() )
	def getChildrenNames( self, current = 1, *args, **namedargs ):
		''' Get the (current) children of Node
		returns two lists:  MFNode children, SFNode children
		current -- boolean currentOnly
			if true, only return current children
			otherwise, include all potential children
		'''
		MFNODES, SFNODES = self.PROTO.MFNodeNames, self.PROTO.SFNodeNames
		mns, sns = [],[]
		for key in MFNODES:
			if current and self.attributeDictionary.has_key(key):
				mns.append(key)
			elif not current:
				mns.append(key)
		for key in SFNODES:
			if self.attributeDictionary.has_key(key):
				sns.append(key)
			elif not current:
				sns.append(key)
		return mns,sns
	def calculateChildren(self, *args, **namedargs):
		'''Calculate the current children of the Node as list of Nodes
		'''
		MFNODES, SFNODES = self.getChildrenNames( )
		temp = []
		for key in MFNODES:
			try:
				temp.extend( self.__getattr__( key, default=0 ) )
			except AttributeError:
				pass
		for key in SFNODES:
			try:
				temp.append( self.__getattr__(key, default = 0 ) )
			except AttributeError:
				pass
		return temp
	def clone(self, newclass=None, name=None, children=None, attrDeepCopy=1, *args, **namedargs):
		'''Return a copy of this Node
		newclass -- object newClass  or None
			optionally use a different Prototype as base
		name -- string DEFName or None or 1
			if 1, copy from current
			elif None, set to ""
			else, set to passed value
		children -- boolean copyChildren
			if true, copy the children of this node
			otherwise, skip children
		attrDeepCopy -- boolean deepCopy
			if true, use deepcopy
			otherwise, use copy
		'''
		if attrDeepCopy:
			cpy = copy.deepcopy
		else:
			cpy = copy.copy
		newattrs = self.attributeDictionary.copy()
		if not children:
			mnames,snames = self.getChildrenNames( )
			for key in mnames+snames:
				try:
					del(newattrs[key])
				except KeyError:
					pass
		for key, val in newattrs.items():
			if type(val) in typeclasses.MutableTypes:
				newattrs[key] = cpy(val)
		# following is Node specific, won't work for sceneGraphs, scripts, etceteras
		if name == 1: # asked to copy the name
			name = self.DEF
		elif name is None: # asked to clear the name
			name = ''
		if not newclass:
			newclass = self.PROTO
		return newclass( name, newattrs )
	def __cmp__( self, other, stop=None ):
		''' Compare this node to another object/node
		other -- object otherNode
		stop -- boolean stopIfFailure
			if true, failure to find comparison causes match failure (i.e. considered unequal)
		'''
			
		if hasattr( other, '__gi__') and other.__gi__ == self.__gi__:
			try:
				return cmp( self.DEF, other.DEF) or cmp( self.attributeDictionary, other.attributeDictionary )
			except:
				if not stop:
					try:
						return other.__cmp__( self , 1) # 1 being stop...
					except:
						pass
		return -1 # could be one, doesn't really matter

def Script( name="", attrDict=None, fieldDict=None, defaultDict=None, eventDict=None, **namedarguments):
	''' Create a script node (and associated prototype)
	name -- string DEFName
	attrDict -- string name: object value
		see class Node.attributeDictionary
	fieldDict -- string name: (string name, string dataType,  boolean exposure)
		see class Prototype.fieldDictionary
	defaultDict -- string name: object value
		see class Prototype.defaultDictionary
	eventDict -- string name: (string name, string dataType, boolean eventOut)
	'''
	fieldDictionary = {
		'directOutput':('directOutput', 'SFBool',0),
		'url':('url',"MFString",0),
		'mustEvaluate':('mustEvaluate', 'SFBool',0),
	}
	fieldDictionary.update( fieldDict or {})
	defaultDictionary = {
		"directOutput":0,
		"url":[],
		"mustEvaluate":0,
	}
	defaultDictionary.update( defaultDict or {})
	PROTO = Prototype(
		"Script",
		fieldDictionary,
		defaultDictionary ,
		eventDict = eventDict,
	)
	if attrDict is not None:
		attrDict.update( namedarguments )
	else:
		attrDict = namedarguments
	return PROTO( name, attrDict )


class NullNode:
	'''NULL SFNode value
	There should only be a single NULL instance for
	any particular system.  It should, for all intents and
	purposes just sit there inertly
	'''
	__gi__ = 'NULL'
	DEF = ''
	__walker_is_temporary_item__ = 1 # hacky signal to walking engine not to reject this node as already processed
	def __repr__(self):
		return '<NULL vrml SFNode>'
	def __vrmlStr__(self,*args,**namedargs):
		return ' NULL '
	toString = __vrmlStr__
	def __nonzero__(self ):
		return 0
	def __call__(self, *args, **namedargs):
		return self
	def __cmp__( self, other ):
		if hasattr( other, '__gi__') and other.__gi__ == self.__gi__:
			return 0
		return -1 # could be one, doesn't really matter
	def clone( self ):
		return self
NULL = NullNode()
	
class fieldRef:
	'''IS Prototype field reference
	'''
	__gi__ = 'IS'
	DEF = ''
	def __init__(self, declaredName):
		self.declaredName = declaredName
	def __repr__(self):
		return 'IS %s'%self.declaredName
	def __vrmlStr__(self,*args,**namedargs):
		return 'IS %s'%self.declaredName
	toString = __vrmlStr__
	def __cmp__( self, other ):
		if hasattr( other, '__gi__') and other.__gi__ == self.__gi__:
			return cmp( self.declaredName, other.declaredName )
		return -1 # could be one, doesn't really matter
	def clone( self ):
		return self.__class__( self.declaredName )
		
IS = fieldRef

class ROUTE:
	''' VRML 97 ROUTE object
	The ROUTE object keeps track of its source and destination nodes and attributes
	It generally lives in a sceneGraph's "routes" collection
	'''
	__gi__ = 'ROUTE'
	def __init__( self, fromNode, fromField, toNode, toField ):
		if type(fromNode) is types.StringType:
			raise TypeError( "String value for ROUTE fromNode",fromNode)
		if type(toNode) is types.StringType:
			raise TypeError( "String value for ROUTE toNode",toNode)
		self.fromNode = fromNode
		self.fromField = fromField
		self.toNode = toNode
		self.toField = toField
	def __getitem__( self, index ):
		return (self.fromNode, self.fromField, self.toNode, self.toField)[index]
	def __setitem__( self, index, value ):
		attribute = ("fromNode","fromField","toNode", "toField")[index]
		setattr( self, attribute, value )
	def __repr__( self ):
		return 'ROUTE %s.%s TO %s.%s'%( self.fromNode.DEF, self.fromField, self.toNode.DEF, self.toField )
	def clone( self ):
		return self.__class__( 
			self.fromNode,
			self.fromField,
			self.toNode,
			self.toField,
		)


class sceneGraph(baseProto):
	''' A VRML 97 sceneGraph
	Attributes:
		__gi__ -- constant string "sceneGraph"
		DEF -- constant string ""
		children -- Node list
			List of the root children of the sceneGraph, nodes/scripts only
		routes -- ROUTE list
			List of the routes within the sceneGraph
		defNames -- string DEFName: Node node
			Mapping of DEF names to their respective nodes
		protoTypes -- Namespace prototypes
			Namespace (with chaining lookup) collection of prototypes
			getattr( sceneGraph.protoTypes, 'nodeGI' ) retrieves a prototype
	'''
	__gi__ = 'sceneGraph'
	DEF = ''
	def __init__(self, root=None, protoTypes=None, routes=None, defNames=None, children=None, *args, **namedargs):
		'''
		root -- sceneGraph root or Dictionary root or Module root or None
			Base object for root of protoType namespace hierarchy
		protoTypes -- string nodeGI: Prototype PROTO
			Dictionary of prototype definitions
		routes -- ROUTE list or (string sourcenode, string sourceeventOut, string destinationnode, string destinationeventOut) list
			List of route objects or tuples to be added to the sceneGraph
			see attribute routes
		defNames -- string DEFName: Node node
			see attribute defNames
		children -- Node list
			see attribute children
		'''
		if children is None:
			self.children = []
		else:
			self.children = children
		if routes is None:
			self.routes = [] # how will we efficiently handle routes?
		else:
			self.routes = routes
		if defNames == None:
			self.defNames = {} # maps 'defName':Node
		else:
			self.defNames = defNames
		if protoTypes is None:
			protoTypes = {}
		if root is None:
			from vrml import basenodes # XXX
			self.protoTypes = namespace.NameSpace(
				protoTypes,
				children = [namespace.NameSpace(basenodes)]
			)
		else: # there is a root file, so need to use it as the children instead of basenodes...
			if hasattr( root, "protoTypes"):
				self.protoTypes = namespace.NameSpace(
					protoTypes,
					children = [root.protoTypes]
				)
			else:
				self.protoTypes = namespace.NameSpace(
					protoTypes,
					children = [ namespace.NameSpace(root) ]
				)
	def __getinitargs__( self ):
		# we only copy our explicit protos, our routes, our defNames, and our children
		# inherited protos will be pulled along by their nodes...
		return None, self.protoTypes._base, self.routes, self.defNames, self.children
	def __getstate__( self ):
		return {}
	def __setstate__( self, dict ):
		pass
	def __del__( self, id=id ):
		'''
		Need to clean up the namespace's mutual references,
		this can be done without affecting the cascade by just
		eliminating the key/value pairs.  The namespaces will
		no longer contain the prototypes, but they will still
		chain up to the higher-level namespaces, and the nodes
		will have those prototypes still in use.
		'''
##		print 'del sceneGraph', id(self )
		try:
##			import pdb
##			pdb.set_trace()
##			self.protoTypes.__dict__.clear()
			self.protoTypes._base.clear()
			del self.protoTypes.__namespace_cascade__[:]
		except:
			print 'unable to free references'
		
	def addRoute(self, routeTuple, getNewNodes=0):
		''' Add a single route to the sceneGraph
		routeTuple -- ROUTE route or (string sourcenode, string sourceeventOut, string destinationnode, string destinationeventOut)
		getNewNodes -- boolean getNewNodes
			if true, look up sourcenode and destinationnode within the current defNames to determine source/destination nodes
			otherwise, just use current if available
		'''
		# create and wire together the Routes here,
		# should just be a matter of pulling the events and passing the nodes...
##		import pdb
##		pdb.set_trace()
		if type( routeTuple) in ( types.TupleType, types.ListType):
			(fromNode, fromField, toNode, toField ) = routeTuple
			if type(fromNode) is types.StringType:
				# get the node instead of the string...
				if self.defNames.has_key( fromNode ):
					fromNode = self.defNames[fromNode]
				else:
					err.err( "ROUTE from an unknown node %s "%(routeTuple) )
					return 0
			if type(toNode) is types.StringType:
				# get the node instead of the string...
				if self.defNames.has_key( toNode ):
					toNode = self.defNames[toNode]
				else:
					err.err( "ROUTE to an unknown node %s "%(routeTuple) )
					return 0
			routeTuple = ROUTE( fromNode, fromField, toNode, toField)
		elif getNewNodes:
			# get the nodes with the same names...
			if self.defNames.has_key( routeTuple[0].DEF ):
				routeTuple[0] = self.defNames[routeTuple[0].DEF]
			else:
				err.err( "ROUTE from an unknown node %s "%(routeTuple) )
				return 0
			if self.defNames.has_key( routeTuple[2].DEF ):
				routeTuple[2] = self.defNames[routeTuple[2].DEF]
			else:
				err.err( "ROUTE to an unknown node %s "%(routeTuple) )
				return 0
		# should be a Route node now, append to our ROUTE list...
		self.routes.append(routeTuple)
		return 1
	def regDefName(self, defName, object):
		''' Register a DEF name for a particular object
		defName -- string DEFName
		object -- Node node
		'''
		object.DEF = defName
		self.defNames[defName] = object
	def addProto(self, proto):
		'''Register a Prototype for this sceneGraph
		proto -- Prototype PROTO
		'''
		setattr( self.protoTypes, proto.__gi__, proto )
	#toString = __vrmlStr__ 
	#__vrmlStr__ = toString
##	def __setattr__( self, key, value ):
##		if key == 'protoTypes' and type( value) is types.ListType:
##			import pdb
##			pdb.set_trace()
##			raise TypeError( "Invalid type for protoTypes attribute of sceneGraph %s"%(`value`) )
##		else:
##			self.__dict__[key] = value

DEFAULTFIELDVALUES ={
	"SFBool": 0,
	"SFString": "",
	"SFFloat": 0,
	"SFTime": 0,
	"SFVec3f": (0, 0,0),
	"SFVec2f": (0,0),
	"SFRotation": (0, 1,0, 0),
	"SFInt32": 0,
	"SFImage": (0,0,0),
	"SFColor": (0,0, 0),
	"SFNode": NULL,
	"MFString": [],
	"MFFloat": [],
	"MFTime": [],
	"MFVec3f": [],
	"MFVec2f": [],
	"MFRotation": [],
	"MFInt32": [],
	"MFColor": [],
	"MFNode": [],
}



