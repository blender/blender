

SOURCES=$(shell ls *.py)
TARGETS:=$(SOURCES:.py=.html)
PYDOC=/usr/lib/python2.2/pydoc.py

all: $(SOURCES)
	epydoc -o BPY_GE_234 --url "http://www.blender.org" -t GameLogic.py \
		-n "Blender GameEngine" --no-private --no-frames \
		$(shell ls *.py )
		
clean:
	rm *.html
