'''
Module giving a float representation
of the interpreter major version (1.4, 1.5 etceteras)

ver -- Float representation of the current interpreter version

Note: Since I no longer have any Python 1.4 modules, this module is
no longer in use by me.  I intend to leave it here for the next version
jump :) .
'''
import regex, sys, string
ver = string.atof(sys.version[:regex.match('[0-9.]*', sys.version)])

### Clean up namespace
del(regex)
del(sys)
del(string)
