'''
DictBool:
Simplistic (and slow) implementation of Boolean operations for
dictionaries... really these should be implemented in C, but I
can't do that till I have MSVC++, which I don't really want to
buy... this will have to do in the meantime.

>>> from mcf.utils import dictbool

>>> a = {1:2}; b = {2:3}; c={4:5,6:7,8:9,1:5}

>>> dictbool.union(a,b,c) # overwrite a with b and the result with c

{1: 5, 2: 3, 4: 5, 8: 9, 6: 7}

>>> dictbool.collectunion(a,b,c) # collect all possible for each key

{1: [2, 5], 2: [3], 4: [5], 8: [9], 6: [7]}

>>> dictbool.intersect(a,b,c) # no common elements in all three

{}

>>> dictbool.intersect(a,c) # one element is common to both

{1: [2, 5]}
'''

def union(*args):
	'''
	Build a new dictionary with the key,val from all args,
	first overwritten by second, overwritten by third etc. 
	Rewritten for Python 1.5 on 98.03.31
	'''
	temp = {}
	for adict in args:
		# following is the 1.5 version
		temp.update(adict)
#		for key,val in adict.items():
#			temp[key] = val
	return temp

def collectunion(*args):
	'''
	As union, save instead of overwriting, all vals are
	returned in lists, and duplicates are appended to those
	lists.
	'''
	temp = {}
	for adict in args:
		for key,val in adict.items():
			try:
				temp[key].append(val)
			except KeyError:
				temp[key] = [val]
	return temp

def intersect(*args):
	'''
	Build a new dictionary with those keys common to all args,
	the vals of the new dict are lists of length len(args), where
	list[ind] is the value of args[ind] for that key.
	'''
	args = map(lambda x: (len(x),x), args)
	args.sort()
	temp = {}
	master = args[0][1]
	rest = map(lambda x: x[1], args[1:])
	for var,val in master.items():
		tempval = [val]
		for slave in rest:
			try:
				tempval.append(slave[var])
			except KeyError:
				tempval = None
				break
		if tempval:
			temp[var] = tempval
	return temp

