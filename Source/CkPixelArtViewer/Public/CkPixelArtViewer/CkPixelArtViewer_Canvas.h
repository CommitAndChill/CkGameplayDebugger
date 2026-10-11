#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "CkPixelArtViewer/CkPixelArtViewer_State.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SLeafWidget.h"


class CKPIXELARTVIEWER_API SCk_PixelArtViewer_Canvas : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCk_PixelArtViewer_Canvas) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_PixelArtViewer_State>, State)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs
        )
        -> void;
    auto
        Set_Frame(
            TObjectPtr<UTexture2D> InTexture,
            TSharedPtr<const FCk_PixelSprite_Manifest> InSnapshot,
            int32 InSourceIndex
        )
        -> void;
    auto
        Set_Decorations(
            const TArray<TObjectPtr<UTexture2D>>& InPrevious,
            const TArray<TObjectPtr<UTexture2D>>& InFollowing,
            TObjectPtr<UTexture2D> InComparison,
            TObjectPtr<UTexture2D> InDifference,
            TOptional<FIntRect> InDifferenceBounds
        )
        -> void;
    auto
        Get_HoverText() const
        -> FText;
    auto
        Fit_View()
        -> bool;
    auto
        Center_View()
        -> bool;

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
        OnMouseWheel(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent
        )
        -> FReply override;

private:
    struct FPreviewLayer
    {
        TStrongObjectPtr<UTexture2D> _Texture;
        FSlateBrush _Brush;
    };

    auto
        Update_Hover(
            const FVector2D& InLocalPoint
        )
        -> void;

    TSharedPtr<FCk_PixelArtViewer_State> _State;
    TSharedPtr<const FCk_PixelSprite_Manifest> _Snapshot;
    TStrongObjectPtr<UTexture2D> _Texture;
    FSlateBrush _ImageBrush;
    TArray<FPreviewLayer> _PreviousLayers;
    TArray<FPreviewLayer> _FollowingLayers;
    FPreviewLayer _ComparisonLayer;
    FPreviewLayer _DifferenceLayer;
    TOptional<FIntRect> _DifferenceBounds;
    FText _HoverText;
    FVector2D _LastMouse = FVector2D::ZeroVector;
    int32 _SourceIndex = INDEX_NONE;
    bool _Panning = false;
};
