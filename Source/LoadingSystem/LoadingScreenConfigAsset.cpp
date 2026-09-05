#include "LoadingScreenConfigAsset.h"

// ============================================================================
// ULoadingScreenConfigAsset::GetConfigForMap
// ----------------------------------------------------------------------------
// 역할:
// GI_Steam이 저장한 TargetMapName으로 PerMap 배열을 검색해
// 해당 목적지 Map에 사용할 Loading Image/Title/Description을 반환한다.
//
// 데이터 흐름:
// GI_Steam::TargetMapName
//      ↓
// LoadingConfigAsset->GetConfigForMap(TargetMapName, Picked)
//      ↓
// PerMap에서 MapName 검색
//      ↓
// Picked(Image/Title/Description)
//      ↓
// GI_Steam::CurrentImage / CurrentTitle / CurrentDesc
// ============================================================================
bool ULoadingScreenConfigAsset::GetConfigForMap(FName MapName, FLoadingScreenConfig& OutConfig) const
{
    // Editor DataAsset에 등록된 Map별 설정을 순회한다.
    for (const FLoadingScreenMapEntry& E : PerMap)
    {
        // 요청받은 TargetMapName과 같은 Entry를 찾으면 해당 Config를 반환한다.
        if (E.MapName == MapName)
        {
            OutConfig = E.Config;
            return true;
        }
    }

    // 등록되지 않은 Map이라도 Loading 화면이 비어버리지 않도록 DefaultConfig 사용.
    OutConfig = DefaultConfig;
    return false;
}
