#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TwoDRopeLiftBar.generated.h"

class ACharacter;
class UCableComponent;
class UBoxComponent;
class UPhysicsHandleComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETwoDRopeSide : uint8
{
	None	UMETA(DisplayName = "None"),
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right")
};

/**
 * 2인 협동 공 운반용 양쪽 독립 도르레 발판.
 *
 * 게임 규칙:
 * - 각 손잡이를 잡은 순간의 도르래-캐릭터 평면거리를 기준으로 사용한다.
 * - 도르래에서 멀어지면 같은 쪽 막대기 끝이 올라간다.
 * - 도르래 쪽으로 가까워지면 같은 쪽 힘이 풀리고 막대기가 내려간다.
 * - 왼쪽 플레이어는 왼쪽 끝만, 오른쪽 플레이어는 오른쪽 끝만 담당한다.
 * - 양쪽 당김량의 차이로 발판 기울기가 결정된다.
 * - 양쪽을 비슷하게 당기면 발판 전체가 올라간다.
 *
 * 안정성 규칙:
 * - Physics Constraint Joint를 사용하지 않는다.
 * - 액터를 월드에서 45도 회전해도 모든 제한과 힘은
 *   BeginPlay에 저장한 액터 로컬 기준틀에서 계산한다.
 * - 발판 중심은 정해진 로컬 사각 범위 안에서만 움직인다.
 * - 상대 기울기는 MaxRelativeTiltDegrees를 넘지 않는다.
 * - 한쪽이 거의 수직으로 서서 반대쪽이 작동하지 않는 상태를 막는다.
 * - 실제 월드 중력 대신 로컬 기준틀의 양끝 목표 높이 제어를 사용한다.
 *   공은 별도 물리 오브젝트이므로 월드 중력을 그대로 받는다.
 */
UCLASS()
class STEAMDEVELOPMENT_API ATwoDRopeLiftBar : public AActor
{
	GENERATED_BODY()

public:
	ATwoDRopeLiftBar();

	virtual void Tick(float DeltaSeconds) override;

	virtual void OnConstruction(
		const FTransform& Transform
	) override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "TwoD|Pulley Lift"
	)
	bool TryGrabNearestRope(ACharacter* Character);

	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "TwoD|Pulley Lift"
	)
	void ReleaseRope(ACharacter* Character);

	UFUNCTION(
		BlueprintPure,
		Category = "TwoD|Pulley Lift"
	)
	ETwoDRopeSide GetRopeSideForCharacter(
		const ACharacter* Character
	) const;

	UFUNCTION(
		BlueprintPure,
		Category = "TwoD|Pulley Lift|Push Ball Travel Gate"
	)
	bool IsReadyForPushBallTravel() const;

	float GetClosestAvailableHandleDistanceSquared(
		const FVector& WorldLocation
	) const;

	FVector ConstrainCharacterMoveInput(
		const ACharacter* Character,
		const FVector& DesiredWorldInput
	) const;

	void EnforceCharacterRopeLimit(
		ACharacter* Character
	) const;

protected:
	virtual void BeginPlay() override;

	/* -------------------- Moving platform -------------------- */

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UBoxComponent> BarBody;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UStaticMeshComponent> BarVisual;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USceneComponent> LeftBarEnd;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USceneComponent> RightBarEnd;

	/* -------------------- Fixed overhead pulley frame -------------------- */

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USceneComponent> LeftPulleyAnchor;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USceneComponent> RightPulleyAnchor;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USphereComponent> LeftPulleyBody;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USphereComponent> RightPulleyBody;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UStaticMeshComponent> PulleyRodVisual;

	/* -------------------- Initial handle points -------------------- */

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USceneComponent> LeftInitialHandlePoint;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USceneComponent> RightInitialHandlePoint;

	/* -------------------- Physical handles -------------------- */

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USphereComponent> LeftHandleBody;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<USphereComponent> RightHandleBody;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UStaticMeshComponent> LeftHandleVisual;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UStaticMeshComponent> RightHandleVisual;

	/* -------------------- Character grip -------------------- */

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UPhysicsHandleComponent> LeftGrabPhysicsHandle;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UPhysicsHandleComponent> RightGrabPhysicsHandle;

	/* -------------------- Visual rope segments -------------------- */

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UCableComponent> LeftCable;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UCableComponent> RightCable;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UCableComponent> LeftLiftCable;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Components"
	)
	TObjectPtr<UCableComponent> RightLiftCable;

	/* -------------------- Platform geometry -------------------- */

	/**
	 * 왼쪽/오른쪽 줄 사이 간격과 막대기 X축 길이에
	 * 마지막으로 적용되는 가로 배율.
	 *
	 * 기본값 1.2:
	 * - 양쪽 도르래/줄 간격 1.2배
	 * - 막대기 전체 길이 1.2배
	 * - BarBody 충돌 길이 1.2배
	 * - BarVisual 길이 1.2배
	 * - 위쪽 도르래 연결봉 길이도 함께 증가
	 *
	 * 폭(Y), 두께(Z), 높이, 당김 거리, 네트워크 로직은 변경하지 않는다.
	 *
	 * 기존 BP에 BarHalfLength=440이 저장돼 있어도
	 * 이 신규 배율이 마지막에 적용되므로 자동으로 반영된다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Geometry",
		meta = (ClampMin = "1.0", ClampMax = "3.0")
	)
	float HorizontalSpanScale = 1.2f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Geometry",
		meta = (ClampMin = "20.0")
	)
	float BarHalfLength = 440.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Geometry",
		meta = (ClampMin = "1.0")
	)
	float BarHalfWidth = 52.5f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Geometry",
		meta = (ClampMin = "1.0")
	)
	float BarHalfThickness = 10.0f;

	/* -------------------- Pulley placement -------------------- */

	/**
	 * 기구의 세로 높이와 세로 로프 이동범위에 적용되는 전체 배율.
	 *
	 * 기본값 2.5:
	 * - 위 도르래 높이 3배
	 * - 막대기 최대 상승 높이 3배
	 * - 손잡이 세로 로프 길이 3배
	 * - 최대 높이까지 필요한 당김 거리 3배
	 *
	 * 막대기 길이/폭/두께와 최대 기울기 각도는 변경하지 않는다.
	 *
	 * 기존 BP에 이전 값이 저장되어 있어도 이 신규 배율이
	 * 마지막 계산 단계에서 적용되므로 그대로 2.5배가 된다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Height Scale",
		meta = (ClampMin = "1.0", ClampMax = "10.0")
	)
	float VerticalHeightScale = 2.5f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley",
		meta = (ClampMin = "100.0")
	)
	float PulleyHeight = 700.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley",
		meta = (ClampMin = "0.0")
	)
	float PulleyAnchorInset = 0.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley"
	)
	float PulleyDepthOffset = 0.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley",
		meta = (ClampMin = "50.0")
	)
	float InitialHandleDrop = 450.0f;

	/**
	 * 위쪽 도르레에서 손잡이까지의 실제 초기 하강거리 배율.
	 *
	 * 기본 450cm * 1.5 = 675cm.
	 * 기존 BP에 InitialHandleDrop=450이 저장돼 있어도 자동으로 1.5배 적용된다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley",
		meta = (ClampMin = "1.0", ClampMax = "3.0")
	)
	float HandleRopeLengthScale = 1.5f;

	/**
	 * 손잡이 줄을 기존 길이보다 추가로 아래쪽으로 연장한다.
	 *
	 * 기본:
	 * 450 * 1.5 + 120 = 795cm
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "1000.0")
	)
	float FreeHandleExtraDrop = 120.0f;

	/**
	 * 시작할 때 손잡이를 도르래 바로 아래가 아니라
	 * 액터 로컬 앞쪽으로 살짝 빼는 거리다.
	 *
	 * 현재 기본값은 로컬 -Y 방향이다.
	 * 프로젝트에서 앞쪽이 반대라면 BP에서 +120으로 바꾸면 된다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "-1000.0", ClampMax = "1000.0")
	)
	float FreeHandleForwardOffset = -120.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley",
		meta = (ClampMin = "0.0")
	)
	float FixedRopeLengthOverride = 0.0f;

	/**
	 * 캐릭터 절대 최대거리 계산에 추가하는 여유 길이.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley",
		meta = (ClampMin = "0.0")
	)
	float RopeLengthExtraSlack = 100.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley",
		meta = (ClampMin = "5.0")
	)
	float MinimumRopeSegmentLength = 50.0f;

	/* -------------------- Local guided ball-course rules -------------------- */

	/**
	 * 이전 버전 호환용 값.
	 *
	 * v11은 아래의 TensionSlackDistance와
	 * PullTravelToMaximumLift를 사용해 0~1 비율로 계산한다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "0.1", ClampMax = "3.0")
	)
	float LiftDistancePerPullDistance = 1.0f;

	/**
	 * 손잡이를 잡은 뒤 이 거리까지는 줄의 느슨함을 없애는 구간이다.
	 *
	 * 이 구간에서는 막대기가 올라가지 않고,
	 * Cable의 시각적 여유만 점점 사라진다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pull Mapping",
		meta = (ClampMin = "0.0")
	)
	float TensionSlackDistance = 0.0f;

	/**
	 * 줄이 팽팽해진 뒤 막대기 한쪽이 최대 높이에 도달할 때까지
	 * 캐릭터가 추가로 이동해야 하는 거리.
	 *
	 * 기본:
	 * 40cm 이동 -> 줄 팽팽
	 * 추가 300cm 이동 -> 해당 끝 최대 높이
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pull Mapping",
		meta = (ClampMin = "50.0")
	)
	float PullTravelToMaximumLift = 260.0f;

	/**
	 * 당김 반응 곡선.
	 * 1.0은 선형, 0.7은 초반부터 더 강하게 반응한다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pull Mapping",
		meta = (ClampMin = "0.25", ClampMax = "3.0")
	)
	float PullResponseExponent = 1.0f;

	/**
	 * 한쪽 끝이 최초 위치에서 올라갈 수 있는 최대 높이.
	 *
	 * 500cm와 기본 막대기 전체 길이 880cm 조합은
	 * 한쪽만 최대로 당겨도 약 30도 수준이라 수직으로 서지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "50.0")
	)
	float MaxEndpointLiftDistance = 500.0f;

	/**
	 * 최대 당김 상태에서 막대기 끝이 조금 더 올라가게 하는
	 * 최종 상승 보너스 배율.
	 *
	 * 기본값 1.20:
	 * 현재 2.5배 높이 기준 최대 상승 1250cm -> 1500cm.
	 *
	 * 기존 BP에 MaxEndpointLiftDistance=500이 저장돼 있어도
	 * 이 신규 배율이 마지막에 적용되므로 자동으로 반영된다.
	 *
	 * 막대기 동작, 당김 거리, 네트워크 보간, 줄 물리는 변경하지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "1.0", ClampMax = "2.0")
	)
	float EndpointLiftBonusScale = 1.20f;

	/**
	 * 발판이 로컬 기준틀에서 허용되는 최대 상대 Pitch.
	 * 액터 전체를 월드에서 45도 회전하는 것과는 별개다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "5.0", ClampMax = "60.0")
	)
	float MaxRelativeTiltDegrees = 32.0f;

	/**
	 * 발판 중심이 최초 위치에서 로컬 X 방향으로 움직일 수 있는 범위.
	 * 너무 왼쪽/오른쪽으로 빠지는 현상을 막는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "0.0")
	)
	float MaxCenterTravelLocalX = 100.0f;

	/**
	 * 발판 중심을 최초 로컬 X 위치로 부드럽게 되돌리는 가속도.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "0.0")
	)
	float CenteringAccelerationPerCentimeter = 25.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "0.0")
	)
	float CenteringVelocityDamping = 10.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "100.0")
	)
	float MaximumCenteringAcceleration = 4000.0f;

	/**
	 * 하드 범위 보정 전 허용하는 작은 수치 오차.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Local Guide",
		meta = (ClampMin = "0.0", ClampMax = "10.0")
	)
	float LocalBoundTolerance = 0.5f;

	/* -------------------- Deterministic rigid bar response -------------------- */

	/**
	 * 발판 중심 높이 오차 1cm당 목표 로컬 Z 속도.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Rigid Response",
		meta = (ClampMin = "0.1")
	)
	float BarVerticalTargetSpeedPerCentimeter = 7.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Rigid Response",
		meta = (ClampMin = "10.0")
	)
	float MaximumBarVerticalSpeed = 1200.0f;

	/**
	 * 현재 Z 속도를 목표 Z 속도로 따라가게 하는 반응속도.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Rigid Response",
		meta = (ClampMin = "0.1")
	)
	float BarVerticalVelocityResponse = 12.0f;

	/**
	 * 상대 Pitch 오차 1도당 목표 각속도.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Rigid Response",
		meta = (ClampMin = "0.1")
	)
	float BarTiltTargetSpeedPerDegree = 10.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Rigid Response",
		meta = (ClampMin = "10.0")
	)
	float MaximumBarTiltSpeedDegrees = 360.0f;

	/**
	 * 현재 Pitch 각속도를 목표 각속도로 따라가게 하는 반응속도.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Rigid Response",
		meta = (ClampMin = "0.1")
	)
	float BarTiltVelocityResponse = 14.0f;

	/**
	 * v12는 막대기를 물리 힘으로 밀지 않고 목표 Transform으로 직접 이동한다.
	 * 이 값이 클수록 당기는 순간 더 빠르게 올라간다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Direct Transform",
		meta = (ClampMin = "0.1")
	)
	float DirectLocationInterpSpeed = 9.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Direct Transform",
		meta = (ClampMin = "0.1")
	)
	float DirectRotationInterpSpeed = 12.0f;

	/**
	 * 한쪽 줄을 더 멀리 당길 때 해당 끝이 올라가는 반응속도.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Simple Curtain Pull",
		meta = (ClampMin = "0.1")
	)
	float LiftUpInterpSpeed = 9.0f;

	/**
	 * 캐릭터가 도르래 쪽으로 가까워질 때 해당 끝이 내려오는 속도.
	 * 올리는 속도보다 조금 느리게 두면 힘이 풀려 내려오는 느낌이 난다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Simple Curtain Pull",
		meta = (ClampMin = "0.1")
	)
	float LiftDownInterpSpeed = 4.5f;

	/* -------------------- Guaranteed pulley controller -------------------- */

	/**
	 * 현재 막대기 줄 길이와 목표 줄 길이의 오차 1cm당
	 * 도르레 방향으로 적용할 가속도.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "0.0")
	)
	float LiftSpringAccelerationPerCentimeter = 80.0f;

	/** 도르레 방향 끝점 속도 감쇠. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "0.0")
	)
	float LiftVelocityDamping = 12.0f;

	/**
	 * 이전 버전 호환용 값.
	 * v10 로컬 가이드 제어에서는 실제 월드 중력을 끄므로 사용하지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "0.0")
	)
	float LiftSupportAcceleration = 0.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "100.0")
	)
	float MaximumLiftAcceleration = 12000.0f;

	/**
	 * AddForce만 사용하지 않고 목표 속도 차이만큼
	 * 추가 Impulse를 넣어 바닥 접촉 상태에서도 확실하게 출발시킨다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "0.0")
	)
	float LiftTargetSpeedPerCentimeter = 3.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "10.0")
	)
	float MaximumLiftTargetSpeed = 600.0f;

	/**
	 * 목표 속도 오차를 한 Tick에 얼마나 보정할지.
	 * 0.35면 속도 오차의 35%만큼 Impulse를 추가한다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "0.0", ClampMax = "1.0")
	)
	float LiftVelocityImpulseResponse = 0.35f;

	/**
	 * 한쪽 끝에 사용할 유효 질량 비율.
	 * 양쪽 기본값 0.5 + 0.5 = 전체 질량 1.0.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "0.05", ClampMax = "1.0")
	)
	float EndpointEffectiveMassScale = 0.5f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "0.0", ClampMax = "10.0")
	)
	float LiftForceDeadZone = 0.25f;

	/**
	 * 캐릭터가 잡았던 시작 위치로 돌아왔을 때 남을 수 있는
	 * 작은 위치 오차를 무시하는 거리.
	 *
	 * PullAmount가 이 값 이하이면 0으로 처리하므로
	 * 점프하거나 높은 물체를 밟지 않아도 막대기가 처음 위치까지 내려간다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Guaranteed Pull",
		meta = (ClampMin = "0.0", ClampMax = "100.0")
	)
	float PullResetSlack = 0.0f;

	/* -------------------- Exact downward return controller -------------------- */

	/**
	 * 발판이 목표 초기 위치보다 위에 떠 있을 때
	 * 도르레 반대 방향으로 내려보내는 가속도.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Exact Return",
		meta = (ClampMin = "0.0")
	)
	float ReturnAccelerationPerCentimeter = 45.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Exact Return",
		meta = (ClampMin = "0.0")
	)
	float ReturnVelocityDamping = 12.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Exact Return",
		meta = (ClampMin = "100.0")
	)
	float MaximumReturnAcceleration = 5000.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Exact Return",
		meta = (ClampMin = "0.0")
	)
	float ReturnTargetSpeedPerCentimeter = 2.5f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Exact Return",
		meta = (ClampMin = "10.0")
	)
	float MaximumReturnTargetSpeed = 500.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Exact Return",
		meta = (ClampMin = "0.0", ClampMax = "1.0")
	)
	float ReturnVelocityImpulseResponse = 0.30f;

	/* -------------------- Loose handle return controller -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Handle Return",
		meta = (ClampMin = "0.0")
	)
	float HandleReturnAccelerationPerCentimeter = 25.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Handle Return",
		meta = (ClampMin = "0.0")
	)
	float HandleReturnVelocityDamping = 10.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Handle Return",
		meta = (ClampMin = "100.0")
	)
	float MaximumHandleReturnAcceleration = 4000.0f;

	/* -------------------- Decorative overhead rod -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley Rod",
		meta = (ClampMin = "0.0")
	)
	float PulleyRodExtraHalfLength = 40.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley Rod",
		meta = (ClampMin = "1.0")
	)
	float PulleyRodHalfWidth = 15.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Pulley Rod",
		meta = (ClampMin = "1.0")
	)
	float PulleyRodHalfThickness = 15.0f;

	/* -------------------- Handle -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Handle",
		meta = (ClampMin = "2.0")
	)
	float HandleCollisionRadius = 18.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Handle",
		meta = (ClampMin = "0.1")
	)
	float HandleMassKg = 2.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Handle",
		meta = (ClampMin = "0.0")
	)
	float HandleLinearDamping = 1.5f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Handle",
		meta = (ClampMin = "0.0")
	)
	float HandleAngularDamping = 2.0f;

	/**
	 * 손잡이를 놓은 뒤 자유 낙하/진자 운동에 사용하는 감쇠값.
	 * 기존 고정 위치 복귀 힘은 더 이상 사용하지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "20.0")
	)
	float FreeHandleLinearDamping = 0.35f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "20.0")
	)
	float FreeHandleAngularDamping = 0.8f;

	/**
	 * 손을 놓은 손잡이가 벽/낭떠러지 쪽으로 완전히 넘어가지 않도록
	 * BP에서 설정한 FreeHandleForwardOffset 방향으로 부드럽게 유도한다.
	 *
	 * 강제로 위치를 고정하거나 순간이동시키지 않고 수평 가속도만 적용하므로
	 * 중력, 충돌, 진자 운동은 그대로 유지된다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle"
	)
	bool bUseFreeHandleRestBias = true;

	/**
	 * 안전한 앞쪽 목표 위치와의 수평 오차 1cm당 적용하는 가속도.
	 * 값이 클수록 손을 놓은 뒤 앞쪽 위치로 더 확실하게 돌아온다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "50.0")
	)
	float FreeHandleRestAccelerationPerCentimeter = 4.0f;

	/**
	 * 앞쪽 목표 위치로 돌아올 때의 수평 속도 감쇠.
	 * 값이 클수록 목표 근처에서 덜 출렁인다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "50.0")
	)
	float FreeHandleRestVelocityDamping = 4.0f;

	/** 앞쪽 복귀 보정에 사용할 최대 가속도. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "5000.0")
	)
	float MaximumFreeHandleRestAcceleration = 1200.0f;

	/** 목표 근처에서 미세하게 떨지 않도록 무시하는 수평 거리. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "100.0")
	)
	float FreeHandleRestDeadZone = 5.0f;

	/**
	 * 최대 줄 길이에 도달하기 직전부터 바깥 방향 속도를 줄이는 구간.
	 * 딱딱하게 순간 정지하는 느낌을 줄인다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "200.0")
	)
	float FreeHandleTensionZone = 25.0f;

	/**
	 * 최대 줄 길이를 넘어간 위치를 보정할 때 허용하는 작은 오차.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Free Handle",
		meta = (ClampMin = "0.0", ClampMax = "20.0")
	)
	float FreeHandleRopeTolerance = 1.0f;

	/* -------------------- Physics Handle grip -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Grip Physics",
		meta = (ClampMin = "10.0")
	)
	float GripLinearStiffness = 2500.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Grip Physics",
		meta = (ClampMin = "0.0")
	)
	float GripLinearDamping = 300.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Grip Physics",
		meta = (ClampMin = "0.1")
	)
	float GripInterpolationSpeed = 10.0f;

	/* -------------------- Platform physics -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Physics",
		meta = (ClampMin = "1.0")
	)
	float BarMassKg = 60.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Physics",
		meta = (ClampMin = "0.0")
	)
	float BarLinearDamping = 1.5f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Physics",
		meta = (ClampMin = "0.0")
	)
	float BarAngularDamping = 3.0f;

	/* -------------------- Interaction -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Interaction",
		meta = (ClampMin = "10.0")
	)
	float GrabDistance = 150.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Interaction"
	)
	FName RopeGrabSocketName = TEXT("RopeGrabSocket");

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Interaction"
	)
	FVector FallbackGrabOffset =
		FVector(30.0f, 0.0f, 100.0f);

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Interaction",
		meta = (ClampMin = "0.0", ClampMax = "10.0")
	)
	float CharacterLimitTolerance = 1.0f;

	/* -------------------- Client network smoothing -------------------- */

	/**
	 * 클라이언트에서 서버 막대기 스냅샷을 따라가는 위치 보간 속도.
	 * 기본 Actor ReplicateMovement의 순간 이동을 사용하지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Network Smoothing",
		meta = (ClampMin = "1.0", ClampMax = "100.0")
	)
	float ClientBarLocationSmoothingSpeed = 22.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Network Smoothing",
		meta = (ClampMin = "1.0", ClampMax = "100.0")
	)
	float ClientBarRotationSmoothingSpeed = 26.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Network Smoothing",
		meta = (ClampMin = "1.0", ClampMax = "100.0")
	)
	float ClientHandleSmoothingSpeed = 24.0f;

	/** 큰 오차만 즉시 스냅하고 일반적인 네트워크 오차는 보간한다. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Network Smoothing",
		meta = (ClampMin = "50.0", ClampMax = "5000.0")
	)
	float ClientNetworkSnapDistance = 500.0f;

	/**
	 * 최대거리 직전부터 바깥 입력을 서서히 줄이는 구간.
	 * 기존처럼 경계에서 입력을 100% 갑자기 끊지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Network Smoothing",
		meta = (ClampMin = "1.0", ClampMax = "200.0")
	)
	float RopeInputSoftZone = 35.0f;

	/** 로컬 클라이언트의 바깥 방향 속도를 부드럽게 줄이는 구간. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Network Smoothing",
		meta = (ClampMin = "1.0", ClampMax = "200.0")
	)
	float ClientVelocityBrakeZone = 30.0f;

	/** 서버가 실제 위치를 되돌리기 전에 허용하는 작은 초과 거리. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Network Smoothing",
		meta = (ClampMin = "0.0", ClampMax = "50.0")
	)
	float ServerHardLimitTolerance = 5.0f;

	/* -------------------- Cable -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable"
	)
	float CableWidth = 4.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable",
		meta = (ClampMin = "2", ClampMax = "80")
	)
	int32 CableSegments = 24;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable",
		meta = (ClampMin = "1", ClampMax = "32")
	)
	int32 CableSolverIterations = 12;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable",
		meta = (ClampMin = "0.005", ClampMax = "0.05")
	)
	float CableSubstepTime = 0.01f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable",
		meta = (ClampMin = "0.0", ClampMax = "1.0")
	)
	float CableCollisionFriction = 0.4f;

	/**
	 * Construction Script와 에디터 미리보기에서 사용하는 기본 배율.
	 * 플레이 중에는 아래 Visual Tension 설정이 우선한다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable",
		meta = (ClampMin = "1.0", ClampMax = "1.2")
	)
	float CableLengthSlackScale = 1.02f;

	/* -------------------- Cable visual tension -------------------- */

	/**
	 * 팽팽한 상태의 Cable 길이 배율.
	 * 1.0이면 시작점과 끝점의 직선거리와 정확히 같다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Visual Tension",
		meta = (ClampMin = "0.99", ClampMax = "1.02")
	)
	float TautCableLengthScale = 1.0f;

	/**
	 * 막대기가 내려오는 동안 손잡이 쪽 줄에 추가되는 최대 여유 길이.
	 * 실제 로프 길이와 게임 동작에는 영향을 주지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Visual Tension",
		meta = (ClampMin = "0.0", ClampMax = "200.0")
	)
	float ReleasingCableExtraSlack = 35.0f;

	/**
	 * 현재 높이와 목표 높이의 차이가 이 값일 때
	 * 시각적 Slack이 최대치가 된다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Visual Tension",
		meta = (ClampMin = "1.0", ClampMax = "500.0")
	)
	float ReleasingSlackFullRange = 100.0f;

	/**
	 * 팽팽한 상태의 Cable 중력 배율.
	 * 0이면 아래로 처지지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Visual Tension",
		meta = (ClampMin = "0.0", ClampMax = "2.0")
	)
	float TautCableGravityScale = 0.0f;

	/**
	 * 막대기가 내려오는 동안의 Cable 중력 배율.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Visual Tension",
		meta = (ClampMin = "0.0", ClampMax = "3.0")
	)
	float ReleasingCableGravityScale = 1.25f;

	/**
	 * 목표 높이와 현재 높이 차이가 이 값 이하이면
	 * 하강이 끝난 것으로 보고 다시 팽팽하게 만든다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Visual Tension",
		meta = (ClampMin = "0.0", ClampMax = "20.0")
	)
	float CableTensionSettleTolerance = 2.0f;

	/**
	 * 팽팽한 상태에서 사용하는 Solver 반복 횟수.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Visual Tension",
		meta = (ClampMin = "1", ClampMax = "64")
	)
	int32 TautCableSolverIterations = 24;

	/**
	 * 팽팽할 때 줄 두께를 약간 키워 힘이 전달되는 느낌을 강화한다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Visual Tension",
		meta = (ClampMin = "1.0", ClampMax = "2.0")
	)
	float TautCableWidthScale = 1.1f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable"
	)
	bool bEnableCableCollision = true;

	/**
	 * 파쿠르/Mantle Trace가 CableComponent와 HandleBody를
	 * 감지하지 않도록 강제로 Ignore 처리할 커스텀 Trace Channel.
	 *
	 * 현재 프로젝트의 Mantle이 첫 번째 커스텀 Trace Channel이라
	 * 기본값은 ECC_GameTraceChannel1이다.
	 *
	 * BP Class Defaults에서는 프로젝트의 표시 이름인
	 * "Mantle"로 보이는 채널을 선택하면 된다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Collision"
	)
	TEnumAsByte<ECollisionChannel> MantleTraceChannel =
		ECC_GameTraceChannel1;

	/**
	 * true이면 다음 컴포넌트가 모두 Mantle Trace를 Ignore한다.
	 *
	 * - LeftCable
	 * - RightCable
	 * - LeftLiftCable
	 * - RightLiftCable
	 * - LeftHandleBody
	 * - RightHandleBody
	 *
	 * ConfigureCable()과 ConfigureHandleBody()가
	 * Construction/BeginPlay마다 다시 적용하므로,
	 * 레벨에 배치된 인스턴스도 Block으로 되돌아가지 않는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Cable|Collision"
	)
	bool bIgnoreMantleTrace = true;

	/* -------------------- Push Ball travel gate -------------------- */

	/**
	 * ResetVolume이 이 발판의 준비 상태를 기다릴 때 사용하는
	 * 양끝 상승량 허용 오차다.
	 *
	 * CurrentLeft/RightLiftHeight가 이 값 이하이고, 실제 막대기 Transform도
	 * 최초 배치 상태로 복귀했을 때만 준비 완료로 판정한다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Push Ball Travel Gate",
		meta = (ClampMin = "0.0", UIMin = "0.0")
	)
	float BallTravelReadyLiftHeightTolerance = 2.0f;

	/**
	 * 캐릭터가 줄을 막 당기기 시작한 바로 그 프레임에
	 * 막대기가 아직 아래에 있다는 이유만으로 준비 완료가 되는 것을 막는다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Push Ball Travel Gate",
		meta = (ClampMin = "0.0", ClampMax = "1.0")
	)
	float BallTravelReadyPullAlphaTolerance = 0.01f;

	/** 최초 배치된 막대기 중심 위치와 비교할 월드/로컬 위치 허용 오차(cm). */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Push Ball Travel Gate",
		meta = (ClampMin = "0.0", UIMin = "0.0")
	)
	float BallTravelReadyPositionTolerance = 5.0f;

	/** 최초 배치된 막대기 회전과 비교할 각도 허용 오차(도). */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|Push Ball Travel Gate",
		meta = (ClampMin = "0.0", ClampMax = "45.0")
	)
	float BallTravelReadyRotationToleranceDegrees = 1.5f;

	/* -------------------- Replication -------------------- */

	UPROPERTY(
		ReplicatedUsing = OnRep_LeftHolder,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|State"
	)
	TObjectPtr<ACharacter> LeftHolder;

	UPROPERTY(
		ReplicatedUsing = OnRep_RightHolder,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|State"
	)
	TObjectPtr<ACharacter> RightHolder;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|State"
	)
	float LeftRopeLength = 0.0f;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "TwoD|Pulley Lift|State"
	)
	float RightRopeLength = 0.0f;

	/**
	 * 클라이언트에서도 서버와 동일한 줄 처짐 연출을 보이게 하는
	 * 시각 전용 복제값이다. 게임 동작에는 사용하지 않는다.
	 */
	UPROPERTY(Replicated)
	float RepLeftCableReleaseAlpha = 0.0f;

	UPROPERTY(Replicated)
	float RepRightCableReleaseAlpha = 0.0f;

	/*
	 * Generic Actor movement replication은 클라이언트에서 스냅되므로 사용하지 않는다.
	 * 서버 Transform을 아래 값으로 복제하고 클라이언트 Tick에서 부드럽게 따라간다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_NetworkVisualState)
	FVector_NetQuantize10 RepBarWorldLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkVisualState)
	FRotator RepBarWorldRotation = FRotator::ZeroRotator;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 RepLeftHandleWorldLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FRotator RepLeftHandleWorldRotation = FRotator::ZeroRotator;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 RepRightHandleWorldLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FRotator RepRightHandleWorldRotation = FRotator::ZeroRotator;

	UFUNCTION()
	void OnRep_LeftHolder();

	UFUNCTION()
	void OnRep_RightHolder();

	UFUNCTION()
	void OnRep_NetworkVisualState();

private:
	void ConfigureBarPhysics();

	void ConfigureHandleBody(
		USphereComponent* HandleBody
	) const;

	void ConfigurePulleyBody(
		USphereComponent* PulleyBody
	) const;

	void ConfigurePhysicsHandle(
		UPhysicsHandleComponent* PhysicsHandle
	) const;

	void ConfigureCable(
		UCableComponent* Cable
	) const;

	void FreezePulleyFrameInWorld();

	/**
	 * Blueprint에 저장된 Built-in DOF 잠금값을 Joint 생성 없이 해제한다.
	 */
	void ClearBuiltInDOFConstraintSettings();

	/**
	 * BeginPlay 순간의 BarBody Transform을 고정된 로컬 기준틀로 저장한다.
	 * 액터를 월드에서 45도 회전해도 이 기준틀을 따라 동작한다.
	 */
	void CaptureMechanismFrame();

	/**
	 * 발판 중심 위치와 상대 회전을 로컬 사각 범위 안으로 제한한다.
	 */
	void EnforceLocalMechanismBounds();

	/**
	 * 발판 중심이 로컬 X 방향으로 멀리 밀리지 않도록 복귀 Impulse를 준다.
	 */
	void ApplyLocalCenteringController(
		float DeltaSeconds
	);

	/**
	 * HorizontalSpanScale이 적용된 실제 막대기 반길이.
	 */
	float GetEffectiveBarHalfLength() const;

	/**
	 * PulleyAnchorInset까지 포함한 기존 줄 반간격에
	 * HorizontalSpanScale을 적용한 실제 줄 반간격.
	 */
	float GetEffectivePulleyAnchorHalfSpacing() const;

	float GetEffectivePulleyHeight() const;
	float GetEffectiveMaxEndpointLiftDistance() const;
	float GetEffectivePullTravelToMaximumLift() const;
	float GetEffectiveTensionSlackDistance() const;
	float GetEffectiveRopeLengthExtraSlack() const;

	float GetEffectiveInitialHandleDrop() const;

	/**
	 * 도르래 기준 손잡이의 시작 로컬 오프셋.
	 */
	FVector GetFreeHandleInitialLocalOffset() const;

	/**
	 * 놓인 손잡이가 도르래에서 벗어날 수 있는 실제 최대 줄 길이.
	 */
	float GetFreeHandleRopeLength() const;

	void DetachHandleAndEnablePhysics(
		USphereComponent* HandleBody
	);

	void UpdatePulleyController(
		float DeltaSeconds
	);

	/**
	 * 도르래에서 손까지의 거리를 기구 로컬 평면(XY)에서 계산한다.
	 * 로컬 Z는 무시하므로 점프 높이가 당김량에 섞이지 않는다.
	 */
	float CalculatePlanarDistanceFromPulley(
		ETwoDRopeSide Side,
		const FVector& HandWorldLocation
	) const;

	/**
	 * 잡은 순간보다 도르래에서 얼마나 더 멀어졌는지 계산한다.
	 * 가까워지면 즉시 0으로 돌아간다.
	 */
	float CalculateRawProjectedPullDistance(
		ETwoDRopeSide Side
	) const;

	float CalculatePullAlpha(
		ETwoDRopeSide Side
	) const;

	float GetMaximumProjectedPullDistance() const;

	FVector GetPlanarOutwardWorldDirection(
		ETwoDRopeSide Side,
		const FVector& HandWorldLocation
	) const;

	/**
	 * 높은 쪽 목표는 그대로 유지하면서 낮은 쪽을 올려
	 * 최대 기울기 범위 안으로 제한한다.
	 */
	void ClampDesiredEndpointHeights(
		float& InOutLeftHeight,
		float& InOutRightHeight
	) const;

	/**
	 * 양끝 목표 높이를 하나의 강체 중심 높이 + Pitch 목표로 변환한다.
	 *
	 * 독립된 양끝 힘이 서로 싸우지 않기 때문에
	 * 막대기의 현재 위치와 기울기에 상관없이 일정하게 반응한다.
	 */
	void ApplyRigidBarTargetController(
		float LeftDesiredHeight,
		float RightDesiredHeight,
		float DeltaSeconds
	);

	void UpdateLooseHandles(
		float DeltaSeconds
	);

	void UpdateLooseHandle(
		ETwoDRopeSide Side,
		float DeltaSeconds
	);

	bool StartHoldingSide(
		ACharacter* Character,
		ETwoDRopeSide Side
	);

	void StopHoldingSide(
		ETwoDRopeSide Side
	);

	void UpdateHeldHandleTargets();

	void UpdateHeldHandleTarget(
		ETwoDRopeSide Side
	);

	/** 서버가 현재 막대기/손잡이 Transform을 복제 스냅샷에 기록한다. */
	void UpdateServerNetworkVisualState(
		bool bForceUpdate
	);

	/** 클라이언트가 복제 스냅샷을 매 프레임 보간한다. */
	void UpdateClientNetworkSmoothing(
		float DeltaSeconds
	);

	void SmoothClientHandleForSide(
		ETwoDRopeSide Side,
		float DeltaSeconds
	);

	FVector GetCharacterGrabLocation(
		const ACharacter* Character
	) const;

	/**
	 * 줄 길이/최대거리 계산용 안정적인 점.
	 * 애니메이션 손 소켓 흔들림을 사용하지 않고 캐릭터 Actor 중심을 사용한다.
	 */
	FVector GetCharacterRopeControlLocation(
		const ACharacter* Character
	) const;

	bool TryGrabSide(
		ACharacter* Character,
		ETwoDRopeSide Side
	);

	bool IsCharacterCloseEnoughToSide(
		const ACharacter* Character,
		ETwoDRopeSide Side
	) const;

	USphereComponent* GetHandleForSide(
		ETwoDRopeSide Side
	) const;

	USceneComponent* GetBarEndForSide(
		ETwoDRopeSide Side
	) const;

	USceneComponent* GetPulleyAnchorForSide(
		ETwoDRopeSide Side
	) const;

	UPhysicsHandleComponent* GetGrabPhysicsHandleForSide(
		ETwoDRopeSide Side
	) const;

	UCableComponent* GetHandleCableForSide(
		ETwoDRopeSide Side
	) const;

	UCableComponent* GetLiftCableForSide(
		ETwoDRopeSide Side
	) const;

	ACharacter* GetHolderForSide(
		ETwoDRopeSide Side
	) const;

	float GetRopeLengthForSide(
		ETwoDRopeSide Side
	) const;

	float GetAbsoluteMaximumHandleReach(
		ETwoDRopeSide Side
	) const;

	void RefreshCableBindings();

	void RefreshCableBindingForSide(
		ETwoDRopeSide Side
	);

	void RefreshLiftCableBindings();

	bool AttachCableEndToCharacterHand(
		UCableComponent* Cable,
		ACharacter* Holder
	) const;

	void UpdateCableLengths();

	void CleanupInvalidHolders();

	void UpdatePulleyRodPreview();

	void ScaleVisualToHalfExtent(
		UStaticMeshComponent* Visual,
		const FVector& DesiredHalfExtent
	) const;

	/**
	 * BeginPlay 순간 실제 월드 거리.
	 * 발판은 줄을 풀었을 때 이 정확한 길이까지 복귀한다.
	 */
	float LeftInitialLiftLength = 0.0f;
	float RightInitialLiftLength = 0.0f;
	float LeftInitialHandleLength = 0.0f;
	float RightInitialHandleLength = 0.0f;

	/**
	 * 사용자가 배치한 게임 시작 위치 자체가 최대 하강 위치다.
	 */
	float LeftMaximumLoweredLiftLength = 0.0f;
	float RightMaximumLoweredLiftLength = 0.0f;

	/**
	 * 움직이지 않는 로컬 게임 기준틀.
	 * Scale은 항상 1로 저장한다.
	 */
	FTransform MechanismFrameTransform =
		FTransform::Identity;

	FVector InitialBarCenterLocal =
		FVector::ZeroVector;

	float LeftInitialEndpointLocalZ = 0.0f;
	float RightInitialEndpointLocalZ = 0.0f;

	bool bMechanismFrameCaptured = false;

	/**
	 * 손잡이를 잡은 순간의 도르래-손 평면거리.
	 *
	 * 현재 거리 - 시작 거리:
	 * 양수면 당김, 0이면 완전히 풀림.
	 */
	UPROPERTY(Replicated)
	float LeftGrabStartPlanarDistance = 0.0f;

	UPROPERTY(Replicated)
	float RightGrabStartPlanarDistance = 0.0f;

	/**
	 * 실제 화면에 적용 중인 양끝 상승량.
	 * 각 끝을 독립적으로 보간해 커튼처럼 올라가고 내려온다.
	 */
	float CurrentLeftLiftHeight = 0.0f;
	float CurrentRightLiftHeight = 0.0f;

	bool bHasReceivedNetworkVisualState = false;
	bool bPulleyFrameFrozen = false;
};