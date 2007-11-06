#ifndef __RAS_INVERT2DFILTER
#define __RAS_INVERT2DFILTER

char * InvertFragmentShader=STRINGIFY(
uniform sampler2D bgl_RenderedTexture;

void main(void)
{
	vec4 texcolor = texture2D(bgl_RenderedTexture, gl_TexCoord[0].st); 
	gl_FragColor.rgb = 1.0 - texcolor.rgb;
    gl_FragColor.a = texcolor.a;
}
);
#endif
