

SOURCES=$(shell ls *.py)
TARGETS:=$(SOURCES:.py=.html)
PYDOC=/usr/lib/python2.2/pydoc.py

all: $(TARGETS)

%.html: %.py
	$(PYDOC) -w $*

