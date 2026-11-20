#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class FCk_HexGrid_EdModeToolkit : public FModeToolkit
{
public:
    virtual void Init(const TSharedPtr<IToolkitHost>& InHost, TWeakObjectPtr<UEdMode> InMode) override;
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual TSharedPtr<SWidget> GetInlineContent() const override;

private:
    TSharedPtr<SWidget> _Content;
};
