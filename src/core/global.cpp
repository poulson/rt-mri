/*
   Copyright (c) 2013, Jack Poulson, Ricardo Otazo, and Emmanuel Candes
   All rights reserved.
 
   This file is part of Real-Time Low-rank Plus Sparse MRI (RT-LPS-MRI) and is 
   under the BSD 2-Clause License, which can be found in the LICENSE file in the
   root directory, or at http://opensource.org/licenses/BSD-2-Clause
*/
#include "rt-lps-mri.hpp"

namespace { 
bool mriInitializedElemental; 
int numMriInits = 0;
mri::Args* args = 0;
#ifndef RELEASE
std::stack<std::string> callStack;
#endif

bool initializedCoilPlans = false;
int numCoils;
int numTimesteps;
int numNonUniformPoints;
int firstBandwidth;
int secondBandwidth;
elem::DistMatrix<double,elem::STAR,elem::VR>* coilPaths;
std::vector<nfft_plan> localCoilPlans;
}

namespace mri {

void PrintVersion( std::ostream& os )
{
    os << "RT-LPS-MRI version information:\n"
       << "  Git revision: " << GIT_SHA1 << "\n"
       << "  Version:      " << RTLPSMRI_VERSION_MAJOR << "."
                             << RTLPSMRI_VERSION_MINOR << "\n"
       << "  Build type:   " << CMAKE_BUILD_TYPE << "\n"
       << std::endl;
}

void PrintConfig( std::ostream& os )
{
    os << "RT-LPS-MRI configuration:\n";
    os << "  NFFT_INC_DIR: " << NFFT_INC_DIR << "\n";
    elem::PrintConfig( os );
}

void PrintCCompilerInfo( std::ostream& os )
{
    os << "RT-LPS-MRI's C compiler info:\n"
       << "  CMAKE_C_COMPILER:    " << CMAKE_C_COMPILER << "\n"
       << "  MPI_C_COMPILER:      " << MPI_C_COMPILER << "\n"
       << "  MPI_C_INCLUDE_PATH:  " << MPI_C_INCLUDE_PATH << "\n"
       << "  MPI_C_COMPILE_FLAGS: " << MPI_C_COMPILE_FLAGS << "\n"
       << "  MPI_C_LINK_FLAGS:    " << MPI_C_LINK_FLAGS << "\n"
       << "  MPI_C_LIBRARIES:     " << MPI_C_LIBRARIES << "\n"
       << std::endl;
}

void PrintCxxCompilerInfo( std::ostream& os )
{
    os << "RT-LPS-MRI's C++ compiler info:\n"
       << "  CMAKE_CXX_COMPILER:    " << CMAKE_CXX_COMPILER << "\n"
       << "  CXX_FLAGS:             " << CXX_FLAGS << "\n"
       << "  MPI_CXX_COMPILER:      " << MPI_CXX_COMPILER << "\n"
       << "  MPI_CXX_INCLUDE_PATH:  " << MPI_CXX_INCLUDE_PATH << "\n"
       << "  MPI_CXX_COMPILE_FLAGS: " << MPI_CXX_COMPILE_FLAGS << "\n"
       << "  MPI_CXX_LINK_FLAGS:    " << MPI_CXX_LINK_FLAGS << "\n"
       << "  MPI_CXX_LIBRARIES:     " << MPI_CXX_LIBRARIES << "\n"
       << std::endl;
}

bool Initialized()
{ return ::numMriInits > 0; }

void Initialize( int& argc, char**& argv )
{
    // If RT-LPS-MRI has already been initialized, this is a no-op
    if( ::numMriInits > 0 )
    {
        ++::numMriInits;
        return;
    }

    ::args = new Args( argc, argv );
    ::numMriInits = 1;
    if( !elem::Initialized() )
    {
        elem::Initialize( argc, argv );
        ::mriInitializedElemental = true;
    }
    else
    {
        ::mriInitializedElemental = false;
    }
}

void Finalize()
{
    if( ::numMriInits <= 0 )
        throw std::logic_error("Finalized RT-LPS-MRI more than initialized");
    --::numMriInits;
    if( ::mriInitializedElemental )
        elem::Finalize();

    if( ::numMriInits == 0 )
    {
        if( InitializedCoilPlans() )
            FinalizeCoilPlans();
        delete ::args;    
        ::args = 0;
    }
}

#ifndef RELEASE
void PushCallStack( std::string s )
{ ::callStack.push( s ); }

void PopCallStack()
{ ::callStack.pop(); }

void DumpCallStack()
{
    std::ostringstream msg;
    while( !::callStack.empty() )
    {
        msg << "[" << ::callStack.size() << "]: " << ::callStack.top() << "\n";
        ::callStack.pop();
    }
    std::cerr << msg.str();;
    std::cerr.flush();
}
#endif // ifndef RELEASE

void ReportException( std::exception& e )
{
    elem::ReportException( e );
#ifndef RELEASE
    DumpCallStack();
#endif
}

Args& GetArgs()
{
    if( args == 0 )
        throw std::runtime_error("No available instance of Args");
    return *::args;
}

bool InitializedCoilPlans()
{ return ::initializedCoilPlans; }

// All coil trajectories for each timestep are stored in subsequent columns, so 
// that, for instance, the first 'numCoils' columns correspond to the first 
// timestep.
void InitializeCoilPlans
( const DistMatrix<double,STAR,VR>& X, int numCoils, int numTimesteps, 
  int N0, int N1, int n0, int n1, int m )
{
#ifndef RELEASE
    CallStackEntry cse("InitializeCoilPlans");
    if( InitializedCoilPlans() )
        LogicError("Already initialized coil plans");
    if( X.Width() != numCoils*numTimesteps )
        LogicError("Wrong number of k-space paths provided");
#endif
    const int d = 2;
    const int M = X.Height()/d;
    const int numLocalPaths = X.LocalWidth();

    ::numCoils = numCoils;
    ::numTimesteps = numTimesteps;
    ::numNonUniformPoints = M;
    ::firstBandwidth = N0;
    ::secondBandwidth = N1;

#ifndef RELEASE
    // TODO: Ensure that each column of X is sorted
#endif
    ::coilPaths = new DistMatrix<double,STAR,VR>( X );

    int NN[2] = { N0, N1 };
    int nn[2] = { n0, n1 };
    unsigned nfftFlags = PRE_PHI_HUT| PRE_FULL_PSI| FFTW_INIT| FFT_OUT_OF_PLACE;
    unsigned fftwFlags = FFTW_MEASURE| FFTW_DESTROY_INPUT;

    ::localCoilPlans.clear();
    ::localCoilPlans.resize( numLocalPaths );
    for( int localPath=0; localPath<numLocalPaths; ++localPath )
    {
        nfft_plan& plan = ::localCoilPlans[localPath];
        plan.x = ::coilPaths->Buffer(0,localPath);
        nfft_init_guru( &plan, d, NN, M, nn, m, nfftFlags, fftwFlags );
        if( plan.nfft_flags & PRE_ONE_PSI )
            nfft_precompute_one_psi( &plan );
    }

    ::initializedCoilPlans = true;
}

void FinalizeCoilPlans()
{
#ifndef RELEASE
    CallStackEntry cse("FinalizeCoilPlans");
    if( !InitializedCoilPlans() )
        LogicError("Have not yet initialized coil plans");
#endif
    const int numLocalPaths = ::localCoilPlans.size();
    for( int localPath=0; localPath<numLocalPaths; ++localPath )
        nfft_finalize( &::localCoilPlans[localPath] );
    ::localCoilPlans.clear();
    delete ::coilPaths;

    ::initializedCoilPlans = false;
}

int NumCoils()
{
#ifndef RELEASE
    CallStackEntry cse("NumCoils");
    if( !InitializedCoilPlans() )
        LogicError("Have not yet initialized coil plans");
#endif
    return ::numCoils;
}

int NumTimesteps()
{
#ifndef RELEASE
    CallStackEntry cse("NumTimesteps");
    if( !InitializedCoilPlans() )
        LogicError("Have not yet initialized coil plans");
#endif
    return ::numTimesteps;
}

int NumLocalPaths()
{
#ifndef RELEASE
    CallStackEntry cse("NumLocalPaths");
    if( !InitializedCoilPlans() )
        LogicError("Have not yet initialized coil plans");
#endif
    return ::localCoilPlans.size();
}

int NumNonUniformPoints()
{ 
#ifndef RELEASE
    CallStackEntry cse("NumNonUniformPoints");
    if( !InitializedCoilPlans() )
        LogicError("Have not yet initialized coil plans");
#endif
    return ::numNonUniformPoints; 
}

int FirstBandwidth()
{ 
#ifndef RELEASE
    CallStackEntry cse("FirstBandwidth");
    if( !InitializedCoilPlans() )
        LogicError("Have not yet initialized coil plans");
#endif
    return ::firstBandwidth; 
}

int SecondBandwidth()
{ 
#ifndef RELEASE
    CallStackEntry cse("SecondBandwidth");
    if( !InitializedCoilPlans() )
        LogicError("Have not yet initialized coil plans");
#endif
    return ::secondBandwidth; 
}

nfft_plan& LocalCoilPlan( int localPath )
{ 
#ifndef RELEASE
    CallStackEntry cse("CoilPlan");
    if( !InitializedCoilPlans() )
        LogicError("Have not yet initialized coil plans");
#endif
    return ::localCoilPlans[localPath]; 
}

} // namespace mri
