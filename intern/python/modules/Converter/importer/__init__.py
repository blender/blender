"""This module contains a list of valid importers in 'importers'. At runtime,
importer modules can be registered by the 'register' function."""

__all__ = ["VRMLimporter"]

importers = __all__

import VRMLimporter

def register(importer):
	"""Register an file importer"""
	methods = ["checkmagic", "importfile"]
	for m in methods:
		if not hasattr(importer, m):
			raise TypeError, "This is not an importer"
	importers.append(importer)

