'''
Destructive Functions for "collapsing" Sequences into single levels

>>> from mcf.utils import collapse

>>> collapse.test([[[1],[2,3]],[[]],[4],5,[6]])

[1, 2, 3, 4, 5, 6] # note that is the same root list

>>> collapse.collapse2([[[1],[2,3]],[[]],(4,()),(5,),[6]])

[1, 2, 3, 4, 5, 6] # note is the same root list
'''
#import copy, types, sys
import types, sys
from types import ListType, TupleType # this now only supports the obsolete stuff...

def hyperCollapse( inlist, allowedmap, type=type, list=list, itype=types.InstanceType, maxint= sys.maxint):
	'''
	Destructively flatten a mixed hierarchy to a single level.
	Non-recursive, many speedups and obfuscations by Tim Peters :)
	'''
	try:
		# for every possible index
		for ind in xrange( maxint):
			# while that index currently holds a list
			expandable = 1
			while expandable:
				expandable = 0
				if allowedmap.has_key( type(inlist[ind]) ):
					# expand that list into the index (and subsequent indicies)
					inlist[ind:ind+1] = list( inlist[ind])
					expandable = 1
				
				# alternately you could iterate through checking for isinstance on all possible
				# classes, but that would be very slow
				elif type( inlist[ind] ) is itype and allowedmap.has_key( inlist[ind].__class__ ):
					# here figure out some way to generically expand that doesn't risk
					# infinite loops...
					templist = []
					for x in inlist[ind]:
						templist.append( x)
					inlist[ind:ind+1] = templist
					expandable = 1
	except IndexError:
		pass
	return inlist
	

def collapse(inlist, type=type, ltype=types.ListType, maxint= sys.maxint):
	'''
	Destructively flatten a list hierarchy to a single level. 
	Non-recursive, and (as far as I can see, doesn't have any
	glaring loopholes).
	Further speedups and obfuscations by Tim Peters :)
	'''
	try:
		# for every possible index
		for ind in xrange( maxint):
			# while that index currently holds a list
			while type(inlist[ind]) is ltype:
				# expand that list into the index (and subsequent indicies)
				inlist[ind:ind+1] = inlist[ind]
			#ind = ind+1
	except IndexError:
		pass
	return inlist

def collapse_safe(inlist):
	'''
	As collapse, but works on a copy of the inlist
	'''
	return collapse( inlist[:] )

def collapse2(inlist, ltype=(types.ListType, types.TupleType), type=type, maxint= sys.maxint ):
	'''
	Destructively flatten a list hierarchy to a single level.
	Will expand tuple children as well, but will fail if the
	top level element is not a list.
	Non-recursive, and (as far as I can see, doesn't have any
	glaring loopholes).
	'''
	ind = 0
	try:
		while 1:
			while type(inlist[ind]) in ltype:
				try:
					inlist[ind:ind+1] = inlist[ind]
				except TypeError:
					inlist[ind:ind+1] = list(inlist[ind])
			ind = ind+1
	except IndexError:
		pass
	return inlist

def collapse2_safe(inlist):
	'''
	As collapse2, but works on a copy of the inlist
	'''
	return collapse( list(inlist) )
	
def old_buggy_collapse(inlist):
	'''Always return a one-level list of all the non-list elements in listin,
	rewritten to be non-recursive 96-12-28  Note that the new versions work
	on the original list, not a copy of the original.'''
	if type(inlist)==TupleType:
		inlist = list(inlist)
	elif type(inlist)!=ListType:
		return [inlist]
	x = 0
	while 1:
		try:
			y = inlist[x]
			if type(y) == ListType:
				ylen = len(y)
				if ylen == 1:
					inlist[x] = y[0]
					if type(inlist[x]) == ListType:
						x = x - 1 # need to collapse that list...
				elif ylen == 0:
					del(inlist[x])
					x = x-1 # list has been shortened
				else:
					inlist[x:x+1]=y
			x = x+1
		except IndexError:
			break
	return inlist


def old_buggy_collapse2(inlist):
	'''As collapse, but also collapse tuples, rewritten 96-12-28 to be non-recursive'''
	if type(inlist)==TupleType:
		inlist = list(inlist)
	elif type(inlist)!=ListType:
		return [inlist]
	x = 0
	while 1:
		try:
			y = inlist[x]
			if type(y) in [ListType, TupleType]:
				ylen = len(y)
				if ylen == 1:
					inlist[x] = y[0]
					if type(inlist[x]) in [ListType,TupleType]:
						x = x-1 #(to deal with that element)
				elif ylen == 0:
					del(inlist[x])
					x = x-1 # list has been shortened, will raise exception with tuples...
				else:
					inlist[x:x+1]=list(y)
			x = x+1
		except IndexError:
			break
	return inlist


def oldest_buggy_collapse(listin):
	'Always return a one-level list of all the non-list elements in listin'
	if type(listin) == ListType:
		return reduce(lambda x,y: x+y, map(collapse, listin), [])
	else: return [listin]

def oldest_buggy_collapse2(seqin):

	if type(seqin) in [ListType, TupleType]:
		return reduce(lambda x,y: x+y, map(collapse2, seqin), [])
	else:
		return [seqin]

