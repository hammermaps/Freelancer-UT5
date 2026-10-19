// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UFLModelData;
class UStaticMesh;
class UTexture2D;
class UMaterialInterface;
namespace fl { struct FlModel; struct FlMaterial; struct FlImage; struct VmsMesh; }

/** Importiert Modelle (Texturen, Materialien, Static Meshes, Modelldaten) aus den Freelancer-Originaldateien. */
class FFLModelImporter
{
public:
	explicit FFLModelImporter(const FString& InDataDir) : DataDir(InDataDir) {}

	/** Sucht eine Datei relativ zu DATA ohne Beachtung der Gross-/Kleinschreibung (Windows-Pfade im Original). */
	FString ResolvePath(const FString& Relative) const;

	/** Importiert Modell + Materialien; liefert das Modell-Asset oder nullptr. AssetName ohne Praefix. */
	UFLModelData* ImportModel(const FString& AssetName, const FString& ArchetypePath, const TArray<FString>& MaterialLibraries);

	/** Sauberer Name fuer Assets (nur A-Z a-z 0-9 _) */
	static FString SanitizeName(const FString& In);

	/** Erzeugt (einmalig) das Master-Material M_FL_Basic mit Parametern BaseColorTex und Tint. */
	static UMaterialInterface* EnsureMasterMaterial();

private:
	FString DataDir;
	TMap<uint32, UMaterialInterface*> MaterialCache; // CRC -> Material-Instanz
	TMap<FString, UTexture2D*> TextureCache;

	void LoadMaterialLibrary(const FString& RelativePath);
	UTexture2D* ImportTexture(const FString& Name, const fl::FlImage& Image);
	UMaterialInterface* FindMaterial(uint32 Crc) const;
	UStaticMesh* BuildStaticMesh(const FString& Name, const fl::VmsMesh& Mesh);
};
