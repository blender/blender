# $Id: SCA_ANDController.py 15444 2008-07-05 17:05:05Z lukep $
# Documentation for SCA_XNORController
from SCA_IController import *

class SCA_XNORController(SCA_IController):
	"""
	An XNOR controller activates when all linked sensors are the same (activated or inative).
	
	There are no special python methods for this controller.
	"""

