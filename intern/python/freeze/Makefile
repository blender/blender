# $Id$
# This is the makefile for the bytecode freezing of all modules which
# the main file depends on (last argument in importer rule)

SRCDIR = ../modules

TARGETDIR = ../../../source/blender/bpython/frozen

PYFLAGS = -S -O

default: importer

install: importer

importer: 
	python $(PYFLAGS) freeze.py -d -x os -x pprint -x Blender -I $(SRCDIR) -o $(TARGETDIR) $(SRCDIR)/VRMLmain.py 

clean:
	rm *.pyo
