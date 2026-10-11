#include "CkPixelArtViewer/CkPixelArtViewer_State.h"

#include "Misc/AutomationTest.h"

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
