uniform vec2 ScaleU;
uniform sampler2D textureSource;

void main()
{
	vec4 color = vec4(0.0);
	color += texture2D( textureSource, gl_TexCoord[0].st + vec2( -3.0*ScaleU.x, -3.0*ScaleU.y ) ) * 0.015625;
	color += texture2D( textureSource, gl_TexCoord[0].st + vec2( -2.0*ScaleU.x, -2.0*ScaleU.y ) )*0.09375;
	color += texture2D( textureSource, gl_TexCoord[0].st + vec2( -1.0*ScaleU.x, -1.0*ScaleU.y ) )*0.234375;
	color += texture2D( textureSource, gl_TexCoord[0].st + vec2( 0.0 , 0.0) )*0.3125;
	color += texture2D( textureSource, gl_TexCoord[0].st + vec2( 1.0*ScaleU.x,  1.0*ScaleU.y ) )*0.234375;
	color += texture2D( textureSource, gl_TexCoord[0].st + vec2( 2.0*ScaleU.x,  2.0*ScaleU.y ) )*0.09375;
	color += texture2D( textureSource, gl_TexCoord[0].st + vec2( 3.0*ScaleU.x, -3.0*ScaleU.y ) ) * 0.015625;

	gl_FragColor = color;
}
