#include "CkGridEditor/Draw/CkHexGrid_AuthoredOverlay.h"

#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Utils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Misc/Crc.h"
#include "SceneManagement.h"
#include "SceneView.h"

namespace ck_hex_grid_authored_overlay
{
    TAutoConsoleVariable<bool> ShowLabels(TEXT("ck.HexGrid.PreviewShowTags"), false,
        TEXT("Draw authored hex coordinates and effective tags in the editor."), ECVF_Default);

    auto CellTags(const FCk_HexGrid_Spec& InSpec, const FCk_HexCoord& InCell) -> FGameplayTagContainer
    {
        auto Tags = InSpec.Get_DefaultCellTags();
        for (const auto& Override : InSpec.Get_PerCellTags())
        {
            if (Override.Get_Coordinate() == InCell)
            { Tags.AppendTags(Override.Get_Tags()); }
        }
        return Tags;
    }

    auto CellColor(const FCk_HexGrid_Spec& InSpec, const FCk_HexCoord& InCell) -> FLinearColor
    {
        if (InSpec.Get_InitialDisabledCells().Contains(InCell))
        { return FLinearColor(0.8f, 0.1f, 0.1f); }
        for (const auto& Blocker : InSpec.Get_Blockers())
        {
            if (Blocker.Get_InitiallyActive() && Blocker.Get_Cells().Contains(InCell))
            { return FLinearColor(1.0f, 0.45f, 0.1f); }
        }
        const auto Tags = CellTags(InSpec, InCell);
        if (Tags.IsEmpty())
        { return FLinearColor(0.15f, 0.8f, 0.5f); }
        return FLinearColor::MakeFromHSV8(static_cast<uint8>(FCrc::StrCrc32(*Tags.First().ToString()) % 256), 200, 255);
    }
}

auto ck::hex_grid_editor::DrawCellOutline(FPrimitiveDrawInterface* InPDI,
    const FCk_HexGrid_Spec& InSpec, const FTransform& InPivotWorld,
    const FCk_HexCoord& InCell, const FLinearColor& InColor, float InThickness) -> void
{
    if (InPDI == nullptr)
    { return; }
    auto Failure = ECk_HexGeometryFailure::None;
    auto Corners = TArray<FVector2D>{};
    if (NOT UCk_Utils_HexGeometry_UE::TryValidateWorldLayout(InSpec.Get_Layout(), InPivotWorld, Failure) ||
        NOT UCk_Utils_HexGeometry_UE::TryGet_LocalCorners(InCell, InSpec.Get_Layout(), Corners, Failure))
    { return; }
    for (auto Index = 0; Index < Corners.Num(); ++Index)
    {
        const auto A = Corners[Index];
        const auto B = Corners[(Index + 1) % Corners.Num()];
        InPDI->DrawLine(InPivotWorld.TransformPosition(FVector(A.X, A.Y, 0.0)),
            InPivotWorld.TransformPosition(FVector(B.X, B.Y, 0.0)), InColor, SDPG_Foreground, InThickness);
    }
}

auto ck::hex_grid_editor::DrawAuthoredOverlay(FPrimitiveDrawInterface* InPDI,
    const FCk_HexGrid_Spec& InSpec, const FTransform& InPivotWorld) -> void
{
    auto Cells = TArray<FCk_HexCoord>{};
    auto Failure = ECk_HexGeometryFailure::None;
    if (NOT UCk_Utils_HexGeometry_UE::TryResolve_Domain(InSpec.Get_Domain(), Cells, Failure))
    { return; }
    for (const auto& Cell : Cells)
    {
        DrawCellOutline(InPDI, InSpec, InPivotWorld, Cell,
            ck_hex_grid_authored_overlay::CellColor(InSpec, Cell), 1.0f);
    }
}

auto ck::hex_grid_editor::DrawAuthoredLabels(FCanvas* InCanvas, const FSceneView* InView,
    const FCk_HexGrid_Spec& InSpec, const FTransform& InPivotWorld) -> void
{
    if (InCanvas == nullptr || InView == nullptr || GEngine == nullptr ||
        NOT ck_hex_grid_authored_overlay::ShowLabels.GetValueOnGameThread())
    { return; }
    auto Cells = TArray<FCk_HexCoord>{};
    auto Failure = ECk_HexGeometryFailure::None;
    if (NOT UCk_Utils_HexGeometry_UE::TryValidateWorldLayout(InSpec.Get_Layout(), InPivotWorld, Failure) ||
        NOT UCk_Utils_HexGeometry_UE::TryResolve_Domain(InSpec.Get_Domain(), Cells, Failure))
    { return; }
    auto* Font = GEngine->GetTinyFont();
    if (Font == nullptr)
    { return; }
    for (const auto& Cell : Cells)
    {
        auto Center = FVector2D{};
        if (NOT UCk_Utils_HexGeometry_UE::TryGet_LocalCenter(Cell, InSpec.Get_Layout(), Center, Failure))
        { continue; }
        const auto Screen = InView->WorldToScreen(InPivotWorld.TransformPosition(FVector(Center.X, Center.Y, 0.0)));
        auto Pixel = FVector2D{};
        if (Screen.W <= 0.0f || NOT InView->ScreenToPixel(Screen, Pixel))
        { continue; }
        const auto Tags = ck_hex_grid_authored_overlay::CellTags(InSpec, Cell);
        const auto Label = FString::Printf(TEXT("(%d,%d) %s"), Cell.Get_Q(), Cell.Get_R(), *Tags.ToStringSimple());
        auto Item = FCanvasTextItem(Pixel, FText::FromString(Label), Font, FLinearColor::White);
        Item.EnableShadow(FLinearColor::Black);
        InCanvas->DrawItem(Item);
    }
}
