# $Id$
# Documentation for Light game objects.
from KX_GameObject import *

class KX_Light(KX_GameObject):
	"""
	A Light object.
	
	Constants:
	@cvar SPOT:   A spot light source. See attribute 'type'
	@cvar SUN:    A point light source with no attenuation. See attribute 'type'
	@cvar NORMAL: A point light source. See attribute 'type'
	
	Attributes:
	@cvar type:            The type of light - must be SPOT, SUN or NORMAL
	@cvar layer:           The layer mask that this light affects object on. (bitfield)
	@cvar energy:          The brightness of this light. (float)
	@cvar distance:        The maximum distance this light can illuminate. (float) (SPOT and NORMAL lights only)
	@cvar colour:          The colour of this light. ([r, g, b]) Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0]
	@cvar color:           Synonym for colour.
	@cvar lin_attenuation: The linear component of this lights attenuation. (SPOT and NORMAL lights only)
	@cvar spotsize:        The cone angle of the spot light, in degrees. (float) (SPOT lights only)
	                       0.0 <= spotsize <= 180.0. Spotsize = 360.0 is also accepted. 
	@cvar spotblend:       Specifies the intensity distribution of the spot light. (float) (SPOT lights only)
	                       Higher values result in a more focused light source.
	                       0.0 <= spotblend <= 1.0.
	
	Example:
	# Turn on a red alert light.
	import GameLogic
	
	co = GameLogic.getCurrentController()
	light = co.getOwner()
	
	light.energy = 1.0
	light.colour = [1.0, 0.0, 0.0]
	"""
