// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ships/FLShipVisual.h"

#include "Components/StaticMeshComponent.h"
#include "Data/FLModelData.h"
#include "Engine/StaticMesh.h"

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

void AFLShipVisual::RebuildFromModel()
{
	for (UStaticMeshComponent* C : PartComponents)
	{
		if (C)
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
		Comp->SetStaticMesh(Part.Mesh.LoadSynchronous());
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		USceneComponent* Parent = (Part.ParentIndex >= 0 && Part.ParentIndex < I && PartComponents[Part.ParentIndex]) ? PartComponents[Part.ParentIndex].Get() : GetRootComponent();
		Comp->SetupAttachment(Parent);
		Comp->SetRelativeTransform(Part.LocalTransform);
		Comp->RegisterComponent();
		AddInstanceComponent(Comp);
		PartComponents[I] = Comp;
	}
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
