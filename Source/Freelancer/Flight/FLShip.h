// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Flight/FLFlightInput.h"
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

	/** Wird beim Spawn auf dem Server gesetzt und repliziert (Client braucht Daten fuer Huelle und identische Flugparameter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship")
	TSoftObjectPtr<UFLShipData> ShipData;

	/** Triebwerk; leer = Standardwerte des Flugmodells */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship")
	TSoftObjectPtr<UFLEquipmentData> Engine;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UFLShipMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UCameraComponent> Camera;

	/** Trefferpunkte (hit_pts aus den Schiffsdaten); werden repliziert. */
	UPROPERTY(ReplicatedUsing = OnRep_Hull, VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	float Hull = 0.f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	float MaxHull = 0.f;

	/** Zerstoert: unsichtbar und ohne Kollision bis zum Neustart am Startpunkt */
	UPROPERTY(ReplicatedUsing = OnRep_Dead, VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	bool bDead = false;

	/** Sekunden bis zum Neustart nach der Zerstoerung */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	float RespawnDelay = 3.f;

	/** Server: Schaden anwenden; bei 0 Trefferpunkten wird das Schiff zerstoert. Gibt den abgezogenen Schaden zurueck. */
	double ApplyDamage(double Damage);

	/** Kollisionsradius in cm (aus dem Modell berechnet) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	float CollisionRadius = 500.f;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	void SetFlightInput(const FFLFlightInput& Input);

	/** Test-Bot: fliegt ohne Spielereingabe ein Kurvenmuster (Konsolenvariable fl.BotFlight 1 oder -FLBot). */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void SetBotEnabled(bool bEnabled) { bBot = bEnabled; }

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Client -> Server: die letzten unbestaetigten Bewegungen (aelteste zuerst). Wird validiert; ungueltige Werte trennen die Verbindung. */
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerMoves(const TArray<FFLMove>& Moves);

	/** Server -> besitzender Client: bestaetigter Zustand nach Verarbeitung bis Sequenz. */
	/** Server -> Besitzer: Neustart (Zustand setzen, Verlauf verwerfen). */
	UFUNCTION(Client, Reliable)
	void ClientTeleport(const FFLFlightState& NewState);

	UFUNCTION(Client, Unreliable)
	void ClientAck(uint32 AckedSequence, const FFLFlightState& ServerState);

protected:
	/** Fuer fremde Clients: vom Server replizierter Zustand */
	UPROPERTY(ReplicatedUsing = OnRep_RepState)
	FFLFlightState RepState;

	UFUNCTION()
	void OnRep_RepState();

	UFUNCTION()
	void OnRep_Hull() {}

	UFUNCTION()
	void OnRep_Dead();

private:
	UPROPERTY(Transient)
	TObjectPtr<AFLShipVisual> Visual;

	FFLInputActions Actions;
	FFLRawInput Raw;
	FFLInputSettings InputSettings;

	void OnThrottle(const struct FInputActionValue& V);
	void OnStrafeRight(const struct FInputActionValue& V);
	void OnStrafeUp(const struct FInputActionValue& V);
	void OnRoll(const struct FInputActionValue& V);
	void OnLook(const struct FInputActionValue& V);
	void OnCruiseToggle(const struct FInputActionValue& V);

	bool bShotScheduled = false;
	bool bBot = false;
	float BotTime = 0.f;
	float AckAccumulator = 0.f;
	float LogAccumulator = 0.f;

	void ApplyShipData();
	void HandleCollisions();
	void Die();
	void Respawn();
	FFLFlightState SpawnState;
	FTimerHandle RespawnTimer;
	void SpawnVisual();
};
