# $Id$
# Documentation for SCA_2DFilterActuator
from SCA_IActuator import *
from SCA_ILogicBrick import *

class SCA_2DFilterActuator(SCA_IActuator):
	"""
	Create, enable and disable 2D filters
	
	Properties:
	
	The following properties don't have an immediate effect. 
	You must active the actuator to get the result.
	The actuator is not persistent: it automatically stops itself after setting up the filter
	but the filter remains active. To stop a filter you must activate the actuator with 'type'
	set to RAS_2DFILTER_DISABLED or RAS_2DFILTER_NOFILTER.
	
	@ivar shaderText: shader source code for custom shader
	@type shaderText: string
	@ivar disableMotionBlur: action on motion blur: 0=enable, 1=disable
	@type disableMotionBlur: integer
	@ivar type: type of 2D filter, use one of the following constants:
	            RAS_2DFILTER_ENABLED      (-2) : enable the filter that was previously disabled
				RAS_2DFILTER_DISABLED     (-1) : disable the filter that is currently active
				RAS_2DFILTER_NOFILTER      (0) : disable and destroy the filter that is currently active
				RAS_2DFILTER_MOTIONBLUR    (1) : create and enable preset filters
				RAS_2DFILTER_BLUR          (2)
				RAS_2DFILTER_SHARPEN       (3)
				RAS_2DFILTER_DILATION      (4)
				RAS_2DFILTER_EROSION       (5)
				RAS_2DFILTER_LAPLACIAN     (6)
				RAS_2DFILTER_SOBEL         (7)
				RAS_2DFILTER_PREWITT       (8)
				RAS_2DFILTER_GRAYSCALE     (9)
				RAS_2DFILTER_SEPIA        (10)
				RAS_2DFILTER_INVERT       (11)
				RAS_2DFILTER_CUSTOMFILTER (12) : customer filter, the code code is set via shaderText property
	@type type: integer				
	@ivar passNb: order number of filter in the stack of 2D filters. Filters are executed in increasing order of passNb.
	              Only be one filter can be defined per passNb.
	@type passNb: integer (0-100)
	@ivar value: argument for motion blur filter
	@type value: float (0.0-100.0)
	"""
