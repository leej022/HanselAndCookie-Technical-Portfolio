#include "TwoDRopeLiftBar.h"

#include "CableComponent.h"

#include "Chaos/ChaosEngineInterface.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

ATwoDRopeLiftBar::ATwoDRopeLiftBar()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	bAlwaysRelevant = true;

	/*
	 * Generic Actor movement replication은 클라이언트에 Transform을
	 * 스냅 적용해서 뚝뚝 끊긴다. 아래 custom snapshot 보간을 사용한다.
	 */
	SetReplicateMovement(false);

	NetUpdateFrequency = 30.0f;
	MinNetUpdateFrequency = 15.0f;

	/* -------------------- Platform -------------------- */

	BarBody = CreateDefaultSubobject<UBoxComponent>(
		TEXT("BarBody")
	);
	SetRootComponent(BarBody);

	BarBody->SetMobility(
		EComponentMobility::Movable
	);

	BarBody->BodyInstance.DOFMode =
		EDOFMode::Default;
	BarBody->BodyInstance.bLockXTranslation = false;
	BarBody->BodyInstance.bLockYTranslation = false;
	BarBody->BodyInstance.bLockZTranslation = false;
	BarBody->BodyInstance.bLockXRotation = false;
	BarBody->BodyInstance.bLockYRotation = false;
	BarBody->BodyInstance.bLockZRotation = false;

	BarBody->SetSimulatePhysics(false);

	/*
	 * 액터 전체를 월드에서 45도 회전해도 발판이 경사 방향으로
	 * 흘러내리지 않도록 BarBody의 월드 중력은 끈다.
	 *
	 * 공은 별도 액터이므로 월드 중력을 그대로 받아 발판 위에서 굴러간다.
	 */
	BarBody->SetEnableGravity(false);
	BarBody->SetUseCCD(true);
	BarBody->SetIsReplicated(false);

	BarBody->SetCollisionEnabled(
		ECollisionEnabled::QueryAndPhysics
	);
	BarBody->SetCollisionObjectType(
		ECC_WorldDynamic
	);
	BarBody->SetCollisionResponseToAllChannels(
		ECR_Block
	);
	BarBody->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Block
	);

	BarVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("BarVisual")
	);
	BarVisual->SetupAttachment(BarBody);
	BarVisual->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);

	LeftBarEnd = CreateDefaultSubobject<USceneComponent>(
		TEXT("LeftBarEnd")
	);
	LeftBarEnd->SetupAttachment(BarBody);

	RightBarEnd = CreateDefaultSubobject<USceneComponent>(
		TEXT("RightBarEnd")
	);
	RightBarEnd->SetupAttachment(BarBody);

	/* -------------------- Fixed pulley frame -------------------- */

	LeftPulleyAnchor =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("LeftPulleyAnchor")
		);
	LeftPulleyAnchor->SetupAttachment(BarBody);

	RightPulleyAnchor =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("RightPulleyAnchor")
		);
	RightPulleyAnchor->SetupAttachment(BarBody);

	LeftPulleyBody =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("LeftPulleyBody")
		);
	LeftPulleyBody->SetupAttachment(LeftPulleyAnchor);

	RightPulleyBody =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("RightPulleyBody")
		);
	RightPulleyBody->SetupAttachment(RightPulleyAnchor);

	PulleyRodVisual =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("PulleyRodVisual")
		);
	PulleyRodVisual->SetupAttachment(BarBody);
	PulleyRodVisual->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);

	LeftInitialHandlePoint =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("LeftInitialHandlePoint")
		);
	LeftInitialHandlePoint->SetupAttachment(
		LeftPulleyAnchor
	);

	RightInitialHandlePoint =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("RightInitialHandlePoint")
		);
	RightInitialHandlePoint->SetupAttachment(
		RightPulleyAnchor
	);

	/* -------------------- Handles -------------------- */

	LeftHandleBody =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("LeftHandleBody")
		);
	LeftHandleBody->SetupAttachment(
		LeftInitialHandlePoint
	);
	LeftHandleBody->SetIsReplicated(false);

	RightHandleBody =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("RightHandleBody")
		);
	RightHandleBody->SetupAttachment(
		RightInitialHandlePoint
	);
	RightHandleBody->SetIsReplicated(false);

	LeftHandleVisual =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("LeftHandleVisual")
		);
	LeftHandleVisual->SetupAttachment(LeftHandleBody);
	LeftHandleVisual->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);

	RightHandleVisual =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("RightHandleVisual")
		);
	RightHandleVisual->SetupAttachment(RightHandleBody);
	RightHandleVisual->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);

	/* -------------------- Physics handles -------------------- */

	LeftGrabPhysicsHandle =
		CreateDefaultSubobject<UPhysicsHandleComponent>(
			TEXT("LeftGrabPhysicsHandle")
		);

	RightGrabPhysicsHandle =
		CreateDefaultSubobject<UPhysicsHandleComponent>(
			TEXT("RightGrabPhysicsHandle")
		);

	/* -------------------- Cables -------------------- */

	LeftCable = CreateDefaultSubobject<UCableComponent>(
		TEXT("LeftCable")
	);
	LeftCable->SetupAttachment(LeftPulleyAnchor);

	RightCable = CreateDefaultSubobject<UCableComponent>(
		TEXT("RightCable")
	);
	RightCable->SetupAttachment(RightPulleyAnchor);

	LeftLiftCable =
		CreateDefaultSubobject<UCableComponent>(
			TEXT("LeftLiftCable")
		);
	LeftLiftCable->SetupAttachment(LeftBarEnd);

	RightLiftCable =
		CreateDefaultSubobject<UCableComponent>(
			TEXT("RightLiftCable")
		);
	RightLiftCable->SetupAttachment(RightBarEnd);

	ConfigurePulleyBody(LeftPulleyBody);
	ConfigurePulleyBody(RightPulleyBody);

	ConfigureHandleBody(LeftHandleBody);
	ConfigureHandleBody(RightHandleBody);

	ConfigurePhysicsHandle(LeftGrabPhysicsHandle);
	ConfigurePhysicsHandle(RightGrabPhysicsHandle);

	ConfigureCable(LeftCable);
	ConfigureCable(RightCable);
	ConfigureCable(LeftLiftCable);
	ConfigureCable(RightLiftCable);
}

void ATwoDRopeLiftBar::BeginPlay()
{
	Super::BeginPlay();

	/* 기존 BP에 Replicate Movement가 저장돼 있어도 런타임에서 끈다. */
	SetReplicateMovement(false);

	ClearBuiltInDOFConstraintSettings();
	ConfigureBarPhysics();

	ConfigurePulleyBody(LeftPulleyBody);
	ConfigurePulleyBody(RightPulleyBody);

	ConfigureHandleBody(LeftHandleBody);
	ConfigureHandleBody(RightHandleBody);

	ConfigurePhysicsHandle(LeftGrabPhysicsHandle);
	ConfigurePhysicsHandle(RightGrabPhysicsHandle);

	ConfigureCable(LeftCable);
	ConfigureCable(RightCable);
	ConfigureCable(LeftLiftCable);
	ConfigureCable(RightLiftCable);

	/*
	 * BarBody가 움직이기 전에 액터의 최초 로컬 기준틀을 저장한다.
	 */
	CaptureMechanismFrame();
	FreezePulleyFrameInWorld();

	DetachHandleAndEnablePhysics(
		LeftHandleBody
	);
	DetachHandleAndEnablePhysics(
		RightHandleBody
	);

	/*
	 * 클라이언트도 Cable 표시와 안전한 기본값을 위해
	 * 현재 실제 거리를 저장한다.
	 * 서버 값이 authoritative하다.
	 */
	LeftInitialLiftLength =
		FVector::Distance(
			LeftPulleyAnchor->GetComponentLocation(),
			LeftBarEnd->GetComponentLocation()
		);

	RightInitialLiftLength =
		FVector::Distance(
			RightPulleyAnchor->GetComponentLocation(),
			RightBarEnd->GetComponentLocation()
		);

	LeftInitialHandleLength =
		FVector::Distance(
			LeftPulleyAnchor->GetComponentLocation(),
			LeftHandleBody->GetComponentLocation()
		);

	RightInitialHandleLength =
		FVector::Distance(
			RightPulleyAnchor->GetComponentLocation(),
			RightHandleBody->GetComponentLocation()
		);

	/*
	 * 중요:
	 * 게임 시작 시 배치된 막대기 위치 자체가 최대 하강 위치다.
	 * 바닥 Trace나 추가 하강 거리를 사용하지 않는다.
	 */
	LeftMaximumLoweredLiftLength =
		LeftInitialLiftLength;

	RightMaximumLoweredLiftLength =
		RightInitialLiftLength;

	const float MinimumTotalLength =
		FMath::Max(
			MinimumRopeSegmentLength * 2.0f,
			20.0f
		);

	if (HasAuthority())
	{
		/*
		 * 설정값 PulleyHeight/InitialHandleDrop을 더해서 추정하지 않고,
		 * Freeze/Detach가 끝난 뒤 실제 월드 거리를 저장한다.
		 *
		 * 이 값이 줄을 풀었을 때의 정확한 복귀 위치다.
		 */
		const float AutomaticLeftLength =
			FMath::Max(
				LeftMaximumLoweredLiftLength +
				LeftInitialHandleLength +
				GetEffectiveRopeLengthExtraSlack(),
				MinimumTotalLength
			);

		const float AutomaticRightLength =
			FMath::Max(
				RightMaximumLoweredLiftLength +
				RightInitialHandleLength +
				GetEffectiveRopeLengthExtraSlack(),
				MinimumTotalLength
			);

		LeftRopeLength =
			FixedRopeLengthOverride > 0.0f
			? FMath::Max(
				FixedRopeLengthOverride *
				VerticalHeightScale,
				MinimumTotalLength
			)
			: AutomaticLeftLength;

		RightRopeLength =
			FixedRopeLengthOverride > 0.0f
			? FMath::Max(
				FixedRopeLengthOverride *
				VerticalHeightScale,
				MinimumTotalLength
			)
			: AutomaticRightLength;

		BarBody->SetSimulatePhysics(false);
		LeftHandleBody->SetSimulatePhysics(true);
		RightHandleBody->SetSimulatePhysics(true);

		BarBody->WakeAllRigidBodies();
		LeftHandleBody->WakeAllRigidBodies();
		RightHandleBody->WakeAllRigidBodies();

		ForceNetUpdate();
	}
	else
	{
		/*
		 * 서버 물리만 사용한다.
		 */
		BarBody->SetSimulatePhysics(false);
		LeftHandleBody->SetSimulatePhysics(false);
		RightHandleBody->SetSimulatePhysics(false);
	}

	/*
	 * 첫 네트워크 패킷 전에도 현재 배치 위치를 유효한 보간 목표로 사용한다.
	 */
	RepBarWorldLocation = BarBody->GetComponentLocation();
	RepBarWorldRotation = BarBody->GetComponentRotation();
	RepLeftHandleWorldLocation = LeftHandleBody->GetComponentLocation();
	RepLeftHandleWorldRotation = LeftHandleBody->GetComponentRotation();
	RepRightHandleWorldLocation = RightHandleBody->GetComponentLocation();
	RepRightHandleWorldRotation = RightHandleBody->GetComponentRotation();

	bHasReceivedNetworkVisualState = true;

	if (HasAuthority())
	{
		UpdateServerNetworkVisualState(true);
	}

	RefreshLiftCableBindings();
	RefreshCableBindings();
	UpdateCableLengths();
}

void ATwoDRopeLiftBar::OnConstruction(
	const FTransform& Transform
)
{
	Super::OnConstruction(Transform);

	if (!IsValid(BarBody))
	{
		return;
	}

	ClearBuiltInDOFConstraintSettings();

	const float EffectiveBarHalfLength =
		GetEffectiveBarHalfLength();

	const float EffectivePulleyHalfSpacing =
		GetEffectivePulleyAnchorHalfSpacing();

	const FVector DesiredBarHalfExtent(
		EffectiveBarHalfLength,
		BarHalfWidth,
		BarHalfThickness
	);

	BarBody->SetBoxExtent(
		DesiredBarHalfExtent
	);

	ScaleVisualToHalfExtent(
		BarVisual,
		DesiredBarHalfExtent
	);

	LeftBarEnd->SetRelativeLocation(
		FVector(
			-EffectiveBarHalfLength,
			0.0f,
			0.0f
		)
	);

	RightBarEnd->SetRelativeLocation(
		FVector(
			EffectiveBarHalfLength,
			0.0f,
			0.0f
		)
	);

	LeftPulleyAnchor->SetRelativeLocation(
		FVector(
			-EffectivePulleyHalfSpacing,
			PulleyDepthOffset,
			GetEffectivePulleyHeight()
		)
	);

	RightPulleyAnchor->SetRelativeLocation(
		FVector(
			EffectivePulleyHalfSpacing,
			PulleyDepthOffset,
			GetEffectivePulleyHeight()
		)
	);

	/*
	 * 손잡이는 도르래 바로 아래 고정점이 아니라
	 * 조금 더 길고 앞쪽으로 빠진 위치에서 시작한다.
	 *
	 * BeginPlay 이후에는 이 점으로 돌아오지 않고
	 * 실제 물리 손잡이가 자유롭게 떨어지고 흔들린다.
	 */
	LeftInitialHandlePoint->SetRelativeLocation(
		GetFreeHandleInitialLocalOffset()
	);

	RightInitialHandlePoint->SetRelativeLocation(
		GetFreeHandleInitialLocalOffset()
	);

	if (!HasActorBegunPlay())
	{
		if (
			LeftHandleBody->GetAttachParent() !=
			LeftInitialHandlePoint
			)
		{
			LeftHandleBody->AttachToComponent(
				LeftInitialHandlePoint,
				FAttachmentTransformRules::
				SnapToTargetNotIncludingScale
			);
		}

		if (
			RightHandleBody->GetAttachParent() !=
			RightInitialHandlePoint
			)
		{
			RightHandleBody->AttachToComponent(
				RightInitialHandlePoint,
				FAttachmentTransformRules::
				SnapToTargetNotIncludingScale
			);
		}

		LeftHandleBody->SetRelativeLocation(
			FVector::ZeroVector
		);
		RightHandleBody->SetRelativeLocation(
			FVector::ZeroVector
		);
	}


	ConfigurePulleyBody(LeftPulleyBody);
	ConfigurePulleyBody(RightPulleyBody);

	ConfigureHandleBody(LeftHandleBody);
	ConfigureHandleBody(RightHandleBody);

	ConfigurePhysicsHandle(LeftGrabPhysicsHandle);
	ConfigurePhysicsHandle(RightGrabPhysicsHandle);

	ConfigureCable(LeftCable);
	ConfigureCable(RightCable);
	ConfigureCable(LeftLiftCable);
	ConfigureCable(RightLiftCable);

	UpdatePulleyRodPreview();
	RefreshLiftCableBindings();
	RefreshCableBindings();

	LeftLiftCable->CableLength =
		FMath::Max(
			GetEffectivePulleyHeight() *
			CableLengthSlackScale,
			10.0f
		);

	RightLiftCable->CableLength =
		FMath::Max(
			GetEffectivePulleyHeight() *
			CableLengthSlackScale,
			10.0f
		);

	LeftCable->CableLength =
		FMath::Max(
			GetFreeHandleRopeLength() *
			CableLengthSlackScale,
			10.0f
		);

	RightCable->CableLength =
		FMath::Max(
			GetFreeHandleRopeLength() *
			CableLengthSlackScale,
			10.0f
		);
}

void ATwoDRopeLiftBar::Tick(
	float DeltaSeconds
)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		CleanupInvalidHolders();
		UpdateHeldHandleTargets();
		UpdateLooseHandles(DeltaSeconds);

		/*
		 * 양끝 높이 제어와 중심 복귀는 고정된 로컬 기준틀에서 수행한다.
		 */
		UpdatePulleyController(DeltaSeconds);

		/* ForceNetUpdate를 매 Tick 호출하지 않고 일반 NetUpdate 주기로 전송한다. */
		UpdateServerNetworkVisualState(false);
	}
	else
	{
		UpdateClientNetworkSmoothing(DeltaSeconds);
	}

	UpdateCableLengths();
}

bool ATwoDRopeLiftBar::IsReadyForPushBallTravel() const
{
	if (
		!bMechanismFrameCaptured ||
		!IsValid(BarBody)
		)
	{
		return false;
	}

	/*
	 * 실제 막대기가 아직 처음 상태에 있어도, 같은 프레임에 줄 당김 목표가
	 * 이미 생겼다면 곧 올라가기 시작할 상태이므로 준비 완료로 보지 않는다.
	 *
	 * 줄을 잡고만 있고 이동하지 않은 경우 CalculatePullAlpha()는 0이므로
	 * 정상적으로 준비 완료가 될 수 있다. 줄을 놓은 경우도 0이다.
	 */
	const float SafePullAlphaTolerance =
		FMath::Clamp(
			BallTravelReadyPullAlphaTolerance,
			0.0f,
			1.0f
		);

	if (
		CalculatePullAlpha(ETwoDRopeSide::Left) >
		SafePullAlphaTolerance ||
		CalculatePullAlpha(ETwoDRopeSide::Right) >
		SafePullAlphaTolerance
		)
	{
		return false;
	}

	const float SafeLiftTolerance =
		FMath::Max(
			BallTravelReadyLiftHeightTolerance,
			0.0f
		);

	if (
		CurrentLeftLiftHeight > SafeLiftTolerance ||
		CurrentRightLiftHeight > SafeLiftTolerance
		)
	{
		return false;
	}

	const FVector CurrentBarCenterLocal =
		MechanismFrameTransform.
		InverseTransformPosition(
			BarBody->GetComponentLocation()
		);

	const float SafePositionTolerance =
		FMath::Max(
			BallTravelReadyPositionTolerance,
			0.0f
		);

	if (!CurrentBarCenterLocal.Equals(
		InitialBarCenterLocal,
		SafePositionTolerance
	))
	{
		return false;
	}

	const float RotationErrorRadians =
		BarBody->GetComponentQuat().AngularDistance(
			MechanismFrameTransform.GetRotation()
		);

	const float RotationErrorDegrees =
		FMath::RadiansToDegrees(
			RotationErrorRadians
		);

	return RotationErrorDegrees <=
		FMath::Max(
			BallTravelReadyRotationToleranceDegrees,
			0.0f
		);
}

void ATwoDRopeLiftBar::ConfigureBarPhysics()
{
	if (!IsValid(BarBody))
	{
		return;
	}

	/*
	 * v12 핵심:
	 * 막대기는 Chaos 힘으로 움직이지 않는다.
	 * 서버가 계산한 목표 위치/회전으로 직접 이동하는 Kinematic 발판이다.
	 * 따라서 Sleep, Mass, 현재 기울기, 다른 쪽 힘에 영향을 받지 않는다.
	 */
	BarBody->SetMobility(EComponentMobility::Movable);
	BarBody->SetIsReplicated(false);
	BarBody->SetSimulatePhysics(false);
	BarBody->SetEnableGravity(false);
	BarBody->SetUseCCD(true);

	BarBody->SetCollisionEnabled(
		ECollisionEnabled::QueryAndPhysics
	);
	BarBody->SetCollisionObjectType(
		ECC_WorldDynamic
	);
	BarBody->SetCollisionResponseToAllChannels(
		ECR_Block
	);
	BarBody->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Block
	);
	BarBody->SetGenerateOverlapEvents(false);
}

void ATwoDRopeLiftBar::ConfigureHandleBody(
	USphereComponent* HandleBody
) const
{
	if (!IsValid(HandleBody))
	{
		return;
	}

	HandleBody->BodyInstance.bStartAwake = true;
	HandleBody->BodyInstance.SleepFamily =
		ESleepFamily::Custom;
	HandleBody->BodyInstance.
		CustomSleepThresholdMultiplier = 0.0f;

	HandleBody->SetMobility(
		EComponentMobility::Movable
	);
	HandleBody->SetIsReplicated(false);

	HandleBody->SetSphereRadius(
		HandleCollisionRadius,
		true
	);

	HandleBody->SetCollisionEnabled(
		ECollisionEnabled::QueryAndPhysics
	);
	HandleBody->SetCollisionObjectType(
		ECC_PhysicsBody
	);
	HandleBody->SetCollisionResponseToAllChannels(
		ECR_Block
	);
	HandleBody->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Ignore
	);
	HandleBody->SetCollisionResponseToChannel(
		ECC_Camera,
		ECR_Ignore
	);

	/*
	 * ConfigureHandleBody()도 위에서 모든 채널을 Block으로 초기화한다.
	 * 따라서 BP에서 LeftHandleBody/RightHandleBody의 Mantle을 Ignore로
	 * 바꿔도 Construction 또는 BeginPlay 때 Block으로 다시 덮어써졌다.
	 *
	 * 마지막 단계에서 Mantle Trace Channel을 명시적으로 Ignore해
	 * 두 손잡이 몸체 모두 파쿠르 Trace 대상에서 제외한다.
	 */
	if (bIgnoreMantleTrace)
	{
		HandleBody->SetCollisionResponseToChannel(
			MantleTraceChannel,
			ECR_Ignore
		);
	}

	HandleBody->SetUseCCD(true);
	HandleBody->SetEnableGravity(false);

	HandleBody->SetLinearDamping(
		HandleLinearDamping
	);
	HandleBody->SetAngularDamping(
		HandleAngularDamping
	);

	/*
	 * 중요: 이 함수는 native 생성자에서도 왼쪽/오른쪽 손잡이에 대해 호출된다.
	 * CDO/Archetype 생성 중 SetMassOverrideInKg()를 호출하면 엔진 초기화 전에
	 * FBodyInstance가 Physical Material을 조회해 패키징 Cook이 실패할 수 있다.
	 *
	 * 실제 레벨 인스턴스에서는 BeginPlay와 DetachHandleAndEnablePhysics() 경로로
	 * 다시 호출되므로 런타임 질량값은 그대로 적용된다.
	 */
	const bool bCanApplyRuntimeMass =
		!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) &&
		!HandleBody->IsTemplate();

	if (bCanApplyRuntimeMass)
	{
		HandleBody->SetMassOverrideInKg(
			NAME_None,
			FMath::Max(HandleMassKg, 0.1f),
			true
		);
	}
}

void ATwoDRopeLiftBar::ConfigurePulleyBody(
	USphereComponent* PulleyBody
) const
{
	if (!IsValid(PulleyBody))
	{
		return;
	}

	PulleyBody->SetSphereRadius(
		8.0f,
		true
	);
	PulleyBody->SetSimulatePhysics(false);
	PulleyBody->SetEnableGravity(false);
	PulleyBody->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);
	PulleyBody->SetCollisionResponseToAllChannels(
		ECR_Ignore
	);
	PulleyBody->SetGenerateOverlapEvents(false);
	PulleyBody->SetHiddenInGame(true);
}

void ATwoDRopeLiftBar::ConfigurePhysicsHandle(
	UPhysicsHandleComponent* PhysicsHandle
) const
{
	if (!IsValid(PhysicsHandle))
	{
		return;
	}

	PhysicsHandle->SetLinearStiffness(
		GripLinearStiffness
	);
	PhysicsHandle->SetLinearDamping(
		GripLinearDamping
	);
	PhysicsHandle->SetInterpolationSpeed(
		GripInterpolationSpeed
	);
}

void ATwoDRopeLiftBar::ConfigureCable(
	UCableComponent* Cable
) const
{
	if (!IsValid(Cable))
	{
		return;
	}

	Cable->CableWidth = CableWidth;
	Cable->NumSegments = CableSegments;
	Cable->SolverIterations = CableSolverIterations;
	Cable->SubstepTime = CableSubstepTime;

	Cable->bUseSubstepping = true;
	Cable->bAttachStart = true;
	Cable->bAttachEnd = true;
	Cable->bEnableStiffness = true;
	Cable->bEnableCollision =
		bEnableCableCollision;

	Cable->CollisionFriction =
		CableCollisionFriction;

	Cable->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly
	);
	Cable->SetCollisionObjectType(
		ECC_WorldDynamic
	);
	Cable->SetCollisionResponseToAllChannels(
		ECR_Block
	);
	Cable->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Ignore
	);
	Cable->SetCollisionResponseToChannel(
		ECC_Camera,
		ECR_Ignore
	);

	/*
	 * 중요:
	 *
	 * 위에서 SetCollisionResponseToAllChannels(ECR_Block)을 호출하기 때문에
	 * BP 컴포넌트에서 Mantle을 Ignore로 바꿔도 Construction Script 또는
	 * 레벨 인스턴스 재생성 시 다시 Block으로 덮어써질 수 있다.
	 *
	 * 따라서 마지막 단계에서 Mantle Trace Channel을 명시적으로
	 * Ignore로 다시 설정한다.
	 */
	if (bIgnoreMantleTrace)
	{
		Cable->SetCollisionResponseToChannel(
			MantleTraceChannel,
			ECR_Ignore
		);
	}

	Cable->CableGravityScale = 1.0f;
}

void ATwoDRopeLiftBar::ClearBuiltInDOFConstraintSettings()
{
	if (!IsValid(BarBody))
	{
		return;
	}

	BarBody->BodyInstance.DOFMode =
		EDOFMode::Default;

	BarBody->BodyInstance.bLockXTranslation = false;
	BarBody->BodyInstance.bLockYTranslation = false;
	BarBody->BodyInstance.bLockZTranslation = false;

	BarBody->BodyInstance.bLockXRotation = false;
	BarBody->BodyInstance.bLockYRotation = false;
	BarBody->BodyInstance.bLockZRotation = false;
}

void ATwoDRopeLiftBar::CaptureMechanismFrame()
{
	if (!IsValid(BarBody))
	{
		return;
	}

	const FTransform BarTransform =
		BarBody->GetComponentTransform();

	MechanismFrameTransform =
		FTransform(
			BarTransform.GetRotation(),
			BarTransform.GetLocation(),
			FVector::OneVector
		);

	InitialBarCenterLocal =
		MechanismFrameTransform.
		InverseTransformPosition(
			BarBody->GetComponentLocation()
		);

	LeftInitialEndpointLocalZ =
		IsValid(LeftBarEnd)
		? MechanismFrameTransform.
		InverseTransformPosition(
			LeftBarEnd->GetComponentLocation()
		).Z
		: 0.0f;

	RightInitialEndpointLocalZ =
		IsValid(RightBarEnd)
		? MechanismFrameTransform.
		InverseTransformPosition(
			RightBarEnd->GetComponentLocation()
		).Z
		: 0.0f;

	bMechanismFrameCaptured = true;
}

void ATwoDRopeLiftBar::EnforceLocalMechanismBounds()
{
	if (
		!bMechanismFrameCaptured ||
		!IsValid(BarBody) ||
		!BarBody->IsSimulatingPhysics()
		)
	{
		return;
	}

	FVector LocalCenter =
		MechanismFrameTransform.
		InverseTransformPosition(
			BarBody->GetComponentLocation()
		);

	const float MinimumLocalX =
		InitialBarCenterLocal.X -
		MaxCenterTravelLocalX;

	const float MaximumLocalX =
		InitialBarCenterLocal.X +
		MaxCenterTravelLocalX;

	const float MinimumLocalZ =
		InitialBarCenterLocal.Z;

	const float MaximumLocalZ =
		InitialBarCenterLocal.Z +
		GetEffectiveMaxEndpointLiftDistance();

	const FVector OriginalLocalCenter =
		LocalCenter;

	LocalCenter.X =
		FMath::Clamp(
			LocalCenter.X,
			MinimumLocalX,
			MaximumLocalX
		);

	LocalCenter.Y =
		InitialBarCenterLocal.Y;

	LocalCenter.Z =
		FMath::Clamp(
			LocalCenter.Z,
			MinimumLocalZ,
			MaximumLocalZ
		);

	const FQuat FrameRotation =
		MechanismFrameTransform.
		GetRotation();

	const FQuat RelativeRotationQuat =
		FrameRotation.Inverse() *
		BarBody->GetComponentQuat();

	FRotator RelativeRotation =
		RelativeRotationQuat.Rotator();

	RelativeRotation.Pitch =
		FMath::Clamp(
			FMath::UnwindDegrees(
				RelativeRotation.Pitch
			),
			-MaxRelativeTiltDegrees,
			MaxRelativeTiltDegrees
		);

	/*
	 * 로컬 Y축 Pitch만 허용한다.
	 */
	RelativeRotation.Roll = 0.0f;
	RelativeRotation.Yaw = 0.0f;

	const FVector CorrectedWorldLocation =
		MechanismFrameTransform.
		TransformPosition(
			LocalCenter
		);

	const FQuat CorrectedWorldRotation =
		FrameRotation *
		RelativeRotation.Quaternion();

	const bool bLocationCorrectionNeeded =
		!OriginalLocalCenter.Equals(
			LocalCenter,
			LocalBoundTolerance
		);

	const bool bRotationCorrectionNeeded =
		!BarBody->GetComponentQuat().
		Equals(
			CorrectedWorldRotation,
			0.0005f
		);

	if (
		bLocationCorrectionNeeded ||
		bRotationCorrectionNeeded
		)
	{
		BarBody->SetWorldLocationAndRotation(
			CorrectedWorldLocation,
			CorrectedWorldRotation.Rotator(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}

	FVector LocalLinearVelocity =
		MechanismFrameTransform.
		InverseTransformVectorNoScale(
			BarBody->
			GetPhysicsLinearVelocity()
		);

	LocalLinearVelocity.Y = 0.0f;

	if (
		LocalCenter.X <=
		MinimumLocalX +
		LocalBoundTolerance &&
		LocalLinearVelocity.X < 0.0f
		)
	{
		LocalLinearVelocity.X = 0.0f;
	}

	if (
		LocalCenter.X >=
		MaximumLocalX -
		LocalBoundTolerance &&
		LocalLinearVelocity.X > 0.0f
		)
	{
		LocalLinearVelocity.X = 0.0f;
	}

	if (
		LocalCenter.Z <=
		MinimumLocalZ +
		LocalBoundTolerance &&
		LocalLinearVelocity.Z < 0.0f
		)
	{
		LocalLinearVelocity.Z = 0.0f;
	}

	if (
		LocalCenter.Z >=
		MaximumLocalZ -
		LocalBoundTolerance &&
		LocalLinearVelocity.Z > 0.0f
		)
	{
		LocalLinearVelocity.Z = 0.0f;
	}

	BarBody->SetPhysicsLinearVelocity(
		MechanismFrameTransform.
		TransformVectorNoScale(
			LocalLinearVelocity
		),
		false,
		NAME_None
	);

	FVector LocalAngularVelocity =
		MechanismFrameTransform.
		InverseTransformVectorNoScale(
			BarBody->
			GetPhysicsAngularVelocityInDegrees()
		);

	/*
	 * 로컬 Y축 각속도만 허용한다.
	 */
	LocalAngularVelocity.X = 0.0f;
	LocalAngularVelocity.Z = 0.0f;

	if (
		RelativeRotation.Pitch <=
		-MaxRelativeTiltDegrees +
		0.1f &&
		LocalAngularVelocity.Y < 0.0f
		)
	{
		LocalAngularVelocity.Y = 0.0f;
	}

	if (
		RelativeRotation.Pitch >=
		MaxRelativeTiltDegrees -
		0.1f &&
		LocalAngularVelocity.Y > 0.0f
		)
	{
		LocalAngularVelocity.Y = 0.0f;
	}

	BarBody->
		SetPhysicsAngularVelocityInDegrees(
			MechanismFrameTransform.
			TransformVectorNoScale(
				LocalAngularVelocity
			),
			false,
			NAME_None
		);
}

void ATwoDRopeLiftBar::ApplyLocalCenteringController(
	float DeltaSeconds
)
{
	if (
		!HasAuthority() ||
		!bMechanismFrameCaptured ||
		!IsValid(BarBody) ||
		!BarBody->IsSimulatingPhysics() ||
		DeltaSeconds <= KINDA_SMALL_NUMBER
		)
	{
		return;
	}

	const FVector LocalCenter =
		MechanismFrameTransform.
		InverseTransformPosition(
			BarBody->GetComponentLocation()
		);

	const FVector LocalVelocity =
		MechanismFrameTransform.
		InverseTransformVectorNoScale(
			BarBody->
			GetPhysicsLinearVelocity()
		);

	const float CenterErrorX =
		InitialBarCenterLocal.X -
		LocalCenter.X;

	float LocalAccelerationX =
		CenterErrorX *
		CenteringAccelerationPerCentimeter
		-
		LocalVelocity.X *
		CenteringVelocityDamping;

	LocalAccelerationX =
		FMath::Clamp(
			LocalAccelerationX,
			-MaximumCenteringAcceleration,
			MaximumCenteringAcceleration
		);

	if (
		FMath::Abs(LocalAccelerationX) <=
		KINDA_SMALL_NUMBER
		)
	{
		return;
	}

	const float BarMass =
		FMath::Max(
			BarBody->GetMass(),
			1.0f
		);

	const FVector LocalImpulse(
		BarMass *
		LocalAccelerationX *
		DeltaSeconds,
		0.0f,
		0.0f
	);

	BarBody->WakeAllRigidBodies();

	BarBody->AddImpulse(
		MechanismFrameTransform.
		TransformVectorNoScale(
			LocalImpulse
		),
		NAME_None,
		false
	);
}

float ATwoDRopeLiftBar::GetEffectiveBarHalfLength() const
{
	return FMath::Max(
		BarHalfLength *
		HorizontalSpanScale,
		20.0f
	);
}

float ATwoDRopeLiftBar::GetEffectivePulleyAnchorHalfSpacing() const
{
	/*
	 * 기존 줄 중심 간격:
	 *
	 * 2 * (BarHalfLength - PulleyAnchorInset)
	 *
	 * 이 전체 간격 자체를 정확히 1.2배로 만든다.
	 */
	const float SafeInset =
		FMath::Clamp(
			PulleyAnchorInset,
			0.0f,
			FMath::Max(
				BarHalfLength - 1.0f,
				0.0f
			)
		);

	return FMath::Max(
		(
			BarHalfLength -
			SafeInset
			) *
		HorizontalSpanScale,
		1.0f
	);
}

float ATwoDRopeLiftBar::GetEffectivePulleyHeight() const
{
	return FMath::Max(
		PulleyHeight *
		VerticalHeightScale,
		100.0f
	);
}

float ATwoDRopeLiftBar::GetEffectiveMaxEndpointLiftDistance() const
{
	/*
	 * 기존 2.5배 높이 구조는 유지하고,
	 * 최대 당김에 도달했을 때의 최종 상승 높이만 10% 추가한다.
	 */
	return FMath::Max(
		MaxEndpointLiftDistance *
		VerticalHeightScale *
		EndpointLiftBonusScale,
		50.0f
	);
}

float ATwoDRopeLiftBar::GetEffectivePullTravelToMaximumLift() const
{
	return FMath::Max(
		PullTravelToMaximumLift *
		VerticalHeightScale,
		1.0f
	);
}

float ATwoDRopeLiftBar::GetEffectiveTensionSlackDistance() const
{
	return FMath::Max(
		TensionSlackDistance *
		VerticalHeightScale,
		0.0f
	);
}

float ATwoDRopeLiftBar::GetEffectiveRopeLengthExtraSlack() const
{
	return FMath::Max(
		RopeLengthExtraSlack *
		VerticalHeightScale,
		0.0f
	);
}

float ATwoDRopeLiftBar::GetEffectiveInitialHandleDrop() const
{
	return
		FMath::Max(
			InitialHandleDrop *
			HandleRopeLengthScale *
			VerticalHeightScale,
			50.0f
		);
}

FVector ATwoDRopeLiftBar::GetFreeHandleInitialLocalOffset() const
{
	return FVector(
		0.0f,
		FreeHandleForwardOffset,
		-(
			GetEffectiveInitialHandleDrop() +
			FreeHandleExtraDrop *
			VerticalHeightScale
			)
	);
}

float ATwoDRopeLiftBar::GetFreeHandleRopeLength() const
{
	return FMath::Max(
		GetFreeHandleInitialLocalOffset().Size(),
		MinimumRopeSegmentLength
	);
}

void ATwoDRopeLiftBar::FreezePulleyFrameInWorld()
{
	if (bPulleyFrameFrozen)
	{
		return;
	}

	const auto DetachAndKeepWorld =
		[](
			USceneComponent* Component
			)
		{
			if (!IsValid(Component))
			{
				return;
			}

			const FTransform WorldTransform =
				Component->GetComponentTransform();

			Component->DetachFromComponent(
				FDetachmentTransformRules::
				KeepWorldTransform
			);

			Component->SetWorldTransform(
				WorldTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics
			);
		};

	DetachAndKeepWorld(LeftPulleyAnchor);
	DetachAndKeepWorld(RightPulleyAnchor);
	DetachAndKeepWorld(PulleyRodVisual);

	bPulleyFrameFrozen = true;
}

void ATwoDRopeLiftBar::DetachHandleAndEnablePhysics(
	USphereComponent* HandleBody
)
{
	if (!IsValid(HandleBody))
	{
		return;
	}

	const FTransform WorldTransform =
		HandleBody->GetComponentTransform();

	HandleBody->SetSimulatePhysics(false);

	HandleBody->DetachFromComponent(
		FDetachmentTransformRules::KeepWorldTransform
	);

	HandleBody->SetWorldTransform(
		WorldTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	ConfigureHandleBody(HandleBody);

	HandleBody->SetSimulatePhysics(true);
	HandleBody->SetEnableGravity(true);
	HandleBody->SetLinearDamping(
		FreeHandleLinearDamping
	);
	HandleBody->SetAngularDamping(
		FreeHandleAngularDamping
	);
	HandleBody->WakeAllRigidBodies();
}

void ATwoDRopeLiftBar::UpdatePulleyController(
	float DeltaSeconds
)
{
	if (
		!HasAuthority() ||
		!bMechanismFrameCaptured ||
		DeltaSeconds <= KINDA_SMALL_NUMBER
		)
	{
		return;
	}

	const float LeftTargetHeight =
		CalculatePullAlpha(
			ETwoDRopeSide::Left
		) *
		GetEffectiveMaxEndpointLiftDistance();

	const float RightTargetHeight =
		CalculatePullAlpha(
			ETwoDRopeSide::Right
		) *
		GetEffectiveMaxEndpointLiftDistance();

	/*
	 * 양쪽 끝을 각각 독립적으로 올리고 내린다.
	 *
	 * 멀어짐:
	 * TargetHeight 증가 -> 같은 쪽 끝 상승
	 *
	 * 가까워짐:
	 * TargetHeight 감소 -> 같은 쪽 끝 하강
	 */
	CurrentLeftLiftHeight =
		FMath::FInterpTo(
			CurrentLeftLiftHeight,
			LeftTargetHeight,
			DeltaSeconds,
			LeftTargetHeight >
			CurrentLeftLiftHeight
			? LiftUpInterpSpeed
			: LiftDownInterpSpeed
		);

	CurrentRightLiftHeight =
		FMath::FInterpTo(
			CurrentRightLiftHeight,
			RightTargetHeight,
			DeltaSeconds,
			RightTargetHeight >
			CurrentRightLiftHeight
			? LiftUpInterpSpeed
			: LiftDownInterpSpeed
		);

	float AppliedLeftHeight =
		CurrentLeftLiftHeight;

	float AppliedRightHeight =
		CurrentRightLiftHeight;

	ClampDesiredEndpointHeights(
		AppliedLeftHeight,
		AppliedRightHeight
	);

	ApplyRigidBarTargetController(
		AppliedLeftHeight,
		AppliedRightHeight,
		DeltaSeconds
	);
}

float ATwoDRopeLiftBar::CalculatePlanarDistanceFromPulley(
	ETwoDRopeSide Side,
	const FVector& HandWorldLocation
) const
{
	const USceneComponent* PulleyAnchor =
		GetPulleyAnchorForSide(Side);

	if (
		!IsValid(PulleyAnchor) ||
		!bMechanismFrameCaptured
		)
	{
		return 0.0f;
	}

	/*
	 * 액터를 45도 회전해 배치해도 기구의 로컬 평면 기준으로 계산한다.
	 * 로컬 Z를 제거하므로 점프/손 높이는 줄 당김에 영향을 주지 않는다.
	 */
	FVector LocalDelta =
		MechanismFrameTransform.
		InverseTransformVectorNoScale(
			HandWorldLocation -
			PulleyAnchor->GetComponentLocation()
		);

	LocalDelta.Z = 0.0f;

	return LocalDelta.Size();
}

float ATwoDRopeLiftBar::CalculateRawProjectedPullDistance(
	ETwoDRopeSide Side
) const
{
	const ACharacter* Holder =
		GetHolderForSide(Side);

	if (!IsValid(Holder))
	{
		return 0.0f;
	}

	const float StartDistance =
		Side == ETwoDRopeSide::Left
		? LeftGrabStartPlanarDistance
		: RightGrabStartPlanarDistance;

	/*
	 * 클라이언트는 Holder보다 기준거리 복제가 한 프레임 늦을 수 있다.
	 * 기준값이 아직 없을 때 잘못 최대거리로 판단하지 않는다.
	 */
	if (
		!HasAuthority() &&
		StartDistance <= KINDA_SMALL_NUMBER
		)
	{
		return 0.0f;
	}

	const float CurrentDistance =
		CalculatePlanarDistanceFromPulley(
			Side,
			GetCharacterRopeControlLocation(Holder)
		);

	/*
	 * 이것이 전체 규칙이다.
	 *
	 * 도르래에서 멀어짐 -> 양수 -> 올라감
	 * 도르래에 가까워짐 -> 0 -> 내려감
	 */
	return FMath::Max(
		CurrentDistance -
		StartDistance -
		PullResetSlack,
		0.0f
	);
}

float ATwoDRopeLiftBar::CalculatePullAlpha(
	ETwoDRopeSide Side
) const
{
	const float PullAfterTension =
		CalculateRawProjectedPullDistance(Side) -
		GetEffectiveTensionSlackDistance();

	const float SafePullTravel =
		GetEffectivePullTravelToMaximumLift();

	const float LinearAlpha =
		FMath::Clamp(
			PullAfterTension /
			SafePullTravel,
			0.0f,
			1.0f
		);

	return FMath::Pow(
		LinearAlpha,
		FMath::Max(
			PullResponseExponent,
			0.01f
		)
	);
}

float ATwoDRopeLiftBar::GetMaximumProjectedPullDistance() const
{
	return
		GetEffectiveTensionSlackDistance() +
		GetEffectivePullTravelToMaximumLift();
}

FVector ATwoDRopeLiftBar::GetPlanarOutwardWorldDirection(
	ETwoDRopeSide Side,
	const FVector& HandWorldLocation
) const
{
	const USceneComponent* PulleyAnchor =
		GetPulleyAnchorForSide(Side);

	if (
		!IsValid(PulleyAnchor) ||
		!bMechanismFrameCaptured
		)
	{
		return FVector::ZeroVector;
	}

	FVector LocalDirection =
		MechanismFrameTransform.
		InverseTransformVectorNoScale(
			HandWorldLocation -
			PulleyAnchor->GetComponentLocation()
		);

	LocalDirection.Z = 0.0f;

	if (!LocalDirection.Normalize())
	{
		LocalDirection =
			Side == ETwoDRopeSide::Left
			? FVector(-1.0f, 0.0f, 0.0f)
			: FVector(1.0f, 0.0f, 0.0f);
	}

	return MechanismFrameTransform.
		TransformVectorNoScale(
			LocalDirection
		).GetSafeNormal();
}

void ATwoDRopeLiftBar::ClampDesiredEndpointHeights(
	float& InOutLeftHeight,
	float& InOutRightHeight
) const
{
	InOutLeftHeight =
		FMath::Clamp(
			InOutLeftHeight,
			0.0f,
			GetEffectiveMaxEndpointLiftDistance()
		);

	InOutRightHeight =
		FMath::Clamp(
			InOutRightHeight,
			0.0f,
			GetEffectiveMaxEndpointLiftDistance()
		);

	/*
	 * 강체 막대기의 실제 끝점 높이 차이는:
	 *
	 * FullLength * sin(Pitch)
	 *
	 * 이다. v10의 tan 기반 계산보다 실제 강체 회전에 맞는 제한이다.
	 */
	const float MaximumHeightDifference =
		FMath::Sin(
			FMath::DegreesToRadians(
				MaxRelativeTiltDegrees
			)
		) *
		FMath::Max(
			GetEffectiveBarHalfLength() *
			2.0f,
			1.0f
		);

	if (
		InOutLeftHeight -
		InOutRightHeight >
		MaximumHeightDifference
		)
	{
		/*
		 * 당겨서 높아진 왼쪽 목표는 유지하고,
		 * 오른쪽만 필요한 만큼 같이 올린다.
		 */
		InOutRightHeight =
			FMath::Max(
				InOutLeftHeight -
				MaximumHeightDifference,
				0.0f
			);
	}
	else if (
		InOutRightHeight -
		InOutLeftHeight >
		MaximumHeightDifference
		)
	{
		InOutLeftHeight =
			FMath::Max(
				InOutRightHeight -
				MaximumHeightDifference,
				0.0f
			);
	}
}

void ATwoDRopeLiftBar::ApplyRigidBarTargetController(
	float LeftDesiredHeight,
	float RightDesiredHeight,
	float DeltaSeconds
)
{
	if (
		!HasAuthority() ||
		!bMechanismFrameCaptured ||
		!IsValid(BarBody) ||
		DeltaSeconds <= KINDA_SMALL_NUMBER
		)
	{
		return;
	}

	/*
	 * 왼쪽과 오른쪽 끝의 목표 좌표를 직접 만든다.
	 *
	 * LeftDesiredHeight는 반드시 로컬 -X 끝,
	 * RightDesiredHeight는 반드시 로컬 +X 끝에 들어간다.
	 *
	 * 따라서 왼쪽 플레이어가 당기면 왼쪽 끝이 올라가고,
	 * 오른쪽 플레이어가 당기면 오른쪽 끝이 올라간다.
	 */
	const float EffectiveBarHalfLength =
		GetEffectiveBarHalfLength();

	FVector LeftTargetLocal(
		-EffectiveBarHalfLength,
		0.0f,
		LeftInitialEndpointLocalZ +
		FMath::Clamp(
			LeftDesiredHeight,
			0.0f,
			GetEffectiveMaxEndpointLiftDistance()
		)
	);

	FVector RightTargetLocal(
		EffectiveBarHalfLength,
		0.0f,
		RightInitialEndpointLocalZ +
		FMath::Clamp(
			RightDesiredHeight,
			0.0f,
			GetEffectiveMaxEndpointLiftDistance()
		)
	);

	const FVector TargetLocalCenter =
		(LeftTargetLocal +
			RightTargetLocal) *
		0.5f;

	const FVector LocalBarDirection =
		(RightTargetLocal -
			LeftTargetLocal).
		GetSafeNormal();

	/*
	 * 막대기 로컬 +X축이 Left -> Right 방향을 보게 만든다.
	 * 이 계산은 Pitch 부호를 추측하지 않으므로 좌우가 뒤집히지 않는다.
	 */
	FRotator TargetLocalRotation =
		LocalBarDirection.Rotation();

	TargetLocalRotation.Roll = 0.0f;
	TargetLocalRotation.Yaw = 0.0f;

	TargetLocalRotation.Pitch =
		FMath::Clamp(
			FMath::UnwindDegrees(
				TargetLocalRotation.Pitch
			),
			-MaxRelativeTiltDegrees,
			MaxRelativeTiltDegrees
		);

	const FVector TargetWorldLocation =
		MechanismFrameTransform.
		TransformPosition(
			TargetLocalCenter
		);

	const FQuat TargetWorldRotation =
		MechanismFrameTransform.
		GetRotation() *
		TargetLocalRotation.Quaternion();

	const FVector NewWorldLocation =
		FMath::VInterpTo(
			BarBody->GetComponentLocation(),
			TargetWorldLocation,
			DeltaSeconds,
			DirectLocationInterpSpeed
		);

	const FQuat NewWorldRotation =
		FMath::QInterpTo(
			BarBody->GetComponentQuat(),
			TargetWorldRotation,
			DeltaSeconds,
			DirectRotationInterpSpeed
		);

	/*
	 * TeleportPhysics가 아니라 None으로 이동해
	 * 발판 위 물리 공이 움직이는 발판의 속도를 받을 수 있게 한다.
	 */
	BarBody->SetWorldLocationAndRotation(
		NewWorldLocation,
		NewWorldRotation.Rotator(),
		false,
		nullptr,
		ETeleportType::None
	);
}

void ATwoDRopeLiftBar::UpdateLooseHandles(
	float DeltaSeconds
)
{
	UpdateLooseHandle(
		ETwoDRopeSide::Left,
		DeltaSeconds
	);

	UpdateLooseHandle(
		ETwoDRopeSide::Right,
		DeltaSeconds
	);
}

void ATwoDRopeLiftBar::UpdateLooseHandle(
	ETwoDRopeSide Side,
	float DeltaSeconds
)
{
	if (IsValid(GetHolderForSide(Side)))
	{
		return;
	}

	USphereComponent* Handle =
		GetHandleForSide(Side);

	const USceneComponent* PulleyAnchor =
		GetPulleyAnchorForSide(Side);

	if (
		!IsValid(Handle) ||
		!IsValid(PulleyAnchor) ||
		!Handle->IsSimulatingPhysics()
		)
	{
		return;
	}

	/*
	 * 놓인 손잡이는 특정 위치로 돌아가지 않는다.
	 *
	 * - 중력 사용
	 * - 바닥/벽 충돌 사용
	 * - 도르래를 중심으로 최대 줄 길이만 제한
	 *
	 * 따라서 공중에서는 진자처럼 흔들리고,
	 * 줄이 바닥까지 닿으면 손잡이가 바닥에 그대로 놓인다.
	 */
	Handle->SetEnableGravity(true);
	Handle->SetLinearDamping(
		FreeHandleLinearDamping
	);
	Handle->SetAngularDamping(
		FreeHandleAngularDamping
	);

	const FVector PulleyLocation =
		PulleyAnchor->GetComponentLocation();

	const FVector HandleLocation =
		Handle->GetComponentLocation();

	/*
	 * BP에서 지정한 초기 손잡이 오프셋을 놓인 상태의 안전한 기준점으로도 쓴다.
	 *
	 * 중요한 점:
	 * - Z축 힘은 주지 않는다. 중력과 줄 길이로 높이가 결정된다.
	 * - PulleyAnchor 로컬 X/Y 평면의 힘만 주므로 자유 진자 운동이 남는다.
	 * - SetWorldLocation으로 고정하지 않아서 벽/바닥 충돌도 그대로 동작한다.
	 */
	if (
		bUseFreeHandleRestBias &&
		FreeHandleRestAccelerationPerCentimeter >
		KINDA_SMALL_NUMBER &&
		MaximumFreeHandleRestAcceleration >
		KINDA_SMALL_NUMBER
		)
	{
		const FTransform PulleyTransform =
			PulleyAnchor->GetComponentTransform();

		const FVector RestTargetWorldLocation =
			PulleyTransform.TransformPositionNoScale(
				GetFreeHandleInitialLocalOffset()
			);

		FVector RestErrorLocal =
			PulleyTransform.InverseTransformVectorNoScale(
				RestTargetWorldLocation -
				HandleLocation
			);

		/* 세로는 중력에 맡기고 앞/뒤와 좌/우만 안전 위치로 유도한다. */
		RestErrorLocal.Z = 0.0f;

		if (
			RestErrorLocal.SizeSquared() >
			FMath::Square(FreeHandleRestDeadZone)
			)
		{
			FVector LocalVelocity =
				PulleyTransform.
				InverseTransformVectorNoScale(
					Handle->GetPhysicsLinearVelocity()
				);

			LocalVelocity.Z = 0.0f;

			FVector RestAccelerationLocal =
				RestErrorLocal *
				FreeHandleRestAccelerationPerCentimeter -
				LocalVelocity *
				FreeHandleRestVelocityDamping;

			RestAccelerationLocal =
				RestAccelerationLocal.
				GetClampedToMaxSize(
					MaximumFreeHandleRestAcceleration
				);

			const FVector RestAccelerationWorld =
				PulleyTransform.TransformVectorNoScale(
					RestAccelerationLocal
				);

			/* bAccelChange=true: 손잡이 질량과 무관하게 같은 반응을 유지한다. */
			Handle->AddForce(
				RestAccelerationWorld,
				NAME_None,
				true
			);
		}
	}

	const FVector PulleyToHandle =
		HandleLocation -
		PulleyLocation;

	const float CurrentDistance =
		PulleyToHandle.Size();

	if (CurrentDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector RopeDirection =
		PulleyToHandle /
		CurrentDistance;

	const float MaximumRopeLength =
		GetFreeHandleRopeLength();

	const float TensionStartDistance =
		FMath::Max(
			MaximumRopeLength -
			FreeHandleTensionZone,
			0.0f
		);

	FVector CurrentVelocity =
		Handle->GetPhysicsLinearVelocity();

	const float OutwardSpeed =
		FVector::DotProduct(
			CurrentVelocity,
			RopeDirection
		);

	/*
	 * 최대 길이에 가까워질수록 바깥 방향 속도를 부드럽게 제거한다.
	 * 접선 방향 속도는 유지하므로 진자 운동은 그대로 남는다.
	 */
	if (
		FreeHandleTensionZone >
		KINDA_SMALL_NUMBER &&
		CurrentDistance >
		TensionStartDistance &&
		OutwardSpeed > 0.0f
		)
	{
		const float TensionAlpha =
			FMath::Clamp(
				(
					CurrentDistance -
					TensionStartDistance
					) /
				FreeHandleTensionZone,
				0.0f,
				1.0f
			);

		CurrentVelocity -=
			RopeDirection *
			OutwardSpeed *
			TensionAlpha;

		Handle->SetPhysicsLinearVelocity(
			CurrentVelocity,
			false,
			NAME_None
		);
	}

	/*
	 * 수치 오차나 빠른 낙하로 최대 길이를 넘으면
	 * 도르래 방향으로만 위치를 되돌리고 바깥 속도만 제거한다.
	 */
	if (
		CurrentDistance >
		MaximumRopeLength +
		FreeHandleRopeTolerance
		)
	{
		const FVector ClampedLocation =
			PulleyLocation +
			RopeDirection *
			MaximumRopeLength;

		/*
		 * 보정 방향이 항상 도르래 쪽이므로
		 * 바닥에서 손잡이를 아래로 밀어 넣지 않는다.
		 */
		Handle->SetWorldLocation(
			ClampedLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		CurrentVelocity =
			Handle->GetPhysicsLinearVelocity();

		const float NewOutwardSpeed =
			FVector::DotProduct(
				CurrentVelocity,
				RopeDirection
			);

		if (NewOutwardSpeed > 0.0f)
		{
			CurrentVelocity -=
				RopeDirection *
				NewOutwardSpeed;

			Handle->SetPhysicsLinearVelocity(
				CurrentVelocity,
				false,
				NAME_None
			);
		}
	}
}

bool ATwoDRopeLiftBar::StartHoldingSide(
	ACharacter* Character,
	ETwoDRopeSide Side
)
{
	if (
		!HasAuthority() ||
		!IsValid(Character)
		)
	{
		return false;
	}

	USphereComponent* HandleBody =
		GetHandleForSide(Side);

	UPhysicsHandleComponent* GrabHandle =
		GetGrabPhysicsHandleForSide(Side);

	if (
		!IsValid(HandleBody) ||
		!IsValid(GrabHandle) ||
		!HandleBody->IsSimulatingPhysics()
		)
	{
		return false;
	}

	if (
		IsValid(
			GrabHandle->GetGrabbedComponent()
		)
		)
	{
		GrabHandle->ReleaseComponent();
	}

	/*
	 * 잡고 있는 동안에는 Physics Handle만 손 위치를 추적하게 하고
	 * 중력은 잠시 끈다.
	 */
	HandleBody->SetEnableGravity(false);
	HandleBody->SetLinearDamping(
		HandleLinearDamping
	);
	HandleBody->SetAngularDamping(
		HandleAngularDamping
	);
	HandleBody->WakeAllRigidBodies();

	const FVector CurrentHandleLocation =
		HandleBody->GetComponentLocation();

	GrabHandle->GrabComponentAtLocation(
		HandleBody,
		NAME_None,
		CurrentHandleLocation
	);

	if (
		GrabHandle->GetGrabbedComponent() !=
		HandleBody
		)
	{
		return false;
	}

	GrabHandle->SetTargetLocation(
		CurrentHandleLocation
	);

	const FVector RopeControlLocationAtGrab =
		GetCharacterRopeControlLocation(Character);

	const float StartPlanarDistance =
		CalculatePlanarDistanceFromPulley(
			Side,
			RopeControlLocationAtGrab
		);

	if (Side == ETwoDRopeSide::Left)
	{
		LeftGrabStartPlanarDistance =
			StartPlanarDistance;
	}
	else if (Side == ETwoDRopeSide::Right)
	{
		RightGrabStartPlanarDistance =
			StartPlanarDistance;
	}

	return true;
}

void ATwoDRopeLiftBar::StopHoldingSide(
	ETwoDRopeSide Side
)
{
	UPhysicsHandleComponent* GrabHandle =
		GetGrabPhysicsHandleForSide(Side);

	if (
		IsValid(GrabHandle) &&
		IsValid(
			GrabHandle->GetGrabbedComponent()
		)
		)
	{
		GrabHandle->ReleaseComponent();
	}

	USphereComponent* Handle =
		GetHandleForSide(Side);

	if (IsValid(Handle))
	{
		/*
		 * 손을 놓는 즉시 고정 복귀 힘 없이 자유 물리 상태로 전환한다.
		 */
		Handle->SetEnableGravity(true);
		Handle->SetLinearDamping(
			FreeHandleLinearDamping
		);
		Handle->SetAngularDamping(
			FreeHandleAngularDamping
		);
		Handle->WakeAllRigidBodies();
	}

	if (Side == ETwoDRopeSide::Left)
	{
		LeftGrabStartPlanarDistance = 0.0f;
	}
	else if (Side == ETwoDRopeSide::Right)
	{
		RightGrabStartPlanarDistance = 0.0f;
	}
}

void ATwoDRopeLiftBar::UpdateHeldHandleTargets()
{
	UpdateHeldHandleTarget(
		ETwoDRopeSide::Left
	);
	UpdateHeldHandleTarget(
		ETwoDRopeSide::Right
	);
}

void ATwoDRopeLiftBar::UpdateHeldHandleTarget(
	ETwoDRopeSide Side
)
{
	ACharacter* Holder =
		GetHolderForSide(Side);

	UPhysicsHandleComponent* GrabHandle =
		GetGrabPhysicsHandleForSide(Side);

	USphereComponent* HandleBody =
		GetHandleForSide(Side);

	if (
		!IsValid(Holder) ||
		!IsValid(GrabHandle) ||
		!IsValid(HandleBody) ||
		GrabHandle->GetGrabbedComponent() !=
		HandleBody
		)
	{
		return;
	}

	HandleBody->WakeAllRigidBodies();

	GrabHandle->SetTargetLocation(
		GetCharacterGrabLocation(Holder)
	);
}

FVector ATwoDRopeLiftBar::GetCharacterGrabLocation(
	const ACharacter* Character
) const
{
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	const USkeletalMeshComponent* CharacterMesh =
		Character->GetMesh();

	if (
		IsValid(CharacterMesh) &&
		RopeGrabSocketName != NAME_None &&
		CharacterMesh->DoesSocketExist(
			RopeGrabSocketName
		)
		)
	{
		return CharacterMesh->GetSocketLocation(
			RopeGrabSocketName
		);
	}

	return Character->GetActorTransform().
		TransformPosition(
			FallbackGrabOffset
		);
}

FVector ATwoDRopeLiftBar::GetCharacterRopeControlLocation(
	const ACharacter* Character
) const
{
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	/*
	 * 손 소켓은 걷기 애니메이션마다 X/Y가 흔들리므로
	 * 최대거리 계산에 사용하면 경계에서 입력이 떨린다.
	 * Actor 위치는 서버와 자율 프록시 모두 CharacterMovement가 예측/보정한다.
	 */
	return Character->GetActorLocation();
}

bool ATwoDRopeLiftBar::TryGrabNearestRope(
	ACharacter* Character
)
{
	if (
		!HasAuthority() ||
		!IsValid(Character)
		)
	{
		return false;
	}

	if (
		GetRopeSideForCharacter(Character) !=
		ETwoDRopeSide::None
		)
	{
		return true;
	}

	const FVector HandLocation =
		GetCharacterGrabLocation(Character);

	const float LeftDistanceSquared =
		IsValid(LeftHandleBody)
		? FVector::DistSquared(
			HandLocation,
			LeftHandleBody->
			GetComponentLocation()
		)
		: MAX_flt;

	const float RightDistanceSquared =
		IsValid(RightHandleBody)
		? FVector::DistSquared(
			HandLocation,
			RightHandleBody->
			GetComponentLocation()
		)
		: MAX_flt;

	const float GrabDistanceSquared =
		FMath::Square(GrabDistance);

	const bool bCanGrabLeft =
		!IsValid(LeftHolder.Get()) &&
		LeftDistanceSquared <=
		GrabDistanceSquared;

	const bool bCanGrabRight =
		!IsValid(RightHolder.Get()) &&
		RightDistanceSquared <=
		GrabDistanceSquared;

	if (!bCanGrabLeft && !bCanGrabRight)
	{
		return false;
	}

	if (bCanGrabLeft && bCanGrabRight)
	{
		return TryGrabSide(
			Character,
			LeftDistanceSquared <=
			RightDistanceSquared
			? ETwoDRopeSide::Left
			: ETwoDRopeSide::Right
		);
	}

	return TryGrabSide(
		Character,
		bCanGrabLeft
		? ETwoDRopeSide::Left
		: ETwoDRopeSide::Right
	);
}

bool ATwoDRopeLiftBar::TryGrabSide(
	ACharacter* Character,
	ETwoDRopeSide Side
)
{
	if (
		!HasAuthority() ||
		!IsValid(Character) ||
		Side == ETwoDRopeSide::None
		)
	{
		return false;
	}

	if (
		!IsCharacterCloseEnoughToSide(
			Character,
			Side
		)
		)
	{
		return false;
	}

	if (IsValid(GetHolderForSide(Side)))
	{
		return false;
	}

	if (!StartHoldingSide(Character, Side))
	{
		return false;
	}

	switch (Side)
	{
	case ETwoDRopeSide::Left:
		LeftHolder = Character;
		break;

	case ETwoDRopeSide::Right:
		RightHolder = Character;
		break;

	default:
		StopHoldingSide(Side);
		return false;
	}

	RefreshCableBindingForSide(Side);

	/*
	 * 잡는 순간에도 발판이 확실히 깨어 있게 한다.
	 */
	BarBody->WakeAllRigidBodies();

	UpdateServerNetworkVisualState(true);
	return true;
}

void ATwoDRopeLiftBar::ReleaseRope(
	ACharacter* Character
)
{
	if (
		!HasAuthority() ||
		!IsValid(Character)
		)
	{
		return;
	}

	bool bChanged = false;

	if (LeftHolder.Get() == Character)
	{
		StopHoldingSide(
			ETwoDRopeSide::Left
		);
		LeftHolder = nullptr;
		bChanged = true;
	}

	if (RightHolder.Get() == Character)
	{
		StopHoldingSide(
			ETwoDRopeSide::Right
		);
		RightHolder = nullptr;
		bChanged = true;
	}

	if (bChanged)
	{
		RefreshCableBindings();
		BarBody->WakeAllRigidBodies();
		UpdateServerNetworkVisualState(true);
	}
}

ETwoDRopeSide
ATwoDRopeLiftBar::GetRopeSideForCharacter(
	const ACharacter* Character
) const
{
	if (!IsValid(Character))
	{
		return ETwoDRopeSide::None;
	}

	if (LeftHolder.Get() == Character)
	{
		return ETwoDRopeSide::Left;
	}

	if (RightHolder.Get() == Character)
	{
		return ETwoDRopeSide::Right;
	}

	return ETwoDRopeSide::None;
}

float
ATwoDRopeLiftBar::
GetClosestAvailableHandleDistanceSquared(
	const FVector& WorldLocation
) const
{
	float BestDistanceSquared = MAX_flt;

	if (
		!IsValid(LeftHolder.Get()) &&
		IsValid(LeftHandleBody)
		)
	{
		BestDistanceSquared =
			FMath::Min(
				BestDistanceSquared,
				FVector::DistSquared(
					WorldLocation,
					LeftHandleBody->
					GetComponentLocation()
				)
			);
	}

	if (
		!IsValid(RightHolder.Get()) &&
		IsValid(RightHandleBody)
		)
	{
		BestDistanceSquared =
			FMath::Min(
				BestDistanceSquared,
				FVector::DistSquared(
					WorldLocation,
					RightHandleBody->
					GetComponentLocation()
				)
			);
	}

	return BestDistanceSquared;
}

FVector ATwoDRopeLiftBar::ConstrainCharacterMoveInput(
	const ACharacter* Character,
	const FVector& DesiredWorldInput
) const
{
	const ETwoDRopeSide Side =
		GetRopeSideForCharacter(Character);

	if (
		Side == ETwoDRopeSide::None ||
		!IsValid(Character) ||
		DesiredWorldInput.IsNearlyZero()
		)
	{
		return DesiredWorldInput;
	}

	const FVector RopeControlLocation =
		GetCharacterRopeControlLocation(Character);

	const FVector OutwardDirection =
		GetPlanarOutwardWorldDirection(
			Side,
			RopeControlLocation
		);

	if (OutwardDirection.IsNearlyZero())
	{
		return DesiredWorldInput;
	}

	const float OutwardInput =
		FVector::DotProduct(
			DesiredWorldInput,
			OutwardDirection
		);

	if (OutwardInput <= 0.0f)
	{
		return DesiredWorldInput;
	}

	const float CurrentPull =
		CalculateRawProjectedPullDistance(Side);

	const float RemainingDistance =
		GetMaximumProjectedPullDistance() -
		CurrentPull;

	if (RemainingDistance >= RopeInputSoftZone)
	{
		return DesiredWorldInput;
	}

	/*
	 * 최대거리 직전 SoftZone 안에서 바깥 입력만 SmoothStep으로 줄인다.
	 * 접선 이동과 안쪽 이동은 그대로 허용한다.
	 */
	const float LinearScale =
		FMath::Clamp(
			RemainingDistance /
			FMath::Max(RopeInputSoftZone, 1.0f),
			0.0f,
			1.0f
		);

	const float SmoothScale =
		LinearScale *
		LinearScale *
		(3.0f - 2.0f * LinearScale);

	return DesiredWorldInput -
		OutwardDirection *
		OutwardInput *
		(1.0f - SmoothScale);
}

void ATwoDRopeLiftBar::EnforceCharacterRopeLimit(
	ACharacter* Character
) const
{
	if (!IsValid(Character))
	{
		return;
	}

	const ETwoDRopeSide Side =
		GetRopeSideForCharacter(Character);

	if (Side == ETwoDRopeSide::None)
	{
		return;
	}

	UCharacterMovementComponent* Movement =
		Character->GetCharacterMovement();

	if (!IsValid(Movement))
	{
		return;
	}

	const FVector RopeControlLocation =
		GetCharacterRopeControlLocation(Character);

	const FVector OutwardDirection =
		GetPlanarOutwardWorldDirection(
			Side,
			RopeControlLocation
		);

	if (OutwardDirection.IsNearlyZero())
	{
		return;
	}

	const float CurrentPull =
		CalculateRawProjectedPullDistance(Side);

	const float MaximumPull =
		GetMaximumProjectedPullDistance();

	const float RemainingDistance =
		MaximumPull - CurrentPull;

	const float OutwardSpeed =
		FVector::DotProduct(
			Movement->Velocity,
			OutwardDirection
		);

	/*
	 * 자율 프록시에서는 SetActorLocation을 절대 반복하지 않는다.
	 * 그 방식은 CharacterMovement의 서버 보정과 싸워 클라 화면이 떨린다.
	 * 입력 제한 + 바깥 속도 브레이크만 로컬 예측에 적용한다.
	 */
	if (!Character->HasAuthority())
	{
		if (
			Character->IsLocallyControlled() &&
			OutwardSpeed > 0.0f &&
			RemainingDistance < ClientVelocityBrakeZone
			)
		{
			const float SpeedScale =
				FMath::Clamp(
					RemainingDistance /
					FMath::Max(ClientVelocityBrakeZone, 1.0f),
					0.0f,
					1.0f
				);

			Movement->Velocity -=
				OutwardDirection *
				OutwardSpeed *
				(1.0f - SpeedScale);
		}

		return;
	}

	/* 서버만 최종 하드 제한을 수행한다. */
	const float ExcessDistance =
		CurrentPull - MaximumPull;

	if (ExcessDistance > ServerHardLimitTolerance)
	{
		FHitResult Hit;

		Character->SetActorLocation(
			Character->GetActorLocation() -
			OutwardDirection *
			ExcessDistance,
			true,
			&Hit,
			ETeleportType::None
		);
	}

	if (
		OutwardSpeed > 0.0f &&
		RemainingDistance <= CharacterLimitTolerance
		)
	{
		Movement->Velocity -=
			OutwardDirection *
			OutwardSpeed;
	}
}

bool ATwoDRopeLiftBar::IsCharacterCloseEnoughToSide(
	const ACharacter* Character,
	ETwoDRopeSide Side
) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	const USphereComponent* Handle =
		GetHandleForSide(Side);

	if (!IsValid(Handle))
	{
		return false;
	}

	return FVector::DistSquared(
		GetCharacterGrabLocation(Character),
		Handle->GetComponentLocation()
	) <= FMath::Square(GrabDistance);
}

USphereComponent*
ATwoDRopeLiftBar::GetHandleForSide(
	ETwoDRopeSide Side
) const
{
	switch (Side)
	{
	case ETwoDRopeSide::Left:
		return LeftHandleBody;

	case ETwoDRopeSide::Right:
		return RightHandleBody;

	default:
		return nullptr;
	}
}

USceneComponent*
ATwoDRopeLiftBar::GetBarEndForSide(
	ETwoDRopeSide Side
) const
{
	switch (Side)
	{
	case ETwoDRopeSide::Left:
		return LeftBarEnd;

	case ETwoDRopeSide::Right:
		return RightBarEnd;

	default:
		return nullptr;
	}
}

USceneComponent*
ATwoDRopeLiftBar::GetPulleyAnchorForSide(
	ETwoDRopeSide Side
) const
{
	switch (Side)
	{
	case ETwoDRopeSide::Left:
		return LeftPulleyAnchor;

	case ETwoDRopeSide::Right:
		return RightPulleyAnchor;

	default:
		return nullptr;
	}
}

UPhysicsHandleComponent*
ATwoDRopeLiftBar::GetGrabPhysicsHandleForSide(
	ETwoDRopeSide Side
) const
{
	switch (Side)
	{
	case ETwoDRopeSide::Left:
		return LeftGrabPhysicsHandle;

	case ETwoDRopeSide::Right:
		return RightGrabPhysicsHandle;

	default:
		return nullptr;
	}
}

UCableComponent*
ATwoDRopeLiftBar::GetHandleCableForSide(
	ETwoDRopeSide Side
) const
{
	switch (Side)
	{
	case ETwoDRopeSide::Left:
		return LeftCable;

	case ETwoDRopeSide::Right:
		return RightCable;

	default:
		return nullptr;
	}
}

UCableComponent*
ATwoDRopeLiftBar::GetLiftCableForSide(
	ETwoDRopeSide Side
) const
{
	switch (Side)
	{
	case ETwoDRopeSide::Left:
		return LeftLiftCable;

	case ETwoDRopeSide::Right:
		return RightLiftCable;

	default:
		return nullptr;
	}
}

ACharacter*
ATwoDRopeLiftBar::GetHolderForSide(
	ETwoDRopeSide Side
) const
{
	switch (Side)
	{
	case ETwoDRopeSide::Left:
		return LeftHolder.Get();

	case ETwoDRopeSide::Right:
		return RightHolder.Get();

	default:
		return nullptr;
	}
}

float ATwoDRopeLiftBar::GetRopeLengthForSide(
	ETwoDRopeSide Side
) const
{
	switch (Side)
	{
	case ETwoDRopeSide::Left:
		return LeftRopeLength;

	case ETwoDRopeSide::Right:
		return RightRopeLength;

	default:
		return 0.0f;
	}
}

float ATwoDRopeLiftBar::GetAbsoluteMaximumHandleReach(
	ETwoDRopeSide Side
) const
{
	if (Side == ETwoDRopeSide::None)
	{
		return 0.0f;
	}

	return
		GetFreeHandleRopeLength() +
		GetMaximumProjectedPullDistance();
}

void ATwoDRopeLiftBar::RefreshCableBindings()
{
	RefreshCableBindingForSide(
		ETwoDRopeSide::Left
	);
	RefreshCableBindingForSide(
		ETwoDRopeSide::Right
	);
}

void ATwoDRopeLiftBar::RefreshCableBindingForSide(
	ETwoDRopeSide Side
)
{
	UCableComponent* Cable =
		GetHandleCableForSide(Side);

	USphereComponent* Handle =
		GetHandleForSide(Side);

	ACharacter* Holder =
		GetHolderForSide(Side);

	if (
		!IsValid(Cable) ||
		!IsValid(Handle)
		)
	{
		return;
	}

	if (
		IsValid(Holder) &&
		AttachCableEndToCharacterHand(
			Cable,
			Holder
		)
		)
	{
		return;
	}

	Cable->SetAttachEndToComponent(
		Handle,
		NAME_None
	);
	Cable->EndLocation =
		FVector::ZeroVector;
	Cable->MarkRenderStateDirty();
}

void ATwoDRopeLiftBar::RefreshLiftCableBindings()
{
	if (
		IsValid(LeftLiftCable) &&
		IsValid(LeftPulleyAnchor)
		)
	{
		LeftLiftCable->SetAttachEndToComponent(
			LeftPulleyAnchor,
			NAME_None
		);
		LeftLiftCable->EndLocation =
			FVector::ZeroVector;
		LeftLiftCable->MarkRenderStateDirty();
	}

	if (
		IsValid(RightLiftCable) &&
		IsValid(RightPulleyAnchor)
		)
	{
		RightLiftCable->SetAttachEndToComponent(
			RightPulleyAnchor,
			NAME_None
		);
		RightLiftCable->EndLocation =
			FVector::ZeroVector;
		RightLiftCable->MarkRenderStateDirty();
	}
}

bool ATwoDRopeLiftBar::AttachCableEndToCharacterHand(
	UCableComponent* Cable,
	ACharacter* Holder
) const
{
	if (
		!IsValid(Cable) ||
		!IsValid(Holder)
		)
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh =
		Holder->GetMesh();

	if (
		!IsValid(CharacterMesh) ||
		RopeGrabSocketName == NAME_None ||
		!CharacterMesh->DoesSocketExist(
			RopeGrabSocketName
		)
		)
	{
		return false;
	}

	Cable->SetAttachEndToComponent(
		CharacterMesh,
		RopeGrabSocketName
	);
	Cable->EndLocation =
		FVector::ZeroVector;
	Cable->MarkRenderStateDirty();

	return true;
}

void ATwoDRopeLiftBar::UpdateCableLengths()
{
	/*
	 * 이 함수는 Cable의 육안 표현만 변경한다.
	 *
	 * 막대기 이동, 당김 거리, 좌우 매핑, 최대거리,
	 * 서버 권한 게임 로직은 기존 v13.1 그대로다.
	 */
	const auto ApplyCableVisualState =
		[this](
			UCableComponent* Cable,
			const FVector& Start,
			const FVector& End,
			float ReleaseAlpha,
			bool bForceTaut
			)
		{
			if (!IsValid(Cable))
			{
				return;
			}

			const float SafeReleaseAlpha =
				bForceTaut
				? 0.0f
				: FMath::Clamp(
					ReleaseAlpha,
					0.0f,
					1.0f
				);

			const bool bTaut =
				SafeReleaseAlpha <=
				KINDA_SMALL_NUMBER;

			const float StraightDistance =
				FVector::Distance(
					Start,
					End
				);

			const float ExtraSlack =
				ReleasingCableExtraSlack *
				SafeReleaseAlpha;

			Cable->CableLength =
				FMath::Max(
					(
						StraightDistance +
						ExtraSlack
						) *
					(
						bTaut
						? TautCableLengthScale
						: 1.0f
						),
					10.0f
				);

			Cable->CableGravityScale =
				FMath::Lerp(
					TautCableGravityScale,
					ReleasingCableGravityScale,
					SafeReleaseAlpha
				);

			Cable->SolverIterations =
				bTaut
				? TautCableSolverIterations
				: CableSolverIterations;

			/*
			 * 늘어진 상태에서도 과하게 흐물거리지 않게
			 * Stiffness 자체는 계속 유지한다.
			 */
			Cable->bEnableStiffness = true;

			Cable->CableWidth =
				CableWidth *
				FMath::Lerp(
					TautCableWidthScale,
					1.0f,
					SafeReleaseAlpha
				);
		};

	/*
	 * 막대기 끝에서 위 도르래까지의 줄은
	 * 발판을 매달고 있으므로 항상 팽팽하게 표시한다.
	 */
	if (
		IsValid(LeftLiftCable) &&
		IsValid(LeftBarEnd) &&
		IsValid(LeftPulleyAnchor)
		)
	{
		ApplyCableVisualState(
			LeftLiftCable,
			LeftBarEnd->GetComponentLocation(),
			LeftPulleyAnchor->
			GetComponentLocation(),
			0.0f,
			true
		);
	}

	if (
		IsValid(RightLiftCable) &&
		IsValid(RightBarEnd) &&
		IsValid(RightPulleyAnchor)
		)
	{
		ApplyCableVisualState(
			RightLiftCable,
			RightBarEnd->GetComponentLocation(),
			RightPulleyAnchor->
			GetComponentLocation(),
			0.0f,
			true
		);
	}

	/*
	 * 서버에서만 실제 하강 상태를 계산하고,
	 * 클라이언트는 복제된 ReleaseAlpha를 사용한다.
	 */
	if (HasAuthority())
	{
		const float LeftTargetHeight =
			IsValid(LeftHolder.Get())
			? CalculatePullAlpha(
				ETwoDRopeSide::Left
			) *
			GetEffectiveMaxEndpointLiftDistance()
			: 0.0f;

		const float RightTargetHeight =
			IsValid(RightHolder.Get())
			? CalculatePullAlpha(
				ETwoDRopeSide::Right
			) *
			GetEffectiveMaxEndpointLiftDistance()
			: 0.0f;

		const float LeftReleaseDifference =
			CurrentLeftLiftHeight -
			LeftTargetHeight;

		const float RightReleaseDifference =
			CurrentRightLiftHeight -
			RightTargetHeight;

		RepLeftCableReleaseAlpha =
			LeftReleaseDifference >
			CableTensionSettleTolerance
			? FMath::Clamp(
				LeftReleaseDifference /
				FMath::Max(
					ReleasingSlackFullRange,
					1.0f
				),
				0.0f,
				1.0f
			)
			: 0.0f;

		RepRightCableReleaseAlpha =
			RightReleaseDifference >
			CableTensionSettleTolerance
			? FMath::Clamp(
				RightReleaseDifference /
				FMath::Max(
					ReleasingSlackFullRange,
					1.0f
				),
				0.0f,
				1.0f
			)
			: 0.0f;
	}

	/*
	 * 손잡이 쪽 줄:
	 *
	 * 당기는 중 / 당긴 상태 유지 / 완전히 다 내려온 상태:
	 * ReleaseAlpha = 0 -> 팽팽
	 *
	 * 캐릭터가 가까워져 막대기가 내려오는 도중:
	 * ReleaseAlpha > 0 -> 잠시 헐렁
	 */
	if (
		IsValid(LeftCable) &&
		IsValid(LeftPulleyAnchor) &&
		IsValid(LeftHandleBody)
		)
	{
		const ACharacter* Holder =
			LeftHolder.Get();

		if (IsValid(Holder))
		{
			ApplyCableVisualState(
				LeftCable,
				LeftPulleyAnchor->
				GetComponentLocation(),
				GetCharacterGrabLocation(
					Holder
				),
				RepLeftCableReleaseAlpha,
				false
			);
		}
		else
		{
			/*
			 * 놓인 손잡이 줄은 실제 고정 길이를 유지한다.
			 *
			 * 손잡이가 공중에서 최대거리까지 늘어나면 팽팽하고,
			 * 바닥이 더 가까우면 남는 길이만큼 자연스럽게 처진다.
			 */
			const float StraightDistance =
				FVector::Distance(
					LeftPulleyAnchor->
					GetComponentLocation(),
					LeftHandleBody->
					GetComponentLocation()
				);

			LeftCable->CableLength =
				FMath::Max(
					GetFreeHandleRopeLength(),
					StraightDistance
				);

			LeftCable->CableGravityScale =
				ReleasingCableGravityScale;

			LeftCable->SolverIterations =
				CableSolverIterations;

			LeftCable->bEnableStiffness = true;
			LeftCable->CableWidth = CableWidth;
		}
	}

	if (
		IsValid(RightCable) &&
		IsValid(RightPulleyAnchor) &&
		IsValid(RightHandleBody)
		)
	{
		const ACharacter* Holder =
			RightHolder.Get();

		if (IsValid(Holder))
		{
			ApplyCableVisualState(
				RightCable,
				RightPulleyAnchor->
				GetComponentLocation(),
				GetCharacterGrabLocation(
					Holder
				),
				RepRightCableReleaseAlpha,
				false
			);
		}
		else
		{
			const float StraightDistance =
				FVector::Distance(
					RightPulleyAnchor->
					GetComponentLocation(),
					RightHandleBody->
					GetComponentLocation()
				);

			RightCable->CableLength =
				FMath::Max(
					GetFreeHandleRopeLength(),
					StraightDistance
				);

			RightCable->CableGravityScale =
				ReleasingCableGravityScale;

			RightCable->SolverIterations =
				CableSolverIterations;

			RightCable->bEnableStiffness = true;
			RightCable->CableWidth = CableWidth;
		}
	}
}

void ATwoDRopeLiftBar::CleanupInvalidHolders()
{
	bool bChanged = false;

	if (
		LeftHolder.Get() != nullptr &&
		!IsValid(LeftHolder.Get())
		)
	{
		StopHoldingSide(
			ETwoDRopeSide::Left
		);
		LeftHolder = nullptr;
		bChanged = true;
	}

	if (
		RightHolder.Get() != nullptr &&
		!IsValid(RightHolder.Get())
		)
	{
		StopHoldingSide(
			ETwoDRopeSide::Right
		);
		RightHolder = nullptr;
		bChanged = true;
	}

	if (bChanged)
	{
		RefreshCableBindings();
		ForceNetUpdate();
	}
}

void ATwoDRopeLiftBar::UpdatePulleyRodPreview()
{
	if (!IsValid(PulleyRodVisual))
	{
		return;
	}

	const float EffectivePulleyHalfSpacing =
		GetEffectivePulleyAnchorHalfSpacing();

	PulleyRodVisual->SetRelativeLocation(
		FVector(
			0.0f,
			PulleyDepthOffset,
			GetEffectivePulleyHeight()
		)
	);

	PulleyRodVisual->SetRelativeRotation(
		FRotator::ZeroRotator
	);

	const FVector RodHalfExtent(
		FMath::Max(
			EffectivePulleyHalfSpacing +
			PulleyRodExtraHalfLength,
			10.0f
		),
		PulleyRodHalfWidth,
		PulleyRodHalfThickness
	);

	ScaleVisualToHalfExtent(
		PulleyRodVisual,
		RodHalfExtent
	);
}

void ATwoDRopeLiftBar::ScaleVisualToHalfExtent(
	UStaticMeshComponent* Visual,
	const FVector& DesiredHalfExtent
) const
{
	if (!IsValid(Visual))
	{
		return;
	}

	const UStaticMesh* StaticMesh =
		Visual->GetStaticMesh();

	if (!IsValid(StaticMesh))
	{
		return;
	}

	const FVector MeshHalfExtent =
		StaticMesh->GetBounds().BoxExtent;

	const FVector SafeMeshHalfExtent(
		FMath::Max(
			MeshHalfExtent.X,
			KINDA_SMALL_NUMBER
		),
		FMath::Max(
			MeshHalfExtent.Y,
			KINDA_SMALL_NUMBER
		),
		FMath::Max(
			MeshHalfExtent.Z,
			KINDA_SMALL_NUMBER
		)
	);

	Visual->SetRelativeScale3D(
		FVector(
			DesiredHalfExtent.X /
			SafeMeshHalfExtent.X,
			DesiredHalfExtent.Y /
			SafeMeshHalfExtent.Y,
			DesiredHalfExtent.Z /
			SafeMeshHalfExtent.Z
		)
	);
}

void ATwoDRopeLiftBar::UpdateServerNetworkVisualState(
	bool bForceUpdate
)
{
	if (!HasAuthority())
	{
		return;
	}

	if (IsValid(BarBody))
	{
		RepBarWorldLocation =
			BarBody->GetComponentLocation();
		RepBarWorldRotation =
			BarBody->GetComponentRotation();
	}

	if (IsValid(LeftHandleBody))
	{
		RepLeftHandleWorldLocation =
			LeftHandleBody->GetComponentLocation();
		RepLeftHandleWorldRotation =
			LeftHandleBody->GetComponentRotation();
	}

	if (IsValid(RightHandleBody))
	{
		RepRightHandleWorldLocation =
			RightHandleBody->GetComponentLocation();
		RepRightHandleWorldRotation =
			RightHandleBody->GetComponentRotation();
	}

	if (bForceUpdate)
	{
		ForceNetUpdate();
	}
}

void ATwoDRopeLiftBar::UpdateClientNetworkSmoothing(
	float DeltaSeconds
)
{
	if (
		HasAuthority() ||
		!bHasReceivedNetworkVisualState ||
		DeltaSeconds <= KINDA_SMALL_NUMBER
		)
	{
		return;
	}

	if (IsValid(BarBody))
	{
		const FVector TargetLocation =
			FVector(RepBarWorldLocation);

		const float DistanceSquared =
			FVector::DistSquared(
				BarBody->GetComponentLocation(),
				TargetLocation
			);

		const FVector NewLocation =
			DistanceSquared >
			FMath::Square(ClientNetworkSnapDistance)
			? TargetLocation
			: FMath::VInterpTo(
				BarBody->GetComponentLocation(),
				TargetLocation,
				DeltaSeconds,
				ClientBarLocationSmoothingSpeed
			);

		const FQuat NewRotation =
			FMath::QInterpTo(
				BarBody->GetComponentQuat(),
				RepBarWorldRotation.Quaternion(),
				DeltaSeconds,
				ClientBarRotationSmoothingSpeed
			);

		BarBody->SetWorldLocationAndRotation(
			NewLocation,
			NewRotation.Rotator(),
			false,
			nullptr,
			ETeleportType::None
		);
	}

	SmoothClientHandleForSide(
		ETwoDRopeSide::Left,
		DeltaSeconds
	);

	SmoothClientHandleForSide(
		ETwoDRopeSide::Right,
		DeltaSeconds
	);
}

void ATwoDRopeLiftBar::SmoothClientHandleForSide(
	ETwoDRopeSide Side,
	float DeltaSeconds
)
{
	USphereComponent* Handle =
		GetHandleForSide(Side);

	if (!IsValid(Handle))
	{
		return;
	}

	const ACharacter* Holder =
		GetHolderForSide(Side);

	const FVector ReplicatedLocation =
		Side == ETwoDRopeSide::Left
		? FVector(RepLeftHandleWorldLocation)
		: FVector(RepRightHandleWorldLocation);

	const FRotator ReplicatedRotation =
		Side == ETwoDRopeSide::Left
		? RepLeftHandleWorldRotation
		: RepRightHandleWorldRotation;

	/*
	 * 잡고 있는 손잡이는 네트워크 왕복을 기다리지 않고
	 * 이미 네트워크 스무딩된 Character 손 위치를 직접 따른다.
	 */
	const FVector TargetLocation =
		IsValid(Holder)
		? GetCharacterGrabLocation(Holder)
		: ReplicatedLocation;

	const float DistanceSquared =
		FVector::DistSquared(
			Handle->GetComponentLocation(),
			TargetLocation
		);

	const FVector NewLocation =
		DistanceSquared >
		FMath::Square(ClientNetworkSnapDistance)
		? TargetLocation
		: FMath::VInterpTo(
			Handle->GetComponentLocation(),
			TargetLocation,
			DeltaSeconds,
			ClientHandleSmoothingSpeed
		);

	const FQuat NewRotation =
		FMath::QInterpTo(
			Handle->GetComponentQuat(),
			ReplicatedRotation.Quaternion(),
			DeltaSeconds,
			ClientHandleSmoothingSpeed
		);

	Handle->SetWorldLocationAndRotation(
		NewLocation,
		NewRotation.Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);
}

void ATwoDRopeLiftBar::OnRep_NetworkVisualState()
{
	bHasReceivedNetworkVisualState = true;
}

void ATwoDRopeLiftBar::OnRep_LeftHolder()
{
	bHasReceivedNetworkVisualState = true;

	RefreshCableBindingForSide(
		ETwoDRopeSide::Left
	);
}

void ATwoDRopeLiftBar::OnRep_RightHolder()
{
	bHasReceivedNetworkVisualState = true;

	RefreshCableBindingForSide(
		ETwoDRopeSide::Right
	);
}

void ATwoDRopeLiftBar::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(
		OutLifetimeProps
	);

	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		LeftHolder
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RightHolder
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		LeftRopeLength
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RightRopeLength
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RepLeftCableReleaseAlpha
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RepRightCableReleaseAlpha
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RepBarWorldLocation
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RepBarWorldRotation
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RepLeftHandleWorldLocation
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RepLeftHandleWorldRotation
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RepRightHandleWorldLocation
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RepRightHandleWorldRotation
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		LeftGrabStartPlanarDistance
	);
	DOREPLIFETIME(
		ATwoDRopeLiftBar,
		RightGrabStartPlanarDistance
	);
}