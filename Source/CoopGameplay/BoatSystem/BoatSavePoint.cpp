#include "BoatSavePoint.h"

#include "Boat.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

ABoatSavePoint::ABoatSavePoint()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);

	bReplicates = true;
	SetReplicateMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(RootComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCastShadow(true);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldStatic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	TriggerBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(150.f, 150.f, 120.f));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABoatSavePoint::OnTriggerBeginOverlap);

	SaveOrder = INDEX_NONE;
	SaveOffset = FVector::ZeroVector;
	bUseThisActorRotation = true;
	CooldownSeconds = 0.25f;

	ActivatedMaterial = nullptr;
	RiseHeight = 40.0f;
	FastSpinTurns = 3.0f;
	FastSpinDuration = 0.35f;
	ReturnDuration = 0.8f;
	ReturnSpinDegrees = 360.0f;

	ActivateSound = nullptr;
	ActivateSoundVolume = 1.0f;
	ActivateSoundPitch = 1.0f;
}

void ABoatSavePoint::BeginPlay()
{
	Super::BeginPlay();

	if (VisualMesh)
	{
		BaseMeshRelativeLocation = VisualMesh->GetRelativeLocation();
		BaseMeshRelativeRotation = VisualMesh->GetRelativeRotation();
	}

	if (bActivated)
	{
		ApplyActivatedMaterial_Local();
	}
}

void ABoatSavePoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAnimating || !VisualMesh)
	{
		return;
	}

	const float FastTotalDegrees = FastSpinTurns * 360.0f;
	const float ReturnTotalDegrees = ReturnSpinDegrees;

	if (AnimPhase == EAnimPhase::FastSpinUp)
	{
		AnimElapsed += DeltaSeconds;

		const float Alpha = FMath::Clamp(
			AnimElapsed / FMath::Max(0.01f, FastSpinDuration),
			0.0f,
			1.0f
		);

		const float EaseAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);

		FVector NewLoc = BaseMeshRelativeLocation;
		NewLoc.Z += RiseHeight * EaseAlpha;
		VisualMesh->SetRelativeLocation(NewLoc);

		FRotator NewRot = BaseMeshRelativeRotation;
		NewRot.Yaw += FastTotalDegrees * EaseAlpha;
		VisualMesh->SetRelativeRotation(NewRot);

		if (Alpha >= 1.0f)
		{
			AnimPhase = EAnimPhase::SlowReturn;
			AnimElapsed = 0.0f;
		}
	}
	else if (AnimPhase == EAnimPhase::SlowReturn)
	{
		AnimElapsed += DeltaSeconds;

		const float Alpha = FMath::Clamp(
			AnimElapsed / FMath::Max(0.01f, ReturnDuration),
			0.0f,
			1.0f
		);

		const float EaseAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

		FVector NewLoc = BaseMeshRelativeLocation;
		NewLoc.Z += FMath::Lerp(RiseHeight, 0.0f, EaseAlpha);
		VisualMesh->SetRelativeLocation(NewLoc);

		const float TotalYawOffset = FastTotalDegrees + (ReturnTotalDegrees * EaseAlpha);

		FRotator NewRot = BaseMeshRelativeRotation;
		NewRot.Yaw += TotalYawOffset;
		VisualMesh->SetRelativeRotation(NewRot);

		if (Alpha >= 1.0f)
		{
			bAnimating = false;
			AnimPhase = EAnimPhase::None;
			AnimElapsed = 0.0f;

			VisualMesh->SetRelativeLocation(BaseMeshRelativeLocation);
			VisualMesh->SetRelativeRotation(BaseMeshRelativeRotation);

			SetActorTickEnabled(false);
		}
	}
}

void ABoatSavePoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABoatSavePoint, bActivated);
}

void ABoatSavePoint::OnRep_Activated()
{
	if (bActivated)
	{
		StartActivateVisuals_Local();
	}
}

void ABoatSavePoint::ApplyActivatedMaterial_Local()
{
	if (!VisualMesh || !ActivatedMaterial)
	{
		return;
	}

	const int32 MatCount = VisualMesh->GetNumMaterials();
	for (int32 i = 0; i < MatCount; ++i)
	{
		VisualMesh->SetMaterial(i, ActivatedMaterial);
	}
}

void ABoatSavePoint::PlayActivateSound_Local()
{
	if (!ActivateSound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		ActivateSound,
		GetActorLocation(),
		ActivateSoundVolume,
		ActivateSoundPitch
	);
}

void ABoatSavePoint::StartActivateVisuals_Local()
{
	if (!VisualMesh)
	{
		return;
	}

	ApplyActivatedMaterial_Local();
	PlayActivateSound_Local();

	BP_OnActivated();

	if (bAnimating)
	{
		return;
	}

	VisualMesh->SetRelativeLocation(BaseMeshRelativeLocation);
	VisualMesh->SetRelativeRotation(BaseMeshRelativeRotation);

	bAnimating = true;
	AnimElapsed = 0.0f;
	AnimPhase = EAnimPhase::FastSpinUp;
	SetActorTickEnabled(true);
}

void ABoatSavePoint::OnTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (CooldownSeconds > 0.f && (Now - LastTriggerTime) < CooldownSeconds) return;
	LastTriggerTime = Now;

	if (!OtherActor) return;

	ABoat* Boat = Cast<ABoat>(OtherActor);
	if (!Boat && OtherComp)
	{
		Boat = Cast<ABoat>(OtherComp->GetOwner());
	}

	if (!Boat) return;

	FTransform SaveTransform = GetActorTransform();
	SaveTransform.AddToTranslation(SaveOffset);

	if (!bUseThisActorRotation)
	{
		SaveTransform.SetRotation(Boat->GetActorQuat());
	}

	Boat->SaveRespawnPoint_Server(SaveTransform, SaveOrder);

	if (!bActivated)
	{
		bActivated = true;
		StartActivateVisuals_Local();
	}
}