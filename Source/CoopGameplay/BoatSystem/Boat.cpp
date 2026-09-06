#include "Boat.h"

#include "BoatPlayerController.h"
#include "GI_Steam.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

ABoat::ABoat()
{
	bReplicates = true;

	SetReplicateMovement(false);

	NetUpdateFrequency = 5.f;
	MinNetUpdateFrequency = 2.f;

	bAlwaysRelevant = true;
	NetPriority = 3.0f;

	PrimaryActorTick.bCanEverTick = true;

	// SetViewTargetWithBlend(this)를 했을 때
	// 이 Actor 안에 있는 CameraComponent를 자동으로 찾게 함
	bFindCameraComponentWhenViewTarget = true;

	BoatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoatMesh"));
	SetRootComponent(BoatMesh);

	BoatMesh->SetIsReplicated(false);

	BoatMesh->SetSimulatePhysics(false);
	BoatMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	BoatMesh->SetLinearDamping(1.6f);
	BoatMesh->SetAngularDamping(3.6f);

	BoatMesh->BodyInstance.bLockXRotation = true;
	BoatMesh->BodyInstance.bLockYRotation = true;

	// 보트 자기 몸체 때문에 카메라 암이 줄어드는 것을 방지
	// 벽/장애물은 Camera Block, 보트 자기 자신은 Camera Ignore가 맞음
	BoatMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BoatMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	BoardTriggerLeft = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardTriggerLeft"));
	BoardTriggerLeft->SetupAttachment(RootComponent);
	BoardTriggerLeft->SetCollisionProfileName(TEXT("Trigger"));
	BoardTriggerLeft->SetGenerateOverlapEvents(true);
	BoardTriggerLeft->SetBoxExtent(FVector(160.f, 110.f, 130.f));
	BoardTriggerLeft->SetRelativeLocation(FVector(0.f, -90.f, 60.f));
	BoardTriggerLeft->OnComponentBeginOverlap.AddDynamic(this, &ABoat::HandleBeginOverlapLeft);

	BoardTriggerRight = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardTriggerRight"));
	BoardTriggerRight->SetupAttachment(RootComponent);
	BoardTriggerRight->SetCollisionProfileName(TEXT("Trigger"));
	BoardTriggerRight->SetGenerateOverlapEvents(true);
	BoardTriggerRight->SetBoxExtent(FVector(160.f, 110.f, 130.f));
	BoardTriggerRight->SetRelativeLocation(FVector(0.f, 90.f, 60.f));
	BoardTriggerRight->OnComponentBeginOverlap.AddDynamic(this, &ABoat::HandleBeginOverlapRight);

	SeatsRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SeatsRoot"));
	SeatsRoot->SetupAttachment(RootComponent);

	SeatLeft = CreateDefaultSubobject<USceneComponent>(TEXT("SeatLeft"));
	SeatLeft->SetupAttachment(SeatsRoot);
	SeatLeft->SetRelativeLocation(FVector(0.f, -60.f, 50.f));

	SeatRight = CreateDefaultSubobject<USceneComponent>(TEXT("SeatRight"));
	SeatRight->SetupAttachment(SeatsRoot);
	SeatRight->SetRelativeLocation(FVector(0.f, 60.f, 50.f));

	SeatAvatarLeft = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SeatAvatarLeft"));
	SeatAvatarLeft->SetupAttachment(SeatLeft);
	SeatAvatarLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SeatAvatarLeft->SetGenerateOverlapEvents(false);
	SeatAvatarLeft->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SeatAvatarLeft->SetHiddenInGame(true, true);
	SeatAvatarLeft->SetVisibility(false, true);

	SeatAvatarRight = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SeatAvatarRight"));
	SeatAvatarRight->SetupAttachment(SeatRight);
	SeatAvatarRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SeatAvatarRight->SetGenerateOverlapEvents(false);
	SeatAvatarRight->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SeatAvatarRight->SetHiddenInGame(true, true);
	SeatAvatarRight->SetVisibility(false, true);

	SeatPaddleLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SeatPaddleLeft"));
	SeatPaddleLeft->SetupAttachment(SeatAvatarLeft);
	SeatPaddleLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SeatPaddleLeft->SetGenerateOverlapEvents(false);
	SeatPaddleLeft->SetHiddenInGame(true, true);
	SeatPaddleLeft->SetVisibility(false, true);

	SeatPaddleRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SeatPaddleRight"));
	SeatPaddleRight->SetupAttachment(SeatAvatarRight);
	SeatPaddleRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SeatPaddleRight->SetGenerateOverlapEvents(false);
	SeatPaddleRight->SetHiddenInGame(true, true);
	SeatPaddleRight->SetVisibility(false, true);

	// =========================
	// Boat Camera Spring Arm
	// =========================
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);

	// 카메라 거리
	SpringArm->TargetArmLength = 450.f;

	// 보트 위에서 살짝 내려다보는 각도
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));

	// 핵심 1: 카메라 벽 관통 방지
	SpringArm->bDoCollisionTest = true;

	// 핵심 2: SpringArm이 Camera 채널로 벽을 검사함
	SpringArm->ProbeChannel = ECC_Camera;

	// 핵심 3: 너무 작으면 얇은 벽에서 뚫리는 느낌이 날 수 있음
	SpringArm->ProbeSize = 24.f;

	// ABoat는 Pawn이 아니라 Actor이므로 이 값은 false로 둠.
	// Tick에서 각 컴퓨터의 로컬 마우스 이동량을 직접 읽어 SpringArm만 회전시킴.
	SpringArm->bUsePawnControlRotation = false;

	// 보트가 기울어지거나 회전할 때 카메라가 보트 회전을 따라가게 할지 여부
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = true;

	// 선택 사항: 카메라 움직임 부드럽게
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 8.f;
	SpringArm->bEnableCameraRotationLag = false;

	BoatCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("BoatCamera"));
	BoatCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	// 핵심 4: CameraComponent 자체 위치를 밀지 말고,
	// 거리는 SpringArm TargetArmLength로 조절해야 충돌 계산이 정상적임
	BoatCamera->SetRelativeLocation(FVector::ZeroVector);
	BoatCamera->SetRelativeRotation(FRotator::ZeroRotator);
	BoatCamera->bAutoActivate = true;

	// =========================
	// Boat BGM Audio Component
	// =========================
	// 실제 재생 여부는 UpdateBoatBGM_Local()이 결정한다.
	// 2D BGM이므로 위치 감쇠/공간화를 사용하지 않는다.
	BoatBGMComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("BoatBGMComponent"));
	BoatBGMComponent->SetupAttachment(RootComponent);
	BoatBGMComponent->bAutoActivate = false;
	BoatBGMComponent->bAllowSpatialization = false;
	BoatBGMComponent->SetIsReplicated(false);

	LeftOccupant = nullptr;
	RightOccupant = nullptr;

	LeftAvatarId = 0;
	RightAvatarId = 0;

	bLeftLoosePaddleHidden = false;
	bRightLoosePaddleHidden = false;

	PaddleSoundCue = nullptr;
	BoatBGMSound = nullptr;
	BoatBGMVolume = 1.0f;
	BoatBGMPitch = 1.0f;
	DeathSoundCue = nullptr;
	DeathSoundAttenuation = nullptr;
	DeathSoundVolume = 1.0f;
	DeathSoundPitch = 1.0f;

	bHideCharacterWhenSeated = true;

	DeathDissolveMaterial = nullptr;
	DeathDissolveParameterName = TEXT("Disolve");
	DeathDissolveStartValue = -1.0f;
	DeathDissolveEndValue = 1.0f;
	DefaultDeathSequenceDuration = 3.0f;
}

void ABoat::BeginPlay()
{
	Super::BeginPlay();

	// BGM은 게임 시작 시 자동 재생하지 않는다.
	// 이 컴퓨터의 로컬 플레이어가 탑승 확정된 순간 UpdateBoatBGM_Local()에서 시작한다.
	if (BoatBGMComponent)
	{
		BoatBGMComponent->Stop();
		BoatBGMComponent->SetSound(BoatBGMSound);
		BoatBGMComponent->SetVolumeMultiplier(FMath::Max(0.0f, BoatBGMVolume));
		BoatBGMComponent->SetPitchMultiplier(FMath::Max(0.1f, BoatBGMPitch));
	}

	// =========================
	// Runtime Camera Collision Force Setting
	// =========================
	// BP 인스턴스에 예전 값이 저장되어 있어도 게임 시작 시 강제로 다시 적용.
	if (SpringArm)
	{
		SpringArm->bDoCollisionTest = true;
		SpringArm->ProbeChannel = ECC_Camera;
		SpringArm->ProbeSize = 24.f;

		SpringArm->bUsePawnControlRotation = false;

		SpringArm->bInheritPitch = true;
		SpringArm->bInheritYaw = true;
		SpringArm->bInheritRoll = true;

		SpringArm->bEnableCameraLag = true;
		SpringArm->CameraLagSpeed = 8.f;
		SpringArm->bEnableCameraRotationLag = false;

		// BP_Boat에서 조절해 둔 기본 카메라 각도를 보존해 둠.
		FreeLookOriginalSpringArmRelativeRotation_Local = SpringArm->GetRelativeRotation();
	}

	if (BoatCamera)
	{
		// BP에서 잡아둔 BoatCamera / SpringArm의 위치, 회전값을 살리기 위해
		// 여기서 SetRelativeLocation / SetRelativeRotation으로 강제 초기화하지 않음.
		// 카메라 각도는 BP_Boat에서 SpringArm 또는 BoatCamera를 선택해서 조절하면 됨.
		BoatCamera->bAutoActivate = true;
		BoatCamera->SetActive(true);
		BoatCamera->Activate(true);
	}

	RefreshLoosePaddles_Local();

	if (!HasAuthority())
	{
		BoatMesh->SetSimulatePhysics(false);
		BoatMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 보트 자기 자신은 카메라 충돌 검사에서 무시
		BoatMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		UpdateBoatOccupancyState_Local();

		// 커스텀 네트워크 스무딩을 사용하지 않더라도 자유 시점 카메라 갱신에 Tick이 필요함.
		SetActorTickEnabled(true);
		return;
	}

	// 리슨 서버의 로컬 플레이어도 자유 시점 카메라를 사용하므로 Tick 유지.
	SetActorTickEnabled(true);

	SavedRespawnTransform = GetActorTransform();
	bHasSavedRespawnTransform = true;
	SavedRespawnOrder = INDEX_NONE;

	if (bOverrideMass)
	{
		BoatMesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	}

	BoatMesh->SetPhysicsMaxAngularVelocityInDegrees(MaxAngularDegPerSec);

	// 서버에서도 보트 자기 자신은 카메라 충돌 검사에서 무시
	BoatMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	UpdateBoatOccupancyState_Server();
	StartNetStateTimer_Server();
}

void ABoat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateDeathVisuals_Local(DeltaSeconds);

	// 서버(리슨 호스트)와 클라이언트가 각자 자기 PC에서 BGM을 한 개만 관리한다.
	// GI_Steam::bLoadingOn까지 함께 확인하므로 로딩 오버레이가 켜지는 순간 BGM을 정지한다.
	UpdateBoatBGM_Local();

	// 서버/클라이언트 모두 자기 컴퓨터의 로컬 마우스 회전만 사용함.
	UpdateFreeLookCamera_Local();

	if (!HasAuthority() && bUseCustomNetSmoothing)
	{
		SmoothBoat_Client(DeltaSeconds);
	}
}

void ABoat::StopBoatBGM_Local()
{
	if (!BoatBGMComponent)
	{
		return;
	}

	if (BoatBGMComponent->IsPlaying())
	{
		BoatBGMComponent->Stop();
	}
}

void ABoat::UpdateBoatBGM_Local()
{
	if (!BoatBGMComponent)
	{
		return;
	}

	// Dedicated Server에서는 소리를 재생할 필요가 없다.
	if (IsRunningDedicatedServer())
	{
		StopBoatBGM_Local();
		return;
	}

	// 기존 로딩 시스템을 침범하지 않고 GI의 현재 로딩 상태만 읽는다.
	// 로딩 화면이 켜져 있는 동안에는 보트 BGM을 확실히 정지한다.
	bool bLoadingNow = false;
	if (const UGI_Steam* SteamGI = Cast<UGI_Steam>(GetGameInstance()))
	{
		bLoadingNow = SteamGI->bLoadingOn;
	}

	// 두 좌석이 모두 찼는지가 아니라, 이 컴퓨터의 로컬 플레이어가
	// 실제로 탑승 승인을 받았는지를 기준으로 재생한다.
	// 따라서 호스트/클라이언트가 각자 자기 탑승 순간 바로 BGM을 듣는다.
	const bool bShouldPlay =
		!bLoadingNow &&
		bLocalPlayerBoardedForBGM_Local &&
		IsValid(BoatBGMSound);

	if (!bShouldPlay)
	{
		StopBoatBGM_Local();
		return;
	}

	// 이미 재생 중이면 절대로 새 인스턴스를 만들거나 처음부터 다시 재생하지 않는다.
	if (BoatBGMComponent->IsPlaying())
	{
		return;
	}

	// BP에서 BGM 에셋/볼륨/피치를 바꿔도 실행 시 현재 값을 사용한다.
	BoatBGMComponent->SetSound(BoatBGMSound);
	BoatBGMComponent->SetVolumeMultiplier(FMath::Max(0.0f, BoatBGMVolume));
	BoatBGMComponent->SetPitchMultiplier(FMath::Max(0.1f, BoatBGMPitch));

	BoatBGMComponent->Play(0.0f);
}

void ABoat::ResetFreeLookCamera_Local()
{
	if (SpringArm && bFreeLookInitialized_Local)
	{
		// 자유 시점이 끝나면 BP_Boat에 저장된 원래 상대 회전 상태로 복구.
		SpringArm->SetAbsolute(false, false, false);
		SpringArm->SetRelativeRotation(FreeLookOriginalSpringArmRelativeRotation_Local);
	}

	bFreeLookInitialized_Local = false;
	FreeLookController_Local.Reset();
	FreeLookYaw_Local = 0.0f;
	FreeLookPitch_Local = 0.0f;
}

void ABoat::UpdateFreeLookCamera_Local()
{
	if (!bEnableFreeLookCamera || !SpringArm || !GetWorld())
	{
		ResetFreeLookCamera_Local();
		return;
	}

	APlayerController* LocalPC = nullptr;

	// 리슨 서버에서는 호스트의 로컬 컨트롤러,
	// 클라이언트에서는 그 클라이언트의 로컬 컨트롤러만 찾음.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Candidate = It->Get();
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (!Candidate->IsLocalController())
		{
			continue;
		}

		// 현재 이 보트를 ViewTarget으로 보고 있는 로컬 플레이어에게만 적용.
		if (Candidate->GetViewTarget() != this)
		{
			continue;
		}

		LocalPC = Candidate;
		break;
	}

	if (!LocalPC)
	{
		ResetFreeLookCamera_Local();
		return;
	}

	// 캐릭터 카메라 → 보트 카메라 시점 전환 중에는
	// SpringArm을 기존 보트 카메라 각도로 유지하고 마우스 입력을 받지 않음.
	if (GetWorld()->GetTimeSeconds() < FreeLookInputUnlockTime_Local)
	{
		ResetFreeLookCamera_Local();
		return;
	}

	// 처음 보트 카메라로 바뀐 순간의 카메라 방향을 시작 방향으로 사용.
	// 이렇게 해야 탑승 순간 카메라가 엉뚱한 방향으로 튀지 않음.
	if (!bFreeLookInitialized_Local || FreeLookController_Local.Get() != LocalPC)
	{
		const FRotator InitialViewRotation = SpringArm->GetComponentRotation();

		FreeLookYaw_Local = InitialViewRotation.Yaw;
		FreeLookPitch_Local = FMath::Clamp(
			FMath::UnwindDegrees(InitialViewRotation.Pitch),
			FreeLookMinPitch,
			FreeLookMaxPitch
		);

		// 위치는 보트를 따라가지만 회전은 보트 회전으로부터 분리.
		SpringArm->SetAbsolute(false, true, false);
		SpringArm->SetWorldRotation(FRotator(FreeLookPitch_Local, FreeLookYaw_Local, 0.0f));

		FreeLookController_Local = LocalPC;
		bFreeLookInitialized_Local = true;
	}

	// BoatPlayerController의 입력 매핑을 수정하지 않고도 동작하도록
	// 현재 프레임의 실제 마우스 이동량을 로컬에서 직접 읽음.
	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	LocalPC->GetInputMouseDelta(MouseDeltaX, MouseDeltaY);

	// 옵션에서 사용하는 GI_Steam의 MouseSensitivity를 그대로 사용.
	// GI 값이 변경되면 보트 탑승 중에도 다음 프레임부터 즉시 반영됨.
	float CurrentMouseSensitivity = 0.35f;

	if (const UGI_Steam* SteamGI = Cast<UGI_Steam>(GetGameInstance()))
	{
		CurrentMouseSensitivity = FMath::Clamp(SteamGI->MouseSensitivity, 0.01f, 10.0f);
	}

	FreeLookYaw_Local += MouseDeltaX * CurrentMouseSensitivity;

	// 현재 확정된 상하 방향 유지:
	// 마우스를 아래로 내리면 아래를 보고, 위로 올리면 위를 봄.
	FreeLookPitch_Local += MouseDeltaY * CurrentMouseSensitivity;
	FreeLookPitch_Local = FMath::Clamp(
		FreeLookPitch_Local,
		FreeLookMinPitch,
		FreeLookMaxPitch
	);

	FreeLookYaw_Local = FMath::UnwindDegrees(FreeLookYaw_Local);

	// 이 회전은 현재 실행 중인 컴퓨터의 Boat Actor 복사본에만 적용되며 복제하지 않음.
	// 따라서 서버와 클라이언트가 서로 완전히 다른 방향을 볼 수 있음.
	SpringArm->SetWorldRotation(FRotator(FreeLookPitch_Local, FreeLookYaw_Local, 0.0f));
}


void ABoat::PrepareDeathMaterialsForComponent_Local(
	UMeshComponent* MeshComp,
	TArray<TObjectPtr<UMaterialInterface>>& OriginalMaterials,
	TArray<TObjectPtr<UMaterialInstanceDynamic>>& DissolveMIDs)
{
	if (!MeshComp || !DeathDissolveMaterial)
	{
		return;
	}

	const int32 NumMaterials = MeshComp->GetNumMaterials();
	if (NumMaterials <= 0)
	{
		return;
	}

	if (OriginalMaterials.Num() != NumMaterials)
	{
		OriginalMaterials.Reset();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			OriginalMaterials.Add(MeshComp->GetMaterial(i));
		}
	}

	if (DissolveMIDs.Num() != NumMaterials)
	{
		DissolveMIDs.Reset();

		for (int32 i = 0; i < NumMaterials; ++i)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(DeathDissolveMaterial, this);
			DissolveMIDs.Add(MID);
		}
	}

	for (int32 i = 0; i < NumMaterials; ++i)
	{
		if (DissolveMIDs.IsValidIndex(i) && DissolveMIDs[i])
		{
			MeshComp->SetMaterial(i, DissolveMIDs[i]);
			DissolveMIDs[i]->SetScalarParameterValue(DeathDissolveParameterName, DeathDissolveStartValue);
		}
	}
}

void ABoat::PrepareAllDeathMaterials_Local()
{
	PrepareDeathMaterialsForComponent_Local(BoatMesh, BoatMeshOriginalMaterials, BoatMeshDissolveMIDs);
	PrepareDeathMaterialsForComponent_Local(SeatAvatarLeft, SeatAvatarLeftOriginalMaterials, SeatAvatarLeftDissolveMIDs);
	PrepareDeathMaterialsForComponent_Local(SeatAvatarRight, SeatAvatarRightOriginalMaterials, SeatAvatarRightDissolveMIDs);
	PrepareDeathMaterialsForComponent_Local(SeatPaddleLeft, SeatPaddleLeftOriginalMaterials, SeatPaddleLeftDissolveMIDs);
	PrepareDeathMaterialsForComponent_Local(SeatPaddleRight, SeatPaddleRightOriginalMaterials, SeatPaddleRightDissolveMIDs);
}

void ABoat::SetAllDeathMaterialParameters_Local(float Value)
{
	auto ApplyValue = [this, Value](TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs)
		{
			for (UMaterialInstanceDynamic* MID : MIDs)
			{
				if (MID)
				{
					MID->SetScalarParameterValue(DeathDissolveParameterName, Value);
				}
			}
		};

	ApplyValue(BoatMeshDissolveMIDs);
	ApplyValue(SeatAvatarLeftDissolveMIDs);
	ApplyValue(SeatAvatarRightDissolveMIDs);
	ApplyValue(SeatPaddleLeftDissolveMIDs);
	ApplyValue(SeatPaddleRightDissolveMIDs);
}

void ABoat::RestoreMaterialsForComponent_Local(
	UMeshComponent* MeshComp,
	const TArray<TObjectPtr<UMaterialInterface>>& OriginalMaterials)
{
	if (!MeshComp || OriginalMaterials.Num() <= 0)
	{
		return;
	}

	const int32 NumMaterials = FMath::Min(MeshComp->GetNumMaterials(), OriginalMaterials.Num());
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		MeshComp->SetMaterial(i, OriginalMaterials[i]);
	}
}

void ABoat::RestoreAllDeathMaterials_Local()
{
	RestoreMaterialsForComponent_Local(BoatMesh, BoatMeshOriginalMaterials);
	RestoreMaterialsForComponent_Local(SeatAvatarLeft, SeatAvatarLeftOriginalMaterials);
	RestoreMaterialsForComponent_Local(SeatAvatarRight, SeatAvatarRightOriginalMaterials);
	RestoreMaterialsForComponent_Local(SeatPaddleLeft, SeatPaddleLeftOriginalMaterials);
	RestoreMaterialsForComponent_Local(SeatPaddleRight, SeatPaddleRightOriginalMaterials);

	RefreshSeatVisuals_Local();
	RefreshPaddleProps_Local();
}

void ABoat::ResetTickEnabledAfterDeath_Local()
{
	// 죽는 연출이 끝난 뒤에도 자유 시점 카메라 갱신이 필요함.
	SetActorTickEnabled(true);
}

void ABoat::UpdateDeathVisuals_Local(float DeltaSeconds)
{
	if (!bDeathVisualActive_Local || !DeathDissolveMaterial)
	{
		return;
	}

	DeathVisualElapsed_Local += DeltaSeconds;

	const float Alpha = FMath::Clamp(
		DeathVisualElapsed_Local / FMath::Max(0.01f, DeathVisualDuration_Local),
		0.0f,
		1.0f
	);

	const float ParamValue = FMath::Lerp(DeathDissolveStartValue, DeathDissolveEndValue, Alpha);
	SetAllDeathMaterialParameters_Local(ParamValue);

	if (Alpha >= 1.0f)
	{
		bDeathVisualActive_Local = false;
	}
}

void ABoat::StartNetStateTimer_Server()
{
	if (!HasAuthority() || !GetWorld()) return;

	const float Hz = FMath::Clamp(NetStateSendHz, 5.f, 60.f);
	const float Interval = 1.f / Hz;

	GetWorldTimerManager().ClearTimer(NetStateTimer);
	GetWorldTimerManager().SetTimer(NetStateTimer, this, &ABoat::BroadcastNetState_Server, Interval, true);

	BroadcastNetState_Server_Immediate();
}

void ABoat::BroadcastNetState_Server()
{
	if (!HasAuthority()) return;
	if (!BoatMesh || !BoatMesh->IsSimulatingPhysics()) return;

	BroadcastNetState_Server_Immediate();
}

void ABoat::BroadcastNetState_Server_Immediate()
{
	if (!HasAuthority() || !BoatMesh || !GetWorld()) return;

	const double Now = GetWorld()->GetTimeSeconds();

	const double MinInterval = 1.0 / 60.0;
	if ((Now - LastNetStateSendTime) < MinInterval) return;
	LastNetStateSendTime = Now;

	FBoatNetState S;
	S.Pos = BoatMesh->GetComponentLocation();
	S.Rot = BoatMesh->GetComponentRotation();
	S.LinVel = BoatMesh->IsSimulatingPhysics() ? BoatMesh->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	S.YawRateDeg = BoatMesh->IsSimulatingPhysics() ? BoatMesh->GetPhysicsAngularVelocityInDegrees().Z : 0.f;
	S.ServerTime = (float)Now;

	Multicast_BoatState(S);
}

void ABoat::Multicast_BoatState_Implementation(const FBoatNetState& State)
{
	if (HasAuthority()) return;
	PushNetState_Client(State);
}

void ABoat::Multicast_BeginDeathSequence_Implementation(float DeathDuration)
{
	DeathVisualDuration_Local = FMath::Max(0.05f, DeathDuration);
	DeathVisualElapsed_Local = 0.0f;
	bDeathVisualActive_Local = true;

	if (DeathDissolveMaterial)
	{
		PrepareAllDeathMaterials_Local();
		SetAllDeathMaterialParameters_Local(DeathDissolveStartValue);
	}

	// 죽는 연출 시작 순간, 서버/모든 클라이언트에서 같은 위치에 사운드 재생
	PlayDeathSound_Local();

	SetActorTickEnabled(true);
	BP_OnDeathSequenceStarted();
}

void ABoat::Multicast_EndDeathSequence_Implementation()
{
	bDeathVisualActive_Local = false;
	DeathVisualElapsed_Local = 0.0f;
	DeathVisualDuration_Local = 0.0f;

	if (DeathDissolveMaterial)
	{
		RestoreAllDeathMaterials_Local();
	}

	ResetTickEnabledAfterDeath_Local();
	BP_OnDeathSequenceFinished();
}

void ABoat::PushNetState_Client(const FBoatNetState& State)
{
	if (!GetWorld()) return;

	const float ClientNow = (float)GetWorld()->GetTimeSeconds();
	const float NewOffset = ClientNow - State.ServerTime;

	if (!bHasTimeOffset)
	{
		TimeOffsetEstimate = NewOffset;
		bHasTimeOffset = true;
	}
	else
	{
		TimeOffsetEstimate = FMath::Lerp(TimeOffsetEstimate, NewOffset, TimeOffsetSmoothingAlpha);
	}

	NetStateBuffer.Add(State);
	NetStateBuffer.Sort([](const FBoatNetState& A, const FBoatNetState& B) { return A.ServerTime < B.ServerTime; });

	const int32 MaxKeep = 8;
	while (NetStateBuffer.Num() > MaxKeep) NetStateBuffer.RemoveAt(0);
}

void ABoat::SmoothBoat_Client(float DeltaSeconds)
{
	if (!GetWorld() || NetStateBuffer.Num() == 0) return;

	if (NetStateBuffer.Num() == 1)
	{
		const FBoatNetState& S = NetStateBuffer[0];
		SetActorLocationAndRotation(S.Pos, S.Rot, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	const double ClientNow = GetWorld()->GetTimeSeconds();
	const double RenderServerTime = ClientNow - (double)TimeOffsetEstimate - (double)InterpDelay;

	int32 AIdx = INDEX_NONE;
	for (int32 i = 0; i < NetStateBuffer.Num() - 1; ++i)
	{
		const float T0 = NetStateBuffer[i].ServerTime;
		const float T1 = NetStateBuffer[i + 1].ServerTime;

		if (T0 <= RenderServerTime && RenderServerTime <= T1)
		{
			AIdx = i;
			break;
		}
	}

	FVector NewPos;
	FQuat NewRot;

	if (AIdx != INDEX_NONE)
	{
		const FBoatNetState& A = NetStateBuffer[AIdx];
		const FBoatNetState& B = NetStateBuffer[AIdx + 1];

		const float Den = FMath::Max(0.001f, (B.ServerTime - A.ServerTime));
		const float Alpha = FMath::Clamp((float)((RenderServerTime - A.ServerTime) / Den), 0.f, 1.f);

		NewPos = FMath::Lerp((FVector)A.Pos, (FVector)B.Pos, Alpha);
		NewRot = FQuat::Slerp(A.Rot.Quaternion(), B.Rot.Quaternion(), Alpha);
	}
	else
	{
		const FBoatNetState& Last = NetStateBuffer.Last();
		const double Dt = FMath::Clamp((double)RenderServerTime - (double)Last.ServerTime, 0.0, (double)MaxExtrapolate);

		NewPos = (FVector)Last.Pos + (FVector)Last.LinVel * (float)Dt;

		FRotator R = Last.Rot;
		R.Yaw += Last.YawRateDeg * (float)Dt;
		NewRot = R.Quaternion();
	}

	SetActorLocationAndRotation(NewPos, NewRot, false, nullptr, ETeleportType::TeleportPhysics);

	while (NetStateBuffer.Num() >= 3)
	{
		if (NetStateBuffer[1].ServerTime < RenderServerTime - 0.5f) NetStateBuffer.RemoveAt(0);
		else break;
	}
}

void ABoat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABoat, LeftOccupant);
	DOREPLIFETIME(ABoat, RightOccupant);
	DOREPLIFETIME(ABoat, LeftAvatarId);
	DOREPLIFETIME(ABoat, RightAvatarId);
	DOREPLIFETIME(ABoat, bLeftLoosePaddleHidden);
	DOREPLIFETIME(ABoat, bRightLoosePaddleHidden);
}

USceneComponent* ABoat::GetSeatComponent(EBoatSeat Seat) const
{
	return (Seat == EBoatSeat::Left) ? SeatLeft :
		(Seat == EBoatSeat::Right) ? SeatRight : nullptr;
}

bool ABoat::IsSeatOccupied(EBoatSeat Seat) const
{
	if (Seat == EBoatSeat::Left)
	{
		return IsValid(LeftOccupant);
	}

	if (Seat == EBoatSeat::Right)
	{
		return IsValid(RightOccupant);
	}

	return false;
}

bool ABoat::IsSeatOccupiedByOther(EBoatSeat Seat, const ACharacter* Character) const
{
	const ACharacter* Occupant = nullptr;

	if (Seat == EBoatSeat::Left)
	{
		Occupant = LeftOccupant;
	}
	else if (Seat == EBoatSeat::Right)
	{
		Occupant = RightOccupant;
	}

	return IsValid(Occupant) && Occupant != Character;
}

bool ABoat::HasAnyOccupant() const
{
	return IsValid(LeftOccupant) || IsValid(RightOccupant);
}

bool ABoat::HasBothOccupants() const
{
	return IsValid(LeftOccupant) && IsValid(RightOccupant);
}

bool ABoat::IsReversePending(EBoatSeat Seat) const
{
	const UWorld* W = GetWorld();
	if (!W) return false;

	const FTimerManager& TM = W->GetTimerManager();
	return (Seat == EBoatSeat::Left)
		? TM.IsTimerActive(PendingReverseTimerLeft)
		: (Seat == EBoatSeat::Right)
		? TM.IsTimerActive(PendingReverseTimerRight)
		: false;
}

void ABoat::UpdateBoatOccupancyState_Local()
{
	if (!BoatMesh) return;

	const bool bAnyOccupied = HasAnyOccupant();

	BoatMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoatMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BoatMesh->SetCollisionResponseToChannel(ECC_Pawn, bAnyOccupied ? ECR_Ignore : ECR_Block);
	BoatMesh->SetSimulatePhysics(false);
}

void ABoat::UpdateBoatOccupancyState_Server()
{
	if (!HasAuthority() || !BoatMesh) return;

	const bool bAnyOccupied = HasAnyOccupant();
	const bool bBothOccupied = HasBothOccupants();
	const bool bShouldSimulate = bBothOccupied;

	BoatMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoatMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BoatMesh->SetCollisionResponseToChannel(ECC_Pawn, bAnyOccupied ? ECR_Ignore : ECR_Block);

	const bool bWasSimulating = BoatMesh->IsSimulatingPhysics();
	if (bWasSimulating == bShouldSimulate)
	{
		return;
	}

	if (bShouldSimulate)
	{
		BoatMesh->SetSimulatePhysics(true);

		if (bOverrideMass)
		{
			BoatMesh->SetMassOverrideInKg(NAME_None, MassKg, true);
		}

		BoatMesh->SetPhysicsMaxAngularVelocityInDegrees(MaxAngularDegPerSec);
		BoatMesh->WakeAllRigidBodies();
	}
	else
	{
		BoatMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		BoatMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		BoatMesh->PutRigidBodyToSleep();
		BoatMesh->SetSimulatePhysics(false);
	}

	ForceNetUpdate();
	BroadcastNetState_Server_Immediate();
}

USkeletalMeshComponent* ABoat::GetSeatAvatarComp(EBoatSeat Seat) const
{
	return (Seat == EBoatSeat::Left) ? SeatAvatarLeft :
		(Seat == EBoatSeat::Right) ? SeatAvatarRight : nullptr;
}

const FBoatAvatarDef* ABoat::GetAvatarDef(uint8 AvatarId) const
{
	if (AvatarId == 1) return &HanselAvatar;
	if (AvatarId == 2) return &CookieAvatar;
	return nullptr;
}

UAnimSequenceBase* ABoat::GetIdleAnim(uint8 AvatarId, EBoatSeat Seat) const
{
	const FBoatAvatarDef* Def = GetAvatarDef(AvatarId);
	if (!Def) return nullptr;
	return (Seat == EBoatSeat::Left) ? Def->LeftIdle : (Seat == EBoatSeat::Right) ? Def->RightIdle : nullptr;
}

UAnimSequenceBase* ABoat::GetPaddleAnim(uint8 AvatarId, EBoatSeat Seat) const
{
	const FBoatAvatarDef* Def = GetAvatarDef(AvatarId);
	if (!Def) return nullptr;
	return (Seat == EBoatSeat::Left) ? Def->LeftPaddle : (Seat == EBoatSeat::Right) ? Def->RightPaddle : nullptr;
}

UAnimSequenceBase* ABoat::GetReversePaddleAnim(uint8 AvatarId, EBoatSeat Seat) const
{
	const FBoatAvatarDef* Def = GetAvatarDef(AvatarId);
	if (!Def) return nullptr;

	return (Seat == EBoatSeat::Left)
		? Def->LeftReversePaddle
		: (Seat == EBoatSeat::Right)
		? Def->RightReversePaddle
		: nullptr;
}

uint8 ABoat::ResolveAvatarIdForCharacter(ACharacter* Character) const
{
	if (!IsValid(Character)) return 0;

	if (HanselCharacterClass && Character->IsA(HanselCharacterClass)) return 1;
	if (CookieCharacterClass && Character->IsA(CookieCharacterClass)) return 2;

	const FString ClassName = Character->GetClass()->GetName();
	if (ClassName.Contains(TEXT("Hansel"))) return 1;
	if (ClassName.Contains(TEXT("Cookie"))) return 2;

	if (UGameInstance* GI = GetGameInstance())
	{
		APlayerController* PC = Cast<APlayerController>(Character->GetController());
		const bool bHost = PC && PC->IsLocalController();
		const FName PropName = bHost ? TEXT("P1SelectedCharacter") : TEXT("P2SelectedCharacter");

		if (FProperty* Prop = GI->GetClass()->FindPropertyByName(PropName))
		{
			if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
			{
				const int32 V = IntProp->GetPropertyValue_InContainer(GI);
				return (uint8)FMath::Clamp(V, 0, 255);
			}
		}
	}

	return 0;
}

void ABoat::ApplySeatAvatar_Local(EBoatSeat Seat, uint8 AvatarId)
{
	USkeletalMeshComponent* Comp = GetSeatAvatarComp(Seat);
	if (!Comp) return;

	const FBoatAvatarDef* Def = GetAvatarDef(AvatarId);
	if (!Def || !Def->Mesh)
	{
		Comp->SetHiddenInGame(true, true);
		Comp->SetVisibility(false, true);

		RefreshPaddleProps_Local();
		return;
	}

	Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Comp->SetSkeletalMesh(Def->Mesh);
	Comp->SetHiddenInGame(false, true);
	Comp->SetVisibility(true, true);

	PlaySeatIdle_Local(Seat, AvatarId);
	RefreshPaddleProps_Local();
}

void ABoat::PlaySeatIdle_Local(EBoatSeat Seat, uint8 AvatarId)
{
	USkeletalMeshComponent* Comp = GetSeatAvatarComp(Seat);
	if (!Comp) return;

	UAnimSequenceBase* Idle = GetIdleAnim(AvatarId, Seat);
	if (!Idle) return;

	Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Comp->PlayAnimation(Idle, true);
}

void ABoat::RestoreSeatIdle(EBoatSeat Seat)
{
	const uint8 Id = (Seat == EBoatSeat::Left) ? LeftAvatarId : RightAvatarId;
	PlaySeatIdle_Local(Seat, Id);
}

void ABoat::PlaySeatPaddle_Local(EBoatSeat Seat, uint8 AvatarId, bool bReverse)
{
	USkeletalMeshComponent* Comp = GetSeatAvatarComp(Seat);
	if (!Comp) return;

	UAnimSequenceBase* Paddle = bReverse
		? GetReversePaddleAnim(AvatarId, Seat)
		: GetPaddleAnim(AvatarId, Seat);

	if (!Paddle) return;

	FTimerHandle& Handle = (Seat == EBoatSeat::Left) ? RestoreIdleTimerLeft : RestoreIdleTimerRight;
	GetWorldTimerManager().ClearTimer(Handle);

	Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Comp->PlayAnimation(Paddle, false);

	const float Len = Paddle->GetPlayLength();
	FTimerDelegate Del;
	Del.BindUObject(this, &ABoat::RestoreSeatIdle, Seat);
	GetWorldTimerManager().SetTimer(Handle, Del, FMath::Max(0.01f, Len), false);
}

void ABoat::RefreshPaddleProps_Local()
{
	auto SetupPaddle =
		[this](UStaticMeshComponent* PaddleComp, USkeletalMeshComponent* AvatarComp, uint8 AvatarId, const FName& SocketName)
		{
			if (!PaddleComp) return;

			const bool bCanShow =
				(PaddleMeshAsset != nullptr) &&
				(AvatarId != 0) &&
				(AvatarComp != nullptr) &&
				(AvatarComp->GetSkeletalMeshAsset() != nullptr);

			if (!bCanShow)
			{
				PaddleComp->SetHiddenInGame(true, true);
				PaddleComp->SetVisibility(false, true);
				return;
			}

			if (PaddleComp->GetStaticMesh() != PaddleMeshAsset)
			{
				PaddleComp->SetStaticMesh(PaddleMeshAsset);
			}

			PaddleComp->AttachToComponent(
				AvatarComp,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				SocketName
			);

			PaddleComp->SetRelativeScale3D(PaddleRelativeScale);
			PaddleComp->SetHiddenInGame(false, true);
			PaddleComp->SetVisibility(true, true);
		};

	SetupPaddle(SeatPaddleLeft, SeatAvatarLeft, LeftAvatarId, LeftSeatPaddleSocketName);
	SetupPaddle(SeatPaddleRight, SeatAvatarRight, RightAvatarId, RightSeatPaddleSocketName);
}

void ABoat::Multicast_PlayPaddleSound_Implementation(EBoatSeat Seat)
{
	PlayPaddleSound_Local(Seat);
}

void ABoat::PlayPaddleSound_Local(EBoatSeat Seat) const
{
	if (!PaddleSoundCue)
	{
		return;
	}

	FVector SoundLocation = GetActorLocation();

	if (USceneComponent* SeatComp = GetSeatComponent(Seat))
	{
		SoundLocation = SeatComp->GetComponentLocation();
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		PaddleSoundCue,
		SoundLocation
	);
}

void ABoat::PlayDeathSound_Local() const
{
	if (!DeathSoundCue)
	{
		return;
	}

	FVector SoundLocation = GetActorLocation();
	if (BoatMesh)
	{
		SoundLocation = BoatMesh->GetComponentLocation();
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		DeathSoundCue,
		SoundLocation,
		DeathSoundVolume,
		DeathSoundPitch,
		0.0f,
		DeathSoundAttenuation
	);
}

void ABoat::OnRep_LoosePaddlesHidden()
{
	RefreshLoosePaddles_Local();
}

void ABoat::RefreshLoosePaddles_Local() const
{
	SetLoosePaddleVisible_Local(EBoatSeat::Left, !bLeftLoosePaddleHidden);
	SetLoosePaddleVisible_Local(EBoatSeat::Right, !bRightLoosePaddleHidden);
}

void ABoat::SetLoosePaddleVisible_Local(EBoatSeat Seat, bool bVisible) const
{
	AActor* TargetPaddleActor = nullptr;

	if (Seat == EBoatSeat::Left)
	{
		TargetPaddleActor = LeftLoosePaddleActor;
	}
	else if (Seat == EBoatSeat::Right)
	{
		TargetPaddleActor = RightLoosePaddleActor;
	}

	if (!IsValid(TargetPaddleActor))
	{
		return;
	}

	TargetPaddleActor->SetActorHiddenInGame(!bVisible);
	TargetPaddleActor->SetActorEnableCollision(bVisible);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	TargetPaddleActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Comp : PrimitiveComponents)
	{
		if (!Comp)
		{
			continue;
		}

		if (!bVisible)
		{
			if (Comp->IsSimulatingPhysics())
			{
				Comp->SetSimulatePhysics(false);
				Comp->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Comp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			}
		}

		Comp->SetGenerateOverlapEvents(bVisible);
		Comp->SetCollisionEnabled(
			bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision
		);
	}
}

void ABoat::RequestSeatVisualRefresh()
{
	RefreshSeatVisuals_Local();

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(SeatVisualRefreshTimer);
		W->GetTimerManager().SetTimer(SeatVisualRefreshTimer, this, &ABoat::RefreshSeatVisuals_Local, 0.0f, false);
	}
}

void ABoat::RefreshSeatVisuals_Local()
{
	if (SeatAvatarLeft)
	{
		if (LeftAvatarId != 0) ApplySeatAvatar_Local(EBoatSeat::Left, LeftAvatarId);
		else
		{
			SeatAvatarLeft->SetHiddenInGame(true, true);
			SeatAvatarLeft->SetVisibility(false, true);
		}
	}

	if (SeatAvatarRight)
	{
		if (RightAvatarId != 0) ApplySeatAvatar_Local(EBoatSeat::Right, RightAvatarId);
		else
		{
			SeatAvatarRight->SetHiddenInGame(true, true);
			SeatAvatarRight->SetVisibility(false, true);
		}
	}

	RefreshPaddleProps_Local();
}

bool ABoat::TrySeatCharacter_Server(ACharacter* Character, EBoatSeat Seat)
{
	if (!HasAuthority() || !IsValid(Character)) return false;
	if (Seat == EBoatSeat::None) return false;

	if (Character == LeftOccupant && Seat == EBoatSeat::Left) return true;
	if (Character == RightOccupant && Seat == EBoatSeat::Right) return true;

	if (Seat == EBoatSeat::Left)
	{
		if (IsValid(LeftOccupant)) return false;
		LeftOccupant = Character;
		LeftAvatarId = ResolveAvatarIdForCharacter(Character);

		// 왼쪽 좌석에 탑승했으니까 바닥에 떨어져 있던 왼쪽 노 숨김
		bLeftLoosePaddleHidden = true;
	}
	else
	{
		if (IsValid(RightOccupant)) return false;
		RightOccupant = Character;
		RightAvatarId = ResolveAvatarIdForCharacter(Character);

		// 오른쪽 좌석에 탑승했으니까 바닥에 떨어져 있던 오른쪽 노 숨김
		bRightLoosePaddleHidden = true;
	}

	// 서버 / 리슨서버 화면에서도 즉시 숨김
	RefreshLoosePaddles_Local();

	UpdateBoatOccupancyState_Server();

	SeatCharacter_Server(Character, Seat);
	RefreshSeatVisuals_Local();

	ForceNetUpdate();

	return true;
}

void ABoat::HandleBeginOverlapLeft(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	ACharacter* Char = Cast<ACharacter>(OtherActor);
	if (!IsValid(Char)) return;

	if (HasAuthority())
	{
		TrySeatCharacter_Server(Char, EBoatSeat::Left);
		return;
	}

	if (Char->IsLocallyControlled())
	{
		if (ABoatPlayerController* BPC = Cast<ABoatPlayerController>(Char->GetController()))
		{
			BPC->Server_RequestBoard(this, EBoatSeat::Left);
		}
	}
}

void ABoat::HandleBeginOverlapRight(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	ACharacter* Char = Cast<ACharacter>(OtherActor);
	if (!IsValid(Char)) return;

	if (HasAuthority())
	{
		TrySeatCharacter_Server(Char, EBoatSeat::Right);
		return;
	}

	if (Char->IsLocallyControlled())
	{
		if (ABoatPlayerController* BPC = Cast<ABoatPlayerController>(Char->GetController()))
		{
			BPC->Server_RequestBoard(this, EBoatSeat::Right);
		}
	}
}

void ABoat::TeleportCharacterToSeat_NoAttach(ACharacter* Character, EBoatSeat Seat) const
{
	if (!IsValid(Character)) return;

	if (USceneComponent* SeatComp = GetSeatComponent(Seat))
	{
		const FVector Loc = SeatComp->GetComponentLocation();
		const FRotator Rot = SeatComp->GetComponentRotation();
		Character->SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ABoat::SeatCharacter_Server(ACharacter* Character, EBoatSeat Seat)
{
	if (!HasAuthority() || !IsValid(Character)) return;

	TeleportCharacterToSeat_NoAttach(Character, Seat);
	Character->ForceNetUpdate();

	if (ABoatPlayerController* BPC = Cast<ABoatPlayerController>(Character->GetController()))
	{
		BPC->ForceBoardOnServer(this, Seat);
	}

	const uint8 AvatarId = (Seat == EBoatSeat::Left) ? LeftAvatarId : RightAvatarId;
	Multicast_OnSeated(Character, Seat, AvatarId);
}

void ABoat::Multicast_OnSeated_Implementation(ACharacter* Character, EBoatSeat Seat, uint8 AvatarId)
{
	if (!IsValid(Character)) return;

	TeleportCharacterToSeat_NoAttach(Character, Seat);
	ApplySeatedState_Local(Character, Seat);

	// 자기 컴퓨터의 로컬 캐릭터가 서버에서 탑승 승인을 받은 순간부터
	// 카메라 전환 시간 동안 자유 시점 마우스 입력을 잠근다.
	if (Character->IsLocallyControlled())
	{
		// 중요: 이 값은 복제하지 않는다.
		// 호스트/각 클라이언트가 자기 탑승 상태만 따로 기억한다.
		bLocalPlayerBoardedForBGM_Local = true;

		// 두 번째 플레이어를 기다리지 않고 이 컴퓨터에서 바로 BGM을 시작한다.
		// UpdateBoatBGM_Local() 내부에서 이미 재생 중인지 검사하므로 중복 재생되지 않는다.
		UpdateBoatBGM_Local();

		if (GetWorld())
		{
			FreeLookInputUnlockTime_Local =
				GetWorld()->GetTimeSeconds()
				+ FMath::Max(0.0f, FreeLookInputLockDurationAfterBoard);

			// 전환 중에는 BP_Boat에 설정된 기존 카메라 각도를 그대로 사용.
			ResetFreeLookCamera_Local();
		}
	}

	if (AvatarId != 0) ApplySeatAvatar_Local(Seat, AvatarId);
}

void ABoat::ApplySeatedState_Local(ACharacter* Character, EBoatSeat Seat) const
{
	if (!IsValid(Character)) return;

	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetGenerateOverlapEvents(false);
		Capsule->SetNotifyRigidBodyCollision(false);
	}

	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetNotifyRigidBodyCollision(false);
	}

	Character->SetActorEnableCollision(false);

	if (bHideCharacterWhenSeated)
	{
		Character->SetActorHiddenInGame(true);

		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			Mesh->SetVisibility(false, true);
			Mesh->SetHiddenInGame(true, true);
		}
	}

	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
		Move->DisableMovement();
		Move->SetMovementMode(MOVE_None);
		Move->bEnablePhysicsInteraction = false;
		Move->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
	}

	if (Character->IsLocallyControlled())
	{
		if (!Cast<ABoatPlayerController>(Character->GetController()))
		{
			TrySetBoatView_Local(Character);
		}
	}
}

void ABoat::TrySetBoatView_Local(ACharacter* Character) const
{
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC) return;

	PC->bAutoManageActiveCameraTarget = false;
	if (PC->GetViewTarget() == this) return;

	PC->SetViewTargetWithBlend(const_cast<ABoat*>(this), 1.0f, VTBlend_EaseInOut, 2.0f, false);
}

void ABoat::OnRep_Occupants()
{
	UpdateBoatOccupancyState_Local();
	RequestSeatVisualRefresh();
}

void ABoat::OnRep_AvatarIds()
{
	RequestSeatVisualRefresh();
}

EBoatSeat ABoat::GetSeatForRower(ACharacter* Rower) const
{
	if (!IsValid(Rower)) return EBoatSeat::None;
	if (Rower == LeftOccupant) return EBoatSeat::Left;
	if (Rower == RightOccupant) return EBoatSeat::Right;
	return EBoatSeat::None;
}

void ABoat::PaddleFromRower(ACharacter* Rower)
{
	if (!HasAuthority() || !IsValid(Rower)) return;

	const EBoatSeat Seat = GetSeatForRower(Rower);
	if (Seat == EBoatSeat::None) return;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	if (PaddleCooldown > 0.f)
	{
		if (Seat == EBoatSeat::Left && (Now - LastPaddleTimeLeft) < PaddleCooldown) return;
		if (Seat == EBoatSeat::Right && (Now - LastPaddleTimeRight) < PaddleCooldown) return;
	}

	if (Seat == EBoatSeat::Left) LastPaddleTimeLeft = Now;
	else LastPaddleTimeRight = Now;

	const uint8 AvatarId = (Seat == EBoatSeat::Left) ? LeftAvatarId : RightAvatarId;
	if (AvatarId != 0) Multicast_PlaySeatPaddle(Seat, AvatarId, false);

	// 두 명이 모두 탑승했을 때만 실제 이동과 사운드 실행
	if (!HasBothOccupants()) return;

	// 좌클릭 패들링은 바로 소리 재생
	Multicast_PlayPaddleSound(Seat);

	ApplyPaddle_Server(Seat, false);

	if (bUseCustomNetSmoothing) BroadcastNetState_Server_Immediate();
}

void ABoat::ExecuteReversePaddle_Server(EBoatSeat Seat)
{
	if (!HasAuthority()) return;
	if (!BoatMesh || !BoatMesh->IsSimulatingPhysics()) return;
	if (!HasBothOccupants()) return;

	if (Seat == EBoatSeat::Left)
	{
		GetWorldTimerManager().ClearTimer(PendingReverseTimerLeft);
		if (!IsValid(LeftOccupant)) return;
	}
	else if (Seat == EBoatSeat::Right)
	{
		GetWorldTimerManager().ClearTimer(PendingReverseTimerRight);
		if (!IsValid(RightOccupant)) return;
	}
	else
	{
		return;
	}

	// 우클릭 역패들링은 ReversePaddleDelay 지난 뒤,
	// 실제 뒤로 젓는 순간에 소리 재생
	Multicast_PlayPaddleSound(Seat);

	ApplyPaddle_Server(Seat, true);

	if (bUseCustomNetSmoothing) BroadcastNetState_Server_Immediate();
}

void ABoat::ReversePaddleFromRower(ACharacter* Rower)
{
	if (!HasAuthority() || !IsValid(Rower)) return;

	const EBoatSeat Seat = GetSeatForRower(Rower);
	if (Seat == EBoatSeat::None) return;

	if (IsReversePending(Seat)) return;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	if (PaddleCooldown > 0.f)
	{
		if (Seat == EBoatSeat::Left && (Now - LastPaddleTimeLeft) < PaddleCooldown) return;
		if (Seat == EBoatSeat::Right && (Now - LastPaddleTimeRight) < PaddleCooldown) return;
	}

	if (Seat == EBoatSeat::Left) LastPaddleTimeLeft = Now;
	else LastPaddleTimeRight = Now;

	const uint8 AvatarId = (Seat == EBoatSeat::Left) ? LeftAvatarId : RightAvatarId;
	if (AvatarId != 0) Multicast_PlaySeatPaddle(Seat, AvatarId, true);

	if (!HasBothOccupants()) return;

	const float Delay = FMath::Max(0.f, ReversePaddleDelay);

	if (Delay <= KINDA_SMALL_NUMBER)
	{
		ExecuteReversePaddle_Server(Seat);
		return;
	}

	FTimerDelegate Del;
	Del.BindUObject(this, &ABoat::ExecuteReversePaddle_Server, Seat);

	if (Seat == EBoatSeat::Left)
	{
		GetWorldTimerManager().SetTimer(PendingReverseTimerLeft, Del, Delay, false);
	}
	else
	{
		GetWorldTimerManager().SetTimer(PendingReverseTimerRight, Del, Delay, false);
	}
}

void ABoat::Multicast_PlaySeatPaddle_Implementation(EBoatSeat Seat, uint8 AvatarId, bool bReverse)
{
	PlaySeatPaddle_Local(Seat, AvatarId, bReverse);
}

void ABoat::ApplyPaddle_Server(EBoatSeat Seat, bool bReverse)
{
	if (!BoatMesh || !BoatMesh->IsSimulatingPhysics()) return;

	const float SeatSign = (Seat == EBoatSeat::Left) ? +1.f : -1.f;

	const FVector Forward = BoatMesh->GetForwardVector();
	const FVector Dir = FRotator(0.f, YawOffsetDeg * SeatSign, 0.f).RotateVector(Forward);

	const float MoveSign = bReverse ? -0.75f : +1.f;
	const float TurnSign = bReverse ? -1.f : +1.f;

	BoatMesh->AddImpulse(Dir * PaddleImpulse * MoveSign, NAME_None, false);
	BoatMesh->AddAngularImpulseInRadians(
		FVector(0.f, 0.f, TurnAngularImpulse * SeatSign * TurnSign),
		NAME_None,
		false
	);

	ClampVelocities_Server();
}

void ABoat::ClampVelocities_Server() const
{
	if (!HasAuthority() || !BoatMesh) return;

	const FVector V = BoatMesh->GetPhysicsLinearVelocity();
	const float Speed = V.Size();
	if (Speed > MaxSpeed)
	{
		BoatMesh->SetPhysicsLinearVelocity(V.GetSafeNormal() * MaxSpeed);
	}

	BoatMesh->SetPhysicsMaxAngularVelocityInDegrees(MaxAngularDegPerSec);
}

void ABoat::LockAllRowersInput_Server(float DurationSeconds)
{
	if (!HasAuthority()) return;

	auto LockOne = [DurationSeconds](ACharacter* Char)
		{
			if (!IsValid(Char)) return;
			if (ABoatPlayerController* PC = Cast<ABoatPlayerController>(Char->GetController()))
			{
				PC->LockBoatInput(DurationSeconds);
			}
		};

	LockOne(LeftOccupant);
	LockOne(RightOccupant);
}

void ABoat::SaveRespawnPoint_Server(const FTransform& NewRespawnTransform, int32 NewSaveOrder)
{
	if (!HasAuthority())
	{
		return;
	}

	if (NewSaveOrder != INDEX_NONE &&
		SavedRespawnOrder != INDEX_NONE &&
		NewSaveOrder < SavedRespawnOrder)
	{
		return;
	}

	SavedRespawnTransform = NewRespawnTransform;
	bHasSavedRespawnTransform = true;

	if (NewSaveOrder != INDEX_NONE)
	{
		SavedRespawnOrder = NewSaveOrder;
	}
}

void ABoat::RespawnToSavedPoint_Server(float InputLockSeconds, bool bZeroVelocity)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bHasSavedRespawnTransform)
	{
		SavedRespawnTransform = GetActorTransform();
		bHasSavedRespawnTransform = true;
	}

	if (InputLockSeconds > KINDA_SMALL_NUMBER)
	{
		LockAllRowersInput_Server(InputLockSeconds);
	}

	if (BoatMesh && BoatMesh->IsSimulatingPhysics() && bZeroVelocity)
	{
		BoatMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		BoatMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	SetActorLocationAndRotation(
		SavedRespawnTransform.GetLocation(),
		SavedRespawnTransform.GetRotation().Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	if (BoatMesh && BoatMesh->IsSimulatingPhysics() && bZeroVelocity)
	{
		BoatMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		BoatMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		BoatMesh->WakeAllRigidBodies();
	}

	if (IsValid(LeftOccupant))
	{
		TeleportCharacterToSeat_NoAttach(LeftOccupant, EBoatSeat::Left);
		LeftOccupant->ForceNetUpdate();
	}

	if (IsValid(RightOccupant))
	{
		TeleportCharacterToSeat_NoAttach(RightOccupant, EBoatSeat::Right);
		RightOccupant->ForceNetUpdate();
	}

	ForceNetUpdate();

	if (bUseCustomNetSmoothing)
	{
		BroadcastNetState_Server_Immediate();
	}
}

void ABoat::StartDeathRespawnSequence_Server(float DeathDuration, float PostRespawnInputLockSeconds, bool bZeroVelocity)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bDeathSequenceActive)
	{
		return;
	}

	bDeathSequenceActive = true;

	const float UseDeathDuration = FMath::Max(
		0.05f,
		(DeathDuration > 0.0f) ? DeathDuration : DefaultDeathSequenceDuration
	);

	const float TotalLockDuration = UseDeathDuration + FMath::Max(0.0f, PostRespawnInputLockSeconds);
	if (TotalLockDuration > KINDA_SMALL_NUMBER)
	{
		LockAllRowersInput_Server(TotalLockDuration);
	}

	GetWorldTimerManager().ClearTimer(PendingReverseTimerLeft);
	GetWorldTimerManager().ClearTimer(PendingReverseTimerRight);

	if (BoatMesh)
	{
		if (bZeroVelocity)
		{
			BoatMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			BoatMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}

		BoatMesh->PutRigidBodyToSleep();
		BoatMesh->SetSimulatePhysics(false);
	}

	ForceNetUpdate();
	BroadcastNetState_Server_Immediate();

	Multicast_BeginDeathSequence(UseDeathDuration);

	FTimerDelegate Del;
	Del.BindUObject(this, &ABoat::FinishDeathRespawnSequence_Server, PostRespawnInputLockSeconds, bZeroVelocity);

	GetWorldTimerManager().ClearTimer(DeathRespawnTimer);
	GetWorldTimerManager().SetTimer(DeathRespawnTimer, Del, UseDeathDuration, false);
}

void ABoat::FinishDeathRespawnSequence_Server(float PostRespawnInputLockSeconds, bool bZeroVelocity)
{
	PostRespawnInputLockSeconds = FMath::Max(0.0f, PostRespawnInputLockSeconds);

	RespawnToSavedPoint_Server(0.0f, bZeroVelocity);

	UpdateBoatOccupancyState_Server();

	Multicast_EndDeathSequence();

	bDeathSequenceActive = false;

	ForceNetUpdate();
	BroadcastNetState_Server_Immediate();
}