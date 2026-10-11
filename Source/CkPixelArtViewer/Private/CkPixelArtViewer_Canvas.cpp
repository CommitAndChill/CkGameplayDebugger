#include "CkPixelArtViewer/CkPixelArtViewer_Canvas.h"

#include "CkCore/Macros/CkMacros.h"

#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#include <cmath>

auto
    SCk_PixelArtViewer_Canvas::
    Construct(
        const FArguments& InArgs
    )
    -> void
{
    SetClipping(EWidgetClipping::ClipToBounds);
    _State = InArgs._State;
    _ImageBrush.DrawAs = ESlateBrushDrawType::Image;
    _ImageBrush.Tiling = ESlateBrushTileType::NoTile;
    _HoverText = FText::FromString(TEXT("Move over a source pixel to inspect its palette index and RGBA"));
}

auto
    SCk_PixelArtViewer_Canvas::
    Set_Frame(
        TObjectPtr<UTexture2D> InTexture,
        TSharedPtr<const FCk_PixelSprite_Manifest> InSnapshot,
        int32 InSourceIndex
    )
    -> void
{
    _Texture = TStrongObjectPtr<UTexture2D>{InTexture.Get()};
    _Snapshot = MoveTemp(InSnapshot);
    _SourceIndex = InSourceIndex;
    _ImageBrush.SetResourceObject(_Texture.Get());
    if (_Texture.IsValid())
    {
        _ImageBrush.ImageSize = FVector2D{static_cast<double>(_Texture->GetSizeX()), static_cast<double>(_Texture->GetSizeY())};
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCk_PixelArtViewer_Canvas::
    Set_Decorations(
        const TArray<TObjectPtr<UTexture2D>>& InPrevious,
        const TArray<TObjectPtr<UTexture2D>>& InFollowing,
        TObjectPtr<UTexture2D> InComparison,
        TObjectPtr<UTexture2D> InDifference,
        TOptional<FIntRect> InDifferenceBounds
    )
    -> void
{
    const auto AddLayers = [](const TArray<TObjectPtr<UTexture2D>>& InTextures,
        TArray<FPreviewLayer>& OutLayers)
    {
        OutLayers.Reset();
        for (const auto Texture : InTextures)
        {
            if (NOT Texture)
            {
                continue;
            }
            auto& Layer = OutLayers.Emplace_GetRef();
            Layer._Texture = TStrongObjectPtr<UTexture2D>{Texture.Get()};
            Layer._Brush.DrawAs = ESlateBrushDrawType::Image;
            Layer._Brush.SetResourceObject(Layer._Texture.Get());
        }
    };
    AddLayers(InPrevious, _PreviousLayers);
    AddLayers(InFollowing, _FollowingLayers);
    _ComparisonLayer._Texture = TStrongObjectPtr<UTexture2D>{InComparison.Get()};
    _ComparisonLayer._Brush.DrawAs = ESlateBrushDrawType::Image;
    _ComparisonLayer._Brush.SetResourceObject(_ComparisonLayer._Texture.Get());
    _DifferenceLayer._Texture = TStrongObjectPtr<UTexture2D>{InDifference.Get()};
    _DifferenceLayer._Brush.DrawAs = ESlateBrushDrawType::Image;
    _DifferenceLayer._Brush.SetResourceObject(_DifferenceLayer._Texture.Get());
    _DifferenceBounds = InDifferenceBounds;
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCk_PixelArtViewer_Canvas::
    Get_HoverText() const
    -> FText
{
    return _HoverText;
}

auto
    SCk_PixelArtViewer_Canvas::
    Center_View()
    -> bool
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Sources.IsValidIndex(_SourceIndex) ||
        NOT _State.IsValid())
    {
        return false;
    }
    const auto& Grid = _Snapshot->_Sources[_SourceIndex]._Grid;
    const auto Size = GetCachedGeometry().GetLocalSize();
    const auto Zoom = _State->_Zoom;
    if (Size.X <= 0.0 || Size.Y <= 0.0 || Grid._Width <= 0 || Grid._Height <= 0 ||
        NOT std::isfinite(Size.X) || NOT std::isfinite(Size.Y) ||
        NOT std::isfinite(Zoom) || Zoom <= 0.0)
    {
        return false;
    }
    const auto SideBySide = _State->_Comparison == ECk_PixelArtViewer_Comparison::SideBySide &&
        _ComparisonLayer._Texture.IsValid();
    const auto Extent = FVector2D{Grid._Width * Zoom * (SideBySide ? 2.0 : 1.0) +
        (SideBySide ? 24.0 : 0.0), Grid._Height * Zoom};
    const auto CandidatePan = (Size - Extent) * 0.5;
    if (NOT std::isfinite(Extent.X) || NOT std::isfinite(Extent.Y) ||
        NOT std::isfinite(CandidatePan.X) || NOT std::isfinite(CandidatePan.Y))
    {
        return false;
    }
    _State->_Pan = CandidatePan;
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

auto
    SCk_PixelArtViewer_Canvas::
    Fit_View()
    -> bool
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Sources.IsValidIndex(_SourceIndex)
        || NOT _State.IsValid())
    {
        return false;
    }
    const auto& Grid = _Snapshot->_Sources[_SourceIndex]._Grid;
    const auto Size = GetCachedGeometry().GetLocalSize();
    auto Fit = false;
    if (_State->_Comparison == ECk_PixelArtViewer_Comparison::SideBySide &&
        _ComparisonLayer._Texture.IsValid())
    {
        constexpr auto Gap = 24.0;
        if (Size.X > Gap && Size.Y > 0.0)
        {
            const auto Scale = FMath::Min((Size.X - Gap) / (Grid._Width * 2.0),
                Size.Y / Grid._Height);
            const auto Zoom = FMath::Clamp(Scale >= 1.0 ? static_cast<double>(FMath::FloorToInt(Scale)) : Scale,
                0.25, 64.0);
            if (_State->Set_Zoom(Zoom))
            {
                _State->_Pan = (Size - FVector2D{Grid._Width * Zoom * 2.0 + Gap,
                    Grid._Height * Zoom}) * 0.5;
                Fit = true;
            }
        }
    }
    else
    {
        Fit = _State->Fit(Size, FIntPoint{Grid._Width, Grid._Height});
    }
    if (Fit)
    {
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return Fit;
}

auto
    SCk_PixelArtViewer_Canvas::
    ComputeDesiredSize(
        float
    ) const
    -> FVector2D
{
    return FVector2D{640.0, 420.0};
}

auto
    SCk_PixelArtViewer_Canvas::
    OnPaint(
        const FPaintArgs&,
        const FGeometry& InGeometry,
        const FSlateRect&,
        FSlateWindowElementList& OutElements,
        int32 InLayer,
        const FWidgetStyle&,
        bool
    ) const
    -> int32
{
    const auto& Brush = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    const auto Size = InGeometry.GetLocalSize();
    FSlateDrawElement::MakeBox(OutElements, InLayer, InGeometry.ToPaintGeometry(),
        &Brush, ESlateDrawEffect::None, FLinearColor{0.08f, 0.09f, 0.11f, 1.0f});
    if (_State.IsValid() && _State->_ShowChecker)
    {
        constexpr auto CheckerSize = 16.0;
        for (auto Y = 0.0; Y < Size.Y; Y += CheckerSize)
        {
            for (auto X = 0.0; X < Size.X; X += CheckerSize)
            {
                if ((static_cast<int32>(X / CheckerSize) + static_cast<int32>(Y / CheckerSize)) % 2 == 0)
                {
                    continue;
                }
                const auto Cell = FVector2D{FMath::Min(CheckerSize, Size.X - X),
                    FMath::Min(CheckerSize, Size.Y - Y)};
                FSlateDrawElement::MakeBox(OutElements, InLayer + 1,
                    InGeometry.ToPaintGeometry(Cell, FSlateLayoutTransform{FVector2D{X, Y}}),
                    &Brush, ESlateDrawEffect::None, FLinearColor{0.13f, 0.14f, 0.16f, 1.0f});
            }
        }
    }

    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Sources.IsValidIndex(_SourceIndex)
        || NOT _Texture.IsValid() || NOT _State.IsValid())
    {
        return InLayer + 1;
    }
    const auto& Grid = _Snapshot->_Sources[_SourceIndex]._Grid;
    const auto Zoom = _State->_Zoom;
    const auto Origin = _State->_Pan;
    const auto ImageSize = FVector2D{Grid._Width * Zoom, Grid._Height * Zoom};
    const auto DrawImage = [&OutElements, &InGeometry, &ImageSize, &Origin](
        const FSlateBrush& InBrush, int32 InLayer, const FLinearColor& InTint,
        const FVector2D& InOffset = FVector2D::ZeroVector)
    {
        FSlateDrawElement::MakeBox(OutElements, InLayer,
            InGeometry.ToPaintGeometry(ImageSize, FSlateLayoutTransform{Origin + InOffset}),
            &InBrush, ESlateDrawEffect::None, InTint);
    };
    if (_State->_ShowOnion)
    {
        const auto Before = FLinearColor{_State->_PreviousTint.R, _State->_PreviousTint.G,
            _State->_PreviousTint.B, _State->_OnionOpacity};
        const auto After = FLinearColor{_State->_FollowingTint.R, _State->_FollowingTint.G,
            _State->_FollowingTint.B, _State->_OnionOpacity};
        for (const auto& Layer : _PreviousLayers)
        {
            DrawImage(Layer._Brush, InLayer + 2, Before);
        }
        for (const auto& Layer : _FollowingLayers)
        {
            DrawImage(Layer._Brush, InLayer + 2, After);
        }
    }
    DrawImage(_ImageBrush, InLayer + 3, FLinearColor::White);
    if (_ComparisonLayer._Texture.IsValid())
    {
        if (_State->_Comparison == ECk_PixelArtViewer_Comparison::Overlay)
        {
            DrawImage(_ComparisonLayer._Brush, InLayer + 4, FLinearColor{1, 1, 1, 0.5f});
        }
        else if (_State->_Comparison == ECk_PixelArtViewer_Comparison::SideBySide)
        {
            DrawImage(_ComparisonLayer._Brush, InLayer + 4, FLinearColor::White,
                FVector2D{ImageSize.X + 24.0, 0.0});
        }
    }
    if (_State->_Comparison == ECk_PixelArtViewer_Comparison::Difference &&
        _DifferenceLayer._Texture.IsValid())
    {
        DrawImage(_DifferenceLayer._Brush, InLayer + 4, FLinearColor::White);
    }
    if (_State->_Comparison == ECk_PixelArtViewer_Comparison::Difference &&
        _State->_ShowDifferenceBounds && _DifferenceBounds.IsSet())
    {
        const auto& Bounds = _DifferenceBounds.GetValue();
        const auto Min = Origin + FVector2D{Bounds.Min.X * Zoom, Bounds.Min.Y * Zoom};
        const auto Max = Origin + FVector2D{Bounds.Max.X * Zoom, Bounds.Max.Y * Zoom};
        FSlateDrawElement::MakeLines(OutElements, InLayer + 5, InGeometry.ToPaintGeometry(),
            TArray<FVector2D>{Min, FVector2D{Max.X, Min.Y}, Max,
                FVector2D{Min.X, Max.Y}, Min},
            ESlateDrawEffect::None, FLinearColor::Yellow, false, 2.0f);
    }
    if (_State->_HighlightPalette && Grid._Palette.IsValidIndex(_State->_SelectedPaletteIndex))
    {
        const auto FirstX = FMath::Max(0, FMath::FloorToInt(-Origin.X / Zoom));
        const auto LastX = FMath::Min(Grid._Width, FMath::CeilToInt((Size.X - Origin.X) / Zoom));
        const auto FirstY = FMath::Max(0, FMath::FloorToInt(-Origin.Y / Zoom));
        const auto LastY = FMath::Min(Grid._Height, FMath::CeilToInt((Size.Y - Origin.Y) / Zoom));
        auto Drawn = 0;
        for (auto Y = FirstY; Y < LastY && Drawn < 4096; ++Y)
        {
            for (auto X = FirstX; X < LastX && Drawn < 4096; ++X)
            {
                if (Grid._Indices[Y * Grid._Width + X] != _State->_SelectedPaletteIndex)
                {
                    continue;
                }
                FSlateDrawElement::MakeBox(OutElements, InLayer + 5,
                    InGeometry.ToPaintGeometry(FVector2D{Zoom, Zoom},
                        FSlateLayoutTransform{Origin + FVector2D{X * Zoom, Y * Zoom}}),
                    &Brush, ESlateDrawEffect::None, FLinearColor{1, 1, 0, 0.42f});
                ++Drawn;
            }
        }
    }

    if (_State->_ShowGrid && Zoom >= 8.0)
    {
        const auto FirstX = FMath::Max(0, FMath::FloorToInt(-Origin.X / Zoom));
        const auto LastX = FMath::Min(Grid._Width, FMath::CeilToInt((Size.X - Origin.X) / Zoom));
        const auto FirstY = FMath::Max(0, FMath::FloorToInt(-Origin.Y / Zoom));
        const auto LastY = FMath::Min(Grid._Height, FMath::CeilToInt((Size.Y - Origin.Y) / Zoom));
        const auto GridColor = FLinearColor{0.85f, 0.88f, 0.92f, 0.20f};
        for (auto X = FirstX; X <= LastX && X < FirstX + 512; ++X)
        {
            const auto At = Origin.X + X * Zoom;
            FSlateDrawElement::MakeLines(OutElements, InLayer + 3, InGeometry.ToPaintGeometry(),
                TArray<FVector2D>{FVector2D{At, Origin.Y}, FVector2D{At, Origin.Y + ImageSize.Y}},
                ESlateDrawEffect::None, GridColor, false, X % 8 == 0 ? 1.5f : 1.0f);
        }
        for (auto Y = FirstY; Y <= LastY && Y < FirstY + 512; ++Y)
        {
            const auto At = Origin.Y + Y * Zoom;
            FSlateDrawElement::MakeLines(OutElements, InLayer + 3, InGeometry.ToPaintGeometry(),
                TArray<FVector2D>{FVector2D{Origin.X, At}, FVector2D{Origin.X + ImageSize.X, At}},
                ESlateDrawEffect::None, GridColor, false, Y % 8 == 0 ? 1.5f : 1.0f);
        }
    }
    if (_State->_ShowAxes)
    {
        FSlateDrawElement::MakeLines(OutElements, InLayer + 4, InGeometry.ToPaintGeometry(),
            TArray<FVector2D>{FVector2D{Origin.X, 0.0}, FVector2D{Origin.X, Size.Y}},
            ESlateDrawEffect::None, FLinearColor::Red, false, 1.0f);
        FSlateDrawElement::MakeLines(OutElements, InLayer + 4, InGeometry.ToPaintGeometry(),
            TArray<FVector2D>{FVector2D{0.0, Origin.Y}, FVector2D{Size.X, Origin.Y}},
            ESlateDrawEffect::None, FLinearColor::Green, false, 1.0f);
    }
    if (_State->_ShowCrosshair)
    {
        FSlateDrawElement::MakeLines(OutElements, InLayer + 5, InGeometry.ToPaintGeometry(),
            TArray<FVector2D>{FVector2D{_LastMouse.X - 8.0, _LastMouse.Y},
                FVector2D{_LastMouse.X + 8.0, _LastMouse.Y}},
            ESlateDrawEffect::None, FLinearColor::Yellow, false, 1.0f);
        FSlateDrawElement::MakeLines(OutElements, InLayer + 5, InGeometry.ToPaintGeometry(),
            TArray<FVector2D>{FVector2D{_LastMouse.X, _LastMouse.Y - 8.0},
                FVector2D{_LastMouse.X, _LastMouse.Y + 8.0}},
            ESlateDrawEffect::None, FLinearColor::Yellow, false, 1.0f);
    }
    return InLayer + 6;
}

auto
    SCk_PixelArtViewer_Canvas::
    Update_Hover(
        const FVector2D& InLocalPoint
    )
    -> void
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Sources.IsValidIndex(_SourceIndex)
        || NOT _State.IsValid())
    {
        return;
    }
    const auto& Grid = _Snapshot->_Sources[_SourceIndex]._Grid;
    const auto X = FMath::FloorToInt((InLocalPoint.X - _State->_Pan.X) / _State->_Zoom);
    const auto Y = FMath::FloorToInt((InLocalPoint.Y - _State->_Pan.Y) / _State->_Zoom);
    if (X < 0 || Y < 0 || X >= Grid._Width || Y >= Grid._Height)
    {
        _HoverText = FText::FromString(TEXT("Outside source canvas"));
        return;
    }
    const auto Index = Grid._Indices[Y * Grid._Width + X];
    if (NOT Grid._Palette.IsValidIndex(Index))
    {
        return;
    }
    const auto Color = Grid._Palette[Index]._Color;
    _HoverText = FText::FromString(FString::Printf(
        TEXT("(%d, %d)  palette %d  RGBA %d, %d, %d, %d"),
        X, Y, Index, Color.R, Color.G, Color.B, Color.A));
}

auto
    SCk_PixelArtViewer_Canvas::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent
    )
    -> FReply
{
    if (InEvent.GetEffectingButton() != EKeys::MiddleMouseButton)
    {
        return FReply::Unhandled();
    }
    _Panning = true;
    _LastMouse = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

auto
    SCk_PixelArtViewer_Canvas::
    OnMouseButtonUp(
        const FGeometry&,
        const FPointerEvent& InEvent
    )
    -> FReply
{
    if (InEvent.GetEffectingButton() != EKeys::MiddleMouseButton || NOT _Panning)
    {
        return FReply::Unhandled();
    }
    _Panning = false;
    return FReply::Handled().ReleaseMouseCapture();
}

auto
    SCk_PixelArtViewer_Canvas::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent
    )
    -> FReply
{
    const auto Local = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());
    if (_Panning && _State.IsValid())
    {
        _State->Pan_By(Local - _LastMouse);
    }
    _LastMouse = Local;
    Update_Hover(Local);
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}

auto
    SCk_PixelArtViewer_Canvas::
    OnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent
    )
    -> FReply
{
    if (NOT _State.IsValid())
    {
        return FReply::Unhandled();
    }
    const auto Cursor = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());
    _State->Zoom_At(std::pow(1.2, InEvent.GetWheelDelta()), Cursor);
    Update_Hover(Cursor);
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}
