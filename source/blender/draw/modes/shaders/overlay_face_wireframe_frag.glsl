uniform vec3 wireColor;
uniform vec3 rimColor;

flat in vec3 ssVec0;
flat in vec3 ssVec1;
flat in vec3 ssVec2;
in float facing;

out vec4 fragColor;

float min_v3(vec3 v) { return min(v.x, min(v.y, v.z)); }

/* In pixels */
const float wire_size = 0.0; /* Expands the core of the wire (part that is 100% wire color) */
const float wire_smooth = 1.4; /* Smoothing distance after the 100% core. */

/* Alpha constants could be exposed in the future. */
const float front_alpha = 0.55;
const float rim_alpha = 0.75;

void main()
{
	vec3 ss_pos = vec3(gl_FragCoord.xy, 1.0);
	vec3 dist_to_edge = vec3(
		dot(ss_pos, ssVec0),
		dot(ss_pos, ssVec1),
		dot(ss_pos, ssVec2)
	);

	float fac = smoothstep(wire_size, wire_size + wire_smooth, min_v3(abs(dist_to_edge)));
	float facing_clamped = clamp((gl_FrontFacing) ? facing : -facing, 0.0, 1.0);

	vec3 final_front_col = rimColor * 0.5 + wireColor * 0.5;
	fragColor = mix(vec4(rimColor, rim_alpha), vec4(final_front_col, front_alpha), facing_clamped);
	fragColor.a *= (1.0 - fac);
}
