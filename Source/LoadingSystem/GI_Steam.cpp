#include "GI_Steam.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Texture2D.h"
#include "MoviePlayer.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "LoadingPlayerControllerBase.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/UnrealType.h"
#include "MediaSource.h"
#include "GameFramework/GameUserSettings.h"

// Audio
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

// Slate
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"

#include "LoadingScreenConfigAsset.h"
#include "LoadingOverlayWidgetBase.h"

/*
================================================================================
GI_Steam.cpp - 주석 보강본 (기존 로직 변경 없음)
================================================================================
이 GameInstance의 로딩 시스템 역할

1) World와 독립적으로 Travel 상태를 보관
   - bLoadingOn
   - TargetMapName
   - CurrentImage / CurrentTitle / CurrentDesc
   - CurrentStage

2) SeamlessTravel의 실제 World 교체 감지
   - HandlePreLoadMap()
   - OnWorldChanged()
   - HandlePostLoadMap()

3) Current Map -> Transition Map -> Target Map 사이에서
   Loading WBP가 끊겨 보이지 않도록 같은 설정의 WBP를 World마다 재생성

4) 2인 멀티 동기화
   - 각 로컬 Client가 TargetMap 도착을 Server RPC로 보고
   - Server가 2/2 도착을 확인
   - Client RPC로 전체 도착 완료를 양쪽에 통보

5) PreTravel Cinematic
   - 서버가 참가자 2명을 수집
   - 각 Client에서 영상을 로컬 재생
   - Skip/영상 종료 Vote 2/2가 모이면 Loading UI를 먼저 켠 뒤 ServerTravel

중요:
GI는 네트워크에서 하나를 공유하는 객체가 아니다.
Host 프로세스와 Remote Client 프로세스는 각각 자기 GI를 가진다.
서버 판정이 필요한 데이터는 LoadingPlayerController RPC를 통해 서버 GI로 전달한다.
================================================================================
*/

// ============================================================================
// UGI_Steam::Init
// ----------------------------------------------------------------------------
// 역할: GameInstance 시작 시 Map Load Delegate, NetworkFailure, Loading Ticker를 등록하고 기본 설정을 적용한다.
// 실행 위치/권한: 각 실행 프로세스의 GI 초기화 시 1회
// 데이터 흐름: Engine Delegate/Ticker 등록 → 이후 World/Loading Lifecycle을 지속 감시
// 다음 연결: HandlePreLoadMap / HandlePostLoadMap / TickLoading / HandleEngineNetworkFailure
// ============================================================================
void UGI_Steam::Init()
{
	Super::Init();

	// 게임 전체 프레임 제한을 60 FPS로 고정
	if (GEngine)
	{
		if (UGameUserSettings* GameUserSettings = GEngine->GetGameUserSettings())
		{
			GameUserSettings->SetFrameRateLimit(60.0f);
			GameUserSettings->ApplyNonResolutionSettings();
		}

		GEngine->SetMaxFPS(60.0f);
	}

	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UGI_Steam::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UGI_Steam::HandlePostLoadMap);

	// 호스트 프로세스가 꺼지거나 서버 연결이 끊어진 경우 클라이언트가 직접 감지한다.
	if (GEngine && !NetworkFailureDelegateHandle.IsValid())
	{
		NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(
			this,
			&UGI_Steam::HandleEngineNetworkFailure
		);
	}

	if (!LoadingTickerHandle.IsValid())
	{
		LoadingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UGI_Steam::TickLoading),
			0.0f
		);
	}

	// 저장 없이 기본값 적용.
	// MasterVolume / MusicVolume / SFXVolume 기본값은 전부 1.0f.
	ApplyAudioSettings(0.0f);
}

// ============================================================================
// UGI_Steam::Shutdown
// ----------------------------------------------------------------------------
// 역할: GI 종료 시 등록했던 NetworkFailure/Ticker와 PreTravel 상태를 정리한다.
// 실행 위치/권한: 각 실행 프로세스 종료 시
// 데이터 흐름: 등록 핸들 제거 → 잔여 PreTravel 상태 취소
// ============================================================================
void UGI_Steam::Shutdown()
{
	CancelPreTravelCinematicAsServer();

	if (GEngine && NetworkFailureDelegateHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
		NetworkFailureDelegateHandle.Reset();
	}

	if (LoadingTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingTickerHandle);
		LoadingTickerHandle.Reset();
	}

	Super::Shutdown();
}

// ============================================================================
// UGI_Steam::TickLoading
// ----------------------------------------------------------------------------
// 역할: 로딩 상태 머신의 반복 처리와 TargetMap 도착 보고 재시도, 2인 이탈 감시를 수행한다.
// 실행 위치/권한: 각 프로세스 CoreTicker
// 데이터 흐름: 도착 보고 재시도 + 이탈 감시 + Stage 전진 + 종료 조건 검사
// ============================================================================
bool UGI_Steam::TickLoading(float DeltaSeconds)
{
	// 최종 맵을 로컬에서 연 뒤 서버 승인을 받을 때까지 도착 보고를 재전송한다.
	TryReportLocalReachedTargetLoadingMap();

	// 로딩/시네마틱 중에는 Listen Server가 현재 PlayerController 수를 계속 확인한다.
	TickLoadingDisconnectGuard();

	if (!bLoadingOn && !bEndRequested)
	{
		return true;
	}

	TryAdvanceStage();
	TryAutoAdvanceFromLoadingMap();
	FinalizeEndIfReady();
	return true;
}

// ============================================================================
// UGI_Steam::ExtractMapShortName
// ----------------------------------------------------------------------------
// 역할: Travel URL에서 실제 맵 Short Name만 추출한다.
// 데이터 흐름: MapURL → ?옵션 제거 → Package ShortName → TargetMapName
// ============================================================================
FName UGI_Steam::ExtractMapShortName(const FString& MapURL)
{
	FString Clean = MapURL;

	int32 QPos = INDEX_NONE;
	if (Clean.FindChar(TEXT('?'), QPos))
	{
		Clean = Clean.Left(QPos);
	}

	Clean = FPackageName::GetShortName(Clean);
	return FName(*Clean);
}

// ============================================================================
// UGI_Steam::BeginLoadingForMapURL
// ----------------------------------------------------------------------------
// 역할: 현재 프로세스의 실제 Loading 상태를 시작하고 목적지 맵별 UI 데이터를 준비한다.
// 실행 위치/권한: 각 Client 로컬 GI
// 데이터 흐름: MapURL → TargetMapName → DataAsset Config → CurrentImage/Title/Desc → Stage Traveling → WBP 표시
// 다음 연결: ServerTravel 이후 HandlePreLoadMap / OnWorldChanged
// ============================================================================
void UGI_Steam::BeginLoadingForMapURL(const FString& MapURL)
{
	if (IsRunningDedicatedServer()) return;

	// 새 로딩이 시작되면 이전 이탈 처리 잠금을 해제한다.
	bHandlingLoadingPlayerLoss = false;

	bLoadingOn = true;
	bEndRequested = false;
	bAutoAdvanceToEnteringPlayers = false;
	bInsideSeamlessTransitionMap = false;
	StageQueue.Reset();

	// 새 Travel마다 최종 맵 도착 보고 상태를 초기화한다.
	bLocalReachedTargetLoadingMap = false;
	bAllPlayersReachedTargetLoadingMap = false;
	NextTargetMapArrivalReportSeconds = 0.0;
	FirstTargetMapArrivalSinceSeconds = -1.0;
	TargetMapReachedPlayers.Reset();

	TargetMapName = ExtractMapShortName(MapURL);

	FLoadingScreenConfig Picked;
	if (LoadingConfigAsset)
	{
		LoadingConfigAsset->GetConfigForMap(TargetMapName, Picked);
	}
	else
	{
		Picked = FLoadingScreenConfig();
	}

	CurrentImage = Picked.Image.IsNull() ? nullptr : Picked.Image.LoadSynchronous();
	CurrentTitle = Picked.Title;
	CurrentDesc = Picked.Description;

	ApplyStageNow(ELoadingStage::Traveling);
	ShowOverlayUMG();

	// Listen Server에서 이미 두 명이 존재하면 이 로딩의 연결 감시를 즉시 활성화한다.
	if (UWorld* World = GetWorld())
	{
		if (World->GetAuthGameMode() && CountServerLoadingPlayers() >= RequiredPlayersDuringLoading)
		{
			bLoadingDisconnectGuardArmed = true;
			LoadingPlayerMissingSinceSeconds = -1.0;
		}
	}
}

// ============================================================================
// UGI_Steam::EndLoading
// ----------------------------------------------------------------------------
// 역할: 로딩을 즉시 닫지 않고 종료 요청 플래그를 세운 뒤 종료 가능 여부를 검사한다.
// 실행 위치/권한: 각 Client 로컬 GI
// 데이터 흐름: bEndRequested=true → FinalizeEndIfReady
// ============================================================================
void UGI_Steam::EndLoading()
{
	if (IsRunningDedicatedServer()) return;

	bEndRequested = true;
	FinalizeEndIfReady();
}

// ============================================================================
// UGI_Steam::AbortLoadingImmediately
// ----------------------------------------------------------------------------
// 역할: 플레이어 이탈/오류처럼 정상 Stage 종료를 기다릴 수 없을 때 Loading을 즉시 정리한다.
// 실행 위치/권한: 각 Client 로컬 GI
// 데이터 흐름: Loading/Movie/입력/도착 상태 강제 Reset
// ============================================================================
void UGI_Steam::AbortLoadingImmediately()
{
	bEndRequested = false;
	bLoadingOn = false;
	bAutoAdvanceToEnteringPlayers = false;
	bInsideSeamlessTransitionMap = false;
	StageQueue.Reset();

	if (GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
	}

	HideOverlayUMG();
	EndPreTravelCinematicAudioMute(0.05f);
	ResetLoadingDisconnectGuard();

	bLocalReachedTargetLoadingMap = false;
	bAllPlayersReachedTargetLoadingMap = false;
	NextTargetMapArrivalReportSeconds = 0.0;
	FirstTargetMapArrivalSinceSeconds = -1.0;
	TargetMapReachedPlayers.Reset();
}

// ============================================================================
// UGI_Steam::SetLocalPreTravelMovieActive
// ----------------------------------------------------------------------------
// 역할: 현재 로컬 프로세스가 PreTravel 영상을 재생 중인지 GI에 기록한다.
// 실행 위치/권한: 각 Client 로컬
// 데이터 흐름: LoadingPC Movie 상태 → GI bLocalPreTravelMovieActive
// ============================================================================
void UGI_Steam::SetLocalPreTravelMovieActive(bool bActive)
{
	bLocalPreTravelMovieActive = bActive;
}

// ============================================================================
// UGI_Steam::RefreshLoadingOverlay
// ----------------------------------------------------------------------------
// 역할: 새 World/새 LocalController가 생겼는데 Loading 상태가 계속될 때 WBP와 입력 잠금을 다시 보장한다.
// 실행 위치/권한: 각 Client 로컬 GI
// 데이터 흐름: bLoadingOn 확인 → ShowOverlayUMG → UIOnly 입력 적용
// ============================================================================
void UGI_Steam::RefreshLoadingOverlay()
{
	if (IsRunningDedicatedServer()) return;
	if (!bLoadingOn) return;

	ShowOverlayUMG();
	ApplyLoadingInputToLocalPlayers(true);
}

// ============================================================================
// UGI_Steam::SetPendingMainMenuNotice
// ----------------------------------------------------------------------------
// 역할: 로딩 중 이탈 등으로 메인 메뉴 복귀 후 보여줄 안내 문구를 GI에 임시 저장한다.
// ============================================================================
void UGI_Steam::SetPendingMainMenuNotice(const FText& InMessage)
{
	PendingMainMenuNotice = InMessage;
	bHasPendingMainMenuNotice = !InMessage.IsEmpty();
}

// ============================================================================
// UGI_Steam::ConsumePendingMainMenuNotice
// ----------------------------------------------------------------------------
// 역할: 메인 메뉴가 GI에 저장된 복귀 안내 문구를 1회 소비하고 로비용 마우스 입력을 보장한다.
// ============================================================================
bool UGI_Steam::ConsumePendingMainMenuNotice(FText& OutMessage)
{
	if (!bHasPendingMainMenuNotice)
	{
		OutMessage = FText::GetEmpty();
		return false;
	}

	// 메인 메뉴 WBP가 방 손실 알림을 꺼내는 시점에는 새 맵의
	// Local PlayerController가 만들어져 있으므로 여기서도 커서를 확실하게 켠다.
	// ServerTravel 전 PlayerController에서 켠 커서 상태가 맵 이동 중 초기화되는 경우를 막는다.
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* LocalPC = UGameplayStatics::GetPlayerController(World, 0))
		{
			FInputModeGameAndUI LobbyInputMode;
			LobbyInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			LobbyInputMode.SetHideCursorDuringCapture(false);
			LocalPC->SetInputMode(LobbyInputMode);

			LocalPC->bShowMouseCursor = true;
			LocalPC->bEnableClickEvents = true;
			LocalPC->bEnableMouseOverEvents = true;
		}
	}

	OutMessage = PendingMainMenuNotice;
	PendingMainMenuNotice = FText::GetEmpty();
	bHasPendingMainMenuNotice = false;
	return true;
}

// ============================================================================
// UGI_Steam::HandlePreLoadMap
// ----------------------------------------------------------------------------
// 역할: World가 실제 blocking load에 들어가는 순간 UMG 공백을 Slate MoviePlayer 이미지로 가린다.
// 실행 위치/권한: 각 Client 로컬 Engine PreLoadMap Delegate
// 데이터 흐름: CurrentImage → SlateBrush → MoviePlayer
// 다음 연결: OnWorldChanged에서 MoviePlayer를 내리고 새 World WBP 생성
// ============================================================================
void UGI_Steam::HandlePreLoadMap(const FString& MapName)
{
	if (IsRunningDedicatedServer()) return;
	if (!bLoadingOn) return;

	/*
	 * 중요:
	 * 이미 TransitionMap으로 넘어온 뒤에는 최종 목적지 맵이 백그라운드에서
	 * 로딩되는 동안 TransitionMap 위의 WBP_Loading을 계속 보여줘야 한다.
	 *
	 * 여기서 MoviePlayer를 다시 켜 버리면 TransitionMap의 UMG 위를
	 * Slate 로딩 화면이 덮어 버리므로, TransitionMap 구간에서는 건드리지 않는다.
	 */
	if (bInsideSeamlessTransitionMap)
	{
		return;
	}

	LoadingBrush = FSlateBrush();

	if (CurrentImage)
	{
		LoadingBrush.SetResourceObject(CurrentImage);
		LoadingBrush.ImageSize = FVector2D((float)CurrentImage->GetSizeX(), (float)CurrentImage->GetSizeY());
	}

	TSharedRef<SWidget> SlateWidget =
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFill)
				[
					SNew(SImage).Image(&LoadingBrush)
				]
		];

	FLoadingScreenAttributes Attr;
	Attr.bAutoCompleteWhenLoadingCompletes = false;
	Attr.bWaitForManualStop = true;
	Attr.MinimumLoadingScreenDisplayTime = 0.f;
	Attr.WidgetLoadingScreen = SlateWidget;

	GetMoviePlayer()->SetupLoadingScreen(Attr);
	GetMoviePlayer()->PlayMovie();
}

// ============================================================================
// UGI_Steam::OnWorldChanged
// ----------------------------------------------------------------------------
// 역할: SeamlessTravel 중 OldWorld→Transition/Target World 교체를 직접 감지하고 WBP를 새 World용으로 재생성한다.
// 실행 위치/권한: 각 Client 로컬 GameInstance World Lifecycle
// 데이터 흐름: Old WBP 제거 → 새 World가 Target인지 판정 → Transition: Stage1 / Target: Stage2 + 도착 보고
// 다음 연결: Transition에서는 동일 WBP 유지, Target에서는 Server_ReportReachedTargetLoadingMap
// ============================================================================
void UGI_Steam::OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld)
{
	Super::OnWorldChanged(OldWorld, NewWorld);

	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (!bLoadingOn || !NewWorld)
	{
		return;
	}

	/*
	 * ServerTravel / SeamlessTravel에서 World가 교체되면 기존 UMG의
	 * Slate 연결은 이전 World와 함께 내려갈 수 있다.
	 * GameInstance가 포인터를 계속 들고 있더라도 새 World에서는
	 * 같은 인스턴스를 억지로 재사용하지 않고 새로 만든다.
	 */
	if (LoadingOverlay)
	{
		LoadingOverlay->RemoveFromParent();
		LoadingOverlay = nullptr;
	}

	/*
	 * OldWorld -> TransitionMap 전환을 가리던 MoviePlayer 화면을 내리고
	 * 이제부터 TransitionMap 자체 위에 실제 WBP_Loading을 보여준다.
	 */
	if (GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
	}

	const bool bIsTargetWorld = IsTargetMapWorld(NewWorld);

	if (!bIsTargetWorld)
	{
		/*
		 * =========================
		 * TransitionMap
		 * =========================
		 * DataAsset은 BeginLoadingForMapURL()에서 이미 최종 목적지 맵 기준으로
		 * CurrentImage / CurrentTitle / CurrentDesc에 저장되어 있다.
		 *
		 * TransitionMap에서는 무조건 Stage 1만 표시한다.
		 * Stage 2/3 진행이나 플레이어 도착 판정은 절대 시작하지 않는다.
		 */
		bInsideSeamlessTransitionMap = true;
		bAutoAdvanceToEnteringPlayers = false;
		StageQueue.Reset();

		CurrentStage = ELoadingStage::Traveling;
		CurrentStageText = StageText_Traveling;

		ShowOverlayUMG();
		return;
	}

	/*
	 * =========================
	 * Final Target Map
	 * =========================
	 * 최종 목적지 World로 교체된 순간부터 기존 2단계 -> 3단계 로직을 시작한다.
	 */
	bInsideSeamlessTransitionMap = false;

	RequestStage(ELoadingStage::LoadingMap);
	bAutoAdvanceToEnteringPlayers = true;
	ShowOverlayUMG();

	bLocalReachedTargetLoadingMap = true;
	NextTargetMapArrivalReportSeconds = 0.0;
	TryReportLocalReachedTargetLoadingMap();
}

// ============================================================================
// UGI_Steam::HandlePostLoadMap
// ----------------------------------------------------------------------------
// 역할: Map Load 완료 후 MoviePlayer/시네마틱 Audio를 정리하고 Transition/Target별 Loading WBP 상태를 보강한다.
// 실행 위치/권한: 각 Client 로컬 PostLoadMapWithWorld Delegate
// ============================================================================
void UGI_Steam::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (IsRunningDedicatedServer()) return;
	if (!bLoadingOn) return;

	/*
	 * 우리가 BeginLoadingForMapURL()로 시작한 로딩 중일 때만 정리한다.
	 * Startup Movie까지 잘못 끄는 기존 문제를 피한다.
	 */
	if (GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
	}

	// 시네마틱 중 죽였던 Music/SFX 복구. 기존 동작 유지.
	EndPreTravelCinematicAudioMute(0.05f);

	const bool bIsTargetWorld = IsTargetMapWorld(LoadedWorld);

	if (!bIsTargetWorld)
	{
		/*
		 * TransitionMap에서는 Stage 1만 유지한다.
		 * 절대로 LoadingMap(2단계) / EnteringPlayers(3단계)로 진행하지 않는다.
		 */
		bInsideSeamlessTransitionMap = true;
		bAutoAdvanceToEnteringPlayers = false;
		StageQueue.Reset();

		CurrentStage = ELoadingStage::Traveling;
		CurrentStageText = StageText_Traveling;

		ShowOverlayUMG();
		return;
	}

	/*
	 * 실제 최종 목적지 맵이 열린 경우에만 기존 Stage 2부터 진행한다.
	 */
	bInsideSeamlessTransitionMap = false;
	RequestStage(ELoadingStage::LoadingMap);
	bAutoAdvanceToEnteringPlayers = true;
	ShowOverlayUMG();

	bLocalReachedTargetLoadingMap = true;
	NextTargetMapArrivalReportSeconds = 0.0;
	TryReportLocalReachedTargetLoadingMap();
}

// ============================================================================
// UGI_Steam::ShowOverlayUMG
// ----------------------------------------------------------------------------
// 역할: 현재 World에서 유효한 LoadingOverlay를 생성/Viewport 등록하고 GI의 Config와 Stage를 적용한다.
// 실행 위치/권한: 각 Client 로컬
// 데이터 흐름: LoadingOverlayClass → CreateWidget → AddToViewport → BP_ApplyLoadingConfig/Stage → Loading Input Lock
// ============================================================================
void UGI_Steam::ShowOverlayUMG()
{
	if (LoadingOverlayClass)
	{
		/*
		 * Travel 과정에서 Widget UObject 포인터는 GameInstance에 남아 있는데
		 * 실제 Viewport/Slate 연결만 끊긴 상태가 생길 수 있다.
		 * 그런 인스턴스는 재사용하지 않고 현재 World용으로 새로 만든다.
		 */
		if (LoadingOverlay && !LoadingOverlay->IsInViewport())
		{
			LoadingOverlay->RemoveFromParent();
			LoadingOverlay = nullptr;
		}

		if (!LoadingOverlay)
		{
			LoadingOverlay = CreateWidget<UUserWidget>(this, LoadingOverlayClass);
		}

		if (LoadingOverlay && !LoadingOverlay->IsInViewport())
		{
			LoadingOverlay->AddToViewport(9999);
		}

		if (ULoadingOverlayWidgetBase* W = Cast<ULoadingOverlayWidgetBase>(LoadingOverlay))
		{
			// CurrentImage / Title / Desc는 항상 최종 목적지 맵의 DataAsset 설정이다.
			W->BP_ApplyLoadingConfig(CurrentImage, CurrentTitle, CurrentDesc);
			ApplyStageToWidget();
		}
	}

	ApplyLoadingInputToLocalPlayers(true);
}

// ============================================================================
// UGI_Steam::HideOverlayUMG
// ----------------------------------------------------------------------------
// 역할: 현재 Loading WBP를 제거하고 로딩 전용 입력 모드를 해제한다.
// 실행 위치/권한: 각 Client 로컬
// ============================================================================
void UGI_Steam::HideOverlayUMG()
{
	if (LoadingOverlay)
	{
		LoadingOverlay->RemoveFromParent();
		LoadingOverlay = nullptr;
	}

	ApplyLoadingInputToLocalPlayers(false);
}

// ============================================================================
// UGI_Steam::ApplyStageNow
// ----------------------------------------------------------------------------
// 역할: 현재 LoadingStage를 즉시 변경하고 표시 텍스트/최소 표시 시간을 설정한다.
// 데이터 흐름: ELoadingStage → CurrentStageText → BP_ApplyLoadingStage
// ============================================================================
void UGI_Steam::ApplyStageNow(ELoadingStage NewStage)
{
	CurrentStage = NewStage;

	switch (CurrentStage)
	{
	case ELoadingStage::Traveling:
		CurrentStageText = StageText_Traveling;
		break;

	case ELoadingStage::LoadingMap:
		CurrentStageText = StageText_LoadingMap;
		break;

	case ELoadingStage::EnteringPlayers:
		CurrentStageText = StageText_EnteringPlayers;
		break;

	default:
		break;
	}

	StageHoldUntilSeconds = FPlatformTime::Seconds() + (double)FMath::Max(0.f, MinStageDisplaySeconds);
	ApplyStageToWidget();
}

// ============================================================================
// UGI_Steam::RequestStage
// ----------------------------------------------------------------------------
// 역할: Stage 변경 요청을 Queue에 넣고 현재 Stage 최소 표시 시간이 끝났으면 전진을 시도한다.
// ============================================================================
void UGI_Steam::RequestStage(ELoadingStage NewStage)
{
	if (NewStage == CurrentStage) return;

	if (StageQueue.Num() > 0 && StageQueue.Last() == NewStage) return;

	StageQueue.Add(NewStage);
	TryAdvanceStage();
}

// ============================================================================
// UGI_Steam::TryAdvanceStage
// ----------------------------------------------------------------------------
// 역할: StageHold 시간이 끝난 경우 Queue의 다음 Stage를 실제 적용한다.
// ============================================================================
void UGI_Steam::TryAdvanceStage()
{
	if (!bLoadingOn) return;

	const double Now = FPlatformTime::Seconds();
	if (Now < StageHoldUntilSeconds)
	{
		return;
	}

	if (StageQueue.Num() <= 0)
	{
		return;
	}

	const ELoadingStage Next = StageQueue[0];
	StageQueue.RemoveAt(0);

	ApplyStageNow(Next);
}

// ============================================================================
// UGI_Steam::TryAutoAdvanceFromLoadingMap
// ----------------------------------------------------------------------------
// 역할: TargetMap에서 두 플레이어가 모두 도착한 뒤 LoadingMap(2)→EnteringPlayers(3)로 자동 전진한다.
// 데이터 흐름: bAllPlayersReachedTargetLoadingMap=true + Hold 완료 → RequestStage(EnteringPlayers)
// ============================================================================
void UGI_Steam::TryAutoAdvanceFromLoadingMap()
{
	if (!bLoadingOn) return;
	if (!bAutoAdvanceToEnteringPlayers) return;
	if (CurrentStage != ELoadingStage::LoadingMap) return;
	if (StageQueue.Num() > 0) return;

	// 두 플레이어가 모두 최종 목적지 맵의 로딩 UMG에 진입하기 전에는
	// 2단계(LoadingMap)에서 그대로 기다린다.
	if (!bAllPlayersReachedTargetLoadingMap) return;

	const double Now = FPlatformTime::Seconds();
	if (Now < StageHoldUntilSeconds)
	{
		return;
	}

	bAutoAdvanceToEnteringPlayers = false;
	RequestStage(ELoadingStage::EnteringPlayers);
}

// ============================================================================
// UGI_Steam::FinalizeEndIfReady
// ----------------------------------------------------------------------------
// 역할: EndLoading 요청, Stage Queue, 자동진행, 최소 표시시간 조건이 모두 끝났을 때 실제 Loading을 종료한다.
// ============================================================================
void UGI_Steam::FinalizeEndIfReady()
{
	if (!bEndRequested) return;

	if (bAutoAdvanceToEnteringPlayers)
	{
		return;
	}

	if (StageQueue.Num() > 0)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now < StageHoldUntilSeconds)
	{
		return;
	}

	bEndRequested = false;
	bLoadingOn = false;
	bInsideSeamlessTransitionMap = false;
	HideOverlayUMG();
	ResetLoadingDisconnectGuard();

	bLocalReachedTargetLoadingMap = false;
	bAllPlayersReachedTargetLoadingMap = false;
	NextTargetMapArrivalReportSeconds = 0.0;
	FirstTargetMapArrivalSinceSeconds = -1.0;
	TargetMapReachedPlayers.Reset();
}

// ============================================================================
// UGI_Steam::ApplyStageToWidget
// ----------------------------------------------------------------------------
// 역할: 현재 Stage를 1/3, 2/3, 3/3 인덱스와 텍스트로 WBP에 전달한다.
// ============================================================================
void UGI_Steam::ApplyStageToWidget()
{
	if (!LoadingOverlay) return;

	if (ULoadingOverlayWidgetBase* W = Cast<ULoadingOverlayWidgetBase>(LoadingOverlay))
	{
		int32 Index = 1;

		switch (CurrentStage)
		{
		case ELoadingStage::Traveling:
			Index = 1;
			break;

		case ELoadingStage::LoadingMap:
			Index = 2;
			break;

		case ELoadingStage::EnteringPlayers:
			Index = 3;
			break;

		default:
			break;
		}

		W->BP_ApplyLoadingStage(CurrentStageText, Index, 3);
	}
}

// ============================================================================
// UGI_Steam::ApplyLoadingInputToLocalPlayers
// ----------------------------------------------------------------------------
// 역할: 현재 World의 Local LoadingPlayerController에 Loading UIOnly 입력 잠금/해제를 적용한다.
// ============================================================================
void UGI_Steam::ApplyLoadingInputToLocalPlayers(bool bEnable)
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

		ALoadingPlayerControllerBase* LoadingPC = Cast<ALoadingPlayerControllerBase>(PC);
		if (!LoadingPC || !LoadingPC->IsLocalController())
		{
			continue;
		}

		if (bEnable)
		{
			LoadingPC->EnterLoadingInputMode(LoadingOverlay);
		}
		else
		{
			LoadingPC->ExitLoadingInputMode();
		}
	}
}

// =========================
// Transition Map -> Final Map Arrival Barrier
// =========================

// ============================================================================
// UGI_Steam::IsTargetMapWorld
// ----------------------------------------------------------------------------
// 역할: 현재 World의 ShortName이 저장된 TargetMapName과 같은지 판정한다.
// 데이터 흐름: NewWorld PackageName ↔ TargetMapName 비교
// ============================================================================
bool UGI_Steam::IsTargetMapWorld(const UWorld* World) const
{
	if (!World || TargetMapName.IsNone())
	{
		return false;
	}

	const FString LoadedShortName = FPackageName::GetShortName(World->GetOutermost()->GetName());
	const FString TargetShortName = TargetMapName.ToString();

	if (LoadedShortName.Equals(TargetShortName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	// PIE에서는 UEDPIE_0_MAP_Name처럼 접두사가 붙으므로 끝 이름으로도 비교한다.
	return LoadedShortName.EndsWith(
		FString(TEXT("_")) + TargetShortName,
		ESearchCase::IgnoreCase);
}

// ============================================================================
// UGI_Steam::TryReportLocalReachedTargetLoadingMap
// ----------------------------------------------------------------------------
// 역할: 로컬 플레이어가 TargetMap에 도착했다면 LoadingPC의 Server RPC로 서버에 반복 보고한다.
// 실행 위치/권한: 각 Client 로컬 GI
// 데이터 흐름: Local GI → Local LoadingPC → Server_ReportReachedTargetLoadingMap(TargetMapName)
// ============================================================================
void UGI_Steam::TryReportLocalReachedTargetLoadingMap()
{
	if (!bLoadingOn
		|| !bLocalReachedTargetLoadingMap
		|| bAllPlayersReachedTargetLoadingMap
		|| TargetMapName.IsNone())
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now < NextTargetMapArrivalReportSeconds)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ALoadingPlayerControllerBase* LocalLoadingPC = Cast<ALoadingPlayerControllerBase>(
		GetFirstLocalPlayerController(World));

	if (!LocalLoadingPC)
	{
		NextTargetMapArrivalReportSeconds = Now + 0.25;
		return;
	}

	LocalLoadingPC->Server_ReportReachedTargetLoadingMap(TargetMapName);
	NextTargetMapArrivalReportSeconds = Now + 0.5;
}

// ============================================================================
// UGI_Steam::ServerRegisterReachedTargetLoadingMap
// ----------------------------------------------------------------------------
// 역할: 서버가 각 PlayerController의 TargetMap 도착 보고를 중복 없이 수집하고 2/2 도착을 판정한다.
// 실행 위치/권한: Server GI / Authority
// 데이터 흐름: ReportingPC → TargetMapReachedPlayers → 2명 확인 → BroadcastAllPlayersReachedTargetLoadingMap
// ============================================================================
void UGI_Steam::ServerRegisterReachedTargetLoadingMap(
	ALoadingPlayerControllerBase* ReportingPC,
	FName LoadedMapName)
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return;
	}

	if (!bLoadingOn || !ReportingPC || LoadedMapName != TargetMapName)
	{
		return;
	}

	const bool bAlreadyReported = TargetMapReachedPlayers.ContainsByPredicate(
		[ReportingPC](const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC)
		{
			return WeakPC.Get() == ReportingPC;
		});

	if (!bAlreadyReported)
	{
		TargetMapReachedPlayers.Add(ReportingPC);

		if (FirstTargetMapArrivalSinceSeconds < 0.0)
		{
			FirstTargetMapArrivalSinceSeconds = FPlatformTime::Seconds();
		}
	}

	TargetMapReachedPlayers.RemoveAll(
		[](const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC)
		{
			return !WeakPC.IsValid();
		});

	if (TargetMapReachedPlayers.Num() < RequiredPlayersDuringLoading)
	{
		return;
	}

	bAllPlayersReachedTargetLoadingMap = true;
	FirstTargetMapArrivalSinceSeconds = -1.0;
	LoadingPlayerMissingSinceSeconds = -1.0;

	BroadcastAllPlayersReachedTargetLoadingMap();
}

// ============================================================================
// UGI_Steam::BroadcastAllPlayersReachedTargetLoadingMap
// ----------------------------------------------------------------------------
// 역할: 2명 모두 도착한 사실을 모든 LoadingPlayerController의 Client RPC로 전달한다.
// 실행 위치/권한: Server GI
// 데이터 흐름: Server → 각 LoadingPC.Client_ConfirmAllPlayersReachedTargetLoadingMap
// ============================================================================
void UGI_Steam::BroadcastAllPlayersReachedTargetLoadingMap()
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALoadingPlayerControllerBase* LoadingPC = Cast<ALoadingPlayerControllerBase>(It->Get()))
		{
			LoadingPC->Client_ConfirmAllPlayersReachedTargetLoadingMap(TargetMapName);
		}
	}
}

// ============================================================================
// UGI_Steam::ConfirmAllPlayersReachedTargetLoadingMap
// ----------------------------------------------------------------------------
// 역할: 서버의 2/2 도착 승인을 각 로컬 GI 상태에 적용한다.
// 실행 위치/권한: 각 Client 로컬 GI
// 데이터 흐름: Client RPC → bAllPlayersReachedTargetLoadingMap=true
// ============================================================================
void UGI_Steam::ConfirmAllPlayersReachedTargetLoadingMap(FName LoadedMapName)
{
	if (!bLoadingOn || LoadedMapName != TargetMapName)
	{
		return;
	}

	bAllPlayersReachedTargetLoadingMap = true;
	FirstTargetMapArrivalSinceSeconds = -1.0;
	NextTargetMapArrivalReportSeconds = 0.0;
}

// =========================
// Loading / Movie Disconnect Guard
// =========================

// ============================================================================
// UGI_Steam::IsLocalLoadingOrMovieActive
// ----------------------------------------------------------------------------
// 역할: 현재 프로세스가 Loading 또는 PreTravel Movie 상태인지 반환한다.
// ============================================================================
bool UGI_Steam::IsLocalLoadingOrMovieActive() const
{
	return bLoadingOn || bLocalPreTravelMovieActive;
}

// ============================================================================
// UGI_Steam::CountServerLoadingPlayers
// ----------------------------------------------------------------------------
// 역할: 서버 World에 존재하는 LoadingPlayerController 수를 센다.
// 실행 위치/권한: Listen Server GI
// ============================================================================
int32 UGI_Steam::CountServerLoadingPlayers() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (Cast<ALoadingPlayerControllerBase>(PC))
		{
			++Count;
		}
	}

	return Count;
}

// ============================================================================
// UGI_Steam::ResetLoadingDisconnectGuard
// ----------------------------------------------------------------------------
// 역할: 로딩/영상 중 2인 이탈 감시 상태를 초기화한다.
// ============================================================================
void UGI_Steam::ResetLoadingDisconnectGuard()
{
	bLoadingDisconnectGuardArmed = false;
	LoadingPlayerMissingSinceSeconds = -1.0;
}

// ============================================================================
// UGI_Steam::TickLoadingDisconnectGuard
// ----------------------------------------------------------------------------
// 역할: 로딩/시네마틱 동안 2인 접속 상태와 두 번째 플레이어의 TargetMap 도착 Timeout을 감시한다.
// 실행 위치/권한: Listen Server GI
// 데이터 흐름: PlayerController 수/Arrival 시간 → 정상 Travel 일시감소는 유예 → 실제 이탈이면 로비 복귀
// ============================================================================
void UGI_Steam::TickLoadingDisconnectGuard()
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return;
	}

	const bool bShouldWatch =
		bLoadingOn
		|| bPreTravelCinematicInProgress
		|| bPreTravelServerTravelStarted;

	if (!bShouldWatch || bHandlingLoadingPlayerLoss)
	{
		if (!bShouldWatch)
		{
			ResetLoadingDisconnectGuard();
		}
		return;
	}

	// 최종 맵에 한 명이 먼저 들어온 뒤 다른 한 명의 도착 보고가 없으면
	// 2단계에서 최대 TransitionMapPlayerWaitSeconds만큼 기다린다.
	if (bLoadingOn
		&& !bAllPlayersReachedTargetLoadingMap
		&& FirstTargetMapArrivalSinceSeconds >= 0.0)
	{
		const double ArrivalWaitDuration =
			FPlatformTime::Seconds() - FirstTargetMapArrivalSinceSeconds;

		if (ArrivalWaitDuration >= (double)FMath::Max(1.0f, TransitionMapPlayerWaitSeconds))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("GI_Steam: The second player did not reach target map %s within %.2f seconds."),
				*TargetMapName.ToString(),
				TransitionMapPlayerWaitSeconds);

			ForceAllPlayersBackToLobbyBecauseLoadingPlayerLost();
			return;
		}
	}

	const int32 CurrentPlayerCount = CountServerLoadingPlayers();

	// 두 명이 실제로 존재했던 로딩에서만 감시를 시작한다.
	// 메인 메뉴에서 혼자 로딩 UI를 띄운 상황을 오인하지 않기 위한 안전장치다.
	if (!bLoadingDisconnectGuardArmed)
	{
		if (CurrentPlayerCount >= RequiredPlayersDuringLoading)
		{
			bLoadingDisconnectGuardArmed = true;
			LoadingPlayerMissingSinceSeconds = -1.0;
		}
		return;
	}

	if (CurrentPlayerCount == RequiredPlayersDuringLoading)
	{
		LoadingPlayerMissingSinceSeconds = -1.0;
		return;
	}

	const double Now = FPlatformTime::Seconds();

	if (LoadingPlayerMissingSinceSeconds < 0.0)
	{
		LoadingPlayerMissingSinceSeconds = Now;
		return;
	}

	const double MissingDuration = Now - LoadingPlayerMissingSinceSeconds;

	// 실제 ServerTravel/Transition Map 로딩 중에는 PlayerController 수가 잠깐 줄어들 수 있으므로
	// 영상 단계보다 긴 대기 시간을 사용한다.
	const float AllowedMissingSeconds = bLoadingOn
		? FMath::Max(1.0f, TransitionMapPlayerWaitSeconds)
		: FMath::Max(0.0f, LoadingDisconnectGraceSeconds);

	if (MissingDuration < (double)AllowedMissingSeconds)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("GI_Steam: Player count became %d during loading/movie. Returning remaining players to lobby."),
		CurrentPlayerCount
	);

	ForceAllPlayersBackToLobbyBecauseLoadingPlayerLost();
}

// ============================================================================
// UGI_Steam::ForceAllPlayersBackToLobbyBecauseLoadingPlayerLost
// ----------------------------------------------------------------------------
// 역할: 서버가 로딩 중 플레이어 이탈을 확정했을 때 남은 모든 플레이어에게 로비 복귀 Client RPC를 보낸다.
// 실행 위치/권한: Server GI
// ============================================================================
void UGI_Steam::ForceAllPlayersBackToLobbyBecauseLoadingPlayerLost()
{
	if (bHandlingLoadingPlayerLoss)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return;
	}

	bHandlingLoadingPlayerLoss = true;

	// 진행 예정이던 ServerTravel 타이머와 시네마틱 서버 상태를 중단한다.
	World->GetTimerManager().ClearTimer(PreTravelServerTravelTimerHandle);
	CancelPreTravelCinematicAsServer();

	TArray<TWeakObjectPtr<ALoadingPlayerControllerBase>> RemainingPlayers;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALoadingPlayerControllerBase* LoadingPC = Cast<ALoadingPlayerControllerBase>(It->Get()))
		{
			RemainingPlayers.Add(LoadingPC);
		}
	}

	// 남아 있는 호스트/클라이언트 모두 기존 로비 복귀 함수를 사용한다.
	for (const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC : RemainingPlayers)
	{
		if (ALoadingPlayerControllerBase* LoadingPC = WeakPC.Get())
		{
			LoadingPC->Client_ForceBackToLobby();
		}
	}

	ResetLoadingDisconnectGuard();
	bLocalReachedTargetLoadingMap = false;
	bAllPlayersReachedTargetLoadingMap = false;
	FirstTargetMapArrivalSinceSeconds = -1.0;
	TargetMapReachedPlayers.Reset();
}

// ============================================================================
// UGI_Steam::ForceLocalBackToLobbyBecauseLoadingPlayerLost
// ----------------------------------------------------------------------------
// 역할: Host 종료 등 Server RPC를 받을 수 없는 NetworkFailure 상황에서 로컬 Client가 직접 로비로 복귀한다.
// 실행 위치/권한: 각 Client 로컬 GI
// ============================================================================
void UGI_Steam::ForceLocalBackToLobbyBecauseLoadingPlayerLost()
{
	if (bHandlingLoadingPlayerLoss)
	{
		return;
	}

	bHandlingLoadingPlayerLoss = true;

	UWorld* World = GetWorld();
	APlayerController* FirstLocalPC = GetFirstLocalPlayerController(World);

	if (ALoadingPlayerControllerBase* LoadingPC = Cast<ALoadingPlayerControllerBase>(FirstLocalPC))
	{
		// 기존 함수가 영상 정리, 입력 복구, 알림 저장, BP_BackToLobby까지 담당한다.
		LoadingPC->Client_ForceBackToLobby();
		return;
	}

	// NetworkFailure 시점에 PlayerController가 이미 파괴된 경우의 최종 안전망.
	AbortLoadingImmediately();
	bLocalPreTravelMovieActive = false;

	SetPendingMainMenuNotice(
		FText::FromString(TEXT("플레이어가 나가서 로비로 돌아왔습니다."))
	);

	BP_BackToLobby();
}

// ============================================================================
// UGI_Steam::HandleEngineNetworkFailure
// ----------------------------------------------------------------------------
// 역할: PreTravel/Loading 중 엔진 NetworkFailure를 감지해 로컬 복귀 안전망을 실행한다.
// 실행 위치/권한: 각 Client Engine NetworkFailure Delegate
// ============================================================================
void UGI_Steam::HandleEngineNetworkFailure(
	UWorld* InWorld,
	UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	// 이번 요청은 로딩 화면 또는 PreTravel 영상 도중의 연결 종료만 처리한다.
	if (!IsLocalLoadingOrMovieActive())
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("GI_Steam: NetworkFailure during loading/movie. Type=%d Error=%s"),
		(int32)FailureType,
		*ErrorString
	);

	// 호스트 프로세스가 꺼진 경우 서버 RPC를 받을 수 없으므로 클라이언트가 직접 복귀한다.
	ForceLocalBackToLobbyBecauseLoadingPlayerLost();
}

// =========================
// PreTravel Cinematic Server State
// =========================

// ============================================================================
// UGI_Steam::BeginPreTravelCinematicAsServer
// ----------------------------------------------------------------------------
// 역할: 서버 PreTravel 시퀀스의 시작점. 2인 참가자를 수집하고 각 Client에 Movie 재생 RPC를 보낸다.
// 실행 위치/권한: Server GI / Authority
// 데이터 흐름: MapURL/MediaSource 저장 → Participants 수집 → 2인 검증 → Client_PlayPreTravelMovie
// ============================================================================
void UGI_Steam::BeginPreTravelCinematicAsServer(const FString& MapURL, UMediaSource* CinematicMediaSource, ALoadingPlayerControllerBase* RequesterPC)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!World->GetAuthGameMode())
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::BeginPreTravelCinematicAsServer - server authority only."));
		return;
	}

	if (MapURL.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::BeginPreTravelCinematicAsServer - MapURL is empty."));
		return;
	}

	if (bPreTravelCinematicInProgress || bPreTravelServerTravelStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::BeginPreTravelCinematicAsServer - already in progress."));
		return;
	}

	bHandlingLoadingPlayerLoss = false;
	ResetLoadingDisconnectGuard();

	// 시네마틱 보는 중에는 누가 나가면 기존 OnLogout 로직이 작동해야 하므로 false.
	SetGameModeIgnoreLogoutDuringTravel(false);

	PendingPreTravelMapURL = MapURL;
	PendingPreTravelMediaSource = CinematicMediaSource;

	PreTravelParticipants.Reset();
	PreTravelVotedPlayers.Reset();

	bPreTravelCinematicInProgress = true;
	bPreTravelServerTravelStarted = false;

	GatherPreTravelParticipants();

	const int32 TotalCount = GetValidPreTravelParticipantCount();

	// 이 게임은 무조건 2인. 영상 시작 시점부터 2명이 아니면 남은 사람을 바로 로비로 보낸다.
	if (TotalCount != RequiredPlayersDuringLoading)
	{
		ForceAllPlayersBackToLobbyBecauseLoadingPlayerLost();
		return;
	}

	bLoadingDisconnectGuardArmed = true;
	LoadingPlayerMissingSinceSeconds = -1.0;

	// 영상이 없으면 바로 기존 로딩화면 + ServerTravel.
	if (!CinematicMediaSource)
	{
		StartPreTravelServerTravel();
		return;
	}

	for (const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC : PreTravelParticipants)
	{
		if (ALoadingPlayerControllerBase* LoadingPC = WeakPC.Get())
		{
			LoadingPC->Client_PlayPreTravelMovie(MapURL, CinematicMediaSource, TotalCount);
		}
	}

	BroadcastPreTravelSkipVoteStatus();
}

// ============================================================================
// UGI_Steam::ServerRegisterPreTravelSkipVote
// ----------------------------------------------------------------------------
// 역할: 각 Client의 영상 종료/Skip Server RPC를 서버에서 중복 없이 집계하고 2/2면 Travel 단계로 진행한다.
// 실행 위치/권한: Server GI
// ============================================================================
void UGI_Steam::ServerRegisterPreTravelSkipVote(ALoadingPlayerControllerBase* VotingPC)
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return;
	}

	if (!bPreTravelCinematicInProgress || bPreTravelServerTravelStarted)
	{
		return;
	}

	if (!VotingPC)
	{
		return;
	}

	if (!IsPreTravelParticipant(VotingPC))
	{
		return;
	}

	if (!HasPreTravelVoted(VotingPC))
	{
		PreTravelVotedPlayers.Add(VotingPC);
	}

	BroadcastPreTravelSkipVoteStatus();

	const int32 VoteCount = GetPreTravelVoteCount();
	const int32 TotalCount = GetValidPreTravelParticipantCount();

	if (TotalCount > 0 && VoteCount >= TotalCount)
	{
		StartPreTravelServerTravel();
	}
}

// ============================================================================
// UGI_Steam::CancelPreTravelCinematicAsServer
// ----------------------------------------------------------------------------
// 역할: 진행 중인 PreTravel Cinematic/Travel Timer와 참가자/Vote 임시 상태를 취소한다.
// ============================================================================
void UGI_Steam::CancelPreTravelCinematicAsServer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreTravelServerTravelTimerHandle);
	}

	PendingPreTravelMapURL.Empty();
	PendingPreTravelMediaSource.Reset();

	PreTravelParticipants.Reset();
	PreTravelVotedPlayers.Reset();

	bPreTravelCinematicInProgress = false;
	bPreTravelServerTravelStarted = false;

	SetGameModeIgnoreLogoutDuringTravel(false);
}

// ============================================================================
// UGI_Steam::GatherPreTravelParticipants
// ----------------------------------------------------------------------------
// 역할: 현재 Server World의 LoadingPlayerController들을 PreTravel 참가자로 수집한다.
// 실행 위치/권한: Server GI
// ============================================================================
void UGI_Steam::GatherPreTravelParticipants()
{
	PreTravelParticipants.Reset();

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
			PreTravelParticipants.Add(LoadingPC);
		}
	}
}

// ============================================================================
// UGI_Steam::BroadcastPreTravelSkipVoteStatus
// ----------------------------------------------------------------------------
// 역할: 현재 Skip Vote 수를 참가자 전원의 Client UI에 갱신한다.
// 실행 위치/권한: Server GI
// ============================================================================
void UGI_Steam::BroadcastPreTravelSkipVoteStatus()
{
	const int32 VoteCount = GetPreTravelVoteCount();
	const int32 TotalCount = GetValidPreTravelParticipantCount();

	for (const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC : PreTravelParticipants)
	{
		if (ALoadingPlayerControllerBase* LoadingPC = WeakPC.Get())
		{
			LoadingPC->Client_UpdatePreTravelSkipVote(VoteCount, TotalCount);
		}
	}
}

// ============================================================================
// UGI_Steam::StartPreTravelServerTravel
// ----------------------------------------------------------------------------
// 역할: 모든 Client에 먼저 Movie 종료+Loading 시작 RPC를 보낸 뒤 짧은 지연 후 ServerTravel을 예약한다.
// 실행 위치/권한: Server GI
// 데이터 흐름: Client_StopPreTravelMovieAndBeginLoading → 0.15s Timer → ExecutePreTravelServerTravel
// ============================================================================
void UGI_Steam::StartPreTravelServerTravel()
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		CancelPreTravelCinematicAsServer();
		return;
	}

	if (PendingPreTravelMapURL.IsEmpty())
	{
		CancelPreTravelCinematicAsServer();
		return;
	}

	if (bPreTravelServerTravelStarted)
	{
		return;
	}

	bPreTravelServerTravelStarted = true;
	bPreTravelCinematicInProgress = false;

	// 이제부터는 진짜 ServerTravel 단계.
	// 이때 발생하는 Logout은 정상 Travel로 봐야 함.
	SetGameModeIgnoreLogoutDuringTravel(true);

	for (const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC : PreTravelParticipants)
	{
		if (ALoadingPlayerControllerBase* LoadingPC = WeakPC.Get())
		{
			LoadingPC->Client_StopPreTravelMovieAndBeginLoading(PendingPreTravelMapURL);
		}
	}

	World->GetTimerManager().SetTimer(
		PreTravelServerTravelTimerHandle,
		this,
		&UGI_Steam::ExecutePreTravelServerTravel,
		0.15f,
		false
	);
}

// ============================================================================
// UGI_Steam::ExecutePreTravelServerTravel
// ----------------------------------------------------------------------------
// 역할: PreTravel 임시 상태를 정리한 뒤 실제 World->ServerTravel(TravelURL)을 호출한다.
// 실행 위치/권한: Server GI
// ============================================================================
void UGI_Steam::ExecutePreTravelServerTravel()
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		CancelPreTravelCinematicAsServer();
		return;
	}

	if (PendingPreTravelMapURL.IsEmpty())
	{
		CancelPreTravelCinematicAsServer();
		return;
	}

	const FString TravelURL = PendingPreTravelMapURL;

	PendingPreTravelMapURL.Empty();
	PendingPreTravelMediaSource.Reset();
	PreTravelParticipants.Reset();
	PreTravelVotedPlayers.Reset();

	bPreTravelCinematicInProgress = false;
	bPreTravelServerTravelStarted = false;

	World->ServerTravel(TravelURL);
}

// ============================================================================
// UGI_Steam::SetGameModeIgnoreLogoutDuringTravel
// ----------------------------------------------------------------------------
// 역할: GameMode의 Travel 중 Logout 무시 플래그를 찾아 실제 ServerTravel 중 일시적인 Logout을 정상 이탈로 오인하지 않게 한다.
// 실행 위치/권한: Server GI → GameMode
// ============================================================================
void UGI_Steam::SetGameModeIgnoreLogoutDuringTravel(bool bIgnore)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM)
	{
		return;
	}

	const TArray<FName> CandidateNames =
	{
		TEXT("IgnoreLogoutDuringTravel"),
		TEXT("bIgnoreLogoutDuringTravel")
	};

	for (const FName& CandidateName : CandidateNames)
	{
		if (FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(GM->GetClass(), CandidateName))
		{
			BoolProp->SetPropertyValue_InContainer(GM, bIgnore);
			return;
		}
	}

	for (TFieldIterator<FBoolProperty> It(GM->GetClass()); It; ++It)
	{
		FBoolProperty* BoolProp = *It;
		if (!BoolProp)
		{
			continue;
		}

		const FString PropName = BoolProp->GetName();

		const bool bLooksLikeIgnoreLogout =
			PropName.Contains(TEXT("IgnoreLogout"), ESearchCase::IgnoreCase)
			|| PropName.Contains(TEXT("Ignore_Logout"), ESearchCase::IgnoreCase)
			|| PropName.Contains(TEXT("LogoutDuringTravel"), ESearchCase::IgnoreCase)
			|| PropName.Contains(TEXT("Logout_During_Travel"), ESearchCase::IgnoreCase);

		if (bLooksLikeIgnoreLogout)
		{
			BoolProp->SetPropertyValue_InContainer(GM, bIgnore);
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("GI_Steam::SetGameModeIgnoreLogoutDuringTravel - Could not find IgnoreLogoutDuringTravel bool on GameMode %s."), *GM->GetName());
}

// ============================================================================
// UGI_Steam::GetValidPreTravelParticipantCount
// ----------------------------------------------------------------------------
// 역할: 현재 유효한 PreTravel 참가자 수를 반환한다.
// ============================================================================
int32 UGI_Steam::GetValidPreTravelParticipantCount() const
{
	int32 Count = 0;

	for (const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC : PreTravelParticipants)
	{
		if (WeakPC.IsValid())
		{
			++Count;
		}
	}

	return Count;
}

// ============================================================================
// UGI_Steam::GetPreTravelVoteCount
// ----------------------------------------------------------------------------
// 역할: 현재 유효한 Skip/영상 종료 Vote 수를 반환한다.
// ============================================================================
int32 UGI_Steam::GetPreTravelVoteCount() const
{
	int32 Count = 0;

	for (const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC : PreTravelVotedPlayers)
	{
		if (WeakPC.IsValid())
		{
			++Count;
		}
	}

	return Count;
}

// ============================================================================
// UGI_Steam::IsPreTravelParticipant
// ----------------------------------------------------------------------------
// 역할: 특정 LoadingPC가 현재 PreTravel 참가자인지 검사한다.
// ============================================================================
bool UGI_Steam::IsPreTravelParticipant(ALoadingPlayerControllerBase* PC) const
{
	if (!PC)
	{
		return false;
	}

	for (const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC : PreTravelParticipants)
	{
		if (WeakPC.Get() == PC)
		{
			return true;
		}
	}

	return false;
}

// ============================================================================
// UGI_Steam::HasPreTravelVoted
// ----------------------------------------------------------------------------
// 역할: 특정 LoadingPC가 이미 Vote했는지 검사해 중복 Vote를 막는다.
// ============================================================================
bool UGI_Steam::HasPreTravelVoted(ALoadingPlayerControllerBase* PC) const
{
	if (!PC)
	{
		return false;
	}

	for (const TWeakObjectPtr<ALoadingPlayerControllerBase>& WeakPC : PreTravelVotedPlayers)
	{
		if (WeakPC.Get() == PC)
		{
			return true;
		}
	}

	return false;
}

// =========================
// Audio Settings
// =========================

// ============================================================================
// UGI_Steam::SetMasterVolume
// ----------------------------------------------------------------------------
// 역할: Master 볼륨 값을 0~1로 제한해 저장하고 SoundMix에 반영한다.
// ============================================================================
void UGI_Steam::SetMasterVolume(float InVolume)
{
	MasterVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	ApplyAudioSettings(0.05f);
}

// ============================================================================
// UGI_Steam::SetMusicVolume
// ----------------------------------------------------------------------------
// 역할: Music 볼륨 값을 0~1로 제한해 저장하고 SoundMix에 반영한다.
// ============================================================================
void UGI_Steam::SetMusicVolume(float InVolume)
{
	MusicVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	ApplyAudioSettings(0.05f);
}

// ============================================================================
// UGI_Steam::SetSFXVolume
// ----------------------------------------------------------------------------
// 역할: SFX 볼륨 값을 0~1로 제한해 저장하고 SoundMix에 반영한다.
// ============================================================================
void UGI_Steam::SetSFXVolume(float InVolume)
{
	SFXVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	ApplyAudioSettings(0.05f);
}

// ============================================================================
// UGI_Steam::BeginPreTravelCinematicAudioMute
// ----------------------------------------------------------------------------
// 역할: PreTravel 영상 중 게임 Music/SFX만 0으로 내리고 Master는 유지해 영상 소리를 살린다.
// 실행 위치/권한: 각 Client 로컬 GI
// ============================================================================
void UGI_Steam::BeginPreTravelCinematicAudioMute(float FadeTime)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (bPreTravelCinematicAudioMuted)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!UserVolumeSoundMix)
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::BeginPreTravelCinematicAudioMute - UserVolumeSoundMix is not assigned."));
		return;
	}

	// Master는 건드리지 않는다.
	// 시네마틱 영상 소리는 옵션 영향 없이 계속 들리게 한다.
	if (MusicSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(
			World,
			UserVolumeSoundMix,
			MusicSoundClass,
			0.0f,
			1.0f,
			FadeTime,
			true
		);
	}

	if (SFXSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(
			World,
			UserVolumeSoundMix,
			SFXSoundClass,
			0.0f,
			1.0f,
			FadeTime,
			true
		);
	}

	UGameplayStatics::PushSoundMixModifier(World, UserVolumeSoundMix);

	bPreTravelCinematicAudioMuted = true;
}

// ============================================================================
// UGI_Steam::EndPreTravelCinematicAudioMute
// ----------------------------------------------------------------------------
// 역할: 시네마틱 음소거 상태를 해제하고 사용자의 원래 Audio 설정을 다시 적용한다.
// ============================================================================
void UGI_Steam::EndPreTravelCinematicAudioMute(float FadeTime)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (!bPreTravelCinematicAudioMuted)
	{
		return;
	}

	bPreTravelCinematicAudioMuted = false;

	ApplyAudioSettings(FadeTime);
}

// ============================================================================
// UGI_Steam::ApplyAudioSettings
// ----------------------------------------------------------------------------
// 역할: Master/Music/SFX 값을 SoundMixClassOverride로 실제 Audio 시스템에 적용한다.
// ============================================================================
void UGI_Steam::ApplyAudioSettings(float FadeTime)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::ApplyAudioSettings - World is null."));
		return;
	}

	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
	SFXVolume = FMath::Clamp(SFXVolume, 0.0f, 1.0f);

	if (!UserVolumeSoundMix)
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::ApplyAudioSettings - UserVolumeSoundMix is not assigned."));
		return;
	}

	if (!MasterSoundClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::ApplyAudioSettings - MasterSoundClass is not assigned."));
		return;
	}

	if (!MusicSoundClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::ApplyAudioSettings - MusicSoundClass is not assigned."));
		return;
	}

	if (!SFXSoundClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GI_Steam::ApplyAudioSettings - SFXSoundClass is not assigned."));
		return;
	}

	UGameplayStatics::SetSoundMixClassOverride(
		World,
		UserVolumeSoundMix,
		MasterSoundClass,
		MasterVolume,
		1.0f,
		FadeTime,
		true
	);

	UGameplayStatics::SetSoundMixClassOverride(
		World,
		UserVolumeSoundMix,
		MusicSoundClass,
		MusicVolume,
		1.0f,
		FadeTime,
		true
	);

	UGameplayStatics::SetSoundMixClassOverride(
		World,
		UserVolumeSoundMix,
		SFXSoundClass,
		SFXVolume,
		1.0f,
		FadeTime,
		true
	);

	UGameplayStatics::PushSoundMixModifier(World, UserVolumeSoundMix);
}

// ============================================================================
// UGI_Steam::ResetAudioSettingsToDefault
// ----------------------------------------------------------------------------
// 역할: Master/Music/SFX 값을 1.0 기본값으로 되돌리고 적용한다.
// ============================================================================
void UGI_Steam::ResetAudioSettingsToDefault()
{
	MasterVolume = 1.0f;
	MusicVolume = 1.0f;
	SFXVolume = 1.0f;

	ApplyAudioSettings(0.05f);
}