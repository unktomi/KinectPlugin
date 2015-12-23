// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreUObject.h"
#include "IKinectPlugin.h"
#include "AllowWindowsPlatformTypes.h"
#undef DWORD
#define DWORD HIDE_DWORD
#include "concrt.h"
#undef DWORD
#define DWORD ::DWORD
#include "Windows/WindowsSystemIncludes.h"


// You should place include statements to your module's private header files here.  You only need to
// add includes for headers that are used in most of your module's source files though.
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <Shlobj.h>

// Direct2D Header Files
#include <d2d1.h>

#include <Kinect.h>
#include <strsafe.h>

#pragma comment ( lib, "d2d1.lib" )

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
 #elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
 #else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
 #endif
 #endif

 // Safe release for interfaces
 template<class Interface>
   inline void SafeRelease( Interface *& pInterfaceToRelease )
 {
   if ( pInterfaceToRelease != NULL )
     {
       pInterfaceToRelease->Release();
       pInterfaceToRelease = NULL;
     }
 }

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p)=NULL; } }
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p)=NULL; } }
#endif

#ifndef SAFE_FUSION_RELEASE_IMAGE_FRAME
#define SAFE_FUSION_RELEASE_IMAGE_FRAME(p) { if (p) { static_cast<void>(NuiFusionReleaseImageFrame(p)); (p)=NULL; } }
#endif

#include "HideWindowsPlatformTypes.h"

