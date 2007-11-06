#ifndef __RAS_LAPLACION2DFILTER
#define __RAS_LAPLACION2DFILTER

char * LaplacionFragmentShader=STRINGIFY(
uniform sampler2D bgl_RenderedTexture;
uniform vec2 bgl_TextureCoordinateOffset[9];

void main(void)
{
    vec4 sample[9];

    for (int i = 0; i < 9; i++)
    {
        sample[i] = texture2D(bgl_RenderedTexture, 
                              gl_TexCoord[0].st + bgl_TextureCoordinateOffset[i]);
    }

    gl_FragColor = (sample[4] * 8.0) - 
                    (sample[0] + sample[1] + sample[2] + 
                     sample[3] + sample[5] + 
                     sample[6] + sample[7] + sample[8]);
	gl_FragColor = vec4(gl_FragColor.rgb, 1.0);
}
);
#endif

