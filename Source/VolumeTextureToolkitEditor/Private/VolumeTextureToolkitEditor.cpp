// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "VolumeTextureToolkitEditor.h"

#include "OctreeRenderTargetActions.h"

#define LOCTEXT_NAMESPACE "FVolumeTextureToolkitModuleEditor"

void FVolumeTextureToolkitEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	OctreeRenderTargetAssetTypeActions = MakeShared<FOctreeRenderTargetAssetTypeActions>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(OctreeRenderTargetAssetTypeActions.ToSharedRef());
}

void FVolumeTextureToolkitEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	if (!FModuleManager::Get().IsModuleLoaded("Octree Render Target"))
		return;

	FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(OctreeRenderTargetAssetTypeActions.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FVolumeTextureToolkitEditorModule, VolumeTextureToolkitEditor)