#include "CkGridEditor/EdMode/CkHexGrid_EdModeToolkit.h"

#include "CkGridEditor/EdMode/CkHexGrid_EdMode.h"
#include "CkGrid/HexGridSystem/Authoring/CkHexGrid_Spec.h"
#include "SGameplayTagCombo.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

void FCk_HexGrid_EdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InHost, TWeakObjectPtr<UEdMode> InMode)
{
    const auto Mode = TWeakObjectPtr<UCk_HexGrid_EdMode>(Cast<UCk_HexGrid_EdMode>(InMode.Get()));
    _Content = SNew(SScrollBox)
        + SScrollBox::Slot().Padding(12.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(STextBlock).AutoWrapText(true)
                .Text_Lambda([Mode]
                {
                    const auto* Asset = Mode.IsValid() ? Mode->Get_SelectedAsset() : nullptr;
                    return Asset != nullptr ? FText::FromString(Asset->GetPathName()) :
                        NSLOCTEXT("CkHexGrid", "SelectHex", "Select a hex grid spawner with a loaded authoring asset.");
                })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [
                SNew(STextBlock).Text(NSLOCTEXT("CkHexGrid", "Radius", "Disk brush radius"))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(SNumericEntryBox<int32>).MinValue(0).MaxValue(32).MinSliderValue(0).MaxSliderValue(8)
                .Value_Lambda([Mode]() -> TOptional<int32> { return Mode.IsValid() ? Mode->Get_Brush().Radius : 0; })
                .OnValueChanged_Lambda([Mode](int32 InValue)
                {
                    if (Mode.IsValid())
                    {
                        auto Brush = Mode->Get_Brush();
                        Brush.Radius = InValue;
                        Mode->Set_Brush(Brush);
                    }
                })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([Mode] { return Mode.IsValid() && Mode->Get_Brush().PaintDisabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged_Lambda([Mode](ECheckBoxState InState)
                {
                    if (Mode.IsValid())
                    {
                        auto Brush = Mode->Get_Brush();
                        Brush.PaintDisabled = InState == ECheckBoxState::Checked;
                        Mode->Set_Brush(Brush);
                    }
                })
                [ SNew(STextBlock).Text(NSLOCTEXT("CkHexGrid", "DisabledTool", "Paint initial disabled state (off: paint tags)")) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(SGameplayTagCombo)
                .Tag_Lambda([Mode] { return Mode.IsValid() ? Mode->Get_Brush().Tag : FGameplayTag{}; })
                .OnTagChanged_Lambda([Mode](FGameplayTag InTag)
                {
                    if (Mode.IsValid())
                    {
                        auto Brush = Mode->Get_Brush();
                        Brush.Tag = InTag;
                        Mode->Set_Brush(Brush);
                    }
                })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(SButton).Text(NSLOCTEXT("CkHexGrid", "OpenAsset", "Open board asset"))
                .OnClicked_Lambda([Mode]
                {
                    if (Mode.IsValid() && Mode->Get_SelectedAsset() != nullptr && GEditor != nullptr)
                    {
                        if (auto* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
                        { Editors->OpenEditorForAsset(Mode->Get_SelectedAsset()); }
                    }
                    return FReply::Handled();
                })
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock).AutoWrapText(true)
                .Text(NSLOCTEXT("CkHexGrid", "Instructions",
                    "Drag to paint. Shift-drag erases. Escape cancels the whole stroke. Release commits one undo step.\n\n"
                    "Cell tags add to the board defaults; erasing a per-cell tag keeps inherited defaults. "
                    "Edit domain, defaults and named blockers in the board asset. Yellow previews a staged stroke; white marks the brush."))
            ]
        ];
    FModeToolkit::Init(InHost, InMode);
}

FName FCk_HexGrid_EdModeToolkit::GetToolkitFName() const
{
    return TEXT("CkHexGridPaintToolkit");
}

FText FCk_HexGrid_EdModeToolkit::GetBaseToolkitName() const
{
    return NSLOCTEXT("CkHexGrid", "Toolkit", "Hex Grid Paint");
}

TSharedPtr<SWidget> FCk_HexGrid_EdModeToolkit::GetInlineContent() const
{
    return _Content;
}
