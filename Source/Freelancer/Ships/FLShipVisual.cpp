// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ships/FLShipVisual.h"

#include "Components/StaticMeshComponent.h"
#include "Data/FLModelData.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogFLShipVisual, Log, All);

AFLShipVisual::AFLShipVisual()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AFLShipVisual::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildFromModel();
}

void AFLShipVisual::BeginPlay()
{
	Super::BeginPlay();
	// Im gekochten Spiel laufen Konstruktionsskripte platzierter Actors nicht immer vor BeginPlay: Teile hier sicherstellen
	if (PartComponents.IsEmpty())
	{
		RebuildFromModel();
	}
	UE_LOG(LogFLShipVisual, Display, TEXT("FLShipVisual BeginPlay %s: Modell gesetzt=%d, Teile=%d, Netzrolle=%d"),
		*GetName(), !Model.IsNull(), PartComponents.Num(), (int32)GetLocalRole());
}

void AFLShipVisual::RebuildFromModel()
{
	// Vorhandene Teilkomponenten entfernen (auch solche, die ein aelterer Editor-Stand im Level gespeichert hat)
	TArray<UStaticMeshComponent*> Existing;
	GetComponents<UStaticMeshComponent>(Existing);
	for (UStaticMeshComponent* C : Existing)
	{
		if (C && C->GetName().StartsWith(TEXT("Part_")))
		{
			C->DestroyComponent();
		}
	}
	PartComponents.Reset();

	const UFLModelData* Data = Model.LoadSynchronous();
	if (!Data)
	{
		return;
	}
	PartComponents.SetNum(Data->Parts.Num());
	for (int32 I = 0; I < Data->Parts.Num(); ++I)
	{
		const FFLModelPart& Part = Data->Parts[I];
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, FName(*FString::Printf(TEXT("Part_%s"), *Part.Name.ToString())));
		Comp->SetFlags(RF_Transient); // aus den Daten abgeleitet: nicht im Level speichern (sonst Namenskollision im gekochten Spiel)
		Comp->SetStaticMesh(Part.Mesh.LoadSynchronous());
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		USceneComponent* Parent = (Part.ParentIndex >= 0 && Part.ParentIndex < I && PartComponents[Part.ParentIndex]) ? PartComponents[Part.ParentIndex].Get() : GetRootComponent();
		Comp->SetupAttachment(Parent);
		Comp->SetRelativeTransform(Part.LocalTransform);
		Comp->RegisterComponent();
		AddInstanceComponent(Comp);
		PartComponents[I] = Comp;
	}

	int32 Meshes = 0, Hardpoints = 0;
	for (const FFLModelPart& P : Data->Parts)
	{
		Meshes += P.Mesh.IsNull() ? 0 : 1;
		Hardpoints += P.Hardpoints.Num();
	}
	// Stabile Zeile fuer die Abnahme: Server und Client muessen dieselben Zahlen melden
	UE_LOG(LogFLShipVisual, Display, TEXT("FLShipVisual %s: Modell %s, %d Teile, %d Meshes, %d Hardpoints"),
		*GetName(), *Data->GetName(), Data->Parts.Num(), Meshes, Hardpoints);
}

UStaticMeshComponent* AFLShipVisual::FindPart(FName PartName) const
{
	const UFLModelData* Data = Model.Get();
	if (!Data)
	{
		return nullptr;
	}
	for (int32 I = 0; I < Data->Parts.Num() && I < PartComponents.Num(); ++I)
	{
		if (Data->Parts[I].Name == PartName)
		{
			return PartComponents[I];
		}
	}
	return nullptr;
}

bool AFLShipVisual::GetHardpointTransform(FName HardpointName, FTransform& OutWorld) const
{
	const UFLModelData* Data = Model.Get();
	if (!Data)
	{
		return false;
	}
	int32 PartIndex = -1;
	const int32 H = Data->FindHardpoint(HardpointName, PartIndex);
	if (H < 0 || PartIndex >= PartComponents.Num() || !PartComponents[PartIndex])
	{
		return false;
	}
	OutWorld = Data->Parts[PartIndex].Hardpoints[H].Transform * PartComponents[PartIndex]->GetComponentTransform();
	return true;
}
