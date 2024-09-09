// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).

#pragma once

#include "Modules/ModuleManager.h"

class FVolumeTextureToolkitModule : public IModuleInterface
{
public:
	TArray<void*> DllHandles;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
