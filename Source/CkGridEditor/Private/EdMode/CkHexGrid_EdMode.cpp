#include "CkGridEditor/EdMode/CkHexGrid_EdMode.h"

#include "CkGridEditor/Draw/CkHexGrid_AuthoredOverlay.h"
#include "CkGridEditor/EdMode/CkHexGrid_EdModeToolkit.h"
#include "CkEntitySpawner/CkEntitySpawner_Actor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/Selection.h"
#include "InputCoreTypes.h"
#include "SceneView.h"
#include "Textures/SlateIcon.h"
#include "UnrealClient.h"

const FEditorModeID UCk_HexGrid_EdMode::EM_CkHexGridPaintModeId = TEXT("Ck.HexGrid.PaintMode");

UCk_HexGrid_EdMode::UCk_HexGrid_EdMode()
{
    Info = FEditorModeInfo(EM_CkHexGridPaintModeId,
        NSLOCTEXT("CkHexGrid", "PaintMode", "Hex Grid Paint"), FSlateIcon(), true);
}

void UCk_HexGrid_EdMode::CreateToolkit()
{
    Toolkit = MakeShared<FCk_HexGrid_EdModeToolkit>();
}

void UCk_HexGrid_EdMode::Exit()
{
    FinishStroke(false);
    Super::Exit();
}

void UCk_HexGrid_EdMode::ActorSelectionChangeNotify()
{
    FinishStroke(false);
    _HoverCells.Reset();
    Super::ActorSelectionChangeNotify();
}

auto UCk_HexGrid_EdMode::Set_Brush(const ck::hex_grid_editor::FBrush& InBrush) -> void
{
    FinishStroke(false);
    _Brush = InBrush;
    _Brush.Radius = FMath::Clamp(_Brush.Radius, 0, 32);
    _HoverCells.Reset();
}

auto UCk_HexGrid_EdMode::ResolveSelection() const -> ck::hex_grid_editor::FSelection
{
    const auto* Manager = GetModeManager();
    const auto* Selected = Manager != nullptr ? Manager->GetSelectedActors() : nullptr;
    if (Selected == nullptr)
    { return {}; }
    for (auto Index = 0; Index < Selected->Num(); ++Index)
    {
        const auto Selection = ck::hex_grid_editor::ResolveSelection(
            Cast<ACk_EntitySpawner_UE>(Selected->GetSelectedObject(Index)));
        if (Selection.IsValid())
        { return Selection; }
    }
    return {};
}

auto UCk_HexGrid_EdMode::Get_SelectedAsset() const -> UCk_HexGrid_AuthoringSpec*
{
    return ResolveSelection().Asset;
}

void UCk_HexGrid_EdMode::Render(const FSceneView* InView, FViewport* InViewport, FPrimitiveDrawInterface* InPDI)
{
    Super::Render(InView, InViewport, InPDI);
    const auto Selection = ResolveSelection();
    if (NOT Selection.IsValid())
    { return; }
    const auto& Spec = _Painting && _Stroke.Valid ? _Stroke.Candidate : Selection.Spec;
    ck::hex_grid_editor::DrawAuthoredOverlay(InPDI, Spec, Selection.PivotWorld);
    const auto* Selected = GetModeManager()->GetSelectedActors();
    for (auto Index = 0; Selected != nullptr && Index < Selected->Num(); ++Index)
    {
        const auto Other = ck::hex_grid_editor::ResolveSelection(
            Cast<ACk_EntitySpawner_UE>(Selected->GetSelectedObject(Index)));
        if (Other.IsValid() && Other.Spawner != Selection.Spawner)
        { ck::hex_grid_editor::DrawAuthoredOverlay(InPDI, Other.Spec, Other.PivotWorld); }
    }
    for (const auto& Cell : _HoverCells)
    {
        ck::hex_grid_editor::DrawCellOutline(InPDI, Spec, Selection.PivotWorld,
            Cell, _Painting ? FLinearColor::Yellow : FLinearColor::White, 3.0f);
    }
}

void UCk_HexGrid_EdMode::DrawHUD(FEditorViewportClient* InClient, FViewport* InViewport,
    const FSceneView* InView, FCanvas* InCanvas)
{
    Super::DrawHUD(InClient, InViewport, InView, InCanvas);
    const auto Selection = ResolveSelection();
    if (Selection.IsValid())
    {
        ck::hex_grid_editor::DrawAuthoredLabels(InCanvas, InView,
            _Painting && _Stroke.Valid ? _Stroke.Candidate : Selection.Spec, Selection.PivotWorld);
        const auto* Selected = GetModeManager()->GetSelectedActors();
        for (auto Index = 0; Selected != nullptr && Index < Selected->Num(); ++Index)
        {
            const auto Other = ck::hex_grid_editor::ResolveSelection(
                Cast<ACk_EntitySpawner_UE>(Selected->GetSelectedObject(Index)));
            if (Other.IsValid() && Other.Spawner != Selection.Spawner)
            { ck::hex_grid_editor::DrawAuthoredLabels(InCanvas, InView, Other.Spec, Other.PivotWorld); }
        }
    }
}

auto UCk_HexGrid_EdMode::UpdateCursor(FEditorViewportClient* InClient, int32 InX, int32 InY) -> bool
{
    _HoverCells.Reset();
    const auto Selection = ResolveSelection();
    if (InClient == nullptr || InClient->Viewport == nullptr || NOT Selection.IsValid())
    { return false; }
    if (_Painting && (_StrokeSpawner.Get() != Selection.Spawner || _Stroke.Asset.Get() != Selection.Asset))
    {
        FinishStroke(false);
        return false;
    }
    auto Family = FSceneViewFamilyContext(FSceneViewFamily::ConstructionValues(
        InClient->Viewport, InClient->GetScene(), InClient->EngineShowFlags).SetRealtimeUpdate(InClient->IsRealtime()));
    auto* View = InClient->CalcSceneView(&Family);
    if (View == nullptr)
    { return false; }
    const auto Ray = FViewportCursorLocation(View, InClient, InX, InY);
    const auto Cell = ck::hex_grid_editor::ResolveCellFromRay(Selection, Ray.GetOrigin(), Ray.GetDirection());
    if (NOT Cell.IsSet())
    { return false; }
    _HoverCells = ck::hex_grid_editor::ComputeDisk(Selection.Spec, Cell.GetValue(), _Brush.Radius);
    if (_Painting)
    { ck::hex_grid_editor::StageStroke(_Stroke, _HoverCells, _Brush); }
    InClient->Invalidate();
    return true;
}

auto UCk_HexGrid_EdMode::FinishStroke(bool InCommit) -> void
{
    if (_Painting && InCommit)
    { ck::hex_grid_editor::CommitStroke(_Stroke); }
    else
    { ck::hex_grid_editor::CancelStroke(_Stroke); }
    _Painting = false;
    _StrokeSpawner.Reset();
}

bool UCk_HexGrid_EdMode::InputKey(FEditorViewportClient* InClient, FViewport* InViewport,
    FKey InKey, EInputEvent InEvent)
{
    if (InKey == EKeys::Escape && InEvent == IE_Pressed && _Painting)
    {
        FinishStroke(false);
        InClient->Invalidate();
        return true;
    }
    if (InKey == EKeys::LeftMouseButton && InEvent == IE_Pressed &&
        InViewport != nullptr && NOT InViewport->KeyState(EKeys::LeftAlt) && NOT InViewport->KeyState(EKeys::RightAlt) &&
        NOT InViewport->KeyState(EKeys::LeftControl) && NOT InViewport->KeyState(EKeys::RightControl) &&
        NOT InViewport->KeyState(EKeys::RightMouseButton))
    {
        const auto Selection = ResolveSelection();
        if (NOT Selection.IsValid() || NOT UpdateCursor(InClient, InViewport->GetMouseX(), InViewport->GetMouseY()))
        { return false; }
        _Stroke = ck::hex_grid_editor::BeginStroke(Selection.Asset);
        _Painting = _Stroke.Valid;
        _StrokeSpawner = Selection.Spawner;
        _Brush.Erase = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);
        if (_Painting)
        { ck::hex_grid_editor::StageStroke(_Stroke, _HoverCells, _Brush); }
        return _Painting;
    }
    if (InKey == EKeys::LeftMouseButton && InEvent == IE_Released && _Painting)
    {
        FinishStroke(true);
        InClient->Invalidate();
        return true;
    }
    return Super::InputKey(InClient, InViewport, InKey, InEvent);
}

bool UCk_HexGrid_EdMode::MouseMove(FEditorViewportClient* InClient, FViewport* InViewport, int32 InX, int32 InY)
{
    return UpdateCursor(InClient, InX, InY);
}

bool UCk_HexGrid_EdMode::CapturedMouseMove(FEditorViewportClient* InClient, FViewport* InViewport, int32 InX, int32 InY)
{
    return UpdateCursor(InClient, InX, InY);
}

bool UCk_HexGrid_EdMode::StartTracking(FEditorViewportClient* InClient, FViewport* InViewport)
{
    return _Painting;
}

bool UCk_HexGrid_EdMode::EndTracking(FEditorViewportClient* InClient, FViewport* InViewport)
{
    const auto WasPainting = _Painting;
    FinishStroke(true);
    return WasPainting;
}

bool UCk_HexGrid_EdMode::DisallowMouseDeltaTracking() const
{
    return _Painting;
}

bool UCk_HexGrid_EdMode::HandleClick(FEditorViewportClient* InClient, HHitProxy* InHit, const FViewportClick& InClick)
{
    return InClick.GetKey() == EKeys::LeftMouseButton && NOT InClick.IsControlDown() &&
        NOT InClick.IsAltDown() && ResolveSelection().IsValid() && _HoverCells.Num() > 0;
}
