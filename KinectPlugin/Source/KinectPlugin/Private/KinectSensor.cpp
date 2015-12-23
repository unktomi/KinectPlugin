// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "KinectPluginPrivatePCH.h"
#include "KinectSensor.h"
#include "AllowWindowsPlatformTypes.h"
#include <kinect.h>
#include <comdef.h>

DEFINE_LOG_CATEGORY(LogKinect);

static void LogError(const FString &context, int hr) {
  _com_error err(hr);
  LPCTSTR errMsg = err.ErrorMessage();
  UE_LOG(LogKinect, Error, TEXT("%s: %d: %s"), *context, hr, errMsg);
}


UKinectSensor::UKinectSensor(const FObjectInitializer& ObjectInitializer)
  : Super( ObjectInitializer )
{
  HRESULT hr = GetDefaultKinectSensor(&KinectSensor);
  if (SUCCEEDED(hr)) {
    hr = KinectSensor->Open();
    if (SUCCEEDED(hr)) {
    }
  } else {
    LogError(TEXT("GetDefaultKinectSensor"), hr);
  }
}
