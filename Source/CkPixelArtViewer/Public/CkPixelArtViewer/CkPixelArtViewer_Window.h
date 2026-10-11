#pragma once

#include "CoreMinimal.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkPixelSprite/CkPixelSprite_Resources.h"
#include "CkPixelArtViewer/CkPixelArtViewer_State.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class SCk_PixelArtViewer_Canvas;
struct FSlateBrush;
class SVerticalBox;
class SHorizontalBox;
class SSplitter;

class CKPIXELARTVIEWER_API SCk_PixelArtViewer_Window : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCk_PixelArtViewer_Window) {}
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs
        )
        -> void;
    auto
        Set_Asset(
            UCk_PixelSpriteAsset_UE& InAsset
        )
        -> bool;
    auto
        Get_Asset() const
        -> TObjectPtr<UCk_PixelSpriteAsset_UE>;
    auto
        Get_Resources() const
        -> TObjectPtr<UCk_PixelSpriteResources_UE>;
    auto
        Get_State() const
        -> TSharedPtr<FCk_PixelArtViewer_State>;
    auto
        Get_Canvas() const
        -> TSharedPtr<SCk_PixelArtViewer_Canvas>;
    auto
        Get_SelectedFrame() const
        -> int32;
    auto
        Get_SelectedAnimationName() const
        -> FString;
    auto
        Get_Prepared() const
        -> TSharedPtr<const FCk_PixelSprite_PreparedAnimation>;
    auto
        Get_MainSplitter() const
        -> TSharedPtr<SSplitter>;
    auto
        Get_ChangedPixels() const
        -> int32;
    auto
        Seek_Milliseconds(
            double InMilliseconds
        )
        -> bool;

    virtual auto
        Tick(
            const FGeometry& InGeometry,
            double InCurrentTime,
            float InDeltaTime
        )
        -> void override;
    virtual auto
        OnKeyDown(
            const FGeometry& InGeometry,
            const FKeyEvent& InEvent
        )
        -> FReply override;
    virtual auto
        SupportsKeyboardFocus() const
        -> bool override
    {
        return true;
    }

private:
    struct FOverrideGeneration
    {
        TStrongObjectPtr<UCk_PixelSpriteAsset_UE> _Asset;
        TStrongObjectPtr<UCk_PixelSpriteResources_UE> _Resources;
    };

    struct FSourceNode
    {
        FString _Name;
        FString _Path;
        int32 _SourceIndex = INDEX_NONE;
        TArray<TSharedPtr<FSourceNode>> _Children;
    };

    auto
        Refresh_Asset()
        -> bool;
    auto
        Ensure_Prepared()
        -> bool;
    auto
        Refresh_Frame()
        -> void;
    auto
        Refresh_Decorations()
        -> void;
    auto
        Rebuild_BaseResources()
        -> bool;
    auto
        Rebuild_Timeline()
        -> void;
    auto
        Rebuild_Palette()
        -> void;
    auto
        Rebuild_Sources()
        -> void;
    auto
        Save_Preferences()
        -> void;
    auto
        Save_LayoutPreferences()
        -> void;
    auto
        Select_Source(
            int32 InSourceIndex
        )
        -> void;
    auto
        Generate_SourceRow(
            TSharedPtr<FSourceNode> InNode,
            const TSharedRef<STableViewBase>& InOwnerTable
        )
        -> TSharedRef<ITableRow>;
    auto
        Get_SourceChildren(
            TSharedPtr<FSourceNode> InNode,
            TArray<TSharedPtr<FSourceNode>>& OutChildren
        ) const
        -> void;
    auto
        Export_Frame()
        -> void;
    auto
        Export_Sheet()
        -> void;
    auto
        Set_RenderMode(
            ECk_PixelSprite_RenderMode InMode
        )
        -> void;
    auto
        Set_RenderScale(
            int32 InScale
        )
        -> void;
    auto
        Get_SelectedPaletteColor() const
        -> FColor;
    auto
        Set_SelectedPaletteChannel(
            int32 InChannel,
            int32 InValue
        )
        -> void;
    auto
        Build_DerivedResources(
            const FCk_PixelSprite_Grid& InGrid,
            const FCk_PixelSprite_RenderSettings& InSettings,
            TStrongObjectPtr<UCk_PixelSpriteAsset_UE>& OutAsset,
            TStrongObjectPtr<UCk_PixelSpriteResources_UE>& OutResources
        )
        -> bool;
    auto
        Rebuild_AnimationList()
        -> void;
    auto
        Select_Animation(
            int32 InIndex
        )
        -> void;
    auto
        Step_Frame(
            int32 InDirection
        )
        -> void;
    auto
        Jump_Edge(
            bool InLast
        )
        -> void;
    auto
        Get_StatusText() const
        -> FText;
    auto
        Get_AssetText() const
        -> FText;
    auto
        Get_DetailText() const
        -> FText;
    auto
        Get_TimeText() const
        -> FText;
    auto
        Get_PlaybackText() const
        -> FText;
    auto
        Get_SpeedText() const
        -> FText;
    auto
        Get_SamplingText() const
        -> FText;
    auto
        Get_DecimationText() const
        -> FText;
    auto
        Get_ComparisonText() const
        -> FText;

    TStrongObjectPtr<UCk_PixelSpriteAsset_UE> _Asset;
    TStrongObjectPtr<UCk_PixelSpriteResources_UE> _Resources;
    TMap<int32, TMap<int32, FColor>> _SourceOverrides;
    TMap<int32, TSharedPtr<FOverrideGeneration>> _OverrideGenerations;
    TStrongObjectPtr<UCk_PixelSpriteAsset_UE> _DifferenceAsset;
    TStrongObjectPtr<UCk_PixelSpriteResources_UE> _DifferenceResources;
    TSharedPtr<const FCk_PixelSprite_Manifest> _Snapshot;
    TSharedPtr<const FCk_PixelSprite_PreparedAnimation> _Prepared;
    TSharedPtr<FCk_PixelArtViewer_State> _State;
    TSharedPtr<SCk_PixelArtViewer_Canvas> _Canvas;
    TSharedPtr<SVerticalBox> _AnimationList;
    TSharedPtr<SVerticalBox> _PaletteList;
    TSharedPtr<SSplitter> _MainSplitter;
    TSharedPtr<STreeView<TSharedPtr<FSourceNode>>> _SourceTree;
    TArray<TSharedPtr<FSourceNode>> _SourceRoots;
    TSharedPtr<SHorizontalBox> _TimelineList;
    TArray<TSharedPtr<FSlateBrush>> _ThumbnailBrushes;
    FString _Status;
    uint64 _RevisionSeen = MAX_uint64;
    uint64 _PreparedRevision = MAX_uint64;
    double _PreparedCycleMilliseconds = 0.0;
    ECk_PixelSprite_Playback _PreparedPlayback = ECk_PixelSprite_Playback::Loop;
    int32 _PreparedAnimationIndex = INDEX_NONE;
    int32 _AnimationIndex = INDEX_NONE;
    int32 _SelectedFrame = INDEX_NONE;
    int32 _CurrentSourceIndex = INDEX_NONE;
    int32 _OverrideSourceIndex = INDEX_NONE;
    int32 _DifferenceSourceA = INDEX_NONE;
    int32 _DifferenceSourceB = INDEX_NONE;
    int32 _ChangedPixels = 0;
    TOptional<FIntRect> _DifferenceBounds;
    int32 _CachedChangedPixels = 0;
    TOptional<FIntRect> _CachedDifferenceBounds;
    FCk_PixelSprite_RenderSettings _AppliedRenderSettings;
};
