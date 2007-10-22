#ifndef __RAS_INVERT2DFILTER
#define __RAS_INVERT2DFILTER

char * InvertFragmentShader=STRINGIFY(
uniform sampler2D sampler0;
uniform vec2 offset[9];

void main(void)
{
	vec4 texcolor = texture2D(sampler0, gl_TexCoord[0].st); 
	gl_FragColor.rgb = 1.0 - texcolor.rgb;
    gl_FragColor.a = texcolor.a;
}
);
#endif
