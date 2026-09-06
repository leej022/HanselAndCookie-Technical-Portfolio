// RiverFlowVolume.cpp
#include "RiverFlowVolume.h"
#include "Components/PrimitiveComponent.h"

ARiverFlowVolume::ARiverFlowVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	// 서버/클라 둘 다에서 계산(클라 예측)
	bReplicates = true;
	SetReplicateMovement(true);

	RiverBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RiverBounds"));
	RootComponent = RiverBounds;
	RiverBounds->SetCollisionProfileName(TEXT("Trigger"));

	FlowDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FlowDirectionArrow"));
	FlowDirectionArrow->SetupAttachment(RootComponent);

	// ===== 기본값(“살짝 트램펄린” 버전) =====
	FlowStrength = 600.0f;
	MaxFlowSpeed = 300.0f;

	TargetFloatingZ = 150.0f;

	// ✅ 통통함 늘리기: 스프링은 조금↑, 댐핑은 조금↓, 상한은 약간↑
	BuoyancyStiffness = 200.0f;          // 위치 오차 복원력(살짝 탄력)
	BuoyancyDamping = 2.0f;             // 낮추면 더 통통 (너무 낮추면 다시 뛰용뛰용)

	MaxBuoyancyDepth = 210.0f;          // 깊게 떨어질 때 오차 제한(폭발 방지)
	MaxUpBuoyancyForceZ = 9000.0f;      // 위로 미는 상한(조금 더 허용)
	MaxDownBuoyancyForceZ = 2800.0f;    // 아래로 누르는 상한

	AboveTargetStiffnessFactor = 0.15f; // 목표 위에서는 “덜 잡아당김” → 살짝 오버슈트(물 느낌)

	// 위로 너무 과하게 튀는 것만 막는 안전장치(임계치 높여서 더 통통 가능)
	MaxUpwardSpeed = 420.0f;
	UpwardSpeedDamping = 6.0f;

	MaxAngularVelocity = 100.0f;
	bIgnoreMass = true;
}

void ARiverFlowVolume::BeginPlay()
{
	Super::BeginPlay();
}

void ARiverFlowVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TArray<UPrimitiveComponent*> OverlappingComps;
	RiverBounds->GetOverlappingComponents(OverlappingComps);

	const FVector FlowDir = FlowDirectionArrow->GetForwardVector().GetSafeNormal();

	for (UPrimitiveComponent* Comp : OverlappingComps)
	{
		if (!Comp || !Comp->IsSimulatingPhysics())
			continue;

		Comp->WakeRigidBody();

		const FVector Vel = Comp->GetPhysicsLinearVelocity();

		// ===== 1) 유속 적용(최대 속도 제한) =====
		{
			const float Along = FVector::DotProduct(Vel, FlowDir);
			if (Along < MaxFlowSpeed)
			{
				Comp->AddForce(FlowDir * FlowStrength, NAME_None, bIgnoreMass);
			}
		}

		// ===== 2) 부력(스프링-댐퍼 + 클램프) =====
		{
			const float CurrentZ = Comp->GetComponentLocation().Z;
			const float ErrorZ = TargetFloatingZ - CurrentZ; // +면 아래(올려야 함), -면 위(내려야 함)

			// 아래로 많이 떨어졌을 때 반발력 폭발 방지(오차 제한)
			float ClampedErrorZ = ErrorZ;
			if (ErrorZ > 0.f)
			{
				ClampedErrorZ = FMath::Min(ErrorZ, MaxBuoyancyDepth);
			}

			// 목표보다 위는 더 부드럽게(잡아당김 약하게) → “물 위 통통”
			const float K = (ErrorZ >= 0.f)
				? BuoyancyStiffness
				: (BuoyancyStiffness * AboveTargetStiffnessFactor);

			// 스프링-댐퍼: Fz = K*Error - C*VelZ
			float ForceZ = (K * ClampedErrorZ) - (BuoyancyDamping * Vel.Z);

			// 위로 튀는 속도가 너무 큰 경우만 살짝 제동(임계치 높여서 “살짝 트램펄린” 허용)
			if (Vel.Z > MaxUpwardSpeed)
			{
				ForceZ -= (Vel.Z - MaxUpwardSpeed) * UpwardSpeedDamping;
			}

			// 최종 상/하한(안전장치)
			ForceZ = FMath::Clamp(ForceZ, -MaxDownBuoyancyForceZ, MaxUpBuoyancyForceZ);

			Comp->AddForce(FVector(0.f, 0.f, ForceZ), NAME_None, bIgnoreMass);
		}

		// ===== 3) 회전 속도 제한 =====
		{
			const FVector AngVel = Comp->GetPhysicsAngularVelocityInDegrees();
			if (AngVel.Size() > MaxAngularVelocity)
			{
				Comp->SetPhysicsAngularVelocityInDegrees(
					AngVel.GetClampedToSize(0.0f, MaxAngularVelocity)
				);
			}
		}
	}
}
