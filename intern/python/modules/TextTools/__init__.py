""" mxTextTools - A tools package for fast text processing.

    (c) Copyright Marc-Andre Lemburg; All Rights Reserved.
    See the documentation for further information on copyrights,
    or contact the author (mal@lemburg.com).
"""
__package_info__ = """
BEGIN PYTHON-PACKAGE-INFO 1.0
Title:			mxTextTools - Tools for fast text processing
Current-Version:	1.1.1
Home-Page:		http://starship.skyport.net/~lemburg/mxTextTools.html
Primary-Site:		http://starship.skyport.net/~lemburg/mxTextTools-1.1.1.zip

This package provides several different functions and mechanisms
to do fast text text processing. Amongst these are character set
operations, parsing & tagging tools (using a finite state machine
executing byte code) and common things such as Boyer-Moore search
objects. For full documentation see the home page.
END PYTHON-PACKAGE-INFO
"""
from TextTools import *
from TextTools import __version__

### Make the types pickleable:

# Shortcuts for pickle (reduces the pickle's length)
def _BMS(match,translate):
    return BMS(match,translate)
def _FS(match,translate):
    return FS(match,translate)

# Module init
class modinit:

    ### Register the two types
    import copy_reg
    def pickle_BMS(so):
	return _BMS,(so.match,so.translate)
    def pickle_FS(so):
	return _FS,(so.match,so.translate)
    copy_reg.pickle(BMSType,
		    pickle_BMS,
		    _BMS)
    copy_reg.pickle(FSType,
		    pickle_FS,
		    _FS)

del modinit
