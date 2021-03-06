/*
   Copyright (c) 2013-2014, Jack Poulson, Ricardo Otazo, and Emmanuel Candes
   All rights reserved.
 
   This file is part of Real-Time Low-rank Plus Sparse MRI (RT-LPS-MRI).

   RT-LPS-MRI is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   RT-LPS-MRI is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with RT-LPS-MRI.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "rt-lps-mri.hpp"
using namespace mri;

int 
main( int argc, char* argv[] )
{
    Initialize( argc, argv );

    try
    {
        const int N0 = Input("--N0","bandwidth in x direction",6);
        const int N1 = Input("--N1","bandwidth in y direction",6);
        const int nnu = Input("--nnu","number of non-uniform nodes",36);
        const int n0 = Input("--n0","FFT size in x direction",16);
        const int n1 = Input("--n1","FFT size in y direction",16);
        const int m = Input("--m","cutoff parameter",2);
        const int width = Input("--width","number of indep. vectors",10);
        const bool print = Input("--print","print matrices?",false);
        const bool display = Input("--display","display matrices?",false);
        ProcessInput();
        PrintInputReport();

        DistMatrix<double,         STAR,VR> paths;
        DistMatrix<Complex<double>,STAR,VR> F, FDirect, FHat, FHatDirect;

        Uniform( paths, 2*nnu, width, 0., 0.5 );
        Uniform( FHat, N0*N1, width );
        if( print )
        {
            Print( paths, "paths" );
            Print( FHat, "FHat" );
        }
        if( display )
        {
            Display( paths, "paths" );
            Display( FHat, "FHat" );
        }
        const double frobFHatInit = FrobeniusNorm( FHat );

        NFFT2D( N0, N1, nnu, n0, n1, m, FHat, paths, F );
        const double frobFHatAft = FrobeniusNorm( FHat );
        if( print )
            Print( F, "F after forward" );
        if( display )
            Display( F, "F after forward" );
        NFT2D( N0, N1, nnu, FHat, paths, FDirect );
        if( print )
            Print( FDirect, "F after direct forward" );
        if( display )
            Display( FDirect, "F after direct forward" );
        const double frobFDir = FrobeniusNorm( FDirect );
        Axpy( -1., F, FDirect );
        const double frobE = FrobeniusNorm( FDirect );

        AdjointNFFT2D( N0, N1, nnu, n0, n1, m, F, paths, FHat );
        if( print )
            Print( FHat, "FHat after adjoint" );
        if( display )
            Display( FHat, "FHat after adjoint" );
        AdjointNFT2D( N0, N1, nnu, F, paths, FHatDirect );
        if( print )
            Print( FHatDirect, "FHat after direct adjoint" );
        if( display )
            Display( FHatDirect, "FHat after direct adjoint" );
        const double frobFHatDir = FrobeniusNorm( FHatDirect );
        Axpy( -1., FHat, FHatDirect );
        const double frobEHat = FrobeniusNorm( FHatDirect );

        if( mpi::WorldRank() == 0 )
        {
            std::cout << "|| FHat ||_F = " << frobFHatInit << " then "
                                           << frobFHatAft << "\n"
                      << "|| F    ||_F = " << frobFDir << "\n"
                      << "|| E    ||_F = " << frobE << "\n"
                      << "|| E    ||_F / || F ||_F = " << frobE/frobFDir <<"\n"
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
