""" mxTextTools - A tools package for fast text processing.

    (c) Copyright Marc-Andre Lemburg; All Rights Reserved.
    See the documentation for further information on copyrights,
    or contact the author (mal@lemburg.com).
"""
#from mxTextTools import *
#from mxTextTools import __version__

#
# Make BMS take the role of FS in case the Fast Search object was not built
#
try:
    FS
except NameError:
    FS = BMS
    FSType = BMSType
