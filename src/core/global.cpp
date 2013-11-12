/*
   Copyright (c) 2013, Jack Poulson, Ricardo Otazo, and Emmanuel Candes
   All rights reserved.

   This file is part of RealTime-MRI and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "rt-mri.hpp"

namespace { 
bool rtmriInitializedElemental; 
int numRtmriInits = 0;
rtmri::Args* args = 0;
#ifndef RELEASE
std::stack<std::string> callStack;
#endif
}

namespace rtmri {

void PrintVersion( std::ostream& os )
{
    os << "RT-MRI version information:\n"
       << "  Git revision: " << GIT_SHA1 << "\n"
       << "  Version:      " << RTMRI_VERSION_MAJOR << "."
                             << RTMRI_VERSION_MINOR << "\n"
       << "  Build type:   " << CMAKE_BUILD_TYPE << "\n"
       << std::endl;
}

void PrintConfig( std::ostream& os )
{
    os << "RT-MRI configuration:\n";
    // TODO: Custom options
    elem::PrintConfig( os );
}

void PrintCCompilerInfo( std::ostream& os )
{
    os << "RT-MRI's C compiler info:\n"
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
    os << "RT-MRI's C++ compiler info:\n"
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
{ return ::numRtmriInits > 0; }

void Initialize( int& argc, char**& argv )
{
    // If RT-MRI has already been initialized, this is a no-op
    if( ::numRtmriInits > 0 )
    {
        ++::numRtmriInits;
        return;
    }

    ::args = new Args( argc, argv );
    ::numRtmriInits = 1;
    if( !elem::Initialized() )
    {
        elem::Initialize( argc, argv );
        ::rtmriInitializedElemental = true;
    }
    else
    {
        ::rtmriInitializedElemental = false;
    }
}

void Finalize()
{
    if( ::numRtmriInits <= 0 )
        throw std::logic_error("Finalized RT-MRI more than initialized");
    --::numRtmriInits;
    if( ::rtmriInitializedElemental )
        elem::Finalize();

    if( ::numRtmriInits == 0 )
    {
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

} // namespace rtmri