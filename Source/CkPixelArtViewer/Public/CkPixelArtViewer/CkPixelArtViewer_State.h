#pragma once

#include "CoreMinimal.h"
#include "CkPixelSprite/CkPixelSprite_Render.h"

enum class ECk_PixelArtViewer_Comparison : uint8
{
    None,
    SideBySide,
    Overlay,
    Difference
};

/** Presentation state only. Authored frames, times and palette stay in the immutable asset. */
struct CKPIXELARTVIEWER_API FCk_PixelArtViewer_State
{
    double _TimeMilliseconds = 0.0;
    double _Speed = 1.0;
    double _Zoom = 16.0;
    FVector2D _Pan = FVector2D::ZeroVector;
    bool _Playing = false;
    bool _Reverse = false;
    bool _ShowChecker = true;
    bool _ShowGrid = true;
    bool _ShowAxes = false;
    bool _ShowCrosshair = false;
    bool _ShowOnion = false;
    bool _HighlightPalette = false;
    bool _ShowDifferenceBounds = true;
    int32 _PreviousOnionCount = 1;
    int32 _FollowingOnionCount = 1;
    float _OnionOpacity = 0.25f;
    FLinearColor _PreviousTint = FLinearColor{0.35f, 0.65f, 1.0f, 1.0f};
    FLinearColor _FollowingTint = FLinearColor{1.0f, 0.45f, 0.35f, 1.0f};
    TSet<int32> _SelectedPreviousOffsets;
    TSet<int32> _SelectedFollowingOffsets;
    int32 _SelectedPaletteIndex = INDEX_NONE;
    int32 _ComparisonFrame = INDEX_NONE;
    ECk_PixelArtViewer_Comparison _Comparison = ECk_PixelArtViewer_Comparison::None;
    FCk_PixelSprite_RenderSettings _RenderSettings;
    TMap<int32, FColor> _TemporaryPaletteOverrides;
    int32 _SamplingFps = 0;
    int32 _Decimation = 1;
    ECk_PixelSprite_Playback _Playback = ECk_PixelSprite_Playback::Loop;

    auto
        Seek(
            double InMilliseconds
        )
        -> bool;
    auto
        Advance(
            double InDeltaSeconds,
            const FCk_PixelSprite_Animation& InAnimation
        )
        -> bool;
    auto
        Advance(
            double InDeltaSeconds,
            double InCycleMilliseconds
        )
        -> bool;
    auto
        Set_Speed(
            double InSpeed
        )
        -> bool;
    auto
        Set_SamplingFps(
            int32 InFps
        )
        -> bool;
    auto
        Set_Decimation(
            int32 InValue
        )
        -> bool;
    auto
        Set_Zoom(
            double InZoom
        )
        -> bool;
    auto
        Zoom_At(
            double InFactor,
            const FVector2D& InCursor
        )
        -> bool;
    auto
        Pan_By(
            const FVector2D& InDelta
        )
        -> bool;
    auto
        Fit(
            const FVector2D& InViewport,
            const FIntPoint& InCanvas
        )
        -> bool;
    auto
        Reset_View()
        -> void;
    auto
        Try_Select(
            const FCk_PixelSprite_Animation& InAnimation,
            FCk_PixelSprite_FrameSelection& OutSelection
        ) const
        -> bool;
    auto
        Try_Select(
            const FCk_PixelSprite_PreparedAnimation& InPrepared,
            const FCk_PixelSprite_Animation& InAnimation,
            FCk_PixelSprite_FrameSelection& OutSelection
        ) const
        -> bool;
    auto
        Get_CycleMilliseconds(
            const FCk_PixelSprite_Animation& InAnimation
        ) const
        -> double;
    /** Cursor on the authored forward-duration axis; a manual frame is pinned to its prefix. */
    /** Validates authored holds and returns their forward sum, regardless of playback mode. */
    auto
        TryGet_TimelineDurationMilliseconds(
            const FCk_PixelSprite_Animation& InAnimation,
            double& OutMilliseconds
        ) const
        -> bool;
    /** Uses sampled presentation time; source decimation never changes this position. */
    auto
        TryGet_TimelineMilliseconds(
            const FCk_PixelSprite_Animation& InAnimation,
            double& OutMilliseconds
        ) const
        -> bool;
    /** The return visit exists only for interior frames in PingPong playback. */
    auto
        TryGet_TimelineReturning(
            const FCk_PixelSprite_Animation& InAnimation,
            bool& OutReturning
        ) const
        -> bool;
    /** Keeps the current absolute cycle and return leg for interior frames. End frames use forward. */
    auto
        Seek_TimelineMilliseconds(
            const FCk_PixelSprite_Animation& InAnimation,
            double InMilliseconds
        )
        -> bool;
    auto
        Step_Frame(
            const FCk_PixelSprite_Animation& InAnimation,
            int32 InDirection
        )
        -> bool;
    auto
        Step_Frame(
            const FCk_PixelSprite_PreparedAnimation& InPrepared,
            const FCk_PixelSprite_Animation& InAnimation,
            int32 InDirection
        )
        -> bool;
    auto
        Select_Frame(
            const FCk_PixelSprite_Animation& InAnimation,
            int32 InIndex
        )
        -> bool;
    auto
        Select_Frame(
            const FCk_PixelSprite_PreparedAnimation& InPrepared,
            const FCk_PixelSprite_Animation& InAnimation,
            int32 InIndex
        )
        -> bool;

private:
    int32 _ManualFrame = INDEX_NONE;
};
