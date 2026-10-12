#include "CkPixelArtViewer/CkPixelArtViewer_Window.h"

#include "CkPixelArtViewer/CkPixelArtViewer_Canvas.h"
#include "CkPixelArtViewer/CkPixelArtViewer_Timeline.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"

#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "EditorReimportHandler.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_pixel_art_viewer_layout
{
    auto
        IconButton(
            FName InIcon,
            const FString& InTooltip,
            FOnClicked InAction,
            TAttribute<FSlateColor> InTint = FSlateColor::UseForeground()
        )
        -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(5)
            .ToolTipText(FText::FromString(InTooltip))
            .OnClicked(MoveTemp(InAction))
            [SNew(SBox).WidthOverride(20).HeightOverride(20)
                [SNew(SImage).Image(FAppStyle::GetBrush(InIcon)).ColorAndOpacity(InTint)]];
    }

    auto
        Heading(
            const FString& InText
        )
        -> TSharedRef<SWidget>
    {
        return SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(FMargin{10, 7})
            [SNew(STextBlock).Text(FText::FromString(InText))];
    }

    auto
        Section(
            const FString& InLabel,
            const TSharedRef<SWidget>& InBody,
            bool InCollapsed = false
        )
        -> TSharedRef<SWidget>
    {
        return SNew(SExpandableArea).InitiallyCollapsed(InCollapsed)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8).HeaderPadding(FMargin{6, 5})
            .HeaderContent()[SNew(STextBlock).Text(FText::FromString(InLabel))]
            .BodyContent()[InBody];
    }
}

auto
    SCk_PixelArtViewer_Window::
    Build_MenuButton(
        FName InMenu,
        TAttribute<FText> InLabel,
        FName InIcon
    )
    -> TSharedRef<SWidget>
{
    auto Content = SNew(SHorizontalBox);
    if (InIcon != NAME_None)
    {
        Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 5, 0)
            [SNew(SBox).WidthOverride(16).HeightOverride(16)
                [SNew(SImage).Image(FAppStyle::GetBrush(InIcon))]];
    }
    Content->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [SNew(STextBlock).Text(InLabel)];
    return SNew(SComboButton)
        .ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
        .ContentPadding(FMargin{7, 4})
        .ToolTipText(FText::FromName(InMenu))
        .OnGetMenuContent_Lambda([WeakWindow = TWeakPtr<SCk_PixelArtViewer_Window>{SharedThis(this)}, InMenu]
        () -> TSharedRef<SWidget>
        {
            const auto Window = WeakWindow.Pin();
            if (Window.IsValid())
            {
                return Window->Build_Menu(InMenu);
            }
            return SNullWidget::NullWidget;
        })
        .ButtonContent()[Content];
}

auto
    SCk_PixelArtViewer_Window::
    Build_AssetToolbar()
    -> TSharedRef<SWidget>
{
    using namespace ck_pixel_art_viewer_layout;
    return SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin{6, 3})
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [IconButton("Icons.BrowseContent", TEXT("Browse to asset"), FOnClicked::CreateLambda([this]
            {
                if (_Asset.IsValid() && GEditor)
                {
                    GEditor->SyncBrowserToObjects(TArray<FAssetData>{FAssetData{_Asset.Get()}});
                }
                return FReply::Handled();
            }))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [IconButton("Icons.Toolbar.Reimport", TEXT("Reimport animation"), FOnClicked::CreateLambda([this]
            {
                if (_Asset.IsValid())
                {
                    constexpr auto AskForNewFile = false;
                    constexpr auto ShowNotification = true;
                    _Status = FReimportManager::Instance()->Reimport(_Asset.Get(), AskForNewFile, ShowNotification)
                        ? TEXT("Reimported source") : TEXT("Reimport failed or was cancelled");
                }
                return FReply::Handled();
            }))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0, 8, 0)
            [Build_MenuButton("Export", FText::FromString(TEXT("Export")), "Icons.Toolbar.Export")]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(4, 0)
            [SNew(SObjectPropertyEntryBox)
                .AllowedClass(UCk_PixelSpriteAsset_UE::StaticClass())
                .ObjectPath_Lambda([this] { return _Asset.IsValid() ? _Asset->GetPathName() : FString{}; })
                .OnObjectChanged_Lambda([this](const FAssetData& InData)
                {
                    const auto Asset = TObjectPtr<UCk_PixelSpriteAsset_UE>{Cast<UCk_PixelSpriteAsset_UE>(InData.GetAsset())};
                    if (Asset)
                    {
                        Set_Asset(*Asset);
                    }
                })]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0)
            [Build_MenuButton("Animation", TAttribute<FText>::CreateLambda([this]
            {
                const auto Name = Get_SelectedAnimationName();
                return FText::FromString(Name.IsEmpty() ? TEXT("Animation") : Name);
            }))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Build_MenuButton("Viewer layout", TAttribute<FText>::CreateLambda([this]
            {
                return FText::FromString(_Layout == ECk_PixelArtViewer_Layout::Studio ? TEXT("Studio") : TEXT("Focused"));
            }))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Build_MenuButton("Workspace options", FText::GetEmpty(), "Icons.EllipsisVerticalNarrow")]];
}

auto
    SCk_PixelArtViewer_Window::
    Build_ViewportToolbar()
    -> TSharedRef<SWidget>
{
    using namespace ck_pixel_art_viewer_layout;
    return SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(4)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Build_MenuButton("Show", FText::FromString(TEXT("Show")))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [IconButton("CurveEd.FitHorizontal", TEXT("Fit"), FOnClicked::CreateLambda([this]
            {
                _Canvas->Fit_View();
                return FReply::Handled();
            }))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Build_MenuButton("Zoom", FText::FromString(TEXT("Zoom")))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
            [IconButton("InputBindingEditor.LayersView", TEXT("Onion skin"), FOnClicked::CreateLambda([this]
            {
                _State->_ShowOnion = NOT _State->_ShowOnion;
                Save_Preferences();
                Refresh_Decorations();
                return FReply::Handled();
            }), TAttribute<FSlateColor>::CreateLambda([this]
            {
                return _State->_ShowOnion ? FStyleColors::AccentBlue : FStyleColors::Foreground;
            }))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Build_MenuButton("Onion settings", FText::GetEmpty())]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
            [Build_MenuButton("Comparison mode", TAttribute<FText>::CreateSP(this,
                &SCk_PixelArtViewer_Window::Get_ComparisonText))]
            + SHorizontalBox::Slot().FillWidth(1)
            [SNew(SBox)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [IconButton("Icons.Toolbar.Settings", TEXT("Inspector"), FOnClicked::CreateLambda([this]
            {
                Toggle_Inspector();
                return FReply::Handled();
            }))]];
}

auto
    SCk_PixelArtViewer_Window::
    Build_Transport()
    -> TSharedRef<SWidget>
{
    using namespace ck_pixel_art_viewer_layout;
    auto Row = SNew(SHorizontalBox);
    const auto Add = [&Row](const TSharedRef<SWidget>& InWidget)
    {
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)[InWidget];
    };
    Add(IconButton("Animation.Backward_End", TEXT("First frame"), FOnClicked::CreateLambda([this]
    {
        Jump_Edge(false);
        return FReply::Handled();
    })));
    Add(IconButton("Animation.Backward_Step", TEXT("Previous frame"), FOnClicked::CreateLambda([this]
    {
        Step_Frame(-1);
        return FReply::Handled();
    })));
    Add(SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").ContentPadding(5)
        .ToolTipText(FText::FromString(TEXT("Play / Pause")))
        .OnClicked_Lambda([this]
        {
            return OnKeyDown(GetCachedGeometry(), FKeyEvent{EKeys::SpaceBar, FModifierKeysState{}, 0, false, 0, 0});
        })
        [SNew(SBox).WidthOverride(20).HeightOverride(20)
            [SNew(SImage).Image_Lambda([this]
            {
                return FAppStyle::GetBrush(_State->_Playing ? "Animation.Pause" : "Animation.Forward");
            })]]);
    Add(IconButton("Animation.Forward_Step", TEXT("Next frame"), FOnClicked::CreateLambda([this]
    {
        Step_Frame(1);
        return FReply::Handled();
    })));
    Add(IconButton("Animation.Forward_End", TEXT("Last frame"), FOnClicked::CreateLambda([this]
    {
        Jump_Edge(true);
        return FReply::Handled();
    })));
    Add(IconButton("Animation.Backward", TEXT("Reverse playback"), FOnClicked::CreateLambda([this]
    {
        _State->_Reverse = NOT _State->_Reverse;
        return FReply::Handled();
    }), TAttribute<FSlateColor>::CreateLambda([this]
    {
        return _State->_Reverse ? FStyleColors::AccentBlue : FStyleColors::Foreground;
    })));
    Add(Build_MenuButton("Playback mode", TAttribute<FText>::CreateSP(this,
        &SCk_PixelArtViewer_Window::Get_PlaybackText)));
    Add(Build_MenuButton("Playback speed", TAttribute<FText>::CreateSP(this,
        &SCk_PixelArtViewer_Window::Get_SpeedText)));
    Add(Build_MenuButton("Playback options", FText::GetEmpty(), "Icons.EllipsisVerticalNarrow"));
    Row->AddSlot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(8, 0)
        [SNew(STextBlock).Text(this, &SCk_PixelArtViewer_Window::Get_TimeText)];
    return SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(4)[Row];
}

auto
    SCk_PixelArtViewer_Window::
    Build_Browser()
    -> TSharedRef<SWidget>
{
    using namespace ck_pixel_art_viewer_layout;
    return SNew(SBorder).Padding(0)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Heading(TEXT("Asset Browser"))]
            + SVerticalBox::Slot().AutoHeight().Padding(4)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [SNew(SButton).Text(FText::FromString(TEXT("Animations")))
                    .OnClicked_Lambda([this] { _BrowserTab = 0; return FReply::Handled(); })]
                + SHorizontalBox::Slot().FillWidth(1)
                [SNew(SButton).Text(FText::FromString(TEXT("Sources")))
                    .OnClicked_Lambda([this] { _BrowserTab = 1; return FReply::Handled(); })]]
            + SVerticalBox::Slot().FillHeight(1)
            [SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return _BrowserTab; })
                + SWidgetSwitcher::Slot()
                [SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(_AnimationList, SVerticalBox)]]
                + SWidgetSwitcher::Slot()
                [SAssignNew(_SourceTree, STreeView<TSharedPtr<FSourceNode>>)
                    .TreeItemsSource(&_SourceRoots)
                    .OnGenerateRow(this, &SCk_PixelArtViewer_Window::Generate_SourceRow)
                    .OnGetChildren(this, &SCk_PixelArtViewer_Window::Get_SourceChildren)]]
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [SNew(STextBlock).Text(this, &SCk_PixelArtViewer_Window::Get_AssetText).AutoWrapText(true)]];
}

auto
    SCk_PixelArtViewer_Window::
    Build_Inspector()
    -> TSharedRef<SWidget>
{
    using namespace ck_pixel_art_viewer_layout;
    return SNew(SBorder).Padding(0)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Heading(TEXT("Inspector"))]
            + SVerticalBox::Slot().AutoHeight().Padding(4)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [SNew(SButton).Text(FText::FromString(TEXT("Details")))
                    .OnClicked_Lambda([this] { _InspectorTab = 0; return FReply::Handled(); })]
                + SHorizontalBox::Slot().FillWidth(1)
                [SNew(SButton).Text(FText::FromString(TEXT("Palette")))
                    .OnClicked_Lambda([this] { _InspectorTab = 1; return FReply::Handled(); })]]
            + SVerticalBox::Slot().FillHeight(1)
            [SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return _InspectorTab; })
                + SWidgetSwitcher::Slot()
                [SNew(SScrollBox)
                    + SScrollBox::Slot().Padding(5)
                    [Section(TEXT("Frame"), SNew(STextBlock).Text(this,
                        &SCk_PixelArtViewer_Window::Get_FrameText).AutoWrapText(true))]
                    + SScrollBox::Slot().Padding(5)
                    [Section(TEXT("Rendering preview"), SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
                        [Build_MenuButton("Rendering filter", TAttribute<FText>::CreateLambda([this]
                        {
                            const auto Mode = _State->_RenderSettings._Mode;
                            return FText::FromString(Mode == ECk_PixelSprite_RenderMode::Nearest ? TEXT("Nearest")
                                : Mode == ECk_PixelSprite_RenderMode::Bilinear ? TEXT("Bilinear") : TEXT("Smooth silhouette"));
                        }))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
                        [Build_MenuButton("Render scale", TAttribute<FText>::CreateLambda([this]
                        {
                            return FText::FromString(FString::Printf(TEXT("Render %dx"), _State->_RenderSettings._Scale));
                        }))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
                        [Build_MenuButton("Rendering options", FText::FromString(TEXT("Outline / background")))])]
                    + SScrollBox::Slot().Padding(5)
                    [Section(TEXT("Source, anchors & metadata"), SNew(STextBlock).Text(this,
                        &SCk_PixelArtViewer_Window::Get_DetailText).AutoWrapText(true), true)]]
                + SWidgetSwitcher::Slot()
                [SNew(SScrollBox) + SScrollBox::Slot().Padding(8)[Build_PalettePanel()]]]];
}

auto
    SCk_PixelArtViewer_Window::
    Build_Timeline()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin{6, 4})
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(2)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [SNew(STextBlock).Text(FText::FromString(TEXT("Timeline")))]
                + SHorizontalBox::Slot().AutoWidth()
                [Build_MenuButton("Timeline zoom", TAttribute<FText>::CreateLambda([this]
                {
                    return FText::FromString(_TimelineZoom == 1.0 ? TEXT("Timeline zoom: Fit")
                        : FString::Printf(TEXT("Timeline zoom: %.0fx"), _TimelineZoom));
                }))]]
            + SVerticalBox::Slot().AutoHeight()
            [SAssignNew(_TimelineScroll, SScrollBox).Orientation(Orient_Horizontal)
                .ScrollBarAlwaysVisible(true)
                + SScrollBox::Slot()
                [SAssignNew(_Timeline, SCk_PixelArtViewer_Timeline)
                    .PlaybackMilliseconds_Lambda([this]
                    {
                        auto Position = 0.0;
                        if (_Snapshot.IsValid() && _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
                        {
                            _State->TryGet_TimelineMilliseconds(_Snapshot->_Animations[_AnimationIndex], Position);
                        }
                        return Position;
                    })
                    .SelectedFrame_Lambda([this] { return _SelectedFrame; })
                    .OnScrub_Lambda([this](double InMilliseconds)
                    {
                        if (_Snapshot.IsValid() && _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
                        {
                            _State->_Playing = false;
                            _State->Seek_TimelineMilliseconds(_Snapshot->_Animations[_AnimationIndex], InMilliseconds);
                            Refresh_Frame();
                        }
                    })]]];
}

auto
    SCk_PixelArtViewer_Window::
    Get_FrameText() const
    -> FText
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex)
        || NOT _Snapshot->_Animations[_AnimationIndex]._Frames.IsValidIndex(_SelectedFrame))
    {
        return FText::FromString(TEXT("Select an animation"));
    }
    const auto& Animation = _Snapshot->_Animations[_AnimationIndex];
    const auto& Frame = Animation._Frames[_SelectedFrame];
    return FText::FromString(FString::Printf(TEXT("Frame     %d / %d\n\nDuration     %.3f ms\n\nCanvas     %d × %d\n\n%s"),
        _SelectedFrame + 1, Animation._Frames.Num(), Frame._DurationMilliseconds,
        _Snapshot->_Width, _Snapshot->_Height, *Frame._Source));
}

auto
    SCk_PixelArtViewer_Window::
    Set_Layout(
        ECk_PixelArtViewer_Layout InLayout
    )
    -> void
{
    const auto ValidLayout = InLayout == ECk_PixelArtViewer_Layout::Studio
        || InLayout == ECk_PixelArtViewer_Layout::Focused;
    CK_ENSURE_IF_NOT(ValidLayout, TEXT("Invalid pixel viewer layout"))
    {
        return;
    }
    if (_Layout == InLayout)
    {
        return;
    }
    Save_LayoutPreferences();
    _Layout = InLayout;
    _InspectorVisible = _Layout == ECk_PixelArtViewer_Layout::Studio;
    Rebuild_Workspace();
    auto Preferences = TObjectPtr<UCkPixelArtViewerSettings>{GetMutableDefault<UCkPixelArtViewerSettings>()};
    Preferences->_Layout = _Layout;
    Preferences->SaveConfig();
}

auto
    SCk_PixelArtViewer_Window::
    Get_Layout() const
    -> ECk_PixelArtViewer_Layout
{
    return _Layout;
}

auto
    SCk_PixelArtViewer_Window::
    Toggle_Inspector()
    -> void
{
    Save_LayoutPreferences();
    _InspectorVisible = NOT _InspectorVisible;
    Rebuild_Workspace();
}

auto
    SCk_PixelArtViewer_Window::
    Rebuild_Workspace()
    -> void
{
    _Workspace->SetContent(SNullWidget::NullWidget);
    if (_MainSplitter.IsValid())
    {
        _MainSplitter->ClearChildren();
    }
    _MainSplitter = SNew(SSplitter)
        .OnSplitterFinishedResizing(this, &SCk_PixelArtViewer_Window::Save_LayoutPreferences);
    const auto Preferences = TObjectPtr<const UCkPixelArtViewerSettings>{GetDefault<UCkPixelArtViewerSettings>()};
    auto Left = Preferences->_LeftPanelWeight;
    auto Center = Preferences->_CanvasPanelWeight;
    auto Right = Preferences->_RightPanelWeight;
    const auto Sum = Left + Center + Right;
    if (NOT FMath::IsFinite(Sum) || NOT FMath::IsFinite(Left) || NOT FMath::IsFinite(Center)
        || NOT FMath::IsFinite(Right) || Sum <= 0 || Left / Sum < 0.03f || Center / Sum < 0.03f || Right / Sum < 0.03f)
    {
        Left = 0.20f;
        Center = 0.58f;
        Right = 0.22f;
    }
    else
    {
        Left /= Sum;
        Center /= Sum;
        Right /= Sum;
    }
    if (_Layout == ECk_PixelArtViewer_Layout::Studio)
    {
        _MainSplitter->AddSlot().Value(Left)[_BrowserPanel.ToSharedRef()];
    }
    _MainSplitter->AddSlot().Value(_Layout == ECk_PixelArtViewer_Layout::Focused ? 0.78f : Center)
        [_CenterPanel.ToSharedRef()];
    if (_InspectorVisible)
    {
        _MainSplitter->AddSlot().Value(Right)[_InspectorPanel.ToSharedRef()];
    }
    _Workspace->SetContent(_MainSplitter.ToSharedRef());
}

auto
    SCk_PixelArtViewer_Window::
    Build_Menu(
        FName InMenu
    )
    -> TSharedRef<SWidget>
{
    auto Menu = FMenuBuilder{true, nullptr};
    const auto WeakWindow = TWeakPtr<SCk_PixelArtViewer_Window>{SharedThis(this)};
    const auto Add = [&Menu, WeakWindow](const FString& InLabel, TFunction<void()> InAction,
        TFunction<bool()> InChecked = {})
    {
        const auto Type = InChecked ? EUserInterfaceActionType::RadioButton : EUserInterfaceActionType::Button;
        Menu.AddMenuEntry(FText::FromString(InLabel), FText::GetEmpty(), FSlateIcon{},
            FUIAction{FExecuteAction::CreateLambda([WeakWindow, Action = MoveTemp(InAction)]
            {
                if (WeakWindow.IsValid())
                {
                    Action();
                }
            }), FCanExecuteAction{}, FIsActionChecked::CreateLambda([WeakWindow, Checked = MoveTemp(InChecked)]
            {
                return WeakWindow.IsValid() && Checked && Checked();
            })}, NAME_None, Type);
    };
    const auto Submenu = [&Menu, WeakWindow](const FString& InLabel, FName InName)
    {
        Menu.AddSubMenu(FText::FromString(InLabel), FText::GetEmpty(),
            FNewMenuDelegate::CreateLambda([WeakWindow, InName](FMenuBuilder& InBuilder)
            {
                const auto Window = WeakWindow.Pin();
                if (Window.IsValid())
                {
                    InBuilder.AddWidget(Window->Build_Menu(InName), FText::GetEmpty(), true);
                }
            }));
    };
    if (InMenu == "Viewer layout")
    {
        Add(TEXT("Studio"), [this] { Set_Layout(ECk_PixelArtViewer_Layout::Studio); },
            [this] { return _Layout == ECk_PixelArtViewer_Layout::Studio; });
        Add(TEXT("Focused"), [this] { Set_Layout(ECk_PixelArtViewer_Layout::Focused); },
            [this] { return _Layout == ECk_PixelArtViewer_Layout::Focused; });
    }
    else if (InMenu == "Timeline zoom")
    {
        for (const auto Zoom : {1.0, 2.0, 4.0, 8.0})
        {
            Add(Zoom == 1.0 ? TEXT("Fit timeline") : FString::Printf(TEXT("Timeline %.0fx"), Zoom),
                [this, Zoom] { Set_TimelineZoom(Zoom); }, [this, Zoom] { return _TimelineZoom == Zoom; });
        }
    }
    else if (InMenu == "Workspace options")
    {
        Add(TEXT("Inspector"), [this] { Toggle_Inspector(); }, [this] { return _InspectorVisible; });
        Add(TEXT("Restore Studio layout"), [this]
        {
            Set_Layout(ECk_PixelArtViewer_Layout::Studio);
            _InspectorVisible = true;
            Rebuild_Workspace();
        });
    }
    else if (InMenu == "Export")
    {
        Add(TEXT("Export frame"), [this] { Export_Frame(); });
        Add(TEXT("Export sheet"), [this] { Export_Sheet(); });
    }
    else if (InMenu == "Animation")
    {
        if (_Snapshot.IsValid())
        {
            for (int32 Index = 0; Index < _Snapshot->_Animations.Num(); ++Index)
            {
                Add(_Snapshot->_Animations[Index]._Name, [this, Index] { Select_Animation(Index); },
                    [this, Index] { return _AnimationIndex == Index; });
            }
        }
    }
    else if (InMenu == "Show")
    {
        const auto Toggle = [this](bool& InSetting)
        {
            InSetting = NOT InSetting;
            Save_Preferences();
            _Canvas->Invalidate(EInvalidateWidgetReason::Paint);
        };
        Add(TEXT("Checker"), [this, Toggle] { Toggle(_State->_ShowChecker); }, [this] { return _State->_ShowChecker; });
        Add(TEXT("Grid"), [this, Toggle] { Toggle(_State->_ShowGrid); }, [this] { return _State->_ShowGrid; });
        Add(TEXT("Axes"), [this, Toggle] { Toggle(_State->_ShowAxes); }, [this] { return _State->_ShowAxes; });
        Add(TEXT("Crosshair"), [this, Toggle] { Toggle(_State->_ShowCrosshair); }, [this] { return _State->_ShowCrosshair; });
        Menu.AddMenuSeparator();
        Add(TEXT("Center"), [this] { _Canvas->Center_View(); });
        Add(TEXT("Reset view"), [this] { _State->Reset_View(); _Canvas->Invalidate(EInvalidateWidgetReason::Paint); });
    }
    else if (InMenu == "Zoom")
    {
        Add(TEXT("Fit"), [this] { _Canvas->Fit_View(); });
        for (const auto Scale : {1, 4, 8, 16})
        {
            Add(FString::Printf(TEXT("%dx"), Scale), [this, Scale]
            {
                _State->Set_Zoom(Scale);
                _Canvas->Center_View();
            });
        }
        Menu.AddMenuSeparator();
        Add(TEXT("Center"), [this] { _Canvas->Center_View(); });
        Add(TEXT("Reset view"), [this] { _State->Reset_View(); _Canvas->Invalidate(EInvalidateWidgetReason::Paint); });
    }
    else if (InMenu == "Playback mode")
    {
        const auto Modes = TArray<ECk_PixelSprite_Playback>{ECk_PixelSprite_Playback::Loop,
            ECk_PixelSprite_Playback::Once, ECk_PixelSprite_Playback::PingPong};
        const auto Labels = TArray<FString>{TEXT("Loop"), TEXT("Once"), TEXT("Ping-pong")};
        for (int32 Index = 0; Index < Modes.Num(); ++Index)
        {
            const auto Mode = Modes[Index];
            Add(Labels[Index], [this, Mode]
            {
                const auto Previous = _State->_Playback;
                _State->_Playback = Mode;
                if (NOT Ensure_Prepared())
                {
                    _State->_Playback = Previous;
                    _Status = TEXT("Cannot prepare the selected playback mode");
                    return;
                }
                Refresh_Frame();
            }, [this, Mode] { return _State->_Playback == Mode; });
        }
    }
    else if (InMenu == "Playback speed")
    {
        for (const auto Speed : {0.25, 0.5, 1.0, 2.0, 4.0})
        {
            Add(FString::Printf(TEXT("%.2fx"), Speed), [this, Speed] { _State->Set_Speed(Speed); },
                [this, Speed] { return _State->_Speed == Speed; });
        }
    }
    else if (InMenu == "Playback options")
    {
        Add(TEXT("Stop"), [this] { _State->_Playing = false; _State->Seek(0.0); Refresh_Frame(); });
        Add(TEXT("Restart"), [this]
        {
            _State->Seek(_State->_Reverse ? _PreparedCycleMilliseconds : 0.0);
            _State->_Playing = true;
            Refresh_Frame();
        });
        Menu.AddMenuSeparator();
        Submenu(TEXT("Sampling"), "Sampling");
        Submenu(TEXT("Frame retention"), "Frame retention");
    }
    else if (InMenu == "Sampling")
    {
        for (const auto Rate : {0, 30, 15, 10, 5})
        {
            Add(Rate == 0 ? TEXT("Off") : FString::Printf(TEXT("%d fps"), Rate), [this, Rate]
            {
                _State->Set_SamplingFps(Rate);
                Refresh_Frame();
            }, [this, Rate] { return _State->_SamplingFps == Rate; });
        }
    }
    else if (InMenu == "Frame retention")
    {
        for (const auto Count : {1, 2, 3})
        {
            Add(FString::Printf(TEXT("Every %d frame"), Count), [this, Count]
            {
                _State->Set_Decimation(Count);
                Refresh_Frame();
            }, [this, Count] { return _State->_Decimation == Count; });
        }
    }
    else if (InMenu == "Comparison mode")
    {
        const auto Labels = TArray<FString>{TEXT("Compare off"), TEXT("Side by side"), TEXT("Overlay"), TEXT("Difference")};
        for (int32 Index = 0; Index < Labels.Num(); ++Index)
        {
            const auto Mode = static_cast<ECk_PixelArtViewer_Comparison>(Index);
            Add(Labels[Index], [this, Mode] { _State->_Comparison = Mode; Refresh_Decorations(); },
                [this, Mode] { return _State->_Comparison == Mode; });
        }
        Menu.AddMenuSeparator();
        if (_Snapshot.IsValid() && _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
        {
            const auto& Frames = _Snapshot->_Animations[_AnimationIndex]._Frames;
            for (int32 Index = 0; Index < Frames.Num(); ++Index)
            {
                Add(FString::Printf(TEXT("Reference frame %d"), Index + 1), [this, Index]
                {
                    _State->_ComparisonFrame = Index;
                    Refresh_Decorations();
                }, [this, Index] { return _State->_ComparisonFrame == Index; });
            }
        }
        Add(TEXT("Difference bounds"), [this]
        {
            _State->_ShowDifferenceBounds = NOT _State->_ShowDifferenceBounds;
            _Canvas->Invalidate(EInvalidateWidgetReason::Paint);
        }, [this] { return _State->_ShowDifferenceBounds; });
    }
    else if (InMenu == "Rendering filter")
    {
        const auto Labels = TArray<FString>{TEXT("Nearest"), TEXT("Bilinear"), TEXT("Smooth silhouette")};
        for (int32 Index = 0; Index < Labels.Num(); ++Index)
        {
            const auto Mode = static_cast<ECk_PixelSprite_RenderMode>(Index);
            Add(Labels[Index], [this, Mode] { Set_RenderMode(Mode); }, [this, Mode] { return _State->_RenderSettings._Mode == Mode; });
        }
    }
    else if (InMenu == "Render scale")
    {
        for (int32 Scale = 1; Scale <= 8; ++Scale)
        {
            Add(FString::Printf(TEXT("%dx"), Scale), [this, Scale] { Set_RenderScale(Scale); },
                [this, Scale] { return _State->_RenderSettings._Scale == Scale; });
        }
    }
    else if (InMenu == "Rendering options")
    {
        Add(TEXT("Outline"), [this]
        {
            _State->_RenderSettings._Outline = NOT _State->_RenderSettings._Outline;
            if (Rebuild_BaseResources())
            {
                Save_Preferences();
            }
        }, [this] { return _State->_RenderSettings._Outline; });
        Menu.AddMenuSeparator();
        const auto Colors = TArray<FColor>{FColor::Transparent, FColor::Black, FColor::White};
        const auto Labels = TArray<FString>{TEXT("Transparent background"), TEXT("Black background"), TEXT("White background")};
        for (int32 Index = 0; Index < Colors.Num(); ++Index)
        {
            const auto Color = Colors[Index];
            Add(Labels[Index], [this, Color]
            {
                _State->_RenderSettings._Background = Color;
                if (Rebuild_BaseResources())
                {
                    Save_Preferences();
                }
            }, [this, Color] { return _State->_RenderSettings._Background == Color; });
        }
    }
    else if (InMenu == "Onion settings")
    {
        Submenu(TEXT("Before"), "Onion before");
        Submenu(TEXT("After"), "Onion after");
        Submenu(TEXT("Previous offset"), "Previous offset");
        Submenu(TEXT("Following offset"), "Following offset");
        Menu.AddWidget(SNew(SBox).WidthOverride(190).Padding(8)
            [SNew(SSlider).ToolTipText(FText::FromString(TEXT("Onion opacity")))
                .Value_Lambda([WeakWindow]
                {
                    const auto Window = WeakWindow.Pin();
                    return Window.IsValid() ? Window->_State->_OnionOpacity / 0.75f : 0.0f;
                })
                .OnValueChanged_Lambda([WeakWindow](float InValue)
                {
                    const auto Window = WeakWindow.Pin();
                    if (Window.IsValid())
                    {
                        Window->_State->_OnionOpacity = InValue * 0.75f;
                        Window->Save_Preferences();
                        Window->_Canvas->Invalidate(EInvalidateWidgetReason::Paint);
                    }
                })], FText::FromString(TEXT("Opacity")));
        Add(TEXT("Previous tint"), [this]
        {
            _State->_PreviousTint = _State->_PreviousTint.B > 0.8f
                ? FLinearColor{0.4f, 1.0f, 0.4f, 1.0f} : FLinearColor{0.35f, 0.65f, 1.0f, 1.0f};
            Save_Preferences();
            Refresh_Decorations();
        });
        Add(TEXT("Following tint"), [this]
        {
            _State->_FollowingTint = _State->_FollowingTint.R > 0.9f
                ? FLinearColor{1.0f, 0.4f, 1.0f, 1.0f} : FLinearColor{1.0f, 0.45f, 0.35f, 1.0f};
            Save_Preferences();
            Refresh_Decorations();
        });
    }
    else if (InMenu == "Onion before" || InMenu == "Onion after")
    {
        const auto Previous = InMenu == "Onion before";
        for (int32 Count = 0; Count <= 4; ++Count)
        {
            Add(FString::FromInt(Count), [this, Previous, Count]
            {
                (Previous ? _State->_PreviousOnionCount : _State->_FollowingOnionCount) = Count;
                Save_Preferences();
                Refresh_Decorations();
            }, [this, Previous, Count] { return (Previous ? _State->_PreviousOnionCount : _State->_FollowingOnionCount) == Count; });
        }
    }
    else if (InMenu == "Previous offset" || InMenu == "Following offset")
    {
        const auto Previous = InMenu == "Previous offset";
        for (int32 Offset = 0; Offset <= 4; ++Offset)
        {
            Add(Offset == 0 ? TEXT("All surrounding offsets") : FString::FromInt(Offset), [this, Previous, Offset]
            {
                auto& Offsets = Previous ? _State->_SelectedPreviousOffsets : _State->_SelectedFollowingOffsets;
                Offsets.Reset();
                if (Offset != 0)
                {
                    Offsets.Add(Offset);
                }
                Refresh_Decorations();
            }, [this, Previous, Offset]
            {
                const auto& Offsets = Previous ? _State->_SelectedPreviousOffsets : _State->_SelectedFollowingOffsets;
                return Offset == 0 ? Offsets.IsEmpty() : Offsets.Contains(Offset);
            });
        }
    }
    return Menu.MakeWidget();
}

auto
    SCk_PixelArtViewer_Window::
    Build_PalettePanel()
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)
    + SVerticalBox::Slot().AutoHeight()
    [SNew(SButton).Text_Lambda([this] { return FText::FromString(_State->_HighlightPalette
        ? TEXT("Highlight selected on") : TEXT("Highlight selected off")); })
        .OnClicked_Lambda([this] { _State->_HighlightPalette = NOT _State->_HighlightPalette;
            _Canvas->Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled(); })]
    + SVerticalBox::Slot().AutoHeight()
    [SNew(SButton).Text(FText::FromString(TEXT("Preview override selected")))
        .ToolTipText(FText::FromString(TEXT("Cycle a temporary color for this source's selected palette index")))
        .OnClicked_Lambda([this]
        {
            if (_State->_SelectedPaletteIndex == INDEX_NONE)
            {
                return FReply::Handled();
            }
            const auto Index = _State->_SelectedPaletteIndex;
            auto Next = FColor::Red;
            if (_State->_TemporaryPaletteOverrides.Contains(Index))
            {
                const auto& Previous = _State->_TemporaryPaletteOverrides.FindChecked(Index);
                Next = Previous == FColor::Red ? FColor::Green
                    : Previous == FColor::Green ? FColor::Blue : FColor::Red;
            }
            _State->_TemporaryPaletteOverrides.Add(Index, Next);
            _OverrideGenerations.Remove(_CurrentSourceIndex);
            Refresh_Decorations();
            return FReply::Handled();
        })]
    + SVerticalBox::Slot().AutoHeight()
    [SNew(SButton).Text(FText::FromString(TEXT("Clear preview overrides")))
        .OnClicked_Lambda([this] { _State->_TemporaryPaletteOverrides.Reset();
            _SourceOverrides.Remove(_CurrentSourceIndex);
            _OverrideGenerations.Remove(_CurrentSourceIndex);
            Refresh_Decorations();
            return FReply::Handled(); })]
    + SVerticalBox::Slot().AutoHeight()
    [SNew(STextBlock).Text(FText::FromString(TEXT("Selected override RGBA")))]
    + SVerticalBox::Slot().AutoHeight()
    [SNew(SSlider).ToolTipText(FText::FromString(TEXT("Red channel")))
        .Value_Lambda([this]
        {
            return Get_SelectedPaletteColor().R / 255.0f;
        })
        .OnValueChanged_Lambda([this](float InValue)

        {
            Set_SelectedPaletteChannel(0, FMath::RoundToInt(InValue * 255.0f));
        })]
    + SVerticalBox::Slot().AutoHeight()
    [SNew(SSlider).ToolTipText(FText::FromString(TEXT("Green channel")))
        .Value_Lambda([this]
        {
            return Get_SelectedPaletteColor().G / 255.0f;
        })
        .OnValueChanged_Lambda([this](float InValue)

        {
            Set_SelectedPaletteChannel(1, FMath::RoundToInt(InValue * 255.0f));
        })]
    + SVerticalBox::Slot().AutoHeight()
    [SNew(SSlider).ToolTipText(FText::FromString(TEXT("Blue channel")))
        .Value_Lambda([this]
        {
            return Get_SelectedPaletteColor().B / 255.0f;
        })
        .OnValueChanged_Lambda([this](float InValue)

        {
            Set_SelectedPaletteChannel(2, FMath::RoundToInt(InValue * 255.0f));
        })]
    + SVerticalBox::Slot().AutoHeight()
    [SNew(SSlider).ToolTipText(FText::FromString(TEXT("Alpha channel")))
        .Value_Lambda([this]
        {
            return Get_SelectedPaletteColor().A / 255.0f;
        })
        .OnValueChanged_Lambda([this](float InValue)

        {
            Set_SelectedPaletteChannel(3, FMath::RoundToInt(InValue * 255.0f));
        })]
    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
    [SAssignNew(_PaletteList, SVerticalBox)];
}
