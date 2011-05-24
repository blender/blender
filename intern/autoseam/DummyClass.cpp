#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues> 


#include "DummyClass.h"

using Eigen::MatrixXd;


DummyClass::DummyClass()
{
	m_A <<  1.0f, 2.0f, 0.0f,
			0.0f, 2.0f, 3.0f,
			0.0f, 0.0f, 3.0f;
}

void DummyClass::solve3x3(float *vec)
{
	Eigen::Vector3f b;
	b << vec[0], vec[1], vec[2];
	//m_A.lu().solve(b, &m_x);
    MatrixXd m(2,2);
    m(0,0) = 3;
    m(1,0) = 2.5;
    m(0,1) = -1;
    m(1,1) = m(1,0) + m(0,1);
    std::cout << m << std::endl;
    
    
    MatrixXd A = MatrixXd::Random(6,6);
    std::cout << "Here is a random 6x6 matrix, A:" << std::endl << A << std::endl << std::endl;
    
    Eigen::EigenSolver<MatrixXd> es(A);
    std::cout << "The eigenvalues of A are:" << std::endl << es.eigenvalues() << std::endl;
    std::cout << "The matrix of eigenvectors, V, is:" << std::endl << es.eigenvectors() << std::endl << std::endl;
    
    /*Eigen::complex<double> lambda = es.eigenvalues()[0];
    std::cout << "Consider the first eigenvalue, lambda = " << lambda << std::endl;
    Eigen::VectorXcd v = es.eigenvectors().col(0);
    std::cout << "If v is the corresponding eigenvector, then lambda * v = " << std::endl << lambda * v << std::endl;
    std::cout << "... and A * v = " << std::endl << A.cast<complex<double> >() * v << std::endl << std::endl;*/
    
    Eigen::MatrixXcd D = es.eigenvalues().asDiagonal();
    Eigen::MatrixXcd V = es.eigenvectors();
    std::cout << "Finally, V * D * V^(-1) = " <<     std::endl << V * D * V.inverse() <<     std::endl;

}

void DummyClass::get_solution(float *vec)
{
	vec[0] = m_x[0];
	vec[1] = m_x[1];
	vec[2] = m_x[2];
}

