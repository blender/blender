'''
Make either cPickle or pickle available as the virtual
module mcf.utils.pickle.  This allows you to use a single
import statement:
	
	from mcf.utils import extpkl, pickle

and then use that pickle, knowing that you have the best
available pickling engine.
'''
defaultset = ('import cPickle', 'cPickle')
import sys, mcf.utils
from mcf.utils import cpickle_extend
try:
	import cPickle
	pickle = cPickle
except:
	import pickle
sys.modules['mcf.utils.pickle'] = mcf.utils.pickle = pickle
