// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#pragma once

#include "Modules/ModuleManager.h"
#include "OctreeRenderTargetActions.h"

class FVolumeTextureToolkitEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
private:
	TSharedPtr<FOctreeRenderTargetAssetTypeActions> OctreeRenderTargetAssetTypeActions;

};
