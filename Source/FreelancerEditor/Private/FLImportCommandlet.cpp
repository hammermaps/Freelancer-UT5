// Copyright Epic Games, Inc. All Rights Reserved.

#include "FLImportCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/FLShipData.h"
#include "Misc/PackageName.h"
#include "ShipData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogFLImport, Log, All);

UFLImportCommandlet::UFLImportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

void UFLImportCommandlet::FillShipData(UFLShipData* Data, const fl::FlShipRecord& R)
{
	Data->Nickname = FName(UTF8_TO_TCHAR(R.Nickname.c_str()));
	Data->ShipType = FName(UTF8_TO_TCHAR(R.Type.c_str()));
	Data->IdsName = R.IdsName;
	Data->IdsInfo = R.IdsInfo;
	Data->ShipClass = R.ShipClass;
	Data->Mass = R.Mass;
	Data->HitPoints = R.HitPts;
	Data->HoldSize = R.HoldSize;
	Data->LinearDrag = R.LinearDrag;
	Data->NudgeForce = R.NudgeForce;
	Data->StrafeForce = R.StrafeForce;
	Data->SteeringTorque = FVector(R.SteeringTorque[0], R.SteeringTorque[1], R.SteeringTorque[2]);
	Data->AngularDrag = FVector(R.AngularDrag[0], R.AngularDrag[1], R.AngularDrag[2]);
	Data->RotationInertia = FVector(R.RotationInertia[0], R.RotationInertia[1], R.RotationInertia[2]);
	Data->SourceArchetype = UTF8_TO_TCHAR(R.Archetype.c_str());
	Data->SourceCockpit = UTF8_TO_TCHAR(R.Cockpit.c_str());
	Data->SourceMaterialLibraries.Reset();
	for (const std::string& M : R.MaterialLibraries)
	{
		Data->SourceMaterialLibraries.Add(UTF8_TO_TCHAR(M.c_str()));
	}
}

int32 UFLImportCommandlet::ImportShips(const FString& DataDir)
{
	const FString IniPath = FPaths::Combine(DataDir, TEXT("SHIPS"), TEXT("shiparch.ini"));
	fl::IniFile Ini = fl::IniFile::Load(TCHAR_TO_UTF8(*IniPath));
	if (!Ini.bOk)
	{
		UE_LOG(LogFLImport, Error, TEXT("%s: %s"), *IniPath, UTF8_TO_TCHAR(Ini.Error.c_str()));
		return -1;
	}

	int32 Count = 0;
	for (const fl::FlShipRecord& R : fl::ParseShipArch(Ini))
	{
		const FString Name = FString::Printf(TEXT("DA_Ship_%s"), UTF8_TO_TCHAR(R.Nickname.c_str()));
		const FString PackageName = FString::Printf(TEXT("/Game/Imported/Ships/%s"), *Name);
		UPackage* Package = CreatePackage(*PackageName);
		UFLShipData* Data = NewObject<UFLShipData>(Package, *Name, RF_Public | RF_Standalone);
		FillShipData(Data, R);
		FAssetRegistryModule::AssetCreated(Data);
		Package->MarkPackageDirty();

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!UPackage::SavePackage(Package, Data, *File, Args))
		{
			UE_LOG(LogFLImport, Error, TEXT("Speichern fehlgeschlagen: %s"), *File);
			return -1;
		}
		++Count;
	}
	UE_LOG(LogFLImport, Display, TEXT("%d Schiffe importiert"), Count);
	return Count;
}

int32 UFLImportCommandlet::Main(const FString& Params)
{
	FString DataDir;
	if (!FParse::Value(*Params, TEXT("-source="), DataDir) || !FPaths::DirectoryExists(DataDir))
	{
		UE_LOG(LogFLImport, Error, TEXT("Aufruf: -run=FLImport -source=<Pfad zu FL/DATA>"));
		return 1;
	}
	return ImportShips(DataDir) < 0 ? 2 : 0;
}
