// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "KinectPluginPrivatePCH.h"





class FKinectPlugin : public IKinectPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FKinectPlugin, KinectPlugin )



void FKinectPlugin::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FKinectPlugin::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}



