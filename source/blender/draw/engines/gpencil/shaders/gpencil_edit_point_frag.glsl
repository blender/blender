in vec4 mColor;
in vec2 mTexCoord;
out vec4 fragColor;

void main()
{
	vec2 centered = mTexCoord - vec2(0.5);
	float dist_squared = dot(centered, centered);
	const float rad_squared = 0.25;

	// round point with jaggy edges
	if (dist_squared > rad_squared) {
		discard;
	}

	fragColor = mColor;
}
