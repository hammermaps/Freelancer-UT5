// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLShip.h"

#include "Camera/CameraComponent.h"
#include "Data/FLEquipmentData.h"
#include "Data/FLShipData.h"
#include "Engine/World.h"
#include "Flight/FLShipMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"
#include "Ships/FLShipVisual.h"

DEFINE_LOG_CATEGORY_STATIC(LogFLShip, Log, All);

static TAutoConsoleVariable<int32> CVarBotFlight(TEXT("fl.BotFlight"), 0, TEXT("1 = besitzende Schiffe fliegen ein Testmuster (fuer Netzwerktests)"));

AFLShip::AFLShip()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false); // eigene Bewegungsreplikation (Vorhersage/Abgleich)
	NetUpdateFrequency = 30.f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Movement = CreateDefaultSubobject<UFLShipMovementComponent>(TEXT("Movement"));
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 1800.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 300.f);
	SpringArm->bDoCollisionTest = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
}

void AFLShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFLShip, RepState, COND_SimulatedOnly);
}

void AFLShip::ApplyShipData()
{
	const UFLShipData* Ship = ShipData.LoadSynchronous();
	const UFLEquipmentData* Eng = Engine.LoadSynchronous();
	if (Ship)
	{
		Movement->Params = FLFlight::MakeParams(Ship, Eng);
	}
}

void AFLShip::SpawnVisual()
{
	const UFLShipData* Ship = ShipData.LoadSynchronous();
	if (!Ship || Ship->Model.IsNull() || !GetWorld())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Visual = GetWorld()->SpawnActor<AFLShipVisual>(AFLShipVisual::StaticClass(), GetActorTransform(), Params);
	if (Visual)
	{
		Visual->Model = Ship->Model;
		Visual->RebuildFromModel();
		Visual->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
		Visual->SetReplicates(false);
	}
}

void AFLShip::BeginPlay()
{
	Super::BeginPlay();
	ApplyShipData();
	SpawnVisual();
	FFLFlightState Start;
	Start.Position = GetActorLocation();
	Start.Rotation = GetActorQuat();
	Movement->ResetState(Start);
	if (IsLocallyControlled() || HasAuthority())
	{
		// Besitzer sendet Bewegungen als RPC an den Server
		Movement->OnSendMoves.BindUObject(this, &AFLShip::ServerMoves);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("FLBot")))
	{
		bBot = true;
	}
}

void AFLShip::SetFlightInput(const FFLFlightInput& Input)
{
	Movement->SetInput(Input);
}

bool AFLShip::ServerMoves_Validate(const TArray<FFLMove>& Moves)
{
	// Nur plausible Werte; alles andere trennt die Verbindung (Manipulation)
	if (Moves.Num() > 32)
	{
		return false;
	}
	for (const FFLMove& Move : Moves)
	{
		if (!(FMath::IsFinite(Move.Dt) && Move.Dt >= 0.f && Move.Dt <= 1.f
			&& FMath::IsFinite(Move.Input.Throttle) && FMath::IsFinite(Move.Input.Pitch) && FMath::IsFinite(Move.Input.Yaw)
			&& FMath::IsFinite(Move.Input.Roll) && FMath::IsFinite(Move.Input.StrafeRight) && FMath::IsFinite(Move.Input.StrafeUp)))
		{
			return false;
		}
	}
	return true;
}

void AFLShip::ServerMoves_Implementation(const TArray<FFLMove>& Moves)
{
	for (const FFLMove& Move : Moves)
	{
		Movement->ServerApplyMove(Move); // bereits verarbeitete Sequenzen werden ignoriert
	}
}

void AFLShip::ClientAck_Implementation(uint32 AckedSequence, const FFLFlightState& ServerState)
{
	Movement->ClientReconcile(AckedSequence, ServerState);
}

void AFLShip::OnRep_RepState()
{
	Movement->SetRemoteTarget(RepState);
}

void AFLShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if ((bBot || CVarBotFlight.GetValueOnGameThread() != 0) && (IsLocallyControlled() || GetLocalRole() == ROLE_AutonomousProxy))
	{
		// Testmuster: Vollschub, wechselnde Kurven (Gieren/Nicken/Rollen), damit Vorhersage und Abgleich arbeiten muessen
		BotTime += DeltaSeconds;
		FFLFlightInput In;
		In.Throttle = 1.f;
		In.Yaw = FMath::Sin(BotTime * 0.8f);
		In.Pitch = 0.5f * FMath::Sin(BotTime * 0.5f + 1.f);
		In.Roll = 0.3f * FMath::Cos(BotTime * 0.7f);
		Movement->SetInput(In);
	}

	if (HasAuthority())
	{
		// Server: Zustand fuer fremde Clients replizieren und dem Besitzer in ~30 Hz bestaetigen
		RepState = Movement->GetState();
		AckAccumulator += DeltaSeconds;
		if (AckAccumulator >= 1.f / 30.f && GetNetMode() != NM_Standalone && GetRemoteRole() == ROLE_AutonomousProxy)
		{
			AckAccumulator = 0.f;
			ClientAck(Movement->GetLastProcessedSequence(), Movement->GetState());
		}
	}

	// Kurze Statistik fuer die Abnahme der Netzwerktests
	LogAccumulator += DeltaSeconds;
	if (LogAccumulator >= 5.f)
	{
		LogAccumulator = 0.f;
		const FFLFlightState& S = Movement->GetState();
		UE_LOG(LogFLShip, Display, TEXT("FLShip %s Rolle=%d: Pos=(%.0f, %.0f, %.0f) Tempo=%.0f cm/s Korrekturen=%d MaxFehler=%.1f cm Moves=%d Abgelehnt=%d Zeit(Spiel)=%.1f s gutgeschrieben=%.1f s verbraucht=%.1f s Wanduhr=%.1f s dt=%.4f Frames=%llu Benchmark=%d FesterSchritt=%d"),
			*GetName(), (int32)GetLocalRole(), S.Position.X, S.Position.Y, S.Position.Z, S.Velocity.Size(),
			Movement->GetCorrectionCount(), Movement->GetMaxPositionError(), Movement->GetAppliedMoves(), Movement->GetTrimmedMoves(), GetWorld()->GetTimeSeconds(), Movement->GetBudgetAccrued(), Movement->GetBudgetConsumed(), FPlatformTime::Seconds() - GStartTime, DeltaSeconds, (unsigned long long)GFrameCounter, FApp::IsBenchmarking() ? 1 : 0, FApp::UseFixedTimeStep() ? 1 : 0);
	}
}
