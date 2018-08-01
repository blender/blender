in vec4 mColor;
in vec2 mTexCoord;

out vec4 fragColor;

void main()
{
	const vec2 center = vec2(0, 0.5);
	vec4 tColor = vec4(mColor);
	/* if alpha < 0, then encap */
	if (mColor.a < 0) {
		tColor.a = tColor.a * -1.0;
		float dist = length(mTexCoord - center);
		if (dist > 0.25) {
			discard;
		}
	}
	/* Solid */
	fragColor = tColor;
}
