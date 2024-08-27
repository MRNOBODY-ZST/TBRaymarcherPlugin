// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
#include "VolumeAsset/Loaders/DCMTKLoader.h"

#include "TextureUtilities.h"

#pragma push_macro("verify")
#pragma push_macro("check")
#undef verify
#undef check

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"

#pragma pop_macro("verify")
#pragma pop_macro("check")

#pragma optimize("", off)

DEFINE_LOG_CATEGORY(LogDCMTK);

UDCMTKLoader* UDCMTKLoader::Get()
{
	return NewObject<UDCMTKLoader>();
}

FVolumeInfo UDCMTKLoader::ParseVolumeInfoFromHeader(FString FileName)
{
	FVolumeInfo Info;
	Info.DataFileName = FileName;

	DcmFileFormat Format;
	if (Format.loadFile(TCHAR_TO_UTF8(*FileName)).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error loading DICOM image!"));
		return Info;
	}

	DcmDataset* Dataset = Format.getDataset();
	OFString SeriesInstanceUID;
	if (Dataset->findAndGetOFString(DCM_SeriesInstanceUID, SeriesInstanceUID).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error getting Series Instance UID!"));
		return Info;
	}

	uint32 NumberOfSlices = 0;
	{
		FString FolderName, FileNameDummy, Extension;
		FPaths::Split(FileName, FolderName, FileNameDummy, Extension);
		TArray<FString> FilesInDir = GetFilesInFolder(FolderName, Extension);

		for (const FString& File : FilesInDir)
		{
			DcmFileFormat FileFormat;
			if (FileFormat.loadFile(TCHAR_TO_UTF8(*(FolderName / File))).bad())
			{
				continue;
			}

			DcmDataset* FileDataset = FileFormat.getDataset();
			OFString FileSeriesInstanceUID;
			if (FileDataset->findAndGetOFString(DCM_SeriesInstanceUID, FileSeriesInstanceUID).bad())
			{
				continue;
			}

			if (FileSeriesInstanceUID == SeriesInstanceUID)
			{
				++NumberOfSlices;
			}
		}
	}

	Uint16 Rows = 0, Columns = 0;
	if (Dataset->findAndGetUint16(DCM_Rows, Rows).bad() || Dataset->findAndGetUint16(DCM_Columns, Columns).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error getting Rows and Columns!"));
		return Info;
	}
	Info.Dimensions = FIntVector(Columns, Rows, NumberOfSlices);

	OFString OfPixelSpacingStr;
	if (Dataset->findAndGetOFString(DCM_PixelSpacing, OfPixelSpacingStr).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error getting Pixel Spacing!"));
		return Info;
	}

	double PixelSpacingX, PixelSpacingY;
	int ScanfResult = sscanf(OfPixelSpacingStr.c_str(), "%lf\\%lf", &PixelSpacingX, &PixelSpacingY);
	if (ScanfResult == 0)
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error parsing Pixel Spacing!"));
		return Info;
	}
	else if (ScanfResult == 1)
	{
		PixelSpacingY = PixelSpacingX;
	}

	double SliceThickness;
	if (Dataset->findAndGetFloat64(DCM_SliceThickness, SliceThickness).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error getting Slice Thickness!"));
		return Info;
	}

	Info.Spacing = FVector(PixelSpacingX, PixelSpacingY, SliceThickness);
	Info.WorldDimensions = Info.Spacing * FVector(Info.Dimensions);

	Uint16 BitsAllocated = 0, PixelRepresentation = 0, SamplesPerPixel = 0;
	if (Dataset->findAndGetUint16(DCM_BitsAllocated, BitsAllocated).bad() ||
		Dataset->findAndGetUint16(DCM_PixelRepresentation, PixelRepresentation).bad() ||
		Dataset->findAndGetUint16(DCM_SamplesPerPixel, SamplesPerPixel).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error getting Pixel Data parameters!"));
		return Info;
	}

	Info.bIsSigned = PixelRepresentation == 1;
	if (SamplesPerPixel == 1)
	{
		switch (BitsAllocated)
		{
			case 8:
				Info.OriginalFormat = Info.bIsSigned ? EVolumeVoxelFormat::SignedChar : EVolumeVoxelFormat::UnsignedChar;
				break;
			case 16:
				Info.OriginalFormat = Info.bIsSigned ? EVolumeVoxelFormat::SignedShort : EVolumeVoxelFormat::UnsignedShort;
				break;
			case 32:
				Info.OriginalFormat = Info.bIsSigned ? EVolumeVoxelFormat::SignedInt : EVolumeVoxelFormat::UnsignedInt;
				break;
			default:
				UE_LOG(LogDCMTK, Error, TEXT("Error getting Bits Allocated!"));
				return Info;
		}

		Info.BytesPerVoxel = BitsAllocated / 8;
	}
	else if (SamplesPerPixel == 3)
	{
		UE_LOG(LogDCMTK, Error, TEXT("RGB DICOM files are not supported!"));
		return Info;
	}
	else
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error getting Samples Per Pixel!"));
		return Info;
	}

	Info.ActualFormat = Info.OriginalFormat;
	Info.bParseWasSuccessful = true;
	Info.bIsCompressed = false;

	return Info;
}

UVolumeAsset* UDCMTKLoader::CreateVolumeFromFile(FString FileName, bool bNormalize /*= true*/, bool bConvertToFloat /*= true*/)
{
	FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
	if (!VolumeInfo.bParseWasSuccessful)
	{
		return nullptr;
	}

	// Get a nice name from the folder we're in to name the asset.
	FString VolumeName;
	GetValidPackageNameFromFolderName(FileName, VolumeName);

	// Create the transient volume asset.
	UVolumeAsset* OutAsset = UVolumeAsset::CreateTransient(VolumeName);
	if (!OutAsset)
	{
		return nullptr;
	}

	// Perform complete load and conversion of data.
	uint8* LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, bConvertToFloat);

	// Get proper pixel format depending on what got saved into the MHDInfo during conversion.
	const EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

	// Create the transient Volume texture.
	UVolumeTextureToolkit::CreateVolumeTextureTransient(OutAsset->DataTexture, PixelFormat, VolumeInfo.Dimensions, LoadedArray);

	delete[] LoadedArray;

	// Check that the texture got created properly.
	if (OutAsset->DataTexture)
	{
		OutAsset->ImageInfo = VolumeInfo;
		return OutAsset;
	}
	else
	{
		return nullptr;
	}
}

UVolumeAsset* UDCMTKLoader::CreatePersistentVolumeFromFile(
	const FString& FileName, const FString& OutFolder, bool bNormalize /*= true*/)
{
	FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
	if (!VolumeInfo.bParseWasSuccessful)
	{
		return nullptr;
	}

	// Get a nice name from the folder we're in to name the asset.
	FString VolumeName;
	GetValidPackageNameFromFolderName(FileName, VolumeName);
	// Create the persistent volume asset.
	UVolumeAsset* OutAsset = UVolumeAsset::CreatePersistent(OutFolder, VolumeName);
	if (!OutAsset)
	{
		return nullptr;
	}

	uint8* LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, false);
	const EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);
	// Create the persistent volume texture.
	const FString VolumeTextureName = "VA_" + VolumeName + "_Data";
	UVolumeTextureToolkit::CreateVolumeTextureAsset(
		OutAsset->DataTexture, VolumeTextureName, OutFolder, PixelFormat, VolumeInfo.Dimensions, LoadedArray, true);
	OutAsset->ImageInfo = VolumeInfo;

	delete[] LoadedArray;
	// Check that the texture got created properly.
	if (OutAsset->DataTexture)
	{
		OutAsset->ImageInfo = VolumeInfo;
		return OutAsset;
	}
	else
	{
		return nullptr;
	}
}

UVolumeAsset* UDCMTKLoader::CreateVolumeFromFileInExistingPackage(
	FString FileName, UObject* ParentPackage, bool bNormalize /*= true*/, bool bConvertToFloat /*= true*/)
{
	FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
	if (!VolumeInfo.bParseWasSuccessful)
	{
		return nullptr;
	}

	// Get a nice name from the folder we're in to name the asset.
	FString VolumeName;
	GetValidPackageNameFromFolderName(FileName, VolumeName);

	// Create the transient volume asset.
	UVolumeAsset* OutAsset = NewObject<UVolumeAsset>(ParentPackage, FName("VA_" + VolumeName), RF_Standalone | RF_Public);
	if (!OutAsset)
	{
		return nullptr;
	}

	uint8* LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, bConvertToFloat);
	EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

	OutAsset->DataTexture =
		NewObject<UVolumeTexture>(ParentPackage, FName("VA_" + VolumeName + "_Data"), RF_Public | RF_Standalone);

	UVolumeTextureToolkit::SetupVolumeTexture(
		OutAsset->DataTexture, PixelFormat, VolumeInfo.Dimensions, LoadedArray, !bConvertToFloat);

	delete[] LoadedArray;

	// Check that the texture got created properly.
	if (OutAsset->DataTexture)
	{
		OutAsset->ImageInfo = VolumeInfo;
		return OutAsset;
	}
	else
	{
		return nullptr;
	}
}

uint8* UDCMTKLoader::LoadAndConvertData(FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat)
{
	unsigned long TotalDataSize = VolumeInfo.GetByteSize();

	FString FolderName, FileNameDummy, Extension;
	FPaths::Split(FilePath, FolderName, FileNameDummy, Extension);

	TArray<FString> FilesInDir = GetFilesInFolder(FolderName, Extension);

	DcmFileFormat Format;
	if (Format.loadFile(TCHAR_TO_UTF8(*FilePath)).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error loading DICOM image!"));
		return nullptr;
	}

	DcmDataset* Dataset = Format.getDataset();
	OFString SeriesInstanceUID;
	if (Dataset->findAndGetOFString(DCM_SeriesInstanceUID, SeriesInstanceUID).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error getting Series Instance UID!"));
		return nullptr;
	}

	uint8* TotalArray = new uint8[TotalDataSize];
	memset(TotalArray, 0, TotalDataSize);

	TArray<double> SliceLocations;
	SliceLocations.Reserve(VolumeInfo.Dimensions.Z);
	uint32 NumberOfSlices = 0;
	for (const FString& SliceFileName : FilesInDir)
	{
		DcmFileFormat SliceFormat;
		if (SliceFormat.loadFile(TCHAR_TO_UTF8(*(FolderName / SliceFileName))).bad())
		{
			continue;
		}

		DcmDataset* SliceDataset = SliceFormat.getDataset();
		OFString SliceSeriesInstancUID;
		if (SliceDataset->findAndGetOFString(DCM_SeriesInstanceUID, SliceSeriesInstancUID).bad())
		{
			continue;
		}

		if (SliceSeriesInstancUID != SeriesInstanceUID)
		{
			continue;
		}

		OFString OfSliceInstanceNumberStr;
		if (SliceDataset->findAndGetOFString(DCM_InstanceNumber, OfSliceInstanceNumberStr).bad())
		{
			UE_LOG(LogDCMTK, Error, TEXT("Error getting Instance Number!"));
			delete[] TotalArray;
			return nullptr;
		}

		const FString SliceInstanceNumberStr = FString(UTF8_TO_TCHAR(OfSliceInstanceNumberStr.c_str()));
		const int SliceNumber = FCString::Atoi(*SliceInstanceNumberStr) - 1;

		double SliceLocation;
		if (SliceDataset->findAndGetFloat64(DCM_SliceLocation, SliceLocation).bad())
		{
			UE_LOG(LogDCMTK, Error, TEXT("Error getting Slice Location!"));
			delete[] TotalArray;
			return nullptr;
		}

		SliceLocations.Add(SliceLocation);

		const Uint8* PixelData;
		unsigned long DataLength;
		SliceDataset->findAndGetUint8Array(DCM_PixelData, PixelData, &DataLength);

		if ((DataLength * SliceNumber) < TotalDataSize)
		{
			memcpy(TotalArray + (DataLength * SliceNumber), PixelData, DataLength);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("DICOM Loader error during memcpy, some data might be missing"));
		}

		++NumberOfSlices;
	}

	SliceLocations.Sort();
	check(SliceLocations.Num() > 2);

	constexpr static const double Tolerance = 0.0001;
	double CalculatedSliceThickness = FMath::Abs(SliceLocations[1] - SliceLocations[0]);
	double PreviousSliceLocation = SliceLocations[1];
	for (int32 i = 2; i < SliceLocations.Num(); ++i)
	{
		const double CurrentSliceLocation = SliceLocations[i];
		const double CurrentSliceThickness = FMath::Abs(CurrentSliceLocation - PreviousSliceLocation);
		const double NewCalculatedSliceThickness = FMath::Abs(CurrentSliceLocation - PreviousSliceLocation);
		if (FMath::Abs(NewCalculatedSliceThickness - CalculatedSliceThickness) > Tolerance)
		{
			UE_LOG(LogDCMTK, Warning, TEXT("Computed slice thickness varies across the dataset! %d != %d"),
				CalculatedSliceThickness, NewCalculatedSliceThickness);
		}
		PreviousSliceLocation = CurrentSliceLocation;
		CalculatedSliceThickness = NewCalculatedSliceThickness;
	}

	if (NumberOfSlices != VolumeInfo.Dimensions.Z)
	{
		UE_LOG(LogDCMTK, Error, TEXT("Number of slices in the folder %d is different from the one in the privided volume info %d"),
			NumberOfSlices, VolumeInfo.Dimensions.Z);
		delete[] TotalArray;
		return nullptr;
	}

	if (FMath::Abs(VolumeInfo.Spacing.Z - CalculatedSliceThickness) > Tolerance)
	{
		UE_LOG(LogDCMTK, Warning, TEXT("Calculated slice thickness %f is different from the one in the header %f"),
			CalculatedSliceThickness, VolumeInfo.Spacing.Z);

		VolumeInfo.Spacing.Z = CalculatedSliceThickness;
		VolumeInfo.WorldDimensions = VolumeInfo.Spacing * FVector(VolumeInfo.Dimensions);
	}

	TotalArray = IVolumeLoader::ConvertData(TotalArray, VolumeInfo, bNormalize, bConvertToFloat);
	return TotalArray;
}

#pragma optimize("", on)
