'''
Generic quoting functions (very fast),
generalised to allow use in any number of
situations, but normally you'll want to create
a new function based on these patterns which
has the default args you need.  This will
prevent an extra function call.
'''
import string, regex
# create a translator which is fully worked out...

def _quote(somestring,trans,start='"',stop='"'):
	'''
	Return a quoted version of somestring.
	'''
	# would be _so_ much better if we could use the
	# getitem, consider...
	# return '%s%s%s'%(start,string.join(map(trans.__getitem__, somestring), ''),stop)
	temp = list(somestring)
	for charno in xrange(len(temp)):
		temp[charno]= trans[temp[charno]]
	return '%s%s%s'%(start,string.join(temp, ''),stop)

def compilerex(trans):
	'''
	Compiles a suitable regex from a dictionary
	translation table.  Should be used at design
	time in most cases to improve speed.  Note:
	is not a very intelligent algo.  You could
	do better by creating a character-class []
	for the single-character keys and then the
	groups for the or-ing after it, but I've not
	got the time at the moment.
	'''
	keyset = trans.keys()
	multitrans = []
	for x in range(len(keyset)):
		if len(keyset[x]) != len(trans[keyset[x]]):
			multitrans.append((keyset[x],trans[keyset[x]]))
		if len(keyset[x])!= 1:
			keyset[x] = '\(%s\)'%keyset[x]
	if multitrans:
		return 1,regex.compile(string.join(keyset,'\|'))


def quote2(somestring,trans,rex,start='',stop=''):
	'''
	Should be a faster version of _quote once
	the regex is built.  Rex should be a simple
	or'ing of all characters requiring substitution,
	use character ranges whereever possible (should
	be in most cases)
	'''
	temp = list(somestring)
	curpos = 0
	try:
		while 	rex.search(somestring,curpos) != -1:
			pos = rex.regs[0]
			print pos
			replacement = list(trans[rex.group(0)])
			temp[pos[0]:pos[1]] = replacement
			curpos = pos[0]+len(replacement)
	except (IndexError,regex.error):
		pass
	return '%s%s%s'%(start,string.join(temp, ''),stop)
# compatability
_quote2 = quote2

def reprq(obj, qtype):
	'''
	Return representation of a string obj as a string with qtype 
	quotes surrounding it.  Usable when linearising Python objects
	to languages which have only a particular type of string. (Such
	as VRML).  This is not a generalised nor a particularly reliable
	solution.  You should use the _quote2 function instead.
	'''
	return '%s%s%s'%(qtype,string.join(string.split(string.join(string.split(obj, '\\'), '\\\\'), qtype), '\\%s'%qtype),qtype)

