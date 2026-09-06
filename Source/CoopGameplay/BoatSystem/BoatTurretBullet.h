#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatTurretBullet.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;
class ABoat;

UCLASS()
class STEAMDEVELOPMENT_API ABoatTurretBullet : public AActor
{
	GENERATED_BODY()

public:
	ABoatTurretBullet();

	/*
	 * 기관총에서 발사 순간 계산한
	 * 실제 월드 발사 방향을 전달한다.
	 */
	void InitializeBoatBullet(
		const FVector& NewWorldDirection
	);

protected:
	virtual void BeginPlay() override;

	// 실제 충돌을 담당하는 Sphere
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Boat|TurretBullet"
	)
	TObjectPtr<USphereComponent> BoatBulletCollision;

	// 화면에 보이는 총알 메시
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Boat|TurretBullet"
	)
	TObjectPtr<UStaticMeshComponent> BoatBulletMesh;

	// 총알 직선 이동
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Boat|TurretBullet"
	)
	TObjectPtr<UProjectileMovementComponent> BoatProjectileMovement;

	// 총알 속도
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Boat|TurretBullet",
		meta = (ClampMin = "1.0")
	)
	float BoatBulletSpeed = 3500.0f;

	// 총알 자동 제거 시간
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Boat|TurretBullet",
		meta = (ClampMin = "0.1")
	)
	float BoatBulletLifeSeconds = 5.0f;

	/*
	 * TeleportBoatTrigger와 동일한 죽음 설정.
	 * 총알이 보트에 닿으면 이 값으로 기존 Boat 죽음 함수를 호출한다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Boat|TurretBullet|Death",
		meta = (ClampMin = "0.05")
	)
	float DeathSequenceDuration = 3.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Boat|TurretBullet|Death",
		meta = (ClampMin = "0.0")
	)
	float InputLockSeconds = 0.6f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Boat|TurretBullet|Death"
	)
	bool bZeroVelAfterRespawn = true;

private:
	// 기관총에서 전달받은 발사 방향
	FVector BoatLaunchDirection = FVector::ForwardVector;

	bool bBoatLaunchDirectionInitialized = false;

	/*
	 * Hit와 Overlap이 동시에 발생하더라도
	 * 한 총알에서 한 번만 처리하기 위한 값.
	 */
	bool bImpactProcessed = false;

	void LaunchBoatBullet();

	/*
	 * OtherActor 또는 OtherComponent의 Owner에서
	 * 실제 ABoat를 찾는다.
	 */
	ABoat* ResolveBoat(
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent
	) const;

	/*
	 * 찾은 Boat에 기존 죽음 함수를 호출하고
	 * 총알을 제거한다.
	 */
	bool TryProcessBoatImpact(
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent
	);

	UFUNCTION()
	void HandleBoatBulletHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

	UFUNCTION()
	void HandleBoatBulletOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
};