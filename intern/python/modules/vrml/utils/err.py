'''
err.py  Encapsulated writing to sys.stderr

The idea of this module is that, for a GUI system (or a more advanced UI),
you can just import a different err module (or object) and keep 
your code the same.  (For instance, you often want a status window
which flashes warnings and info, and have error messages pop up an 
alert to get immediate attention.
'''

import sys

def err(message, Code=0):
	'''
	report an error, with an optional error code
	'''
	if Code:
		sys.stderr.write('Error #%i: %s\n'%(Code,message))
	else:
		sys.stderr.write('Error: %s\n'%message)
def warn(message, Code=0):
	'''
	report a warning, with an optional error code
	'''
	if Code:
		sys.stderr.write('Warning #%i: %s\n'%(Code,message))
	else:
		sys.stderr.write('Warning: %s\n'%message)
def info(message, Code=0):
	'''
	report information/status, with an optional error code
	'''
	if Code:
		sys.stderr.write('Info #%i: %s\n'%(Code,message))
	else:
		sys.stderr.write('Info: %s\n'%message)

