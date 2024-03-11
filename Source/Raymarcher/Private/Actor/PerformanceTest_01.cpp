// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).


#include "Actor/PerformanceTest_01.h"
#include <cstdlib> // For system function
#include "Python3/Win64/include/Python.h"


void APerformanceTest_01::Tick(float DeltaSeconds)
{
	if (!bRunning)
		return;

	PerformanceHelper->Tick(DeltaSeconds);

	CurrentTime += DeltaSeconds;
	static constexpr float TimeWindow = 1.0f;

	// Iterate the test. Each if case is played every frame in the TimeWindow.
	if (CurrentTime < TimeWindow)
	{
		SetWindowCenter(100.0f);
	}
	else if(CurrentTime > TimeWindow && CurrentTime < TimeWindow * 2.0f)
	{
		for (auto* ListenerVolume : ListenerVolumes)
		{
			ListenerVolume->bRequestedRecompute = true;
		}
	}
	else if(CurrentTime > TimeWindow * 2.0f && CurrentTime < TimeWindow * 3.0f)
	{
		SetWindowCenter(400.0f + TimeWindow * 30);
	}
	else if (CurrentTime > TimeWindow * 3.0f && CurrentTime < TimeWindow * 4.0f)
	{
		for (auto* ListenerVolume : ListenerVolumes)
		{
			ListenerVolume->bRequestedRecompute = true;
		}
	}
	else
	{
		// Report the numbers to the test output window in UE.
		if(auto* stat = PerformanceHelper->GetCurrentRecord())
		{
			FString outStr = stat->GetReportString();
			LogStep(ELogVerbosity::Log, outStr);
		}

		// Write the log file with the csv data to plot. 
		PerformanceHelper->WriteLogFile("PerformanceTest01",".csv");

		PerformanceHelper->EndRecording();
		PerformanceHelper->EndStatsFile();

		FinishTest(EFunctionalTestResult::Succeeded, "PerformanceTest01 passed.");
	}
	
	Super::Tick(DeltaSeconds);
}
bool APerformanceTest_01::RunTest(const TArray<FString>& Params)
{
	bRunning = true;
	GEngine->AddOnScreenDebugMessage(20, 20, FColor::Purple, "Performance test 01 started.");

	PerformanceHelper = NewObject<UAutomationPerformaceHelper>(this);
	PerformanceHelper->BeginRecording(TEXT("PerformanceTest01"), 60.0f, 30.0f, 20.0f);

	// Save also the stats file. The stats file are saved in <Engine>/Saved/Profiling/UnrealStats
	PerformanceHelper->BeginStatsFile(TEXT("MyPerformanceRecord"));

	return Super::RunTest(Params);
}
void APerformanceTest_01::SetWindowCenter(float Value)
{
	for (auto* ListenerVolume : ListenerVolumes)
	{
		//ListenerVolume->Norm
		ListenerVolume->SetWindowCenter(ListenerVolume->VolumeAsset->ImageInfo.NormalizeValue(Value));
	}
}