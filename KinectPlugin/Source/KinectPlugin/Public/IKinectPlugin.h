// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

KINECTPLUGIN_API DECLARE_LOG_CATEGORY_EXTERN(LogKinect, Warning, All);

/**
 * The public interface to this module
 */
class IKinectPlugin : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IKinectPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IKinectPlugin >( "KinectPlugin" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "KinectPlugin" );
	}
};

