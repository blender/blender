#include "parametrizer.hpp"

namespace qflow {

void Parametrizer::ComputeInverseAffine()
{
    if (flag_adaptive_scale == 0)
        return;
    triangle_space.resize(F.cols());
#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int i = 0; i < F.cols(); ++i) {
        Matrix3d p, q;
        p.col(0) = V.col(F(1, i)) - V.col(F(0, i));
        p.col(1) = V.col(F(2, i)) - V.col(F(0, i));
        p.col(2) = Nf.col(i);
        q = p.inverse();
        triangle_space[i].resize(2, 3);
        for (int j = 0; j < 2; ++j) {
            for (int k = 0; k < 3; ++k) {
                triangle_space[i](j, k) = q(j, k);
            }
        }
    }
}

void Parametrizer::EstimateSlope() {
    auto& mF = hierarchy.mF;
    auto& mQ = hierarchy.mQ[0];
    auto& mN = hierarchy.mN[0];
    auto& mV = hierarchy.mV[0];
    FS.resize(2, mF.cols());
    FQ.resize(3, mF.cols());
    for (int i = 0; i < mF.cols(); ++i) {
        const Vector3d& n = Nf.col(i);
        const Vector3d &q_1 = mQ.col(mF(0, i)), &q_2 = mQ.col(mF(1, i)), &q_3 = mQ.col(mF(2, i));
        const Vector3d &n_1 = mN.col(mF(0, i)), &n_2 = mN.col(mF(1, i)), &n_3 = mN.col(mF(2, i));
        Vector3d q_1n = rotate_vector_into_plane(q_1, n_1, n);
        Vector3d q_2n = rotate_vector_into_plane(q_2, n_2, n);
        Vector3d q_3n = rotate_vector_into_plane(q_3, n_3, n);
        
        auto p = compat_orientation_extrinsic_4(q_1n, n, q_2n, n);
        Vector3d q = (p.first + p.second).normalized();
        p = compat_orientation_extrinsic_4(q, n, q_3n, n);
        q = (p.first * 2 + p.second);
        q = q - n * q.dot(n);
        FQ.col(i) = q.normalized();
    }
    for (int i = 0; i < mF.cols(); ++i) {
        double step = hierarchy.mScale * 1.f;
        
        const Vector3d &n = Nf.col(i);
        Vector3d p = (mV.col(mF(0, i)) + mV.col(mF(1, i)) + mV.col(mF(2, i))) * (1.0 / 3.0);
        Vector3d q_x = FQ.col(i), q_y = n.cross(q_x);
        Vector3d q_xl = -q_x, q_xr = q_x;
        Vector3d q_yl = -q_y, q_yr = q_y;
        Vector3d q_yl_unfold = q_y, q_yr_unfold = q_y, q_xl_unfold = q_x, q_xr_unfold = q_x;
        int f;
        double tx, ty, len;
        
        f = i; len = step;
        TravelField(p, q_xl, len, f, hierarchy.mE2E, mV, mF, Nf, FQ, mQ, mN, triangle_space, &tx, &ty, &q_yl_unfold);
        
        f = i; len = step;
        TravelField(p, q_xr, len, f, hierarchy.mE2E, mV, mF, Nf, FQ, mQ, mN, triangle_space, &tx, &ty, &q_yr_unfold);
        
        f = i; len = step;
        TravelField(p, q_yl, len, f, hierarchy.mE2E, mV, mF, Nf, FQ, mQ, mN, triangle_space, &tx, &ty, &q_xl_unfold);
        
        f = i; len = step;
        TravelField(p, q_yr, len, f, hierarchy.mE2E, mV, mF, Nf, FQ, mQ, mN, triangle_space, &tx, &ty, &q_xr_unfold);
        double dSx = (q_yr_unfold - q_yl_unfold).dot(q_x) / (2.0f * step);
        double dSy = (q_xr_unfold - q_xl_unfold).dot(q_y) / (2.0f * step);
        FS.col(i) = Vector2d(dSx, dSy);
    }
    
    std::vector<double> areas(mV.cols(), 0.0);
    for (int i = 0; i < mF.cols(); ++i) {
        Vector3d p1 = mV.col(mF(1, i)) - mV.col(mF(0, i));
        Vector3d p2 = mV.col(mF(2, i)) - mV.col(mF(0, i));
        double area = p1.cross(p2).norm();
        for (int j = 0; j < 3; ++j) {
            auto index = compat_orientation_extrinsic_index_4(FQ.col(i), Nf.col(i), mQ.col(mF(j, i)), mN.col(mF(j, i)));
            double scaleX = FS.col(i).x(), scaleY = FS.col(i).y();
            if (index.first != index.second % 2) {
                std::swap(scaleX, scaleY);
            }
            if (index.second >= 2) {
                scaleX = -scaleX;
                scaleY = -scaleY;
            }
            hierarchy.mK[0].col(mF(j, i)) += area * Vector2d(scaleX, scaleY);
            areas[mF(j, i)] += area;
        }
    }
    for (int i = 0; i < mV.cols(); ++i) {
        if (areas[i] != 0)
            hierarchy.mK[0].col(i) /= areas[i];
    }
    for (int l = 0; l< hierarchy.mK.size() - 1; ++l)  {
        const MatrixXd &K = hierarchy.mK[l];
        MatrixXd &K_next = hierarchy.mK[l + 1];
        auto& toUpper = hierarchy.mToUpper[l];
        for (int i = 0; i < toUpper.cols(); ++i) {
            Vector2i upper = toUpper.col(i);
            Vector2d k0 = K.col(upper[0]);
            
            if (upper[1] != -1) {
                Vector2d k1 = K.col(upper[1]);
                k0 = 0.5 * (k0 + k1);
            }
            
            K_next.col(i) = k0;
        }
    }
}

} // namespace qflow
