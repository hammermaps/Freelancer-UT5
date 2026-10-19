// Copyright Epic Games, Inc. All Rights Reserved.

#include "FLImportCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/FLEquipmentData.h"
#include "Data/FLGoodData.h"
#include "Data/FLModelData.h"
#include "Data/FLShipData.h"
#include "Data/FLSystemData.h"
#include "FlMath.h"
#include "FlModel.h"
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

namespace
{
FVector ToUePos(const std::vector<float>& V)
{
	if (V.size() < 3) return FVector::ZeroVector;
	const float In[3] = {V[0], V[1], V[2]};
	float Out[3];
	fl::FlToUePosition(In, Out);
	return FVector(Out[0], Out[1], Out[2]);
}

FQuat ToUeRotation(const std::vector<float>& Euler)
{
	if (Euler.size() < 3) return FQuat::Identity;
	const float E[3] = {Euler[0], Euler[1], Euler[2]};
	float R[9], U[9];
	fl::FlEulerToRotation(E, R);
	fl::FlToUeRotation(R, U);
	return FQuat(FMatrix(FPlane(U[0], U[3], U[6], 0), FPlane(U[1], U[4], U[7], 0), FPlane(U[2], U[5], U[8], 0), FPlane(0, 0, 0, 1)));
}

EFLZoneShape ToShape(const FString& S)
{
	if (S.Equals(TEXT("SPHERE"), ESearchCase::IgnoreCase)) return EFLZoneShape::Sphere;
	if (S.Equals(TEXT("ELLIPSOID"), ESearchCase::IgnoreCase)) return EFLZoneShape::Ellipsoid;
	if (S.Equals(TEXT("BOX"), ESearchCase::IgnoreCase)) return EFLZoneShape::Box;
	if (S.Equals(TEXT("CYLINDER"), ESearchCase::IgnoreCase)) return EFLZoneShape::Cylinder;
	if (S.Equals(TEXT("RING"), ESearchCase::IgnoreCase)) return EFLZoneShape::Ring;
	return EFLZoneShape::Unknown;
}

FString Prop(const fl::FlRecord& R, const char* Key)
{
	const std::string* V = R.Find(Key);
	return V ? FString(UTF8_TO_TCHAR(V->c_str())) : FString();
}
}

void UFLImportCommandlet::FillSystemData(UFLSystemData* Data, const fl::FlRecord& U, const std::vector<fl::FlRecord>& Objects,
	const std::vector<fl::FlRecord>& Zones, const fl::FlRecord* Info, const TArray<FName>& Bases)
{
	Data->Nickname = FName(UTF8_TO_TCHAR(U.Nickname.c_str()));
	Data->IdsName = FCString::Atoi(*Prop(U, "strid_name"));
	const std::vector<float> Map = fl::ParseFloats(U.Find("pos") ? *U.Find("pos") : std::string());
	Data->MapPosition = Map.size() >= 2 ? FVector2D(Map[0], Map[1]) : FVector2D::ZeroVector;
	FillProperties(Data->Properties, U);
	Data->Bases = Bases;
	Data->Info.Reset();
	if (Info)
	{
		FillProperties(Data->Info, *Info);
	}

	Data->Objects.Reset();
	for (const fl::FlRecord& O : Objects)
	{
		FFLSystemObject Out;
		Out.Nickname = FName(UTF8_TO_TCHAR(O.Nickname.c_str()));
		Out.Archetype = FName(*Prop(O, "archetype"));
		Out.IdsName = FCString::Atoi(*Prop(O, "ids_name"));
		Out.IdsInfo = FCString::Atoi(*Prop(O, "ids_info"));
		Out.Position = ToUePos(fl::ParseFloats(O.Find("pos") ? *O.Find("pos") : std::string()));
		Out.Rotation = ToUeRotation(fl::ParseFloats(O.Find("rotate") ? *O.Find("rotate") : std::string()));
		Out.Base = FName(*Prop(O, "base"));
		Out.Reputation = FName(*Prop(O, "reputation"));
		Out.Loadout = FName(*Prop(O, "loadout"));
		if (const std::string* Goto = O.Find("goto"))
		{
			TArray<FString> Parts;
			FString(UTF8_TO_TCHAR(Goto->c_str())).ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() >= 2)
			{
				Out.GotoSystem = FName(*Parts[0].TrimStartAndEnd());
				Out.GotoObject = FName(*Parts[1].TrimStartAndEnd());
			}
		}
		FillProperties(Out.Properties, O);
		Data->Objects.Add(MoveTemp(Out));
	}

	Data->Zones.Reset();
	for (const fl::FlRecord& Z : Zones)
	{
		FFLSystemZone Out;
		Out.Nickname = FName(UTF8_TO_TCHAR(Z.Nickname.c_str()));
		Out.Shape = ToShape(Prop(Z, "shape"));
		Out.Position = ToUePos(fl::ParseFloats(Z.Find("pos") ? *Z.Find("pos") : std::string()));
		Out.Rotation = ToUeRotation(fl::ParseFloats(Z.Find("rotate") ? *Z.Find("rotate") : std::string()));
		std::vector<float> S = fl::ParseFloats(Z.Find("size") ? *Z.Find("size") : std::string());
		if (S.size() == 1) { S = {S[0], S[0], S[0]}; }
		if (S.size() >= 3)
		{
			// Groesse in Originalachsen (x, y, z) -> Unreal-Achsen, Meter -> cm
			Out.Size = FVector(FMath::Abs(S[2]), FMath::Abs(S[0]), FMath::Abs(S[1])) * 100.0;
		}
		FillProperties(Out.Properties, Z);
		Data->Zones.Add(MoveTemp(Out));
	}
}

int32 UFLImportCommandlet::ImportUniverse(const FString& DataDir)
{
	FFLModelImporter Paths(DataDir); // nur fuer ResolvePath
	const FString UniversePath = Paths.ResolvePath(TEXT("UNIVERSE/universe.ini"));
	fl::IniFile Universe = LoadIni(UniversePath);
	if (!Universe.bOk)
	{
		UE_LOG(LogFLImport, Error, TEXT("universe.ini: %s"), UTF8_TO_TCHAR(Universe.Error.c_str()));
		return -1;
	}
	// Basen je System sammeln
	TMap<FString, TArray<FName>> BasesBySystem;
	for (const fl::FlRecord& B : fl::ParseRecordsOfType(Universe, "Base"))
	{
		BasesBySystem.FindOrAdd(Prop(B, "system").ToLower()).Add(FName(UTF8_TO_TCHAR(B.Nickname.c_str())));
	}

	int32 Count = 0;
	for (const fl::FlRecord& S : fl::ParseRecordsOfType(Universe, "system"))
	{
		const FString File = Prop(S, "file");
		const FString Path = Paths.ResolvePath(TEXT("UNIVERSE/") + File.Replace(TEXT("\\"), TEXT("/")));
		std::vector<fl::FlRecord> Objects, Zones;
		fl::FlRecord Info;
		bool bHasInfo = false;
		if (Path.IsEmpty())
		{
			UE_LOG(LogFLImport, Warning, TEXT("System-Datei nicht gefunden: %s"), *File);
		}
		else
		{
			fl::IniFile SysIni = LoadIni(Path);
			if (!SysIni.bOk)
			{
				UE_LOG(LogFLImport, Warning, TEXT("%s: %s"), *Path, UTF8_TO_TCHAR(SysIni.Error.c_str()));
				continue;
			}
			for (const fl::FlRecord& R : fl::ParseRecordsOfType(SysIni, "Object")) { Objects.push_back(R); }
			for (const fl::FlRecord& R : fl::ParseRecordsOfType(SysIni, "zone")) { Zones.push_back(R); }
			// SystemInfo hat einen nickname, Eintraege der Sektion werden als Info uebernommen
			for (const fl::IniSection* Sec : SysIni.FindAll("SystemInfo"))
			{
				fl::FlRecord Rec;
				Rec.Section = Sec->Name;
				Rec.Nickname = S.Nickname;
				for (const fl::IniEntry& E : Sec->Entries)
				{
					std::string J;
					for (size_t i = 0; i < E.Values.size(); ++i) { if (i) J += ", "; J += E.Values[i]; }
					std::string K = E.Key;
					for (char& Ch : K) Ch = (char)tolower((unsigned char)Ch);
					Rec.Props.emplace_back(K, J);
				}
				Info = Rec;
				bHasInfo = true;
			}
		}
		const FString Name = FString::Printf(TEXT("DA_System_%s"), UTF8_TO_TCHAR(S.Nickname.c_str()));
		TArray<FName> Bases = BasesBySystem.FindRef(UTF8_TO_TCHAR(S.Nickname.c_str()));
		Bases.Sort(FNameLexicalLess());
		if (!CreateAndSave<UFLSystemData>(TEXT("/Game/Imported/Systems/") + Name, Name,
			[&](UFLSystemData* D) { FillSystemData(D, S, Objects, Zones, bHasInfo ? &Info : nullptr, Bases); }))
		{
			return -1;
		}
		++Count;
	}
	UE_LOG(LogFLImport, Display, TEXT("%d Systeme importiert"), Count);
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
	if (ImportShips(DataDir, ModelFilter) < 0 || ImportUniverse(DataDir) < 0 || ImportEquipment(DataDir) < 0 || ImportGoods(DataDir) < 0)
	{
		return 2;
	}
	return 0;
}
