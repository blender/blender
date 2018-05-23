void main()
{
	vec2 p= vec2(floor(gl_FragCoord.x), floor(gl_FragCoord.y));
	vec2 test = mod(p, 2.0);
	if (mod(test.x + test.y, 2.0)==0.0) {
		discard;
	} else {
		gl_FragDepth = 1.0;
	}
}