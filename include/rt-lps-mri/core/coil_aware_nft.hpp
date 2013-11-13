/*
   Copyright (c) 2013, Jack Poulson, Ricardo Otazo, and Emmanuel Candes
   All rights reserved.
 
   This file is part of Real-Time Low-rank Plus Sparse MRI (RT-LPS-MRI) and is 
   under the BSD 2-Clause License, which can be found in the LICENSE file in the
   root directory, or at http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef RTLPSMRI_CORE_DIRECT_COIL_AWARE_NFT_HPP
#define RTLPSMRI_CORE_DIRECT_COIL_AWARE_NFT_HPP

namespace mri {

inline void
CoilAwareNFT2D
( const DistMatrix<int,            STAR,VR>& coils,
  const DistMatrix<Complex<double>,STAR,VR>& FHat, 
        DistMatrix<Complex<double>,STAR,VR>& F )
{
#ifndef RELEASE
    CallStackEntry cse("CoilAwareNFT2D");
#endif
    const double pi = 4*elem::Atan( 1. );
    const int width = coils.Width();
    const int M = NumNonUniformPoints();
    const int N0 = FirstBandwidth();
    const int N1 = SecondBandwidth();
#ifndef RELEASE
    if( FHat.Height() != N0*N1 )
        LogicError("Invalid FHat height");
    if( FHat.Width() != width )
        LogicError("FHat and coils must have the same width");
    if( FHat.RowAlign() != coils.RowAlign() )
        LogicError("FHat and coils are not aligned");
#endif
    F.AlignWith( FHat );
    Zeros( F, M, width );
    const int locWidth = F.LocalWidth();
    for( int jLoc=0; jLoc<locWidth; ++jLoc )
    {
        nfft_plan& plan = CoilPlan( coils.GetLocal(0,jLoc) );
        const double* XCol = plan.x;
        Complex<double>* FCol = F.Buffer(0,jLoc);
        const Complex<double>* FHatCol = FHat.LockedBuffer(0,jLoc);
        for( int xi=0; xi<M; ++xi )
        {
            const double x0 = XCol[2*xi+0];
            const double x1 = XCol[2*xi+1];
            for( int ki=0; ki<N0; ++ki )
            {
                const double k0 = -N0/2+ki;
                for( int kj=0; kj<N1; ++kj )
                {
                    const Complex<double> fhat = FHatCol[kj+ki*N1];
                    const double k1 = -N1/2+kj;
                    const double theta = -2*pi*(x0*k0+x1*k1);
                    const double realPart = cos(theta);
                    const double imagPart = sin(theta);
                    FCol[xi] += Complex<double>(realPart,imagPart)*fhat;
                }
            }
        }
    }
}

inline void
CoilAwareAdjointNFT2D
( const DistMatrix<int,            STAR,VR>& coils,
        DistMatrix<Complex<double>,STAR,VR>& FHat,
  const DistMatrix<Complex<double>,STAR,VR>& F )
{
#ifndef RELEASE
    CallStackEntry cse("CoilAwareAdjointNFT2D");
#endif
    const double pi = 4*elem::Atan( 1. );
    const int width = coils.Width();
    const int M = NumNonUniformPoints();
    const int N0 = FirstBandwidth();
    const int N1 = SecondBandwidth();
#ifndef RELEASE
    if( F.Height() != M )
        LogicError("Invalid F height");
    if( F.Width() != width )
        LogicError("F and coils must have the same width");
    if( F.RowAlign() != coils.RowAlign() )
        LogicError("F and coils are not aligned");
#endif
    FHat.AlignWith( F );
    Zeros( FHat, N0*N1, width );
    const int locWidth = FHat.LocalWidth();

    for( int jLoc=0; jLoc<locWidth; ++jLoc )
    {
        nfft_plan& plan = CoilPlan( coils.GetLocal(0,jLoc) );
        const double* XCol = plan.x;
        const Complex<double>* FCol = F.LockedBuffer(0,jLoc);
        Complex<double>* FHatCol = FHat.Buffer(0,jLoc);
        for( int xi=0; xi<M; ++xi )
        {
            const double x0 = XCol[2*xi+0];
            const double x1 = XCol[2*xi+1];
            const Complex<double> f = FCol[xi];
            for( int ki=0; ki<N0; ++ki )
            {
                const double k0 = -N0/2+ki;
                for( int kj=0; kj<N1; ++kj )
                {
                    const double k1 = -N1/2+kj;
                    const double theta = 2*pi*(x0*k0+x1*k1);
                    const double realPart = cos(theta);
                    const double imagPart = sin(theta);
                    FHatCol[kj+ki*N1] += Complex<double>(realPart,imagPart)*f;
                }
            }
        }
    }
}

} // namespace mri

#endif // ifndef RTLPSMRI_CORE_DIRECT_COIL_AWARE_NFT_HPP
