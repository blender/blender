# The VRML loader
# supports gzipped files
# 
# TODO: better progress monitoring

import parser 

def quiet(txt):
	pass

debug = quiet

def debug1(txt):
	print "Loader:", txt

g_last = 0

def getFileType(file):
	"returns the file type string from 'file'"
	file.seek(0)
	magic = file.readline()
	if magic[:3] == '\037\213\010':
		file.seek(0)
		return "gzip"
	elif magic[:10] == '#VRML V2.0':
		file.seek(0)
		return "vrml"
	else:
		file.seek(0)
		return ""

class Loader:
	def __init__(self, url, progress = None):
		self.url = url
		self.debug = debug
		self.fail = debug
		self.monitor = debug
		self.progress = progress
		self.nodes = 0 # number of nodes parsed

	def getGzipFile(self, file):
		'''Return gzip file (only called when gzip type is recognised)'''
		# we now have the local filename and the headers
		# read the first few bytes, check for gzip magic number
		self.monitor( "gzip-encoded file... loading gzip library")
		try:
			import gzip
			file = gzip.open(file,"rb")
			return file
		except ImportError, value:
			self.fail("Gzip library unavailable, compressed file cannot be read")
		except:
			self.fail("Failed to open Gzip file")

		return None

	def load(self):
		self.debug("try: load file from %s" % self.url)
		url = self.url

		# XXX
		try:
			file = open(url, 'rb')
		except IOError, val:
			self.debug("couldn't open file %s" % url)
			return None

		if getFileType(file) == 'gzip':
			file.close()
			file = self.getGzipFile(url)
		try:
			data = file.read()
		except MemoryError, value:
			self.fail("Insufficient memory to load file as string", value)
			return None
		except IOError, value:
				self.fail("I/O Error while reading data from file %s "% url)
		p = parser.Parser(data)
		if self.progress:
			scenegraph = p.parse(self.progress)
			print "progress"
		else:
			scenegraph = p.parse()
			
		self.nodes = p.progresscount # progress
		del p
		return scenegraph


def load(url, progress = None):
	l = Loader(url, progress)
	return l.load()
	
def test(name = None):	
	if not name:
		name = '/tmp/gna.wrl'
	return load(name)	
