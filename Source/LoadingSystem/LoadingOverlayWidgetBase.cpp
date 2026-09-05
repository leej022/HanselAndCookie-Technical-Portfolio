#include "LoadingOverlayWidgetBase.h"

/*
================================================================================
LoadingOverlayWidgetBase.cpp
================================================================================
현재 Base Widget의 실제 표시 함수는 BlueprintImplementableEvent로 선언되어 있으므로
C++ 구현 코드가 별도로 필요하지 않다.

GI_Steam::ShowOverlayUMG()
    ↓
ULoadingOverlayWidgetBase로 Cast
    ↓
BP_ApplyLoadingConfig(...)
BP_ApplyLoadingStage(...)
    ↓
WBP_Loading Blueprint에서 실제 Image/Text UI 갱신
================================================================================
*/
