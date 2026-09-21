#pragma once

#ifndef USE_CHROME_BASE_LIBRARY
#define USE_CHROME_BASE_LIBRARY
#endif

#ifdef _WIN32
#define PLATFORM PLATFORM_WINDOWS
#else
#define PLATFORM
#endif

#ifdef DYNAMIC_BUILD

#ifdef _WIN32
#ifdef BUILD_DYNAMIC_MODULE
#define BLUETOOTH_EXPORT __declspec(dllexport)
#else
#define BLUETOOTH_EXPORT __declspec(dllimport)
#endif  // defined(BUILD_DYNAMIC_MODULE)
#endif

#else

#ifdef _WIN32
#define BLUETOOTH_EXPORT
#endif

#endif

