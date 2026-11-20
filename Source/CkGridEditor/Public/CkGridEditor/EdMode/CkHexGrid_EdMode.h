#pragma once

#include "CoreMinimal.h"
#include "CkGridEditor/EdMode/CkHexGrid_PaintService.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "CkHexGrid_EdMode.generated.h"

UCLASS()
class CKGRIDEDITOR_API UCk_HexGrid_EdMode : public UBaseLegacyWidgetEdMode
{
    GENERATED_BODY()

public:
    static const FEditorModeID EM_CkHexGridPaintModeId;
    UCk_HexGrid_EdMode();

    virtual void CreateToolkit() override;
    virtual void Exit() override;
    virtual void ActorSelectionChangeNotify() override;
    virtual void Render(const FSceneView* InView, FViewport* InViewport, FPrimitiveDrawInterface* InPDI) override;
    virtual void DrawHUD(FEditorViewportClient* InClient, FViewport* InViewport, const FSceneView* InView, FCanvas* InCanvas) override;
    virtual bool InputKey(FEditorViewportClient* InClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;
    virtual bool MouseMove(FEditorViewportClient* InClient, FViewport* InViewport, int32 InX, int32 InY) override;
    virtual bool CapturedMouseMove(FEditorViewportClient* InClient, FViewport* InViewport, int32 InX, int32 InY) override;
    virtual bool StartTracking(FEditorViewportClient* InClient, FViewport* InViewport) override;
    virtual bool EndTracking(FEditorViewportClient* InClient, FViewport* InViewport) override;
    virtual bool DisallowMouseDeltaTracking() const override;
    virtual bool HandleClick(FEditorViewportClient* InClient, HHitProxy* InHit, const FViewportClick& InClick) override;

    auto Get_Brush() const -> const ck::hex_grid_editor::FBrush& { return _Brush; }
    auto Set_Brush(const ck::hex_grid_editor::FBrush& InBrush) -> void;
    auto Get_SelectedAsset() const -> UCk_HexGrid_AuthoringSpec*;

private:
    auto ResolveSelection() const -> ck::hex_grid_editor::FSelection;
    auto UpdateCursor(FEditorViewportClient* InClient, int32 InX, int32 InY) -> bool;
    auto FinishStroke(bool InCommit) -> void;

    ck::hex_grid_editor::FBrush _Brush;
    ck::hex_grid_editor::FStroke _Stroke;
    TWeakObjectPtr<ACk_EntitySpawner_UE> _StrokeSpawner;
    TArray<FCk_HexCoord> _HoverCells;
    bool _Painting = false;
};
