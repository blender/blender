#ifndef DUMMY_CLASS_H_INCLUDED
#define DUMMY_CLASS_H_INCLUDED

#include <Eigen/Core>

class DummyClass
{
	public:
		DummyClass();
		
		void solve3x3(float *vec);
		void get_solution(float *vec);
	private:
		Eigen::Matrix3f m_A;
		Eigen::Vector3f m_x;
    

    
    
};

#endif