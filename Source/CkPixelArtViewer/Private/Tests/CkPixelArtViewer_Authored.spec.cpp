#include "../../CkPixelArtViewer_Module.h"

#include "CkPixelArtViewer/CkPixelArtViewer_Window.h"
#include "CkPixelArtViewer/CkPixelArtViewer_Canvas.h"
#include "CkPixelArtViewer/CkPixelArtViewer_Timeline.h"
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
#include "Widgets/Input/SComboButton.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Layout/SScrollBox.h"
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
        HasToolTipText(
            const TSharedRef<SWidget>& InWidget,
            const FString& InText
        )
        -> bool
    {
        const TSharedPtr<IToolTip> ToolTip = InWidget->GetToolTip();
        if (NOT ToolTip.IsValid())
        {
            return false;
        }
        const auto Content = ToolTip->GetContentWidget();
        Content->SlatePrepass();
        return Content->GetAccessibleText().ToString().Equals(InText, ESearchCase::CaseSensitive);
    }

    auto
        FindButton(
            const TSharedRef<SWidget>& InRoot,
            const FString& InText
        )
        -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton")
            && (ContainsText(InRoot, InText)
                || HasToolTipText(InRoot, InText)))
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
        FindComboButton(
            const TSharedRef<SWidget>& InRoot,
            const FString& InText
        )
        -> TSharedPtr<SComboButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SComboButton")
            && (ContainsText(InRoot, InText)
                || HasToolTipText(InRoot, InText)))
        {
            return StaticCastSharedRef<SComboButton>(InRoot);
        }
        const auto& Children = *InRoot->GetChildren();
        for (auto Index = int32{0}; Index < Children.Num(); ++Index)
        {
            const auto Found = FindComboButton(ConstCastSharedRef<SWidget>(Children.GetChildAt(Index)), InText);
            if (Found.IsValid())
            {
                return Found;
            }
        }
        return {};
    }

    auto
        FindMenuButtons(
            const TSharedRef<SWidget>& InRoot,
            const FString& InText,
            TArray<TSharedRef<SWidget>>& OutButtons
        )
        -> void
    {
        const auto Type = InRoot->GetTypeAsString();
        if ((Type == TEXT("SMenuEntryButton") || Type == TEXT("SButton"))
            && ContainsText(InRoot, InText))
        {
            OutButtons.Add(InRoot);
            return;
        }
        const auto& Children = *InRoot->GetChildren();
        for (auto Index = int32{0}; Index < Children.Num(); ++Index)
        {
            FindMenuButtons(ConstCastSharedRef<SWidget>(Children.GetChildAt(Index)), InText, OutButtons);
        }
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
            const TSharedRef<SWindow>&,
            const TSharedRef<SWidget>& InButton
        )
        -> bool
    {
        const auto ActualWindow = InSlate.FindWidgetWindow(InButton);
        if (NOT ActualWindow.IsValid() || NOT ActualWindow->GetNativeWindow().IsValid())
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
        auto VisibleWindows = TArray<TSharedRef<SWindow>>{};
        InSlate.GetAllVisibleWindowsOrdered(VisibleWindows);
        const auto Target = InSlate.LocateWindowUnderMouse(Position,
            VisibleWindows, false, 0);
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
            ActualWindow->GetNativeWindow(), Down);
        const auto UpHandled = InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return DownHandled && UpHandled;
    }

    auto
        OpenMenu(
            FSlateApplication& InSlate,
            const TSharedRef<SWindow>& InHostWindow,
            const TSharedRef<SComboButton>& InMenu
        )
        -> bool
    {
        const auto Clicked = Click(InSlate, InHostWindow, InMenu);
        const auto Open = InMenu->IsOpen();
        const auto Popup = InMenu->GetMenuWindow();
        if (NOT Clicked || NOT Open || NOT Popup.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Pixel viewer combo did not open: clicked=%d open=%d popup=%d"),
                Clicked, Open, Popup.IsValid());
        }
        return Clicked && Open && Popup.IsValid();
    }

    auto
        ClickMenuButton(
            FSlateApplication& InSlate,
            const TSharedRef<SWindow>& InHostWindow,
            const FString& InText
        )
        -> bool
    {
        auto Windows = TArray<TSharedRef<SWindow>>{};
        InSlate.GetAllVisibleWindowsOrdered(Windows);
        auto CandidateCount = int32{0};
        auto SizedCount = int32{0};
        for (const auto IncludeHost : {false, true})
        {
            for (const auto& Window : Windows)
            {
                if ((Window == InHostWindow) != IncludeHost)
                {
                    continue;
                }
                auto Buttons = TArray<TSharedRef<SWidget>>{};
                FindMenuButtons(Window, InText, Buttons);
                CandidateCount += Buttons.Num();
                for (const auto& Button : Buttons)
                {
                    const auto Size = Button->GetCachedGeometry().GetLocalSize();
                    if (Size.X <= 0.0 || Size.Y <= 0.0)
                    {
                        continue;
                    }
                    ++SizedCount;
                    if (Click(InSlate, InHostWindow, Button))
                    {
                        return true;
                    }
                }
            }
        }
        UE_LOG(LogTemp, Error, TEXT("Pixel viewer menu entry '%s' was not clicked: %d windows, %d matching buttons, %d with geometry"),
            *InText, Windows.Num(), CandidateCount, SizedCount);
        return false;
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

    auto
        Scrub_Timeline(
            FSlateApplication& InSlate,
            const TSharedRef<SCk_PixelArtViewer_Timeline>& InTimeline,
            double InStartX,
            double InEndX
        )
        -> bool
    {
        const auto Window = InSlate.FindWidgetWindow(InTimeline);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid())
        {
            return false;
        }
        const auto Geometry = InTimeline->GetCachedGeometry();
        const auto Size = Geometry.GetLocalSize();
        if (Size.X <= 0.0 || Size.Y <= 0.0 ||
            InStartX < 0.0 || InStartX >= Size.X ||
            InEndX < 0.0 || InEndX >= Size.X)
        {
            return false;
        }
        const auto Start = Geometry.LocalToAbsolute(FVector2D{InStartX, Size.Y * 0.5});
        const auto End = Geometry.LocalToAbsolute(FVector2D{InEndX, Size.Y * 0.5});
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
        auto VisibleWindows = TArray<TSharedRef<SWindow>>{};
        InSlate.GetAllVisibleWindowsOrdered(VisibleWindows);
        const auto Target = InSlate.LocateWindowUnderMouse(Start, VisibleWindows, false, 0);
        auto Found = false;
        for (auto Index = 0; Index < Target.Widgets.Num(); ++Index)
        {
            Found |= Target.Widgets[Index].Widget == InTimeline;
        }
        if (NOT Found)
        {
            return false;
        }
        const auto DownHandled = InSlate.ProcessMouseButtonDownEvent(
            Window->GetNativeWindow(), Down);
        InSlate.SetCursorPos(End);
        InSlate.ProcessMouseMoveEvent(Move, false);
        const auto UpHandled = InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return DownHandled && UpHandled;
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
    const auto OldLayout = Preferences->_Layout;
    Preferences->_ShowGrid = true;
    Preferences->_ShowOnion = false;
    Preferences->_RenderMode = ECk_PixelSprite_RenderMode::Nearest;
    Preferences->_RenderScale = 1;
    Preferences->_LeftPanelWeight = std::numeric_limits<float>::quiet_NaN();
    Preferences->_CanvasPanelWeight = 0.0f;
    Preferences->_RightPanelWeight = 1.0f;
    Preferences->_Layout = ECk_PixelArtViewer_Layout::Studio;
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
        Preferences->_Layout = OldLayout;
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
    const auto ZoomMenu = FindComboButton(Viewer.ToSharedRef(), TEXT("Zoom"));
    const auto ShowMenu = FindComboButton(Viewer.ToSharedRef(), TEXT("Show"));
    TestTrue(TEXT("Mounted zoom menu"), ZoomMenu.IsValid());
    TestTrue(TEXT("Mounted visibility menu"), ShowMenu.IsValid());
    TestTrue(TEXT("Mounted playback control"), FindButton(Viewer.ToSharedRef(), TEXT("Play / Pause")).IsValid());
    TestTrue(TEXT("Mounted timeline and animation navigation"),
        ContainsText(Viewer.ToSharedRef(), TEXT("Asset Browser"))
        && ContainsText(Viewer.ToSharedRef(), TEXT("Timeline")));
    TestTrue(TEXT("Advanced controls exist"), FindButton(Viewer.ToSharedRef(), TEXT("Onion skin")).IsValid()
        && FindComboButton(Viewer.ToSharedRef(), TEXT("Comparison mode")).IsValid()
        && FindComboButton(Viewer.ToSharedRef(), TEXT("Export")).IsValid());
    if (ZoomMenu.IsValid())
    {
        TestTrue(TEXT("Physical zoom menu opens"),
            OpenMenu(Slate, HostWindow.ToSharedRef(), ZoomMenu.ToSharedRef()));
        TestTrue(TEXT("Physical 4x menu option routes input"),
            ClickMenuButton(Slate, HostWindow.ToSharedRef(), TEXT("4x")));
        TestEqual(TEXT("Zoom menu changes production view"), State->_Zoom, 4.0);
    }
    if (ShowMenu.IsValid())
    {
        TestTrue(TEXT("Physical visibility menu opens"),
            OpenMenu(Slate, HostWindow.ToSharedRef(), ShowMenu.ToSharedRef()));
        TestTrue(TEXT("Physical grid menu option routes input"),
            ClickMenuButton(Slate, HostWindow.ToSharedRef(), TEXT("Grid")));
        TestFalse(TEXT("Grid menu changes production view"), State->_ShowGrid);
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
    if (const auto OnionButton = FindButton(Viewer.ToSharedRef(), TEXT("Onion skin")))
    {
        TestTrue(TEXT("Physical onion toggle routes input"), Click(Slate, HostWindow.ToSharedRef(), OnionButton.ToSharedRef()));
    }
    if (const auto CompareMenu = FindComboButton(Viewer.ToSharedRef(), TEXT("Comparison mode")))
    {
        TestTrue(TEXT("Physical comparison menu opens"),
            OpenMenu(Slate, HostWindow.ToSharedRef(), CompareMenu.ToSharedRef()));
        TestTrue(TEXT("Physical side-by-side comparison option routes input"),
            ClickMenuButton(Slate, HostWindow.ToSharedRef(), TEXT("Side by side")));
    }
    TestTrue(TEXT("Character comparison counts changed source pixels"),
        Viewer->Get_ChangedPixels() > 0);
    const auto Canvas = Viewer->Get_Canvas();
    TestTrue(TEXT("Set zoom before centering"), State->Set_Zoom(4.0));
    TestTrue(TEXT("Pan before centering"), State->Pan_By(FVector2D{35.0, -20.0}));
    if (const auto CenterMenu = FindComboButton(Viewer.ToSharedRef(), TEXT("Zoom")))
    {
        TestTrue(TEXT("Physical Zoom menu opens for Center"),
            OpenMenu(Slate, HostWindow.ToSharedRef(), CenterMenu.ToSharedRef()));
        TestTrue(TEXT("Physical Center menu option routes input"),
            ClickMenuButton(Slate, HostWindow.ToSharedRef(), TEXT("Center")));
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
        TestTrue(TEXT("Center menu exists"), false);
    }
    if (const auto ResetMenu = FindComboButton(Viewer.ToSharedRef(), TEXT("Zoom")))
    {
        TestTrue(TEXT("Physical Zoom menu opens for Reset"),
            OpenMenu(Slate, HostWindow.ToSharedRef(), ResetMenu.ToSharedRef()));
        TestTrue(TEXT("Physical Reset menu option routes input"),
            ClickMenuButton(Slate, HostWindow.ToSharedRef(), TEXT("Reset view")));
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
    if (const auto PlaybackMenu = FindComboButton(Viewer.ToSharedRef(), TEXT("Playback mode")))
    {
        TestTrue(TEXT("Physical playback menu opens"),
            OpenMenu(Slate, HostWindow.ToSharedRef(), PlaybackMenu.ToSharedRef()));
        TestTrue(TEXT("Physical Once option changes prepared mode"),
            ClickMenuButton(Slate, HostWindow.ToSharedRef(), TEXT("Once")));
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
                const auto SourcesTab = FindButton(Viewer.ToSharedRef(), TEXT("Sources"));
                TestTrue(TEXT("Mounted source tab exists"), SourcesTab.IsValid());
                if (SourcesTab.IsValid())
                {
                    TestTrue(TEXT("Physical source tab opens source navigation"),
                        Click(Slate, HostWindow.ToSharedRef(), SourcesTab.ToSharedRef()));
                }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkPixelArtViewer_LayoutSwitch,
    "Ck.PixelArtViewer.LayoutSwitch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

auto
    FCkPixelArtViewer_LayoutSwitch::
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
    if (NOT TestTrue(TEXT("Character fixture plugin exists"), Plugin.IsValid()))
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
    if (NOT TestTrue(TEXT("Character imports through production factory"), Asset.IsValid()))
    {
        return false;
    }

    auto Preferences = TObjectPtr<UCkPixelArtViewerSettings>{
        GetMutableDefault<UCkPixelArtViewerSettings>()};
    const auto OldLayout = Preferences->_Layout;
    Preferences->_Layout = ECk_PixelArtViewer_Layout::Studio;
    ON_SCOPE_EXIT
    {
        Preferences->_Layout = OldLayout;
        Preferences->SaveConfig();
    };

    auto& Slate = FSlateApplication::Get();
    auto Viewer = SNew(SCk_PixelArtViewer_Window);
    auto HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1280.0f, 800.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Viewer];
    ON_SCOPE_EXIT
    {
        Slate.DestroyWindowImmediately(HostWindow);
    };
    Slate.AddWindow(HostWindow, true);
    Tick(Slate);
    if (NOT TestTrue(TEXT("Mounted Studio viewer accepts Character"), Viewer->Set_Asset(*Asset)))
    {
        return false;
    }
    TestTrue(TEXT("Seek second authored frame"), Viewer->Seek_Milliseconds(250.0));
    Tick(Slate);
    TestEqual(TEXT("Studio frame before switch"), Viewer->Get_SelectedFrame(), 1);
    TestEqual(TEXT("Studio layout starts from isolated preference"),
        Viewer->Get_Layout(), ECk_PixelArtViewer_Layout::Studio);
    const auto State = Viewer->Get_State();
    const auto Prepared = Viewer->Get_Prepared();
    const auto Resources = Viewer->Get_Resources();
    if (NOT TestTrue(TEXT("Studio keeps state and prepared frame resources"),
        State.IsValid() && Prepared.IsValid() && static_cast<bool>(Resources)))
    {
        return false;
    }
    TestTrue(TEXT("Studio canvas fits before capture"), Viewer->Get_Canvas()->Fit_View());
    Tick(Slate);
    auto Pixels = TArray<FColor>{};
    auto Size = FIntVector::ZeroValue;
    const auto StudioOutput = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteAcceptance/Viewer-Studio.png"));
    TestTrue(TEXT("Studio captures the mounted Slate renderer"),
        Capture(Slate, Viewer, StudioOutput, Pixels, Size));
    AddInfo(FString::Printf(TEXT("Studio viewer screenshot: %s"), *StudioOutput));

    const auto LayoutMenu = FindComboButton(Viewer, TEXT("Viewer layout"));
    if (TestTrue(TEXT("Mounted layout menu exists"), LayoutMenu.IsValid()))
    {
        TestTrue(TEXT("Physical layout menu opens"),
            OpenMenu(Slate, HostWindow, LayoutMenu.ToSharedRef()));
        TestTrue(TEXT("Physical Focused menu item switches layout"),
            ClickMenuButton(Slate, HostWindow, TEXT("Focused")));
    }
    TestEqual(TEXT("Focused layout selected"),
        Viewer->Get_Layout(), ECk_PixelArtViewer_Layout::Focused);
    TestEqual(TEXT("Focused preference persists"),
        Preferences->_Layout, ECk_PixelArtViewer_Layout::Focused);
    TestTrue(TEXT("Focused switch keeps state and immutable resources"),
        Viewer->Get_State() == State && Viewer->Get_Prepared() == Prepared
        && Viewer->Get_Resources() == Resources && Viewer->Get_Asset() == Asset.Get());
    TestEqual(TEXT("Focused switch keeps paused time"), State->_TimeMilliseconds, 250.0);
    TestFalse(TEXT("Focused switch remains paused"), State->_Playing);
    TestEqual(TEXT("Focused switch keeps authored frame"), Viewer->Get_SelectedFrame(), 1);
    TestTrue(TEXT("Focused workspace starts with only the canvas"),
        Viewer->Get_MainSplitter().IsValid()
        && Viewer->Get_MainSplitter()->GetChildren()->Num() == 1);
    if (const auto InspectorButton = FindButton(Viewer, TEXT("Inspector")))
    {
        TestTrue(TEXT("Physical Inspector control opens a second panel"),
            Click(Slate, HostWindow, InspectorButton.ToSharedRef())
            && Viewer->Get_MainSplitter()->GetChildren()->Num() == 2);
        TestTrue(TEXT("Inspector toggle keeps presentation state"),
            Viewer->Get_State() == State && Viewer->Get_Prepared() == Prepared
            && State->_TimeMilliseconds == 250.0 && Viewer->Get_SelectedFrame() == 1);
        TestTrue(TEXT("Physical Inspector control restores canvas focus"),
            Click(Slate, HostWindow, InspectorButton.ToSharedRef())
            && Viewer->Get_MainSplitter()->GetChildren()->Num() == 1);
    }
    else
    {
        TestTrue(TEXT("Mounted Inspector control exists"), false);
    }
    TestTrue(TEXT("Focused canvas fits before capture"), Viewer->Get_Canvas()->Fit_View());
    Tick(Slate);
    const auto FocusedOutput = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteAcceptance/Viewer-Focused.png"));
    TestTrue(TEXT("Focused captures the mounted Slate renderer"),
        Capture(Slate, Viewer, FocusedOutput, Pixels, Size));
    AddInfo(FString::Printf(TEXT("Focused viewer screenshot: %s"), *FocusedOutput));

    const auto StudioMenu = FindComboButton(Viewer, TEXT("Viewer layout"));
    if (TestTrue(TEXT("Studio layout menu survives rebuild"), StudioMenu.IsValid()))
    {
        TestTrue(TEXT("Physical Studio layout menu opens"),
            OpenMenu(Slate, HostWindow, StudioMenu.ToSharedRef()));
        TestTrue(TEXT("Physical Studio menu item switches layout"),
            ClickMenuButton(Slate, HostWindow, TEXT("Studio")));
    }
    TestEqual(TEXT("Studio layout restored"),
        Viewer->Get_Layout(), ECk_PixelArtViewer_Layout::Studio);
    TestTrue(TEXT("Studio return restores three resizable panels"),
        Viewer->Get_MainSplitter().IsValid()
        && Viewer->Get_MainSplitter()->GetChildren()->Num() == 3);
    TestTrue(TEXT("Studio return keeps state and immutable resources"),
        Viewer->Get_State() == State && Viewer->Get_Prepared() == Prepared
        && Viewer->Get_Resources() == Resources);
    TestEqual(TEXT("Studio return keeps paused time"), State->_TimeMilliseconds, 250.0);
    TestFalse(TEXT("Studio return remains paused"), State->_Playing);
    TestEqual(TEXT("Studio return keeps authored frame"), Viewer->Get_SelectedFrame(), 1);

    State->_Playing = true;
    Viewer->Set_Layout(ECk_PixelArtViewer_Layout::Focused);
    TestEqual(TEXT("Programmatic switch selects Focused"),
        Viewer->Get_Layout(), ECk_PixelArtViewer_Layout::Focused);
    TestEqual(TEXT("Programmatic Focused switch retains playback time"),
        State->_TimeMilliseconds, 250.0);
    TestTrue(TEXT("Programmatic Focused switch retains play state"), State->_Playing);
    TestEqual(TEXT("Programmatic Focused switch retains frame"), Viewer->Get_SelectedFrame(), 1);
    TestTrue(TEXT("Programmatic Focused switch retains state and timing"),
        Viewer->Get_State() == State && Viewer->Get_Prepared() == Prepared);
    State->_Playing = false;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkPixelArtViewer_Sproutling,
    "Ck.PixelArtViewer.SproutlingExample",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

auto
    FCkPixelArtViewer_Sproutling::
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
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        LoadObject<UCk_PixelSpriteAsset_UE>(nullptr,
            TEXT("/Game/PixelSpriteExamples/Sproutling.Sproutling"))};
    if (NOT TestTrue(TEXT("Native Sproutling example loads"), Asset.IsValid()))
    {
        return false;
    }
    auto Preferences = TObjectPtr<UCkPixelArtViewerSettings>{
        GetMutableDefault<UCkPixelArtViewerSettings>()};
    const auto OldLayout = Preferences->_Layout;
    Preferences->_Layout = ECk_PixelArtViewer_Layout::Studio;
    ON_SCOPE_EXIT
    {
        Preferences->_Layout = OldLayout;
        Preferences->SaveConfig();
    };
    auto& Slate = FSlateApplication::Get();
    auto Viewer = SNew(SCk_PixelArtViewer_Window);
    const auto State = Viewer->Get_State();
    *State = FCk_PixelArtViewer_State{};
    auto HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1280.0f, 800.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Viewer];
    ON_SCOPE_EXIT
    {
        Slate.DestroyWindowImmediately(HostWindow);
    };
    Slate.AddWindow(HostWindow, true);
    Tick(Slate);
    if (NOT TestTrue(TEXT("Viewer accepts native eight-frame creature"), Viewer->Set_Asset(*Asset)))
    {
        return false;
    }
    Tick(Slate);
    TestTrue(TEXT("Creature fits the real canvas"), Viewer->Get_Canvas()->Fit_View());
    TestTrue(TEXT("Rest pose is selectable"), Viewer->Seek_Milliseconds(0.0));
    TestEqual(TEXT("Rest frame"), Viewer->Get_SelectedFrame(), 0);
    TestTrue(TEXT("Raised breath pose is selectable"), Viewer->Seek_Milliseconds(670.0));
    TestEqual(TEXT("Full breath frame"), Viewer->Get_SelectedFrame(), 2);
    if (const auto Next = FindButton(Viewer, TEXT("Next frame")))
    {
        TestTrue(TEXT("Creature steps through the mounted transport"), Click(Slate, HostWindow, Next.ToSharedRef()));
        TestEqual(TEXT("Half-blink frame follows full breath"), Viewer->Get_SelectedFrame(), 3);
    }
    else
    {
        TestTrue(TEXT("Mounted next-frame transport exists"), false);
    }
    TestTrue(TEXT("Blink pose is selectable"), Viewer->Seek_Milliseconds(980.0));
    TestEqual(TEXT("Closed-eye blink frame"), Viewer->Get_SelectedFrame(), 4);
    TestTrue(TEXT("Return to rest for presentation"), Viewer->Seek_Milliseconds(0.0));
    Tick(Slate);
    auto Pixels = TArray<FColor>{};
    auto Size = FIntVector::ZeroValue;
    const auto Output = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteAcceptance/Sproutling/Viewer-Sproutling.png"));
    TestTrue(TEXT("Native creature captures the real Slate renderer"), Capture(Slate, Viewer, Output, Pixels, Size));
    AddInfo(FString::Printf(TEXT("Sproutling native viewer screenshot: %s"), *Output));
    Viewer->Set_Layout(ECk_PixelArtViewer_Layout::Focused);
    TestEqual(TEXT("Sproutling Focused layout selected"),
        Viewer->Get_Layout(), ECk_PixelArtViewer_Layout::Focused);
    Tick(Slate);
    TestTrue(TEXT("Sproutling fits the Focused canvas"), Viewer->Get_Canvas()->Fit_View());
    Tick(Slate);
    const auto FocusedOutput = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteAcceptance/Sproutling/Viewer-Sproutling-Focused.png"));
    TestTrue(TEXT("Focused creature captures the real Slate renderer"),
        Capture(Slate, Viewer, FocusedOutput, Pixels, Size));
    AddInfo(FString::Printf(TEXT("Sproutling Focused viewer screenshot: %s"), *FocusedOutput));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkPixelArtViewer_TimeTrack,
    "Ck.PixelArtViewer.TimeTrack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

auto
    FCkPixelArtViewer_TimeTrack::
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
    if (NOT TestTrue(TEXT("Sproutling fixture plugin exists"), Plugin.IsValid()))
    {
        return false;
    }
    const auto Fixture = Plugin->GetBaseDir() /
        TEXT("Tools/CkPixel/Examples/Sproutling/sproutling.json");
    auto Factory = TStrongObjectPtr<UCk_PixelSpriteFactory_UE>{NewObject<UCk_PixelSpriteFactory_UE>()};
    auto Canceled = false;
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        Cast<UCk_PixelSpriteAsset_UE>(Factory->FactoryCreateFile(
            UCk_PixelSpriteAsset_UE::StaticClass(), GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UCk_PixelSpriteAsset_UE::StaticClass()),
            RF_Transient, Fixture, nullptr, GWarn, Canceled))};
    if (NOT TestTrue(TEXT("Production factory imports authored Sproutling"), Asset.IsValid()))
    {
        return false;
    }
    const auto Snapshot = Asset->Get_Snapshot();
    if (NOT TestTrue(TEXT("Imported Sproutling has its idle animation"),
        Snapshot.IsValid() && Snapshot->_Animations.Num() == 1 &&
        Snapshot->_Animations[0]._Frames.Num() == 8))
    {
        return false;
    }
    const auto& Animation = Snapshot->_Animations[0];
    const auto Durations = TArray<double>{450.0, 220.0, 220.0, 90.0,
        110.0, 220.0, 220.0, 450.0};
    for (auto Index = int32{0}; Index < Durations.Num(); ++Index)
    {
        TestEqual(FString::Printf(TEXT("Imported frame %d keeps its authored duration"), Index + 1),
            Animation._Frames[Index]._DurationMilliseconds, Durations[Index]);
    }

    auto Preferences = TObjectPtr<UCkPixelArtViewerSettings>{
        GetMutableDefault<UCkPixelArtViewerSettings>()};
    const auto OldLayout = Preferences->_Layout;
    Preferences->_Layout = ECk_PixelArtViewer_Layout::Studio;
    ON_SCOPE_EXIT
    {
        Preferences->_Layout = OldLayout;
        Preferences->SaveConfig();
    };
    auto& Slate = FSlateApplication::Get();
    auto Viewer = SNew(SCk_PixelArtViewer_Window);
    const auto State = Viewer->Get_State();
    *State = FCk_PixelArtViewer_State{};
    auto HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1280.0f, 800.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Viewer];
    ON_SCOPE_EXIT
    {
        Slate.DestroyWindowImmediately(HostWindow);
    };
    Slate.AddWindow(HostWindow, true);
    Tick(Slate);
    if (NOT TestTrue(TEXT("Mounted viewer accepts imported Sproutling"), Viewer->Set_Asset(*Asset)))
    {
        return false;
    }
    Tick(Slate);
    TestTrue(TEXT("Sproutling fits the canvas for timeline captures"),
        Viewer->Get_Canvas()->Fit_View());
    Tick(Slate);
    const auto Timeline = Viewer->Get_Timeline();
    const auto Scroll = Viewer->Get_TimelineScroll();
    if (NOT TestTrue(TEXT("Native time track and horizontal viewport are mounted"),
        Timeline.IsValid() && Scroll.IsValid()))
    {
        return false;
    }
    TestTrue(TEXT("Fit timeline zoom is selected"), FMath::IsNearlyEqual(Viewer->Get_TimelineZoom(), 1.0));
    const auto ViewportWidth = Scroll->GetCachedGeometry().GetLocalSize().X;
    const auto FitWidth = Timeline->Get_TrackWidth();
    TestTrue(TEXT("Fit track fills a measurable viewport without horizontal clipping"),
        ViewportWidth > 0.0 && FitWidth > 0.0 && FitWidth <= ViewportWidth + 2.0);
    TestTrue(TEXT("Frame blocks preserve the 450 to 90 millisecond ratio"),
        FMath::IsNearlyEqual(Timeline->Get_FrameWidth(0) /
            Timeline->Get_FrameWidth(3), 5.0f, 0.05f));
    TestTrue(TEXT("Authored short block is narrower than its fixed thumbnail"),
        Timeline->Get_FrameWidth(3) < 56.0f);
    TestTrue(TEXT("Timeline thumbnails keep a fixed 56 pixel square"),
        Timeline->Get_ThumbnailSize().Equals(FVector2D{56.0, 56.0}, 0.01));
    auto Pixels = TArray<FColor>{};
    auto Size = FIntVector::ZeroValue;
    const auto FitOutput = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteAcceptance/Sproutling-TimeTrack-Fit.png"));
    TestTrue(TEXT("Fit time track captures the mounted renderer"),
        Capture(Slate, Viewer, FitOutput, Pixels, Size));
    AddInfo(FString::Printf(TEXT("Sproutling fit time track screenshot: %s"), *FitOutput));

    const auto CanvasZoom = State->_Zoom;
    TestFalse(TEXT("Unsupported timeline zoom is rejected"), Viewer->Set_TimelineZoom(3.0));
    TestFalse(TEXT("Non-finite timeline zoom is rejected"),
        Viewer->Set_TimelineZoom(std::numeric_limits<double>::quiet_NaN()));
    TestTrue(TEXT("Unsupported zoom leaves fitted track unchanged"),
        FMath::IsNearlyEqual(Viewer->Get_TimelineZoom(), 1.0));
    TestTrue(TEXT("Two times timeline zoom is accepted"), Viewer->Set_TimelineZoom(2.0));
    TestTrue(TEXT("Four times timeline zoom is accepted"), Viewer->Set_TimelineZoom(4.0));
    if (const auto ZoomMenu = FindComboButton(Viewer, TEXT("Timeline zoom")))
    {
        TestTrue(TEXT("Physical timeline zoom menu opens"),
            OpenMenu(Slate, HostWindow, ZoomMenu.ToSharedRef()));
        TestTrue(TEXT("Physical eight times timeline zoom option routes input"),
            ClickMenuButton(Slate, HostWindow, TEXT("Timeline 8x")));
    }
    else
    {
        TestTrue(TEXT("Mounted timeline zoom menu exists"), false);
    }
    Tick(Slate);
    TestTrue(TEXT("Eight times zoom expands the full authored track"),
        FMath::IsNearlyEqual(Viewer->Get_TimelineZoom(), 8.0) &&
        Timeline->Get_TrackWidth() > ViewportWidth * 2.0 &&
        FMath::IsNearlyEqual(Timeline->Get_FrameWidth(0) /
            Timeline->Get_FrameWidth(3), 5.0f, 0.05f));
    TestTrue(TEXT("Eight times zoom reveals the full fixed thumbnail in the shortest frame"),
        Timeline->Get_FrameWidth(3) >= Timeline->Get_ThumbnailSize().X + 4.0);
    TestTrue(TEXT("Timeline zoom does not change canvas zoom"),
        FMath::IsNearlyEqual(State->_Zoom, CanvasZoom));
    const auto ZoomOutput = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteAcceptance/Sproutling-TimeTrack-Zoom.png"));
    TestTrue(TEXT("Zoomed time track captures the mounted renderer"),
        Capture(Slate, Viewer, ZoomOutput, Pixels, Size));
    AddInfo(FString::Printf(TEXT("Sproutling zoomed time track screenshot: %s"), *ZoomOutput));
    Scroll->SetScrollOffset(Timeline->Get_TrackWidth());
    Tick(Slate);
    TestTrue(TEXT("Eight times zoom allows physical horizontal scrolling to the final frame"),
        Scroll->GetScrollOffset() > 0.0f &&
        Scroll->GetScrollOffset() + Scroll->GetCachedGeometry().GetLocalSize().X >=
            Timeline->Get_TrackWidth() - 2.0f);
    Viewer->Set_Layout(ECk_PixelArtViewer_Layout::Focused);
    Tick(Slate);
    TestTrue(TEXT("Focused layout retains timeline zoom and canvas zoom"),
        FMath::IsNearlyEqual(Viewer->Get_TimelineZoom(), 8.0) &&
        FMath::IsNearlyEqual(State->_Zoom, CanvasZoom));
    Viewer->Set_Layout(ECk_PixelArtViewer_Layout::Studio);
    Tick(Slate);
    TestTrue(TEXT("Studio layout return retains timeline zoom and canvas zoom"),
        FMath::IsNearlyEqual(Viewer->Get_TimelineZoom(), 8.0) &&
        FMath::IsNearlyEqual(State->_Zoom, CanvasZoom));

    TestTrue(TEXT("Return to fitted timeline for physical scrubbing"), Viewer->Set_TimelineZoom(1.0));
    Scroll->SetScrollOffset(0.0f);
    Tick(Slate);
    const auto SecondFrameX = Timeline->Get_FrameWidth(0) +
        Timeline->Get_FrameWidth(1) * 0.5;
    TestTrue(TEXT("Physical time-track click selects the second authored frame"),
        Scrub_Timeline(Slate, Timeline.ToSharedRef(), SecondFrameX, SecondFrameX));
    TestEqual(TEXT("Click selects frame two"), Viewer->Get_SelectedFrame(), 1);
    TestTrue(TEXT("Click maps track position to authored milliseconds"),
        FMath::IsNearlyEqual(State->_TimeMilliseconds, 560.0, 4.0));
    const auto LastFrameX = Timeline->Get_TrackWidth() -
        Timeline->Get_FrameWidth(7) * 0.5;
    TestTrue(TEXT("Physical drag scrubs across the mounted time track"),
        Scrub_Timeline(Slate, Timeline.ToSharedRef(),
            Timeline->Get_FrameWidth(0) * 0.25, LastFrameX));
    TestEqual(TEXT("Drag selects the final authored frame"), Viewer->Get_SelectedFrame(), 7);
    TestTrue(TEXT("Drag maps to a point inside the last authored duration"),
        State->_TimeMilliseconds > 1530.0 && State->_TimeMilliseconds < 1980.0);

    State->_Playback = ECk_PixelSprite_Playback::Loop;
    TestTrue(TEXT("Loop playback seeks beyond one forward cycle"), Viewer->Seek_Milliseconds(2060.0));
    Tick(Slate);
    auto Playhead = 0.0;
    TestTrue(TEXT("Loop timeline mapping succeeds"),
        State->TryGet_TimelineMilliseconds(Animation, Playhead));
    TestTrue(TEXT("2060 milliseconds wraps to 80 on the visible track"),
        FMath::IsNearlyEqual(Playhead, 80.0, 0.01) &&
        FMath::IsNearlyEqual(Timeline->Get_PlayheadMilliseconds(), 80.0, 0.01));
    State->_Playback = ECk_PixelSprite_Playback::PingPong;
    TestTrue(TEXT("Ping-pong playback seeks into its return leg"), Viewer->Seek_Milliseconds(2000.0));
    Tick(Slate);
    auto Returning = false;
    auto ReturnAt2000 = 0.0;
    TestTrue(TEXT("First return position maps onto the authored track"),
        State->TryGet_TimelineReturning(Animation, Returning) && Returning &&
        State->TryGet_TimelineMilliseconds(Animation, ReturnAt2000) &&
        FMath::IsNearlyEqual(Timeline->Get_PlayheadMilliseconds(), ReturnAt2000, 0.01));
    TestTrue(TEXT("Ping-pong playback seeks further along the return leg"),
        Viewer->Seek_Milliseconds(2100.0));
    Tick(Slate);
    auto ReturnAt2100 = 0.0;
    TestTrue(TEXT("Returning playhead moves left as absolute time increases"),
        State->TryGet_TimelineReturning(Animation, Returning) && Returning &&
        State->TryGet_TimelineMilliseconds(Animation, ReturnAt2100) &&
        ReturnAt2100 < ReturnAt2000 &&
        FMath::IsNearlyEqual(Timeline->Get_PlayheadMilliseconds(), ReturnAt2100, 0.01));

    const auto OriginalTrackWidth = Timeline->Get_TrackWidth();
    const auto OriginalLastFrameWidth = Timeline->Get_FrameWidth(7);
    auto CandidateFrames = TArray<FCk_PixelArtViewer_TimelineFrame>{};
    auto CandidateFrame = FCk_PixelArtViewer_TimelineFrame{};
    CandidateFrame._DurationMilliseconds = 100.0;
    CandidateFrames.Add(CandidateFrame);
    CandidateFrame._DurationMilliseconds = std::numeric_limits<double>::quiet_NaN();
    CandidateFrames.Add(CandidateFrame);
    AddExpectedError(TEXT("Pixel art timeline frame has invalid duration"),
        EAutomationExpectedErrorFlags::Contains, 2);
    TestFalse(TEXT("Malformed later duration rejects the whole timeline candidate"),
        Timeline->Set_Frames(MoveTemp(CandidateFrames)));
    TestTrue(TEXT("Rejected candidate preserves all eight authored frame blocks"),
        Timeline->Get_FrameWidth(7) == OriginalLastFrameWidth &&
        Timeline->Get_TrackWidth() == OriginalTrackWidth);
    return true;
}

#endif
