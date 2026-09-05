#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LoadingScreenConfigAsset.generated.h"

class UTexture2D;

/*
================================================================================
FLoadingScreenConfig
================================================================================
한 개 TargetMap의 Loading 화면에 필요한 표시 데이터 묶음.
Travel Logic과 화면 데이터를 분리하여 Map별 if/else 하드코딩을 피한다.
================================================================================
*/
USTRUCT(BlueprintType)
struct FLoadingScreenConfig
{
    GENERATED_BODY()

    // 로딩 배경 이미지. Soft Reference로 보관하고 Loading 시작 시 필요할 때 Load한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
    TSoftObjectPtr<UTexture2D> Image;

    // 해당 Map의 Loading 화면 제목.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
    FText Title;

    // 해당 Map의 Loading 화면 설명 문구.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (MultiLine = true))
    FText Description;
};

/*
================================================================================
FLoadingScreenMapEntry
================================================================================
Map Short Name과 그 Map이 사용할 Loading Config를 한 Entry로 묶는다.
예: MAP_Game1 → Image/Title/Description
================================================================================
*/
USTRUCT(BlueprintType)
struct FLoadingScreenMapEntry
{
    GENERATED_BODY()

    // Map short name. 예) "MAP_Game1" 또는 "Game1" (프로젝트 맵 이름 그대로)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
    FName MapName;

    // 위 MapName에 대응하는 Loading 표시 데이터.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
    FLoadingScreenConfig Config;
};

/*
================================================================================
ULoadingScreenConfigAsset
================================================================================
TargetMapName을 Key처럼 사용해 해당 Map의 Loading 화면 설정을 찾는 DataAsset.
GI_Steam::BeginLoadingForMapURL()에서 GetConfigForMap()을 호출한다.
================================================================================
*/
UCLASS(BlueprintType)
class STEAMDEVELOPMENT_API ULoadingScreenConfigAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // PerMap에 TargetMap이 등록되지 않은 경우 사용할 기본 Loading 설정.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
    FLoadingScreenConfig DefaultConfig;

    // MapName ↔ Config 목록.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
    TArray<FLoadingScreenMapEntry> PerMap;

    // TargetMap의 Short Name을 받아 해당 Loading Config를 반환한다.
    // 일치하는 Map이 없으면 DefaultConfig를 OutConfig에 넣고 false를 반환한다.
    UFUNCTION(BlueprintCallable, Category = "Loading")
    bool GetConfigForMap(FName MapName, FLoadingScreenConfig& OutConfig) const;
};
