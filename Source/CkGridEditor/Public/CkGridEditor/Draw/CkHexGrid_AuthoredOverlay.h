#pragma once

#include "CoreMinimal.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment_Data.h"

class FCanvas;
class FPrimitiveDrawInterface;
class FSceneView;

namespace ck::hex_grid_editor
{
    CKGRIDEDITOR_API auto DrawAuthoredOverlay(FPrimitiveDrawInterface* InPDI,
        const FCk_HexGrid_Spec& InSpec, const FTransform& InPivotWorld) -> void;
    CKGRIDEDITOR_API auto DrawCellOutline(FPrimitiveDrawInterface* InPDI,
        const FCk_HexGrid_Spec& InSpec, const FTransform& InPivotWorld,
        const FCk_HexCoord& InCell, const FLinearColor& InColor, float InThickness = 2.0f) -> void;
    CKGRIDEDITOR_API auto DrawAuthoredLabels(FCanvas* InCanvas, const FSceneView* InView,
        const FCk_HexGrid_Spec& InSpec, const FTransform& InPivotWorld) -> void;
}
