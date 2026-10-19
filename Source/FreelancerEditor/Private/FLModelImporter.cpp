// Copyright Epic Games, Inc. All Rights Reserved.

#include "FLModelImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/FLModelData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "FlImage.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "FlModel.h"
#include "StaticMeshAttributes.h"
#include "UObject/SavePackage.h"
#include "FlUtf.h"
#include "FlVms.h"

DEFINE_LOG_CATEGORY_STATIC(LogFLModel, Log, All);

namespace
{
bool SaveAsset(UObject* Asset)
{
	UPackage* Package = Asset->GetOutermost();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Package, Asset, *File, Args))
	{
		UE_LOG(LogFLModel, Error, TEXT("Speichern fehlgeschlagen: %s"), *File);
		return false;
	}
	return true;
}

/** Vorhandenes Asset laden oder neu anlegen (wiederholbarer Import). */
template <typename T>
T* LoadOrCreate(const FString& PackageName, const FString& AssetName, bool& bOutExisting)
{
	T* Obj = FPackageName::DoesPackageExist(PackageName) ? LoadObject<T>(nullptr, *(PackageName + TEXT(".") + AssetName)) : nullptr;
	bOutExisting = Obj != nullptr;
	if (!Obj)
	{
		UPackage* Package = CreatePackage(*PackageName);
		Obj = NewObject<T>(Package, *AssetName, RF_Public | RF_Standalone);
	}
	return Obj;
}

FString ToFString(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }
}

FString FFLModelImporter::SanitizeName(const FString& In)
{
	FString Out;
	for (TCHAR C : In)
	{
		Out.AppendChar(FChar::IsAlnum(C) || C == TEXT('_') ? C : TEXT('_'));
	}
	return Out;
}

FString FFLModelImporter::ResolvePath(const FString& Relative) const
{
	TArray<FString> Parts;
	FString Rel = Relative.Replace(TEXT("\\"), TEXT("/"));
	Rel.ParseIntoArray(Parts, TEXT("/"), true);
	FString Current = DataDir;
	for (const FString& Part : Parts)
	{
		// Immer die echte Schreibweise aus dem Verzeichnis nehmen (UE-Dateizugriff ist teils schreibungsunabhaengig, std::ifstream nicht)
		TArray<FString> Entries;
		IFileManager::Get().FindFiles(Entries, *FPaths::Combine(Current, TEXT("*")), true, true);
		const FString* Found = Entries.FindByPredicate([&Part](const FString& E) { return E.Equals(Part, ESearchCase::IgnoreCase); });
		if (!Found)
		{
			return FString();
		}
		Current = FPaths::Combine(Current, *Found);
	}
	return Current;
}

UMaterialInterface* FFLModelImporter::EnsureMasterMaterial()
{
	const FString PackageName = TEXT("/Game/Freelancer/Materials/M_FL_Basic");
	bool bExisting = false;
	UMaterial* Mat = LoadOrCreate<UMaterial>(PackageName, TEXT("M_FL_Basic"), bExisting);
	if (bExisting)
	{
		return Mat;
	}
	auto* Tex = Cast<UMaterialExpressionTextureSampleParameter2D>(UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -600, 0));
	Tex->ParameterName = TEXT("BaseColorTex");
	Tex->SamplerType = SAMPLERTYPE_Color;
	auto* Tint = Cast<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -600, 300));
	Tint->ParameterName = TEXT("Tint");
	Tint->DefaultValue = FLinearColor::White;
	auto* Mul = Cast<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -300, 100));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Tex, TEXT("RGB"), Mul, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Tint, TEXT(""), Mul, TEXT("B"));
	UMaterialEditingLibrary::ConnectMaterialProperty(Mul, TEXT(""), MP_BaseColor);
	UMaterialEditingLibrary::RecompileMaterial(Mat);
	FAssetRegistryModule::AssetCreated(Mat);
	SaveAsset(Mat);
	return Mat;
}

UTexture2D* FFLModelImporter::ImportTexture(const FString& Name, const fl::FlImage& Image)
{
	if (UTexture2D** Cached = TextureCache.Find(Name))
	{
		return *Cached;
	}
	const FString AssetName = TEXT("T_") + SanitizeName(Name);
	bool bExisting = false;
	UTexture2D* Tex = LoadOrCreate<UTexture2D>(TEXT("/Game/Imported/Textures/") + AssetName, AssetName, bExisting);
	TArray<uint8> Bgra;
	Bgra.SetNumUninitialized(Image.Rgba.size());
	for (size_t i = 0; i + 3 < Image.Rgba.size(); i += 4)
	{
		Bgra[i] = Image.Rgba[i + 2]; Bgra[i + 1] = Image.Rgba[i + 1]; Bgra[i + 2] = Image.Rgba[i]; Bgra[i + 3] = Image.Rgba[i + 3];
	}
	Tex->Source.Init(Image.Width, Image.Height, 1, 1, TSF_BGRA8, Bgra.GetData());
	Tex->SRGB = true;
	Tex->CompressionSettings = TC_Default;
	Tex->MipGenSettings = TMGS_FromTextureGroup;
	Tex->PostEditChange();
	if (!bExisting)
	{
		FAssetRegistryModule::AssetCreated(Tex);
	}
	Tex->MarkPackageDirty();
	SaveAsset(Tex);
	TextureCache.Add(Name, Tex);
	return Tex;
}

void FFLModelImporter::LoadMaterialLibrary(const FString& RelativePath)
{
	const FString Path = ResolvePath(RelativePath);
	if (Path.IsEmpty())
	{
		UE_LOG(LogFLModel, Warning, TEXT("Materialbibliothek nicht gefunden: %s"), *RelativePath);
		return;
	}
	fl::UtfFile File = fl::UtfFile::Load(TCHAR_TO_UTF8(*Path));
	if (!File.Root)
	{
		UE_LOG(LogFLModel, Warning, TEXT("%s: %s"), *Path, *ToFString(File.Error));
		return;
	}
	// Texturen nach Name (ohne Endung, klein) vorhalten
	TMap<FString, UTexture2D*> Local;
	for (const fl::FlTexture& T : fl::ReadTextures(*File.Root))
	{
		fl::FlImage Img; std::string Err;
		if (!fl::DecodeImage(T.Data, Img, Err))
		{
			UE_LOG(LogFLModel, Warning, TEXT("Textur %s: %s"), *ToFString(T.Name), *ToFString(Err));
			continue;
		}
		Local.Add(ToFString(T.Name).ToLower(), ImportTexture(ToFString(T.Name), Img));
	}
	UMaterialInterface* Master = EnsureMasterMaterial();
	for (const auto& Pair : fl::ReadMaterials(*File.Root))
	{
		if (MaterialCache.Contains(Pair.first))
		{
			continue;
		}
		const fl::FlMaterial& M = Pair.second;
		FString TexName = ToFString(M.DiffuseTexture);
		const int32 Dot = TexName.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Dot != INDEX_NONE)
		{
			TexName.LeftInline(Dot);
		}
		const FString AssetName = TEXT("MI_") + SanitizeName(ToFString(M.Name));
		bool bExisting = false;
		UMaterialInstanceConstant* MI = LoadOrCreate<UMaterialInstanceConstant>(TEXT("/Game/Imported/Materials/") + AssetName, AssetName, bExisting);
		MI->SetParentEditorOnly(Master);
		if (UTexture2D** Tex = Local.Find(TexName.ToLower()))
		{
			UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MI, TEXT("BaseColorTex"), *Tex);
		}
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MI, TEXT("Tint"), FLinearColor(M.Dc[0], M.Dc[1], M.Dc[2], 1.f));
		UMaterialEditingLibrary::UpdateMaterialInstance(MI);
		if (!bExisting)
		{
			FAssetRegistryModule::AssetCreated(MI);
		}
		MI->MarkPackageDirty();
		SaveAsset(MI);
		MaterialCache.Add(Pair.first, MI);
	}
}

UMaterialInterface* FFLModelImporter::FindMaterial(uint32 Crc) const
{
	UMaterialInterface* const* M = MaterialCache.Find(Crc);
	return M ? *M : nullptr;
}

UStaticMesh* FFLModelImporter::BuildStaticMesh(const FString& Name, const fl::VmsMesh& Mesh)
{
	const FString AssetName = TEXT("SM_") + SanitizeName(Name);
	bool bExisting = false;
	UStaticMesh* SM = LoadOrCreate<UStaticMesh>(TEXT("/Game/Imported/Meshes/") + AssetName, AssetName, bExisting);

	FMeshDescription Desc;
	FStaticMeshAttributes Attr(Desc);
	Attr.Register();
	TVertexAttributesRef<FVector3f> Positions = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> Uvs = Attr.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> Slots = Attr.GetPolygonGroupMaterialSlotNames();
	Uvs.SetNumChannels(1);

	const int32 VertexCount = (int32)(Mesh.Positions.size() / 3);
	TArray<FVertexInstanceID> Instances;
	Instances.Reserve(VertexCount);
	for (int32 V = 0; V < VertexCount; ++V)
	{
		const FVertexID Vid = Desc.CreateVertex();
		float Out[3];
		fl::FlToUePosition(&Mesh.Positions[V * 3], Out);
		Positions[Vid] = FVector3f(Out[0], Out[1], Out[2]);
		const FVertexInstanceID Inst = Desc.CreateVertexInstance(Vid);
		if (!Mesh.Normals.empty())
		{
			fl::FlToUeDirection(&Mesh.Normals[V * 3], Out);
			Normals[Inst] = FVector3f(Out[0], Out[1], Out[2]).GetSafeNormal();
		}
		if (!Mesh.Uv0.empty())
		{
			Uvs.Set(Inst, 0, FVector2f(Mesh.Uv0[V * 2], Mesh.Uv0[V * 2 + 1]));
		}
		Instances.Add(Inst);
	}

	TArray<FStaticMaterial> Materials;
	for (size_t G = 0; G < Mesh.Groups.size(); ++G)
	{
		const fl::VmsGroup& Grp = Mesh.Groups[G];
		const FName Slot(*FString::Printf(TEXT("M%u_%d"), Grp.MaterialCrc, (int32)G));
		const FPolygonGroupID Pg = Desc.CreatePolygonGroup();
		Slots[Pg] = Slot;
		UMaterialInterface* Mat = FindMaterial(Grp.MaterialCrc);
		Materials.Add(FStaticMaterial(Mat, Slot, Slot));
		for (uint32 I = 0; I + 2 < Grp.NumRefIndices; I += 3)
		{
			const uint16 A = Mesh.Indices[Grp.IndexStart + I], B = Mesh.Indices[Grp.IndexStart + I + 1], C = Mesh.Indices[Grp.IndexStart + I + 2];
			// Handedness-Spiegelung: Windung umkehren
			Desc.CreateTriangle(Pg, TArray<FVertexInstanceID>{Instances[A], Instances[C], Instances[B]});
		}
	}
	SM->GetStaticMaterials() = Materials;
	TArray<const FMeshDescription*> Descs;
	Descs.Add(&Desc);
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	SM->BuildFromMeshDescriptions(Descs, Params);
	if (!bExisting)
	{
		FAssetRegistryModule::AssetCreated(SM);
	}
	SM->MarkPackageDirty();
	SaveAsset(SM);
	return SM;
}

UFLModelData* FFLModelImporter::ImportModel(const FString& AssetName, const FString& ArchetypePath, const TArray<FString>& MaterialLibraries)
{
	const FString Path = ResolvePath(ArchetypePath);
	if (Path.IsEmpty())
	{
		UE_LOG(LogFLModel, Warning, TEXT("Modell nicht gefunden: %s"), *ArchetypePath);
		return nullptr;
	}
	fl::UtfFile File = fl::UtfFile::Load(TCHAR_TO_UTF8(*Path));
	fl::FlModel Model; std::string Err;
	if (!File.Root || !fl::BuildModel(*File.Root, Model, Err))
	{
		UE_LOG(LogFLModel, Warning, TEXT("%s: %s"), *Path, *ToFString(File.Root ? Err : File.Error));
		return nullptr;
	}
	// Materialien und Texturen: eigene Bibliothek des Modells, dann die genannten
	{
		fl::UtfFile Own = fl::UtfFile::Load(TCHAR_TO_UTF8(*Path));
		(void)Own;
	}
	for (const FString& Lib : MaterialLibraries)
	{
		LoadMaterialLibrary(Lib);
	}

	const FString DataName = TEXT("DA_Model_") + SanitizeName(AssetName);
	bool bExisting = false;
	UFLModelData* Data = LoadOrCreate<UFLModelData>(TEXT("/Game/Imported/Models/") + DataName, DataName, bExisting);
	Data->Parts.Reset();
	for (const fl::FlModelPart& P : Model.Parts)
	{
		FFLModelPart Out;
		Out.Name = FName(*ToFString(P.Name));
		Out.ParentIndex = P.Parent;
		float Pos[3], Rot[9];
		fl::FlToUePosition(P.Position, Pos);
		fl::FlToUeRotation(P.Rotation, Rot);
		// Zeilenweise 3x3 -> FMatrix (Zeilenvektor-Konvention von Unreal: transponieren)
		FMatrix M(FPlane(Rot[0], Rot[3], Rot[6], 0), FPlane(Rot[1], Rot[4], Rot[7], 0), FPlane(Rot[2], Rot[5], Rot[8], 0), FPlane(0, 0, 0, 1));
		Out.LocalTransform = FTransform(FQuat(M), FVector(Pos[0], Pos[1], Pos[2]));
		Out.Joint = P.Joint == fl::FlConstruct::Rev ? EFLJointType::Revolute : (P.Joint == fl::FlConstruct::Pris ? EFLJointType::Prismatic : EFLJointType::Fixed);
		float Ax[3];
		fl::FlToUeDirection(P.Axis, Ax);
		Out.Axis = FVector(Ax[0], Ax[1], Ax[2]);
		Out.Min = P.Min;
		Out.Max = P.Max;
		for (const fl::FlHardpoint& H : P.Hardpoints)
		{
			FFLHardpoint HP;
			HP.Name = FName(*ToFString(H.Name));
			HP.bRevolute = H.bRevolute;
			float HPos[3], HRot[9];
			fl::FlToUePosition(H.Position, HPos);
			fl::FlToUeRotation(H.Orientation, HRot);
			FMatrix HM(FPlane(HRot[0], HRot[3], HRot[6], 0), FPlane(HRot[1], HRot[4], HRot[7], 0), FPlane(HRot[2], HRot[5], HRot[8], 0), FPlane(0, 0, 0, 1));
			HP.Transform = FTransform(FQuat(HM), FVector(HPos[0], HPos[1], HPos[2]));
			Out.Hardpoints.Add(HP);
		}
		if (P.bHasMesh)
		{
			Out.Mesh = BuildStaticMesh(AssetName + TEXT("_") + ToFString(P.Name), P.Mesh);
		}
		Data->Parts.Add(Out);
	}
	if (!bExisting)
	{
		FAssetRegistryModule::AssetCreated(Data);
	}
	Data->MarkPackageDirty();
	SaveAsset(Data);
	return Data;
}
