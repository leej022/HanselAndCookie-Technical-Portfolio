#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatSavePoint.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class USoundBase;
class ABoat;

UCLASS()
class STEAMDEVELOPMENT_API ABoatSavePoint : public AActor
{
	GENERATED_BODY()

public:
	ABoatSavePoint();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BoatSavePoint")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BoatSavePoint")
	UStaticMeshComponent* VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BoatSavePoint")
	UBoxComponent* TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint")
	int32 SaveOrder = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint")
	FVector SaveOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint")
	bool bUseThisActorRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Visual")
	UMaterialInterface* ActivatedMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Visual", meta = (ClampMin = "0.0"))
	float RiseHeight = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Visual", meta = (ClampMin = "0.0"))
	float FastSpinTurns = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Visual", meta = (ClampMin = "0.01"))
	float FastSpinDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Visual", meta = (ClampMin = "0.01"))
	float ReturnDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Visual")
	float ReturnSpinDegrees = 360.0f;

	// ✅ BP에서 사운드만 끼워 넣으면 됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Audio")
	USoundBase* ActivateSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Audio", meta = (ClampMin = "0.0"))
	float ActivateSoundVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BoatSavePoint|Audio", meta = (ClampMin = "0.0"))
	float ActivateSoundPitch = 1.0f;

	// ✅ BP에서 추가 이펙트/사운드/나이아가라 붙이고 싶으면 이 이벤트 사용
	UFUNCTION(BlueprintImplementableEvent, Category = "BoatSavePoint|FX")
	void BP_OnActivated();

private:
	float LastTriggerTime = -1000.f;

	UFUNCTION()
	void OnTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(ReplicatedUsing = OnRep_Activated)
	bool bActivated = false;

	UFUNCTION()
	void OnRep_Activated();

	void StartActivateVisuals_Local();
	void ApplyActivatedMaterial_Local();
	void PlayActivateSound_Local();

	bool bAnimating = false;
	float AnimElapsed = 0.0f;

	enum class EAnimPhase : uint8
	{
		None,
		FastSpinUp,
		SlowReturn
	};

	EAnimPhase AnimPhase = EAnimPhase::None;

	FVector BaseMeshRelativeLocation = FVector::ZeroVector;
	FRotator BaseMeshRelativeRotation = FRotator::ZeroRotator;
};