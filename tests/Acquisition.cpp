/*
   Copyright (c) 2013, Jack Poulson, Ricardo Otazo, and Emmanuel Candes
   All rights reserved.
 
   This file is part of Real-Time Low-rank Plus Sparse MRI (RT-LPS-MRI) and is 
   under the BSD 2-Clause License, which can be found in the LICENSE file in the
   root directory, or at http://opensource.org/licenses/BSD-2-Clause
*/
#include "rt-lps-mri.hpp"
using namespace mri;

int 
main( int argc, char* argv[] )
{
    Initialize( argc, argv );

    try
    {
        const int nc = Input("--nc","number of coils",16);
        const int nt = Input("--nt","number of timesteps",10);
        const int N0 = Input("--N0","bandwidth in x direction",6);
        const int N1 = Input("--N1","bandwidth in y direction",6);
        const int M  = Input("--M","number of non-uniform nodes",36);
        const int n0 = Input("--n0","FFT size in x direction",16);
        const int n1 = Input("--n1","FFT size in y direction",16);
        const int m = Input("--m","cutoff parameter",2);
        const bool print = Input("--print","print matrices?",false);
        ProcessInput();
        PrintInputReport();

        // Give each process a full copy sense there is not much data
        DistMatrix<double,         STAR,STAR> densityComp;
        DistMatrix<Complex<double>,STAR,STAR> sensitivity;

        // Distribute the data amongst the processes working on this slice
        DistMatrix<double,         STAR,VR> X;
        DistMatrix<Complex<double>,STAR,VR> F, FDirect, FHat, FHatDirect;

        // Sample from [0,1]
        Uniform( densityComp, N0*N1, nc, 0.5, 0.5 );

        // Sample from the complex unit ball
        Uniform( sensitivity, M, nt, Complex<double>(0.,0.), 1. );

        // Initialize each column to a 2*M length vector of samples from the
        // real ball of radius 0.5 centered at the origin, then sort
        Uniform( X, 2*M, nc*nt, 0., 0.5 );
        Sort( X ); 

        // Initialize acquisition operator
        InitializeAcquisition
        ( densityComp, sensitivity, X, nc, nt, N0, N1, n0, n1, m );

        // Generate a random source vector
        Uniform( FHat, N0*N1, nc*nt );
        if( print )
        {
            Print( densityComp, "density compensation" );
            Print( sensitivity, "coil sensitivities" );
            Print( X, "X" );
            Print( FHat, "FHat" );
        }

        // TODO: Apply acquisition operator instead

        CoilAwareNFFT2D( FHat, F );
        if( print )
            Print( F, "F after forward" );

        CoilAwareNFT2D( FHat, FDirect );
        if( print )
            Print( FDirect, "F after direct forward" );

        CoilAwareAdjointNFFT2D( FHat, F );
        if( print )
            Print( FHat, "FHat after adjoint" );

        CoilAwareAdjointNFT2D( FHatDirect, F );
        if( print )
            Print( FHatDirect, "FHat after direct adjoint" );

        const double frobFDir = FrobeniusNorm( FDirect );
        const double frobFHatDir = FrobeniusNorm( FHatDirect );
        Axpy( -1., F, FDirect );
        Axpy( -1., FHat, FHatDirect );
        const double frobE = FrobeniusNorm( FDirect );
        const double frobEHat = FrobeniusNorm( FHatDirect );
        if( mpi::WorldRank() == 0 )
        {
            std::cout << "|| F ||_F = " << frobFDir << "\n"
                      << "|| E ||_F = " << frobE << "\n"
                      << "|| E ||_F / || F ||_F = " << frobE/frobFDir <<"\n"
                      << "\n"
                      << "|| FHat ||_F = " << frobFHatDir << "\n"
                      << "|| EHat ||_F = " << frobEHat << "\n"
                      << "|| EHat ||_F / || FHat ||_F = " 
                      << frobEHat/frobFHatDir << "\n"
                      << std::endl;
        }
    }
    catch( std::exception& e ) { ReportException(e); }

    Finalize();
    return 0;
}
