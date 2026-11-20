#pragma once

#include "CkGrid/HexGridSystem/Authoring/CkHexGrid_Spec.h"

class ACk_EntitySpawner_UE;
class USceneComponent;

namespace ck::hex_grid_editor
{
    struct CKGRIDEDITOR_API FSelection
    {
        ACk_EntitySpawner_UE* Spawner = nullptr;
        UCk_HexGrid_AuthoringSpec* Asset = nullptr;
        FCk_HexGrid_Spec Spec;
        FTransform PivotWorld = FTransform::Identity;
        auto IsValid() const -> bool { return Spawner != nullptr && Asset != nullptr; }
    };

    struct CKGRIDEDITOR_API FBrush
    {
        int32 Radius = 0;
        bool PaintDisabled = true;
        bool Erase = false;
        FGameplayTag Tag;
    };

    struct CKGRIDEDITOR_API FStroke
    {
        FCk_HexGrid_Spec Original;
        FCk_HexGrid_Spec Candidate;
        TWeakObjectPtr<UCk_HexGrid_AuthoringSpec> Asset;
        int32 OriginalRevision = 0;
        bool Valid = false;
    };

    CKGRIDEDITOR_API auto ResolveSelection(const ACk_EntitySpawner_UE* InSpawner) -> FSelection;
    CKGRIDEDITOR_API auto ResolveDrawableSelection(const USceneComponent* InComponent, bool InHexModeActive) -> FSelection;
    CKGRIDEDITOR_API auto ResolveCellFromRay(const FSelection& InSelection,
        const FVector& InOrigin, const FVector& InDirection) -> TOptional<FCk_HexCoord>;
    CKGRIDEDITOR_API auto ComputeDisk(const FCk_HexGrid_Spec& InSpec,
        const FCk_HexCoord& InCenter, int32 InRadius) -> TArray<FCk_HexCoord>;
    CKGRIDEDITOR_API auto BeginStroke(UCk_HexGrid_AuthoringSpec* InAsset) -> FStroke;
    CKGRIDEDITOR_API auto StageStroke(FStroke& InStroke, const TArray<FCk_HexCoord>& InCells, const FBrush& InBrush) -> bool;
    CKGRIDEDITOR_API auto CommitStroke(FStroke& InStroke) -> bool;
    CKGRIDEDITOR_API auto CancelStroke(FStroke& InStroke) -> void;
    CKGRIDEDITOR_API auto StartupPainting() -> void;
    CKGRIDEDITOR_API auto ShutdownPainting() -> void;
    // Counts rebuild requests issued by this service, not completed deferred constructions.
    CKGRIDEDITOR_API auto GetRebuildCount(const ACk_EntitySpawner_UE* InSpawner) -> int32;
}
