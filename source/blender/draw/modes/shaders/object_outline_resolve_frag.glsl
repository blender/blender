
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D outlineBluredColor;
uniform vec2 rcpDimensions;

void main()
{
#ifdef USE_FXAA
    float aa_alpha = FxaaPixelShader(
        uvcoordsvar.st,
        outlineBluredColor,
        rcpDimensions,
        1.0,
        0.166,
        0.0833
    ).r;
#endif

    FragColor = texture(outlineBluredColor, uvcoordsvar.st).rgba;

#ifdef USE_FXAA
    FragColor.a = aa_alpha;
#endif
}
