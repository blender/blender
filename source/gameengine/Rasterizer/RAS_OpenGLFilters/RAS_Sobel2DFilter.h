#ifndef __RAS_SOBEL2DFILTER
#define __RAS_SOBEL2DFILTER

char * SobelFragmentShader=STRINGIFY(
uniform sampler2D sampler0;
uniform vec2 tc_offset[9];

void main(void)
{
    vec4 sample[9];

    for (int i = 0; i < 9; i++)
    {
        sample[i] = texture2D(sampler0, 
                              gl_TexCoord[0].st + tc_offset[i]);
    }

    vec4 horizEdge = sample[2] + (2.0*sample[5]) + sample[8] -
                     (sample[0] + (2.0*sample[3]) + sample[6]);

    vec4 vertEdge = sample[0] + (2.0*sample[1]) + sample[2] -
                    (sample[6] + (2.0*sample[7]) + sample[8]);

    gl_FragColor.rgb = sqrt((horizEdge.rgb * horizEdge.rgb) + 
                            (vertEdge.rgb * vertEdge.rgb));
    gl_FragColor.a = 1.0;
}
);
#endif

