// ------------------------------------
#ifdef WIN32
#include <windows.h>
#endif // WIN32
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <iostream>
#include "BL_Shader.h"
#include "BL_Material.h"

#include "MT_assert.h"
#include "MT_Matrix4x4.h"
#include "MT_Matrix3x3.h"
#include "KX_PyMath.h"

#include "RAS_GLExtensionManager.h"

using namespace bgl;
#define spit(x) std::cout << x << std::endl;


BL_Shader::BL_Shader(int n, PyTypeObject *T)
:	PyObjectPlus(T),
	mShader(0),
	mVert(0),
	mFrag(0),
	mPass(1),
	vertProg(""),
	fragProg(""),
	mOk(0),
	mUse(0)
{
	// if !RAS_EXT_support._ARB_shader_objects this class will not be used

	mBlending.src	= -1;
	mBlending.dest	= -1;
	mBlending.const_color[0] = 0.0;
	mBlending.const_color[1] = 0.0;
	mBlending.const_color[2] = 0.0;
	mBlending.const_color[3] = 1.0;

	for (int i=0; i<MAXTEX; i++)
	{
		mSampler[i].type = 0;
		mSampler[i].pass = 0;
		mSampler[i].unit = -1;
		mSampler[i].loc  = -1;
		mSampler[i].glTexture =0;
	}
}

BL_Shader::~BL_Shader()
{
#ifdef GL_ARB_shader_objects
	if( mShader ) {
		glDeleteObjectARB(mShader);
		mShader = 0;
	}
	if( mFrag ) {
		glDeleteObjectARB(mFrag);
		mFrag = 0;
	}
	if( mVert ) {
		glDeleteObjectARB(mVert);
		mVert		= 0;
	}

	vertProg	= 0;
	fragProg	= 0;
	mOk			= 0;

	glUseProgramObjectARB(0);
#endif//GL_ARB_shader_objects
}


bool BL_Shader::LinkProgram()
{
#ifdef GL_ARB_shader_objects
	if(!vertProg || !fragProg ) return false;

	int vertstat,fragstat,progstat;
	
	// vertex prog
	unsigned int vert = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	glShaderSourceARB(vert, 1, (const char**) &vertProg, 0);
	glCompileShaderARB(vert);
	glGetObjectParameterivARB(vert, GL_OBJECT_INFO_LOG_LENGTH_ARB, &vertstat);
	// errors if any
	printInfo(vert);
	
	// fragment prog
	unsigned int frag = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB(frag, 1,(const char**) &fragProg, 0);
	glCompileShaderARB(frag);
	glGetObjectParameterivARB(frag, GL_OBJECT_INFO_LOG_LENGTH_ARB, &fragstat);
	// errors if any
	printInfo(frag);
	
	if(!vertstat || !fragstat) return false;

	// main prog
	unsigned int prog = glCreateProgramObjectARB();
	glAttachObjectARB(prog,vert);
	glAttachObjectARB(prog,frag);

	glLinkProgramARB(prog);
	glGetObjectParameterivARB(prog, GL_OBJECT_INFO_LOG_LENGTH_ARB, &progstat);
	// info on how it compiled &| linked
	printInfo(prog);

	if(!progstat)
		return false;

	// assign
	mShader = prog;
	mVert	= vert;
	mFrag	= frag;
	mOk		= 1;
	return true;
#else
	return false;
#endif//GL_ARB_shader_objects
}

void BL_Shader::printInfo(unsigned int pr)
{
#ifdef GL_ARB_shader_objects
	int length=0;
	glGetObjectParameterivARB(pr, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);

	if(length > 1)
	{
		GLcharARB*logger = (GLcharARB*)malloc(sizeof(GLcharARB)*length);
		int chars=0;
		
		glGetInfoLogARB(pr, length, &chars, logger);
		if(chars>0)
			std::cout << (logger) << std::endl;

		if(logger)
			free(logger);
	}
#endif//GL_ARB_shader_objects
}

char *BL_Shader::GetVertPtr()
{
	return vertProg?vertProg:0;
}

char *BL_Shader::GetFragPtr()
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

unsigned int BL_Shader::GetVertexShader()
{ 
	return mVert;  
}

unsigned int BL_Shader::GetFragmentShader()
{ 
	return mFrag;  
}

const uSampler* BL_Shader::getSampler(int i)
{
	MT_assert(i<=MAXTEX);
	return &mSampler[i];
}

const uBlending *BL_Shader::getBlending( int pass )
{
	return &mBlending;
}

const bool BL_Shader::Ok()const
{
	return (mShader !=0 && mOk && mUse);
}


void BL_Shader::InitializeSampler(
	int type,
	int unit,
	int pass,
	unsigned int texture)
{
	MT_assert(unit<=MAXTEX);
	mSampler[unit].glTexture = texture;
	mSampler[unit].loc =-1;
	mSampler[unit].pass=0;
	mSampler[unit].type=type;
	mSampler[unit].unit=unit;
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

	KX_PYMETHODTABLE( BL_Shader, setUniformfv ),
	KX_PYMETHODTABLE( BL_Shader, setUniformiv ),

	KX_PYMETHODTABLE( BL_Shader, setSampler  ),
	KX_PYMETHODTABLE( BL_Shader, setUniformMatrix4 ),
	KX_PYMETHODTABLE( BL_Shader, setUniformMatrix3 ),
	// KX_PYMETHODTABLE( BL_Shader, setBlending ),

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
		if( LinkProgram() )
		{
			glUseProgramObjectARB( mShader );
			mUse = apply!=0;
			Py_Return;
		}
		else
		{
			vertProg = 0;
			fragProg = 0;
			mUse = 0;
			glUseProgramObjectARB( 0 );
		}
	}
	return NULL;
#else
	Py_Return;
#endif
}


KX_PYMETHODDEF_DOC( BL_Shader, delSource, "delSource( )" )
{
#ifdef GL_ARB_shader_objects
	glDeleteObjectARB(mShader);
	glDeleteObjectARB(mFrag);
	glDeleteObjectARB(mVert);
	mShader		= 0;
	mFrag		= 0;
	mVert		= 0;
	vertProg	= 0;
	fragProg	= 0;
	mOk			= 0;
	mUse		= 0;
	glUseProgramObjectARB(0);
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
	if(mShader==0)
	{
		PyErr_Format(PyExc_TypeError, "invalid shader object");
		return NULL;
	}

	int stat = 0;
	glValidateProgramARB(mShader);
	glGetObjectParameterivARB(mShader, GL_OBJECT_VALIDATE_STATUS_ARB, &stat);
	printInfo(mShader);

	return PyInt_FromLong((stat!=0));
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setSampler, "setSampler(name, index)" )
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	int index=-1;
	if(PyArg_ParseTuple(args, "si", &uniform, &index)) 
	{
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader, uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			if(index <= MAXTEX)
			{
				mSampler[index].loc = loc;
			}else
			{
				spit("Invalid texture sample index: " << index);
			}
			Py_Return;
		}
	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}

KX_PYMETHODDEF_DOC( BL_Shader, setNumberOfPasses, "setNumberOfPasses( max-pass )" )
{
	int pass = 1;
	if(!PyArg_ParseTuple(args, "i", &pass))
		return NULL;

	mPass = pass;
	Py_Return;
}

/// access functions
KX_PYMETHODDEF_DOC( BL_Shader, setUniform1f, "setUniform1f(name, fx)" )
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	float value=0;
	if(PyArg_ParseTuple(args, "sf", &uniform, &value ))
	{
		if( mShader==0 )
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader, uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			glUseProgramObjectARB( mShader );
			glUniform1fARB( loc, value );
			Py_Return;
		}

	}
	return NULL;
#else
	Py_Return;
#endif
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform2f , "setUniform2f(name, fx, fy)")
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	float array[2]={ 0,0 };
	if(PyArg_ParseTuple(args, "sff", &uniform, &array[0],&array[1] ))
	{
		if( mShader==0 )
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			glUseProgramObjectARB( mShader );
			glUniform2fARB(loc, array[0],array[1] );
			Py_Return;
		}

	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform3f, "setUniform3f(name, fx,fy,fz) ")
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	float array[3]={0,0,0};
	if(PyArg_ParseTuple(args, "sfff", &uniform, &array[0],&array[1],&array[2]))
	{
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			glUseProgramObjectARB(mShader);
			glUniform3fARB(loc, array[0],array[1],array[2]);
			Py_Return;
		}

	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform4f, "setUniform4f(name, fx,fy,fz, fw) ")
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	float array[4]={0,0,0,0};
	if(PyArg_ParseTuple(args, "sffff", &uniform, &array[0],&array[1],&array[2], &array[3]))
	{
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			glUseProgramObjectARB(mShader);
			glUniform4fARB(loc, array[0],array[1],array[2], array[3]);
			Py_Return;
		}
	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform1i, "setUniform1i(name, ix)" )
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	int value=0;
	if(PyArg_ParseTuple(args, "si", &uniform, &value ))
	{
		if( mShader==0 )
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader, uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			glUseProgramObjectARB( mShader );
			glUniform1iARB( loc, value );
			Py_Return;
		}
	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform2i , "setUniform2i(name, ix, iy)")
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	int array[2]={ 0,0 };
	if(PyArg_ParseTuple(args, "sii", &uniform, &array[0],&array[1] ))
	{
		if( mShader==0 )
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			glUseProgramObjectARB( mShader );
			glUniform2iARB(loc, array[0],array[1] );
			Py_Return;
		}
	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniform3i, "setUniform3i(name, ix,iy,iz) ")
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	int array[3]={0,0,0};
	if(PyArg_ParseTuple(args, "siii", &uniform, &array[0],&array[1],&array[2]))
	{
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			glUseProgramObjectARB(mShader);
			glUniform3iARB(loc, array[0],array[1],array[2]);
			Py_Return;
		}
	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}

KX_PYMETHODDEF_DOC( BL_Shader, setUniform4i, "setUniform4i(name, ix,iy,iz, iw) ")
{
#ifdef GL_ARB_shader_objects
	char *uniform="";
	int array[4]={0,0,0, 0};
	if(PyArg_ParseTuple(args, "siiii", &uniform, &array[0],&array[1],&array[2], &array[3] ))
	{
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			glUseProgramObjectARB(mShader);
			glUniform4iARB(loc, array[0],array[1],array[2], array[3]);
			Py_Return;
		}
	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}

KX_PYMETHODDEF_DOC( BL_Shader, setUniformfv , "setUniformfv( float (list2 or list3 or list4) )")
{
#ifdef GL_ARB_shader_objects
	char*uniform = "";
	PyObject *listPtr =0;
	float array_data[4] = {0.f,0.f,0.f,0.f};

	if(PyArg_ParseTuple(args, "sO", &uniform, &listPtr))
	{
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			if(PySequence_Check(listPtr))
			{
				unsigned int list_size = PySequence_Size(listPtr);
				
				for(unsigned int i=0; (i<list_size && i<=4); i++)
				{
					PyObject *item = PySequence_GetItem(listPtr, i);
					array_data[i] = (float)PyFloat_AsDouble(item);
					Py_DECREF(item);
				}
				switch(list_size)
				{
				case 2:
					{
						glUseProgramObjectARB(mShader);
						glUniform2fARB(loc, array_data[0],array_data[1]);
						Py_Return;
					} break;
				case 3:
					{
						glUseProgramObjectARB(mShader);
						glUniform3fARB(loc, array_data[0],array_data[1], array_data[2]);
						Py_Return;
					}break;
				case 4:
					{
						glUseProgramObjectARB(mShader);
						glUniform4fARB(loc, array_data[0],array_data[1], array_data[2], array_data[3]);
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
#else
	Py_Return;
#endif//GL_ARB_shader_objects

}

KX_PYMETHODDEF_DOC( BL_Shader, setUniformiv, "setUniformiv( int (list2 or list3 or list4) )")
{
#ifdef GL_ARB_shader_objects
	char*uniform = "";
	PyObject *listPtr =0;
	int array_data[4] = {0,0,0,0};

	if(PyArg_ParseTuple(args, "sO", &uniform, &listPtr))
	{
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			if(PySequence_Check(listPtr))
			{
				unsigned int list_size = PySequence_Size(listPtr);
				
				for(unsigned int i=0; (i<list_size && i<=4); i++)
				{
					PyObject *item = PySequence_GetItem(listPtr, i);
					array_data[i] = PyInt_AsLong(item);
					Py_DECREF(item);
				}
				switch(list_size)
				{
				case 2:
					{
						glUseProgramObjectARB(mShader);
						glUniform2iARB(loc, array_data[0],array_data[1]);
						Py_Return;
					} break;
				case 3:
					{
						glUseProgramObjectARB(mShader);
						glUniform3iARB(loc, array_data[0],array_data[1], array_data[2]);
						Py_Return;
					}break;
				case 4:
					{
						glUseProgramObjectARB(mShader);
						glUniform4iARB(loc, array_data[0],array_data[1], array_data[2], array_data[3]);
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
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniformMatrix4, 
"setUniformMatrix4(uniform-name, mat-4x4, transpose(row-major=true, col-major=false)" )
{
#ifdef GL_ARB_shader_objects
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
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			if (PyObject_IsMT_Matrix(matrix, 4))
			{
				MT_Matrix4x4 mat;
				if (PyMatTo(matrix, mat))
				{
					mat.getValue(matr);
					glUseProgramObjectARB(mShader);
					glUniformMatrix4fvARB(loc, 1, (transp!=0)?GL_TRUE:GL_FALSE, matr);
					Py_Return;
				}
			}
		}
	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setUniformMatrix3,
"setUniformMatrix3(uniform-name, list[3x3], transpose(row-major=true, col-major=false)" )
{
#ifdef GL_ARB_shader_objects
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
		if(mShader==0)
		{
			PyErr_Format(PyExc_ValueError, "invalid shader object");
			return NULL;
		}
		int loc= glGetUniformLocationARB(mShader , uniform);
		if( loc==-1 )
		{
			spit("Invalid uniform value: " << uniform << ".");
			Py_Return;
		}else
		{
			if (PyObject_IsMT_Matrix(matrix, 3))
			{
				MT_Matrix3x3 mat;
				if (PyMatTo(matrix, mat))
				{
					mat.getValue(matr);
					glUseProgramObjectARB(mShader);
					glUniformMatrix3fvARB(loc, 1, (transp!=0)?GL_TRUE:GL_FALSE, matr);
					Py_Return;
				}
			}
		}
	}
	return NULL;
#else
	Py_Return;
#endif//GL_ARB_shader_objects
}


KX_PYMETHODDEF_DOC( BL_Shader, setBlending, "setBlending(src, dest)" )
{
	int src, dest;
	if(PyArg_ParseTuple(args, "ii", &src, &dest))
	{
		mBlending.src = src;
		mBlending.dest = dest;
		Py_Return;
	}
	return NULL;
}
