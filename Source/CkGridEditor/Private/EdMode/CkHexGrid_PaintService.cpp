#include "CkGridEditor/EdMode/CkHexGrid_PaintService.h"

#include "CkGrid/HexGridSystem/Authoring/CkHexGrid_EntityScript.h"
#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Utils.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkEntitySpawner/CkEntitySpawner_Actor.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include <Components/SceneComponent.h>
#include <Editor.h>
#include <EditorUndoClient.h>
#include <Engine/World.h>
#include <Misc/PackageName.h>
#include <ScopedTransaction.h>
#include <UObject/UObjectIterator.h>
#include <UObject/UnrealType.h>

namespace ck::hex_grid_editor
{
    namespace paint_service_detail
    {
        auto Less(const FCk_HexCoord& A, const FCk_HexCoord& B) -> bool
        { return A.Get_R() == B.Get_R() ? A.Get_Q() < B.Get_Q() : A.Get_R() < B.Get_R(); }

        auto Encode(const FCk_HexGrid_Spec& InSpec) -> TArray<uint8>
        {
            TArray<uint8> Bytes;
            auto Failure = ECk_HexBoardFailure::None;
            ck::hex_grid::TryEncodeRecipe(InSpec, Bytes, Failure);
            return Bytes;
        }

        TMap<TWeakObjectPtr<UCk_HexGrid_AuthoringSpec>, TArray<uint8>> Observed;
        TMap<TWeakObjectPtr<ACk_EntitySpawner_UE>, int32> RebuildCounts;
        FDelegateHandle PropertyChangedHandle;

        auto IsPaintable(const UCk_HexGrid_AuthoringSpec* InAsset) -> bool
        {
            return IsValid(InAsset) && InAsset->IsAsset() && !InAsset->IsTemplate() &&
                !InAsset->HasAnyFlags(RF_Transient) && InAsset->GetPackage() != GetTransientPackage() &&
                !InAsset->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn | PKG_PlayInEditor) &&
                FPackageName::IsValidLongPackageName(InAsset->GetPackage()->GetName(), false);
        }

        auto ObserveBytes(const UCk_HexGrid_AuthoringSpec* InAsset) -> TArray<uint8>
        {
            auto Bytes = Encode(InAsset->Get_Spec());
            const auto Revision = static_cast<uint32>(InAsset->Get_RecipeRevision());
            for (int32 Shift = 0; Shift < 32; Shift += 8) { Bytes.Add(static_cast<uint8>(Revision >> Shift)); }
            return Bytes;
        }

        auto Refresh(UCk_HexGrid_AuthoringSpec* InAsset) -> void
        {
            if (!IsValid(InAsset)) { return; }
            const auto Bytes = ObserveBytes(InAsset);
            auto* Previous = Observed.Find(InAsset);
            if (Previous != nullptr && *Previous == Bytes) { return; }
            Observed.Add(InAsset, Bytes);
            for (TObjectIterator<ACk_EntitySpawner_UE> It; It; ++It)
            {
                auto* Spawner = *It;
                if (Spawner->IsTemplate() || !IsValid(Spawner->GetWorld())) { continue; }
                if (Spawner->GetWorld()->WorldType != EWorldType::Editor) { continue; }
                const auto* Script = Cast<UCk_HexGrid_EntityScript>(Spawner->Get_EntityScript());
                if (Script == nullptr || Script->Get_ResolvedAuthoringSpec() != InAsset) { continue; }
                Spawner->EditorOnly_RebuildEntity();
                ++RebuildCounts.FindOrAdd(Spawner);
            }
        }

        class FUndoObserver final : public FEditorUndoClient
        {
        public:
            auto PostUndo(bool InSuccess) -> void override
            {
                if (!InSuccess) { return; }
                for (auto It = Observed.CreateIterator(); It; ++It)
                {
                    if (!It.Key().IsValid()) { It.RemoveCurrent(); }
                }
                for (auto It = RebuildCounts.CreateIterator(); It; ++It)
                {
                    if (!It.Key().IsValid()) { It.RemoveCurrent(); }
                }
                TArray<TWeakObjectPtr<UCk_HexGrid_AuthoringSpec>> Assets;
                Observed.GetKeys(Assets);
                for (const auto& Asset : Assets) { Refresh(Asset.Get()); }
            }
            auto PostRedo(bool InSuccess) -> void override { PostUndo(InSuccess); }
        } UndoObserver;
    }

    using namespace paint_service_detail;

    auto StartupPainting() -> void
    {
        if (PropertyChangedHandle.IsValid()) { return; }
        if (GEditor != nullptr) { GEditor->RegisterForUndo(&UndoObserver); }
        PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda(
            [](UObject* InObject, FPropertyChangedEvent&) { Refresh(Cast<UCk_HexGrid_AuthoringSpec>(InObject)); });
    }

    auto ShutdownPainting() -> void
    {
        FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
        PropertyChangedHandle.Reset();
        if (GEditor != nullptr) { GEditor->UnregisterForUndo(&UndoObserver); }
        Observed.Reset();
        RebuildCounts.Reset();
    }

    auto GetRebuildCount(const ACk_EntitySpawner_UE* InSpawner) -> int32
    { return RebuildCounts.FindRef(const_cast<ACk_EntitySpawner_UE*>(InSpawner)); }

    auto ResolveSelection(const ACk_EntitySpawner_UE* InSpawner) -> FSelection
    {
        FSelection Result;
        if (!IsValid(InSpawner) || InSpawner->IsTemplate()) { return Result; }
        const auto* Script = Cast<UCk_HexGrid_EntityScript>(InSpawner->Get_EntityScript());
        if (Script == nullptr) { return Result; }
        auto* Asset = Script->Get_ResolvedAuthoringSpec();
        auto Failure = ECk_HexBoardFailure::None;
        if (!IsValid(Asset) || Asset->Get_RecipeRevision() < 1 ||
            !UCk_Utils_HexGrid_UE::TryValidateSpec(Asset->Get_Spec(), Failure)) { return {}; }
        if (!Observed.Contains(Asset))
        {
            Observed.Add(Asset, ObserveBytes(Asset));
        }
        Result.Spec = Asset->Get_Spec();

        // Authoring construction adopts an existing entity transform, otherwise the spawner root frame.
        auto World = InSpawner->GetActorTransform();
        const auto Transform = UCk_Utils_Transform_UE::Cast(InSpawner->Get_EditorEntityHandle());
        if (ck::IsValid(Transform)) { World = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Transform); }
        auto SpatialFailure = ECk_HexGeometryFailure::None;
        Result.PivotWorld = Result.Spec.Get_Pivot() * World;
        if (!UCk_Utils_HexGeometry_UE::TryValidateWorldLayout(Result.Spec.Get_Layout(), World, SpatialFailure) ||
            !UCk_Utils_HexGeometry_UE::TryValidateWorldLayout(Result.Spec.Get_Layout(), Result.PivotWorld, SpatialFailure))
        { return {}; }
        Result.Spawner = const_cast<ACk_EntitySpawner_UE*>(InSpawner);
        Result.Asset = Asset;
        return Result;
    }

    auto ResolveDrawableSelection(const USceneComponent* InComponent, bool InHexModeActive) -> FSelection
    {
        if (!IsValid(InComponent) || InHexModeActive) { return {}; }
        const auto* Spawner = Cast<ACk_EntitySpawner_UE>(InComponent->GetOwner());
        if (Spawner == nullptr || Spawner->GetRootComponent() != InComponent) { return {}; }
        return ResolveSelection(Spawner);
    }

    auto ResolveCellFromRay(const FSelection& InSelection, const FVector& InOrigin,
        const FVector& InDirection) -> TOptional<FCk_HexCoord>
    {
        if (!InSelection.IsValid()) { return {}; }
        FCk_HexCoord Coordinate;
        FVector Hit;
        auto Failure = ECk_HexGeometryFailure::None;
        if (!UCk_Utils_HexGeometry_UE::TryGet_CoordFromWorldRay(InOrigin, InDirection,
            InSelection.PivotWorld, InSelection.Spec.Get_Layout(), Coordinate, Hit, Failure)) { return {}; }
        TArray<FCk_HexCoord> Domain;
        if (!UCk_Utils_HexGeometry_UE::TryResolve_Domain(InSelection.Spec.Get_Domain(), Domain, Failure) ||
            !Domain.Contains(Coordinate)) { return {}; }
        return Coordinate;
    }

    auto ComputeDisk(const FCk_HexGrid_Spec& InSpec, const FCk_HexCoord& InCenter, int32 InRadius) -> TArray<FCk_HexCoord>
    {
        TArray<FCk_HexCoord> Result;
        auto Failure = ECk_HexGeometryFailure::None;
        if (InRadius < 0 || !UCk_Utils_HexGeometry_UE::TryResolve_Domain(InSpec.Get_Domain(), Result, Failure)) { return {}; }
        Result.RemoveAll([&](const FCk_HexCoord& Cell)
        {
            int64 Distance = 0;
            return !UCk_Utils_HexGeometry_UE::TryGet_Distance(InCenter, Cell, Distance, Failure) || Distance > InRadius;
        });
        Result.Sort(Less);
        return Result;
    }

    auto BeginStroke(UCk_HexGrid_AuthoringSpec* InAsset) -> FStroke
    {
        FStroke Result;
        auto Failure = ECk_HexBoardFailure::None;
        if (!IsPaintable(InAsset) || !InAsset->TryResolve(Result.Original, Failure)) { return Result; }
        Result.Candidate = Result.Original;
        Result.Asset = InAsset;
        Result.Valid = true;
        Result.OriginalRevision = InAsset->Get_RecipeRevision();
        Observed.FindOrAdd(InAsset, ObserveBytes(InAsset));
        return Result;
    }

    auto StageStroke(FStroke& InStroke, const TArray<FCk_HexCoord>& InCells, const FBrush& InBrush) -> bool
    {
        if (!InStroke.Valid || !InStroke.Asset.IsValid()) { return false; }
        auto Candidate = InStroke.Candidate;
        auto Disabled = Candidate.Get_InitialDisabledCells();
        auto Overrides = Candidate.Get_PerCellTags();
        TArray<FCk_HexCoord> Domain;
        auto Spatial = ECk_HexGeometryFailure::None;
        if (!UCk_Utils_HexGeometry_UE::TryResolve_Domain(Candidate.Get_Domain(), Domain, Spatial) ||
            (!InBrush.PaintDisabled && !InBrush.Tag.IsValid()))
        { InStroke.Valid = false; return false; }
        TSet<FCk_HexCoord> Visited;
        for (const auto& Cell : InCells)
        {
            if (!Domain.Contains(Cell)) { InStroke.Valid = false; return false; }
            if (Visited.Contains(Cell)) { continue; }
            Visited.Add(Cell);
            if (InBrush.PaintDisabled)
            {
                if (InBrush.Erase) { Disabled.Remove(Cell); }
                else { Disabled.AddUnique(Cell); }
                continue;
            }
            // Defaults are additive runtime tags. Per-cell erasure cannot subtract them.
            if (!InBrush.Erase && Candidate.Get_DefaultCellTags().HasTagExact(InBrush.Tag)) { continue; }
            const auto Index = Overrides.IndexOfByPredicate([&](const auto& Entry) { return Entry.Get_Coordinate() == Cell; });
            auto Tags = Index == INDEX_NONE ? FGameplayTagContainer{} : Overrides[Index].Get_Tags();
            if (InBrush.Erase) { Tags.RemoveTag(InBrush.Tag); }
            else { Tags.AddTag(InBrush.Tag); }
            if (Index != INDEX_NONE) { Overrides.RemoveAt(Index); }
            if (!Tags.IsEmpty()) { Overrides.Emplace(Cell, Tags); }
        }
        Disabled.Sort(Less);
        Overrides.Sort([](const auto& A, const auto& B) { return Less(A.Get_Coordinate(), B.Get_Coordinate()); });
        Candidate.Set_InitialDisabledCells(Disabled).Set_PerCellTags(Overrides);
        auto Failure = ECk_HexBoardFailure::None;
        if (!UCk_Utils_HexGrid_UE::TryValidateSpec(Candidate, Failure)) { InStroke.Valid = false; return false; }
        InStroke.Candidate = MoveTemp(Candidate);
        return true;
    }

    auto CommitStroke(FStroke& InStroke) -> bool
    {
        auto* Asset = InStroke.Asset.Get();
        if (!InStroke.Valid || !IsPaintable(Asset)) { CancelStroke(InStroke); return false; }
        auto Failure = ECk_HexBoardFailure::None;
        if (!UCk_Utils_HexGrid_UE::TryValidateSpec(InStroke.Candidate, Failure))
        { CancelStroke(InStroke); return false; }
        const auto Original = Encode(InStroke.Original);
        const auto Candidate = Encode(InStroke.Candidate);
        if (InStroke.OriginalRevision != Asset->Get_RecipeRevision() ||
            !FCk_HexGrid_Spec::StaticStruct()->CompareScriptStruct(&InStroke.Original, &Asset->Get_Spec(), 0) ||
            Original == Candidate)
        { CancelStroke(InStroke); return false; }
        StartupPainting();
        FScopedTransaction Transaction(NSLOCTEXT("CkHexGrid", "PaintStroke", "Paint Hex Grid"));
        Asset->SetFlags(RF_Transactional);
        Asset->Modify();
        if (!Asset->EditorOnly_TryReplaceSpec(InStroke.Candidate, Failure))
        { Transaction.Cancel(); CancelStroke(InStroke); return false; }
        FPropertyChangedEvent Changed(FindFProperty<FProperty>(UCk_HexGrid_AuthoringSpec::StaticClass(), TEXT("_Spec")), EPropertyChangeType::ValueSet);
        Asset->PostEditChangeProperty(Changed);
        Asset->MarkPackageDirty();
        Refresh(Asset);
        CancelStroke(InStroke);
        return true;
    }

    auto CancelStroke(FStroke& InStroke) -> void { InStroke = {}; }
}
