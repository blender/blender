# epy_docgen.sh
# generates blender python doc using epydoc
# requires epydoc in your PATH.
# run from the doc directory containing the .py files
# usage:  sh epy_docgen.sh

# set posix locale so regex works properly for [A-Z]*.py
LC_ALL=POSIX

epydoc -v  -o BPY_GE --url "http://www.blender.org" --top GameLogic \
 --name "Blender GameEngine" --no-private --no-frames *.py 
