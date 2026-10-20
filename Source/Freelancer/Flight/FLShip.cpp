// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLShip.h"

#include "Camera/CameraComponent.h"
#include "Data/FLEquipmentData.h"
#include "Data/FLModelData.h"
#include "Data/FLShipData.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "EngineUtils.h"
#include "Flight/FLCollision.h"
#include "Flight/FLShipMovementComponent.h"
#include "Flight/FLStaticCollision.h"
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
	// Relevanz: Schiffe jenseits von 10 km (Sichtweite im Original begrenzt) werden nicht zu diesem Client repliziert
	NetCullDistanceSquared = 1.0e12; // (10 km = 1.000.000 cm)^2
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
	DOREPLIFETIME_CONDITION(AFLShip, ShipData, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AFLShip, Engine, COND_InitialOnly);
}

void AFLShip::ApplyShipData()
{
	const UFLShipData* Ship = ShipData.LoadSynchronous();
	const UFLEquipmentData* Eng = Engine.LoadSynchronous();
	if (Ship)
	{
		Movement->Params = FLFlight::MakeParams(Ship, Eng);
		CollisionRadius = (float)FLCollision::ModelRadius(Ship->Model.LoadSynchronous(), CollisionRadius);
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
	// Saubere Transformation (Skalierung 1): ein replizierter Pawn kann auf dem Client zunaechst eine Skalierung von 0 haben,
	// das ergibt unsichtbare Huellen und ungueltige Matrizen im Renderer.
	Visual = GetWorld()->SpawnActor<AFLShipVisual>(AFLShipVisual::StaticClass(), FTransform(GetActorRotation(), GetActorLocation(), FVector::OneVector), Params);
	if (Visual)
	{
		Visual->Model = Ship->Model;
		Visual->RebuildFromModel();
		Visual->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Visual->SetReplicates(false);
	}
}

void AFLShip::BeginPlay()
{
	Super::BeginPlay();
	if (GetActorScale3D().ContainsNaN() || GetActorScale3D().GetMin() <= KINDA_SMALL_NUMBER)
	{
		SetActorScale3D(FVector::OneVector);
	}
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

void AFLShip::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!Actions.Mapping)
	{
		Actions = FLInput::CreateDefaultActions(this);
	}
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Axis-Aktionen liefern jeden Frame Werte, solange Tasten gehalten werden; bei Loslassen (Completed) auf 0 setzen
		EIC->BindAction(Actions.Throttle, ETriggerEvent::Triggered, this, &AFLShip::OnThrottle);
		EIC->BindAction(Actions.Throttle, ETriggerEvent::Completed, this, &AFLShip::OnThrottle);
		EIC->BindAction(Actions.StrafeRight, ETriggerEvent::Triggered, this, &AFLShip::OnStrafeRight);
		EIC->BindAction(Actions.StrafeRight, ETriggerEvent::Completed, this, &AFLShip::OnStrafeRight);
		EIC->BindAction(Actions.StrafeUp, ETriggerEvent::Triggered, this, &AFLShip::OnStrafeUp);
		EIC->BindAction(Actions.StrafeUp, ETriggerEvent::Completed, this, &AFLShip::OnStrafeUp);
		EIC->BindAction(Actions.Roll, ETriggerEvent::Triggered, this, &AFLShip::OnRoll);
		EIC->BindAction(Actions.Roll, ETriggerEvent::Completed, this, &AFLShip::OnRoll);
		EIC->BindAction(Actions.Look, ETriggerEvent::Triggered, this, &AFLShip::OnLook);
		EIC->BindAction(Actions.Cruise, ETriggerEvent::Started, this, &AFLShip::OnCruiseToggle);
	}
}

void AFLShip::PawnClientRestart()
{
	Super::PawnClientRestart();
	// Belegung dem lokalen Spieler zuweisen (Besitzer-Client)
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (!Actions.Mapping)
				{
					Actions = FLInput::CreateDefaultActions(this);
				}
				Sub->AddMappingContext(Actions.Mapping, 0);
			}
		}
	}
}

void AFLShip::OnThrottle(const FInputActionValue& V) { Raw.Throttle = V.Get<float>(); }
void AFLShip::OnStrafeRight(const FInputActionValue& V) { Raw.StrafeRight = V.Get<float>(); }
void AFLShip::OnStrafeUp(const FInputActionValue& V) { Raw.StrafeUp = V.Get<float>(); }
void AFLShip::OnRoll(const FInputActionValue& V) { Raw.Roll = V.Get<float>(); }
void AFLShip::OnCruiseToggle(const FInputActionValue& V) { Raw.bCruise = !Raw.bCruise; }
void AFLShip::OnLook(const FInputActionValue& V)
{
	// Maus liefert ein Delta je Frame (wird in Tick verbraucht), Stick einen Wert -1..1 (bleibt stehen, bis neu gesetzt)
	Raw.Look += V.Get<FVector2D>();
	Raw.bLookIsDelta = true;
}

void AFLShip::HandleCollisions()
{
	// Feste Objekte (Planeten, Stationen)
	if (const UFLStaticCollisionSubsystem* Statics = GetWorld()->GetSubsystem<UFLStaticCollisionSubsystem>())
	{
		TArray<const FFLStaticSphere*> Hits;
		const FFLFlightState& S = Movement->GetState();
		Statics->Query(S.Position, CollisionRadius, Hits);
		TArray<FFLStaticSphere> Asteroids;
		Statics->QueryAsteroids(S.Position, CollisionRadius, Asteroids);
		for (const FFLStaticSphere& A : Asteroids) { Hits.Add(&A); }
		for (const FFLStaticSphere* Hit : Hits)
		{
			const FFLCollisionBody Body{Movement->GetState().Position, Movement->GetState().Velocity, Movement->Params.Mass, (double)CollisionRadius};
			const FFLCollisionResult R = FLCollision::ResolveStatic(Body, *Hit, 0.3);
			if (R.bHit)
			{
				Movement->ApplyCollision(R.DeltaVelocityA, R.PushA, R.ImpactSpeed);
				UE_LOG(LogFLShip, Display, TEXT("Kollision %s <-> fest %s: Aufprall %.0f cm/s"), *GetName(), *Hit->Id.ToString(), R.ImpactSpeed);
			}
		}
	}

	// Nur der Server entscheidet; jedes Paar einmal (Reihenfolge nach Objektname)
	const FFLFlightState& MyState = Movement->GetState();
	for (TActorIterator<AFLShip> It(GetWorld()); It; ++It)
	{
		AFLShip* Other = *It;
		if (Other == this || Other->GetFName().Compare(GetFName()) < 0)
		{
			continue;
		}
		const FFLFlightState& OtherState = Other->Movement->GetState();
		const double Reach = (double)CollisionRadius + Other->CollisionRadius;
		if (FVector::DistSquared(MyState.Position, OtherState.Position) > Reach * Reach)
		{
			continue;
		}
		FFLCollisionBody A{MyState.Position, MyState.Velocity, Movement->Params.Mass, (double)CollisionRadius};
		FFLCollisionBody B{OtherState.Position, OtherState.Velocity, Other->Movement->Params.Mass, (double)Other->CollisionRadius};
		const FFLCollisionResult R = FLCollision::ResolveSpheres(A, B, 0.4);
		if (R.bHit)
		{
			Movement->ApplyCollision(R.DeltaVelocityA, R.PushA, R.ImpactSpeed);
			Other->Movement->ApplyCollision(R.DeltaVelocityB, R.PushB, R.ImpactSpeed);
			UE_LOG(LogFLShip, Display, TEXT("Kollision %s <-> %s: Aufprall %.0f cm/s, Eindringtiefe %.0f cm"), *GetName(), *Other->GetName(), R.ImpactSpeed, R.Penetration);
		}
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

	// Testhilfe: mit -FLShot=<Sekunden> macht der besitzende Client nach dem Beitritt einen Screenshot (sichtbare Abnahmetests)
	if (!bShotScheduled && IsLocallyControlled())
	{
		float ShotDelay = 0.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("-FLShot="), ShotDelay) && ShotDelay > 0.f)
		{
			bShotScheduled = true;
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) { PC->ConsoleCommand(TEXT("HighResShot 1280x720")); } }, ShotDelay, false);
		}
	}

	if (IsLocallyControlled() && !(bBot || CVarBotFlight.GetValueOnGameThread() != 0))
	{
		Movement->SetInput(FLInput::Compose(Raw, InputSettings));
		Raw.Look = FVector2D::ZeroVector; // Delta wurde verbraucht
	}

	if ((bBot || CVarBotFlight.GetValueOnGameThread() != 0) && (IsLocallyControlled() || GetLocalRole() == ROLE_AutonomousProxy))
	{
		// Testmuster: Vollschub, wechselnde Kurven (Gieren/Nicken/Rollen), damit Vorhersage und Abgleich arbeiten muessen
		BotTime += DeltaSeconds;
		FFLFlightInput In;
		In.Throttle = 1.f;
		In.Yaw = FMath::Sin(BotTime * 0.8f);
		In.Pitch = 0.5f * FMath::Sin(BotTime * 0.5f + 1.f);
		In.Roll = 0.3f * FMath::Cos(BotTime * 0.7f);
		In.bCruise = FMath::Fmod(BotTime, 40.f) > 15.f && FMath::Fmod(BotTime, 40.f) < 30.f; // Cruise-Abschnitt im Wechsel (Netzwerk-Test mit Zustandsautomat)
		Movement->SetInput(In);
	}

	if (HasAuthority())
	{
		HandleCollisions();
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
		const FVector CamLoc = Camera ? Camera->GetComponentLocation() : FVector::ZeroVector;
		const FVector ActLoc = GetActorLocation();
		UE_LOG(LogFLShip, Display, TEXT("FLShip %s Kamera=(%.0f, %.0f, %.0f) Actor=(%.0f, %.0f, %.0f) Skalierung=%.2f NaN=%d Visual=%d"),
			*GetName(), CamLoc.X, CamLoc.Y, CamLoc.Z, ActLoc.X, ActLoc.Y, ActLoc.Z, GetActorScale3D().X,
			(CamLoc.ContainsNaN() || ActLoc.ContainsNaN() || GetActorQuat().ContainsNaN()) ? 1 : 0, Visual ? 1 : 0);
		UE_LOG(LogFLShip, Display, TEXT("FLShip %s Rolle=%d: Pos=(%.0f, %.0f, %.0f) Tempo=%.0f cm/s Korrekturen=%d MaxFehler=%.1f cm Moves=%d Abgelehnt=%d Zeit(Spiel)=%.1f s gutgeschrieben=%.1f s verbraucht=%.1f s Wanduhr=%.1f s dt=%.4f Frames=%llu Benchmark=%d FesterSchritt=%d"),
			*GetName(), (int32)GetLocalRole(), S.Position.X, S.Position.Y, S.Position.Z, S.Velocity.Size(),
			Movement->GetCorrectionCount(), Movement->GetMaxPositionError(), Movement->GetAppliedMoves(), Movement->GetTrimmedMoves(), GetWorld()->GetTimeSeconds(), Movement->GetBudgetAccrued(), Movement->GetBudgetConsumed(), FPlatformTime::Seconds() - GStartTime, DeltaSeconds, (unsigned long long)GFrameCounter, FApp::IsBenchmarking() ? 1 : 0, FApp::UseFixedTimeStep() ? 1 : 0);
	}
}
