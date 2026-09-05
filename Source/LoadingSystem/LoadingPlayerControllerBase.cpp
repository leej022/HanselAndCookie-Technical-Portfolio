#include "LoadingPlayerControllerBase.h"
#include "GI_Steam.h"
#include "WarningStateWidgetBase.h"

#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "UObject/UnrealType.h"

/*
================================================================================
LoadingPlayerControllerBase.cpp - 주석 보강본 (기존 로직 변경 없음)
================================================================================
이 PlayerController의 로딩 시스템 역할

- PlayerController Ownership을 이용한 Server RPC / Client RPC 통신 Gateway
- 각 Client 로컬의 PreTravel Movie Widget / MediaPlayer / SoundActor 관리
- Loading 동안 Gameplay Input 차단 + Mouse/UI Input 활성화
- TargetMap 도착 보고를 Client → Server로 전달
- 서버의 2인 도착 완료 판정을 Server → Client로 전달
- Gameplay Spawn 완료 후 Input/Enhanced Input Context 복구

중요:
실제 Travel 전체 State는 GI_Steam이 관리하고,
LoadingPlayerControllerBase는 네트워크 통신과 각 플레이어의 Local UI/Input을 담당한다.
================================================================================
*/

// ============================================================================
// ALoadingPlayerControllerBase::BeginPlay
// ----------------------------------------------------------------------------
// 역할: LocalController 초기 UI/Input 환경을 준비하고 GI가 Loading 중이면 Overlay를 복구한다.
// 실행 위치/권한: 각 PlayerController BeginPlay
// 데이터 흐름: Local PC → Register Input/UI → GI.bLoadingOn이면 RefreshLoadingOverlay
// ============================================================================
void ALoadingPlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		RegisterKnownContexts();

		if (bAutoCreatePersistentGameplayWidget)
		{
			CreatePersistentGameplayWidget();
		}

		if (bAutoCreateWarningStateWidget)
		{
			CreateWarningStateWidget();
		}

		if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
		{
			// 로딩 상태가 살아있으면 오버레이/입력잠금을 다시 복구
			if (GI->bLoadingOn)
			{
				GI->RefreshLoadingOverlay();
			}
		}
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::EndPlay
// ----------------------------------------------------------------------------
// 역할: PlayerController 종료 시 PreTravel Movie, Persistent UI, Warning UI, Timer를 정리한다.
// ============================================================================
void ALoadingPlayerControllerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
	{
		StopPreTravelMovieLocal(true);

		RemovePersistentGameplayWidget();
		RemoveWarningStateWidget();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningStateBeginHideTimerHandle);
		World->GetTimerManager().ClearTimer(WarningStateFinishHideTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// ALoadingPlayerControllerBase::ProcessPlayerInput
// ----------------------------------------------------------------------------
// 역할: bLoadingInputLocked 동안 Gameplay Input 처리 자체를 중단하고 눌린 키를 Flush한다.
// 실행 위치/권한: Local PlayerController 매 프레임 입력 처리
// ============================================================================
void ALoadingPlayerControllerBase::ProcessPlayerInput(const float DeltaTime, const bool bGamePaused)
{
	// 로딩 중에는 게임 입력 처리 자체를 끊는다.
	if (bLoadingInputLocked)
	{
		if (PlayerInput)
		{
			PlayerInput->FlushPressedKeys();
		}
		return;
	}

	Super::ProcessPlayerInput(DeltaTime, bGamePaused);
}

// ============================================================================
// ALoadingPlayerControllerBase::GetEnhancedInputSubsystem
// ----------------------------------------------------------------------------
// 역할: 현재 LocalPlayer의 EnhancedInputLocalPlayerSubsystem을 반환한다.
// ============================================================================
UEnhancedInputLocalPlayerSubsystem* ALoadingPlayerControllerBase::GetEnhancedInputSubsystem() const
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}
	return nullptr;
}

// ============================================================================
// ALoadingPlayerControllerBase::GetEnhancedInputUserSettings
// ----------------------------------------------------------------------------
// 역할: 현재 LocalPlayer의 Enhanced Input UserSettings를 반환한다.
// ============================================================================
UEnhancedInputUserSettings* ALoadingPlayerControllerBase::GetEnhancedInputUserSettings() const
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem())
	{
		return Subsystem->GetUserSettings();
	}
	return nullptr;
}

// ============================================================================
// ALoadingPlayerControllerBase::RegisterKnownContexts
// ----------------------------------------------------------------------------
// 역할: 설정/리바인딩에 사용할 InputMappingContext들을 UserSettings에 등록한다.
// ============================================================================
void ALoadingPlayerControllerBase::RegisterKnownContexts()
{
	if (!IsLocalController())
	{
		return;
	}

	for (UInputMappingContext* Context : ContextsToRegisterForSettings)
	{
		RegisterContextForSettings(Context);
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::RegisterContextForSettings
// ----------------------------------------------------------------------------
// 역할: 특정 MappingContext를 Enhanced Input UserSettings에 등록한다.
// ============================================================================
bool ALoadingPlayerControllerBase::RegisterContextForSettings(UInputMappingContext* MappingContext)
{
	if (!IsLocalController() || !MappingContext)
	{
		return false;
	}

	if (UEnhancedInputUserSettings* Settings = GetEnhancedInputUserSettings())
	{
		if (!Settings->IsMappingContextRegistered(MappingContext))
		{
			return Settings->RegisterInputMappingContext(MappingContext);
		}
		return true;
	}

	return false;
}

// ============================================================================
// ALoadingPlayerControllerBase::AddGameplayMappingContext
// ----------------------------------------------------------------------------
// 역할: Gameplay MappingContext를 LocalPlayer Subsystem에 추가하고 잔여 키 입력을 Flush한다.
// ============================================================================
bool ALoadingPlayerControllerBase::AddGameplayMappingContext(UInputMappingContext* MappingContext, int32 Priority)
{
	if (!IsLocalController() || !MappingContext)
	{
		return false;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem())
	{
		FModifyContextOptions Options;
		Options.bNotifyUserSettings = true;
		Options.bIgnoreAllPressedKeysUntilRelease = true;

		Subsystem->AddMappingContext(MappingContext, Priority, Options);

		if (PlayerInput)
		{
			PlayerInput->FlushPressedKeys();
		}

		return true;
	}

	return false;
}

// ============================================================================
// ALoadingPlayerControllerBase::RemoveGameplayMappingContext
// ----------------------------------------------------------------------------
// 역할: Gameplay MappingContext를 LocalPlayer Subsystem에서 제거한다.
// ============================================================================
bool ALoadingPlayerControllerBase::RemoveGameplayMappingContext(UInputMappingContext* MappingContext)
{
	if (!IsLocalController() || !MappingContext)
	{
		return false;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem())
	{
		FModifyContextOptions Options;
		Options.bNotifyUserSettings = false;
		Options.bIgnoreAllPressedKeysUntilRelease = true;

		Subsystem->RemoveMappingContext(MappingContext, Options);

		if (PlayerInput)
		{
			PlayerInput->FlushPressedKeys();
		}

		return true;
	}

	return false;
}

// ============================================================================
// ALoadingPlayerControllerBase::RebindPlayerKey
// ----------------------------------------------------------------------------
// 역할: 사용자가 선택한 새 키를 PlayerMappable Input 설정에 저장한다.
// ============================================================================
bool ALoadingPlayerControllerBase::RebindPlayerKey(FName MappingName, FKey NewKey)
{
	if (!NewKey.IsValid())
	{
		return false;
	}

	if (UEnhancedInputUserSettings* Settings = GetEnhancedInputUserSettings())
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = MappingName;
		Args.NewKey = NewKey;
		Args.Slot = EPlayerMappableKeySlot::First;

		FGameplayTagContainer FailureReason;
		Settings->MapPlayerKey(Args, FailureReason);

		const bool bSuccess = (FailureReason.Num() == 0);
		if (bSuccess)
		{
			Settings->SaveSettings();
		}
		return bSuccess;
	}

	return false;
}

// ============================================================================
// ALoadingPlayerControllerBase::ResetPlayerKey
// ----------------------------------------------------------------------------
// 역할: 특정 Mapping Row의 사용자 키 설정을 기본값으로 되돌린다.
// ============================================================================
bool ALoadingPlayerControllerBase::ResetPlayerKey(FName MappingName)
{
	if (UEnhancedInputUserSettings* Settings = GetEnhancedInputUserSettings())
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = MappingName;
		Args.Slot = EPlayerMappableKeySlot::First;

		FGameplayTagContainer FailureReason;
		Settings->ResetAllPlayerKeysInRow(Args, FailureReason);

		const bool bSuccess = (FailureReason.Num() == 0);
		if (bSuccess)
		{
			Settings->SaveSettings();
		}
		return bSuccess;
	}

	return false;
}

// ============================================================================
// ALoadingPlayerControllerBase::GetCurrentPlayerKey
// ----------------------------------------------------------------------------
// 역할: 현재 Mapping Row에 적용된 사용자 키를 조회한다.
// ============================================================================
bool ALoadingPlayerControllerBase::GetCurrentPlayerKey(FName MappingName, FKey& OutKey) const
{
	OutKey = FKey();

	if (UEnhancedInputUserSettings* Settings = GetEnhancedInputUserSettings())
	{
		const TSet<FPlayerKeyMapping>& FoundMappings = Settings->FindMappingsInRow(MappingName);

		for (const FPlayerKeyMapping& Mapping : FoundMappings)
		{
			if (Mapping.GetSlot() == EPlayerMappableKeySlot::First)
			{
				OutKey = Mapping.GetCurrentKey();
				return OutKey.IsValid();
			}
		}
	}

	return false;
}

// ============================================================================
// ALoadingPlayerControllerBase::CreatePersistentGameplayWidget
// ----------------------------------------------------------------------------
// 역할: Crosshair/HUD 등 항상 사용할 Gameplay Widget을 Local Viewport에 생성한다.
// ============================================================================
UUserWidget* ALoadingPlayerControllerBase::CreatePersistentGameplayWidget()
{
	if (!IsLocalController())
	{
		return nullptr;
	}

	if (!PersistentGameplayWidgetClass)
	{
		return nullptr;
	}

	if (PersistentGameplayWidgetInstance)
	{
		if (!PersistentGameplayWidgetInstance->IsInViewport())
		{
			PersistentGameplayWidgetInstance->AddToViewport(PersistentGameplayWidgetZOrder);
		}
		return PersistentGameplayWidgetInstance;
	}

	PersistentGameplayWidgetInstance = CreateWidget<UUserWidget>(this, PersistentGameplayWidgetClass);
	if (PersistentGameplayWidgetInstance)
	{
		PersistentGameplayWidgetInstance->AddToViewport(PersistentGameplayWidgetZOrder);
	}

	return PersistentGameplayWidgetInstance;
}

// ============================================================================
// ALoadingPlayerControllerBase::RemovePersistentGameplayWidget
// ----------------------------------------------------------------------------
// 역할: Persistent Gameplay Widget을 Viewport에서 제거한다.
// ============================================================================
void ALoadingPlayerControllerBase::RemovePersistentGameplayWidget()
{
	if (PersistentGameplayWidgetInstance)
	{
		PersistentGameplayWidgetInstance->RemoveFromParent();
		PersistentGameplayWidgetInstance = nullptr;
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::SetPersistentGameplayWidgetVisible
// ----------------------------------------------------------------------------
// 역할: Travel/Cinematic 중 Persistent HUD를 숨기거나 다시 표시한다.
// ============================================================================
void ALoadingPlayerControllerBase::SetPersistentGameplayWidgetVisible(bool bVisible)
{
	if (bVisible)
	{
		if (!PersistentGameplayWidgetInstance)
		{
			CreatePersistentGameplayWidget();
		}

		if (PersistentGameplayWidgetInstance)
		{
			PersistentGameplayWidgetInstance->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else
	{
		if (PersistentGameplayWidgetInstance)
		{
			PersistentGameplayWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::CreateWarningStateWidget
// ----------------------------------------------------------------------------
// 역할: WarningState UI를 Local Viewport에 생성하고 현재 경고 상태를 반영한다.
// ============================================================================
UWarningStateWidgetBase* ALoadingPlayerControllerBase::CreateWarningStateWidget()
{
	if (!IsLocalController())
	{
		return nullptr;
	}

	if (!WarningStateWidgetClass)
	{
		return nullptr;
	}

	if (WarningStateWidgetInstance)
	{
		if (!WarningStateWidgetInstance->IsInViewport())
		{
			WarningStateWidgetInstance->AddToViewport(WarningStateWidgetZOrder);
		}

		WarningStateWidgetInstance->SetWarningStateText(CurrentWarningStateText);

		if (bWarningStateVisible)
		{
			WarningStateWidgetInstance->ShowWarningStateAnimated();
		}
		else
		{
			WarningStateWidgetInstance->FinishHideWarningState();
		}

		return WarningStateWidgetInstance;
	}

	WarningStateWidgetInstance = CreateWidget<UWarningStateWidgetBase>(this, WarningStateWidgetClass);
	if (WarningStateWidgetInstance)
	{
		WarningStateWidgetInstance->AddToViewport(WarningStateWidgetZOrder);
		WarningStateWidgetInstance->SetWarningStateText(CurrentWarningStateText);
		WarningStateWidgetInstance->FinishHideWarningState();
	}

	return WarningStateWidgetInstance;
}

// ============================================================================
// ALoadingPlayerControllerBase::RemoveWarningStateWidget
// ----------------------------------------------------------------------------
// 역할: WarningState UI와 관련 Timer를 정리한다.
// ============================================================================
void ALoadingPlayerControllerBase::RemoveWarningStateWidget()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningStateBeginHideTimerHandle);
		World->GetTimerManager().ClearTimer(WarningStateFinishHideTimerHandle);
	}

	if (WarningStateWidgetInstance)
	{
		WarningStateWidgetInstance->RemoveFromParent();
		WarningStateWidgetInstance = nullptr;
	}

	CurrentWarningStateText = FText::GetEmpty();
	bWarningStateVisible = false;
}

// ============================================================================
// ALoadingPlayerControllerBase::SetWarningStateWidgetVisible
// ----------------------------------------------------------------------------
// 역할: WarningState UI의 표시/숨김 애니메이션 상태를 전환한다.
// ============================================================================
void ALoadingPlayerControllerBase::SetWarningStateWidgetVisible(bool bVisible)
{
	bWarningStateVisible = bVisible;

	if (bVisible)
	{
		if (!WarningStateWidgetInstance)
		{
			CreateWarningStateWidget();
		}

		if (WarningStateWidgetInstance)
		{
			WarningStateWidgetInstance->ShowWarningStateAnimated();
		}
	}
	else
	{
		if (WarningStateWidgetInstance)
		{
			WarningStateWidgetInstance->FinishHideWarningState();
		}
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::ShowWarningStateLocal
// ----------------------------------------------------------------------------
// 역할: 로컬 Warning 메시지를 설정하고 일정 시간 후 FadeOut하도록 Timer를 건다.
// ============================================================================
void ALoadingPlayerControllerBase::ShowWarningStateLocal(const FText& InText, float Duration)
{
	if (!IsLocalController())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningStateBeginHideTimerHandle);
		World->GetTimerManager().ClearTimer(WarningStateFinishHideTimerHandle);
	}

	CurrentWarningStateText = InText;
	bWarningStateVisible = true;

	if (!WarningStateWidgetInstance)
	{
		CreateWarningStateWidget();
	}

	if (WarningStateWidgetInstance)
	{
		WarningStateWidgetInstance->SetWarningStateText(CurrentWarningStateText);
		WarningStateWidgetInstance->ShowWarningStateAnimated();
	}

	if (UWorld* World = GetWorld())
	{
		const float FinalDuration = (Duration > 0.0f) ? Duration : DefaultWarningStateDuration;

		float FadeOutDuration = 0.0f;
		if (WarningStateWidgetInstance)
		{
			FadeOutDuration = WarningStateWidgetInstance->GetWarningStateFadeOutDuration();
		}

		const float DelayBeforeFadeOut = FMath::Max(FinalDuration - FadeOutDuration, 0.0f);

		if (FinalDuration > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				WarningStateBeginHideTimerHandle,
				this,
				&ALoadingPlayerControllerBase::BeginHideWarningStateLocal,
				DelayBeforeFadeOut,
				false);
		}
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::BeginHideWarningStateLocal
// ----------------------------------------------------------------------------
// 역할: Warning UI의 FadeOut 애니메이션을 시작하고 최종 숨김 Timer를 설정한다.
// ============================================================================
void ALoadingPlayerControllerBase::BeginHideWarningStateLocal()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningStateBeginHideTimerHandle);
	}

	bWarningStateVisible = false;

	if (!WarningStateWidgetInstance)
	{
		return;
	}

	WarningStateWidgetInstance->BeginHideWarningStateAnimated();

	const float FadeOutDuration = WarningStateWidgetInstance->GetWarningStateFadeOutDuration();

	if (FadeOutDuration <= KINDA_SMALL_NUMBER)
	{
		HideWarningStateLocal();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WarningStateFinishHideTimerHandle,
			this,
			&ALoadingPlayerControllerBase::HideWarningStateLocal,
			FadeOutDuration,
			false);
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::HideWarningStateLocal
// ----------------------------------------------------------------------------
// 역할: Warning UI를 최종 숨김 상태로 만든다.
// ============================================================================
void ALoadingPlayerControllerBase::HideWarningStateLocal()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningStateBeginHideTimerHandle);
		World->GetTimerManager().ClearTimer(WarningStateFinishHideTimerHandle);
	}

	bWarningStateVisible = false;

	if (WarningStateWidgetInstance)
	{
		WarningStateWidgetInstance->FinishHideWarningState();
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_ShowWarningState_Implementation
// ----------------------------------------------------------------------------
// 역할: 서버가 특정 Owning Client에게 Warning UI 표시를 지시하는 Client RPC 구현이다.
// 실행 위치/권한: Server → Owning Client
// ============================================================================
void ALoadingPlayerControllerBase::Client_ShowWarningState_Implementation(const FText& InText, float Duration)
{
	ShowWarningStateLocal(InText, Duration);
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_HideWarningState_Implementation
// ----------------------------------------------------------------------------
// 역할: 서버가 특정 Owning Client의 Warning UI를 숨기는 Client RPC 구현이다.
// 실행 위치/권한: Server → Owning Client
// ============================================================================
void ALoadingPlayerControllerBase::Client_HideWarningState_Implementation()
{
	HideWarningStateLocal();
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_PlayWarningStateSFX_Implementation
// ----------------------------------------------------------------------------
// 역할: Warning SFX를 해당 Owning Client에서 로컬 2D Sound로 재생한다.
// 실행 위치/권한: Server → Owning Client
// ============================================================================
void ALoadingPlayerControllerBase::Client_PlayWarningStateSFX_Implementation(USoundBase* InSound)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!InSound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, InSound);
}

// ============================================================================
// ALoadingPlayerControllerBase::EnterLoadingInputMode
// ----------------------------------------------------------------------------
// 역할: Loading/Cinematic 동안 Gameplay 입력을 잠그고 Mouse/UIOnly 입력만 허용한다.
// 실행 위치/권한: 각 Client 로컬 PC
// ============================================================================
void ALoadingPlayerControllerBase::EnterLoadingInputMode(UUserWidget* FocusWidget)
{
	if (!IsLocalController())
	{
		return;
	}

	// 먼저 게임 입력 처리 자체를 잠근다.
	bLoadingInputLocked = true;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	if (FocusWidget)
	{
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}

	SetInputMode(InputMode);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);

	if (PlayerInput)
	{
		PlayerInput->FlushPressedKeys();
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::ExitLoadingInputMode
// ----------------------------------------------------------------------------
// 역할: Loading 종료 후 GameOnly 입력, 이동/시점 입력, Mouse 상태를 Gameplay용으로 복구한다.
// 실행 위치/권한: 각 Client 로컬 PC
// ============================================================================
void ALoadingPlayerControllerBase::ExitLoadingInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();

	if (PlayerInput)
	{
		PlayerInput->FlushPressedKeys();
	}

	// 마지막에 다시 입력 처리 허용
	bLoadingInputLocked = false;
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_BeginLoadingForMapURL_Implementation
// ----------------------------------------------------------------------------
// 역할: 서버가 특정 Client에게 해당 MapURL의 Loading을 시작시키는 Client RPC이며 실제 상태 관리는 Local GI로 넘긴다.
// 실행 위치/권한: Server → Owning Client
// 데이터 흐름: Client RPC → Local GI.BeginLoadingForMapURL(MapURL)
// ============================================================================
void ALoadingPlayerControllerBase::Client_BeginLoadingForMapURL_Implementation(const FString& MapURL)
{
	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		GI->BeginLoadingForMapURL(MapURL);
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_EndLoading_Implementation
// ----------------------------------------------------------------------------
// 역할: Gameplay 준비 완료 후 해당 Client GI에 Loading 종료를 요청하는 Client RPC다.
// 실행 위치/권한: Server → Owning Client
// 데이터 흐름: Client RPC → Local GI.EndLoading
// ============================================================================
void ALoadingPlayerControllerBase::Client_EndLoading_Implementation()
{
	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		GI->EndLoading();
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::Server_ReportReachedTargetLoadingMap_Implementation
// ----------------------------------------------------------------------------
// 역할: Client가 TargetMap 도착을 보고하는 Server RPC이며 서버 GI 도착 목록으로 전달한다.
// 실행 위치/권한: Owning Client → Server
// 데이터 흐름: LoadedMapName + this PC → Server GI.ServerRegisterReachedTargetLoadingMap
// ============================================================================
void ALoadingPlayerControllerBase::Server_ReportReachedTargetLoadingMap_Implementation(FName LoadedMapName)
{
	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		GI->ServerRegisterReachedTargetLoadingMap(this, LoadedMapName);
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_ConfirmAllPlayersReachedTargetLoadingMap_Implementation
// ----------------------------------------------------------------------------
// 역할: 서버가 2/2 TargetMap 도착 완료를 해당 Client의 Local GI에 적용하는 Client RPC다.
// 실행 위치/권한: Server → Owning Client
// ============================================================================
void ALoadingPlayerControllerBase::Client_ConfirmAllPlayersReachedTargetLoadingMap_Implementation(FName LoadedMapName)
{
	if (!IsLocalController())
	{
		return;
	}

	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		GI->ConfirmAllPlayersReachedTargetLoadingMap(LoadedMapName);
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_RestoreGameplayInput_Implementation
// ----------------------------------------------------------------------------
// 역할: Spawn/Possess 완료 후 Loading Input을 풀고 기본 Gameplay MappingContext를 다시 보장한다.
// 실행 위치/권한: Server → Owning Client
// ============================================================================
void ALoadingPlayerControllerBase::Client_RestoreGameplayInput_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	// 남아있을 수 있는 로딩 입력 잠금/커서/UIOnly 상태 복구
	ExitLoadingInputMode();

	// 기본 게임플레이 IMC를 다시 보장
	if (DefaultGameplayMappingContext)
	{
		RemoveGameplayMappingContext(DefaultGameplayMappingContext);
		AddGameplayMappingContext(DefaultGameplayMappingContext, DefaultGameplayMappingPriority);
	}
	else if (PlayerInput)
	{
		PlayerInput->FlushPressedKeys();
	}
}

// =========================
// PreTravel Cinematic
// =========================

// ============================================================================
// ALoadingPlayerControllerBase::BeginPreTravelCinematicAsServer
// ----------------------------------------------------------------------------
// 역할: Server Authority에서 PreTravel Cinematic/Travel 흐름을 GI에 시작 요청한다.
// 실행 위치/권한: Server PC
// 데이터 흐름: LoadingPC → Server GI.BeginPreTravelCinematicAsServer
// ============================================================================
void ALoadingPlayerControllerBase::BeginPreTravelCinematicAsServer(const FString& MapURL, UMediaSource* CinematicMediaSource)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadingPC::BeginPreTravelCinematicAsServer - authority only."));
		return;
	}

	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		GI->BeginPreTravelCinematicAsServer(MapURL, CinematicMediaSource, this);
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::Server_RequestPreTravelSkipVote_Implementation
// ----------------------------------------------------------------------------
// 역할: Client의 Skip 버튼/영상 종료를 Server GI Vote 집계로 전달하는 Server RPC다.
// 실행 위치/권한: Owning Client → Server
// ============================================================================
void ALoadingPlayerControllerBase::Server_RequestPreTravelSkipVote_Implementation()
{
	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		GI->ServerRegisterPreTravelSkipVote(this);
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_PlayPreTravelMovie_Implementation
// ----------------------------------------------------------------------------
// 역할: 각 Client 로컬에서 Gameplay Audio/UI/Input을 전환하고 Movie Widget, SoundActor, MediaPlayer를 재생한다.
// 실행 위치/권한: Server → Owning Client
// 데이터 흐름: Mute + HUD Hide + UIOnly → Movie WBP/SoundActor → MediaPlayer Play
// ============================================================================
void ALoadingPlayerControllerBase::Client_PlayPreTravelMovie_Implementation(const FString& MapURL, UMediaSource* CinematicMediaSource, int32 TotalCount)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!CinematicMediaSource)
	{
		Server_RequestPreTravelSkipVote();
		return;
	}

	// 이전에 남아있던 시네마틱 위젯/사운드 정리
	StopPreTravelMovieLocal(true);

	CurrentPreTravelMapURL = MapURL;
	CurrentPreTravelMediaSource = CinematicMediaSource;
	bPreTravelLocalFinishedOrVoted = false;

	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		// NetworkFailure가 영상 도중 발생해도 로비로 복귀할 수 있도록 로컬 영상 상태를 GI에 기록.
		GI->SetLocalPreTravelMovieActive(true);

		// Music / SFX만 죽임. Master는 건드리지 않음.
		GI->BeginPreTravelCinematicAudioMute(0.05f);
	}

	SetPersistentGameplayWidgetVisible(false);
	SetWarningStateWidgetVisible(false);

	// =========================
	// 1. 영상 위젯 생성
	// =========================
	if (PreTravelMovieWidgetClass)
	{
		PreTravelMovieWidgetInstance = CreateWidget<UUserWidget>(this, PreTravelMovieWidgetClass);
		if (PreTravelMovieWidgetInstance)
		{
			SetOwnerLoadingPCOnPreTravelWidget();

			PreTravelMovieWidgetInstance->AddToViewport(PreTravelMovieWidgetZOrder);
			ApplyPreTravelSkipVoteToWidget(0, TotalCount);

			EnterLoadingInputMode(PreTravelMovieWidgetInstance);

			bShowMouseCursor = true;
			bEnableClickEvents = true;
			bEnableMouseOverEvents = true;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadingPC::Client_PlayPreTravelMovie - PreTravelMovieWidgetClass is not assigned."));
	}

	// =========================
	// 2. 각 클라이언트 로컬에서 사운드 Actor Spawn
	// 중요: 서버에서만 Spawn하면 클라는 안 들림.
	// 이 함수는 Client RPC이므로 각 클라가 자기 로컬 월드에 직접 Spawn해야 함.
	// =========================
	if (PreTravelMovieSoundActorClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetPawn();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// APlayerController에 SpawnLocation 멤버가 있어서 이름 겹치면 C4458 에러남.
		// 그래서 LocalSpawnLocation / LocalSpawnRotation 이름 사용.
		FVector LocalSpawnLocation = FVector::ZeroVector;
		FRotator LocalSpawnRotation = FRotator::ZeroRotator;

		if (PlayerCameraManager)
		{
			LocalSpawnLocation = PlayerCameraManager->GetCameraLocation();
			LocalSpawnRotation = PlayerCameraManager->GetCameraRotation();
		}
		else if (GetPawn())
		{
			LocalSpawnLocation = GetPawn()->GetActorLocation();
			LocalSpawnRotation = GetPawn()->GetActorRotation();
		}

		PreTravelMovieSoundActorInstance = GetWorld()->SpawnActor<AActor>(
			PreTravelMovieSoundActorClass,
			LocalSpawnLocation,
			LocalSpawnRotation,
			SpawnParams
		);

		if (!PreTravelMovieSoundActorInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("LoadingPC::Client_PlayPreTravelMovie - Failed to spawn PreTravelMovieSoundActor."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadingPC::Client_PlayPreTravelMovie - PreTravelMovieSoundActorClass is not assigned."));
	}

	// =========================
	// 3. MediaPlayer 재생
	// MediaSoundActor가 MP_PreTravelMovie를 바라보고 있으므로
	// Actor를 먼저 Spawn하고 그 다음 OpenSource 하는 게 안전함.
	// =========================
	if (!PreTravelMediaPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadingPC::Client_PlayPreTravelMovie - PreTravelMediaPlayer is not assigned."));
		Server_RequestPreTravelSkipVote();
		return;
	}

	PreTravelMediaPlayer->OnEndReached.RemoveDynamic(this, &ALoadingPlayerControllerBase::HandlePreTravelMovieEndReached);
	PreTravelMediaPlayer->OnEndReached.AddDynamic(this, &ALoadingPlayerControllerBase::HandlePreTravelMovieEndReached);

	PreTravelMediaPlayer->Close();

	const bool bOpened = PreTravelMediaPlayer->OpenSource(CinematicMediaSource);

	if (!bOpened)
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadingPC::Client_PlayPreTravelMovie - Failed to open media source."));
		Server_RequestPreTravelSkipVote();
		return;
	}

	PreTravelMediaPlayer->Rewind();
	PreTravelMediaPlayer->Play();
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_UpdatePreTravelSkipVote_Implementation
// ----------------------------------------------------------------------------
// 역할: 현재 Skip Vote 수를 각 Client의 PreTravel WBP에 갱신한다.
// 실행 위치/권한: Server → Owning Client
// ============================================================================
void ALoadingPlayerControllerBase::Client_UpdatePreTravelSkipVote_Implementation(int32 VotedCount, int32 TotalCount)
{
	if (!IsLocalController())
	{
		return;
	}

	ApplyPreTravelSkipVoteToWidget(VotedCount, TotalCount);
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_StopPreTravelMovieAndBeginLoading_Implementation
// ----------------------------------------------------------------------------
// 역할: PreTravel Movie를 정리하되 Audio는 유지하고 Local GI의 Loading WBP를 ServerTravel 전에 먼저 시작한다.
// 실행 위치/권한: Server → Owning Client
// 데이터 흐름: Stop Movie(false) → GI.BeginLoadingForMapURL
// ============================================================================
void ALoadingPlayerControllerBase::Client_StopPreTravelMovieAndBeginLoading_Implementation(const FString& MapURL)
{
	if (!IsLocalController())
	{
		return;
	}

	// 여기서 오디오는 바로 복구하지 않는다.
	// 바로 기존 로딩화면 + ServerTravel로 넘어가므로,
	// 다음 맵 로딩 완료 시 GI_Steam::HandlePostLoadMap에서 복구한다.
	StopPreTravelMovieLocal(false);

	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		GI->BeginLoadingForMapURL(MapURL);
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::HandlePreTravelMovieEndReached
// ----------------------------------------------------------------------------
// 역할: MediaPlayer 자연 종료를 Skip/완료 Vote 1회로 처리해 Server RPC를 호출한다.
// 실행 위치/권한: 각 Client 로컬
// ============================================================================
void ALoadingPlayerControllerBase::HandlePreTravelMovieEndReached()
{
	if (!IsLocalController())
	{
		return;
	}

	if (bPreTravelLocalFinishedOrVoted)
	{
		return;
	}

	bPreTravelLocalFinishedOrVoted = true;
	MarkPreTravelWidgetLocalVoted();

	Server_RequestPreTravelSkipVote();
}

// ============================================================================
// ALoadingPlayerControllerBase::StopPreTravelMovieLocal
// ----------------------------------------------------------------------------
// 역할: MediaPlayer, SoundActor, Movie Widget을 로컬에서 정리하고 필요 시 Audio 설정을 즉시 복구한다.
// ============================================================================
void ALoadingPlayerControllerBase::StopPreTravelMovieLocal(bool bRestoreAudioImmediately)
{
	if (PreTravelMediaPlayer)
	{
		PreTravelMediaPlayer->OnEndReached.RemoveDynamic(this, &ALoadingPlayerControllerBase::HandlePreTravelMovieEndReached);
		PreTravelMediaPlayer->Close();
	}

	if (PreTravelMovieSoundActorInstance)
	{
		PreTravelMovieSoundActorInstance->Destroy();
		PreTravelMovieSoundActorInstance = nullptr;
	}

	if (PreTravelMovieWidgetInstance)
	{
		PreTravelMovieWidgetInstance->RemoveFromParent();
		PreTravelMovieWidgetInstance = nullptr;
	}

	CurrentPreTravelMapURL.Empty();
	CurrentPreTravelMediaSource = nullptr;
	bPreTravelLocalFinishedOrVoted = false;

	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		GI->SetLocalPreTravelMovieActive(false);

		if (bRestoreAudioImmediately)
		{
			GI->EndPreTravelCinematicAudioMute(0.05f);
		}
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::SetOwnerLoadingPCOnPreTravelWidget
// ----------------------------------------------------------------------------
// 역할: PreTravel WBP의 OwnerLoadingPC 프로퍼티에 현재 PC를 주입해 Skip 버튼이 RPC를 호출할 수 있게 연결한다.
// ============================================================================
void ALoadingPlayerControllerBase::SetOwnerLoadingPCOnPreTravelWidget()
{
	if (!PreTravelMovieWidgetInstance)
	{
		return;
	}

	static const FName OwnerPropertyName(TEXT("OwnerLoadingPC"));

	if (FObjectPropertyBase* ObjectProp = FindFProperty<FObjectPropertyBase>(PreTravelMovieWidgetInstance->GetClass(), OwnerPropertyName))
	{
		ObjectProp->SetObjectPropertyValue_InContainer(PreTravelMovieWidgetInstance, this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadingPC::SetOwnerLoadingPCOnPreTravelWidget - OwnerLoadingPC property not found on widget."));
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::ApplyPreTravelSkipVoteToWidget
// ----------------------------------------------------------------------------
// 역할: Reflection으로 PreTravel WBP의 Vote 표시 함수/프로퍼티에 현재 VoteCount/TotalCount를 전달한다.
// ============================================================================
void ALoadingPlayerControllerBase::ApplyPreTravelSkipVoteToWidget(int32 VotedCount, int32 TotalCount)
{
	if (!PreTravelMovieWidgetInstance)
	{
		return;
	}

	static const FName FuncName(TEXT("ApplySkipVoteStatus"));

	UFunction* Func = PreTravelMovieWidgetInstance->FindFunction(FuncName);
	if (!Func)
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadingPC::ApplyPreTravelSkipVoteToWidget - ApplySkipVoteStatus function not found on widget."));
		return;
	}

	struct FApplySkipVoteStatusParams
	{
		int32 VotedCount;
		int32 TotalCount;
	};

	FApplySkipVoteStatusParams Params;
	Params.VotedCount = VotedCount;
	Params.TotalCount = TotalCount;

	PreTravelMovieWidgetInstance->ProcessEvent(Func, &Params);
}

// ============================================================================
// ALoadingPlayerControllerBase::MarkPreTravelWidgetLocalVoted
// ----------------------------------------------------------------------------
// 역할: 현재 로컬 플레이어가 이미 Skip/완료 Vote했다는 상태를 WBP에 표시한다.
// ============================================================================
void ALoadingPlayerControllerBase::MarkPreTravelWidgetLocalVoted()
{
	if (!PreTravelMovieWidgetInstance)
	{
		return;
	}

	static const FName FuncName(TEXT("MarkLocalSkipVoted"));

	UFunction* Func = PreTravelMovieWidgetInstance->FindFunction(FuncName);
	if (!Func)
	{
		return;
	}

	PreTravelMovieWidgetInstance->ProcessEvent(Func, nullptr);
}

// =========================
// Session
// =========================

// ============================================================================
// ALoadingPlayerControllerBase::Server_RequestCloseRoom_Implementation
// ----------------------------------------------------------------------------
// 역할: 방장이 Session 종료를 요청하면 서버 측 Close Room 흐름을 실행한다.
// 실행 위치/권한: Client/Host 요청 → Server
// ============================================================================
void ALoadingPlayerControllerBase::Server_RequestCloseRoom_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		if (ALoadingPlayerControllerBase* LoadingPC = Cast<ALoadingPlayerControllerBase>(PC))
		{
			LoadingPC->Client_ForceBackToLobby();
		}
	}
}

// ============================================================================
// ALoadingPlayerControllerBase::Client_ForceBackToLobby_Implementation
// ----------------------------------------------------------------------------
// 역할: 로딩 중 플레이어 이탈 등 강제 복귀 시 Movie/Loading/Input을 즉시 정리하고 MainMenu/Lobby 복귀를 실행한다.
// 실행 위치/권한: Server → Owning Client 또는 로컬 안전망
// ============================================================================
void ALoadingPlayerControllerBase::Client_ForceBackToLobby_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	// 시네마틱이 떠있으면 먼저 정리.
	StopPreTravelMovieLocal(true);

	// 여기서 먼저 입력 잠금/커서 상태를 정리해준다.
	ExitLoadingInputMode();

	if (UGI_Steam* GI = GetGameInstance<UGI_Steam>())
	{
		// 상대가 나간 상황에서는 단계 타이머를 기다리지 않고 로딩창을 즉시 정리한다.
		// AbortLoadingImmediately() 내부에서 ExitLoadingInputMode()가 호출되어
		// 마우스 커서가 다시 숨겨질 수 있으므로, 그 다음에 로비용 마우스 상태를 적용한다.
		GI->AbortLoadingImmediately();

		FInputModeGameAndUI LobbyInputMode;
		LobbyInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		LobbyInputMode.SetHideCursorDuringCapture(false);
		SetInputMode(LobbyInputMode);

		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		if (PlayerInput)
		{
			PlayerInput->FlushPressedKeys();
		}

		GI->SetPendingMainMenuNotice(FText::FromString(TEXT("플레이어가 나가서 로비로 돌아왔습니다.")));
		GI->BP_BackToLobby();
	}
}