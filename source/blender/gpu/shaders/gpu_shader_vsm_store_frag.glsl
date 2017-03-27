/**
 * This fragment shader was initially found at http://fabiensanglard.net/shadowmappingVSM/index.php
 */

#if __VERSION__ == 120
  varying vec4 v_position;
  #define fragColor gl_FragColor
#else
  in vec4 v_position;
  out vec4 fragColor;
#endif

void main()
{
	float depth = v_position.z / v_position.w;
	depth = depth * 0.5 + 0.5;

	float moment1 = depth;
	float moment2 = depth * depth;

	// Adjusting moments using partial derivative
	float dx = dFdx(depth);
	float dy = dFdy(depth);
	moment2 += 0.25 * (dx * dx + dy * dy);

	fragColor = vec4(moment1, moment2, 0.0, 0.0);
	// TODO: write to a 2-component target --^
}
