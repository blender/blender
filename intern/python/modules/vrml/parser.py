from TextTools import TextTools

from simpleparse import generator

import scenegraph as proto
import strop as string

IMPORT_PARSE_TIME = 0.4
PROGRESS_DEPTH = 5

class UnfinishedError(Exception):
	pass

class Parser:
	def __init__( self, data ):
		self.data = data
		self.position = 0
		self.result = proto.sceneGraph()
		self.finalised = None
		self.sceneGraphStack = [self.result]
		self.prototypeStack = []
		self.nodeStack = []
		self.fieldTypeStack = []
		self.readHeader()
		self.depth = 0
		self.progresscount = 0
	def _lines( self, index=None ):
		if index is None:
			index = self.position
		return TextTools.countlines (self.data[:index])
	def parse( self, progressCallback=None ):
		datalength = float( len( self.data ))
		while self.readNext():
			if progressCallback:
				if not progressCallback(IMPORT_PARSE_TIME * self.position/datalength ):
					raise UnfinishedError(
						"Did not complete parsing, cancelled by user. Stopped at line %s" %(self._lines())
					)
		if self.position < len( self.data ):
			raise UnfinishedError(
				'''Unable to complete parsing of file, stopped at line %s:\n%s...'''%(self._lines(), self.data[self.position:self.position+120])
			)
		return self.result
	def readHeader( self ):
		'''Read the file header'''
		success, tags, next = TextTools.tag( self.data, HEADERPARSER, self.position )
		if success:
			self.datalength = len( self.data )
			#print "header ok"
			return success
		else:
			try:
				self.decompress()
				success, tags, next = TextTools.tag( self.data, HEADERPARSER, self.position )
				self.datalength = len( self.data )
				return success
			except:
				raise ValueError( "Could not find VRML97 header in file!" )
	def readNext( self):
		'''Read the next root-level construct'''
		success, tags, next = TextTools.tag( self.data, ROOTITEMPARSER, self.position )
##		print 'readnext', success
		if self.position >= self.datalength:
			print 'reached file end'
			return None
		if success:
#			print '  successful parse'
			self.position = next
			map (self.rootItem_Item, tags )
			return success
		else:
			return None
	def rootItem (self, (type, start, stop, (item,))):
		''' Process a single root item '''
		self.rootItem_Item( item )
	def rootItem_Item( self, item ):
		result = self._dispatch(item)
		if result is not None:
##			print "non-null result"
##			print id( self.sceneGraphStack[-1] ), id(self.result )
			self.sceneGraphStack[-1].children.append( result )
	def _getString (self, (tag, start, stop, sublist)):
		''' Return the raw string for a given interval in the data '''
		return self.data [start: stop]

	def _dispatch (self, (tag, left, right, sublist)):
		''' Dispatch to the appropriate processing function based on tag value '''
##		print "dispatch", tag
		self.depth += 1
		if self.depth < PROGRESS_DEPTH:
			self.progresscount += 1
		try:
			meth = getattr (self, tag)
		except AttributeError:
			raise AttributeError("Unknown parse tag '%s' found! Check the parser definition!" % (tag))
		ret =  meth( (tag, left, right, sublist) )
		self.depth -= 1
		return ret

	def Proto(self, (tag, start, stop, sublist)):
		''' Create a new prototype in the current sceneGraph '''
		# first entry is always ID
		ID = self._getString ( sublist [0])
		print "PROTO",ID
		newNode = proto.Prototype (ID)
##		print "\t",newNode
		setattr ( self.sceneGraphStack [-1].protoTypes, ID, newNode)
		self.prototypeStack.append( newNode )
		# process the rest of the entries with the given stack
		map ( self._dispatch, sublist [1:] )
		self.prototypeStack.pop( )
	def fieldDecl(self,(tag, left, right, (exposure, datatype, name, field))):
		''' Create a new field declaration for the current prototype'''
		# get the definition in recognizable format
		exposure = self._getString (exposure) == "exposedField"
		datatype = self._getString (datatype)
		name = self._getString (name)
		# get the vrml value for the field
		self.fieldTypeStack.append( datatype )
		field = self._dispatch (field)
		self.fieldTypeStack.pop( )
		self.prototypeStack[-1].addField ((name, datatype, exposure), field)
	def eventDecl(self,(tag, left, right, (direction, datatype, name))):
		# get the definition in recognizable format
		direction = self._getString (direction) == "eventOut"
		datatype = self._getString (datatype)
		name = self._getString (name)
		# get the vrml value for the field
		self.prototypeStack[-1].addEvent((name, datatype, direction))
	def decompress( self ):
		pass
	def ExternProto( self, (tag, start, stop, sublist)):
		''' Create a new external prototype from a tag list'''
		# first entry is always ID
		ID = self._getString ( sublist [0])
		newNode = proto.Prototype (ID)
		setattr ( self.sceneGraphStack [-1].protoTypes, ID, newNode)
		self.prototypeStack.append( newNode )
		# process the rest of the entries with the given stack
		map ( self._dispatch, sublist [1:] )
		self.prototypeStack.pop( )
	def ExtProtoURL( self, (tag, start, stop, sublist)):
		''' add the url to the external prototype '''
##		print sublist
		values = self.MFString( sublist )
		self.prototypeStack[-1].url = values
		return values
	def extFieldDecl(self, (tag, start, stop, (exposure, datatype, name))):
		''' An external field declaration, no default value '''
		# get the definition in recognizable format
		exposure = self._getString (exposure) == "exposedField"
		datatype = self._getString (datatype)
		name = self._getString (name)
		# get the vrml value for the field
		self.prototypeStack[-1].addField ((name, datatype, exposure))
	def ROUTE(self, (tag, start, stop, names )):
		''' Create a new route object, add the current sceneGraph '''
		names = map(self._getString, names)
		self.sceneGraphStack [-1].addRoute( names )
	def Node (self, (tag, start, stop, sublist)):
		''' Create new node, returning the value to the caller'''
##		print 'node'

		if sublist[0][0] == 'name':
			name = self._getString ( sublist [0])
			ID = self._getString ( sublist [1])
			rest = sublist [2:]
		else:
			name = ""
			ID = self._getString ( sublist [0])
			rest = sublist [1:]
		try:
			prototype = getattr ( self.sceneGraphStack [-1].protoTypes, ID)
		except AttributeError:
			#raise NameError ('''Prototype %s used without declaration! %s:%s'''%(ID, start, stop) )
			print ('''### Prototype %s used without declaration! %s:%s'''%(ID, start, stop) )
			
			return None
		newNode = prototype(name)
		if name:
			self.sceneGraphStack [-1].regDefName( name, newNode )
		self.nodeStack.append (newNode)
		map (self._dispatch, rest)
		self.nodeStack.pop ()
##		print 'node finished'
		return newNode
	def Attr(self, (tag, start, stop, (name, value))):
		''' An attribute of a node or script '''
		name = self._getString ( name )
		self.fieldTypeStack.append( self.nodeStack[-1].PROTO.getField( name ).type )
		value = self._dispatch( value )
		self.fieldTypeStack.pop()
		if hasattr( self.nodeStack[-1], "__setattr__" ):
			self.nodeStack[-1].__setattr__( name, value, raw=1 )
		else:
			# use slower coercing versions...
			setattr( self.nodeStack[-1], name, value )
	def Script( self, (tag, start, stop, sublist)):
		''' A script node (can be a root node)'''
		# what's the DEF name...
		if sublist and sublist[0][0] == 'name':
			name = self._getString ( sublist [0])
			rest = sublist [1:]
		else:
			name = ""
			rest = sublist
		# build the script node...
		newNode = proto.Script( name )
		# register with sceneGraph
		if name:
			self.sceneGraphStack [-1].regDefName( name, newNode )
		self.nodeStack.append (newNode)
		map( self._dispatch, rest )
		self.nodeStack.pop ()
		return newNode
	def ScriptEventDecl( self,(tag, left, right, sublist)):
		# get the definition in recognizable format
		direction, datatype, name = sublist[:3] # must have at least these...
		direction = self._getString (direction) == "eventOut"
		datatype = self._getString (datatype)
		name = self._getString (name)
		# get the vrml value for the field
		self.nodeStack[-1].PROTO.addEvent((name, datatype, direction))
		if sublist[3:]:
			# will this work???
			setattr( self.nodeStack[-1], name, self._dispatch( sublist[3] ) )
	def ScriptFieldDecl(self,(tag, left, right, (exposure, datatype, name, field))):
		''' Create a new field declaration for the current prototype'''
		# get the definition in recognizable format
		exposure = self._getString (exposure) == "exposedField"
		datatype = self._getString (datatype)
		name = self._getString (name)
		# get the vrml value for the field
		self.fieldTypeStack.append( datatype )
		field = self._dispatch (field)
		self.fieldTypeStack.pop( )
		self.nodeStack[-1].PROTO.addField ((name, datatype, exposure))
		setattr( self.nodeStack[-1], name, field )
	def SFNull(self, tup):
		''' Create a reference to the SFNull node '''
##		print 'hi'
		return proto.NULL
	def USE( self, (tag, start, stop, (nametuple,) )):
		''' Create a reference to an already defined node'''
		name = self._getString (nametuple)
		if self.depth < PROGRESS_DEPTH:
			self.progresscount += 1
		try:
			node = self.sceneGraphStack [-1].defNames [name]
			return node
		except KeyError:
			raise NameError ('''USE without DEF for node %s  %s:%s'''%(name, start, stop))
	def IS(self, (tag, start, stop, (nametuple,))):
		''' Create a field reference '''
		name = self._getString (nametuple)
		if not self.prototypeStack [-1].getField (name):
			raise Exception (''' Attempt to create IS mapping of non-existent field %s %s:%s'''%(name, start, stop))
		return proto.IS(name)
	def Field( self, (tag, start, stop, sublist)):
		''' A field value (of any type) '''
		
		if sublist and sublist[0][0] in ('USE','Script','Node','SFNull'):
			if self.fieldTypeStack[-1] == 'SFNode':
				return self._dispatch( sublist[0] )
			else:
				return map( self._dispatch, sublist )
		elif self.fieldTypeStack[-1] == 'MFNode':
			return []
		else:
			# is a simple data type...
			function = getattr( self, self.fieldTypeStack[-1] )
			try:
				return function( sublist )
			except ValueError:
				traceback.print_exc()
				print sublist
				raise
			
	def SFBool( self, (tup,) ):
		'''Boolean, in Python tradition is either 0 or 1'''
		return self._getString(tup) == 'TRUE'
	def SFFloat( self, (x,) ):
		return string.atof( self._getString(x) )
	SFTime = SFFloat
	def SFInt32( self, (x,) ):
		return string.atoi( self._getString(x), 0 ) # allow for non-decimal numbers
	def SFVec3f( self, (x,y,z) ):
		return map( string.atof, map(self._getString, (x,y,z)) )
	def SFVec2f( self, (x,y) ):
		return map( string.atof, map(self._getString, (x,y)) )
	def SFColor( self, (r,g,b) ):
		return map( string.atof, map(self._getString, (r,g,b)) )
	def SFRotation( self, (x,y,z,a) ):
		return map( string.atof, map(self._getString, (x,y,z,a)) )

	def MFInt32( self, tuples ):
		result = []
		# localisation
		atoi = string.atoi
		append = result.append
		data = self.data
		for tag, start, stop, children in tuples:
			append( atoi( data[start:stop], 0) )
		return result
	SFImage = MFInt32
	def MFFloat( self, tuples ):
		result = []
		# localisation
		atof = string.atof
		append = result.append
		data = self.data
		for tag, start, stop, children in tuples:
			append( atof( data[start:stop]) )
		return result
	MFTime = MFFloat
	def MFVec3f( self, tuples, length=3, typename='MFVec3f'):
		result = []
		# localisation
		atof = string.atof
		data = self.data
		while tuples:
			newobj = []
			for tag, start, stop, children in tuples[:length]:
				newobj.append( atof(data[start:stop] ))
			if len(newobj) != length:
				raise ValueError(
					'''Incorrect number of elements in %s field at line %s'''%(typename, self._lines(stop))
				)
			result.append( newobj )
			del tuples[:length]
		return result
	def MFVec2f( self, tuples):
		return self.MFVec3f( tuples, length=2, typename='MFVec2f')
	def MFRotation( self, tuples ):
		return self.MFVec3f( tuples, length=4, typename='MFRotation')
	def MFColor( self, tuples ):
		return self.MFVec3f( tuples, length=3, typename='MFColor')
	
	def MFString( self, tuples ):
		bigresult = []
		for (tag, start, stop, sublist) in tuples:
			result = []
			for element in sublist:
				if element[0] == 'CHARNODBLQUOTE':
					result.append( self.data[element[1]:element[2]] )
				elif element[0] == 'ESCAPEDCHAR':
					result.append( self.data[element[1]+1:element[2]] )
				elif element[0] == 'SIMPLEBACKSLASH':
					result.append( '\\' )
			bigresult.append( string.join( result, "") )
		return bigresult
##		result = []
##		for tuple in tuples:
##			result.append( self.SFString( tuple) )
##		return result
	def SFString( self, tuples ):
		'''Return the (escaped) string as a simple Python string'''
		if tuples:
			(tag, start, stop, sublist) = tuples[0]
			if len( tuples ) > 1:
				print '''Warning: SFString field has more than one string value''', self.data[tuples[0][1]:tuples[-1][2]]
			result = []
			for element in sublist:
				if element[0] == 'CHARNODBLQUOTE':
					result.append( self.data[element[1]:element[2]] )
				elif element[0] == 'ESCAPEDCHAR':
					result.append( self.data[element[1]+1:element[2]] )
				elif element[0] == 'SIMPLEBACKSLASH':
					result.append( '\\' )
			return string.join( result, "")
		else:
			raise ValueError( "NULL SFString parsed???!!!" )
	def vrmlScene( self, (tag, start, stop, sublist)):
		'''A (prototype's) vrml sceneGraph'''
		newNode = proto.sceneGraph (root=self.sceneGraphStack [-1])
		self.sceneGraphStack.append (newNode)
		#print 'setting proto sceneGraph', `newNode`
		self.prototypeStack[-1].sceneGraph = newNode
		results = filter (None, map (self._dispatch, sublist))
		if results:
			# items which are not auto-magically inserted into their parent
			for result in results:
				newNode.children.append( result)
		self.sceneGraphStack.pop()

PARSERDECLARATION = r'''header         := -[\n]*
rootItem       := ts,(Proto/ExternProto/ROUTE/('USE',ts,USE,ts)/Script/Node),ts
vrmlScene      := rootItem*
Proto          := 'PROTO',ts,nodegi,ts,'[',ts,(fieldDecl/eventDecl)*,']', ts, '{', ts, vrmlScene,ts, '}', ts
fieldDecl	     := fieldExposure,ts,dataType,ts,name,ts,Field,ts
fieldExposure  := 'field'/'exposedField'
dataType       := 'SFBool'/'SFString'/'SFFloat'/'SFTime'/'SFVec3f'/'SFVec2f'/'SFRotation'/'SFInt32'/'SFImage'/'SFColor'/'SFNode'/'MFBool'/'MFString'/'MFFloat'/'MFTime'/'MFVec3f'/'MFVec2f'/'MFRotation'/'MFInt32'/'MFColor'/'MFNode'
eventDecl      := eventDirection, ts, dataType, ts, name, ts
eventDirection := 'eventIn'/'eventOut'
ExternProto    := 'EXTERNPROTO',ts,nodegi,ts,'[',ts,(extFieldDecl/eventDecl)*,']', ts, ExtProtoURL
extFieldDecl   := fieldExposure,ts,dataType,ts,name,ts
ExtProtoURL    := '['?,(ts,SFString)*, ts, ']'?, ts  # just an MFString by another name :)
ROUTE          := 'ROUTE',ts, name,'.',name, ts, 'TO', ts, name,'.',name, ts
Node           := ('DEF',ts,name,ts)?,nodegi,ts,'{',ts,(Proto/ExternProto/ROUTE/Attr)*,ts,'}', ts
Script         := ('DEF',ts,name,ts)?,'Script',ts,'{',ts,(ScriptFieldDecl/ScriptEventDecl/Proto/ExternProto/ROUTE/Attr)*,ts,'}', ts
ScriptEventDecl := eventDirection, ts, dataType, ts, name, ts, ('IS', ts, IS,ts)?
ScriptFieldDecl := fieldExposure,ts,dataType,ts,name,ts,(('IS', ts,IS,ts)/Field),ts
SFNull         := 'NULL', ts

# should really have an optimised way of declaring a different reporting name for the same production...
USE            := name
IS             := name
nodegi         := name 
Attr           := name, ts, (('IS', ts,IS,ts)/Field), ts
Field          := ( '[',ts,((SFNumber/SFBool/SFString/('USE',ts,USE,ts)/Script/Node),ts)*, ']', ts )/((SFNumber/SFBool/SFNull/SFString/('USE',ts,USE,ts)/Script/Node),ts)+

name           := -[][0-9{}\000-\020"'#,.\\ ],  -[][{}\000-\020"'#,.\\ ]*
SFNumber       := [-+]*, ( ('0',[xX],[0-9]+) / ([0-9.]+,([eE],[-+0-9.]+)?))
SFBool         := 'TRUE'/'FALSE'
SFString       := '"',(CHARNODBLQUOTE/ESCAPEDCHAR/SIMPLEBACKSLASH)*,'"'
CHARNODBLQUOTE :=  -[\134"]+
SIMPLEBACKSLASH := '\134'
ESCAPEDCHAR    := '\\"'/'\134\134'
<ts>           :=  ( [ \011-\015,]+ / ('#',-'\012'*,'\n')+ )*
'''


PARSERTABLE = generator.buildParser( PARSERDECLARATION )
HEADERPARSER = PARSERTABLE.parserbyname( "header" )
ROOTITEMPARSER = PARSERTABLE.parserbyname( "rootItem" )

