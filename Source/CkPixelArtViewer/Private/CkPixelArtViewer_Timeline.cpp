#include "CkPixelArtViewer/CkPixelArtViewer_Timeline.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"

#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

#include <cmath>

namespace ck_pixel_art_viewer_timeline
{
    constexpr auto RulerHeight = 24.0f;
    constexpr auto CardTop = 27.0f;
    constexpr auto CardHeight = 80.0f;
    constexpr auto DesiredHeight = 110.0f;
    constexpr auto ThumbnailSide = 56.0f;
    constexpr auto CardPadding = 4.0f;
    constexpr auto ThumbnailTop = 44.0f;
    constexpr auto PlayheadWidth = 2.0f;

    auto
        MakeGeometry(
            const FGeometry& InGeometry,
            float InX,
            float InY,
            float InWidth,
            float InHeight
        )
        -> FPaintGeometry
    {
        return InGeometry.ToPaintGeometry(
            FVector2f{InWidth, InHeight}, FSlateLayoutTransform{FVector2f{InX, InY}});
    }

    auto
        ChooseRulerStep(
            double InTotalMilliseconds,
            float InWidth
        )
        -> double
    {
        if (InWidth <= 72.0f)
        {
            return InTotalMilliseconds;
        }
        const auto MinimumMilliseconds = InTotalMilliseconds * (72.0 / InWidth);
        auto Magnitude = 1.0;
        while (Magnitude * 10.0 < MinimumMilliseconds)
        {
            Magnitude *= 10.0;
        }
        for (const auto Multiplier : {1.0, 2.0, 5.0, 10.0})
        {
            const auto Step = Magnitude * Multiplier;
            if (Step >= MinimumMilliseconds)
            {
                return Step;
            }
        }
        return Magnitude * 10.0;
    }
}

auto
    SCk_PixelArtViewer_Timeline::
    Construct(const FArguments& InArgs)
    -> void
{
    SetClipping(EWidgetClipping::ClipToBounds);
    ForceVolatile(true);
    _PlaybackMilliseconds = InArgs._PlaybackMilliseconds;
    _SelectedFrame = InArgs._SelectedFrame;
    _OnScrub = InArgs._OnScrub;
    SetToolTipText(TAttribute<FText>::CreateLambda([this]() -> FText
    {
        if (NOT _Frames.IsValidIndex(_HoveredFrame))
        {
            return FText::GetEmpty();
        }
        const auto& Frame = _Frames[_HoveredFrame];
        return FText::FromString(FString::Printf(TEXT("Frame %d · %.3f s · %s"),
            _HoveredFrame + 1, Frame._DurationMilliseconds / 1000.0, *Frame._SourceName));
    }));
}

auto
    SCk_PixelArtViewer_Timeline::
    Set_Frames(
        TArray<FCk_PixelArtViewer_TimelineFrame> InFrames
    )
    -> bool
{
    auto CandidateStarts = TArray<double>{};
    CandidateStarts.Reserve(InFrames.Num());
    auto CandidateTotal = 0.0;
    for (const auto& Frame : InFrames)
    {
        const auto DurationIsValid = std::isfinite(Frame._DurationMilliseconds)
            && Frame._DurationMilliseconds > 0.0;
        CK_ENSURE_IF_NOT(DurationIsValid, TEXT("Pixel art timeline frame has invalid duration"))
        {
            return false;
        }
        CandidateStarts.Add(CandidateTotal);
        CandidateTotal += Frame._DurationMilliseconds;
        const auto TotalIsValid = std::isfinite(CandidateTotal) && CandidateTotal > 0.0;
        CK_ENSURE_IF_NOT(TotalIsValid, TEXT("Pixel art timeline duration overflow"))
        {
            return false;
        }
    }

    _Frames = MoveTemp(InFrames);
    _FrameStarts = MoveTemp(CandidateStarts);
    _TotalMilliseconds = CandidateTotal;
    _HoveredFrame = INDEX_NONE;
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

auto
    SCk_PixelArtViewer_Timeline::
    Set_TrackWidth(
        float InWidth
    )
    -> void
{
    const auto WidthIsValid = std::isfinite(InWidth) && InWidth > 0.0f && InWidth <= 10000000.0f;
    CK_ENSURE_IF_NOT(WidthIsValid, TEXT("Pixel art timeline track width is invalid"))
    {
        return;
    }
    if (_TrackWidth == InWidth)
    {
        return;
    }
    _TrackWidth = InWidth;
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

auto
    SCk_PixelArtViewer_Timeline::
    Get_TrackWidth() const
    -> float
{
    return _TrackWidth;
}

auto
    SCk_PixelArtViewer_Timeline::
    Get_FrameWidth(
        int32 InIndex
    ) const
    -> float
{
    if (NOT _Frames.IsValidIndex(InIndex) || _TotalMilliseconds <= 0.0)
    {
        return 0.0f;
    }
    const auto End = InIndex + 1 < _FrameStarts.Num()
        ? _FrameStarts[InIndex + 1] : _TotalMilliseconds;
    return static_cast<float>((End - _FrameStarts[InIndex]) / _TotalMilliseconds * _TrackWidth);
}

auto
    SCk_PixelArtViewer_Timeline::
    Get_ThumbnailSize() const
    -> FVector2D
{
    return FVector2D{ck_pixel_art_viewer_timeline::ThumbnailSide,
        ck_pixel_art_viewer_timeline::ThumbnailSide};
}

auto
    SCk_PixelArtViewer_Timeline::
    Get_PlayheadMilliseconds() const
    -> double
{
    if (_TotalMilliseconds <= 0.0)
    {
        return 0.0;
    }
    const auto Playback = _PlaybackMilliseconds.Get(0.0);
    if (NOT std::isfinite(Playback))
    {
        return 0.0;
    }
    return FMath::Clamp(Playback, 0.0, _TotalMilliseconds);
}

auto
    SCk_PixelArtViewer_Timeline::
    ComputeDesiredSize(
        float
    ) const
    -> FVector2D
{
    return FVector2D{_TrackWidth, ck_pixel_art_viewer_timeline::DesiredHeight};
}

auto
    SCk_PixelArtViewer_Timeline::
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
    using namespace ck_pixel_art_viewer_timeline;

    const auto Size = InGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X);
    const auto Height = static_cast<float>(Size.Y);
    const auto& WhiteBrush = *FAppStyle::Get().GetBrush(TEXT("WhiteBrush"));
    const auto& Font = FAppStyle::GetFontStyle(TEXT("SmallFont"));
    const auto LineColor = FLinearColor{0.32f, 0.35f, 0.39f, 1.0f};
    const auto TextColor = FLinearColor{0.72f, 0.75f, 0.78f, 1.0f};

    FSlateDrawElement::MakeBox(OutElements, InLayer,
        MakeGeometry(InGeometry, 0.0f, RulerHeight - 1.0f, Width, 1.0f),
        &WhiteBrush, ESlateDrawEffect::None, LineColor);

    if (_TotalMilliseconds <= 0.0 || _Frames.IsEmpty() || Width <= 0.0f || Height <= 0.0f)
    {
        return InLayer + 1;
    }

    const auto Step = ChooseRulerStep(_TotalMilliseconds, Width);
    for (auto Tick = 0.0; Tick <= _TotalMilliseconds && Tick / Step < 1000.0; Tick += Step)
    {
        const auto X = static_cast<float>(Tick / _TotalMilliseconds * Width);
        FSlateDrawElement::MakeBox(OutElements, InLayer + 1,
            MakeGeometry(InGeometry, X, RulerHeight - 7.0f, 1.0f, 7.0f),
            &WhiteBrush, ESlateDrawEffect::None, LineColor);
        FSlateDrawElement::MakeText(OutElements, InLayer + 1,
            MakeGeometry(InGeometry, X + (X > 0.0f ? -18.0f : 2.0f), 1.0f, 60.0f, 18.0f),
            FString::Printf(TEXT("%.2f s"), Tick / 1000.0), Font,
            ESlateDrawEffect::None, TextColor);
    }

    for (auto Index = 0; Index < _Frames.Num(); ++Index)
    {
        const auto StartX = static_cast<float>(_FrameStarts[Index] / _TotalMilliseconds * Width);
        const auto EndMilliseconds = Index + 1 < _FrameStarts.Num()
            ? _FrameStarts[Index + 1] : _TotalMilliseconds;
        const auto EndX = static_cast<float>(EndMilliseconds / _TotalMilliseconds * Width);
        const auto FrameWidth = FMath::Max(0.0f, EndX - StartX);
        if (FrameWidth <= 0.0f)
        {
            continue;
        }

        const auto ClipTopLeft = InGeometry.LocalToAbsolute(FVector2D{StartX, CardTop});
        const auto ClipBottomRight = InGeometry.LocalToAbsolute(
            FVector2D{EndX, FMath::Min(Height, CardTop + CardHeight)});
        OutElements.PushClip(FSlateClippingZone(FSlateRect{
            ClipTopLeft.X, ClipTopLeft.Y, ClipBottomRight.X, ClipBottomRight.Y}));

        const auto Selected = Index == _SelectedFrame.Get(INDEX_NONE);
        const auto CardColor = Selected
            ? FLinearColor{0.18f, 0.29f, 0.38f, 1.0f}
            : FLinearColor{0.14f, 0.16f, 0.19f, 1.0f};
        FSlateDrawElement::MakeBox(OutElements, InLayer + 1,
            MakeGeometry(InGeometry, StartX, CardTop, FrameWidth, CardHeight),
            &WhiteBrush, ESlateDrawEffect::None, CardColor);
        FSlateDrawElement::MakeBox(OutElements, InLayer + 2,
            MakeGeometry(InGeometry, EndX - 1.0f, CardTop, 1.0f, CardHeight),
            &WhiteBrush, ESlateDrawEffect::None, LineColor);

        FSlateDrawElement::MakeText(OutElements, InLayer + 3,
            MakeGeometry(InGeometry, StartX + CardPadding, CardTop + 2.0f, 60.0f, 14.0f),
            FString::Printf(TEXT("%02d"), Index + 1), Font,
            ESlateDrawEffect::None, FLinearColor::White);
        FSlateDrawElement::MakeText(OutElements, InLayer + 3,
            MakeGeometry(InGeometry, StartX + 28.0f, CardTop + 2.0f, 100.0f, 14.0f),
            FString::Printf(TEXT("%.0f ms"), _Frames[Index]._DurationMilliseconds), Font,
            ESlateDrawEffect::None, TextColor);

        if (_Frames[Index]._Brush.IsValid())
        {
            FSlateDrawElement::MakeBox(OutElements, InLayer + 3,
                MakeGeometry(InGeometry, StartX + CardPadding, ThumbnailTop,
                    ThumbnailSide, ThumbnailSide),
                _Frames[Index]._Brush.Get(), ESlateDrawEffect::None, FLinearColor::White);
        }
        OutElements.PopClip();
    }

    const auto PlayheadX = static_cast<float>(Get_PlayheadMilliseconds() / _TotalMilliseconds * Width);
    FSlateDrawElement::MakeBox(OutElements, InLayer + 4,
        MakeGeometry(InGeometry, FMath::Clamp(PlayheadX - PlayheadWidth * 0.5f,
            0.0f, FMath::Max(0.0f, Width - PlayheadWidth)), RulerHeight - 1.0f,
            PlayheadWidth, FMath::Max(0.0f, Height - RulerHeight + 1.0f)),
        &WhiteBrush, ESlateDrawEffect::None, FLinearColor{1.0f, 0.49f, 0.32f, 1.0f});
    return InLayer + 4;
}

auto
    SCk_PixelArtViewer_Timeline::
    Get_FrameAtX(
        double InX,
        double InWidth
    ) const
    -> int32
{
    if (_TotalMilliseconds <= 0.0 || _Frames.IsEmpty() || InWidth <= 0.0)
    {
        return INDEX_NONE;
    }
    const auto Time = FMath::Clamp(InX / InWidth, 0.0, 1.0) * _TotalMilliseconds;
    for (auto Index = 0; Index + 1 < _FrameStarts.Num(); ++Index)
    {
        if (Time < _FrameStarts[Index + 1])
        {
            return Index;
        }
    }
    return _Frames.Num() - 1;
}

auto
    SCk_PixelArtViewer_Timeline::
    ScrubAt(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent
    )
    -> void
{
    if (_TotalMilliseconds <= 0.0 || NOT _OnScrub.IsBound())
    {
        return;
    }
    const auto LocalX = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition()).X;
    const auto Width = InGeometry.GetLocalSize().X;
    if (NOT std::isfinite(LocalX) || NOT std::isfinite(Width) || Width <= 0.0)
    {
        return;
    }
    _OnScrub.Execute(FMath::Clamp(LocalX / Width, 0.0, 1.0) * _TotalMilliseconds);
}

auto
    SCk_PixelArtViewer_Timeline::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent
    )
    -> FReply
{
    if (InEvent.GetEffectingButton() != EKeys::LeftMouseButton || _Frames.IsEmpty())
    {
        return FReply::Unhandled();
    }
    ScrubAt(InGeometry, InEvent);
    return FReply::Handled().CaptureMouse(AsShared());
}

auto
    SCk_PixelArtViewer_Timeline::
    OnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent
    )
    -> FReply
{
    if (InEvent.GetEffectingButton() != EKeys::LeftMouseButton || NOT HasMouseCapture())
    {
        return FReply::Unhandled();
    }
    ScrubAt(InGeometry, InEvent);
    return FReply::Handled().ReleaseMouseCapture();
}

auto
    SCk_PixelArtViewer_Timeline::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent
    )
    -> FReply
{
    const auto LocalX = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition()).X;
    _HoveredFrame = std::isfinite(LocalX)
        ? Get_FrameAtX(LocalX, InGeometry.GetLocalSize().X) : INDEX_NONE;
    if (HasMouseCapture())
    {
        ScrubAt(InGeometry, InEvent);
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

auto
    SCk_PixelArtViewer_Timeline::
    OnMouseLeave(
        const FPointerEvent& InEvent
    )
    -> void
{
    _HoveredFrame = INDEX_NONE;
    SLeafWidget::OnMouseLeave(InEvent);
}
