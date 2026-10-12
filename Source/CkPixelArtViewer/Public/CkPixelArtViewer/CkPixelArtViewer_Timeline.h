#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

struct FCk_PixelArtViewer_TimelineFrame
{
    double _DurationMilliseconds = 0.0;
    FString _SourceName;
    TSharedPtr<FSlateBrush> _Brush;
};

DECLARE_DELEGATE_OneParam(FCk_PixelArtViewer_OnTimelineScrub, double);

class CKPIXELARTVIEWER_API SCk_PixelArtViewer_Timeline : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCk_PixelArtViewer_Timeline) {}
        SLATE_ATTRIBUTE(double, PlaybackMilliseconds)
        SLATE_ATTRIBUTE(int32, SelectedFrame)
        SLATE_EVENT(FCk_PixelArtViewer_OnTimelineScrub, OnScrub)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs
        )
        -> void;
    auto
        Set_Frames(
            TArray<FCk_PixelArtViewer_TimelineFrame> InFrames
        )
        -> bool;
    auto
        Set_TrackWidth(
            float InWidth
        )
        -> void;
    auto Get_TrackWidth() const -> float;
    auto
        Get_FrameWidth(
            int32 InIndex
        ) const
        -> float;
    auto Get_ThumbnailSize() const -> FVector2D;
    auto Get_PlayheadMilliseconds() const -> double;

    virtual auto
        ComputeDesiredSize(
            float InLayoutScaleMultiplier
        ) const
        -> FVector2D override;
    virtual auto
        OnPaint(
            const FPaintArgs& InArgs,
            const FGeometry& InGeometry,
            const FSlateRect& InCullingRect,
            FSlateWindowElementList& OutElements,
            int32 InLayer,
            const FWidgetStyle& InStyle,
            bool InParentEnabled
        ) const
        -> int32 override;
    virtual auto
        OnMouseButtonDown(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent
        )
        -> FReply override;
    virtual auto
        OnMouseButtonUp(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent
        )
        -> FReply override;
    virtual auto
        OnMouseMove(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent
        )
        -> FReply override;
    virtual auto
        OnMouseLeave(
            const FPointerEvent& InEvent
        )
        -> void override;

private:
    auto
        Get_FrameAtX(
            double InX,
            double InWidth
        ) const
        -> int32;
    auto
        ScrubAt(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent
        )
        -> void;

    TAttribute<double> _PlaybackMilliseconds;
    TAttribute<int32> _SelectedFrame;
    FCk_PixelArtViewer_OnTimelineScrub _OnScrub;
    TArray<FCk_PixelArtViewer_TimelineFrame> _Frames;
    TArray<double> _FrameStarts;
    double _TotalMilliseconds = 0.0;
    float _TrackWidth = 640.0f;
    int32 _HoveredFrame = INDEX_NONE;
};
