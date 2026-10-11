#include "../../CkPixelArtViewer_Module.h"

#include "CkPixelArtViewer/CkPixelArtViewer_Window.h"
#include "CkPixelArtViewer/CkPixelArtViewer_Canvas.h"
#include "CkPixelArtViewer/CkPixelArtViewer_Settings.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkPixelSpriteEditor/CkPixelSpriteEditor_AssetDefinition.h"
#include "CkPixelSpriteEditor/CkPixelSpriteEditor_Factory.h"
#include "AssetRegistry/AssetData.h"
#include "CkCore/Macros/CkMacros.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableViewBase.h"

#include <limits>

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_art_viewer_authored_tests
{
    auto
        Tick(
            FSlateApplication& InSlate
        )
        -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto
        ContainsText(
            const TSharedRef<SWidget>& InRoot,
            const FString& InText
        )
        -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString().Equals(
                InText, ESearchCase::CaseSensitive))
        {
            return true;
        }
        const auto& Children = *InRoot->GetChildren();
        for (auto Index = int32{0}; Index < Children.Num(); ++Index)
        {
            if (ContainsText(ConstCastSharedRef<SWidget>(Children.GetChildAt(Index)), InText))
            {
                return true;
            }
        }
        return false;
    }

    auto
        FindButton(
            const TSharedRef<SWidget>& InRoot,
            const FString& InText
        )
        -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton")
            && ContainsText(InRoot, InText))
        {
            return StaticCastSharedRef<SButton>(InRoot);
        }
        const auto& Children = *InRoot->GetChildren();
        for (auto Index = int32{0}; Index < Children.Num(); ++Index)
        {
            const auto Found = FindButton(ConstCastSharedRef<SWidget>(Children.GetChildAt(Index)), InText);
            if (Found.IsValid())
            {
                return Found;
            }
        }
        return {};
    }

    auto
        FindTreeView(
            const TSharedRef<SWidget>& InRoot
        )
        -> TSharedPtr<STableViewBase>
    {
        if (InRoot->GetTypeAsString().StartsWith(TEXT("STreeView")))
        {
            return StaticCastSharedRef<STableViewBase>(InRoot);
        }
        const auto& Children = *InRoot->GetChildren();
        for (auto Index = int32{0}; Index < Children.Num(); ++Index)
        {
            const auto Found = FindTreeView(ConstCastSharedRef<SWidget>(Children.GetChildAt(Index)));
            if (Found.IsValid())
            {
                return Found;
            }
        }
        return {};
    }

    auto
        Capture(
            FSlateApplication& InSlate,
            const TSharedRef<SWidget>& InRoot,
            const FString& InPath,
            TArray<FColor>& OutPixels,
            FIntVector& OutSize
        )
        -> bool
    {
        TArray<FColor> Pixels;
        auto Size = FIntVector::ZeroValue;
        if (NOT InSlate.TakeScreenshot(InRoot, Pixels, Size)
            || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y)
        {
            return false;
        }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), true);
        OutPixels = Pixels;
        OutSize = Size;
        return FImageUtils::SaveImageByExtension(*InPath,
            FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8));
    }

    auto
        Click(
            FSlateApplication& InSlate,
            const TSharedRef<SWindow>& InWindow,
            const TSharedRef<SButton>& InButton
        )
        -> bool
    {
        if (NOT InWindow->GetNativeWindow().IsValid())
        {
            return false;
        }
        const auto Geometry = InButton->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0 || Geometry.GetLocalSize().Y <= 0.0)
        {
            return false;
        }
        const auto Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
        const auto NoButtons = TSet<FKey>{};
        const auto LeftButton = TSet<FKey>{EKeys::LeftMouseButton};
        const auto Move = FPointerEvent{0, FSlateApplication::CursorPointerIndex,
            Position, Position, NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{}};
        const auto Down = FPointerEvent{0, FSlateApplication::CursorPointerIndex,
            Position, Position, LeftButton, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
        const auto Up = FPointerEvent{0, FSlateApplication::CursorPointerIndex,
            Position, Position, NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(Move, true);
        const auto Target = InSlate.LocateWindowUnderMouse(Position,
            InSlate.GetInteractiveTopLevelWindows(), false, 0);
        auto Found = false;
        for (auto Index = 0; Index < Target.Widgets.Num(); ++Index)
        {
            Found |= Target.Widgets[Index].Widget == InButton;
        }
        if (NOT Found)
        {
            return false;
        }
        const auto DownHandled = InSlate.ProcessMouseButtonDownEvent(
            InWindow->GetNativeWindow(), Down);
        const auto UpHandled = InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return DownHandled && UpHandled;
    }

    auto
        Drag_FirstSplitterHandle(
            FSlateApplication& InSlate,
            const TSharedRef<SWindow>& InWindow,
            const TSharedRef<SSplitter>& InSplitter,
            double InPixels)
        -> bool
    {
        if (NOT InWindow->GetNativeWindow().IsValid() || InSplitter->GetChildren()->Num() < 2)
        {
            return false;
        }
        const auto& First = InSplitter->GetChildren()->GetChildAt(0)->GetCachedGeometry();
        const auto Start = First.LocalToAbsolute(FVector2D{First.GetLocalSize().X + 2.5,
            First.GetLocalSize().Y * 0.5});
        const auto End = Start + FVector2D{InPixels, 0.0};
        const auto InitialWeight = InSplitter->SlotAt(0).GetSizeValue();
        const auto NoButtons = TSet<FKey>{};
        const auto LeftButton = TSet<FKey>{EKeys::LeftMouseButton};
        const auto Hover = FPointerEvent{0, FSlateApplication::CursorPointerIndex,
            Start, Start, NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{}};
        const auto Down = FPointerEvent{0, FSlateApplication::CursorPointerIndex,
            Start, Start, LeftButton, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
        const auto Move = FPointerEvent{0, FSlateApplication::CursorPointerIndex,
            End, Start, LeftButton, EKeys::Invalid, 0.0f, FModifierKeysState{}};
        const auto Up = FPointerEvent{0, FSlateApplication::CursorPointerIndex,
            End, End, NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
        InSlate.SetCursorPos(Start);
        InSlate.ProcessMouseMoveEvent(Hover, true);
        const auto DownHandled = InSlate.ProcessMouseButtonDownEvent(
            InWindow->GetNativeWindow(), Down);
        InSlate.SetCursorPos(End);
        InSlate.ProcessMouseMoveEvent(Move, false);
        const auto Resized = InSplitter->SlotAt(0).GetSizeValue() > InitialWeight + 0.001f;
        const auto UpHandled = InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return DownHandled && Resized && UpHandled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkPixelArtViewer_Authored,
    "Ck.PixelArtViewer.AuthoredWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

auto
    FCkPixelArtViewer_Authored::
    RunTest(
        const FString&
    )
    -> bool
{
    using namespace ck_pixel_art_viewer_authored_tests;

    if (NOT TestTrue(TEXT("Slate is initialized"), FSlateApplication::IsInitialized()))
    {
        return false;
    }
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Fixture plugin exists"), Plugin.IsValid()))
    {
        return false;
    }
    const auto Fixture = Plugin->GetBaseDir() / TEXT("Tools/CkPixel/Examples/Character/character.json");
    auto Factory = TStrongObjectPtr<UCk_PixelSpriteFactory_UE>{NewObject<UCk_PixelSpriteFactory_UE>()};
    auto Canceled = false;
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        Cast<UCk_PixelSpriteAsset_UE>(Factory->FactoryCreateFile(
            UCk_PixelSpriteAsset_UE::StaticClass(), GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UCk_PixelSpriteAsset_UE::StaticClass()),
            RF_Transient, Fixture, nullptr, GWarn, Canceled))};
    if (NOT TestTrue(TEXT("16 by 16 Character imports through production factory"),
        Asset.IsValid()))
    {
        return false;
    }

    auto& Module = FCkPixelArtViewerModule::Get();
    auto Definition = TStrongObjectPtr<UCk_PixelSpriteAssetDefinition_UE>{
        NewObject<UCk_PixelSpriteAssetDefinition_UE>()};
    auto OpenArgs = FAssetOpenArgs{};
    OpenArgs.Assets = TArray<FAssetData>{FAssetData{Asset.Get()}};
    OpenArgs.OpenMethod = EAssetOpenMethod::Edit;
    TestEqual(TEXT("Asset definition opens production viewer"),
        Definition->OpenAssets(OpenArgs), EAssetCommandResult::Handled);
    TestTrue(TEXT("Viewer selects the imported Character"), Module.Get_Window().IsValid()
        && Module.Get_Window()->Get_Asset() == Asset.Get());
    Module.Close_Viewer();

    auto& Slate = FSlateApplication::Get();
    auto Preferences = TObjectPtr<UCkPixelArtViewerSettings>{
        GetMutableDefault<UCkPixelArtViewerSettings>()};
    const auto OldGrid = Preferences->_ShowGrid;
    const auto OldChecker = Preferences->_ShowChecker;
    const auto OldAxes = Preferences->_ShowAxes;
    const auto OldCrosshair = Preferences->_ShowCrosshair;
    const auto OldOnion = Preferences->_ShowOnion;
    const auto OldPreviousCount = Preferences->_PreviousCount;
    const auto OldFollowingCount = Preferences->_FollowingCount;
    const auto OldOnionOpacity = Preferences->_OnionOpacity;
    const auto OldPreviousTint = Preferences->_PreviousTint;
    const auto OldFollowingTint = Preferences->_FollowingTint;
    const auto OldMode = Preferences->_RenderMode;
    const auto OldScale = Preferences->_RenderScale;
    const auto OldOutline = Preferences->_Outline;
    const auto OldOutlineColor = Preferences->_OutlineColor;
    const auto OldBackground = Preferences->_Background;
    const auto OldLeftWeight = Preferences->_LeftPanelWeight;
    const auto OldCanvasWeight = Preferences->_CanvasPanelWeight;
    const auto OldRightWeight = Preferences->_RightPanelWeight;
    Preferences->_ShowGrid = true;
    Preferences->_ShowOnion = false;
    Preferences->_RenderMode = ECk_PixelSprite_RenderMode::Nearest;
    Preferences->_RenderScale = 1;
    Preferences->_LeftPanelWeight = std::numeric_limits<float>::quiet_NaN();
    Preferences->_CanvasPanelWeight = 0.0f;
    Preferences->_RightPanelWeight = 1.0f;
    ON_SCOPE_EXIT
    {
        Preferences->_ShowGrid = OldGrid;
        Preferences->_ShowChecker = OldChecker;
        Preferences->_ShowAxes = OldAxes;
        Preferences->_ShowCrosshair = OldCrosshair;
        Preferences->_ShowOnion = OldOnion;
        Preferences->_PreviousCount = OldPreviousCount;
        Preferences->_FollowingCount = OldFollowingCount;
        Preferences->_OnionOpacity = OldOnionOpacity;
        Preferences->_PreviousTint = OldPreviousTint;
        Preferences->_FollowingTint = OldFollowingTint;
        Preferences->_RenderMode = OldMode;
        Preferences->_RenderScale = OldScale;
        Preferences->_Outline = OldOutline;
        Preferences->_OutlineColor = OldOutlineColor;
        Preferences->_Background = OldBackground;
        Preferences->_LeftPanelWeight = OldLeftWeight;
        Preferences->_CanvasPanelWeight = OldCanvasWeight;
        Preferences->_RightPanelWeight = OldRightWeight;
        Preferences->SaveConfig();
    };
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid())
        {
            Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
        }
    };
    auto Viewer = TSharedPtr<SCk_PixelArtViewer_Window>{};
    SAssignNew(Viewer, SCk_PixelArtViewer_Window);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1280.0f, 800.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Viewer.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    Tick(Slate);
    const auto Splitter = Viewer->Get_MainSplitter();
    TestTrue(TEXT("Malformed saved panel layout falls back to useful widths"),
        Splitter.IsValid() &&
        FMath::IsNearlyEqual(Splitter->SlotAt(0).GetSizeValue(), 0.20f) &&
        FMath::IsNearlyEqual(Splitter->SlotAt(1).GetSizeValue(), 0.58f) &&
        FMath::IsNearlyEqual(Splitter->SlotAt(2).GetSizeValue(), 0.22f));
    if (Splitter.IsValid())
    {
        TestTrue(TEXT("Physical splitter handle drag is handled"),
            Drag_FirstSplitterHandle(Slate, HostWindow.ToSharedRef(), Splitter.ToSharedRef(), 48.0));
        TestTrue(TEXT("Panel resize persists normalized user layout"),
            Preferences->_LeftPanelWeight > 0.20f &&
            FMath::IsNearlyEqual(Preferences->_LeftPanelWeight +
                Preferences->_CanvasPanelWeight + Preferences->_RightPanelWeight, 1.0f, 0.001f));
        const auto Reconstructed = SNew(SCk_PixelArtViewer_Window);
        const auto ReconstructedSplitter = Reconstructed->Get_MainSplitter();
        TestTrue(TEXT("New viewer reconstructs persisted panel layout"),
            ReconstructedSplitter.IsValid() &&
            FMath::IsNearlyEqual(ReconstructedSplitter->SlotAt(0).GetSizeValue(),
                Preferences->_LeftPanelWeight, 0.001f) &&
            FMath::IsNearlyEqual(ReconstructedSplitter->SlotAt(1).GetSizeValue(),
                Preferences->_CanvasPanelWeight, 0.001f) &&
            FMath::IsNearlyEqual(ReconstructedSplitter->SlotAt(2).GetSizeValue(),
                Preferences->_RightPanelWeight, 0.001f));
    }
    if (NOT TestTrue(TEXT("Production viewer displays asset and GPU textures"),
        Viewer->Set_Asset(*Asset) && static_cast<bool>(Viewer->Get_Resources())))
    {
        return false;
    }
    Tick(Slate);
    TestEqual(TEXT("Initial frame"), Viewer->Get_SelectedFrame(), 0);
    const auto InitialPrepared = Viewer->Get_Prepared();
    TestTrue(TEXT("Selected animation has prepared timing"), InitialPrepared.IsValid());
    const auto State = Viewer->Get_State();
    const auto ZoomButton = FindButton(Viewer.ToSharedRef(), TEXT("4x"));
    const auto GridButton = FindButton(Viewer.ToSharedRef(), TEXT("Grid on"));
    TestTrue(TEXT("Mounted zoom control"), ZoomButton.IsValid());
    TestTrue(TEXT("Mounted grid control"), GridButton.IsValid());
    TestTrue(TEXT("Mounted playback control"), FindButton(Viewer.ToSharedRef(), TEXT("Play / Pause")).IsValid());
    TestTrue(TEXT("Mounted timeline and asset detail"), ContainsText(Viewer.ToSharedRef(), TEXT("Asset / animations"))
        && ContainsText(Viewer.ToSharedRef(), TEXT("Frame timeline • click a thumbnail to scrub")));
    TestTrue(TEXT("Advanced controls exist"), FindButton(Viewer.ToSharedRef(), TEXT("Onion off")).IsValid()
        && FindButton(Viewer.ToSharedRef(), TEXT("Compare off")).IsValid()
        && FindButton(Viewer.ToSharedRef(), TEXT("Export frame")).IsValid());
    if (ZoomButton.IsValid())
    {
        TestTrue(TEXT("Physical zoom button routes input"),
            Click(Slate, HostWindow.ToSharedRef(), ZoomButton.ToSharedRef()));
        TestEqual(TEXT("Zoom button changes production view"), State->_Zoom, 4.0);
    }
    if (GridButton.IsValid())
    {
        TestTrue(TEXT("Physical grid button routes input"),
            Click(Slate, HostWindow.ToSharedRef(), GridButton.ToSharedRef()));
        TestFalse(TEXT("Grid button changes production view"), State->_ShowGrid);
    }
    TestTrue(TEXT("Pan changes production view"), State->Pan_By(FVector2D{48.0, 24.0}));
    State->_ShowGrid = true;
    State->_ShowAxes = true;
    State->_ShowCrosshair = true;
    TestTrue(TEXT("Absolute timeline seek accepted"), Viewer->Seek_Milliseconds(250.0));
    Tick(Slate);
    TestEqual(TEXT("Timeline seeks to second authored idle frame"), Viewer->Get_SelectedFrame(), 1);
    TestTrue(TEXT("Return to first authored frame"), Viewer->Seek_Milliseconds(0.0));
    TestTrue(TEXT("Seek and tick reuse prepared timing"), Viewer->Get_Prepared() == InitialPrepared);
    if (const auto OnionButton = FindButton(Viewer.ToSharedRef(), TEXT("Onion off")))
    {
        TestTrue(TEXT("Physical onion toggle routes input"), Click(Slate, HostWindow.ToSharedRef(), OnionButton.ToSharedRef()));
    }
    if (const auto CompareButton = FindButton(Viewer.ToSharedRef(), TEXT("Compare off")))
    {
        TestTrue(TEXT("Physical comparison toggle routes input"), Click(Slate, HostWindow.ToSharedRef(), CompareButton.ToSharedRef()));
    }
    TestTrue(TEXT("Character comparison counts changed source pixels"),
        Viewer->Get_ChangedPixels() > 0);
    const auto Canvas = Viewer->Get_Canvas();
    TestTrue(TEXT("Set zoom before centering"), State->Set_Zoom(4.0));
    TestTrue(TEXT("Pan before centering"), State->Pan_By(FVector2D{35.0, -20.0}));
    if (const auto CenterButton = FindButton(Viewer.ToSharedRef(), TEXT("Center")))
    {
        TestTrue(TEXT("Physical Center control routes input"),
            Click(Slate, HostWindow.ToSharedRef(), CenterButton.ToSharedRef()));
        const auto CenterSize = Canvas->GetCachedGeometry().GetLocalSize();
        const auto ExpectedPan = (CenterSize - FVector2D{16.0 * 4.0 * 2.0 + 24.0,
            16.0 * 4.0}) * 0.5;
        TestTrue(TEXT("Center preserves zoom and centers both comparison images"),
            State->_Zoom == 4.0 && State->_Pan.Equals(ExpectedPan, 1.0));
        const auto BeforeInvalid = State->_Pan;
        State->_Zoom = std::numeric_limits<double>::max();
        TestFalse(TEXT("Center rejects an overflowing extent"), Canvas->Center_View());
        TestTrue(TEXT("Rejected center leaves pan and zoom unchanged"),
            State->_Pan.Equals(BeforeInvalid) &&
            State->_Zoom == std::numeric_limits<double>::max());
        State->_Zoom = 4.0;
    }
    else
    {
        TestTrue(TEXT("Center control exists"), false);
    }
    if (const auto ResetButton = FindButton(Viewer.ToSharedRef(), TEXT("Reset view")))
    {
        TestTrue(TEXT("Physical Reset control routes input"),
            Click(Slate, HostWindow.ToSharedRef(), ResetButton.ToSharedRef()));
        TestTrue(TEXT("Reset restores default zoom and pan"),
            State->_Zoom == 16.0 && State->_Pan.IsNearlyZero());
    }
    TestTrue(TEXT("Side by side fit accepted"), Canvas->Fit_View());
    Tick(Slate);
    const auto CanvasSize = Canvas->GetCachedGeometry().GetLocalSize();
    TestTrue(TEXT("Both comparison sprites fit inside canvas"),
        State->_Pan.X >= 0.0 && State->_Pan.Y >= 0.0 &&
        State->_Pan.X + 32.0 * State->_Zoom + 24.0 <= CanvasSize.X + 1.0 &&
        State->_Pan.Y + 16.0 * State->_Zoom <= CanvasSize.Y + 1.0);
    const auto Output = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteAcceptance/Viewer-Advanced-Character.png"));
    auto Pixels = TArray<FColor>{};
    auto Size = FIntVector::ZeroValue;
    TestTrue(TEXT("Real Slate renderer captures pixel viewer PNG"),
        Capture(Slate, Viewer.ToSharedRef(), Output, Pixels, Size));
    if (Size.X > 0 && Size.Y > 0 && Pixels.Num() >= Size.X * Size.Y)
    {
        const auto CanvasOrigin = Viewer->GetCachedGeometry().AbsoluteToLocal(
            Canvas->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector));
        const auto Sample = [&](int32 InX, int32 InY) -> FColor
        {
            const auto Local = CanvasOrigin + State->_Pan +
                FVector2D{(InX + 0.5) * State->_Zoom, (InY + 0.5) * State->_Zoom};
            const auto Scale = FVector2D{Size.X / Viewer->GetCachedGeometry().GetLocalSize().X,
                Size.Y / Viewer->GetCachedGeometry().GetLocalSize().Y};
            const auto X = FMath::RoundToInt(Local.X * Scale.X);
            const auto Y = FMath::RoundToInt(Local.Y * Scale.Y);
            return X >= 0 && Y >= 0 && X < Size.X && Y < Size.Y
                ? Pixels[Y * Size.X + X] : FColor::Transparent;
        };
        const auto Head = Sample(7, 0);
        const auto Face = Sample(7, 3);
        TestTrue(TEXT("Rendered Character head retains authored yellow"),
            FMath::Abs(int32{Head.R} - 255) <= 8 &&
            FMath::Abs(int32{Head.G} - 227) <= 8 &&
            FMath::Abs(int32{Head.B} - 122) <= 8);
        TestTrue(TEXT("Rendered Character face retains authored cyan"),
            FMath::Abs(int32{Face.R} - 100) <= 8 &&
            FMath::Abs(int32{Face.G} - 223) <= 8 &&
            FMath::Abs(int32{Face.B} - 217) <= 8);
    }
    AddInfo(FString::Printf(TEXT("Pixel viewer screenshot: %s"), *Output));
    const auto CanvasWidth = Canvas->GetCachedGeometry().GetLocalSize().X;
    const auto BleedTargetX = CanvasWidth + 12.0;
    TestTrue(TEXT("Pan sprite beyond preview boundary for clipping proof"),
        State->Pan_By(FVector2D{BleedTargetX - 7.5 * State->_Zoom - State->_Pan.X, 0.0}));
    Tick(Slate);
    const auto ClipOutput = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteAcceptance/Viewer-Advanced-Clip.png"));
    auto ClipPixels = TArray<FColor>{};
    auto ClipSize = FIntVector::ZeroValue;
    if (TestTrue(TEXT("Clipping proof captures real Slate renderer"),
        Capture(Slate, Viewer.ToSharedRef(), ClipOutput, ClipPixels, ClipSize)) &&
        ClipSize.X > 0 && ClipSize.Y > 0)
    {
        const auto CanvasOrigin = Viewer->GetCachedGeometry().AbsoluteToLocal(
            Canvas->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector));
        const auto Scale = FVector2D{ClipSize.X / Viewer->GetCachedGeometry().GetLocalSize().X,
            ClipSize.Y / Viewer->GetCachedGeometry().GetLocalSize().Y};
        const auto X = FMath::RoundToInt((CanvasOrigin.X + BleedTargetX) * Scale.X);
        const auto Y = FMath::RoundToInt((CanvasOrigin.Y + State->_Pan.Y + 3.5 * State->_Zoom) * Scale.Y);
        if (TestTrue(TEXT("Clipping sample lies inside the sidebar image"),
            X >= 0 && Y >= 0 && X < ClipSize.X && Y < ClipSize.Y &&
            ClipPixels.Num() >= ClipSize.X * ClipSize.Y))
        {
            const auto Color = ClipPixels[Y * ClipSize.X + X];
            TestFalse(TEXT("Authored cyan does not bleed into right sidebar"),
                FMath::Abs(int32{Color.R} - 100) <= 8 &&
                FMath::Abs(int32{Color.G} - 223) <= 8 &&
                FMath::Abs(int32{Color.B} - 217) <= 8);
        }
    }
    AddInfo(FString::Printf(TEXT("Pixel viewer clipping screenshot: %s"), *ClipOutput));
    if (const auto PlaybackButton = FindButton(Viewer.ToSharedRef(), TEXT("Loop")))
    {
        TestTrue(TEXT("Physical playback control changes prepared mode"),
            Click(Slate, HostWindow.ToSharedRef(), PlaybackButton.ToSharedRef()));
        TestTrue(TEXT("Playback mode receives fresh prepared timing"),
            Viewer->Get_Prepared().IsValid() && Viewer->Get_Prepared() != InitialPrepared);
    }
    const auto Original = Asset->Get_Snapshot();
    if (TestTrue(TEXT("Imported Character snapshot remains valid"), Original.IsValid()))
    {
        auto Reordered = *Original;
        if (TestTrue(TEXT("Character has multiple named animations"), Reordered._Animations.Num() > 1))
        {
            Reordered._Animations.Swap(0, 1);
            auto Diagnostic = FCk_PixelSprite_Diagnostic{};
            if (TestTrue(TEXT("Reordered revision accepted"),
                Asset->Request_ReplaceManifest(Reordered, Diagnostic)))
            {
                Tick(Slate);
                TestEqual(TEXT("Selected animation survives reordered asset revision"),
                    Viewer->Get_SelectedAnimationName(), FString{TEXT("idle")});
                TestTrue(TEXT("Asset revision receives fresh prepared timing"),
                    Viewer->Get_Prepared().IsValid() && Viewer->Get_Prepared() != InitialPrepared);
            }
            auto Hierarchical = Reordered;
            const auto SourceIndex = Hierarchical._Animations[0]._Frames[0]._SourceIndex;
            const auto RelativeSource = FString{TEXT("Idle/Inspect/tree_target.xpm")};
            Hierarchical._Sources[SourceIndex]._Path = RelativeSource;
            for (auto& Animation : Hierarchical._Animations)
            {
                for (auto& Frame : Animation._Frames)
                {
                    if (Frame._SourceIndex == SourceIndex)
                    {
                        Frame._Source = RelativeSource;
                    }
                }
            }
            auto Unreferenced = Hierarchical._Sources[SourceIndex];
            Unreferenced._Path = TEXT("Unused/unreferenced.xpm");
            Hierarchical._Sources.Add(MoveTemp(Unreferenced));
            auto HierarchyDiagnostic = FCk_PixelSprite_Diagnostic{};
            if (TestTrue(TEXT("Hierarchical source revision accepted"),
                Asset->Request_ReplaceManifest(Hierarchical, HierarchyDiagnostic)))
            {
                Tick(Slate);
                const auto SourceTree = FindTreeView(Viewer.ToSharedRef());
                TestTrue(TEXT("Mounted source tree is available for navigation"),
                    SourceTree.IsValid());
                TestTrue(TEXT("Relative source folders are navigable"),
                    SourceTree.IsValid() &&
                    FindButton(SourceTree.ToSharedRef(), TEXT("Idle")).IsValid() &&
                    FindButton(SourceTree.ToSharedRef(), TEXT("Inspect")).IsValid());
                const auto Folder = SourceTree.IsValid()
                    ? FindButton(SourceTree.ToSharedRef(), TEXT("Idle")) : TSharedPtr<SButton>{};
                if (Folder.IsValid())
                {
                    TestTrue(TEXT("Physical source folder collapses"),
                        Click(Slate, HostWindow.ToSharedRef(), Folder.ToSharedRef()));
                    const auto RetainedLeaf = FindButton(SourceTree.ToSharedRef(),
                        TEXT("tree_target.xpm"));
                    if (RetainedLeaf.IsValid())
                    {
                        const auto BeforeName = Viewer->Get_SelectedAnimationName();
                        const auto BeforeFrame = Viewer->Get_SelectedFrame();
                        TestFalse(TEXT("Collapsed folder blocks physical source leaf click"),
                            Click(Slate, HostWindow.ToSharedRef(), RetainedLeaf.ToSharedRef()));
                        TestTrue(TEXT("Collapsed folder leaves selection unchanged"),
                            Viewer->Get_SelectedAnimationName() == BeforeName &&
                            Viewer->Get_SelectedFrame() == BeforeFrame);
                    }
                    TestTrue(TEXT("Physical source folder expands"),
                        Click(Slate, HostWindow.ToSharedRef(), Folder.ToSharedRef()));
                }
                const auto Leaf = SourceTree.IsValid()
                    ? FindButton(SourceTree.ToSharedRef(), TEXT("tree_target.xpm")) : TSharedPtr<SButton>{};
                if (Leaf.IsValid())
                {
                    TestTrue(TEXT("Physical source leaf click routes selection"),
                        Click(Slate, HostWindow.ToSharedRef(), Leaf.ToSharedRef()));
                    TestEqual(TEXT("Source click selects first referencing animation"),
                        Viewer->Get_SelectedAnimationName(), Hierarchical._Animations[0]._Name);
                    TestEqual(TEXT("Source click selects exact referencing frame"),
                        Viewer->Get_SelectedFrame(), 0);
                    const auto BeforeName = Viewer->Get_SelectedAnimationName();
                    const auto BeforeFrame = Viewer->Get_SelectedFrame();
                    if (SourceTree.IsValid())
                    {
                        SourceTree->SetScrollOffset(1000.0f);
                        Tick(Slate);
                    }
                    const auto UnusedLeaf = SourceTree.IsValid()
                        ? FindButton(SourceTree.ToSharedRef(), TEXT("unreferenced.xpm"))
                        : TSharedPtr<SButton>{};
                    if (UnusedLeaf.IsValid())
                    {
                        TestTrue(TEXT("Physical unreferenced source click is handled"),
                            Click(Slate, HostWindow.ToSharedRef(), UnusedLeaf.ToSharedRef()));
                        TestTrue(TEXT("Unreferenced source leaves selection unchanged"),
                            Viewer->Get_SelectedAnimationName() == BeforeName &&
                            Viewer->Get_SelectedFrame() == BeforeFrame);
                    }
                    else
                    {
                        TestTrue(TEXT("Unreferenced source leaf is visible"), false);
                    }
                    if (SourceTree.IsValid())
                    {
                        SourceTree->SetScrollOffset(0.0f);
                        Tick(Slate);
                    }
                    TestTrue(TEXT("Fit hierarchical source view"), Canvas->Fit_View());
                    Tick(Slate);
                    const auto HierarchyOutput = FPaths::Combine(FPaths::ProjectSavedDir(),
                        TEXT("PixelSpriteAcceptance/Viewer-SourceHierarchy.png"));
                    auto HierarchyPixels = TArray<FColor>{};
                    auto HierarchySize = FIntVector::ZeroValue;
                    TestTrue(TEXT("Source hierarchy captures real Slate renderer"),
                        Capture(Slate, Viewer.ToSharedRef(), HierarchyOutput,
                            HierarchyPixels, HierarchySize));
                    AddInfo(FString::Printf(TEXT("Pixel viewer source hierarchy screenshot: %s"),
                        *HierarchyOutput));
                }
                else
                {
                    TestTrue(TEXT("Hierarchical source leaf is visible"), false);
                }
            }
        }
    }
    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    return true;
}

#endif
