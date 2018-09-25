
uniform mat4 ModelViewProjectionMatrix;

in float weight;
in vec3 pos;

out vec2 weight_interp; /* (weight, alert) */

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	/* Separate actual weight and alerts for independent interpolation */
	weight_interp = max(vec2(weight, -weight), 0.0);
}
