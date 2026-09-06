#include "BoatPlayerController.h"

#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

ABoatPlayerController::ABoatPlayerController()
{
	bReplicates = true;
}

void ABoatPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;
}

void ABoatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC =
		Cast<UEnhancedInputComponent>(
			InputComponent
		);

	if (!EIC)
	{
		return;
	}

	if (IA_Paddle)
	{
		EIC->BindAction(
			IA_Paddle,
			ETriggerEvent::Started,
			this,
			&ABoatPlayerController::OnPaddle
		);
	}

	if (IA_ReversePaddle)
	{
		EIC->BindAction(
			IA_ReversePaddle,
			ETriggerEvent::Started,
			this,
			&ABoatPlayerController::
			OnReversePaddle
		);
	}
}

void ABoatPlayerController::
GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>&
	OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(
		OutLifetimeProps
	);

	DOREPLIFETIME(
		ABoatPlayerController,
		CurrentBoat
	);

	DOREPLIFETIME(
		ABoatPlayerController,
		MySeat
	);

	DOREPLIFETIME(
		ABoatPlayerController,
		bBoatInputLocked
	);
}

bool ABoatPlayerController::IsInBoat() const
{
	return
		IsValid(CurrentBoat) &&
		MySeat != EBoatSeat::None;
}

void ABoatPlayerController::AddBoatIMC_Local()
{
	if (!IsLocalController())
	{
		return;
	}

	if (bBoatIMCAdded)
	{
		return;
	}

	if (!IMC_Boat)
	{
		return;
	}

	if (
		ULocalPlayer* LP =
		GetLocalPlayer()
		)
	{
		if (
			UEnhancedInputLocalPlayerSubsystem*
			Subsys =
			ULocalPlayer::GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem
			>(LP)
			)
		{
			Subsys->AddMappingContext(
				IMC_Boat,
				50
			);

			bBoatIMCAdded = true;
		}
	}
}

void ABoatPlayerController::
RemoveBoatIMC_Local()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!bBoatIMCAdded)
	{
		return;
	}

	if (!IMC_Boat)
	{
		return;
	}

	if (
		ULocalPlayer* LP =
		GetLocalPlayer()
		)
	{
		if (
			UEnhancedInputLocalPlayerSubsystem*
			Subsys =
			ULocalPlayer::GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem
			>(LP)
			)
		{
			Subsys->RemoveMappingContext(
				IMC_Boat
			);

			bBoatIMCAdded = false;
		}
	}
}

void ABoatPlayerController::
RemoveOnFootIMC_Local()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!IMC_OnFoot)
	{
		return;
	}

	if (bOnFootIMCRemoved)
	{
		return;
	}

	if (
		ULocalPlayer* LP =
		GetLocalPlayer()
		)
	{
		if (
			UEnhancedInputLocalPlayerSubsystem*
			Subsys =
			ULocalPlayer::GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem
			>(LP)
			)
		{
			Subsys->RemoveMappingContext(
				IMC_OnFoot
			);

			bOnFootIMCRemoved = true;
		}
	}
}

void ABoatPlayerController::
AddOnFootIMC_Local()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!IMC_OnFoot)
	{
		return;
	}

	if (!bOnFootIMCRemoved)
	{
		return;
	}

	if (
		ULocalPlayer* LP =
		GetLocalPlayer()
		)
	{
		if (
			UEnhancedInputLocalPlayerSubsystem*
			Subsys =
			ULocalPlayer::GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem
			>(LP)
			)
		{
			Subsys->AddMappingContext(
				IMC_OnFoot,
				0
			);

			bOnFootIMCRemoved = false;
		}
	}
}

void ABoatPlayerController::
ApplySeatedState_Local(
	ACharacter* Char
) const
{
	if (!IsValid(Char))
	{
		return;
	}

	if (
		UCapsuleComponent* Capsule =
		Char->GetCapsuleComponent()
		)
	{
		Capsule->SetCollisionEnabled(
			ECollisionEnabled::NoCollision
		);

		Capsule->SetGenerateOverlapEvents(
			false
		);

		Capsule->
			SetNotifyRigidBodyCollision(
				false
			);
	}

	if (
		USkeletalMeshComponent* Mesh =
		Char->GetMesh()
		)
	{
		Mesh->SetVisibility(
			false,
			true
		);

		Mesh->SetHiddenInGame(
			true,
			true
		);

		Mesh->SetCollisionEnabled(
			ECollisionEnabled::NoCollision
		);

		Mesh->SetGenerateOverlapEvents(
			false
		);

		Mesh->
			SetNotifyRigidBodyCollision(
				false
			);
	}

	Char->SetActorEnableCollision(
		false
	);

	if (
		UCharacterMovementComponent* Move =
		Char->GetCharacterMovement()
		)
	{
		Move->StopMovementImmediately();

		Move->Velocity =
			FVector::ZeroVector;

		Move->DisableMovement();

		Move->SetMovementMode(
			MOVE_None
		);

		Move->bEnablePhysicsInteraction =
			false;

		Move->NetworkSmoothingMode =
			ENetworkSmoothingMode::Disabled;
	}
}

void ABoatPlayerController::
RestoreFromPrediction()
{
	ACharacter* Char =
		Cast<ACharacter>(
			GetPawn()
		);

	if (!IsValid(Char))
	{
		bHasLocalBoardPrediction = false;
		return;
	}

	Char->DetachFromActor(
		FDetachmentTransformRules::
		KeepWorldTransform
	);

	if (
		USkeletalMeshComponent* Mesh =
		Char->GetMesh()
		)
	{
		Mesh->SetHiddenInGame(
			false,
			true
		);

		Mesh->SetVisibility(
			true,
			true
		);

		Mesh->SetCollisionEnabled(
			ECollisionEnabled::NoCollision
		);

		Mesh->SetGenerateOverlapEvents(
			false
		);

		Mesh->
			SetNotifyRigidBodyCollision(
				false
			);
	}

	if (
		UCapsuleComponent* Capsule =
		Char->GetCapsuleComponent()
		)
	{
		Capsule->SetCollisionEnabled(
			ECollisionEnabled::
			QueryAndPhysics
		);

		Capsule->SetGenerateOverlapEvents(
			true
		);

		Capsule->
			SetNotifyRigidBodyCollision(
				false
			);
	}

	Char->SetActorEnableCollision(
		true
	);

	if (
		UCharacterMovementComponent* Move =
		Char->GetCharacterMovement()
		)
	{
		Move->SetMovementMode(
			MOVE_Walking
		);

		Move->NetworkSmoothingMode =
			ENetworkSmoothingMode::
			Exponential;

		Move->SetComponentTickEnabled(
			true
		);
	}

	if (IsLocalController())
	{
		AActor* Back =
			PrevViewTarget
			? PrevViewTarget
			: Char;

		SetViewTargetWithBlend(
			Back,
			0.1f
		);
	}

	RemoveBoatIMC_Local();
	AddOnFootIMC_Local();

	CurrentBoat = nullptr;

	MySeat =
		EBoatSeat::None;

	PrevViewTarget = nullptr;

	bHasLocalBoardPrediction =
		false;

	SetBoatInputLocked_Local(
		false
	);
}

void ABoatPlayerController::
ApplyBoatViewBlend_Local(
	ABoat* Boat
)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!IsValid(Boat))
	{
		return;
	}

	if (GetViewTarget() == Boat)
	{
		return;
	}

	ACharacter* Char =
		Cast<ACharacter>(
			GetPawn()
		);

	if (!IsValid(Char))
	{
		return;
	}

	Boat->TrySetBoatView_Local(
		Char
	);
}

void ABoatPlayerController::
PredictBoard_Local(
	ABoat* Boat,
	EBoatSeat Seat
)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!IsValid(Boat))
	{
		return;
	}

	ACharacter* Char =
		Cast<ACharacter>(
			GetPawn()
		);

	if (!IsValid(Char))
	{
		return;
	}

	if (
		Boat->IsSeatOccupiedByOther(
			Seat,
			Char
		)
		)
	{
		return;
	}

	USceneComponent* SeatComp =
		Boat->GetSeatComponent(
			Seat
		);

	if (!SeatComp)
	{
		return;
	}

	if (!PrevViewTarget)
	{
		PrevViewTarget =
			GetViewTarget();
	}

	Char->AttachToComponent(
		SeatComp,
		FAttachmentTransformRules::
		SnapToTargetNotIncludingScale
	);

	Char->SetActorRelativeLocation(
		FVector::ZeroVector
	);

	Char->SetActorRelativeRotation(
		FRotator::ZeroRotator
	);

	ApplySeatedState_Local(
		Char
	);

	AddBoatIMC_Local();
	RemoveOnFootIMC_Local();

	CurrentBoat = Boat;
	MySeat = Seat;

	bHasLocalBoardPrediction =
		true;
}

void ABoatPlayerController::
Server_RequestBoard_Implementation(
	ABoat* Boat,
	EBoatSeat Seat
)
{
	if (!IsValid(Boat))
	{
		Client_BoardRejected();
		return;
	}

	ACharacter* Char =
		Cast<ACharacter>(
			GetPawn()
		);

	if (!IsValid(Char))
	{
		Client_BoardRejected();
		return;
	}

	if (
		Boat->IsSeatOccupiedByOther(
			Seat,
			Char
		)
		)
	{
		Client_BoardRejected();
		return;
	}

	const bool bOk =
		Boat->TrySeatCharacter_Server(
			Char,
			Seat
		);

	if (!bOk)
	{
		Client_BoardRejected();
		return;
	}

	CurrentBoat = Boat;
	MySeat = Seat;

	ForceBoardOnServer(
		Boat,
		Seat
	);

	Client_ConfirmBoard(
		Boat,
		Seat
	);
}

void ABoatPlayerController::
Client_BoardRejected_Implementation()
{
	if (bHasLocalBoardPrediction)
	{
		RestoreFromPrediction();
		return;
	}

	CurrentBoat = nullptr;

	MySeat =
		EBoatSeat::None;

	RemoveBoatIMC_Local();
	AddOnFootIMC_Local();

	SetBoatInputLocked_Local(
		false
	);
}

void ABoatPlayerController::
Client_ConfirmBoard_Implementation(
	ABoat* Boat,
	EBoatSeat Seat
)
{
	CurrentBoat = Boat;
	MySeat = Seat;

	bHasLocalBoardPrediction =
		false;

	if (
		IsLocalController() &&
		IsValid(Boat)
		)
	{
		AddBoatIMC_Local();
		RemoveOnFootIMC_Local();
	}
}

void ABoatPlayerController::
OnRep_BoatInputLocked()
{
	SetBoatInputLocked_Local(
		bBoatInputLocked
	);
}

void ABoatPlayerController::
SetBoatInputLocked_Local(
	bool bLocked
)
{
	if (
		UWorld* W =
		GetWorld()
		)
	{
		W->GetTimerManager().
			ClearTimer(
				BoatInputUnlockTimer
			);
	}

	bBoatInputLocked = bLocked;
}

void ABoatPlayerController::
LockBoatInput(
	float DurationSeconds
)
{
	DurationSeconds =
		FMath::Max(
			0.01f,
			DurationSeconds
		);

	if (IsLocalController())
	{
		SetBoatInputLocked_Local(
			true
		);

		if (
			UWorld* W =
			GetWorld()
			)
		{
			W->GetTimerManager().
				SetTimer(
					BoatInputUnlockTimer,
					[this]()
					{
						SetBoatInputLocked_Local(
							false
						);
					},
					DurationSeconds,
					false
				);
		}
	}

	if (HasAuthority())
	{
		bBoatInputLocked = true;

		if (
			UWorld* W =
			GetWorld()
			)
		{
			W->GetTimerManager().
				ClearTimer(
					BoatInputUnlockTimer
				);

			W->GetTimerManager().
				SetTimer(
					BoatInputUnlockTimer,
					[this]()
					{
						bBoatInputLocked =
							false;

						SetBoatInputLocked_Local(
							false
						);

						Client_SetBoatInputLocked(
							false,
							0.f
						);
					},
					DurationSeconds,
					false
				);
		}

		Client_SetBoatInputLocked(
			true,
			DurationSeconds
		);
	}
	else
	{
		Server_SetBoatInputLocked(
			true,
			DurationSeconds
		);
	}
}

void ABoatPlayerController::
Server_SetBoatInputLocked_Implementation(
	bool bLocked,
	float DurationSeconds
)
{
	DurationSeconds =
		FMath::Max(
			0.01f,
			DurationSeconds
		);

	bBoatInputLocked = bLocked;

	if (
		UWorld* W =
		GetWorld()
		)
	{
		W->GetTimerManager().
			ClearTimer(
				BoatInputUnlockTimer
			);

		if (bLocked)
		{
			W->GetTimerManager().
				SetTimer(
					BoatInputUnlockTimer,
					[this]()
					{
						bBoatInputLocked =
							false;

						Client_SetBoatInputLocked(
							false,
							0.f
						);
					},
					DurationSeconds,
					false
				);
		}
	}

	Client_SetBoatInputLocked(
		bLocked,
		DurationSeconds
	);
}

void ABoatPlayerController::
Client_SetBoatInputLocked_Implementation(
	bool bLocked,
	float DurationSeconds
)
{
	SetBoatInputLocked_Local(
		bLocked
	);

	if (
		bLocked &&
		DurationSeconds > 0.f
		)
	{
		if (
			UWorld* W =
			GetWorld()
			)
		{
			W->GetTimerManager().
				ClearTimer(
					BoatInputUnlockTimer
				);

			W->GetTimerManager().
				SetTimer(
					BoatInputUnlockTimer,
					[this]()
					{
						SetBoatInputLocked_Local(
							false
						);
					},
					DurationSeconds,
					false
				);
		}
	}
}

ABoat* ABoatPlayerController::
ResolveBoat() const
{
	if (CurrentBoat)
	{
		return CurrentBoat;
	}

	APawn* P = GetPawn();

	if (!P)
	{
		return nullptr;
	}

	if (
		AActor* Parent =
		P->GetAttachParentActor()
		)
	{
		return Cast<ABoat>(
			Parent
		);
	}

	return nullptr;
}

// =====================================================
// 기존 좌클릭
// =====================================================

void ABoatPlayerController::OnPaddle()
{
	if (bBoatInputLocked)
	{
		return;
	}

	ABoat* Boat =
		ResolveBoat();

	if (!Boat)
	{
		return;
	}

	Server_PaddleStroke(
		Boat
	);
}

// =====================================================
// 기존 우클릭
// =====================================================

void ABoatPlayerController::
OnReversePaddle()
{
	if (bBoatInputLocked)
	{
		return;
	}

	ABoat* Boat =
		ResolveBoat();

	if (!Boat)
	{
		return;
	}

	Server_ReversePaddleStroke(
		Boat
	);
}

// =====================================================
// TEMP SOLO TEST
// 왼쪽 앞으로
// =====================================================

void ABoatPlayerController::
TestLeftForwardPaddle()
{
	RequestTestPaddleSeat(
		EBoatSeat::Left,
		false
	);
}

// =====================================================
// TEMP SOLO TEST
// 왼쪽 뒤로
// =====================================================

void ABoatPlayerController::
TestLeftReversePaddle()
{
	RequestTestPaddleSeat(
		EBoatSeat::Left,
		true
	);
}

// =====================================================
// TEMP SOLO TEST
// 오른쪽 앞으로
// =====================================================

void ABoatPlayerController::
TestRightForwardPaddle()
{
	RequestTestPaddleSeat(
		EBoatSeat::Right,
		false
	);
}

// =====================================================
// TEMP SOLO TEST
// 오른쪽 뒤로
// =====================================================

void ABoatPlayerController::
TestRightReversePaddle()
{
	RequestTestPaddleSeat(
		EBoatSeat::Right,
		true
	);
}

// =====================================================
// TEMP SOLO TEST
// 공통 요청
// =====================================================

void ABoatPlayerController::
RequestTestPaddleSeat(
	EBoatSeat Seat,
	bool bReverse
)
{
	// 기존 입력 잠금 그대로 존중
	if (bBoatInputLocked)
	{
		return;
	}

	if (Seat == EBoatSeat::None)
	{
		return;
	}

	ABoat* Boat =
		ResolveBoat();

	if (!IsValid(Boat))
	{
		return;
	}

	Server_TestPaddleSeat(
		Boat,
		Seat,
		bReverse
	);
}

// =====================================================
// 기존 좌클릭 Server RPC
// =====================================================

void ABoatPlayerController::
Server_PaddleStroke_Implementation(
	ABoat* Boat
)
{
	if (bBoatInputLocked)
	{
		return;
	}

	if (!Boat)
	{
		return;
	}

	ACharacter* Char =
		Cast<ACharacter>(
			GetPawn()
		);

	if (!Char)
	{
		return;
	}

	const EBoatSeat Seat =
		Boat->GetSeatForRower(
			Char
		);

	if (Seat == EBoatSeat::None)
	{
		return;
	}

	CurrentBoat = Boat;
	MySeat = Seat;

	// 기존 코드 그대로
	Boat->PaddleFromRower(
		Char
	);
}

// =====================================================
// 기존 우클릭 Server RPC
// =====================================================

void ABoatPlayerController::
Server_ReversePaddleStroke_Implementation(
	ABoat* Boat
)
{
	if (bBoatInputLocked)
	{
		return;
	}

	if (!Boat)
	{
		return;
	}

	ACharacter* Char =
		Cast<ACharacter>(
			GetPawn()
		);

	if (!Char)
	{
		return;
	}

	const EBoatSeat Seat =
		Boat->GetSeatForRower(
			Char
		);

	if (Seat == EBoatSeat::None)
	{
		return;
	}

	CurrentBoat = Boat;
	MySeat = Seat;

	// 기존 코드 그대로
	Boat->ReversePaddleFromRower(
		Char
	);
}

// =====================================================
// TEMP SOLO TEST
//
// 서버에서 원하는 좌석에 실제로 앉아 있는
// ACharacter를 찾는다.
//
// Boat의 LeftOccupant / RightOccupant를
// 노출하거나 수정하지 않기 위해
// 현재 플레이어들의 Pawn을 검사한다.
// =====================================================

ACharacter* ABoatPlayerController::
FindRowerForSeat_Server(
	ABoat* Boat,
	EBoatSeat Seat
) const
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	if (!IsValid(Boat))
	{
		return nullptr;
	}

	if (Seat == EBoatSeat::None)
	{
		return nullptr;
	}

	UWorld* World =
		GetWorld();

	if (!World)
	{
		return nullptr;
	}

	for (
		FConstPlayerControllerIterator It =
		World->
		GetPlayerControllerIterator();
		It;
		++It
		)
	{
		APlayerController* PC =
			It->Get();

		if (!IsValid(PC))
		{
			continue;
		}

		ACharacter* Candidate =
			Cast<ACharacter>(
				PC->GetPawn()
			);

		if (!IsValid(Candidate))
		{
			continue;
		}

		const EBoatSeat CandidateSeat =
			Boat->GetSeatForRower(
				Candidate
			);

		if (CandidateSeat == Seat)
		{
			return Candidate;
		}
	}

	return nullptr;
}

// =====================================================
// TEMP SOLO TEST
//
// 실제 테스트 입력 처리.
//
// 중요한 점:
// 새로운 보트 물리/패들 시스템을 만들지 않는다.
//
// 원하는 좌석의 실제 캐릭터를 찾아서
// 기존 PaddleFromRower 또는
// ReversePaddleFromRower에 그대로 넘긴다.
//
// 그래서 기존 시스템을 그대로 사용한다.
// =====================================================

void ABoatPlayerController::
Server_TestPaddleSeat_Implementation(
	ABoat* Boat,
	EBoatSeat Seat,
	bool bReverse
)
{
	// 기존 입력 잠금
	if (bBoatInputLocked)
	{
		return;
	}

	if (!IsValid(Boat))
	{
		return;
	}

	if (Seat == EBoatSeat::None)
	{
		return;
	}

	ACharacter* Requester =
		Cast<ACharacter>(
			GetPawn()
		);

	if (!IsValid(Requester))
	{
		return;
	}

	// 이 테스트 입력을 누른 플레이어 본인이
	// 실제로 이 보트에 타고 있는지 확인.
	const EBoatSeat RequesterSeat =
		Boat->GetSeatForRower(
			Requester
		);

	if (
		RequesterSeat ==
		EBoatSeat::None
		)
	{
		return;
	}

	CurrentBoat = Boat;
	MySeat = RequesterSeat;

	// =====================================================
	// 핵심 조건
	//
	// 반드시 두 명 모두 보트에 타고 있어야
	// 이 임시 테스트 입력이 작동함.
	// =====================================================

	if (
		!Boat->IsSeatOccupied(
			EBoatSeat::Left
		) ||
		!Boat->IsSeatOccupied(
			EBoatSeat::Right
		)
		)
	{
		return;
	}

	// 원하는 좌석의 실제 캐릭터 찾기
	ACharacter* TargetRower =
		FindRowerForSeat_Server(
			Boat,
			Seat
		);

	if (!IsValid(TargetRower))
	{
		return;
	}

	// =====================================================
	// 여기서 기존 시스템 그대로 사용
	// =====================================================

	if (bReverse)
	{
		Boat->ReversePaddleFromRower(
			TargetRower
		);
	}
	else
	{
		Boat->PaddleFromRower(
			TargetRower
		);
	}
}

void ABoatPlayerController::
ForceBoardOnServer(
	ABoat* Boat,
	EBoatSeat Seat
)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!Boat)
	{
		return;
	}

	CurrentBoat = Boat;
	MySeat = Seat;

	Client_EnterBoatView(
		Boat
	);
}

void ABoatPlayerController::
Client_EnterBoatView_Implementation(
	ABoat* Boat
)
{
	if (!Boat)
	{
		return;
	}

	bHasLocalBoardPrediction =
		false;

	ApplyBoatViewBlend_Local(
		Boat
	);

	AddBoatIMC_Local();
	RemoveOnFootIMC_Local();
}