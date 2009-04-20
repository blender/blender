
## This was used to make V, but faster not to do all that
##valid = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_'
##v = range(255)
##for c in valid: v.remove(ord(c))
v = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,46,47,58,59,60,61,62,63,64,91,92,93,94,96,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254]
invalid = ''.join([chr(i) for i in v])
## del v, c, i, valid
del v, i

def cleanName(name):
	for ch in invalid:	name = name.replace(ch, '_')
	return name

def caseInsensitivePath(path, RET_FOUND=False):
	'''
	Get a case insensitive path on a case sensitive system
	
	RET_FOUND is for internal use only, to avoid too many calls to os.path.exists
	# Example usage
	getCaseInsensitivePath('/hOmE/mE/sOmEpAtH.tXt')
	'''
	import os # todo, what happens with no os?
	
	if os==None:
		if RET_FOUND:	ret = path, True
		else:			ret = path
		return ret
	
	if path=='' or os.path.exists(path):
		if RET_FOUND:	ret = path, True
		else:			ret = path
		return ret
	
	f = os.path.basename(path) # f may be a directory or a file
	d = os.path.dirname(path)
	
	suffix = ''
	if not f: # dir ends with a slash?
		if len(d) < len(path):
			suffix = path[:len(path)-len(d)]

		f = os.path.basename(d)
		d = os.path.dirname(d)
	
	if not os.path.exists(d):
		d, found = caseInsensitivePath(d, True)
		
		if not found:
			if RET_FOUND:	ret = path, False
			else:			ret = path
			return ret
	
	# at this point, the directory exists but not the file
	
	try: # we are expecting 'd' to be a directory, but it could be a file
		files = os.listdir(d)
	except:
		if RET_FOUND:	ret = path, False
		else:			ret = path

	f_low = f.lower()
	
	try:	f_nocase = [fl for fl in files if fl.lower() == f_low][0]
	except:	f_nocase = None
	
	if f_nocase:
		if RET_FOUND:	ret = os.path.join(d, f_nocase) + suffix, True
		else:			ret = os.path.join(d, f_nocase) + suffix
		return ret
	else:
		if RET_FOUND:	ret = path, False
		else:			ret = path
		return ret # cant find the right one, just return the path as is.