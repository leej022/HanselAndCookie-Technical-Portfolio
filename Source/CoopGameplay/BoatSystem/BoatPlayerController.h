#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Boat.h"
#include "LoadingPlayerControllerBase.h"
#include "BoatPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class STEAMDEVELOPMENT_API ABoatPlayerController
	: public ALoadingPlayerControllerBase
{
	GENERATED_BODY()

public:
	ABoatPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	/**
	 * 현재 이 PlayerController가 보트에 탑승한 상태인지 반환한다.
	 *
	 * 로컬 BP의 펀치 입력 차단과
	 * 서버의 실제 Punch Trace 차단 양쪽에서 사용할 수 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "Boat|State")
	bool IsInBoat() const;

	void PredictBoard_Local(
		class ABoat* Boat,
		EBoatSeat Seat
	);

	UFUNCTION(Server, Reliable)
	void Server_RequestBoard(
		class ABoat* Boat,
		EBoatSeat Seat
	);

	void ForceBoardOnServer(
		class ABoat* Boat,
		EBoatSeat Seat
	);

	UFUNCTION(BlueprintCallable, Category = "Boat|Input")
	void LockBoatInput(
		float DurationSeconds = 1.0f
	);

	// =====================================================
	// TEMP SOLO TEST
	//
	// 테스트용.
	// 두 플레이어가 모두 보트에 탑승했을 때만 작동.
	//
	// 기존 LMB / RMB 입력은 전혀 변경하지 않음.
	//
	// BP에서 Q / E / Z / C 등 원하는 임시 키에
	// 아래 함수들을 연결해서 사용하면 됨.
	// =====================================================

	UFUNCTION(
		BlueprintCallable,
		Category = "Boat|Test"
	)
	void TestLeftForwardPaddle();

	UFUNCTION(
		BlueprintCallable,
		Category = "Boat|Test"
	)
	void TestLeftReversePaddle();

	UFUNCTION(
		BlueprintCallable,
		Category = "Boat|Test"
	)
	void TestRightForwardPaddle();

	UFUNCTION(
		BlueprintCallable,
		Category = "Boat|Test"
	)
	void TestRightReversePaddle();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* IMC_Boat = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Paddle = nullptr;

	// 우클릭 후진 입력
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_ReversePaddle = nullptr;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Input|Optional"
	)
	UInputMappingContext* IMC_OnFoot = nullptr;

private:
	UPROPERTY(Replicated)
	ABoat* CurrentBoat = nullptr;

	UPROPERTY(Replicated)
	EBoatSeat MySeat = EBoatSeat::None;

	bool bBoatIMCAdded = false;
	bool bOnFootIMCRemoved = false;

	AActor* PrevViewTarget = nullptr;

	bool bHasLocalBoardPrediction = false;

	void OnPaddle();
	void OnReversePaddle();

	ABoat* ResolveBoat() const;

	void AddBoatIMC_Local();
	void RemoveBoatIMC_Local();
	void RemoveOnFootIMC_Local();
	void AddOnFootIMC_Local();

	void ApplySeatedState_Local(
		ACharacter* Char
	) const;

	void RestoreFromPrediction();

	void ApplyBoatViewBlend_Local(
		ABoat* Boat
	);

	UFUNCTION(Server, Reliable)
	void Server_PaddleStroke(
		ABoat* Boat
	);

	// 우클릭 후진 RPC
	UFUNCTION(Server, Reliable)
	void Server_ReversePaddleStroke(
		ABoat* Boat
	);

	// =====================================================
	// TEMP SOLO TEST
	// =====================================================

	// 로컬 BP 함수들이 공통으로 사용하는 함수
	void RequestTestPaddleSeat(
		EBoatSeat Seat,
		bool bReverse
	);

	// 서버에서 원하는 좌석의 실제 캐릭터를 찾음
	ACharacter* FindRowerForSeat_Server(
		ABoat* Boat,
		EBoatSeat Seat
	) const;

	// 클라이언트 -> 서버 테스트 입력
	UFUNCTION(Server, Reliable)
	void Server_TestPaddleSeat(
		ABoat* Boat,
		EBoatSeat Seat,
		bool bReverse
	);

	UFUNCTION(Client, Reliable)
	void Client_EnterBoatView(
		ABoat* Boat
	);

	UFUNCTION(Client, Reliable)
	void Client_BoardRejected();

	UFUNCTION(Client, Reliable)
	void Client_ConfirmBoard(
		ABoat* Boat,
		EBoatSeat Seat
	);

	UPROPERTY(
		ReplicatedUsing = OnRep_BoatInputLocked
	)
	bool bBoatInputLocked = false;

	UFUNCTION()
	void OnRep_BoatInputLocked();

	FTimerHandle BoatInputUnlockTimer;

	void SetBoatInputLocked_Local(
		bool bLocked
	);

	UFUNCTION(Server, Reliable)
	void Server_SetBoatInputLocked(
		bool bLocked,
		float DurationSeconds
	);

	UFUNCTION(Client, Reliable)
	void Client_SetBoatInputLocked(
		bool bLocked,
		float DurationSeconds
	);
};