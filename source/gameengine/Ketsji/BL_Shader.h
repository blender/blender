
/** \file BL_Shader.h
 *  \ingroup ketsji
 */

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

#define SHADER_ATTRIBMAX 1

/**
 * BL_Sampler
 *  Sampler access 
 */
class BL_Sampler
{
public:
	BL_Sampler():
		mLoc(-1)
	{
	}
	int				mLoc;		// Sampler location
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_Sampler")
#endif
};

/**
 * BL_Uniform
 *  uniform storage 
 */
class BL_Uniform 
{
private:
	int			mLoc;		// Uniform location
	void*		mData;		// Memory allocated for variable
	bool		mDirty;		// Caching variable  
	int			mType;		// Enum UniformTypes
	bool		mTranspose; // Transpose matrices
	const int	mDataLen;	// Length of our data
public:
	BL_Uniform(int data_size);
	~BL_Uniform();
	

	void Apply(class BL_Shader *shader);
	void SetData(int location, int type, bool transpose=false);

	enum UniformTypes {
		UNI_NONE	=0,
		UNI_INT,
		UNI_FLOAT,
		UNI_INT2,
		UNI_FLOAT2,
		UNI_INT3,
		UNI_FLOAT3,
		UNI_INT4,
		UNI_FLOAT4,
		UNI_MAT3,
		UNI_MAT4,
		UNI_MAX
	};

	int GetLocation()	{ return mLoc; }
	void* getData()		{ return mData; }
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_Uniform")
#endif
};

/**
 * BL_DefUniform
 * pre defined uniform storage 
 */
class BL_DefUniform
{
public:
	BL_DefUniform() :
		mType(0),
		mLoc(0),
		mFlag(0)
	{
	}
	int				mType;
	int				mLoc;
	unsigned int	mFlag;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_DefUniform")
#endif
};

/**
 * BL_Shader
 *  shader access
 */
class BL_Shader : public PyObjectPlus
{
	Py_Header
private:
	typedef std::vector<BL_Uniform*>	BL_UniformVec;
	typedef std::vector<BL_DefUniform*>	BL_UniformVecDef;

	unsigned int	mShader;			// Shader object 
	int				mPass;				// 1.. unused
	bool			mOk;				// Valid and ok
	bool			mUse;				// ...
//BL_Sampler		mSampler[MAXTEX];	// Number of samplers
	int				mAttr;				// Tangent attribute
	const char*		vertProg;			// Vertex program string
	const char*		fragProg;			// Fragment program string
	bool			mError;				// ...
	bool			mDirty;				// 

	// Compiles and links the shader
	bool LinkProgram();

	// Stored uniform variables
	BL_UniformVec		mUniforms;
	BL_UniformVecDef	mPreDef;

	// search by location
	BL_Uniform*		FindUniform(const int location);
	// clears uniform data
	void			ClearUniforms();

public:
	BL_Shader();
	virtual ~BL_Shader();

	// Unused for now tangent is set as 
	// tex coords
	enum AttribTypes {
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

		// Current camera position 
		CAM_POS,

		// RAS timer
		CONSTANT_TIMER
	};

	const char* GetVertPtr();
	const char* GetFragPtr();
	void SetVertPtr( char *vert );
	void SetFragPtr( char *frag );
	
	// ---
	int getNumPass()	{return mPass;}
	bool GetError()		{return mError;}
	// ---
	//const BL_Sampler*	GetSampler(int i);
	void				SetSampler(int loc, int unit);

	bool				Ok()const;
	unsigned int		GetProg();
	void				SetProg(bool enable);
	int					GetAttribute() { return mAttr; }

	// -- 
	// Apply methods : sets colected uniforms
	void ApplyShader();
	void UnloadShader();

	// Update predefined uniforms each render call
	void Update(const class RAS_MeshSlot & ms, class RAS_IRasterizer* rasty);

	//// Set sampler units (copied)
	//void InitializeSampler(int unit, BL_Texture* texture );


	void SetUniformfv(int location,int type, float *param, int size,bool transpose=false);
	void SetUniformiv(int location,int type, int *param, int size,bool transpose=false);

	int GetAttribLocation(const STR_String& name);
	void BindAttribute(const STR_String& attr, int loc);
	int GetUniformLocation(const STR_String& name);

	void SetUniform(int uniform, const MT_Tuple2& vec);
	void SetUniform(int uniform, const MT_Tuple3& vec);
	void SetUniform(int uniform, const MT_Tuple4& vec);
	void SetUniform(int uniform, const MT_Matrix4x4& vec, bool transpose=false);
	void SetUniform(int uniform, const MT_Matrix3x3& vec, bool transpose=false);
	void SetUniform(int uniform, const float& val);
	void SetUniform(int uniform, const float* val, int len);
	void SetUniform(int uniform, const int* val, int len);
	void SetUniform(int uniform, const unsigned int& val);
	void SetUniform(int uniform, const int val);

	// Python interface
#ifdef WITH_PYTHON
	virtual PyObject* py_repr(void) { return PyUnicode_FromFormat("BL_Shader\n\tvertex shader:%s\n\n\tfragment shader%s\n\n", vertProg, fragProg); }

	// -----------------------------------
	KX_PYMETHOD_DOC(BL_Shader, setSource);
	KX_PYMETHOD_DOC(BL_Shader, delSource);
	KX_PYMETHOD_DOC(BL_Shader, getVertexProg);
	KX_PYMETHOD_DOC(BL_Shader, getFragmentProg);
	KX_PYMETHOD_DOC(BL_Shader, setNumberOfPasses);
	KX_PYMETHOD_DOC(BL_Shader, isValid);
	KX_PYMETHOD_DOC(BL_Shader, validate);

	// -----------------------------------
	KX_PYMETHOD_DOC(BL_Shader, setUniform4f);
	KX_PYMETHOD_DOC(BL_Shader, setUniform3f);
	KX_PYMETHOD_DOC(BL_Shader, setUniform2f);
	KX_PYMETHOD_DOC(BL_Shader, setUniform1f);
	KX_PYMETHOD_DOC(BL_Shader, setUniform4i);
	KX_PYMETHOD_DOC(BL_Shader, setUniform3i);
	KX_PYMETHOD_DOC(BL_Shader, setUniform2i);
	KX_PYMETHOD_DOC(BL_Shader, setUniform1i);
	KX_PYMETHOD_DOC(BL_Shader, setUniformfv);
	KX_PYMETHOD_DOC(BL_Shader, setUniformiv);
	KX_PYMETHOD_DOC(BL_Shader, setUniformMatrix4);
	KX_PYMETHOD_DOC(BL_Shader, setUniformMatrix3);
	KX_PYMETHOD_DOC(BL_Shader, setUniformDef);
	KX_PYMETHOD_DOC(BL_Shader, setAttrib);
	KX_PYMETHOD_DOC(BL_Shader, setSampler);
#endif
};

#endif//__BL_SHADER_H__
