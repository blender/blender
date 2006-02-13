#ifndef __BL_SHADER_H__
#define __BL_SHADER_H__

#include "PyObjectPlus.h"
#include "BL_Material.h"
#include "BL_Texture.h"
// --
#include "MT_Matrix4x4.h"
#include "MT_Matrix3x3.h"
#include "MT_Tuple2.h"
#include "MT_Tuple3.h"
#include "MT_Tuple4.h"

// -----------------------------------
// user state management
typedef struct uSampler
{
	unsigned int	type;
	int				pass;
	int				unit;
	int				loc;
	BL_Texture*		gl_texture;
	int				flag;
}uSampler;

#define SAMP_2D		1
#define SAMP_CUBE	2
#define ATTRIBMAX	1

// uSampler::flag;
enum 
{
	OWN=1
};

// ----------------
class BL_Shader : public PyObjectPlus
{
	Py_Header;
private:
	unsigned int	mShader;
	int				mPass;
	bool			mOk;
	bool			mUse;
	uSampler		mSampler[MAXTEX];
	char*			vertProg;
	char*			fragProg;
	bool			mError;
	
	int				mAttr;
	int				mPreDefLoc;
	int				mPreDefType;
	bool			mDeleteTexture;

	bool			LinkProgram();
public:
	BL_Shader(PyTypeObject *T=&Type);
	virtual ~BL_Shader();

	enum AttribTypes{
		SHD_TANGENT =1
	};

	enum GenType {
		MODELVIEWMATRIX,
		MODELVIEWMATRIX_TRANSPOSE,
		MODELVIEWMATRIX_INVERSE,
		MODELVIEWMATRIX_INVERSETRANSPOSE,
	
		// Model matrix
		MODELMATRIX,
		MODELMATRIX_TRANSPOSE,
		MODELMATRIX_INVERSE,
		MODELMATRIX_INVERSETRANSPOSE,
	
		// View Matrix
		VIEWMATRIX,
		VIEWMATRIX_TRANSPOSE,
		VIEWMATRIX_INVERSE,
		VIEWMATRIX_INVERSETRANSPOSE,
		CAM_POS
	};

	char*		GetVertPtr();
	char*		GetFragPtr();
	void		SetVertPtr( char *vert );
	void		SetFragPtr( char *frag );
	
	// ---
	int getNumPass()	{return mPass;}
	bool use()			{return mUse;}
	bool GetError()		{return mError;}
	// ---
	// access
	const uSampler*		getSampler(int i);
	void				SetSampler(int loc, int unit);

	const bool			Ok()const;
	unsigned int		GetProg();
	void				SetProg(bool enable);
	int					GetAttribute(){return mAttr;};

	void InitializeSampler( int type, int unit, int pass, BL_Texture* texture );

	void Update( const class KX_MeshSlot & ms, class RAS_IRasterizer* rasty );

	// form tuhopuu2
	virtual int GetAttribLocation(const STR_String& name);
	virtual void BindAttribute(const STR_String& attr, int loc);
	virtual int GetUniformLocation(const STR_String& name);
	virtual void SetUniform(int uniform, const MT_Tuple2& vec);
	virtual void SetUniform(int uniform, const MT_Tuple3& vec);
	virtual void SetUniform(int uniform, const MT_Tuple4& vec);
	virtual void SetUniform(int uniform, const unsigned int& val);
	virtual void SetUniform(int uniform, const float& val);
	virtual void SetUniform(int uniform, const MT_Matrix4x4& vec, bool transpose=false);
	virtual void SetUniform(int uniform, const MT_Matrix3x3& vec, bool transpose=false);

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

	KX_PYMETHOD_DOC( BL_Shader, setUniformDef );

	KX_PYMETHOD_DOC( BL_Shader, setAttrib );

	// these come from within the material buttons
	// sampler2d/samplerCube work
	KX_PYMETHOD_DOC( BL_Shader, setSampler);
};



#endif//__BL_SHADER_H__
