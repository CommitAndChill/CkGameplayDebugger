#include "CkPixelArtViewer/CkPixelArtViewer_State.h"

#include "CkCore/Macros/CkMacros.h"

#include <cmath>

namespace ck_pixel_art_viewer_state
{
    struct FTimelineDurations
    {
        double Forward = 0.0;
        double Cycle = 0.0;
    };

    auto
        TryGet_Durations(
            const FCk_PixelSprite_Animation& InAnimation,
            ECk_PixelSprite_Playback InPlayback,
            FTimelineDurations& OutDurations
        )
        -> bool
    {
        const auto Count = InAnimation._Frames.Num();
        if (Count == 0 || Count > 4096 ||
            (InPlayback != ECk_PixelSprite_Playback::Loop &&
             InPlayback != ECk_PixelSprite_Playback::Once &&
             InPlayback != ECk_PixelSprite_Playback::PingPong))
        {
            return false;
        }

        auto Durations = FTimelineDurations{};
        for (const auto& Frame : InAnimation._Frames)
        {
            if (NOT std::isfinite(Frame._DurationMilliseconds) || Frame._DurationMilliseconds <= 0.0)
            {
                return false;
            }
            Durations.Forward += Frame._DurationMilliseconds;
            if (NOT std::isfinite(Durations.Forward))
            {
                return false;
            }
        }
        Durations.Cycle = Durations.Forward;
        if (InPlayback == ECk_PixelSprite_Playback::PingPong)
        {
            for (auto Index = Count - 2; Index > 0; --Index)
            {
                Durations.Cycle += InAnimation._Frames[Index]._DurationMilliseconds;
                if (NOT std::isfinite(Durations.Cycle))
                {
                    return false;
                }
            }
        }
        OutDurations = Durations;
        return true;
    }

    auto
        TryGet_Phase(
            double InAbsoluteMilliseconds,
            int32 InSamplingFps,
            ECk_PixelSprite_Playback InPlayback,
            double InCycleMilliseconds,
            double& OutPhase
        )
        -> bool
    {
        if (NOT std::isfinite(InAbsoluteMilliseconds))
        {
            return false;
        }
        auto Time = InAbsoluteMilliseconds;
        if (InSamplingFps > 0)
        {
            const auto Interval = 1000.0 / InSamplingFps;
            Time = std::floor(Time / Interval) * Interval;
            if (NOT std::isfinite(Time))
            {
                return false;
            }
        }
        if (InPlayback == ECk_PixelSprite_Playback::Once)
        {
            OutPhase = FMath::Clamp(Time, 0.0, InCycleMilliseconds);
            return true;
        }
        auto Phase = std::fmod(Time, InCycleMilliseconds);
        if (Phase < 0.0)
        {
            Phase += InCycleMilliseconds;
        }
        if (Phase >= InCycleMilliseconds)
        {
            Phase = std::nextafter(InCycleMilliseconds, 0.0);
        }
        OutPhase = Phase;
        return true;
    }
}

auto
    FCk_PixelArtViewer_State::
    Seek(
        double InMilliseconds
    )
    -> bool
{
    if (NOT std::isfinite(InMilliseconds))
    {
        return false;
    }
    _TimeMilliseconds = InMilliseconds;
    _ManualFrame = INDEX_NONE;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Advance(
        double InDeltaSeconds,
        const FCk_PixelSprite_Animation& InAnimation
    )
    -> bool
{
    return Advance(InDeltaSeconds, Get_CycleMilliseconds(InAnimation));
}

auto
    FCk_PixelArtViewer_State::
    Advance(
        double InDeltaSeconds,
        double InCycleMilliseconds
    )
    -> bool
{
    if (NOT _Playing || NOT std::isfinite(InDeltaSeconds) || InDeltaSeconds < 0.0)
    {
        return false;
    }
    const auto Next = _TimeMilliseconds + InDeltaSeconds * 1000.0 * _Speed * (_Reverse ? -1.0 : 1.0);
    if (NOT std::isfinite(Next))
    {
        return false;
    }
    if (_Playback == ECk_PixelSprite_Playback::Once &&
        (InCycleMilliseconds <= 0.0 || NOT std::isfinite(InCycleMilliseconds)))
    {
        return false;
    }
    if (Next == _TimeMilliseconds)
    {
        return true;
    }
    if (_Playback == ECk_PixelSprite_Playback::Once)
    {
        const auto End = InCycleMilliseconds;
        if ((_Reverse && Next <= 0.0) || (NOT _Reverse && Next >= End))
        {
            _Playing = false;
            return Seek(FMath::Clamp(Next, 0.0, End));
        }
    }
    return Seek(Next);
}

auto
    FCk_PixelArtViewer_State::
    Set_Speed(
        double InSpeed
    )
    -> bool
{
    if (NOT std::isfinite(InSpeed) || InSpeed < 0.25 || InSpeed > 4.0)
    {
        return false;
    }
    _Speed = InSpeed;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Set_SamplingFps(
        int32 InFps
    )
    -> bool
{
    if (InFps != 0 && InFps != 30 && InFps != 15 && InFps != 10 && InFps != 5)
    {
        return false;
    }
    _SamplingFps = InFps;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Set_Decimation(
        int32 InValue
    )
    -> bool
{
    if (InValue < 1 || InValue > 3)
    {
        return false;
    }
    _Decimation = InValue;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Set_Zoom(
        double InZoom
    )
    -> bool
{
    if (NOT std::isfinite(InZoom) || InZoom < 0.25 || InZoom > 64.0)
    {
        return false;
    }
    _Zoom = InZoom;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Zoom_At(
        double InFactor,
        const FVector2D& InCursor
    )
    -> bool
{
    if (NOT std::isfinite(InFactor) || InFactor <= 0.0 ||
        NOT std::isfinite(InCursor.X) || NOT std::isfinite(InCursor.Y))
    {
        return false;
    }
    const auto OldZoom = _Zoom;
    const auto NewZoom = FMath::Clamp(OldZoom * InFactor, 0.25, 64.0);
    _Pan = InCursor - (InCursor - _Pan) * (NewZoom / OldZoom);
    _Zoom = NewZoom;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Pan_By(
        const FVector2D& InDelta
    )
    -> bool
{
    if (NOT std::isfinite(InDelta.X) || NOT std::isfinite(InDelta.Y) ||
        NOT std::isfinite(_Pan.X + InDelta.X) || NOT std::isfinite(_Pan.Y + InDelta.Y))
    {
        return false;
    }
    _Pan += InDelta;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Fit(
        const FVector2D& InViewport,
        const FIntPoint& InCanvas
    )
    -> bool
{
    if (InCanvas.X <= 0 || InCanvas.Y <= 0 || InViewport.X <= 0 || InViewport.Y <= 0 ||
        NOT std::isfinite(InViewport.X) || NOT std::isfinite(InViewport.Y))
    {
        return false;
    }
    _Zoom = FMath::Clamp(FMath::Min(InViewport.X / InCanvas.X,
        InViewport.Y / InCanvas.Y), 0.25, 64.0);
    _Pan = (InViewport - FVector2D{InCanvas.X * _Zoom, InCanvas.Y * _Zoom}) * 0.5;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Reset_View()
    -> void
{
    _Zoom = 16.0;
    _Pan = FVector2D::ZeroVector;
}

auto
    FCk_PixelArtViewer_State::
    Get_CycleMilliseconds(
        const FCk_PixelSprite_Animation& InAnimation
    ) const
    -> double
{
    auto Duration = 0.0;
    for (const auto& Frame : InAnimation._Frames)
    {
        Duration += Frame._DurationMilliseconds;
    }
    if (_Playback == ECk_PixelSprite_Playback::PingPong && InAnimation._Frames.Num() > 1)
    {
        for (int32 Index = InAnimation._Frames.Num() - 2; Index > 0; --Index)
        {
            Duration += InAnimation._Frames[Index]._DurationMilliseconds;
        }
    }
    return Duration;
}

auto
    FCk_PixelArtViewer_State::
    TryGet_TimelineDurationMilliseconds(
        const FCk_PixelSprite_Animation& InAnimation,
        double& OutMilliseconds
    ) const
    -> bool
{
    auto Durations = ck_pixel_art_viewer_state::FTimelineDurations{};
    if (NOT ck_pixel_art_viewer_state::TryGet_Durations(InAnimation, _Playback, Durations))
    {
        return false;
    }
    OutMilliseconds = Durations.Forward;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    TryGet_TimelineMilliseconds(
        const FCk_PixelSprite_Animation& InAnimation,
        double& OutMilliseconds
    ) const
    -> bool
{
    auto Durations = ck_pixel_art_viewer_state::FTimelineDurations{};
    if (NOT ck_pixel_art_viewer_state::TryGet_Durations(InAnimation, _Playback, Durations) ||
        NOT std::isfinite(_TimeMilliseconds))
    {
        return false;
    }
    if (_ManualFrame != INDEX_NONE)
    {
        if (NOT InAnimation._Frames.IsValidIndex(_ManualFrame))
        {
            return false;
        }
        auto Prefix = 0.0;
        for (auto Index = 0; Index < _ManualFrame; ++Index)
        {
            Prefix += InAnimation._Frames[Index]._DurationMilliseconds;
        }
        OutMilliseconds = Prefix;
        return true;
    }

    auto Phase = 0.0;
    if (NOT ck_pixel_art_viewer_state::TryGet_Phase(
        _TimeMilliseconds, _SamplingFps, _Playback, Durations.Cycle, Phase))
    {
        return false;
    }
    if (_Playback != ECk_PixelSprite_Playback::PingPong || Phase < Durations.Forward)
    {
        OutMilliseconds = Phase;
        return true;
    }

    auto ReturnStart = Durations.Forward;
    auto SourceEnd = Durations.Forward - InAnimation._Frames.Last()._DurationMilliseconds;
    for (auto Index = InAnimation._Frames.Num() - 2; Index > 0; --Index)
    {
        const auto FrameDuration = InAnimation._Frames[Index]._DurationMilliseconds;
        if (Phase < ReturnStart + FrameDuration)
        {
            OutMilliseconds = SourceEnd - (Phase - ReturnStart);
            return true;
        }
        ReturnStart += FrameDuration;
        SourceEnd -= FrameDuration;
    }
    return false;
}

auto
    FCk_PixelArtViewer_State::
    TryGet_TimelineReturning(
        const FCk_PixelSprite_Animation& InAnimation,
        bool& OutReturning
    ) const
    -> bool
{
    auto Durations = ck_pixel_art_viewer_state::FTimelineDurations{};
    if (NOT ck_pixel_art_viewer_state::TryGet_Durations(InAnimation, _Playback, Durations) ||
        NOT std::isfinite(_TimeMilliseconds))
    {
        return false;
    }
    if (_ManualFrame != INDEX_NONE)
    {
        if (NOT InAnimation._Frames.IsValidIndex(_ManualFrame))
        {
            return false;
        }
        OutReturning = false;
        return true;
    }
    auto Phase = 0.0;
    if (NOT ck_pixel_art_viewer_state::TryGet_Phase(
        _TimeMilliseconds, _SamplingFps, _Playback, Durations.Cycle, Phase))
    {
        return false;
    }
    OutReturning = _Playback == ECk_PixelSprite_Playback::PingPong &&
        Phase >= Durations.Forward;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Seek_TimelineMilliseconds(
        const FCk_PixelSprite_Animation& InAnimation,
        double InMilliseconds
    )
    -> bool
{
    auto Durations = ck_pixel_art_viewer_state::FTimelineDurations{};
    if (NOT ck_pixel_art_viewer_state::TryGet_Durations(InAnimation, _Playback, Durations) ||
        NOT std::isfinite(InMilliseconds) || InMilliseconds < 0.0 ||
        InMilliseconds > Durations.Forward)
    {
        return false;
    }
    auto CurrentPhase = 0.0;
    if (NOT ck_pixel_art_viewer_state::TryGet_Phase(
        _TimeMilliseconds, 0, _Playback, Durations.Cycle, CurrentPhase))
    {
        return false;
    }

    auto TargetPhase = InMilliseconds;
    if (_Playback != ECk_PixelSprite_Playback::Once && TargetPhase == Durations.Forward)
    {
        TargetPhase = std::nextafter(Durations.Forward, 0.0);
    }
    if (_Playback == ECk_PixelSprite_Playback::PingPong &&
        CurrentPhase >= Durations.Forward && InMilliseconds < Durations.Forward)
    {
        auto Prefix = 0.0;
        for (auto Index = 0; Index < InAnimation._Frames.Num(); ++Index)
        {
            const auto End = Prefix + InAnimation._Frames[Index]._DurationMilliseconds;
            if (InMilliseconds < End)
            {
                if (Index > 0 && Index < InAnimation._Frames.Num() - 1 &&
                    InMilliseconds > Prefix)
                {
                    auto ReturnStart = Durations.Forward;
                    for (auto ReturnIndex = InAnimation._Frames.Num() - 2;
                        ReturnIndex > Index; --ReturnIndex)
                    {
                        ReturnStart += InAnimation._Frames[ReturnIndex]._DurationMilliseconds;
                    }
                    TargetPhase = ReturnStart + (End - InMilliseconds);
                }
                break;
            }
            Prefix = End;
        }
    }
    const auto CycleBase = _Playback == ECk_PixelSprite_Playback::Once
        ? 0.0 : _TimeMilliseconds - CurrentPhase;
    const auto Absolute = CycleBase + TargetPhase;
    if (NOT std::isfinite(Absolute))
    {
        return false;
    }
    return Seek(Absolute);
}

auto
    FCk_PixelArtViewer_State::
    Try_Select(
        const FCk_PixelSprite_Animation& InAnimation,
        FCk_PixelSprite_FrameSelection& OutSelection
    ) const
    -> bool
{
    auto Preview = InAnimation;
    Preview._Playback = _Playback;
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT ck::pixel_sprite::TryPrepare_Animation(Preview, Prepared))
    {
        return false;
    }
    return Try_Select(*Prepared, InAnimation, OutSelection);
}

auto
    FCk_PixelArtViewer_State::
    Try_Select(
        const FCk_PixelSprite_PreparedAnimation& InPrepared,
        const FCk_PixelSprite_Animation& InAnimation,
        FCk_PixelSprite_FrameSelection& OutSelection
    ) const
    -> bool
{
    auto SampleMilliseconds = _TimeMilliseconds;
    if (_SamplingFps > 0)
    {
        const auto Interval = 1000.0 / _SamplingFps;
        SampleMilliseconds = std::floor(SampleMilliseconds / Interval) * Interval;
        if (NOT std::isfinite(SampleMilliseconds))
        {
            return false;
        }
    }
    auto Selection = FCk_PixelSprite_FrameSelection{};
    if (NOT ck::pixel_sprite::TryEvaluate_PreparedAnimationMilliseconds(
        InPrepared, SampleMilliseconds, Selection))
    {
        return false;
    }
    if (_ManualFrame != INDEX_NONE)
    {
        if (NOT InAnimation._Frames.IsValidIndex(_ManualFrame))
        {
            return false;
        }
        Selection._Index = _ManualFrame;
    }
    else if (_Decimation > 1)
    {
        Selection._Index = (Selection._Index / _Decimation) * _Decimation;
    }
    OutSelection = Selection;
    return true;
}

auto
    FCk_PixelArtViewer_State::
    Step_Frame(
        const FCk_PixelSprite_Animation& InAnimation,
        int32 InDirection
    )
    -> bool
{
    auto Preview = InAnimation;
    Preview._Playback = _Playback;
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT ck::pixel_sprite::TryPrepare_Animation(Preview, Prepared))
    {
        return false;
    }
    return Step_Frame(*Prepared, InAnimation, InDirection);
}

auto
    FCk_PixelArtViewer_State::
    Step_Frame(
        const FCk_PixelSprite_PreparedAnimation& InPrepared,
        const FCk_PixelSprite_Animation& InAnimation,
        int32 InDirection
    )
    -> bool
{
    if (InAnimation._Frames.IsEmpty() || (InDirection != -1 && InDirection != 1))
    {
        return false;
    }
    auto Current = FCk_PixelSprite_FrameSelection{};
    if (NOT Try_Select(InPrepared, InAnimation, Current))
    {
        return false;
    }
    const auto Count = InAnimation._Frames.Num();
    const auto Next = _Playback == ECk_PixelSprite_Playback::Once
        ? FMath::Clamp(Current._Index + InDirection, 0, Count - 1)
        : (Current._Index + InDirection + Count) % Count;
    _Playing = false;
    return Select_Frame(InPrepared, InAnimation, Next);
}

auto
    FCk_PixelArtViewer_State::
    Select_Frame(
        const FCk_PixelSprite_Animation& InAnimation,
        int32 InIndex
    )
    -> bool
{
    auto Preview = InAnimation;
    Preview._Playback = _Playback;
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT ck::pixel_sprite::TryPrepare_Animation(Preview, Prepared))
    {
        return false;
    }
    return Select_Frame(*Prepared, InAnimation, InIndex);
}

auto
    FCk_PixelArtViewer_State::
    Select_Frame(
        const FCk_PixelSprite_PreparedAnimation& InPrepared,
        const FCk_PixelSprite_Animation& InAnimation,
        int32 InIndex
    )
    -> bool
{
    if (NOT InAnimation._Frames.IsValidIndex(InIndex))
    {
        return false;
    }
    auto Start = 0.0;
    if (NOT ck::pixel_sprite::TryGet_PreparedFrameStartMilliseconds(
        InPrepared, InIndex, Start))
    {
        return false;
    }
    if (NOT Seek(Start))
    {
        return false;
    }
    _ManualFrame = InIndex;
    return true;
}
