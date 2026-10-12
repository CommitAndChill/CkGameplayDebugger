#include "CkPixelArtViewer/CkPixelArtViewer_State.h"

#include "Misc/AutomationTest.h"

#include <cmath>
#include <limits>

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkPixelArtViewer_StateTest,
    "Ck.PixelArtViewer.State", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkPixelArtViewer_StateTest::
    RunTest(
        const FString&
    )
    -> bool
{
    auto State = FCk_PixelArtViewer_State{};
    TestFalse(TEXT("Negative speed rejected"), State.Set_Speed(-1.0));
    TestFalse(TEXT("Unsupported sampling rate rejected"), State.Set_SamplingFps(7));
    TestFalse(TEXT("Zero decimation rejected"), State.Set_Decimation(0));
    TestTrue(TEXT("Zoom at cursor accepted"), State.Zoom_At(2.0, FVector2D{100.0, 50.0}));
    TestEqual(TEXT("Cursor zoom"), State._Zoom, 32.0);
    TestEqual(TEXT("Cursor stays on same image point"), State._Pan, FVector2D{-100.0, -50.0});
    TestTrue(TEXT("Pan accepted"), State.Pan_By(FVector2D{12.0, 4.0}));
    TestEqual(TEXT("Pan offset"), State._Pan, FVector2D{-88.0, -46.0});
    TestTrue(TEXT("Fit accepted"), State.Fit(FVector2D{320.0, 240.0}, FIntPoint{16, 16}));
    TestEqual(TEXT("Fit integer zoom"), State._Zoom, 15.0);
    TestEqual(TEXT("Fit centers horizontally"), State._Pan.X, 40.0);

    auto Animation = FCk_PixelSprite_Animation{};
    Animation._Name = TEXT("walk");
    for (const auto Duration : {100.0, 200.0, 300.0})
    {
        auto Frame = FCk_PixelSprite_Frame{};
        Frame._DurationMilliseconds = Duration;
        Animation._Frames.Add(MoveTemp(Frame));
    }
    TestEqual(TEXT("Loop duration"), State.Get_CycleMilliseconds(Animation), 600.0);
    State._Playback = ECk_PixelSprite_Playback::PingPong;
    TestEqual(TEXT("Ping pong duration"), State.Get_CycleMilliseconds(Animation), 800.0);
    State._Playback = ECk_PixelSprite_Playback::Loop;
    auto PreparedLoop = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT TestTrue(TEXT("Prepare reusable viewer timing"),
        ck::pixel_sprite::TryPrepare_Animation(Animation, PreparedLoop)))
    {
        return false;
    }
    auto CachedSelection = FCk_PixelSprite_FrameSelection{};
    TestTrue(TEXT("Prepared timing selects same authored frame"), State.Seek(125.0)
        && State.Try_Select(*PreparedLoop, Animation, CachedSelection));
    TestEqual(TEXT("Prepared frame index"), CachedSelection._Index, 1);
    auto Selection = FCk_PixelSprite_FrameSelection{};
    TestTrue(TEXT("Scrub into second authored frame"), State.Seek(125.0)
        && State.Try_Select(Animation, Selection));
    TestEqual(TEXT("Second frame selected"), Selection._Index, 1);
    TestTrue(TEXT("Step moves to following frame"), State.Step_Frame(Animation, 1)
        && State.Try_Select(Animation, Selection));
    TestEqual(TEXT("Third frame selected"), Selection._Index, 2);
    TestTrue(TEXT("Sampling accepted"), State.Set_SamplingFps(10));
    TestTrue(TEXT("Decimation accepted"), State.Set_Decimation(2));
    TestTrue(TEXT("Sampled and decimated selection valid"), State.Try_Select(Animation, Selection));
    TestEqual(TEXT("Decimation snaps source selection"), Selection._Index, 2);
    TestTrue(TEXT("Seek to start for manual step"), State.Seek(0.0));
    TestTrue(TEXT("Manual step ignores frame decimation and sampling"),
        State.Step_Frame(Animation, 1) && State.Try_Select(Animation, Selection));
    TestEqual(TEXT("Manual step selects the exact second authored frame"), Selection._Index, 1);
    TestTrue(TEXT("Following manual step advances from exact frame"),
        State.Step_Frame(Animation, 1) && State.Try_Select(Animation, Selection));
    TestEqual(TEXT("Second manual step selects third authored frame"), Selection._Index, 2);
    State._Playback = ECk_PixelSprite_Playback::Once;
    State._Reverse = true;
    TestTrue(TEXT("Reverse once starts inside sequence"), State.Seek(25.0));
    State._Playing = true;
    TestTrue(TEXT("Reverse once reaches beginning"), State.Advance(0.050, Animation));
    TestEqual(TEXT("Reverse once clamps at start"), State._TimeMilliseconds, 0.0);
    TestFalse(TEXT("Reverse once stops at start"), State._Playing);
    State._Reverse = false;
    TestTrue(TEXT("Forward once starts inside sequence"), State.Seek(575.0));
    State._Playing = true;
    TestTrue(TEXT("Forward once reaches end"), State.Advance(0.050, Animation));
    TestEqual(TEXT("Forward once clamps at end"), State._TimeMilliseconds, 600.0);
    TestFalse(TEXT("Forward once stops at end"), State._Playing);

    auto Timeline = FCk_PixelArtViewer_State{};
    auto Cursor = -1.0;
    auto Returning = false;
    auto TimelineDuration = -1.0;
    TestTrue(TEXT("Authored timeline duration"),
        Timeline.TryGet_TimelineDurationMilliseconds(Animation, TimelineDuration));
    TestEqual(TEXT("Authored duration ignores playback visits"), TimelineDuration, 600.0);
    TestTrue(TEXT("Loop maps uneven holds directly"), Timeline.Seek(425.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Loop cursor uses authored duration"), Cursor, 425.0);
    TestTrue(TEXT("Exact loop cycle wraps"), Timeline.Seek(600.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Exact loop cursor is zero"), Cursor, 0.0);
    TestTrue(TEXT("Negative loop clock wraps"), Timeline.Seek(-25.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Negative loop cursor"), Cursor, 575.0);
    TestTrue(TEXT("Negative exact loop cycle wraps"), Timeline.Seek(-600.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Negative exact loop cursor is zero"), Cursor, 0.0);
    TestTrue(TEXT("Seek preserves negative loop quotient"),
        Timeline.Seek(-25.0) && Timeline.Seek_TimelineMilliseconds(Animation, 125.0));
    TestEqual(TEXT("Negative loop seek absolute clock"), Timeline._TimeMilliseconds, -475.0);

    Timeline._Playback = ECk_PixelSprite_Playback::PingPong;
    TestTrue(TEXT("Ping pong keeps authored timeline duration"),
        Timeline.TryGet_TimelineDurationMilliseconds(Animation, TimelineDuration));
    TestEqual(TEXT("Ping pong authored duration"), TimelineDuration, 600.0);
    TestTrue(TEXT("Ping pong enters interior return"), Timeline.Seek(650.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor) &&
        Timeline.TryGet_TimelineReturning(Animation, Returning));
    TestEqual(TEXT("Return begins inside second source frame"), Cursor, 250.0);
    TestTrue(TEXT("Return flag"), Returning);
    Timeline._Playing = true;
    TestTrue(TEXT("Forward playback advances return visit"), Timeline.Advance(0.050, Animation) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Return cursor moves backward"), Cursor, 200.0);
    TestTrue(TEXT("Inverse seek retains return visit"),
        Timeline.Seek_TimelineMilliseconds(Animation, 150.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Inverse return cursor"), Cursor, 150.0);
    TestEqual(TEXT("Inverse return absolute clock"), Timeline._TimeMilliseconds, 750.0);
    TestTrue(TEXT("Return leg survives seek"),
        Timeline.TryGet_TimelineReturning(Animation, Returning));
    TestTrue(TEXT("Return leg remains active"), Returning);
    TestTrue(TEXT("Seek to last endpoint uses forward visit"),
        Timeline.Seek_TimelineMilliseconds(Animation, 350.0) &&
        Timeline.TryGet_TimelineReturning(Animation, Returning));
    TestEqual(TEXT("Last endpoint forward clock"), Timeline._TimeMilliseconds, 350.0);
    TestFalse(TEXT("Last endpoint has no return visit"), Returning);
    TestTrue(TEXT("Seek to first endpoint uses forward visit"),
        Timeline.Seek(700.0) && Timeline.Seek_TimelineMilliseconds(Animation, 50.0) &&
        Timeline.TryGet_TimelineReturning(Animation, Returning));
    TestEqual(TEXT("First endpoint forward clock"), Timeline._TimeMilliseconds, 50.0);
    TestFalse(TEXT("First endpoint has no return visit"), Returning);
    TestTrue(TEXT("Next cycle return retains quotient"), Timeline.Seek(1450.0) &&
        Timeline.Seek_TimelineMilliseconds(Animation, 250.0));
    TestEqual(TEXT("Next cycle return absolute clock"), Timeline._TimeMilliseconds, 1450.0);
    TestTrue(TEXT("Negative ping pong return wraps"), Timeline.Seek(-50.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Negative return cursor"), Cursor, 150.0);
    TestTrue(TEXT("Exact ping pong cycle wraps"), Timeline.Seek(800.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Exact ping pong cursor"), Cursor, 0.0);

    TestTrue(TEXT("Sampling rate applies to cursor"), Timeline.Set_SamplingFps(10) &&
        Timeline.Seek(250.0) && Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Sampled cursor"), Cursor, 200.0);
    TestTrue(TEXT("Decimation accepted for cursor check"), Timeline.Set_Decimation(3) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Decimation does not alter logical cursor"), Cursor, 200.0);
    TestTrue(TEXT("Manual frame pin"), Timeline.Select_Frame(Animation, 2) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Manual pin uses authored prefix"), Cursor, 300.0);
    TestTrue(TEXT("Manual pin is forward"),
        Timeline.TryGet_TimelineReturning(Animation, Returning));
    TestFalse(TEXT("Manual pin does not claim a return visit"), Returning);
    TestTrue(TEXT("Seek clears manual pin"), Timeline.Seek(650.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Sampled return cursor"), Cursor, 300.0);
    TestTrue(TEXT("Disable sampling"), Timeline.Set_SamplingFps(0));
    Timeline._Reverse = true;
    Timeline._Playing = true;
    TestTrue(TEXT("Reverse playback traverses return backward"), Timeline.Seek(700.0) &&
        Timeline.Advance(0.050, Animation) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Reverse playback cursor moves forward"), Cursor, 250.0);

    Timeline._Playback = ECk_PixelSprite_Playback::Once;
    TestTrue(TEXT("Once seek reaches exact authored end"),
        Timeline.Seek_TimelineMilliseconds(Animation, 600.0) &&
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestEqual(TEXT("Once authored end cursor"), Cursor, 600.0);
    TestEqual(TEXT("Once authored end clock"), Timeline._TimeMilliseconds, 600.0);

    const auto ClockBeforeRejection = Timeline._TimeMilliseconds;
    const auto CursorBeforeRejection = Cursor;
    TestFalse(TEXT("Timeline rejects NaN seek"), Timeline.Seek_TimelineMilliseconds(
        Animation, std::numeric_limits<double>::quiet_NaN()));
    TestFalse(TEXT("Timeline rejects out of range seek"),
        Timeline.Seek_TimelineMilliseconds(Animation, 601.0));
    TestEqual(TEXT("Rejected seeks preserve clock"), Timeline._TimeMilliseconds, ClockBeforeRejection);
    auto InvalidAnimation = Animation;
    InvalidAnimation._Frames[1]._DurationMilliseconds =
        std::numeric_limits<double>::infinity();
    TestFalse(TEXT("Timeline rejects nonfinite duration"),
        Timeline.Seek_TimelineMilliseconds(InvalidAnimation, 150.0));
    TestFalse(TEXT("Timeline getter rejects nonfinite duration"),
        Timeline.TryGet_TimelineMilliseconds(InvalidAnimation, Cursor));
    Returning = true;
    TestFalse(TEXT("Return getter rejects nonfinite duration"),
        Timeline.TryGet_TimelineReturning(InvalidAnimation, Returning));
    TestTrue(TEXT("Rejected return getter preserves output"), Returning);
    TestFalse(TEXT("Timeline duration rejects nonfinite frame"),
        Timeline.TryGet_TimelineDurationMilliseconds(InvalidAnimation, TimelineDuration));
    TestEqual(TEXT("Rejected getter preserves output"), Cursor, CursorBeforeRejection);
    TestEqual(TEXT("Rejected duration preserves output"), TimelineDuration, 600.0);
    TestEqual(TEXT("Rejected duration preserves clock"),
        Timeline._TimeMilliseconds, ClockBeforeRejection);
    InvalidAnimation = Animation;
    for (auto& Frame : InvalidAnimation._Frames)
    {
        Frame._DurationMilliseconds = std::numeric_limits<double>::max();
    }
    TestFalse(TEXT("Timeline rejects overflowing total"),
        Timeline.Seek_TimelineMilliseconds(InvalidAnimation, 100.0));
    TestEqual(TEXT("Overflow rejection preserves clock"),
        Timeline._TimeMilliseconds, ClockBeforeRejection);
    Timeline._TimeMilliseconds = std::numeric_limits<double>::infinity();
    TestFalse(TEXT("Timeline getter rejects nonfinite clock"),
        Timeline.TryGet_TimelineMilliseconds(Animation, Cursor));
    TestFalse(TEXT("Timeline seek rejects nonfinite clock"),
        Timeline.Seek_TimelineMilliseconds(Animation, 150.0));
    TestTrue(TEXT("Rejected clock remains unchanged"),
        std::isinf(Timeline._TimeMilliseconds));

    auto TinyFrames = FCk_PixelSprite_Animation{};
    TinyFrames._Name = TEXT("tiny");
    for (const auto Duration : {86400000.0, 1e-300, 2e-300, 100.0})
    {
        auto Frame = FCk_PixelSprite_Frame{};
        Frame._DurationMilliseconds = Duration;
        TinyFrames._Frames.Add(MoveTemp(Frame));
    }
    State._Playback = ECk_PixelSprite_Playback::Loop;
    State._SamplingFps = 5;
    State._Decimation = 3;
    auto PreparedTiny = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT TestTrue(TEXT("Prepare sub-ULP animation once"),
        ck::pixel_sprite::TryPrepare_Animation(TinyFrames, PreparedTiny)))
    {
        return false;
    }
    TestTrue(TEXT("Select first frame before sub-ULP step"), State.Select_Frame(TinyFrames, 0));
    for (int32 Index = 1; Index < 4; ++Index)
    {
        TestTrue(TEXT("Manual step reaches authored sub-ULP frame"),
            State.Step_Frame(*PreparedTiny, TinyFrames, 1) &&
            State.Try_Select(*PreparedTiny, TinyFrames, Selection));
        TestEqual(TEXT("Manual index remains exact"), Selection._Index, Index);
    }
    TestTrue(TEXT("Select sub-ULP frame for stationary playback"),
        State.Select_Frame(*PreparedTiny, TinyFrames, 2));
    State._Playing = true;
    TestTrue(TEXT("Zero delta preserves manual frame"),
        State.Advance(0.0, State.Get_CycleMilliseconds(TinyFrames)) &&
        State.Try_Select(*PreparedTiny, TinyFrames, Selection));
    TestEqual(TEXT("Zero delta keeps the sub-ULP frame"), Selection._Index, 2);
    TestTrue(TEXT("Unrepresentably small positive delta preserves manual frame"),
        State.Advance(1e-300, State.Get_CycleMilliseconds(TinyFrames)) &&
        State.Try_Select(*PreparedTiny, TinyFrames, Selection));
    TestEqual(TEXT("Rounded zero-motion keeps sub-ULP frame"), Selection._Index, 2);
    TestFalse(TEXT("NaN seek rejected"), State.Seek(std::numeric_limits<double>::quiet_NaN()));
    return true;
}

#endif
