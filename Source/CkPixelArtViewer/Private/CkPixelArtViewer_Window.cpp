#include "CkPixelArtViewer/CkPixelArtViewer_Window.h"

#include "CkPixelArtViewer/CkPixelArtViewer_Canvas.h"
#include "CkPixelArtViewer/CkPixelArtViewer_Timeline.h"
#include "CkPixelArtViewer/CkPixelArtViewer_Settings.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkPixelSprite/CkPixelSprite_Resources.h"
#include "CkPixelSprite/CkPixelSprite_Render.h"
#include "CkPixelSpriteEditor/CkPixelSpriteEditor_Export.h"

#include "DesktopPlatformModule.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#include <cmath>

namespace
{
    auto
        TryPreparePreview(
            const FCk_PixelSprite_Animation& InAnimation,
            ECk_PixelSprite_Playback InPlayback,
            TSharedPtr<const FCk_PixelSprite_PreparedAnimation>& OutPrepared,
            double& OutCycleMilliseconds
        )
        -> bool
    {
        auto Preview = InAnimation;
        Preview._Playback = InPlayback;
        auto Candidate = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
        auto Selection = FCk_PixelSprite_FrameSelection{};
        if (NOT ck::pixel_sprite::TryPrepare_Animation(Preview, Candidate) ||
            NOT ck::pixel_sprite::TryEvaluate_PreparedAnimationMilliseconds(
                *Candidate, 0.0, Selection))
        {
            return false;
        }
        const auto Cycle = Selection._CycleDuration.Get_Seconds() * 1000.0;
        if (NOT std::isfinite(Cycle) || Cycle <= 0.0)
        {
            return false;
        }
        OutPrepared = MoveTemp(Candidate);
        OutCycleMilliseconds = Cycle;
        return true;
    }
}

auto
    SCk_PixelArtViewer_Window::
    Construct(
        const FArguments&
    )
    -> void
{
    _State = MakeShared<FCk_PixelArtViewer_State>();
    const auto Preferences = TObjectPtr<const UCkPixelArtViewerSettings>{
        GetDefault<UCkPixelArtViewerSettings>()};
    _State->_ShowChecker = Preferences->_ShowChecker;
    _State->_ShowGrid = Preferences->_ShowGrid;
    _State->_ShowAxes = Preferences->_ShowAxes;
    _State->_ShowCrosshair = Preferences->_ShowCrosshair;
    _State->_ShowOnion = Preferences->_ShowOnion;
    _State->_PreviousOnionCount = FMath::Clamp(Preferences->_PreviousCount, 0, 4);
    _State->_FollowingOnionCount = FMath::Clamp(Preferences->_FollowingCount, 0, 4);
    _State->_OnionOpacity = FMath::Clamp(Preferences->_OnionOpacity, 0.0f, 0.75f);
    _State->_PreviousTint = Preferences->_PreviousTint;
    _State->_FollowingTint = Preferences->_FollowingTint;
    _State->_RenderSettings._Mode = Preferences->_RenderMode;
    _State->_RenderSettings._Scale = FMath::Clamp(Preferences->_RenderScale, 1, 8);
    _State->_RenderSettings._Outline = Preferences->_Outline;
    _State->_RenderSettings._OutlineColor = Preferences->_OutlineColor;
    _State->_RenderSettings._Background = Preferences->_Background;
    _Layout = Preferences->_Layout == ECk_PixelArtViewer_Layout::Focused
        ? ECk_PixelArtViewer_Layout::Focused : ECk_PixelArtViewer_Layout::Studio;
    _InspectorVisible = _Layout == ECk_PixelArtViewer_Layout::Studio;
    _BrowserPanel = Build_Browser();
    _InspectorPanel = Build_Inspector();
    _CenterPanel = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Build_ViewportToolbar()]
        + SVerticalBox::Slot().FillHeight(1)
        [SAssignNew(_Canvas, SCk_PixelArtViewer_Canvas).State(_State)]
        + SVerticalBox::Slot().AutoHeight()[Build_Timeline()]
        + SVerticalBox::Slot().AutoHeight()[Build_Transport()];
    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(TEXT("CkPixelArtViewer"))
        .ToolTabId(TEXT("CkPixelArtViewer"))
        .StatusText(this, &SCk_PixelArtViewer_Window::Get_StatusText)
        .Content()
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Build_AssetToolbar()]
            + SVerticalBox::Slot().FillHeight(1)[SAssignNew(_Workspace, SBox)]
            + SVerticalBox::Slot().AutoHeight().Padding(8, 3)
            [SNew(STextBlock).Text_Lambda([this]
            {
                return _Canvas.IsValid() ? _Canvas->Get_HoverText() : FText::GetEmpty();
            })]]
    ];
    Rebuild_Workspace();
}

auto
    SCk_PixelArtViewer_Window::
    Set_Asset(
        UCk_PixelSpriteAsset_UE& InAsset
    )
    -> bool
{
    auto PreviousName = FString{};
    if (_Snapshot.IsValid() && _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        PreviousName = _Snapshot->_Animations[_AnimationIndex]._Name;
    }
    const auto CandidateSnapshot = InAsset.Get_Snapshot();
    if (NOT CandidateSnapshot.IsValid())
    {
        _Status = TEXT("Asset has no valid imported pixel data; previous view retained");
        return false;
    }
    auto CandidateResources = TStrongObjectPtr<UCk_PixelSpriteResources_UE>{
        NewObject<UCk_PixelSpriteResources_UE>(GetTransientPackage())};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT CandidateResources->TryBuild(
        TObjectPtr<UCk_PixelSpriteAsset_UE>{&InAsset}, _State->_RenderSettings, Diagnostic))
    {
        _Status = Diagnostic.Get_Text();
        return false;
    }

    auto CandidateAnimationIndex = CandidateSnapshot->_Animations.IsEmpty() ? INDEX_NONE
        : FMath::Clamp(_AnimationIndex, 0, CandidateSnapshot->_Animations.Num() - 1);
    for (int32 Index = 0; Index < CandidateSnapshot->_Animations.Num(); ++Index)
    {
        if (CandidateSnapshot->_Animations[Index]._Name.Equals(PreviousName, ESearchCase::CaseSensitive))
        {
            CandidateAnimationIndex = Index; break;
        }
    }
    auto CandidatePrepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    auto CandidateCycleMilliseconds = 0.0;
    auto CandidatePlayback = _State->_Playback;
    if (CandidateSnapshot->_Animations.IsValidIndex(CandidateAnimationIndex))
    {
        CandidatePlayback = CandidateSnapshot->_Animations[CandidateAnimationIndex]._Playback;
        if (NOT TryPreparePreview(CandidateSnapshot->_Animations[CandidateAnimationIndex],
            CandidatePlayback, CandidatePrepared, CandidateCycleMilliseconds))
        {
            _Status = TEXT("Animation timing is invalid; previous view retained");
            return false;
        }
    }

    _Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{&InAsset};
    _Resources = MoveTemp(CandidateResources);
    _Snapshot = CandidateSnapshot;
    _Prepared = MoveTemp(CandidatePrepared);
    _PreparedCycleMilliseconds = CandidateCycleMilliseconds;
    _PreparedRevision = InAsset.Get_Revision();
    _PreparedAnimationIndex = CandidateAnimationIndex;
    _PreparedPlayback = CandidatePlayback;
    _AppliedRenderSettings = _State->_RenderSettings;
    _RevisionSeen = InAsset.Get_Revision();
    _CurrentSourceIndex = INDEX_NONE;
    _OverrideSourceIndex = INDEX_NONE;
    _SourceOverrides.Reset();
    _OverrideGenerations.Reset();
    _DifferenceAsset.Reset();
    _DifferenceResources.Reset();
    _DifferenceSourceA = INDEX_NONE;
    _DifferenceSourceB = INDEX_NONE;
    _State->_TemporaryPaletteOverrides.Reset();
    _AnimationIndex = CandidateAnimationIndex;
    _State->_Playback = CandidatePlayback;
    _Status = TEXT("Ready");
    Rebuild_AnimationList();
    Rebuild_Sources();
    Rebuild_Timeline();
    Refresh_Frame();
    return true;
}

auto
    SCk_PixelArtViewer_Window::
    Get_Asset() const
    -> TObjectPtr<UCk_PixelSpriteAsset_UE>
{
    return _Asset.Get();
}

auto
    SCk_PixelArtViewer_Window::
    Get_Resources() const
    -> TObjectPtr<UCk_PixelSpriteResources_UE>
{
    return _Resources.Get();
}

auto
    SCk_PixelArtViewer_Window::
    Get_State() const
    -> TSharedPtr<FCk_PixelArtViewer_State>
{
    return _State;
}

auto
    SCk_PixelArtViewer_Window::
    Get_Canvas() const
    -> TSharedPtr<SCk_PixelArtViewer_Canvas>
{
    return _Canvas;
}

auto
    SCk_PixelArtViewer_Window::
    Get_SelectedFrame() const
    -> int32
{
    return _SelectedFrame;
}

auto
    SCk_PixelArtViewer_Window::
    Get_SelectedAnimationName() const
    -> FString
{
    return _Snapshot.IsValid() && _Snapshot->_Animations.IsValidIndex(_AnimationIndex)
        ? _Snapshot->_Animations[_AnimationIndex]._Name : FString{};
}

auto
    SCk_PixelArtViewer_Window::
    Get_Prepared() const
    -> TSharedPtr<const FCk_PixelSprite_PreparedAnimation>
{
    return _Prepared;
}

auto
    SCk_PixelArtViewer_Window::
    Get_MainSplitter() const
    -> TSharedPtr<SSplitter>
{
    return _MainSplitter;
}

auto
    SCk_PixelArtViewer_Window::
    Get_ChangedPixels() const
    -> int32
{
    return _ChangedPixels;
}

auto
    SCk_PixelArtViewer_Window::
    Seek_Milliseconds(
        double InMilliseconds
    )
    -> bool
{
    if (NOT _State->Seek(InMilliseconds))
    {
        return false;
    }
    _State->_Playing = false;
    Refresh_Frame();
    return true;
}

auto
    SCk_PixelArtViewer_Window::
    Refresh_Asset()
    -> bool
{
    if (NOT _Asset.IsValid() || _Asset->Get_Revision() == _RevisionSeen)
    {
        return false;
    }
    return Set_Asset(*_Asset);
}

auto
    SCk_PixelArtViewer_Window::
    Ensure_Prepared()
    -> bool
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        return false;
    }
    if (_Prepared.IsValid() && _PreparedRevision == _RevisionSeen &&
        _PreparedAnimationIndex == _AnimationIndex && _PreparedPlayback == _State->_Playback)
    {
        return true;
    }
    auto Candidate = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    auto CycleMilliseconds = 0.0;
    if (NOT TryPreparePreview(_Snapshot->_Animations[_AnimationIndex], _State->_Playback,
        Candidate, CycleMilliseconds))
    {
        return false;
    }
    _Prepared = MoveTemp(Candidate);
    _PreparedCycleMilliseconds = CycleMilliseconds;
    _PreparedRevision = _RevisionSeen;
    _PreparedAnimationIndex = _AnimationIndex;
    _PreparedPlayback = _State->_Playback;
    return true;
}

auto
    SCk_PixelArtViewer_Window::
    Refresh_Frame()
    -> void
{
    if (NOT _Resources.IsValid() || NOT Ensure_Prepared())
    {
        return;
    }
    const auto& Animation = _Snapshot->_Animations[_AnimationIndex];
    auto Selection = FCk_PixelSprite_FrameSelection{};
    if (NOT _State->Try_Select(*_Prepared, Animation, Selection) ||
        NOT Animation._Frames.IsValidIndex(Selection._Index))
    {
        return;
    }
    const auto FrameChanged = _SelectedFrame != Selection._Index;
    _SelectedFrame = Selection._Index;
    const auto SourceIndex = Animation._Frames[Selection._Index]._SourceIndex;
    if (NOT _Canvas.IsValid())
    {
        return;
    }
    if (SourceIndex != _CurrentSourceIndex)
    {
        if (_OverrideSourceIndex != SourceIndex)
        {
            if (_OverrideSourceIndex != INDEX_NONE &&
                NOT _State->_TemporaryPaletteOverrides.IsEmpty())
            {
                _SourceOverrides.Add(_OverrideSourceIndex, _State->_TemporaryPaletteOverrides);
            }
            _State->_TemporaryPaletteOverrides = _SourceOverrides.FindRef(SourceIndex);
            _OverrideSourceIndex = SourceIndex;
            _State->_SelectedPaletteIndex = INDEX_NONE;
        }
        auto Texture = _Resources->Get_Texture(SourceIndex);
        if (NOT Texture)
        {
            return;
        }
        _Canvas->Set_Frame(Texture, _Snapshot, SourceIndex);
        _CurrentSourceIndex = SourceIndex;
        Rebuild_Palette();
    }
    if (FrameChanged || SourceIndex != _DifferenceSourceA)
    {
        Refresh_Decorations();
    }
    if (Selection._Complete && _State->_Playback == ECk_PixelSprite_Playback::Once &&
        NOT _State->_Reverse)
    {
        _State->_Playing = false;
    }
}

auto
    SCk_PixelArtViewer_Window::
    Rebuild_AnimationList()
    -> void
{
    if (NOT _AnimationList.IsValid())
    {
        return;
    }
    _AnimationList->ClearChildren();
    if (NOT _Snapshot.IsValid())
    {
        return;
    }
    for (int32 Index = 0; Index < _Snapshot->_Animations.Num(); ++Index)
    {
        const auto Label = FText::FromString(_Snapshot->_Animations[Index]._Name);
        _AnimationList->AddSlot().AutoHeight().Padding(2)
        [SNew(SButton).Text(Label)
            .OnClicked_Lambda([this, Index]
            {
                Select_Animation(Index);
                return FReply::Handled();
            })];
    }
}

auto
    SCk_PixelArtViewer_Window::
    Rebuild_Sources()
    -> void
{
    if (NOT _SourceTree.IsValid())
    {
        return;
    }
    _SourceRoots.Reset();
    if (NOT _Snapshot.IsValid())
    {
        _SourceTree->RequestTreeRefresh();
        return;
    }
    auto Folders = TArray<TSharedPtr<FSourceNode>>{};
    for (int32 Index = 0; Index < _Snapshot->_Sources.Num(); ++Index)
    {
        auto Path = FPaths::GetCleanFilename(_Snapshot->_Sources[Index]._Path);
        auto FoundReference = false;
        for (const auto& Animation : _Snapshot->_Animations)
        {
            for (const auto& Frame : Animation._Frames)
            {
                if (Frame._SourceIndex == Index && NOT Frame._Source.IsEmpty())
                {
                    Path = Frame._Source;
                    FoundReference = true;
                    break;
                }
            }
            if (FoundReference)
            {
                break;
            }
        }
        if (NOT FPaths::IsRelative(Path))
        {
            Path = FPaths::GetCleanFilename(Path);
        }
        Path.ReplaceInline(TEXT("\\"), TEXT("/"));
        auto Components = TArray<FString>{};
        Path.ParseIntoArray(Components, TEXT("/"), true);
        auto Parent = TSharedPtr<FSourceNode>{};
        for (int32 Part = 0; Part < Components.Num(); ++Part)
        {
            auto& Siblings = Parent.IsValid() ? Parent->_Children : _SourceRoots;
            const auto IsLeaf = Part == Components.Num() - 1;
            auto Match = Siblings.IndexOfByPredicate([&](const TSharedPtr<FSourceNode>& InNode)
            {
                return InNode.IsValid() &&
                    InNode->_Name.Equals(Components[Part], ESearchCase::CaseSensitive) &&
                    (IsLeaf ? InNode->_SourceIndex == Index
                        : InNode->_SourceIndex == INDEX_NONE);
            });
            if (Match == INDEX_NONE)
            {
                auto Node = MakeShared<FSourceNode>();
                Node->_Name = Components[Part];
                Node->_Path = IsLeaf ? _Snapshot->_Sources[Index]._Path
                    : Parent.IsValid() ? Parent->_Path / Components[Part] : Components[Part];
                Node->_SourceIndex = IsLeaf ? Index : INDEX_NONE;
                Match = Siblings.Add(Node);
                if (NOT IsLeaf)
                {
                    Folders.Add(Node);
                }
            }
            Parent = Siblings[Match];
        }
    }
    _SourceTree->RequestTreeRefresh();
    for (const auto& Folder : Folders)
    {
        _SourceTree->SetItemExpansion(Folder, true);
    }
}

auto
    SCk_PixelArtViewer_Window::
    Generate_SourceRow(
        TSharedPtr<FSourceNode> InNode,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    return SNew(STableRow<TSharedPtr<FSourceNode>>, InOwnerTable)
    [SNew(SButton)
        .Text(FText::FromString(InNode.IsValid() ? InNode->_Name : FString{}))
        .ToolTipText(FText::FromString(InNode.IsValid() ? InNode->_Path : FString{}))
        .OnClicked_Lambda([this, InNode]
        {
            if (NOT InNode.IsValid())
            {
                return FReply::Handled();
            }
            if (InNode->_SourceIndex != INDEX_NONE)
            {
                Select_Source(InNode->_SourceIndex);
            }
            else if (_SourceTree.IsValid())
            {
                _SourceTree->SetItemExpansion(InNode, NOT _SourceTree->IsItemExpanded(InNode));
            }
            return FReply::Handled();
        })];
}

auto
    SCk_PixelArtViewer_Window::
    Get_SourceChildren(
        TSharedPtr<FSourceNode> InNode,
        TArray<TSharedPtr<FSourceNode>>& OutChildren) const
    -> void
{
    if (InNode.IsValid())
    {
        OutChildren = InNode->_Children;
    }
}

auto
    SCk_PixelArtViewer_Window::
    Select_Source(
        int32 InSourceIndex)
    -> void
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Sources.IsValidIndex(InSourceIndex))
    {
        return;
    }
    for (int32 AnimationIndex = 0; AnimationIndex < _Snapshot->_Animations.Num(); ++AnimationIndex)
    {
        const auto& Animation = _Snapshot->_Animations[AnimationIndex];
        for (int32 FrameIndex = 0; FrameIndex < Animation._Frames.Num(); ++FrameIndex)
        {
            if (Animation._Frames[FrameIndex]._SourceIndex != InSourceIndex)
            {
                continue;
            }
            if (AnimationIndex != _AnimationIndex)
            {
                Select_Animation(AnimationIndex);
                if (_AnimationIndex != AnimationIndex)
                {
                    return;
                }
            }
            if (Ensure_Prepared() &&
                _State->Select_Frame(*_Prepared, _Snapshot->_Animations[_AnimationIndex], FrameIndex))
            {
                _State->_Playing = false;
                Refresh_Frame();
            }
            return;
        }
    }
    _Status = TEXT("Selected source is not referenced by an animation frame");
}

auto
    SCk_PixelArtViewer_Window::
    Rebuild_Timeline()
    -> void
{
    if (NOT _Timeline.IsValid())
    {
        return;
    }
    auto Frames = TArray<FCk_PixelArtViewer_TimelineFrame>{};
    if (_Snapshot.IsValid() && _Resources.IsValid() && _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        const auto& Animation = _Snapshot->_Animations[_AnimationIndex];
        for (const auto& Frame : Animation._Frames)
        {
            const auto Texture = _Resources->Get_Texture(Frame._SourceIndex);
            if (NOT Texture)
            {
                return;
            }
            auto Brush = MakeShared<FSlateBrush>();
            Brush->DrawAs = ESlateBrushDrawType::Image;
            Brush->SetResourceObject(Texture.Get());
            Brush->ImageSize = FVector2D{56.0, 56.0};
            Frames.Add({Frame._DurationMilliseconds, Frame._Source, Brush});
        }
    }
    _Timeline->Set_Frames(MoveTemp(Frames));
}

auto
    SCk_PixelArtViewer_Window::
    Rebuild_Palette()
    -> void
{
    if (NOT _PaletteList.IsValid())
    {
        return;
    }
    _PaletteList->ClearChildren();
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Sources.IsValidIndex(_CurrentSourceIndex))
    {
        return;
    }
    const auto& Grid = _Snapshot->_Sources[_CurrentSourceIndex]._Grid;
    auto Usage = TArray<int32>{};
    Usage.Init(0, Grid._Palette.Num());
    for (const auto Index : Grid._Indices)
    {
        if (Usage.IsValidIndex(Index))
        {
            ++Usage[Index];
        }
    }
    for (int32 Index = 0; Index < Grid._Palette.Num(); ++Index)
    {
        const auto& Entry = Grid._Palette[Index];
        const auto Color = Entry._Color;
        const auto Label = FString::Printf(TEXT("%s%d  '%s'  #%02X%02X%02X%02X  %d px%s"),
            _State->_SelectedPaletteIndex == Index ? TEXT("▶ ") : TEXT(""),
            Index, *Entry._Symbol, Color.R, Color.G, Color.B, Color.A, Usage[Index],
            Color.A == 0 ? TEXT("  transparent") : TEXT(""));
        _PaletteList->AddSlot().AutoHeight().Padding(2)
        [SNew(SButton)
            .OnClicked_Lambda([this, Index]
            {
                _State->_SelectedPaletteIndex = Index;
                Rebuild_Palette();
                _Canvas->Invalidate(EInvalidateWidgetReason::Paint);
                return FReply::Handled();
            })
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(2)
                [SNew(SBox).WidthOverride(20.0f).HeightOverride(20.0f)
                    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(FLinearColor{Color})]]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [SNew(STextBlock).Text(FText::FromString(Label))]
            ]];
    }
}

auto
    SCk_PixelArtViewer_Window::
    Build_DerivedResources(
        const FCk_PixelSprite_Grid& InGrid,
        const FCk_PixelSprite_RenderSettings& InSettings,
        TStrongObjectPtr<UCk_PixelSpriteAsset_UE>& OutAsset,
        TStrongObjectPtr<UCk_PixelSpriteResources_UE>& OutResources
    )
    -> bool
{
    auto Manifest = FCk_PixelSprite_Manifest{};
    Manifest._Width = InGrid._Width;
    Manifest._Height = InGrid._Height;
    auto Source = FCk_PixelSprite_Source{};
    Source._Path = TEXT("preview.xpm");
    Source._Bytes = 1;
    Source._Grid = InGrid;
    Manifest._Sources.Add(MoveTemp(Source));
    auto Animation = FCk_PixelSprite_Animation{};
    Animation._Name = TEXT("default");
    auto Frame = FCk_PixelSprite_Frame{};
    Frame._Source = TEXT("preview.xpm");
    Frame._SourceIndex = 0;
    Frame._DurationMilliseconds = 100.0;
    Animation._Frames.Add(MoveTemp(Frame));
    Manifest._Animations.Add(MoveTemp(Animation));
    auto CandidateAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        NewObject<UCk_PixelSpriteAsset_UE>(GetTransientPackage())};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT CandidateAsset->Request_ReplaceManifest(Manifest, Diagnostic))
    {
        _Status = Diagnostic.Get_Text();
        return false;
    }
    auto CandidateResources = TStrongObjectPtr<UCk_PixelSpriteResources_UE>{
        NewObject<UCk_PixelSpriteResources_UE>(GetTransientPackage())};
    if (NOT CandidateResources->TryBuild(
        TObjectPtr<UCk_PixelSpriteAsset_UE>{CandidateAsset.Get()}, InSettings, Diagnostic))
    {
        _Status = Diagnostic.Get_Text();
        return false;
    }
    OutAsset = MoveTemp(CandidateAsset);
    OutResources = MoveTemp(CandidateResources);
    return true;
}

auto
    SCk_PixelArtViewer_Window::
    Refresh_Decorations()
    -> void
{
    if (NOT _Snapshot.IsValid() || NOT _Resources.IsValid() || NOT _Canvas.IsValid() ||
        NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex) ||
        NOT _Snapshot->_Sources.IsValidIndex(_CurrentSourceIndex))
    {
        return;
    }
    const auto& Animation = _Snapshot->_Animations[_AnimationIndex];
    const auto Count = Animation._Frames.Num();
    if (NOT Animation._Frames.IsValidIndex(_SelectedFrame))
    {
        return;
    }
    auto Previous = TArray<TObjectPtr<UTexture2D>>{};
    auto Following = TArray<TObjectPtr<UTexture2D>>{};
    if (_State->_ShowOnion)
    {
        for (int32 Offset = 1; Offset <= _State->_PreviousOnionCount; ++Offset)
        {
            const auto Index = _SelectedFrame - Offset;
            if (Animation._Frames.IsValidIndex(Index) &&
                (_State->_SelectedPreviousOffsets.IsEmpty() ||
                    _State->_SelectedPreviousOffsets.Contains(Offset)))
            {
                Previous.Add(_Resources->Get_Texture(Animation._Frames[Index]._SourceIndex));
            }
        }
        for (int32 Offset = 1; Offset <= _State->_FollowingOnionCount; ++Offset)
        {
            const auto Index = _SelectedFrame + Offset;
            if (Animation._Frames.IsValidIndex(Index) &&
                (_State->_SelectedFollowingOffsets.IsEmpty() ||
                    _State->_SelectedFollowingOffsets.Contains(Offset)))
            {
                Following.Add(_Resources->Get_Texture(Animation._Frames[Index]._SourceIndex));
            }
        }
    }
    auto ComparisonTexture = TObjectPtr<UTexture2D>{};
    auto DifferenceTexture = TObjectPtr<UTexture2D>{};
    _ChangedPixels = 0;
    _DifferenceBounds.Reset();
    auto Target = _State->_ComparisonFrame;
    if (Target == INDEX_NONE || Target < 0 || Target >= Count)
    {
        Target = Count > 1 ? (_SelectedFrame + 1) % Count : _SelectedFrame;
    }
    if (_State->_Comparison != ECk_PixelArtViewer_Comparison::None &&
        Animation._Frames.IsValidIndex(Target))
    {
        const auto TargetSource = Animation._Frames[Target]._SourceIndex;
        ComparisonTexture = _Resources->Get_Texture(TargetSource);
        if (_Snapshot->_Sources.IsValidIndex(TargetSource))
        {
            if (_DifferenceSourceA != _CurrentSourceIndex ||
                _DifferenceSourceB != TargetSource || NOT _DifferenceResources.IsValid())
            {
                auto Difference = FCk_PixelSprite_Difference{};
                auto Diagnostic = FCk_PixelSprite_Diagnostic{};
                if (ck::pixel_sprite::TryCompare_Grids(
                    _Snapshot->_Sources[_CurrentSourceIndex]._Grid,
                    _Snapshot->_Sources[TargetSource]._Grid, Difference, Diagnostic))
                {
                    auto Grid = FCk_PixelSprite_Grid{};
                    Grid._Width = Difference._Image._Width;
                    Grid._Height = Difference._Image._Height;
                    Grid._Palette = {{TEXT("."), FColor::Transparent},
                        {TEXT("X"), FColor{255, 64, 128, 255}}};
                    Grid._Indices.Reserve(Difference._Image._Pixels.Num());
                    for (const auto Pixel : Difference._Image._Pixels)
                    {
                        Grid._Indices.Add(Pixel.A == 0 ? 0 : 1);
                    }
                    auto Settings = FCk_PixelSprite_RenderSettings{};
                    if (Build_DerivedResources(Grid, Settings,
                        _DifferenceAsset, _DifferenceResources))
                    {
                        _ChangedPixels = Difference._ChangedPixels;
                        _DifferenceBounds = Difference._Bounds;
                        _CachedChangedPixels = _ChangedPixels;
                        _CachedDifferenceBounds = _DifferenceBounds;
                        _DifferenceSourceA = _CurrentSourceIndex;
                        _DifferenceSourceB = TargetSource;
                    }
                }
                else
                {
                    _Status = Diagnostic.Get_Text();
                }
            }
            DifferenceTexture = _DifferenceResources.IsValid()
                ? _DifferenceResources->Get_Texture(0) : nullptr;
            if (_DifferenceSourceA == _CurrentSourceIndex &&
                _DifferenceSourceB == TargetSource && _DifferenceResources.IsValid())
            {
                _ChangedPixels = _CachedChangedPixels;
                _DifferenceBounds = _CachedDifferenceBounds;
            }
        }
    }
    if (NOT _State->_TemporaryPaletteOverrides.IsEmpty())
    {
        auto Generation = _OverrideGenerations.FindRef(_CurrentSourceIndex);
        if (NOT Generation.IsValid())
        {
            Generation = MakeShared<FOverrideGeneration>();
            auto Settings = _State->_RenderSettings;
            Settings._PaletteOverrides = _State->_TemporaryPaletteOverrides;
            if (Build_DerivedResources(_Snapshot->_Sources[_CurrentSourceIndex]._Grid,
                Settings, Generation->_Asset, Generation->_Resources))
            {
                _OverrideGenerations.Add(_CurrentSourceIndex, Generation);
            }
        }
        if (Generation.IsValid() && Generation->_Resources.IsValid())
        {
            _Canvas->Set_Frame(Generation->_Resources->Get_Texture(0), _Snapshot, _CurrentSourceIndex);
        }
    }
    else
    {
        _Canvas->Set_Frame(_Resources->Get_Texture(_CurrentSourceIndex), _Snapshot, _CurrentSourceIndex);
    }
    _Canvas->Set_Decorations(Previous, Following, ComparisonTexture,
        DifferenceTexture, _DifferenceBounds);
    if (_State->_Comparison == ECk_PixelArtViewer_Comparison::None)
    {
        _DifferenceSourceA = _CurrentSourceIndex;
    }
}

auto
    SCk_PixelArtViewer_Window::
    Rebuild_BaseResources()
    -> bool
{
    if (NOT _Asset.IsValid())
    {
        return false;
    }
    auto Candidate = TStrongObjectPtr<UCk_PixelSpriteResources_UE>{
        NewObject<UCk_PixelSpriteResources_UE>(GetTransientPackage())};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT Candidate->TryBuild(TObjectPtr<UCk_PixelSpriteAsset_UE>{_Asset.Get()},
        _State->_RenderSettings, Diagnostic))
    {
        _State->_RenderSettings = _AppliedRenderSettings;
        _Status = Diagnostic.Get_Text();
        return false;
    }
    _Resources = MoveTemp(Candidate);
    _AppliedRenderSettings = _State->_RenderSettings;
    _OverrideGenerations.Reset();
    _CurrentSourceIndex = INDEX_NONE;
    _DifferenceResources.Reset();
    _DifferenceSourceA = INDEX_NONE;
    Rebuild_Timeline();
    Refresh_Frame();
    _Status = TEXT("Preview rendering updated");
    return true;
}

auto
    SCk_PixelArtViewer_Window::
    Select_Animation(
        int32 InIndex
    )
    -> void
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Animations.IsValidIndex(InIndex))
    {
        return;
    }
    auto Candidate = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    auto CycleMilliseconds = 0.0;
    const auto Playback = _Snapshot->_Animations[InIndex]._Playback;
    if (NOT TryPreparePreview(_Snapshot->_Animations[InIndex], Playback,
        Candidate, CycleMilliseconds))
    {
        _Status = TEXT("Cannot prepare selected animation");
        return;
    }
    _Prepared = MoveTemp(Candidate);
    _PreparedCycleMilliseconds = CycleMilliseconds;
    _PreparedRevision = _RevisionSeen;
    _PreparedAnimationIndex = InIndex;
    _PreparedPlayback = Playback;
    _AnimationIndex = InIndex;
    _State->_Playback = Playback;
    _State->Seek(0.0);
    _State->_Playing = false;
    _CurrentSourceIndex = INDEX_NONE;
    _State->_ComparisonFrame = INDEX_NONE;
    Rebuild_Timeline();
    Refresh_Frame();
}

auto
    SCk_PixelArtViewer_Window::
    Step_Frame(
        int32 InDirection
    )
    -> void
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        return;
    }
    if (Ensure_Prepared() &&
        _State->Step_Frame(*_Prepared, _Snapshot->_Animations[_AnimationIndex], InDirection))
    {
        Refresh_Frame();
    }
}

auto
    SCk_PixelArtViewer_Window::
    Jump_Edge(
        bool InLast
    )
    -> void
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        return;
    }
    const auto& Frames = _Snapshot->_Animations[_AnimationIndex]._Frames;
    if (Frames.IsEmpty())
    {
        return;
    }
    _State->_Playing = false;
    if (Ensure_Prepared())
    {
        _State->Select_Frame(*_Prepared, _Snapshot->_Animations[_AnimationIndex], InLast ? Frames.Num() - 1 : 0);
    }
    Refresh_Frame();
}

auto
    SCk_PixelArtViewer_Window::
    Save_LayoutPreferences()
    -> void
{
    if (_Layout != ECk_PixelArtViewer_Layout::Studio || NOT _InspectorVisible
        || NOT _MainSplitter.IsValid() || NOT _MainSplitter->IsValidSlotIndex(2))
    {
        return;
    }
    const auto Left = _MainSplitter->SlotAt(0).GetSizeValue();
    const auto Canvas = _MainSplitter->SlotAt(1).GetSizeValue();
    const auto Right = _MainSplitter->SlotAt(2).GetSizeValue();
    const auto Sum = Left + Canvas + Right;
    if (NOT std::isfinite(Left) || NOT std::isfinite(Canvas) ||
        NOT std::isfinite(Right) || NOT std::isfinite(Sum) || Sum <= 0.0f ||
        Left / Sum < 0.03f || Canvas / Sum < 0.03f || Right / Sum < 0.03f)
    {
        return;
    }
    auto Preferences = TObjectPtr<UCkPixelArtViewerSettings>{
        GetMutableDefault<UCkPixelArtViewerSettings>()};
    const auto NewLeft = Left / Sum;
    const auto NewCanvas = Canvas / Sum;
    const auto NewRight = Right / Sum;
    if (FMath::IsNearlyEqual(Preferences->_LeftPanelWeight, NewLeft, 0.0001f) &&
        FMath::IsNearlyEqual(Preferences->_CanvasPanelWeight, NewCanvas, 0.0001f) &&
        FMath::IsNearlyEqual(Preferences->_RightPanelWeight, NewRight, 0.0001f))
    {
        return;
    }
    Preferences->_LeftPanelWeight = NewLeft;
    Preferences->_CanvasPanelWeight = NewCanvas;
    Preferences->_RightPanelWeight = NewRight;
    Preferences->SaveConfig();
}

auto
    SCk_PixelArtViewer_Window::
    Save_Preferences()
    -> void
{
    auto Preferences = TObjectPtr<UCkPixelArtViewerSettings>{
        GetMutableDefault<UCkPixelArtViewerSettings>()};
    Preferences->_ShowChecker = _State->_ShowChecker;
    Preferences->_ShowGrid = _State->_ShowGrid;
    Preferences->_ShowAxes = _State->_ShowAxes;
    Preferences->_ShowCrosshair = _State->_ShowCrosshair;
    Preferences->_ShowOnion = _State->_ShowOnion;
    Preferences->_PreviousCount = _State->_PreviousOnionCount;
    Preferences->_FollowingCount = _State->_FollowingOnionCount;
    Preferences->_OnionOpacity = _State->_OnionOpacity;
    Preferences->_PreviousTint = _State->_PreviousTint;
    Preferences->_FollowingTint = _State->_FollowingTint;
    Preferences->_RenderMode = _State->_RenderSettings._Mode;
    Preferences->_RenderScale = _State->_RenderSettings._Scale;
    Preferences->_Outline = _State->_RenderSettings._Outline;
    Preferences->_OutlineColor = _State->_RenderSettings._OutlineColor;
    Preferences->_Background = _State->_RenderSettings._Background;
    Preferences->SaveConfig();
}

auto
    SCk_PixelArtViewer_Window::
    Set_RenderMode(
        ECk_PixelSprite_RenderMode InMode
    )
    -> void
{
    _State->_RenderSettings._Mode = InMode;
    if (Rebuild_BaseResources())
    {
        Save_Preferences();
    }
}

auto
    SCk_PixelArtViewer_Window::
    Set_RenderScale(
        int32 InScale
    )
    -> void
{
    _State->_RenderSettings._Scale = FMath::Clamp(InScale, 1, 8);
    if (Rebuild_BaseResources())
    {
        Save_Preferences();
    }
}

auto
    SCk_PixelArtViewer_Window::
    Get_SelectedPaletteColor() const
    -> FColor
{
    if (_State->_TemporaryPaletteOverrides.Contains(_State->_SelectedPaletteIndex))
    {
        return _State->_TemporaryPaletteOverrides.FindChecked(_State->_SelectedPaletteIndex);
    }
    if (_Snapshot.IsValid() && _Snapshot->_Sources.IsValidIndex(_CurrentSourceIndex))
    {
        const auto& Palette = _Snapshot->_Sources[_CurrentSourceIndex]._Grid._Palette;
        if (Palette.IsValidIndex(_State->_SelectedPaletteIndex))
        {
            return Palette[_State->_SelectedPaletteIndex]._Color;
        }
    }
    return FColor::Transparent;
}

auto
    SCk_PixelArtViewer_Window::
    Set_SelectedPaletteChannel(
        int32 InChannel,
        int32 InValue
    )
    -> void
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Sources.IsValidIndex(_CurrentSourceIndex) ||
        NOT _Snapshot->_Sources[_CurrentSourceIndex]._Grid._Palette.IsValidIndex(
            _State->_SelectedPaletteIndex))
    {
        return;
    }
    auto Color = Get_SelectedPaletteColor();
    const auto Value = static_cast<uint8>(FMath::Clamp(InValue, 0, 255));
    switch (InChannel)
    {
    case 0: Color.R = Value; break;
    case 1: Color.G = Value; break;
    case 2: Color.B = Value; break;
    case 3: Color.A = Value; break;
    default: return;
    }
    _State->_TemporaryPaletteOverrides.Add(_State->_SelectedPaletteIndex, Color);
    _OverrideGenerations.Remove(_CurrentSourceIndex);
    Refresh_Decorations();
}

auto
    SCk_PixelArtViewer_Window::
    Export_Frame()
    -> void
{
    if (NOT _Asset.IsValid() || NOT _Snapshot.IsValid() ||
        NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex) || _SelectedFrame == INDEX_NONE)
    {
        _Status = TEXT("Select an animation frame before exporting");
        return;
    }
    if (FDesktopPlatformModule::Get() == nullptr)
    {
        _Status = TEXT("Native save dialog is unavailable");
        return;
    }
    auto& Desktop = *FDesktopPlatformModule::Get();
    auto Outputs = TArray<FString>{};
    const auto& Name = _Snapshot->_Animations[_AnimationIndex]._Name;
    if (NOT Desktop.SaveFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
        TEXT("Export current pixel sprite frame as a new PNG"),
        FPaths::ProjectSavedDir(), Name + TEXT(".png"),
        TEXT("PNG image|*.png"), EFileDialogFlags::None, Outputs) || Outputs.IsEmpty())
    {
        return;
    }
    auto Settings = _State->_RenderSettings;
    if (NOT _State->_TemporaryPaletteOverrides.IsEmpty())
    {
        Settings._PaletteOverrides = _State->_TemporaryPaletteOverrides;
    }
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT ck::pixel_sprite_editor::TryExport_Frame(*_Asset, Name, _SelectedFrame,
        Settings, Outputs[0], Diagnostic))
    {
        _Status = Diagnostic.Get_Text();
        return;
    }
    _Status = FString::Printf(TEXT("Exported new PNG: %s"), *Outputs[0]);
}

auto
    SCk_PixelArtViewer_Window::
    Export_Sheet()
    -> void
{
    if (NOT _Asset.IsValid() || NOT _Snapshot.IsValid() ||
        NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        _Status = TEXT("Select an animation before exporting");
        return;
    }
    if (FDesktopPlatformModule::Get() == nullptr)
    {
        _Status = TEXT("Native save dialog is unavailable");
        return;
    }
    auto& Desktop = *FDesktopPlatformModule::Get();
    auto Outputs = TArray<FString>{};
    const auto& Name = _Snapshot->_Animations[_AnimationIndex]._Name;
    if (NOT Desktop.SaveFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
        TEXT("Export sprite sheet as a new PNG and JSON pair (temporary palette overrides excluded)"),
        FPaths::ProjectSavedDir(), Name + TEXT("-sheet.png"),
        TEXT("PNG image|*.png"), EFileDialogFlags::None, Outputs) || Outputs.IsEmpty())
    {
        return;
    }
    const auto Json = FPaths::ChangeExtension(Outputs[0], TEXT("json"));
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT ck::pixel_sprite_editor::TryExport_Sheet(*_Asset, Name, _State->_RenderSettings,
        Outputs[0], Json, Diagnostic))
    {
        _Status = Diagnostic.Get_Text();
        return;
    }
    _Status = FString::Printf(TEXT("Exported new sheet pair: %s and %s"),
        *Outputs[0], *Json);
}

auto
    SCk_PixelArtViewer_Window::
    Tick(
        const FGeometry& InGeometry,
        double InCurrentTime,
        float InDeltaTime
    )
    -> void
{
    SCompoundWidget::Tick(InGeometry, InCurrentTime, InDeltaTime);
    if (_Timeline.IsValid() && _TimelineScroll.IsValid())
    {
        const auto ViewWidth = _TimelineScroll->GetCachedGeometry().GetLocalSize().X;
        if (ViewWidth > 0.0)
        {
            _Timeline->Set_TrackWidth(static_cast<float>(ViewWidth * _TimelineZoom));
            if (_TimelineZoomAnchor.IsSet())
            {
                _TimelineScroll->SetScrollOffset(_TimelineZoom == 1.0 ? 0.0f
                    : FMath::Max(0.0f, static_cast<float>(_TimelineZoomAnchor.GetValue()
                        * ViewWidth * _TimelineZoom - ViewWidth * 0.5)));
                _TimelineZoomAnchor.Reset();
            }
        }
    }
    Refresh_Asset();
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        return;
    }
    if (NOT Ensure_Prepared())
    {
        return;
    }
    if (_State->Advance(InDeltaTime, _PreparedCycleMilliseconds))
    {
        Refresh_Frame();
    }
}

auto
    SCk_PixelArtViewer_Window::
    OnKeyDown(
        const FGeometry&,
        const FKeyEvent& InEvent
    )
    -> FReply
{
    if (InEvent.GetKey() == EKeys::SpaceBar)
    {
        if (NOT _State->_Playing && _State->_Playback == ECk_PixelSprite_Playback::Once &&
            _Snapshot.IsValid() && _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
        {
            const auto End = _PreparedCycleMilliseconds;
            if (_State->_Reverse && _State->_TimeMilliseconds <= 0.0)
            {
                _State->Seek(End);
            }
            else if (NOT _State->_Reverse && _State->_TimeMilliseconds >= End)
            {
                _State->Seek(0.0);
            }
        }
        _State->_Playing = NOT _State->_Playing;
        return FReply::Handled();
    }
    if (InEvent.GetKey() == EKeys::Left)
    {
        Step_Frame(-1);
        return FReply::Handled();
    }
    if (InEvent.GetKey() == EKeys::Right)
    {
        Step_Frame(1);
        return FReply::Handled();
    }
    if (InEvent.GetKey() == EKeys::Home)
    {
        Jump_Edge(false);
        return FReply::Handled();
    }
    if (InEvent.GetKey() == EKeys::End)
    {
        Jump_Edge(true);
        return FReply::Handled();
    }
    if (InEvent.GetKey() == EKeys::F)
    {
        _Canvas->Fit_View();
        return FReply::Handled();
    }
    if (InEvent.GetKey() == EKeys::G)
    {
        _State->_ShowGrid = NOT _State->_ShowGrid;
        Save_Preferences();
        _Canvas->Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }
    if (InEvent.GetKey() == EKeys::O)
    {
        _State->_ShowOnion = NOT _State->_ShowOnion;
        Save_Preferences();
        Refresh_Decorations();
        return FReply::Handled();
    }
    if (InEvent.GetKey() == EKeys::R)
    {
        _State->Seek(0.0);
        _State->_Playing = true;
        Refresh_Frame();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

auto
    SCk_PixelArtViewer_Window::
    Get_StatusText() const
    -> FText
{
    return FText::FromString(_Status);
}

auto
    SCk_PixelArtViewer_Window::
    Get_AssetText() const
    -> FText
{
    if (NOT _Asset.IsValid() || NOT _Snapshot.IsValid())
    {
        return FText::FromString(TEXT("Select an imported pixel sprite asset."));
    }
    return FText::FromString(FString::Printf(TEXT("%s\n%d × %d • %d sources • %d uploads"),
        *_Asset->GetName(), _Snapshot->_Width, _Snapshot->_Height,
        _Snapshot->_Sources.Num(), _Resources.IsValid() ? _Resources->Get_UploadCount() : 0));
}

auto
    SCk_PixelArtViewer_Window::
    Get_DetailText() const
    -> FText
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        return FText::FromString(TEXT("No animation selected."));
    }
    const auto& Animation = _Snapshot->_Animations[_AnimationIndex];
    if (NOT Animation._Frames.IsValidIndex(_SelectedFrame))
    {
        return FText::FromString(Animation._Name);
    }
    const auto& Frame = Animation._Frames[_SelectedFrame];
    const auto PaletteCount = _Snapshot->_Sources.IsValidIndex(Frame._SourceIndex)
        ? _Snapshot->_Sources[Frame._SourceIndex]._Grid._Palette.Num() : 0;
    auto Anchors = FString{};
    for (const auto& Anchor : Frame._Anchors)
    {
        Anchors += FString::Printf(TEXT("\n  %s (%d, %d)"),
            *Anchor.Key, Anchor.Value.X, Anchor.Value.Y);
    }
    const auto SourceBytes = _Snapshot->_Sources.IsValidIndex(Frame._SourceIndex)
        ? _Snapshot->_Sources[Frame._SourceIndex]._Bytes : 0;
    auto Comparison = _State->_Comparison == ECk_PixelArtViewer_Comparison::None
        ? FString{TEXT("Off")} : FString::Printf(TEXT("%d changed pixels"), _ChangedPixels);
    if (_State->_Comparison != ECk_PixelArtViewer_Comparison::None &&
        _DifferenceBounds.IsSet())
    {
        const auto& Bounds = _DifferenceBounds.GetValue();
        Comparison += FString::Printf(TEXT("; bounds [%d,%d)-[%d,%d)"),
            Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y);
    }
    return FText::FromString(FString::Printf(
        TEXT("Animation: %s\nFrame: %d / %d\nDuration: %.3f ms\nSource: %s (%lld bytes)\nPalette entries: %d\nAnchors: %d%s\nComparison: %s\nEvents: %s\nMetadata: %s\nDiagnostic: %s"),
        *Animation._Name, _SelectedFrame + 1, Animation._Frames.Num(),
        Frame._DurationMilliseconds, *Frame._Source, SourceBytes, PaletteCount,
        Frame._Anchors.Num(), *Anchors, *Comparison, *Frame._EventsJson,
        *Frame._MetadataJson, *_Status));
}

auto
    SCk_PixelArtViewer_Window::
    Get_TimeText() const
    -> FText
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->_Animations.IsValidIndex(_AnimationIndex))
    {
        return FText::FromString(TEXT("Time: —"));
    }
    const auto& Animation = _Snapshot->_Animations[_AnimationIndex];
    auto Duration = 0.0;
    for (const auto& Frame : Animation._Frames)
    {
        Duration += Frame._DurationMilliseconds;
    }
    auto Position = 0.0;
    auto Returning = false;
    _State->TryGet_TimelineMilliseconds(Animation, Position);
    _State->TryGet_TimelineReturning(Animation, Returning);
    return FText::FromString(FString::Printf(TEXT("%.3f / %.3f s  •  %d / %d  •  %s"),
        Position / 1000.0, Duration / 1000.0, _SelectedFrame + 1, Animation._Frames.Num(),
        Returning ? TEXT("Returning") : TEXT("Forward")));
}

auto
    SCk_PixelArtViewer_Window::
    Get_PlaybackText() const
    -> FText
{
    const FString Label = _State->_Playback == ECk_PixelSprite_Playback::Loop ? TEXT("Loop")
        : _State->_Playback == ECk_PixelSprite_Playback::Once ? TEXT("Once") : TEXT("Ping-pong");
    return FText::FromString(Label);
}

auto
    SCk_PixelArtViewer_Window::
    Get_SpeedText() const
    -> FText
{
    return FText::FromString(FString::Printf(TEXT("%.2fx"), _State->_Speed));
}

auto
    SCk_PixelArtViewer_Window::
    Get_SamplingText() const
    -> FText
{
    return FText::FromString(_State->_SamplingFps == 0 ? TEXT("Sample: off")
        : FString::Printf(TEXT("Sample: %d fps"), _State->_SamplingFps));
}

auto
    SCk_PixelArtViewer_Window::
    Get_DecimationText() const
    -> FText
{
    return FText::FromString(FString::Printf(TEXT("Every %d frame"), _State->_Decimation));
}

auto
    SCk_PixelArtViewer_Window::
    Get_ComparisonText() const
    -> FText
{
    const auto Label = _State->_Comparison == ECk_PixelArtViewer_Comparison::None ? TEXT("Compare off")
        : _State->_Comparison == ECk_PixelArtViewer_Comparison::SideBySide ? TEXT("Side by side")
        : _State->_Comparison == ECk_PixelArtViewer_Comparison::Overlay ? TEXT("Overlay")
        : TEXT("Difference");
    return FText::FromString(Label);
}

auto
    SCk_PixelArtViewer_Window::
    Get_Timeline() const
    -> TSharedPtr<SCk_PixelArtViewer_Timeline>
{
    return _Timeline;
}

auto
    SCk_PixelArtViewer_Window::
    Get_TimelineScroll() const
    -> TSharedPtr<SScrollBox>
{
    return _TimelineScroll;
}

auto
    SCk_PixelArtViewer_Window::
    Get_TimelineZoom() const
    -> double
{
    return _TimelineZoom;
}

auto
    SCk_PixelArtViewer_Window::
    Set_TimelineZoom(
        double InZoom
    )
    -> bool
{
    if (NOT std::isfinite(InZoom) ||
        (InZoom != 1.0 && InZoom != 2.0 && InZoom != 4.0 && InZoom != 8.0))
    {
        return false;
    }
    if (_TimelineScroll.IsValid() && _Timeline.IsValid() && _Timeline->Get_TrackWidth() > 0.0f)
    {
        const auto ViewWidth = _TimelineScroll->GetCachedGeometry().GetLocalSize().X;
        _TimelineZoomAnchor = (_TimelineScroll->GetScrollOffset() + ViewWidth * 0.5)
            / _Timeline->Get_TrackWidth();
    }
    _TimelineZoom = InZoom;
    return true;
}
