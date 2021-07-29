
class DxfDrawing(object):
	"""
	Represents intermediate model of DXF drawing. It is useful in iterating
	through exported object and for easy change the DXF handling library.
	"""
	def __init__(self):
		self._entities = {}
		self._layers = {}
		self._views = []
		self._vports = []
		self._blocks = []

	def isEmpty(self):
		return len(self._entities) == 0

	def addEntity(self, type, **kwargs):
		if type not in self._entities:
			self._entities[type] = []
		self._entities[type].append(kwargs)

	def addLayer(self, name, color):
		self._layers[name] = color

	def containsLayer(self, name):
		return name in self._layers

	def addBlock(self, block):
		self._blocks.append(block)

	def containsBlock(self, blockname):
		return blockname in self._blocks

	def convert(self, **kwargs):
		""" Converts this drawing into DXF representation object """
		raise NotImplementedError()


