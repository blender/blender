# $Id$
# Documentation for Light game objects.
from KX_GameObject import *

class KX_Light(KX_GameObject):
	"""
	A Light object.

	Example:
	
	# Turn on a red alert light.
	import GameLogic
	
	co = GameLogic.getCurrentController()
	light = co.getOwner()
	
	light.energy = 1.0
	light.colour = [1.0, 0.0, 0.0]
	
	@group Constants: NORMAL, SPOT, SUN
	@ivar SPOT:   A spot light source. See attribute 'type'
	@ivar SUN:    A point light source with no attenuation. See attribute 'type'
	@ivar NORMAL: A point light source. See attribute 'type'
	
	@ivar type:            The type of light - must be SPOT, SUN or NORMAL
	@ivar layer:           The layer mask that this light affects object on.
	@type layer:           bitfield
	@ivar energy:          The brightness of this light. 
	@type energy:          float
	@ivar distance:        The maximum distance this light can illuminate. (SPOT and NORMAL lights only)
	@type distance:        float
	@ivar colour:          The colour of this light. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0]
	@type colour:          list [r, g, b]
	@ivar color:           Synonym for colour.
	@ivar lin_attenuation: The linear component of this light's attenuation. (SPOT and NORMAL lights only)
	@type lin_attenuation: float
	@ivar quad_attenuation: The quadratic component of this light's attenuation (SPOT and NORMAL lights only)
	@type quad_attenuation: float
	@ivar spotsize:        The cone angle of the spot light, in degrees. (float) (SPOT lights only)
	                       0.0 <= spotsize <= 180.0. Spotsize = 360.0 is also accepted. 
	@ivar spotblend:       Specifies the intensity distribution of the spot light. (float) (SPOT lights only)
	                       Higher values result in a more focused light source.
	                       0.0 <= spotblend <= 1.0.
	
	"""
