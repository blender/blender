

#ifdef WIN32
#include <windows.h>
#endif // WIN32
#ifdef __APPLE__
#define GL_GLEXT_LEGACY 1
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
/* #if defined(__sun__) && !defined(__sparc__)
#include <mesa/glu.h>
#else
*/
#include <GL/glu.h>
/* #endif */
#endif

#include <iostream>
#include "BL_Shader.h"
#include "BL_Material.h"

#include "MT_assert.h"
#include "MT_Matrix4x4.h"
#include "MT_Matrix3x3.h"
#include "KX_PyMath.h"
#include "MEM_guardedalloc.h"

#include "RAS_GLExtensionManager.h"
#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"

//using namespace bgl;
#define spit(x) std::cout << x << std::endl;

#define SORT_UNIFORMS 1
#define UNIFORM_MAX_LEN sizeof(float)*16
#define MAX_LOG_LEN 262144 // bounds

BL_Uniform::BL_Uniform(int data_size)
:	mLoc(-1),
	mDirty(true),
	mType(UNI_NONE),
	mTranspose(0),
	mDataLen(data_size)
{
#ifdef SORT_UNIFORMS
	MT_assert(mDataLen <= UNIFORM_MAX_LEN);
	mData = (void*)MEM_mallocN(mDataLen, "shader-uniform-alloc");
#endif
}

BL_Uniform::~BL_Uniform()
{
#ifdef SORT_UNIFORMS
	if(mData) {
		MEM_freeN(mData);
		mData=0;
	}
#endif
}

void BL_Uniform::Apply(class BL_Shader *shader)
{
#ifdef GL_ARB_shader_objects
#ifdef SORT_UNIFORMS
	MT_assert(mType > UNI_NONE && mType < UNI_MAX && mData);

	if(!mDirty) 
		return;

	switch(mType)
	{
	case UNI_FLOAT: {
			float *f = (float*)mData;
			bgl::blUniform1fARB(mLoc,(GLfloat)*f);
		}break;
	case UNI_INT: {
			int *f = (int*)mData;
			bgl::blUniform1iARB(mLoc, (GLint)*f);
		}break;
	case UNI_FLOAT2: {
			float *f = (float*)mData;
			bgl::blUniform2fvARB(mLoc,1, (GLfloat*)f);
		}break;
	case UNI_FLOAT3: {
			float *f = (float*)mData;
			bgl::blUniform3fvARB(mLoc,1,(GLfloat*)f);
		}break;
	case UNI_FLOAT4: {
			float *f = (float*)mData;
			bgl::blUniform4fvARB(mLoc,1,(GLfloat*)f);
		}break;
	case UNI_INT2: {
			int *f = (int*)mData;
			bgl::blUniform2ivARB(mLoc,1,(GLint*)f);
		}break; 
	case UNI_INT3: {
			int *f = (int*)mData;
			bgl::blUniform3ivARB(mLoc,1,(GLint*)f);
		}break; 
	case UNI_INT4: {
			int *f = (int*)mData;
			bgl::blUniform4ivARB(mLoc,1,(GLint*)f);
		}break;
	case UNI_MAT4: {
			float *f = (float*)mData;
			bgl::blUniformMatrix4fvARB(mLoc, 1, mTranspose?GL_TRUE:GL_FALSE,(GLfloat*)f);
		}break;
	case UNI_MAT3: {
			float *f = (float*)mData;
			bgl::blUniformMatrix3fvARB(mLoc, 1, mTranspose?GL_TRUE:GL_FALSE,(GLfloat*)f);
		}break;
	}
	mDirty = false;
#endif
#endif
}

void BL_Uniform::SetData(int location, int type,bool transpose)
{
#ifdef SORT_UNIFORMS
	mType	= type;
	mLoc	= location;
	mDirty	= true;
#endif
}

const bool BL_Shader::Ok()const
{
	return (mShader !=0 && mOk && mUse);
}

BL_Shader::BL_Shader(PyTypeObject *T)
:	PyObjectPlus(T),
	mShader(0),
	mPass(1),
	mOk(0),
	mUse(0),
	mAttr(0),
	vertProg(""),
	fragProg(""),
	mError(0),
	mDirty(true)
{
	// if !RAS_EXT_support._ARB_shader_objects this class will not be used
	//for (int i=0; i<MAXTEX; i++) {
	//	mSampler[i] = BL_Sampler();
	//}
}

using namespace bgl;

BL_Shader::~BL_Shader()
{
#ifdef GL_ARB_shader_objects
	//for (int i=0; i<MAXTEX; i++){
	//	if(mSampler[i].mOwn) {
	//		if(mSampler[i].mTexture)
	//			mSampler[i].mTexture->DeleteTex();
	//	}
	//}
	ClearUniforms();

	if( mShader ) {
		bgl::blDeleteObjectARB(mShader);
		mShader = 0;
	}
	vertProg	= 0;
	fragProg	= 0;
	mOk			= 0;
	bgl::blUseProgramObjectARB(0);
#endif//GL_ARB_shader_objects
}

void BL_Shader::ClearUniforms()
{
	BL_UniformVec::iterator it = mUniforms.begin();
	while(it != mUniforms.end()){
		delete (*it);
		it++;
	}
	mUniforms.clear();

	
	BL_UniformVecDef::iterator itp = mPreDef.begin();
	while(itp != mPreDef.end()) {
		delete (*itp);
		itp++;
	}
	mPreDef.clear();

}


BL_Uniform  *BL_Shader::FindUniform(const int location)
{
#ifdef SORT_UNIFORMS
	BL_UniformVec::iterator it = mUniforms.begin();
	while(it != mUniforms.end()) {
		if((*it)->GetLocation() == location)
			return (*it);
		it++;
	}
#endif
	return 0;
}

void BL_Shader::SetUniformfv(int location, int type, float *param,int size, bool transpose)
{
#ifdef SORT_UNIFORMS
	BL_Uniform *uni= FindUniform(location);
	if(uni) {
		memcpy(uni->getData(), param, size);
		uni->SetData(location, type, transpose);
	}
	else {
		uni = new BL_Uniform(size);
		memcpy(uni->getData(), param, size);

		uni->SetData(location, type, transpose);
		mUniforms.push_back(uni);
	}
	mDirty = true;
#endif
}

void BL_Shader::SetUniformiv(int location, int type, int *param,int size, bool transpose)
{
#ifdef SORT_UNIFORMS
	BL_Uniform *uni= FindUniform(location);
	if(uni) {
		memcpy(uni->getData(), param, size);
		uni->SetData(location, type, transpose);
	}
	else {
		uni = new BL_Uniform(size);
		memcpy(uni->getData(), param, size);
		uni->SetData(location, type, transpose);
		mUniforms.push_back(uni);
	}
	mDirty = true;
#endif
}


void BL_Shader::ApplyShader()
{
#ifdef SORT_UNIFORMS
	if(!mDirty) 
		return;

	for(unsigned int i=0; i<mUniforms.size(); i++)
		mUniforms[i]->Apply(this);

	mDirty = false;
#endif
}

void BL_Shader::UnloadShader()
{
	//
}


bool BL_Shader::LinkProgram()
{
#ifdef GL_ARB_shader_objects

	int vertlen = 0, fraglen=0, proglen=0;
	int vertstatus=0, fragstatus=0, progstatus=0;
	unsigned int tmpVert=0, tmpFrag=0, tmpProg=0;
	int char_len=0;
	char *logInf =0;

	if(mError)
		goto programError;

	if(!vertProg || !fragProg){
		spit("Invalid GLSL sources");
		return false;
	}
	if( !RAS_EXT_support._ARB_fragment_shader) {
		spit("Fragment shaders not supported");
		return false;
	}
	if( !RAS_EXT_support._ARB_vertex_shader) {
		spit("Vertex shaders not supported");
		return false;
	}
	
	// -- vertex shader ------------------
	tmpVert = bgl::blCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	bgl::blShaderSourceARB(tmpVert, 1, (const char**)&vertProg, 0);
	bgl::blCompileShaderARB(tmpVert);
	bgl::blGetObjectParameterivARB(tmpVert, GL_OBJECT_INFO_LOG_LENGTH_ARB,(GLint*) &vertlen);
	
	// print info if any
	if( vertlen > 0 && vertlen < MAX_LOG_LEN){
		logInf = (char*)MEM_mallocN(vertlen, "vert-log");
		bgl::blGetInfoLogARB(tmpVert, vertlen, (GLsizei*)&char_len, logInf);
		if(char_len >0) {
			spit("---- Vertex Shader Error ----");
			spit(logInf);
		}
		MEM_freeN(logInf);
		logInf=0;
	}
	// check for compile errors
	bgl::blGetObjectParameterivARB(tmpVert, GL_OBJECT_COMPILE_STATUS_ARB,(GLint*)&vertstatus);
	if(!vertstatus) {
		spit("---- Vertex shader failed to compile ----");
		goto programError;
	}

	// -- fragment shader ----------------
	tmpFrag = bgl::blCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	bgl::blShaderSourceARB(tmpFrag, 1,(const char**)&fragProg, 0);
	bgl::blCompileShaderARB(tmpFrag);
	bgl::blGetObjectParameterivARB(tmpFrag, GL_OBJECT_INFO_LOG_LENGTH_ARB, (GLint*) &fraglen);
	if(fraglen >0 && fraglen < MAX_LOG_LEN){
		logInf = (char*)MEM_mallocN(fraglen, "frag-log");
		bgl::blGetInfoLogARB(tmpFrag, fraglen,(GLsizei*) &char_len, logInf);
		if(char_len >0) {
			spit("---- Fragment Shader Error ----");
			spit(logInf);
		}
		MEM_freeN(logInf);
		logInf=0;
	}

	bgl::blGetObjectParameterivARB(tmpFrag, GL_OBJECT_COMPILE_STATUS_ARB, (GLint*) &fragstatus);
	if(!fragstatus){
		spit("---- Fragment shader failed to compile ----");
		goto programError;
	}

	
	// -- program ------------------------
	//  set compiled vert/frag shader & link
	tmpProg = bgl::blCreateProgramObjectARB();
	bgl::blAttachObjectARB(tmpProg, tmpVert);
	bgl::blAttachObjectARB(tmpProg, tmpFrag);
	bgl::blLinkProgramARB(tmpProg);
	bgl::blGetObjectParameterivARB(tmpProg, GL_OBJECT_INFO_LOG_LENGTH_ARB, (GLint*) &proglen);
	bgl::blGetObjectParameterivARB(tmpProg, GL_OBJECT_LINK_STATUS_ARB, (GLint*) &progstatus);
	

	if(proglen > 0 && proglen < MAX_LOG_LEN) {
		logInf = (char*)MEM_mallocN(proglen, "prog-log");
		bgl::blGetInfoLogARB(tmpProg, proglen, (GLsizei*)&char_len, logInf);
		if(char_len >0) {
			spit("---- GLSL Program ----");
			spit(logInf);
		}
		MEM_freeN(logInf);
		logInf=0;
	}

	if(!progstatus){
		spit("---- GLSL program failed to link ----");
		goto programError;
	}

	// set
	mShader = tmpProg;
	bgl::blDeleteObjectARB(tmpVert);
	bgl::blDeleteObjectARB(tmpFrag);
	mOk		= 1;
	mError = 0;
	return true;

programError:
	if(tmpVert) {
		bgl::blDeleteObjectARB(tmpVert);
		tmpVert=0;
	}
	if(tmpFrag) {
		bgl::blDeleteObjectARB(tmpFrag);
		tmpFrag=0;
	}

	if(tmpProg) {
		bgl::blDeleteObjectARB(tmpProg);
		tmpProg=0;
	}

	mOk		= 0;
	mUse	= 0;
	mError	= 1;
	return false;
#else
	return false;
#endif//GL_ARB_shader_objects
}

const char *BL_Shader::GetVertPtr()
{
	return vertProg?vertProg:0;
}

const char *BL_Shader::GetFragPtr()
{
	return fragProg?fragProg:0;
}

void BL_Shader::SetVertPtr( char *vert )
{
	vertProg = vert;
}

void BL_Shader::SetFragPtr( char *frag )
{
	fragProg = frag;
}

unsigned int BL_Shader::GetProg()
{ 
	return mShader;
}
//
//const BL_Sampler* BL_Shader::GetSampler(int i)
//{
//	MT_assert(i<=MAXTEX);
//	return &mSampler[i];
//}

void BL_Shader::SetSampler(int loc, int unit)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		bgl::blUniform1iARB(loc, unit);
	}
#endif
}
//
//void BL_Shader::InitializeSampler(int unit, BL_Texture* texture)
//{
//	MT_assert(unit<=MAXTEX);
//	mSampler[unit].mTexture = texture;
//	mSampler[unit].mLoc =-1;
//	mSampler[unit].mOwn = 0;
//}

void BL_Shader::SetProg(bool enable)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		if(	mShader != 0 && mOk && enable) {
			bgl::blUseProgramObjectARB(mShader);
		}
		else {
			bgl::blUseProgramObjectARB(0);	
		}
	}
#endif
}

void BL_Shader::Update( const KX_MeshSlot & ms, RAS_IRasterizer* rasty )
{
#ifdef GL_ARB_shader_objects
	if(!Ok() || !mPreDef.size()) 
		return;

	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		MT_Matrix4x4 model;
		model.setValue(ms.m_OpenGLMatrix);
		MT_Matrix4x4 view;
		rasty->GetViewMatrix(view);

		if(mAttr==SHD_TANGENT)
			 ms.m_mesh->SetMeshModified(true);

		BL_UniformVecDef::iterator it;
		for(it = mPreDef.begin(); it!= mPreDef.end(); it++)
		{
			BL_DefUniform *uni = (*it);
			if(uni->mLoc == -1) continue;

			switch (uni->mType)
			{
				case MODELMATRIX:
					{
						SetUniform(uni->mLoc, model);
						break;
					}
				case MODELMATRIX_TRANSPOSE:
					{
						SetUniform(uni->mLoc, model, true);
						break;
					}
				case MODELMATRIX_INVERSE:
					{
						model.invert();
						SetUniform(uni->mLoc, model);
						break;
					}
				case MODELMATRIX_INVERSETRANSPOSE:
					{
						model.invert();
						SetUniform(uni->mLoc, model, true);
						break;
					}
				case MODELVIEWMATRIX:
					{
						SetUniform(uni->mLoc, view*model);
						break;
					}

				case MODELVIEWMATRIX_TRANSPOSE:
					{
						MT_Matrix4x4 mat(view*model);
						SetUniform(uni->mLoc, mat, true);
						break;
					}
				case MODELVIEWMATRIX_INVERSE:
					{
						MT_Matrix4x4 mat(view*model);
						mat.invert();
						SetUniform(uni->mLoc, mat);
						break;
					}
				case MODELVIEWMATRIX_INVERSETRANSPOSE:
					{
						MT_Matrix4x4 mat(view*model);
						mat.invert();
						SetUniform(uni->mLoc, mat, true);
						break;
					}
				case CAM_POS:
					{
						MT_Point3 pos(rasty->GetCameraPosition());
						SetUniform(uni->mLoc, pos);
						break;
					}
				case VIEWMATRIX:
					{
						SetUniform(uni->mLoc, view);
						break;
					}
				case VIEWMATRIX_TRANSPOSE:
					{
						SetUniform(uni->mLoc, view, true);
						break;
					}
				case VIEWMATRIX_INVERSE:
					{
						view.invert();
						SetUniform(uni->mLoc, view);
						break;
					}
				case VIEWMATRIX_INVERSETRANSPOSE:
					{
						view.invert();
						SetUniform(uni->mLoc, view, true);
						break;
					}
				case CONSTANT_TIMER:
					{
						SetUniform(uni->mLoc, (float)rasty->GetTime());
						break;
					}
				default:
					break;
			}
		}
	}
#endif
}


int BL_Shader::GetAttribLocation(const STR_String& name)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		return bgl::blGetAttribLocationARB(mShader, name.ReadPtr());
	}
#endif
	return -1;
}

void BL_Shader::BindAttribute(const STR_String& attr, int loc)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		bgl::blBindAttribLocationARB(mShader, loc, attr.ReadPtr());
	}
#endif
}

int BL_Shader::GetUniformLocation(const STR_String& name)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		MT_assert(mShader!=0);
		int location = bgl::blGetUniformLocationARB(mShader, name.ReadPtr());
		if(location == -1)
			spit("Invalid uniform value: " << name.ReadPtr() << ".");
		return location;
	}
#endif
	return -1;
}

void BL_Shader::SetUniform(int uniform, const MT_Tuple2& vec)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		float value[2];
		vec.getValue(value);
		bgl::blUniform2fvARB(uniform, 1, value);
	}
#endif

}

void BL_Shader::SetUniform(int uniform, const MT_Tuple3& vec)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{	
		float value[3];
		vec.getValue(value);
		bgl::blUniform3fvARB(uniform, 1, value);
	}
#endif
}

void BL_Shader::SetUniform(int uniform, const MT_Tuple4& vec)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		float value[4];
		vec.getValue(value);
		bgl::blUniform4fvARB(uniform, 1, value);
	}
#endif
}

void BL_Shader::SetUniform(int uniform, const unsigned int& val)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		bgl::blUniform1iARB(uniform, val);
	}
#endif
}

void BL_Shader::SetUniform(int uniform, const int val)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		bgl::blUniform1iARB(uniform, val);
	}
#endif
}

void BL_Shader::SetUniform(int uniform, const float& val)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		bgl::blUniform1fARB(uniform, val);
	}
#endif
}

void BL_Shader::SetUniform(int uniform, const MT_Matrix4x4& vec, bool transpose)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		float value[16];
		vec.getValue(value);
		bgl::blUniformMatrix4fvARB(uniform, 1, transpose?GL_TRUE:GL_FALSE, value);
	}
#endif
}

void BL_Shader::SetUniform(int uniform, const MT_Matrix3x3& vec, bool transpose)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		float value[9];
		value[0] = (float)vec[0][0]; value[1] = (float)vec[1][0]; value[2] = (float)vec[2][0]; 
		value[3] = (float)vec[0][1]; value[4] = (float)vec[1][1]; value[5] = (float)vec[2][1]; 
		value[6] = (float)vec[0][2]; value[7] = (float)vec[1][2]; value[7] = (float)vec[2][2]; 
		bgl::blUniformMatrix3fvARB(uniform, 1, transpose?GL_TRUE:GL_FALSE, value);
	}
#endif
}

void BL_Shader::SetUniform(int uniform, const float* val, int len)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		if(len == 2) 
			bgl::blUniform2fvARB(uniform, 1,(GLfloat*)val);
		else if (len == 3)
			bgl::blUniform3fvARB(uniform, 1,(GLfloat*)val);
		else if (len == 4)
			bgl::blUniform4fvARB(uniform, 1,(GLfloat*)val);
		else
			MT_assert(0);
	}
#endif
}

void BL_Shader::SetUniform(int uniform, const int* val, int len)
{
#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_fragment_shader &&
		RAS_EXT_support._ARB_vertex_shader &&
		RAS_EXT_support._ARB_shader_objects 
		)
	{
		if(len == 2) 
			bgl::blUniform2ivARB(uniform, 1, (GLint*)val);
		else if (len == 3)
			bgl::blUniform3ivARB(uniform, 1, (GLint*)val);
		else if (len == 4)
			bgl::blUniform4ivARB(uniform, 1, (GLint*)val);
		else
			MT_assert(0);
	}
#endif
}


PyObject* BL_Shader::_getattr(const STR_String& attr)
{
	_getattr_up(PyObjectPlus);
}


PyMethodDef BL_Shader::Methods[] = 
{
	// creation
	KX_PYMETHODTABLE( BL_Shader, setSource ),
	KX_PYMETHODTABLE( BL_Shader, delSource ),
	KX_PYMETHODTABLE( BL_Shader, getVertexProg ),
	KX_PYMETHODTABLE( BL_Shader, getFragmentProg ),
	KX_PYMETHODTABLE( BL_Shader, setNumberOfPasses ),
	KX_PYMETHODTABLE( BL_Shader, validate),
	/// access functions
	KX_PYMETHODTABLE( BL_Shader, isValid),
	KX_PYMETHODTABLE( BL_Shader, setUniform1f ),
	KX_PYMETHODTABLE( BL_Shader, setUniform2f ),
	KX_PYMETHODTABLE( BL_Shader, setUniform3f ),
	KX_PYMETHODTABLE( BL_Shader, setUniform4f ),
	KX_PYMETHODTABLE( BL_Shader, setUniform1i ),
	KX_PYMETHODTABLE( BL_Shader, setUniform2i ),
	KX_PYMETHODTABLE( BL_Shader, setUniform3i ),
	KX_PYMETHODTABLE( BL_Shader, setUniform4i ),
	KX_PYMETHODTABLE( BL_Shader, setAttrib ),

	KX_PYMETHODTABLE( BL_Shader, setUniformfv ),
	KX_PYMETHODTABLE( BL_Shader, setUniformiv ),
	KX_PYMETHODTABLE( BL_Shader, setUniformDef ),

	KX_PYMETHODTABLE( BL_Shader, setSampler  ),
	KX_PYMETHODTABLE( BL_Shader, setUniformMatrix4 ),
	KX_PYMETHODTABLE( BL_Shader, setUniformMatrix3 ),

	{NULL,NULL} //Sentinel
};


PyTypeObject BL_Shader::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"BL_Shader",
		sizeof(BL_Shader),
		0,
		PyDestructor,
		0,
		__getattr,
		__setattr,
		0,
		__repr,
		0
};


PyParentObject BL_Shader::Parents[] = {
	&PyObjectPlus::Type,
	&BL_Shader::Type,
	NULL
};


KX_PYMETHODDEF_DOC( BL_Shader, setSource," setSource(vertexProgram, fragmentProgram)" )
{
#ifdef GL_ARB_shader_objects
	if(mShader !=0 && mOk  )
	{
		// already set...
		Py_Return;
	}
	char *v,*f;
	int apply=0;
	if( PyArg_ParseTuple(args, "ssi", &v, &f, &apply) )
	{
		vertProg = v;
		fragProg = f;
		if( LinkProgram() ) {
			bgl::blUseProgramObjectARB( mShader );
			mUse = apply!=0;
			Py_Return;
		}
		vertProg = 0;
		fragProg = 0;
		mUse = 0;
		Py_Return;
	}
	return NULL;
#else
	Py_Return;
#endif
}


KX_PYMETHODDEF_DOC( BL_Shader, delSource, "delSource( )" )
{
#ifdef GL_ARB_shader_objects
	ClearUniforms();
	bgl::blUseProgramObjectARB(0);

	bgl::blDeleteObjectARB(mShader);
	mShader		= 0;
	mOk			= 0;
	mUse		= 0;
#endif
	Py_Return;
}

KX_PYMETHODDEF_DOC( BL_Shader, isValid, "isValid()" )
{
	return PyInt_FromLong( ( mShader !=0 &&  mOk ) );
}

KX_PYMETHODDEF_DOC( BL_Shader, getVertexProg ,"getVertexProg( )" )
{
	return PyString_FromString(vertProg?vertProg:"");
}

KX_PYMETHODDEF_DOC( BL_Shader, getFragmentProg ,"getFragmentProg( )" )
{
	return PyString_FromString(fragProg?fragProg:"");
}

KX_PYMETHODDEF_DOC( BL_Shader, validate, "validate()")
{
#ifdef GL_ARB_shader_objects
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	if(mShader==0) {
		PyErr_Format(PyExc_TypeError, "invalid shader object");
		return NULL;
	}
	int stat = 0;
	bgl::blValidateProgramARB(mShader);
	bgl::blGetObjectParameterivARB(mShader, GL_OBJECT_VALIDATE_STATUS_ARB,(GLint*) &stat);


	if(stat > 0 && stat < MAX_LOG_LEN) {
		int char_len=0;
		char *logInf = (char*)MEM_mallocN(stat, "validate-log");

		bgl::blGetInfoLogARB(mShader, stat,(GLsizei*) &char_len, logInf);
		if(char_len >0) {
			spit("---- GLSL Validation ----");
			spit(logInf);
		}
		MEM_freeN(logInf);
		logInf=0;
	}
#endif//GL_ARB_shader_objects
	Py_Return;
}


KX_PYMETHODDEF_DOC( BL_Shader, setSampler, "setSampler(name, index)" )
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	char *uniform="";
	int index=-1;
	if(PyArg_ParseTuple(args, "si", &uniform, &index)) 
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1) {
			if(index >= MAXTEX &&  index < 0)
				spit("Invalid texture sample index: " << index);

#ifdef SORT_UNIFORMS
			SetUniformiv(loc, BL_Uniform::UNI_INT, &index, (sizeof(int)) );
#else
			SetUniform(loc, index);
#endif
			//if(index <= MAXTEX)
			//	mSampler[index].mLoc = loc;
			//else
			//	spit("Invalid texture sample index: " << index);
		}
		Py_Return;
	}
	return NULL;
}

KX_PYMETHODDEF_DOC( BL_Shader, setNumberOfPasses, "setNumberOfPasses( max-pass )" )
{
	int pass = 1;
	if(!PyArg_ParseTuple(args, "i", &pass))
		return NULL;

	mPass = 1;
	Py_Return;
}

/// access functions
KX_PYMETHODDEF_DOC( BL_Shader, setUniform1f, "setUniform1f(name, fx)" )
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	char *uniform="";
	float value=0;
	if(PyArg_ParseTuple(args, "sf", &uniform, &value ))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
#ifdef SORT_UNIFORMS
			SetUniformfv(loc, BL_Uniform::UNI_FLOAT, &value, sizeof(float));
#else			
			SetUniform( loc, (float)value );
#endif
		}
		Py_Return;
	}
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform2f , "setUniform2f(name, fx, fy)")
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	char *uniform="";
	float array[2]={ 0,0 };
	if(PyArg_ParseTuple(args, "sff", &uniform, &array[0],&array[1] ))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
#ifdef SORT_UNIFORMS
			SetUniformfv(loc, BL_Uniform::UNI_FLOAT2, array, (sizeof(float)*2) );
#else
			SetUniform(loc, array, 2);
#endif
		}
		Py_Return;
	}
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform3f, "setUniform3f(name, fx,fy,fz) ")
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	char *uniform="";
	float array[3]={0,0,0};
	if(PyArg_ParseTuple(args, "sfff", &uniform, &array[0],&array[1],&array[2]))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
#ifdef SORT_UNIFORMS
			SetUniformfv(loc, BL_Uniform::UNI_FLOAT3, array, (sizeof(float)*3) );
#else
			SetUniform(loc, array, 3);
#endif
		}
		Py_Return;

	}
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform4f, "setUniform4f(name, fx,fy,fz, fw) ")
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	char *uniform="";
	float array[4]={0,0,0,0};
	if(PyArg_ParseTuple(args, "sffff", &uniform, &array[0],&array[1],&array[2], &array[3]))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
#ifdef SORT_UNIFORMS
			SetUniformfv(loc, BL_Uniform::UNI_FLOAT4, array, (sizeof(float)*4) );
#else
			SetUniform(loc, array, 4);
#endif
		}
		Py_Return;
	}
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform1i, "setUniform1i(name, ix)" )
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	char *uniform="";
	int value=0;
	if(PyArg_ParseTuple(args, "si", &uniform, &value ))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
#ifdef SORT_UNIFORMS
			SetUniformiv(loc, BL_Uniform::UNI_INT, &value, sizeof(int));
#else
			SetUniform(loc, (int)value);
#endif
		}
		Py_Return;
	}
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform2i , "setUniform2i(name, ix, iy)")
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	char *uniform="";
	int array[2]={ 0,0 };
	if(PyArg_ParseTuple(args, "sii", &uniform, &array[0],&array[1] ))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
#ifdef SORT_UNIFORMS
			SetUniformiv(loc, BL_Uniform::UNI_INT2, array, sizeof(int)*2);
#else
			SetUniform(loc, array, 2);
#endif
		}
		Py_Return;
	}
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform3i, "setUniform3i(name, ix,iy,iz) ")
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	char *uniform="";
	int array[3]={0,0,0};
	if(PyArg_ParseTuple(args, "siii", &uniform, &array[0],&array[1],&array[2]))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
#ifdef SORT_UNIFORMS
			SetUniformiv(loc, BL_Uniform::UNI_INT3, array, sizeof(int)*3);
#else
			SetUniform(loc, array, 3);
#endif
		}
		Py_Return;
	}
	return NULL;
}

KX_PYMETHODDEF_DOC( BL_Shader, setUniform4i, "setUniform4i(name, ix,iy,iz, iw) ")
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	char *uniform="";
	int array[4]={0,0,0, 0};
	if(PyArg_ParseTuple(args, "siiii", &uniform, &array[0],&array[1],&array[2], &array[3] ))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
#ifdef SORT_UNIFORMS
			SetUniformiv(loc, BL_Uniform::UNI_INT4, array, sizeof(int)*4);
#else
			SetUniform(loc, array, 4);
#endif
		}
		Py_Return;
	}
	return NULL;
}

KX_PYMETHODDEF_DOC( BL_Shader, setUniformfv , "setUniformfv( float (list2 or list3 or list4) )")
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	char*uniform = "";
	PyObject *listPtr =0;
	float array_data[4] = {0.f,0.f,0.f,0.f};

	if(PyArg_ParseTuple(args, "sO", &uniform, &listPtr))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
			if(PySequence_Check(listPtr))
			{
				unsigned int list_size = PySequence_Size(listPtr);
				
				for(unsigned int i=0; (i<list_size && i<4); i++)
				{
					PyObject *item = PySequence_GetItem(listPtr, i);
					array_data[i] = (float)PyFloat_AsDouble(item);
					Py_DECREF(item);
				}

				switch(list_size)
				{
				case 2:
					{
						float array2[2] = { array_data[0],array_data[1] };
#ifdef SORT_UNIFORMS
						SetUniformfv(loc, BL_Uniform::UNI_FLOAT2, array2, sizeof(float)*2);
#else
						SetUniform(loc, array2, 2);						
#endif
						Py_Return;
					} break;
				case 3:
					{
						float array3[3] = { array_data[0],array_data[1],array_data[2] };
#ifdef SORT_UNIFORMS
						SetUniformfv(loc, BL_Uniform::UNI_FLOAT3, array3, sizeof(float)*3);
#else
						SetUniform(loc, array3, 3);	
#endif
						Py_Return;
					}break;
				case 4:
					{
						float array4[4] = { array_data[0],array_data[1],array_data[2],array_data[3] };
#ifdef SORT_UNIFORMS
						SetUniformfv(loc, BL_Uniform::UNI_FLOAT4, array4, sizeof(float)*4);
#else
						SetUniform(loc, array4, 4);	
#endif
						Py_Return;
					}break;
				default:
					{
						PyErr_Format(PyExc_TypeError, "Invalid list size");
						return NULL;
					}break;
				}
			}
		}
	}
	return NULL;
}

KX_PYMETHODDEF_DOC( BL_Shader, setUniformiv, "setUniformiv( int (list2 or list3 or list4) )")
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	char*uniform = "";
	PyObject *listPtr =0;
	int array_data[4] = {0,0,0,0};

	if(PyArg_ParseTuple(args, "sO", &uniform, &listPtr))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
			if(PySequence_Check(listPtr))
			{
				unsigned int list_size = PySequence_Size(listPtr);
				
				for(unsigned int i=0; (i<list_size && i<4); i++)
				{
					PyObject *item = PySequence_GetItem(listPtr, i);
					array_data[i] = PyInt_AsLong(item);
					Py_DECREF(item);
				}
				switch(list_size)
				{
				case 2:
					{
						int array2[2] = { array_data[0],array_data[1]};
#ifdef SORT_UNIFORMS
						SetUniformiv(loc, BL_Uniform::UNI_INT2, array2, sizeof(int)*2);
#else
						SetUniform(loc, array2, 2);						
#endif
						Py_Return;
					} break;
				case 3:
					{
						int array3[3] = { array_data[0],array_data[1],array_data[2] };
#ifdef SORT_UNIFORMS
						SetUniformiv(loc, BL_Uniform::UNI_INT3, array3, sizeof(int)*3);
						
#else
						SetUniform(loc, array3, 3);	
#endif
						Py_Return;
					}break;
				case 4:
					{
						int array4[4] = { array_data[0],array_data[1],array_data[2],array_data[3] };
#ifdef SORT_UNIFORMS
						SetUniformiv(loc, BL_Uniform::UNI_INT4, array4, sizeof(int)*4);
						
#else
						SetUniform(loc, array4, 4);	
#endif
						Py_Return;
					}break;
				default:
					{
						PyErr_Format(PyExc_TypeError, "Invalid list size");
						return NULL;
					}break;
				}
			}
		}
	}
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniformMatrix4, 
"setUniformMatrix4(uniform-name, mat-4x4, transpose(row-major=true, col-major=false)" )
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	float matr[16] = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	};

	char *uniform="";
	PyObject *matrix=0;
	int transp=1; // MT_ is row major so transpose by default....
	if(PyArg_ParseTuple(args, "sO|i",&uniform, &matrix,&transp))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
			if (PyObject_IsMT_Matrix(matrix, 4))
			{
				MT_Matrix4x4 mat;
				if (PyMatTo(matrix, mat))
				{
#ifdef SORT_UNIFORMS
					mat.getValue(matr);
					SetUniformfv(loc, BL_Uniform::UNI_MAT4, matr, (sizeof(float)*16), (transp!=0) );
#else
					SetUniform(loc,mat,(transp!=0));
#endif
					Py_Return;
				}
			}
		}
	}
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniformMatrix3,
"setUniformMatrix3(uniform-name, list[3x3], transpose(row-major=true, col-major=false)" )
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	float matr[9] = {
		1,0,0,
		0,1,0,
		0,0,1,
	};

	char *uniform="";
	PyObject *matrix=0;
	int transp=1; // MT_ is row major so transpose by default....
	if(PyArg_ParseTuple(args, "sO|i",&uniform, &matrix,&transp))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
			if (PyObject_IsMT_Matrix(matrix, 3))
			{
				MT_Matrix3x3 mat;
				if (PyMatTo(matrix, mat))
				{
#ifdef SORT_UNIFORMS
					mat.getValue(matr);
					SetUniformfv(loc, BL_Uniform::UNI_MAT3, matr, (sizeof(float)*9), (transp!=0) );
#else
					SetUniform(loc,mat,(transp!=0));
#endif
					Py_Return;

				}
			}
		}
	}
	return NULL;
}

KX_PYMETHODDEF_DOC( BL_Shader, setAttrib, "setAttrib(enum)" )
{
#ifdef GL_ARB_shader_objects
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	int attr=0;
	if(PyArg_ParseTuple(args, "i", &attr )) {
		if(mShader==0) {
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		mAttr=SHD_TANGENT;
		bgl::blUseProgramObjectARB(mShader);
		bgl::blBindAttribLocationARB(mShader, mAttr, "Tangent");
		Py_Return;
	}
#endif
	return NULL;
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniformDef, "setUniformDef(name, enum)" )
{
	if(mError) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	char *uniform="";
	int nloc=0;
	if(PyArg_ParseTuple(args, "si",&uniform, &nloc))
	{
		int loc = GetUniformLocation(uniform);
		if(loc != -1)
		{
			bool defined = false;
			BL_UniformVecDef::iterator it = mPreDef.begin();
			while(it != mPreDef.end()) {
				if((*it)->mLoc == loc) {
					defined = true;
					break;
				}
				it++;
			}
			if(defined)
			{
				Py_Return;
			}

			BL_DefUniform *uni = new BL_DefUniform();
			uni->mLoc = loc;
			uni->mType = nloc;
			uni->mFlag = 0;
			mPreDef.push_back(uni);
			Py_Return;
		}
	}
	return NULL;
}

// eof
