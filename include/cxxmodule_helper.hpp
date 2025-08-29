#pragma once
#ifndef IOUXX_CXXMOUDULE_HELPER_H
#define IOUXX_CXXMOUDULE_HELPER_H 1

/*
    * This file contains some helper macros for C++ modules.
    * It is not intended to be included directly by users.
    * Use '-DIOUXX_USE_CXX_MODULE' to enable C++ module support.
*/

#ifdef IOUXX_USE_CXX_MODULE

#if defined(__cpp_modules) && __cpp_modules >= 201907L

#define IOUXX_EXPORT export

#else // ! __cpp_modules
#error "C++ modules are not supported by the compiler."
#endif // __cpp_modules

#else // ! IOUXX_USE_CXX_MODULE

#define IOUXX_EXPORT /* nothing */

#endif // IOUXX_USE_CXX_MODULE

#endif // IOUXX_CXXMOUDULE_HELPER_H
