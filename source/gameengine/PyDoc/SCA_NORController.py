# $Id: SCA_ANDController.py 15444 2008-07-05 17:05:05Z lukep $
# Documentation for SCA_NORController
from SCA_IController import *

class SCA_NORController(SCA_IController):
	"""
	An NOR controller activates only when all linked sensors are de-activated.
	
	There are no special python methods for this controller.
	"""

