// Copyright Epic Games, Inc. All Rights Reserved.

#include "FLImportCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/FLEquipmentData.h"
#include "Data/FLGoodData.h"
#include "Data/FLModelData.h"
#include "Data/FLShipData.h"
#include "FLModelImporter.h"
#include "FlRecords.h"
#include "Misc/PackageName.h"
#include "FlShipData.h"
#include "HAL/FileManager.h"
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

namespace
{
/** Legt ein Asset an und speichert es. Gibt false bei Fehler zurueck. */
template <typename T>
bool CreateAndSave(const FString& PackageName, const FString& AssetName, TFunctionRef<void(T*)> Fill)
{
	// Wiederholbarer Import: vorhandenes Asset laden und aktualisieren, sonst neu anlegen
	T* Data = FPackageName::DoesPackageExist(PackageName) ? LoadObject<T>(nullptr, *(PackageName + TEXT(".") + AssetName)) : nullptr;
	UPackage* Package = Data ? Data->GetOutermost() : CreatePackage(*PackageName);
	const bool bExisting = Data != nullptr;
	if (!Data)
	{
		Data = NewObject<T>(Package, *AssetName, RF_Public | RF_Standalone);
	}
	Fill(Data);
	if (!bExisting)
	{
		FAssetRegistryModule::AssetCreated(Data);
	}
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Package, Data, *File, Args))
	{
		UE_LOG(LogFLImport, Error, TEXT("Speichern fehlgeschlagen: %s"), *File);
		return false;
	}
	return true;
}

fl::IniFile LoadIni(const FString& Path)
{
	return fl::IniFile::Load(TCHAR_TO_UTF8(*Path));
}
}

int32 UFLImportCommandlet::ImportShips(const FString& DataDir, const FString& ModelFilter)
{
	FFLModelImporter ModelImporter(DataDir);
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
		UFLModelData* ModelData = nullptr;
		if (!ModelFilter.IsEmpty() && (ModelFilter == TEXT("all") || ModelFilter.Equals(UTF8_TO_TCHAR(R.Nickname.c_str()), ESearchCase::IgnoreCase)))
		{
			TArray<FString> Libs;
			for (const std::string& L : R.MaterialLibraries) { Libs.Add(UTF8_TO_TCHAR(L.c_str())); }
			ModelData = ModelImporter.ImportModel(UTF8_TO_TCHAR(R.Nickname.c_str()), UTF8_TO_TCHAR(R.Archetype.c_str()), Libs);
		}
		if (!CreateAndSave<UFLShipData>(TEXT("/Game/Imported/Ships/") + Name, Name, [&R, ModelData](UFLShipData* D) { FillShipData(D, R); if (ModelData) { D->Model = ModelData; } }))
		{
			return -1;
		}
		++Count;
	}
	UE_LOG(LogFLImport, Display, TEXT("%d Schiffe importiert"), Count);
	return Count;
}

void UFLImportCommandlet::FillProperties(TMap<FName, FString>& Out, const fl::FlRecord& R)
{
	Out.Reset();
	TMap<FName, int32> Seen;
	for (const auto& P : R.Props)
	{
		const FName Base(UTF8_TO_TCHAR(P.first.c_str()));
		int32& N = Seen.FindOrAdd(Base);
		++N;
		const FName Key = (N == 1) ? Base : FName(*FString::Printf(TEXT("%s#%d"), *Base.ToString(), N));
		Out.Add(Key, UTF8_TO_TCHAR(P.second.c_str()));
	}
}

void UFLImportCommandlet::FillEquipmentData(UFLEquipmentData* Data, const fl::FlRecord& R)
{
	auto Num = [&R](const char* Key) { const std::string* V = R.Find(Key); return V ? FCString::Atof(UTF8_TO_TCHAR(V->c_str())) : 0.f; };
	Data->Nickname = FName(UTF8_TO_TCHAR(R.Nickname.c_str()));
	Data->EquipType = FName(UTF8_TO_TCHAR(R.Section.c_str()));
	Data->IdsName = (int32)Num("ids_name");
	Data->IdsInfo = (int32)Num("ids_info");
	Data->Mass = Num("mass");
	Data->Volume = Num("volume");
	Data->HitPoints = (int32)Num("hit_pts");
	const std::string* Arch = R.Find("da_archetype");
	Data->SourceArchetype = Arch ? UTF8_TO_TCHAR(fl::NormalizePath(*Arch).c_str()) : TEXT("");
	Data->SourceMaterialLibraries.Reset();
	for (const std::string& M : R.FindAll("material_library"))
	{
		Data->SourceMaterialLibraries.Add(UTF8_TO_TCHAR(fl::NormalizePath(M).c_str()));
	}
	FillProperties(Data->Properties, R);
}

void UFLImportCommandlet::FillGoodData(UFLGoodData* Data, const fl::FlRecord& R)
{
	auto Str = [&R](const char* Key) { const std::string* V = R.Find(Key); return V ? FString(UTF8_TO_TCHAR(V->c_str())) : FString(); };
	Data->Nickname = FName(UTF8_TO_TCHAR(R.Nickname.c_str()));
	Data->Category = FName(*Str("category"));
	Data->Price = FCString::Atoi64(*Str("price"));
	Data->bCombinable = Str("combinable").Equals(TEXT("true"), ESearchCase::IgnoreCase);
	// Verweis: equipment=, ship= oder hull= je nach Kategorie
	FString Ref = Str("equipment");
	if (Ref.IsEmpty()) Ref = Str("ship");
	if (Ref.IsEmpty()) Ref = Str("hull");
	Data->Reference = FName(*Ref);
	FillProperties(Data->Properties, R);
}

int32 UFLImportCommandlet::ImportEquipment(const FString& DataDir)
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(DataDir, TEXT("EQUIPMENT"), TEXT("*_equip.ini")), true, false);
	Files.Sort();
	int32 Count = 0;
	TSet<FString> Names;
	for (const FString& F : Files)
	{
		const FString Path = FPaths::Combine(DataDir, TEXT("EQUIPMENT"), F);
		fl::IniFile Ini = LoadIni(Path);
		if (!Ini.bOk)
		{
			UE_LOG(LogFLImport, Error, TEXT("%s: %s"), *Path, UTF8_TO_TCHAR(Ini.Error.c_str()));
			return -1;
		}
		for (const fl::FlRecord& R : fl::ParseRecords(Ini, fl::NonEquipmentSections()))
		{
			FString Name = FString::Printf(TEXT("DA_Equip_%s"), UTF8_TO_TCHAR(R.Nickname.c_str()));
			if (Names.Contains(Name.ToLower()))
			{
				Name += FString::Printf(TEXT("_%s"), UTF8_TO_TCHAR(R.Section.c_str())); // gleicher Nickname in anderem Sektionstyp
			}
			Names.Add(Name.ToLower());
			if (!CreateAndSave<UFLEquipmentData>(TEXT("/Game/Imported/Equipment/") + Name, Name, [&R](UFLEquipmentData* D) { FillEquipmentData(D, R); }))
			{
				return -1;
			}
			++Count;
		}
	}
	UE_LOG(LogFLImport, Display, TEXT("%d Ausruestungsobjekte importiert"), Count);
	return Count;
}

int32 UFLImportCommandlet::ImportGoods(const FString& DataDir)
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(DataDir, TEXT("EQUIPMENT"), TEXT("*good*.ini")), true, false);
	Files.Sort();
	int32 Count = 0;
	TSet<FString> Names;
	for (const FString& F : Files)
	{
		fl::IniFile Ini = LoadIni(FPaths::Combine(DataDir, TEXT("EQUIPMENT"), F));
		if (!Ini.bOk)
		{
			UE_LOG(LogFLImport, Error, TEXT("%s: %s"), *F, UTF8_TO_TCHAR(Ini.Error.c_str()));
			return -1;
		}
		for (const fl::FlRecord& R : fl::ParseRecordsOfType(Ini, "Good"))
		{
			const FString Name = FString::Printf(TEXT("DA_Good_%s"), UTF8_TO_TCHAR(R.Nickname.c_str()));
			if (Names.Contains(Name.ToLower()))
			{
				continue; // erste Definition gewinnt
			}
			Names.Add(Name.ToLower());
			if (!CreateAndSave<UFLGoodData>(TEXT("/Game/Imported/Goods/") + Name, Name, [&R](UFLGoodData* D) { FillGoodData(D, R); }))
			{
				return -1;
			}
			++Count;
		}
	}
	UE_LOG(LogFLImport, Display, TEXT("%d Waren importiert"), Count);
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
	FString ModelFilter;
	FParse::Value(*Params, TEXT("-models="), ModelFilter); // "all" oder Schiff-Nickname
	if (ImportShips(DataDir, ModelFilter) < 0 || ImportEquipment(DataDir) < 0 || ImportGoods(DataDir) < 0)
	{
		return 2;
	}
	return 0;
}
