#ifndef __RAS_I2DFILTER
#define __RAS_I2DFILTER



#define MAX_RENDER_PASS	100

class RAS_2DFilterManager
{
private:
	unsigned int	CreateShaderProgram(char* shadersource);
	unsigned int	CreateShaderProgram(int filtermode);
	void			StartShaderProgram(unsigned int shaderprogram);
	void			EndShaderProgram();

	float			textureoffsets[18];
	float			view[4];
	unsigned int	texname;
	int				texturewidth;
	int				textureheight;
	int				canvaswidth;
	int				canvasheight;
	int				numberoffilters;

	bool			isshadersupported;
public:
	enum RAS_2DFILTER_MODE {
		RAS_2DFILTER_ENABLED = -2,
		RAS_2DFILTER_DISABLED = -1,
		RAS_2DFILTER_NOFILTER = 0,
		RAS_2DFILTER_MOTIONBLUR,
		RAS_2DFILTER_BLUR,
		RAS_2DFILTER_SHARPEN,
		RAS_2DFILTER_DILATION,
		RAS_2DFILTER_EROSION,
		RAS_2DFILTER_LAPLACIAN,
		RAS_2DFILTER_SOBEL,
		RAS_2DFILTER_PREWITT,
		RAS_2DFILTER_GRAYSCALE,
		RAS_2DFILTER_SEPIA,
		RAS_2DFILTER_INVERT,
		RAS_2DFILTER_CUSTOMFILTER,
		RAS_2DFILTER_NUMBER_OF_FILTERS
	};

	unsigned int	m_filters[MAX_RENDER_PASS];
	short			m_enabled[MAX_RENDER_PASS];

	RAS_2DFilterManager();

	~RAS_2DFilterManager();

	void SetupTexture();

	void UpdateOffsetMatrix(int width, int height);

	void RenderFilters(RAS_ICanvas* canvas);

	void EnableFilter(RAS_2DFILTER_MODE mode, int pass, STR_String& text);
};
#endif
