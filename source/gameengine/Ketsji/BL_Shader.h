#ifndef __BL_SHADER_H__
#define __BL_SHADER_H__

#include "PyObjectPlus.h"
#include "BL_Material.h"

// -----------------------------------
// user state management
typedef struct uSampler
{
	unsigned int	type;
	int				pass;
	int				unit;
	int				loc;
	unsigned int	glTexture;
}uSampler;

#define SAMP_2D		1
#define SAMP_CUBE	2


// -----------------------------------
typedef struct uBlending
{
	unsigned int pass;
	int src;	// GL_ blend func values
	int dest;
	float const_color[4];
}uBlending;
// -----------------------------------

// ----------------
class BL_Shader : public PyObjectPlus
{
	Py_Header;
private:
	unsigned int	mShader, 
					mVert,
					mFrag;
	int				mPass;
	bool			mOk;
	bool			mUse;
	uSampler		mSampler[MAXTEX];
	uBlending		mBlending;
	char*			vertProg;
	char*			fragProg;
	bool			LinkProgram();
	bool			PrintInfo(int len, unsigned int handle, const char *type);

public:
	BL_Shader(int n, PyTypeObject *T=&Type);
	virtual ~BL_Shader();

	char*		GetVertPtr();
	char*		GetFragPtr();
	void		SetVertPtr( char *vert );
	void		SetFragPtr( char *frag );
	
	// ---
	int getNumPass()	{return mPass;}
	bool use()			{return mUse;}

	// ---
	// access
	const uSampler*		getSampler(int i);
	const uBlending*	getBlending( int pass );
	const bool			Ok()const;

	unsigned int		GetProg();
	unsigned int		GetVertexShader();
	unsigned int		GetFragmentShader();
	
	void InitializeSampler(
		int type,
		int unit,
		int pass,
		unsigned int texture
	);

	// -----------------------------------
	// python interface
	virtual PyObject* _getattr(const STR_String& attr);

	KX_PYMETHOD_DOC( BL_Shader, setSource );
	KX_PYMETHOD_DOC( BL_Shader, delSource );
	KX_PYMETHOD_DOC( BL_Shader, getVertexProg );
	KX_PYMETHOD_DOC( BL_Shader, getFragmentProg );
	KX_PYMETHOD_DOC( BL_Shader, setNumberOfPasses );

	// -----------------------------------
	KX_PYMETHOD_DOC( BL_Shader, isValid);
	KX_PYMETHOD_DOC( BL_Shader, validate);
	KX_PYMETHOD_DOC( BL_Shader, setUniform4f );
	KX_PYMETHOD_DOC( BL_Shader, setUniform3f );
	KX_PYMETHOD_DOC( BL_Shader, setUniform2f );
	KX_PYMETHOD_DOC( BL_Shader, setUniform1f );
	KX_PYMETHOD_DOC( BL_Shader, setUniform4i );
	KX_PYMETHOD_DOC( BL_Shader, setUniform3i );
	KX_PYMETHOD_DOC( BL_Shader, setUniform2i );
	KX_PYMETHOD_DOC( BL_Shader, setUniform1i );

	KX_PYMETHOD_DOC( BL_Shader, setUniformfv );
	KX_PYMETHOD_DOC( BL_Shader, setUniformiv );

	KX_PYMETHOD_DOC( BL_Shader, setUniformMatrix4 );
	KX_PYMETHOD_DOC( BL_Shader, setUniformMatrix3 );

	// these come from within the material buttons
	// sampler2d/samplerCube work
	KX_PYMETHOD_DOC( BL_Shader, setSampler);
	// user blending funcs
	KX_PYMETHOD_DOC( BL_Shader, setBlending );
};



#endif//__BL_SHADER_H__
