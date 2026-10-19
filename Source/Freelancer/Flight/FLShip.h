// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Flight/FLFlightModel.h"
#include "Flight/FLPrediction.h"
#include "GameFramework/Pawn.h"
#include "FLShip.generated.h"

class UFLShipData;
class UFLEquipmentData;
class UFLShipMovementComponent;
class AFLShipVisual;
class USpringArmComponent;
class UCameraComponent;

/** Spielbares Schiff: Bewegung (server-autoritativ mit Client-Vorhersage) + sichtbare Huelle aus dem Import. */
UCLASS()
class FREELANCER_API AFLShip : public APawn
{
	GENERATED_BODY()

public:
	AFLShip();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TSoftObjectPtr<UFLShipData> ShipData;

	/** Triebwerk; leer = Standardwerte des Flugmodells */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TSoftObjectPtr<UFLEquipmentData> Engine;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UFLShipMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UCameraComponent> Camera;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	void SetFlightInput(const FFLFlightInput& Input);

	/** Test-Bot: fliegt ohne Spielereingabe ein Kurvenmuster (Konsolenvariable fl.BotFlight 1 oder -FLBot). */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void SetBotEnabled(bool bEnabled) { bBot = bEnabled; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Client -> Server: die letzten unbestaetigten Bewegungen (aelteste zuerst). Wird validiert; ungueltige Werte trennen die Verbindung. */
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerMoves(const TArray<FFLMove>& Moves);

	/** Server -> besitzender Client: bestaetigter Zustand nach Verarbeitung bis Sequenz. */
	UFUNCTION(Client, Unreliable)
	void ClientAck(uint32 AckedSequence, const FFLFlightState& ServerState);

protected:
	/** Fuer fremde Clients: vom Server replizierter Zustand */
	UPROPERTY(ReplicatedUsing = OnRep_RepState)
	FFLFlightState RepState;

	UFUNCTION()
	void OnRep_RepState();

private:
	UPROPERTY(Transient)
	TObjectPtr<AFLShipVisual> Visual;

	bool bBot = false;
	float BotTime = 0.f;
	float AckAccumulator = 0.f;
	float LogAccumulator = 0.f;

	void ApplyShipData();
	void SpawnVisual();
};
