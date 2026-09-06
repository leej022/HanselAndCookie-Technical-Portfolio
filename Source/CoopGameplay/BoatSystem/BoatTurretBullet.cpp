#include "BoatTurretBullet.h"

#include "Boat.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ABoatTurretBullet::ABoatTurretBullet()
{
	PrimaryActorTick.bCanEverTick = false;

	/*
	 * 총알은 서버에서 생성되고
	 * 이동과 제거가 클라이언트에 복제된다.
	 */
	bReplicates = true;
	SetReplicateMovement(true);

	NetUpdateFrequency = 30.0f;
	MinNetUpdateFrequency = 15.0f;

	// =====================================================
	// Bullet Collision
	// =====================================================

	BoatBulletCollision =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("BoatBulletCollision")
		);

	SetRootComponent(BoatBulletCollision);

	BoatBulletCollision->InitSphereRadius(12.0f);

	BoatBulletCollision->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly
	);

	/*
	 * 보트 Trigger가 ECC_WorldDynamic에 Overlap하도록
	 * 설정되어 있으므로 총알도 WorldDynamic으로 둔다.
	 */
	BoatBulletCollision->SetCollisionObjectType(
		ECC_WorldDynamic
	);

	BoatBulletCollision->SetCollisionResponseToAllChannels(
		ECR_Ignore
	);

	/*
	 * BoatMesh가 PhysicsActor 프로필이므로
	 * PhysicsBody를 Block한다.
	 *
	 * BoatMesh 본체 충돌:
	 * HandleBoatBulletHit 호출
	 */
	BoatBulletCollision->SetCollisionResponseToChannel(
		ECC_PhysicsBody,
		ECR_Block
	);

	/*
	 * 벽과 일반 장애물은 Block.
	 */
	BoatBulletCollision->SetCollisionResponseToChannel(
		ECC_WorldStatic,
		ECR_Block
	);

	BoatBulletCollision->SetCollisionResponseToChannel(
		ECC_WorldDynamic,
		ECR_Block
	);

	/*
	 * 보트의 BoardTrigger처럼 상대 컴포넌트가
	 * WorldDynamic을 Overlap하도록 설정되어 있으면
	 * 최종 결과는 Overlap이 되므로 이것도 켜야 한다.
	 */
	BoatBulletCollision->SetGenerateOverlapEvents(true);

	/*
	 * Block 충돌 시 Hit 이벤트 발생.
	 */
	BoatBulletCollision->SetNotifyRigidBodyCollision(true);

	BoatBulletCollision->SetCanEverAffectNavigation(false);

	BoatBulletCollision->OnComponentHit.AddDynamic(
		this,
		&ABoatTurretBullet::HandleBoatBulletHit
	);

	BoatBulletCollision->OnComponentBeginOverlap.AddDynamic(
		this,
		&ABoatTurretBullet::HandleBoatBulletOverlap
	);

	// =====================================================
	// Bullet Mesh
	// =====================================================

	BoatBulletMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("BoatBulletMesh")
		);

	BoatBulletMesh->SetupAttachment(
		BoatBulletCollision
	);

	/*
	 * 메시에는 충돌을 넣지 않는다.
	 * Collision Sphere 하나만 충돌을 담당한다.
	 */
	BoatBulletMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);

	BoatBulletMesh->SetGenerateOverlapEvents(false);
	BoatBulletMesh->SetCanEverAffectNavigation(false);

	// =====================================================
	// Projectile Movement
	// =====================================================

	BoatProjectileMovement =
		CreateDefaultSubobject<UProjectileMovementComponent>(
			TEXT("BoatProjectileMovement")
		);

	BoatProjectileMovement->SetUpdatedComponent(
		BoatBulletCollision
	);

	/*
	 * 고정된 BP Velocity를 사용하지 않는다.
	 * LaunchBoatBullet에서 계산된 방향을 직접 넣는다.
	 */
	BoatProjectileMovement->InitialSpeed = 0.0f;
	BoatProjectileMovement->MaxSpeed = BoatBulletSpeed;
	BoatProjectileMovement->Velocity = FVector::ZeroVector;

	BoatProjectileMovement->ProjectileGravityScale = 0.0f;
	BoatProjectileMovement->bRotationFollowsVelocity = true;
	BoatProjectileMovement->bShouldBounce = false;

	/*
	 * 전달받은 방향은 월드 방향이다.
	 */
	BoatProjectileMovement->bInitialVelocityInLocalSpace = false;

	/*
	 * 빠른 총알이 보트를 통과하지 않게 Sweep 충돌을 사용한다.
	 */
	BoatProjectileMovement->bSweepCollision = true;

	/*
	 * InitializeBoatBullet로 방향을 받기 전에
	 * 자동으로 출발하지 않게 한다.
	 */
	BoatProjectileMovement->bAutoActivate = false;

	// TeleportBoatTrigger와 동일한 기본 죽음 설정
	DeathSequenceDuration = 3.0f;
	InputLockSeconds = 0.6f;
	bZeroVelAfterRespawn = true;
}

void ABoatTurretBullet::InitializeBoatBullet(
	const FVector& NewWorldDirection)
{
	FVector HorizontalDirection(
		NewWorldDirection.X,
		NewWorldDirection.Y,
		0.0f
	);

	if (!HorizontalDirection.Normalize())
	{
		HorizontalDirection =
			GetActorForwardVector();

		HorizontalDirection.Z = 0.0f;

		if (!HorizontalDirection.Normalize())
		{
			HorizontalDirection =
				FVector::ForwardVector;
		}
	}

	BoatLaunchDirection =
		HorizontalDirection;

	bBoatLaunchDirectionInitialized = true;

	/*
	 * 총알 메시도 날아가는 방향을 바라보게 한다.
	 */
	SetActorRotation(
		BoatLaunchDirection.Rotation()
	);
}

void ABoatTurretBullet::BeginPlay()
{
	Super::BeginPlay();

	/*
	 * BP에 예전 Collision 값이 저장돼 있어도
	 * 실행 시 필요한 설정으로 다시 덮어쓴다.
	 */
	if (BoatBulletCollision)
	{
		BoatBulletCollision->SetCollisionEnabled(
			ECollisionEnabled::QueryOnly
		);

		BoatBulletCollision->SetCollisionObjectType(
			ECC_WorldDynamic
		);

		BoatBulletCollision->SetCollisionResponseToAllChannels(
			ECR_Ignore
		);

		BoatBulletCollision->SetCollisionResponseToChannel(
			ECC_PhysicsBody,
			ECR_Block
		);

		BoatBulletCollision->SetCollisionResponseToChannel(
			ECC_WorldStatic,
			ECR_Block
		);

		BoatBulletCollision->SetCollisionResponseToChannel(
			ECC_WorldDynamic,
			ECR_Block
		);

		BoatBulletCollision->SetGenerateOverlapEvents(true);
		BoatBulletCollision->SetNotifyRigidBodyCollision(true);

		if (GetOwner())
		{
			/*
			 * 생성 직후 자기 기관총과 충돌하지 않게 한다.
			 */
			BoatBulletCollision->IgnoreActorWhenMoving(
				GetOwner(),
				true
			);
		}
	}

	/*
	 * 복제된 클라이언트 총알은 InitializeBoatBullet이
	 * 직접 호출되지 않을 수 있으므로 SpawnRotation을 사용한다.
	 */
	if (!bBoatLaunchDirectionInitialized)
	{
		BoatLaunchDirection =
			GetActorForwardVector();

		BoatLaunchDirection.Z = 0.0f;

		if (!BoatLaunchDirection.Normalize())
		{
			BoatLaunchDirection =
				FVector::ForwardVector;
		}
	}

	LaunchBoatBullet();

	SetLifeSpan(
		FMath::Max(
			0.1f,
			BoatBulletLifeSeconds
		)
	);
}

void ABoatTurretBullet::LaunchBoatBullet()
{
	if (!BoatProjectileMovement ||
		!BoatBulletCollision)
	{
		return;
	}

	const FVector FinalDirection =
		BoatLaunchDirection.GetSafeNormal();

	if (FinalDirection.IsNearlyZero())
	{
		return;
	}

	const float FinalSpeed =
		FMath::Max(
			1.0f,
			BoatBulletSpeed
		);

	BoatProjectileMovement->Deactivate();

	BoatProjectileMovement->SetUpdatedComponent(
		BoatBulletCollision
	);

	BoatProjectileMovement->InitialSpeed = 0.0f;
	BoatProjectileMovement->MaxSpeed = FinalSpeed;
	BoatProjectileMovement->ProjectileGravityScale = 0.0f;

	/*
	 * 발사 순간 계산된 보트 방향을
	 * 월드 Velocity로 직접 입력한다.
	 */
	BoatProjectileMovement->Velocity =
		FinalDirection * FinalSpeed;

	BoatProjectileMovement->UpdateComponentVelocity();
	BoatProjectileMovement->Activate(true);
}

ABoat* ABoatTurretBullet::ResolveBoat(
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent) const
{
	/*
	 * TeleportBoatTrigger와 동일하게
	 * 먼저 충돌한 Actor를 Boat로 변환한다.
	 */
	if (ABoat* Boat = Cast<ABoat>(OtherActor))
	{
		return Boat;
	}

	/*
	 * Actor가 Boat로 잡히지 않으면
	 * 충돌한 컴포넌트의 Owner를 확인한다.
	 *
	 * BoatMesh, BoardTriggerLeft, BoardTriggerRight 등은
	 * 모두 Owner가 ABoat다.
	 */
	if (OtherComponent)
	{
		if (ABoat* Boat =
			Cast<ABoat>(OtherComponent->GetOwner()))
		{
			return Boat;
		}
	}

	/*
	 * 혹시 보트에 부착된 별도 Actor에 맞은 경우까지 확인.
	 */
	if (IsValid(OtherActor))
	{
		if (ABoat* ParentBoat =
			Cast<ABoat>(
				OtherActor->GetAttachParentActor()
			))
		{
			return ParentBoat;
		}

		if (ABoat* OwnerBoat =
			Cast<ABoat>(
				OtherActor->GetOwner()
			))
		{
			return OwnerBoat;
		}
	}

	return nullptr;
}

bool ABoatTurretBullet::TryProcessBoatImpact(
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent)
{
	/*
	 * 죽음 판정은 서버에서만 실행한다.
	 */
	if (!HasAuthority())
	{
		return false;
	}

	/*
	 * Hit와 Overlap이 동시에 들어오더라도
	 * 한 총알에서는 한 번만 처리한다.
	 */
	if (bImpactProcessed)
	{
		return false;
	}

	ABoat* HitBoat =
		ResolveBoat(
			OtherActor,
			OtherComponent
		);

	if (!IsValid(HitBoat))
	{
		return false;
	}

	bImpactProcessed = true;

	/*
	 * 같은 총알에서 추가 충돌 이벤트가 발생하지 않게
	 * 이동과 Collision을 먼저 정지한다.
	 */
	if (BoatProjectileMovement)
	{
		BoatProjectileMovement->StopMovementImmediately();
		BoatProjectileMovement->Deactivate();
	}

	if (BoatBulletCollision)
	{
		BoatBulletCollision->SetCollisionEnabled(
			ECollisionEnabled::NoCollision
		);

		BoatBulletCollision->SetGenerateOverlapEvents(false);
	}

	/*
	 * 핵심:
	 * TeleportBoatTrigger와 완전히 같은 방식으로
	 * 기존 Boat 죽음 함수를 호출한다.
	 *
	 * Boat 내부에서 이미 죽는 중인지 검사하므로
	 * 죽는 연출 중 추가 총알에 맞아도 재실행되지 않는다.
	 * 리스폰 완료 후에는 Boat 내부 상태가 복구되어 다시 죽을 수 있다.
	 */
	HitBoat->StartDeathRespawnSequence_Server(
		DeathSequenceDuration,
		InputLockSeconds,
		bZeroVelAfterRespawn
	);

	Destroy();

	return true;
}

void ABoatTurretBullet::HandleBoatBulletOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(OtherActor) ||
		OtherActor == this ||
		OtherActor == GetOwner())
	{
		return;
	}

	/*
	 * BoardTrigger 등 보트의 Overlap 컴포넌트에 닿았을 때
	 * 기존 Boat 죽음 함수 호출.
	 *
	 * Boat가 아닌 일반 Overlap 오브젝트면 통과한다.
	 */
	TryProcessBoatImpact(
		OtherActor,
		OtherComponent
	);
}

void ABoatTurretBullet::HandleBoatBulletHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bImpactProcessed)
	{
		return;
	}

	if (!IsValid(OtherActor) ||
		OtherActor == this ||
		OtherActor == GetOwner())
	{
		return;
	}

	/*
	 * BoatMesh 본체에 Block 충돌한 경우
	 * 기존 Boat 죽음 함수 호출.
	 */
	if (TryProcessBoatImpact(
		OtherActor,
		OtherComponent))
	{
		return;
	}

	/*
	 * Boat가 아니라 벽이나 장애물에 Block 충돌한 경우
	 * 총알만 제거한다.
	 */
	bImpactProcessed = true;

	if (BoatProjectileMovement)
	{
		BoatProjectileMovement->StopMovementImmediately();
		BoatProjectileMovement->Deactivate();
	}

	if (BoatBulletCollision)
	{
		BoatBulletCollision->SetCollisionEnabled(
			ECollisionEnabled::NoCollision
		);
	}

	Destroy();
}