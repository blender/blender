class importer:
	def __init__(self,writer=None):
		self.writer = writer
		self.filename = None
		self.file = None
		self.ext = ""
	def readfile(self, name):
		file = open(name, "r")
		if not file:	
			return 0
		self.file = file	
		self.filename = name
		self.lines = file.readlines()
	def close(self):
		if self.filename:
			self.file.close()
	def checkmagic(self, name):
		# return 1 if magic true (format verified), 0 else
		return 0
	def parse(self, data):
		# parse and convert the data shere
		pass

class writer:
	def __init__(self, args = None):
		pass
	def mesh(self, me, name):
		pass

_inst = importer()
readfile = _inst.readfile
close = _inst.close
checkmagic = _inst.checkmagic
parse = _inst.parse
