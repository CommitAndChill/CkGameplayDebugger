#pragma once

#include "Engine/DeveloperSettings.h"
#include "CkPixelSprite/CkPixelSprite_Render.h"

#include "CkPixelArtViewer_Settings.generated.h"

/** Personal preview preferences. Authored sprite data and temporary palette overrides stay in memory. */
UCLASS(Config = GameUserSettings, meta = (DisplayName = "Ck Pixel Art Viewer"))
class CKPIXELARTVIEWER_API UCkPixelArtViewerSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual auto
        GetContainerName() const
        -> FName override
    {
        return TEXT("Editor");
    }

    virtual auto
        GetCategoryName() const
        -> FName override
    {
        return TEXT("CkGameplayDebugger");
    }

    UPROPERTY(Config, EditAnywhere, Category = "Layout")
    float _LeftPanelWeight = 0.20f;

    UPROPERTY(Config, EditAnywhere, Category = "Layout")
    float _CanvasPanelWeight = 0.58f;

    UPROPERTY(Config, EditAnywhere, Category = "Layout")
    float _RightPanelWeight = 0.22f;

    UPROPERTY(Config, EditAnywhere, Category = "Canvas")
    bool _ShowChecker = true;

    UPROPERTY(Config, EditAnywhere, Category = "Canvas")
    bool _ShowGrid = true;

    UPROPERTY(Config, EditAnywhere, Category = "Canvas")
    bool _ShowAxes = false;

    UPROPERTY(Config, EditAnywhere, Category = "Canvas")
    bool _ShowCrosshair = false;

    UPROPERTY(Config, EditAnywhere, Category = "Onion Skin")
    bool _ShowOnion = false;

    UPROPERTY(Config, EditAnywhere, Category = "Onion Skin", meta = (ClampMin = "0", ClampMax = "4"))
    int32 _PreviousCount = 1;

    UPROPERTY(Config, EditAnywhere, Category = "Onion Skin", meta = (ClampMin = "0", ClampMax = "4"))
    int32 _FollowingCount = 1;

    UPROPERTY(Config, EditAnywhere, Category = "Onion Skin", meta = (ClampMin = "0.0", ClampMax = "0.75"))
    float _OnionOpacity = 0.25f;

    UPROPERTY(Config, EditAnywhere, Category = "Onion Skin")
    FLinearColor _PreviousTint = FLinearColor{0.35f, 0.65f, 1.0f, 1.0f};

    UPROPERTY(Config, EditAnywhere, Category = "Onion Skin")
    FLinearColor _FollowingTint = FLinearColor{1.0f, 0.45f, 0.35f, 1.0f};

    UPROPERTY(Config, EditAnywhere, Category = "Rendering")
    ECk_PixelSprite_RenderMode _RenderMode = ECk_PixelSprite_RenderMode::Nearest;

    UPROPERTY(Config, EditAnywhere, Category = "Rendering", meta = (ClampMin = "1", ClampMax = "8"))
    int32 _RenderScale = 1;

    UPROPERTY(Config, EditAnywhere, Category = "Rendering")
    bool _Outline = false;

    UPROPERTY(Config, EditAnywhere, Category = "Rendering")
    FColor _OutlineColor = FColor::Black;

    UPROPERTY(Config, EditAnywhere, Category = "Rendering")
    FColor _Background = FColor::Transparent;
};
