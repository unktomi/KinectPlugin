// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KinectSensor.generated.h"


/**
 * Example UStruct declared in a plugin module
 */
USTRUCT()
struct FMyKinectPluginStruct
{
  GENERATED_USTRUCT_BODY()
  
  UPROPERTY()
  FString TestString;
};
 

/**
 * Example of declaring a UObject in a plugin module
 */
UCLASS()
class UKinectSensor : public UObject
{
  GENERATED_BODY()

 public:
  UKinectSensor(const FObjectInitializer& ObjectInitializer);
  
 private:
  struct IKinectSensor *KinectSensor;
};


