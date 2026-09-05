#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "LoadingPlayerControllerBase.generated.h"

class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputUserSettings;
class UUserWidget;
class UWarningStateWidgetBase;
class USoundBase;
class UMediaPlayer;
class UMediaSource;

/*
================================================================================
ALoadingPlayerControllerBase
================================================================================
공통 PlayerController Base로서 다음 역할을 담당한다.

[네트워크]
- Server → Owning Client : Movie/Loading/입력복구/도착완료 통지
- Owning Client → Server : Skip Vote / TargetMap 도착 보고

[로컬 표현]
- PreTravel Movie Widget / MediaPlayer / Movie SoundActor 관리
- Persistent Gameplay UI / Warning UI 관리

[입력]
- Loading 중 UIOnly + Mouse 활성화
- Move/Look 및 ProcessPlayerInput 차단
- Gameplay 복귀 시 GameOnly + EnhancedInput MappingContext 복구

실제 Travel State(TargetMap, Stage, Arrival Players 등)는 GI_Steam에서 관리하며,
이 클래스는 플레이어 Ownership에 기반한 RPC Gateway + Local UI/Input 담당자이다.
================================================================================
*/
UCLASS(BlueprintType)
class STEAMDEVELOPMENT_API ALoadingPlayerControllerBase : public APlayerController
{
	GENERATED_BODY()

public:
	// Controller가 현재 World에 진입할 때 Local UI/Input을 준비하고,
	// GI가 이미 Loading 중이면 Overlay를 다시 복구한다.
	virtual void BeginPlay() override;

	// Controller가 World에서 빠질 때 Movie/UI/Timer 등 로컬 리소스를 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// bLoadingInputLocked=true 동안 Gameplay 입력 처리 자체를 막는다.
	virtual void ProcessPlayerInput(const float DeltaTime, const bool bGamePaused) override;

	/**
	 * 현재 레벨의 Move 입력을 사다리용 Horizontal / Vertical 값으로 변환한다.
	 *
	 * 일반 3인칭 레벨의 기본 입력:
	 * - MovementVector.X = A / D
	 * - MovementVector.Y = S / W
	 *
	 * 따라서 기본값은:
	 * - Horizontal = X
	 * - Vertical   = Y
	 *
	 * 입력축 구조가 다른 레벨의 자식 PlayerController는
	 * 이 함수를 Override해서 해당 레벨에 맞는 값으로 변환한다.
	 */
	virtual FVector2D ConvertMoveInputToClimbInput(
		const FVector2D& MovementVector) const
	{
		return FVector2D(
			MovementVector.X,
			MovementVector.Y);
	}

	// ====== 로딩 / 세션 기존 ======
	// [Server → Owning Client] 해당 MapURL 기준 Local GI Loading 시작.
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "Loading")
	void Client_BeginLoadingForMapURL(const FString& MapURL);

	// [Server → Owning Client] Local GI에 Loading 종료 요청.
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "Loading")
	void Client_EndLoading();

	// 최종 목적지 맵의 로딩 UMG에 진입했다고 서버에 알린다.
	// [Owning Client → Server] Server GI의 TargetMapReachedPlayers에 도착 보고.
	UFUNCTION(Server, Reliable, Category = "Loading|Transition Wait")
	void Server_ReportReachedTargetLoadingMap(FName LoadedMapName);

	// 서버가 두 플레이어 모두 최종 목적지 맵에 진입한 것을 확인한 뒤 양쪽에 전달한다.
	// [Server → Owning Client] 각 Local GI에 bAllPlayersReachedTargetLoadingMap=true 적용.
	UFUNCTION(Client, Reliable, Category = "Loading|Transition Wait")
	void Client_ConfirmAllPlayersReachedTargetLoadingMap(FName LoadedMapName);

	// [Owning Client → Server] 방 종료 요청.
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Session")
	void Server_RequestCloseRoom();

	// [Server → Owning Client] Movie/Loading을 즉시 정리하고 Lobby/MainMenu로 복귀.
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "Session")
	void Client_ForceBackToLobby();

	// ====== 첫 게임맵 입력 복구용 ======
	// Spawn/Possess 완료 뒤 Local Gameplay 입력과 MappingContext를 복구한다.
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "Loading")
	void Client_RestoreGameplayInput();

	// Travel 이후 다시 보장할 기본 Gameplay Input Mapping Context.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Context")
	TObjectPtr<UInputMappingContext> DefaultGameplayMappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Context")
	int32 DefaultGameplayMappingPriority = 0;

	// ====== 로딩 중 입력 잠금 ======
	// Local PC를 UIOnly/Mouse ON/Move&Look Ignore 상태로 만든다.
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void EnterLoadingInputMode(UUserWidget* FocusWidget);

	// Local PC를 GameOnly/Mouse OFF/Move&Look Restore 상태로 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void ExitLoadingInputMode();

	// =========================
	// PreTravel Cinematic
	// =========================

	// TravelPoint BP에서 서버 권한일 때 이 함수를 호출하면 됨.
	// CinematicMediaSource가 None이면 영상 없이 바로 기존 로딩화면 + ServerTravel.
	// 실제 참가자/Vote/Travel State 관리는 GI_Steam으로 전달한다.
	UFUNCTION(BlueprintCallable, Category = "PreTravel Movie")
	void BeginPreTravelCinematicAsServer(const FString& MapURL, UMediaSource* CinematicMediaSource);

	// WBP_PreTravelMovie의 스킵 버튼에서 호출할 함수.
	// [Owning Client → Server] 서버 GI에 Skip Vote를 등록한다.
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "PreTravel Movie")
	void Server_RequestPreTravelSkipVote();

	// [Server → Owning Client] 각 Client 로컬에서 Movie Widget/SoundActor/MediaPlayer를 실행한다.
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "PreTravel Movie")
	void Client_PlayPreTravelMovie(const FString& MapURL, UMediaSource* CinematicMediaSource, int32 TotalCount);

	// [Server → Owning Client] 현재 Vote 수를 PreTravel WBP에 갱신한다.
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "PreTravel Movie")
	void Client_UpdatePreTravelSkipVote(int32 VotedCount, int32 TotalCount);

	// [Server → Owning Client] Movie를 내리고 ServerTravel 전에 Loading WBP를 먼저 시작한다.
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "PreTravel Movie")
	void Client_StopPreTravelMovieAndBeginLoading(const FString& MapURL);

	// PreTravel Movie 화면 클래스.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreTravel Movie")
	TSubclassOf<UUserWidget> PreTravelMovieWidgetClass;

	// MediaPlayer Audio를 각 Client Local World에서 재생하기 위한 SoundActor 클래스.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreTravel Movie")
	TSubclassOf<AActor> PreTravelMovieSoundActorClass;

	// 실제 영상 재생 MediaPlayer.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreTravel Movie")
	TObjectPtr<UMediaPlayer> PreTravelMediaPlayer = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreTravel Movie")
	int32 PreTravelMovieWidgetZOrder = 10000;

	// ====== 입력 리맵용 ======
	// 옵션 UI에서 사용할 MappingContext들을 UserSettings에 등록한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Rebind")
	TArray<TObjectPtr<UInputMappingContext>> ContextsToRegisterForSettings;

	UFUNCTION(BlueprintCallable, Category = "Input|Rebind")
	void RegisterKnownContexts();

	UFUNCTION(BlueprintCallable, Category = "Input|Rebind")
	bool RegisterContextForSettings(UInputMappingContext* MappingContext);

	UFUNCTION(BlueprintCallable, Category = "Input|Context")
	bool AddGameplayMappingContext(UInputMappingContext* MappingContext, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Input|Context")
	bool RemoveGameplayMappingContext(UInputMappingContext* MappingContext);

	UFUNCTION(BlueprintCallable, Category = "Input|Rebind")
	bool RebindPlayerKey(FName MappingName, FKey NewKey);

	UFUNCTION(BlueprintCallable, Category = "Input|Rebind")
	bool ResetPlayerKey(FName MappingName);

	UFUNCTION(BlueprintPure, Category = "Input|Rebind")
	bool GetCurrentPlayerKey(FName MappingName, FKey& OutKey) const;

	// ====== 항상 띄울 UI / 크로스헤어용 ======
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Persistent")
	TSubclassOf<UUserWidget> PersistentGameplayWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Persistent")
	bool bAutoCreatePersistentGameplayWidget = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Persistent")
	int32 PersistentGameplayWidgetZOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Persistent")
	TObjectPtr<UUserWidget> PersistentGameplayWidgetInstance = nullptr;

	UFUNCTION(BlueprintCallable, Category = "UI|Persistent")
	UUserWidget* CreatePersistentGameplayWidget();

	UFUNCTION(BlueprintCallable, Category = "UI|Persistent")
	void RemovePersistentGameplayWidget();

	UFUNCTION(BlueprintCallable, Category = "UI|Persistent")
	void SetPersistentGameplayWidgetVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "UI|Persistent")
	UUserWidget* GetPersistentGameplayWidget() const { return PersistentGameplayWidgetInstance; }

	// ====== WarningState UI ======
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|WarningState")
	TSubclassOf<UWarningStateWidgetBase> WarningStateWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|WarningState")
	bool bAutoCreateWarningStateWidget = true;

	// 경고 UI는 기본적으로 아래쪽에 깔리게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|WarningState")
	int32 WarningStateWidgetZOrder = -100;

	// 총 표시 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|WarningState")
	float DefaultWarningStateDuration = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UI|WarningState")
	TObjectPtr<UWarningStateWidgetBase> WarningStateWidgetInstance = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "UI|WarningState")
	FText CurrentWarningStateText;

	UPROPERTY(BlueprintReadOnly, Category = "UI|WarningState")
	bool bWarningStateVisible = false;

	UFUNCTION(BlueprintCallable, Category = "UI|WarningState")
	UWarningStateWidgetBase* CreateWarningStateWidget();

	UFUNCTION(BlueprintCallable, Category = "UI|WarningState")
	void RemoveWarningStateWidget();

	UFUNCTION(BlueprintCallable, Category = "UI|WarningState")
	void SetWarningStateWidgetVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "UI|WarningState")
	void ShowWarningStateLocal(const FText& InText, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "UI|WarningState")
	void HideWarningStateLocal();

	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "UI|WarningState")
	void Client_ShowWarningState(const FText& InText, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "UI|WarningState")
	void Client_HideWarningState();

	// WarningState 알림 사운드 로컬 재생용
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "UI|WarningState")
	void Client_PlayWarningStateSFX(USoundBase* InSound);

	UFUNCTION(BlueprintPure, Category = "UI|WarningState")
	UWarningStateWidgetBase* GetWarningStateWidget() const { return WarningStateWidgetInstance; }

protected:
	// LocalPlayer의 Enhanced Input Subsystem 조회 헬퍼.
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

	// LocalPlayer의 사용자 키 설정 객체 조회 헬퍼.
	UEnhancedInputUserSettings* GetEnhancedInputUserSettings() const;

	void BeginHideWarningStateLocal();

	// 로딩 중엔 게임 입력 처리 자체를 끊기 위한 플래그
	UPROPERTY(Transient)
	bool bLoadingInputLocked = false;

	FTimerHandle WarningStateBeginHideTimerHandle;
	FTimerHandle WarningStateFinishHideTimerHandle;

private:
	// MediaPlayer 영상이 자연 종료됐을 때 Server Skip/완료 Vote를 1회 보낸다.
	UFUNCTION()
	void HandlePreTravelMovieEndReached();

	// MoviePlayer/Widget/SoundActor를 로컬에서 정리한다.
	void StopPreTravelMovieLocal(bool bRestoreAudioImmediately);

	// PreTravel WBP에 현재 LoadingPC를 OwnerLoadingPC 프로퍼티로 주입한다.
	void SetOwnerLoadingPCOnPreTravelWidget();

	// 현재 Skip Vote 수를 WBP에 반영한다.
	void ApplyPreTravelSkipVoteToWidget(int32 VotedCount, int32 TotalCount);

	// 내가 이미 Skip/영상종료 Vote를 보냈음을 WBP에 표시한다.
	void MarkPreTravelWidgetLocalVoted();

private:
	// 현재 Local PreTravel Movie Widget 인스턴스.
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PreTravelMovieWidgetInstance = nullptr;

	// 현재 Client Local World에 Spawn한 Movie SoundActor.
	UPROPERTY(Transient)
	TObjectPtr<AActor> PreTravelMovieSoundActorInstance = nullptr;

	// 현재 재생 중인 MediaSource.
	UPROPERTY(Transient)
	TObjectPtr<UMediaSource> CurrentPreTravelMediaSource = nullptr;

	// 현재 Movie가 끝나면 이동할 Map URL.
	UPROPERTY(Transient)
	FString CurrentPreTravelMapURL;

	// 동일 Client의 Skip/OnEndReached 중복 Vote 방지 플래그.
	bool bPreTravelLocalFinishedOrVoted = false;
};
