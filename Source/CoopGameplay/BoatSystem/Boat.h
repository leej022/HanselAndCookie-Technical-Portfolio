#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/NetSerialization.h"
#include "Boat.generated.h"

class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;
class ACharacter;
class APlayerController;

UENUM(BlueprintType)
enum class EBoatSeat : uint8
{
	None,
	Left,
	Right
};

USTRUCT(BlueprintType)
struct FBoatAvatarDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	USkeletalMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimSequenceBase* LeftIdle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimSequenceBase* LeftPaddle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimSequenceBase* LeftReversePaddle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimSequenceBase* RightIdle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimSequenceBase* RightPaddle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimSequenceBase* RightReversePaddle = nullptr;
};

USTRUCT()
struct FBoatNetState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Pos = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rot = FRotator::ZeroRotator;

	UPROPERTY()
	FVector_NetQuantize10 LinVel = FVector::ZeroVector;

	UPROPERTY()
	float YawRateDeg = 0.f;

	UPROPERTY()
	float ServerTime = 0.f;
};

UCLASS()
class STEAMDEVELOPMENT_API ABoat : public AActor
{
	GENERATED_BODY()

public:
	ABoat();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Boat|Paddle")
	void PaddleFromRower(ACharacter* Rower);

	UFUNCTION(BlueprintCallable, Category = "Boat|Paddle")
	void ReversePaddleFromRower(ACharacter* Rower);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Boat")
	EBoatSeat GetSeatForRower(ACharacter* Rower) const;

	bool TrySeatCharacter_Server(ACharacter* Character, EBoatSeat Seat);

	USceneComponent* GetSeatComponent(EBoatSeat Seat) const;

	void TrySetBoatView_Local(ACharacter* Character) const;

	void LockAllRowersInput_Server(float DurationSeconds = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Boat|Seat")
	bool IsSeatOccupied(EBoatSeat Seat) const;

	UFUNCTION(BlueprintPure, Category = "Boat|Seat")
	bool IsSeatOccupiedByOther(EBoatSeat Seat, const ACharacter* Character) const;

	UFUNCTION(BlueprintCallable, Category = "Boat|BoatSavePoint")
	void SaveRespawnPoint_Server(const FTransform& NewRespawnTransform, int32 NewSaveOrder = -1);

	UFUNCTION(BlueprintCallable, Category = "Boat|BoatSavePoint")
	void RespawnToSavedPoint_Server(float InputLockSeconds = 0.6f, bool bZeroVelocity = true);

	// ✅ 죽는 디졸브 연출 시작 후, 끝나면 체크포인트로 리스폰
	UFUNCTION(BlueprintCallable, Category = "Boat|Death")
	void StartDeathRespawnSequence_Server(
		float DeathDuration = 3.0f,
		float PostRespawnInputLockSeconds = 0.6f,
		bool bZeroVelocity = true
	);

	UFUNCTION(BlueprintPure, Category = "Boat|BoatSavePoint")
	bool HasSavedRespawnPoint() const { return bHasSavedRespawnTransform; }

	UFUNCTION(BlueprintPure, Category = "Boat|BoatSavePoint")
	FTransform GetSavedRespawnTransform() const { return SavedRespawnTransform; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
	UStaticMeshComponent* BoatMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
	UBoxComponent* BoardTriggerLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
	UBoxComponent* BoardTriggerRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
	USceneComponent* SeatsRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
	USceneComponent* SeatLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
	USceneComponent* SeatRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|Camera")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|Camera")
	UCameraComponent* BoatCamera;

	// ===== 로컬 자유 시점 카메라 =====
	// 복제하지 않음. 서버와 각 클라이언트가 자기 마우스로 서로 다른 방향을 봄.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera|FreeLook")
	bool bEnableFreeLookCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera|FreeLook",
		meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float FreeLookMinPitch = -65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera|FreeLook",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float FreeLookMaxPitch = 35.0f;

	// 보트 카메라로 시점 전환되는 동안에는 자유 시점 마우스 입력을 받지 않음.
	// 현재 SetViewTargetWithBlend의 1초 전환 시간과 맞춰 둔 기본값.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera|FreeLook",
		meta = (ClampMin = "0.0"))
	float FreeLookInputLockDurationAfterBoard = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|Avatar")
	USkeletalMeshComponent* SeatAvatarLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|Avatar")
	USkeletalMeshComponent* SeatAvatarRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|PaddleProp")
	UStaticMeshComponent* SeatPaddleLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|PaddleProp")
	UStaticMeshComponent* SeatPaddleRight;

	// ===== 바닥에 떨어져 있는 노 오브젝트 =====
	// 왼쪽 좌석에 탑승하면 LeftLoosePaddleActor만 사라짐
	// 오른쪽 좌석에 탑승하면 RightLoosePaddleActor만 사라짐
	// 손에 붙는 SeatPaddleLeft / SeatPaddleRight와는 별개임
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boat|LoosePaddle")
	AActor* LeftLoosePaddleActor = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boat|LoosePaddle")
	AActor* RightLoosePaddleActor = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|PaddleProp")
	UStaticMesh* PaddleMeshAsset = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|PaddleProp")
	FVector PaddleRelativeScale = FVector(10.f, 10.f, 10.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|PaddleProp")
	FName LeftSeatPaddleSocketName = TEXT("PaddleSocket_Left");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|PaddleProp")
	FName RightSeatPaddleSocketName = TEXT("PaddleSocket_Right");

	// ===== 패들링 사운드 =====
	// SoundCue를 여기에 넣으면 됨.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Sound")
	USoundBase* PaddleSoundCue = nullptr;

	// ===== 보트 BGM =====
	// 두 좌석이 모두 찼을 때 각 PC에서 로컬로 한 번 재생한다.
	// SoundCue의 Wave Player에서 Looping을 켜고, Sound Class는 SC_Music으로 지정하면
	// 기존 GI_Steam의 Master/Music 옵션 및 로딩/시네마틱 음소거 구조를 그대로 따른다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|BGM")
	USoundBase* BoatBGMSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|BGM", meta = (ClampMin = "0.0"))
	float BoatBGMVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|BGM", meta = (ClampMin = "0.1"))
	float BoatBGMPitch = 1.0f;

	// 로컬 재생 전용. 네트워크로 AudioComponent 자체를 복제하지 않는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|BGM")
	UAudioComponent* BoatBGMComponent = nullptr;

	// ===== 죽는 사운드 =====
	// 보트가 죽는 디졸브 연출을 시작할 때 한 번 재생할 SoundCue.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Death|Sound")
	USoundBase* DeathSoundCue = nullptr;

	// 위치 기반으로 들리게 할 Attenuation.
	// SoundCue 안에 Attenuation을 이미 넣어놨으면 비워도 됨.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Death|Sound")
	USoundAttenuation* DeathSoundAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Death|Sound", meta = (ClampMin = "0.0"))
	float DeathSoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Death|Sound", meta = (ClampMin = "0.1"))
	float DeathSoundPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Avatar")
	TSubclassOf<ACharacter> HanselCharacterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Avatar")
	TSubclassOf<ACharacter> CookieCharacterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Avatar")
	FBoatAvatarDef HanselAvatar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boat|Avatar")
	FBoatAvatarDef CookieAvatar;

	UPROPERTY(ReplicatedUsing = OnRep_Occupants, BlueprintReadOnly, Category = "Boat")
	ACharacter* LeftOccupant;

	UPROPERTY(ReplicatedUsing = OnRep_Occupants, BlueprintReadOnly, Category = "Boat")
	ACharacter* RightOccupant;

	UPROPERTY(ReplicatedUsing = OnRep_AvatarIds, BlueprintReadOnly, Category = "Boat|Avatar")
	uint8 LeftAvatarId = 0;

	UPROPERTY(ReplicatedUsing = OnRep_AvatarIds, BlueprintReadOnly, Category = "Boat|Avatar")
	uint8 RightAvatarId = 0;

	// ===== 바닥 노 숨김 상태 복제 =====
	UPROPERTY(ReplicatedUsing = OnRep_LoosePaddlesHidden, BlueprintReadOnly, Category = "Boat|LoosePaddle")
	bool bLeftLoosePaddleHidden = false;

	UPROPERTY(ReplicatedUsing = OnRep_LoosePaddlesHidden, BlueprintReadOnly, Category = "Boat|LoosePaddle")
	bool bRightLoosePaddleHidden = false;

	// ===== 죽는 디졸브 설정 =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Death")
	UMaterialInterface* DeathDissolveMaterial = nullptr;

	// 스샷 기준 파라미터 이름 그대로 기본값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Death")
	FName DeathDissolveParameterName = TEXT("Disolve");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Death")
	float DeathDissolveStartValue = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Death")
	float DeathDissolveEndValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Death", meta = (ClampMin = "0.05"))
	float DefaultDeathSequenceDuration = 3.0f;

	// ✅ 필요하면 BP에서 이 두 이벤트만 구현해서 Niagara / 사운드 붙이면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "Boat|Death")
	void BP_OnDeathSequenceStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Boat|Death")
	void BP_OnDeathSequenceFinished();

	UFUNCTION()
	void OnRep_Occupants();

	UFUNCTION()
	void OnRep_AvatarIds();

	UFUNCTION()
	void OnRep_LoosePaddlesHidden();

	UFUNCTION()
	void HandleBeginOverlapLeft(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleBeginOverlapRight(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	void SeatCharacter_Server(ACharacter* Character, EBoatSeat Seat);
	void ApplySeatedState_Local(ACharacter* Character, EBoatSeat Seat) const;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnSeated(ACharacter* Character, EBoatSeat Seat, uint8 AvatarId);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySeatPaddle(EBoatSeat Seat, uint8 AvatarId, bool bReverse);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayPaddleSound(EBoatSeat Seat);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_BoatState(const FBoatNetState& State);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginDeathSequence(float DeathDuration);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EndDeathSequence();

	void PushNetState_Client(const FBoatNetState& State);
	void SmoothBoat_Client(float DeltaSeconds);

	FTimerHandle NetStateTimer;
	void StartNetStateTimer_Server();
	void BroadcastNetState_Server();
	void BroadcastNetState_Server_Immediate();

	UPROPERTY(EditAnywhere, Category = "Boat|NetSmoothing")
	bool bUseCustomNetSmoothing = true;

	UPROPERTY(EditAnywhere, Category = "Boat|NetSmoothing", meta = (ClampMin = "5.0", ClampMax = "60.0"))
	float NetStateSendHz = 25.f;

	UPROPERTY(EditAnywhere, Category = "Boat|NetSmoothing", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float InterpDelay = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Boat|NetSmoothing", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MaxExtrapolate = 0.20f;

	UPROPERTY(EditAnywhere, Category = "Boat|NetSmoothing", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float TimeOffsetSmoothingAlpha = 0.10f;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Paddle")
	float PaddleImpulse = 75000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Paddle")
	float TurnAngularImpulse = 3500000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Paddle")
	float YawOffsetDeg = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Paddle")
	float PaddleCooldown = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Paddle", meta = (ClampMin = "0.0"))
	float ReversePaddleDelay = 0.17f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Safety")
	float MaxSpeed = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Safety")
	float MaxAngularDegPerSec = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Safety")
	bool bOverrideMass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Safety", meta = (EditCondition = "bOverrideMass"))
	float MassKg = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Seat")
	bool bHideCharacterWhenSeated = true;

private:
	float LastPaddleTimeLeft = -1000.f;
	float LastPaddleTimeRight = -1000.f;

	void ApplyPaddle_Server(EBoatSeat Seat, bool bReverse);
	void ClampVelocities_Server() const;

	void TeleportCharacterToSeat_NoAttach(ACharacter* Character, EBoatSeat Seat) const;

	uint8 ResolveAvatarIdForCharacter(ACharacter* Character) const;
	const FBoatAvatarDef* GetAvatarDef(uint8 AvatarId) const;

	USkeletalMeshComponent* GetSeatAvatarComp(EBoatSeat Seat) const;
	UAnimSequenceBase* GetIdleAnim(uint8 AvatarId, EBoatSeat Seat) const;
	UAnimSequenceBase* GetPaddleAnim(uint8 AvatarId, EBoatSeat Seat) const;
	UAnimSequenceBase* GetReversePaddleAnim(uint8 AvatarId, EBoatSeat Seat) const;

	void RequestSeatVisualRefresh();
	FTimerHandle SeatVisualRefreshTimer;

	void RefreshSeatVisuals_Local();
	void RefreshPaddleProps_Local();

	void RefreshLoosePaddles_Local() const;
	void SetLoosePaddleVisible_Local(EBoatSeat Seat, bool bVisible) const;

	void ApplySeatAvatar_Local(EBoatSeat Seat, uint8 AvatarId);
	void PlaySeatIdle_Local(EBoatSeat Seat, uint8 AvatarId);
	void PlaySeatPaddle_Local(EBoatSeat Seat, uint8 AvatarId, bool bReverse);
	void PlayPaddleSound_Local(EBoatSeat Seat) const;
	void PlayDeathSound_Local() const;

	void RestoreSeatIdle(EBoatSeat Seat);

	FTimerHandle RestoreIdleTimerLeft;
	FTimerHandle RestoreIdleTimerRight;

	FTimerHandle PendingReverseTimerLeft;
	FTimerHandle PendingReverseTimerRight;

	void ExecuteReversePaddle_Server(EBoatSeat Seat);
	bool IsReversePending(EBoatSeat Seat) const;

	TArray<FBoatNetState> NetStateBuffer;
	bool bHasTimeOffset = false;
	float TimeOffsetEstimate = 0.f;
	double LastNetStateSendTime = -1e9;

	UPROPERTY()
	FTransform SavedRespawnTransform;

	UPROPERTY()
	bool bHasSavedRespawnTransform = false;

	UPROPERTY()
	int32 SavedRespawnOrder = INDEX_NONE;

	// ===== 죽는 연출 런타임 상태 =====
	bool bDeathSequenceActive = false;
	bool bDeathVisualActive_Local = false;
	float DeathVisualElapsed_Local = 0.0f;
	float DeathVisualDuration_Local = 0.0f;
	FTimerHandle DeathRespawnTimer;

	void FinishDeathRespawnSequence_Server(float PostRespawnInputLockSeconds, bool bZeroVelocity);
	void UpdateDeathVisuals_Local(float DeltaSeconds);
	void PrepareAllDeathMaterials_Local();
	void SetAllDeathMaterialParameters_Local(float Value);
	void RestoreAllDeathMaterials_Local();
	void ResetTickEnabledAfterDeath_Local();

	void PrepareDeathMaterialsForComponent_Local(
		UMeshComponent* MeshComp,
		TArray<TObjectPtr<UMaterialInterface>>& OriginalMaterials,
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& DissolveMIDs
	);

	void RestoreMaterialsForComponent_Local(
		UMeshComponent* MeshComp,
		const TArray<TObjectPtr<UMaterialInterface>>& OriginalMaterials
	);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> BoatMeshOriginalMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SeatAvatarLeftOriginalMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SeatAvatarRightOriginalMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SeatPaddleLeftOriginalMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SeatPaddleRightOriginalMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BoatMeshDissolveMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SeatAvatarLeftDissolveMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SeatAvatarRightDissolveMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SeatPaddleLeftDissolveMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SeatPaddleRightDissolveMIDs;

private:
	// ===== 보트 BGM 로컬 처리 =====
	void UpdateBoatBGM_Local();
	void StopBoatBGM_Local();

	// 이 컴퓨터의 로컬 플레이어가 이 보트에 탑승했는지 여부.
	// 서버/클라이언트가 각자 따로 가지며 절대 복제하지 않는다.
	bool bLocalPlayerBoardedForBGM_Local = false;

	// 현재 컴퓨터의 로컬 PlayerController만 사용해서 카메라를 회전시킴.
	// 네트워크로 복제하지 않기 때문에 호스트/클라이언트 시점이 서로 독립적임.
	void UpdateFreeLookCamera_Local();
	void ResetFreeLookCamera_Local();

	bool bFreeLookInitialized_Local = false;
	TWeakObjectPtr<APlayerController> FreeLookController_Local;
	FRotator FreeLookOriginalSpringArmRelativeRotation_Local = FRotator::ZeroRotator;
	float FreeLookYaw_Local = 0.0f;
	float FreeLookPitch_Local = 0.0f;
	float FreeLookInputUnlockTime_Local = 0.0f;

	bool HasAnyOccupant() const;
	bool HasBothOccupants() const;
	void UpdateBoatOccupancyState_Server();
	void UpdateBoatOccupancyState_Local();
};