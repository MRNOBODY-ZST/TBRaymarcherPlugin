// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "Raymarcher/Public/Actor/RaymarchVolume.h"
#include "PerformanceTest1.generated.h"

/**
 * 
 */
UCLASS()
class TESTS_API APerformanceTest1 : public AFunctionalTest
{
	GENERATED_BODY()

	virtual void Tick(float DeltaSeconds) override;

	// Called when the test is started.
	virtual bool RunTest(const TArray<FString>& Params = TArray<FString>()) override;

	// Set the window center to each volume added to ListenerVolumes.
	void SetWindowCenter(float Value);

	// Define if the test was started by calling 'RunTest'
	bool bRunning = false;

	// Define the current execution time from the start of the test.
	float CurrentTime = 0.0f;

	// The volumes this test is affecting.
	UPROPERTY(EditAnywhere)
	TArray<ARaymarchVolume*> ListenerVolumes;

	// Performance helper that captures the current performance. The performance is logged to csv file in
	// <Engine>/Saved/Profiling. It also captures the .uestats file to <Engine>/Saved/Profiling/UnrealStats/.
	UAutomationPerformaceHelper* PerformanceHelper;
};
