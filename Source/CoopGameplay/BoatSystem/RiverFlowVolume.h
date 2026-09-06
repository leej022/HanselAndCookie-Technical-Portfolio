// RiverFlowVolume.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "RiverFlowVolume.generated.h"

UCLASS()
class STEAMDEVELOPMENT_API ARiverFlowVolume : public AActor
{
	GENERATED_BODY()

public:
	ARiverFlowVolume();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/** 강 유역 범위 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "River")
	UBoxComponent* RiverBounds;

	/** 유속 방향 표시기 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "River")
	UArrowComponent* FlowDirectionArrow;

	/** 밀어내는 힘의 세기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Flow")
	float FlowStrength;

	/** 제한할 최대 유속 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Flow")
	float MaxFlowSpeed;

	/** 보트가 떠 있을 절대적인 목표 높이 (Z값) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy")
	float TargetFloatingZ;

	/** 부력 스프링 강도(위치 오차 -> 힘/가속도) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy")
	float BuoyancyStiffness;

	/** 부력 감쇠(수직 속도 -> 힘). 낮출수록 트램펄린 느낌 증가 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy")
	float BuoyancyDamping;

	/** 목표 Z 아래로 이 깊이(cm)까지만 오차 인정(폭발 튐 방지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy")
	float MaxBuoyancyDepth;

	/** 위로 밀어올리는 최대 힘(또는 최대 가속도) 상한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy")
	float MaxUpBuoyancyForceZ;

	/** 아래로 누르는 최대 힘(또는 최대 가속도) 상한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy")
	float MaxDownBuoyancyForceZ;

	/** 목표보다 위에 있을 때 내려앉히는 강도 비율(0~1). 낮을수록 더 “물 위에서 살짝 통통” */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AboveTargetStiffnessFactor;

	/** 위로 올라가는 속도가 너무 커지면 추가 감쇠를 걸기 시작하는 임계치(cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy")
	float MaxUpwardSpeed;

	/** MaxUpwardSpeed 초과분에 추가로 거는 감쇠(낮을수록 더 통통) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Buoyancy")
	float UpwardSpeedDamping;

	/** 회전 속도 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Safety")
	float MaxAngularVelocity;

	/** 질량 무시 여부 (true면 가속도 개념으로 적용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Settings|Safety")
	bool bIgnoreMass;
};
