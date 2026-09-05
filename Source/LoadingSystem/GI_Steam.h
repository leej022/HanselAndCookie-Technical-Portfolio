#pragma once

#include "CoreMinimal.h"
#include "AdvancedFriendsGameInstance.h"
#include "Styling/SlateBrush.h"
#include "Engine/EngineBaseTypes.h"
#include "GI_Steam.generated.h"

class UUserWidget;
class UTexture2D;
class ULoadingScreenConfigAsset;
class USoundClass;
class USoundMix;
class UMediaSource;
class ALoadingPlayerControllerBase;
class UNetDriver;
class UWorld;


/*
================================================================================
GI_Steam.h - 주석 보강본 (기존 선언/로직 변경 없음)
================================================================================
핵심 책임
- GameInstance Lifetime을 이용해 World 교체와 독립적으로 Loading State 유지
- SeamlessTravel의 Current → Transition → Target World Lifecycle 감지
- Loading Stage 1/2/3 상태 머신 관리
- TargetMap별 Loading DataAsset Config 관리
- 2인 TargetMap Arrival Barrier 관리
- PreTravel Cinematic 참가자/Skip Vote/ServerTravel 상태 관리
- Loading/Movie 중 Disconnect/NetworkFailure 안전 처리
- Audio Volume 및 Cinematic Music/SFX Mute 관리

주의
GameInstance는 서버/클라이언트가 공유하는 Replicated 객체가 아니다.
각 프로세스에 자기 GI가 있으며, 서버 판정이 필요한 정보는 PlayerController RPC를 통해 전달된다.
================================================================================
*/
UENUM(BlueprintType)
enum class ELoadingStage : uint8
{
	Traveling        UMETA(DisplayName = "Traveling"),
	LoadingMap       UMETA(DisplayName = "LoadingMap"),
	EnteringPlayers  UMETA(DisplayName = "EnteringPlayers"),
};

UCLASS()
class STEAMDEVELOPMENT_API UGI_Steam : public UAdvancedFriendsGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Input", meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float MouseSensitivity = 0.35f;

	// =========================
	// Audio Settings
	// 저장 안 함.
	// 게임 실행 중에는 GI에서 유지되고,
	// 게임을 완전히 끄면 기본값으로 초기화됨.
	// =========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundMix> UserVolumeSoundMix = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> MasterSoundClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> MusicSoundClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> SFXSoundClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MusicVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SFXVolume = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMasterVolume(float InVolume);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMusicVolume(float InVolume);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetSFXVolume(float InVolume);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void ApplyAudioSettings(float FadeTime = 0.05f);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void ResetAudioSettingsToDefault();

	// 시네마틱 영상 재생 중에는 Music/SFX만 0으로 내림.
	// Master는 건드리지 않는다. 그래야 영상 소리가 죽지 않음.
	UFUNCTION(BlueprintCallable, Category = "PreTravel Movie|Audio")
	void BeginPreTravelCinematicAudioMute(float FadeTime = 0.05f);

	UFUNCTION(BlueprintCallable, Category = "PreTravel Movie|Audio")
	void EndPreTravelCinematicAudioMute(float FadeTime = 0.05f);

	// 로컬에서 PreTravel 영상이 켜져 있는지 GI에 알려준다.
	// 호스트가 영상 중 강제 종료됐을 때 NetworkFailure로 로비 복귀하기 위해 사용.
	void SetLocalPreTravelMovieActive(bool bActive);

	UPROPERTY(BlueprintReadOnly, Category = "PhotoPieces")
	int32 PhotoPieceCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PhotoPieces", meta = (ClampMin = "1"))
	int32 TotalPieces = 5;

	virtual void Init() override;
	virtual void Shutdown() override;

	// Seamless Travel에서 OldWorld -> TransitionMap -> FinalMap으로
	// 실제 World가 교체되는 순간을 직접 감지한다.
	virtual void OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld) override;

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void BeginLoadingForMapURL(const FString& MapURL);

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void EndLoading();

	// 플레이어 이탈처럼 정상 로딩 종료를 기다릴 수 없는 경우 즉시 정리.
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void AbortLoadingImmediately();

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void RefreshLoadingOverlay();

	// PlayerController의 Server RPC가 서버 GI로 전달하는 최종 맵 진입 보고.
	void ServerRegisterReachedTargetLoadingMap(ALoadingPlayerControllerBase* ReportingPC, FName LoadedMapName);

	// 서버가 두 명 모두 도착했다고 승인했을 때 각 로컬 GI에 적용.
	void ConfirmAllPlayersReachedTargetLoadingMap(FName LoadedMapName);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Session")
	void BP_BackToLobby();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void SetPendingMainMenuNotice(const FText& InMessage);

	UFUNCTION(BlueprintCallable, Category = "Session")
	bool ConsumePendingMainMenuNotice(FText& OutMessage);

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bHasPendingMainMenuNotice = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FText PendingMainMenuNotice;

	UPROPERTY(BlueprintReadOnly, Category = "Loading")
	bool bLoadingOn = false;

	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	ULoadingScreenConfigAsset* LoadingConfigAsset = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	TSubclassOf<UUserWidget> LoadingOverlayClass;

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Stage")
	FText StageText_Traveling = FText::FromString(TEXT("맵 이동중입니다."));

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Stage")
	FText StageText_LoadingMap = FText::FromString(TEXT("맵 생성중입니다."));

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Stage")
	FText StageText_EnteringPlayers = FText::FromString(TEXT("캐릭터 입장중입니다."));

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Stage", meta = (ClampMin = "0.0"))
	float MinStageDisplaySeconds = 2.0f;

	// =========================
	// 로딩/영상 중 2인 연결 감시
	// =========================

	// 네 게임은 무조건 2인이므로 기본 2.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading|Disconnect", meta = (ClampMin = "2"))
	int32 RequiredPlayersDuringLoading = 2;

	// 영상 재생 중 실제 이탈을 판정하기 전에 잠깐 기다리는 시간.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading|Disconnect", meta = (ClampMin = "0.0"))
	float LoadingDisconnectGraceSeconds = 1.0f;

	// Transition Map/Seamless Travel 중 일시적으로 한 명처럼 보이는 시간을 허용한다.
	// 두 명이 모두 최종 맵 로딩 UMG에 진입하면 즉시 다음 단계로 진행하고,
	// 한 명이 끝까지 오지 않으면 이 시간이 지난 뒤 실제 이탈로 처리한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading|Disconnect", meta = (ClampMin = "1.0"))
	float TransitionMapPlayerWaitSeconds = 10.0f;

	// =========================
	// PreTravel Cinematic Server State
	// =========================

	// 서버에서 호출됨.
	// CinematicMediaSource가 nullptr이면 영상 없이 바로 기존 로딩화면 + ServerTravel.
	void BeginPreTravelCinematicAsServer(const FString& MapURL, UMediaSource* CinematicMediaSource, ALoadingPlayerControllerBase* RequesterPC);

	// 클라가 스킵 버튼을 누르거나 영상이 끝났을 때 서버에서 호출됨.
	void ServerRegisterPreTravelSkipVote(ALoadingPlayerControllerBase* VotingPC);

	void CancelPreTravelCinematicAsServer();

private:
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	static FName ExtractMapShortName(const FString& MapURL);

	void ShowOverlayUMG();
	void HideOverlayUMG();

	bool TickLoading(float DeltaSeconds);

	void ApplyStageNow(ELoadingStage NewStage);
	void RequestStage(ELoadingStage NewStage);
	void TryAdvanceStage();
	void TryAutoAdvanceFromLoadingMap();
	void FinalizeEndIfReady();
	void ApplyStageToWidget();
	void ApplyLoadingInputToLocalPlayers(bool bEnable);

	// Transition Map을 지나 최종 목적지 맵에 두 명 모두 진입할 때까지 2단계에서 대기.
	void TryReportLocalReachedTargetLoadingMap();
	void BroadcastAllPlayersReachedTargetLoadingMap();
	bool IsTargetMapWorld(const UWorld* World) const;

	// 로딩/영상 중 연결 끊김 처리
	void TickLoadingDisconnectGuard();
	void ResetLoadingDisconnectGuard();
	int32 CountServerLoadingPlayers() const;
	void ForceAllPlayersBackToLobbyBecauseLoadingPlayerLost();
	void ForceLocalBackToLobbyBecauseLoadingPlayerLost();
	bool IsLocalLoadingOrMovieActive() const;

	void HandleEngineNetworkFailure(
		UWorld* InWorld,
		UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType,
		const FString& ErrorString);

	// PreTravel Cinematic 내부 처리
	void GatherPreTravelParticipants();
	void BroadcastPreTravelSkipVoteStatus();
	void StartPreTravelServerTravel();
	void ExecutePreTravelServerTravel();
	void SetGameModeIgnoreLogoutDuringTravel(bool bIgnore);

	int32 GetValidPreTravelParticipantCount() const;
	int32 GetPreTravelVoteCount() const;
	bool IsPreTravelParticipant(ALoadingPlayerControllerBase* PC) const;
	bool HasPreTravelVoted(ALoadingPlayerControllerBase* PC) const;

private:
	UPROPERTY()
	UUserWidget* LoadingOverlay = nullptr;

	UPROPERTY()
	FName TargetMapName = NAME_None;

	UPROPERTY()
	FText CurrentTitle;

	UPROPERTY()
	FText CurrentDesc;

	UPROPERTY()
	UTexture2D* CurrentImage = nullptr;

	FSlateBrush LoadingBrush;

	UPROPERTY()
	ELoadingStage CurrentStage = ELoadingStage::Traveling;

	UPROPERTY()
	FText CurrentStageText;

	double StageHoldUntilSeconds = 0.0;
	TArray<ELoadingStage> StageQueue;
	bool bEndRequested = false;
	bool bAutoAdvanceToEnteringPlayers = false;

	/*
	 * true일 때 현재 World는 Seamless Travel의 TransitionMap이다.
	 * 이 구간에서는 최종맵 로딩을 위해 PreLoadMap이 다시 불려도
	 * MoviePlayer로 WBP를 덮지 않고 Stage 1 WBP를 그대로 유지한다.
	 */
	bool bInsideSeamlessTransitionMap = false;

	FTSTicker::FDelegateHandle LoadingTickerHandle;

	bool bAudioMixPushed = false;

	bool bPreTravelCinematicAudioMuted = false;
	bool bLocalPreTravelMovieActive = false;

	// 엔진 네트워크 실패 감지 핸들
	FDelegateHandle NetworkFailureDelegateHandle;

	// 서버 로딩 인원 감시 상태
	bool bLoadingDisconnectGuardArmed = false;
	double LoadingPlayerMissingSinceSeconds = -1.0;
	bool bHandlingLoadingPlayerLoss = false;

	// 최종 목적지 맵 도착 동기화 상태.
	bool bLocalReachedTargetLoadingMap = false;
	bool bAllPlayersReachedTargetLoadingMap = false;
	double NextTargetMapArrivalReportSeconds = 0.0;
	double FirstTargetMapArrivalSinceSeconds = -1.0;

	TArray<TWeakObjectPtr<ALoadingPlayerControllerBase>> TargetMapReachedPlayers;

	// PreTravel Cinematic 서버 상태
	FString PendingPreTravelMapURL;

	TWeakObjectPtr<UMediaSource> PendingPreTravelMediaSource;

	TArray<TWeakObjectPtr<ALoadingPlayerControllerBase>> PreTravelParticipants;
	TArray<TWeakObjectPtr<ALoadingPlayerControllerBase>> PreTravelVotedPlayers;

	bool bPreTravelCinematicInProgress = false;
	bool bPreTravelServerTravelStarted = false;

	FTimerHandle PreTravelServerTravelTimerHandle;
};