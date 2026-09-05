#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingOverlayWidgetBase.generated.h"

class UTexture2D;

/*
================================================================================
ULoadingOverlayWidgetBase
================================================================================
GI_Steam의 C++ Loading State와 실제 Blueprint WBP_Loading 표현을 연결하는 Base Widget.

C++가 담당:
- 최종 TargetMap에 맞는 Image / Title / Description 선택
- Current Loading Stage(1/2/3) 결정

Blueprint WBP가 담당:
- 실제 Image/Text/Throbber 등 화면 디자인

즉 Travel Logic과 UI Presentation을 분리하기 위한 인터페이스 역할이다.
================================================================================
*/
UCLASS(Abstract, Blueprintable)
class STEAMDEVELOPMENT_API ULoadingOverlayWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    // 최종 목적지 Map 기준 DataAsset에서 읽은 배경/타이틀/설명을 WBP에 전달한다.
    // World가 바뀌어 WBP가 새로 생성되어도 GI에 저장된 같은 Config를 다시 전달하므로
    // Current → Transition → Final Map에서 동일 화면처럼 보이게 된다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
    void BP_ApplyLoadingConfig(UTexture2D* Image, const FText& Title, const FText& Description);

    // 현재 3단계 로딩 진행상태를 WBP에 전달한다.
    // Stage 1: Traveling / Stage 2: LoadingMap / Stage 3: EnteringPlayers
    UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
    void BP_ApplyLoadingStage(const FText& StageText, int32 StageIndex, int32 StageTotal);
};
