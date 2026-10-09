#include "CkDebugOverlay_Provider_IntegerAttributes.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Attribute utils — mirroring CkInspector_IntegerAttributes.cpp includes
#include "CkAttribute/IntegerAttribute/CkIntegerAttribute_Utils.h"
#include "CkLabel/CkLabel_Utils.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_ObservationBank.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_IntegerAttributes,
    "Ck.OnScreenDebugger.Provider.IntegerAttributes")

// A single "catch-all" field tag for the collection (same rationale as FloatAttributes).
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_IntegerAttributes_Value,
    "Ck.OnScreenDebugger.Provider.IntegerAttributes.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()      { return TAG_Ck_OnScreenDebugger_Provider_IntegerAttributes; }
    FGameplayTag FieldTag_Value()   { return TAG_Ck_OnScreenDebugger_Provider_IntegerAttributes_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_IntegerAttributes::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_IntegerAttributes::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_IntegerAttributes::CanInspect:
//   UCk_Utils_IntegerAttribute_UE::Has_Any(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_IntegerAttributes::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    if (UCk_Utils_IntegerAttribute_UE::Has_Any(Entity)) { return true; }
    auto Observation = FCk_DebugOverlay_SourceObservation{};
    return FCk_DebugOverlay_ObservationBankUtils::TryGetPublishedSource(Entity, Observation) &&
        NOT Observation.Integers.IsEmpty();
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// FieldTag approach: same as FloatAttributes — all rows share FieldTag_Value().
// Attribute name is embedded in the Value string as "AttrName: 42".
//
// EntryFilter: if non-empty, matched against the attribute's gameplay label tag.
//
// BATCH-VERIFY:
//   UCk_Utils_IntegerAttribute_UE::Has_Any(Entity)                              — confirmed from CkInspector_IntegerAttributes.cpp
//   UCk_Utils_IntegerAttribute_UE::ForEach(Entity, TFunction<...>)             — confirmed from CkInspector_IntegerAttributes.cpp
//   UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute)                         — confirmed from CkInspector_IntegerAttributes.cpp
//   UCk_Utils_IntegerAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent)     — confirmed from CkInspector_IntegerAttributes.cpp
//   UCk_Utils_IntegerAttribute_UE::Get_BaseValue(Attr, ECk_MinMaxCurrent)      — confirmed from CkInspector_IntegerAttributes.cpp
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_IntegerAttributes::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    if (ck::Is_NOT_Valid(Entity)) { return; }
    const auto Passes = [&Cfg](const FGameplayTag& AttributeTag,
        FString& OutAttributeName) -> bool
    {
        OutAttributeName = AttributeTag.IsValid()
            ? AttributeTag.GetTagName().ToString()
            : FString(TEXT("Unnamed"));
        if (NOT UCk_DebugOverlay_Settings::Get_PassesAttributeFilter(OutAttributeName))
        { return false; }
        if (NOT Cfg.EntryFilter.IsEmpty() &&
            (NOT AttributeTag.IsValid() ||
                NOT Cfg.EntryFilter.Matches(FGameplayTagContainer(AttributeTag))))
        { return false; }
        return true;
    };

    const auto EmitValue = [&Out](const FString& AttributeName,
        int32 BaseVal, int32 FinalVal)
    {
        auto DisplayStr = AttributeName + TEXT(": ") + ck::Format_UE(TEXT("{}"), FinalVal);
        if (BaseVal != FinalVal)
        { DisplayStr += ck::Format_UE(TEXT(" (base {})"), BaseVal); }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value = FText::FromString(DisplayStr);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    };

    if (UCk_Utils_IntegerAttribute_UE::Has_Any(Entity))
    {
        auto MutableEntity = Entity;
        UCk_Utils_IntegerAttribute_UE::ForEach(MutableEntity,
            [&Passes, &EmitValue](FCk_Handle_IntegerAttribute InAttribute)
            {
                const auto AttributeTag = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
                auto AttributeName = FString{};
                if (NOT Passes(AttributeTag, AttributeName)) { return; }
                const auto FinalVal = UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
                    InAttribute, ECk_MinMaxCurrent::Current);
                const auto BaseVal = UCk_Utils_IntegerAttribute_UE::Get_BaseValue(
                    InAttribute, ECk_MinMaxCurrent::Current);
                EmitValue(AttributeName, BaseVal, FinalVal);
            });
        return;
    }

    auto Observation = FCk_DebugOverlay_SourceObservation{};
    if (NOT FCk_DebugOverlay_ObservationBankUtils::TryGetPublishedSource(Entity, Observation))
    { return; }
    for (const auto& Fact : Observation.Integers)
    {
        auto AttributeName = FString{};
        if (Passes(Fact.Label, AttributeName))
        { EmitValue(AttributeName, Fact.Base, Fact.Final); }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_IntegerAttributes::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    // Show count of integer attributes in compact mode.
    int32 Count = 0;
    if (UCk_Utils_IntegerAttribute_UE::Has_Any(Entity))
    {
        auto MutableEntity = Entity;
        UCk_Utils_IntegerAttribute_UE::ForEach(MutableEntity,
            [&Count](FCk_Handle_IntegerAttribute) { ++Count; });
    }
    else
    {
        auto Observation = FCk_DebugOverlay_SourceObservation{};
        if (NOT FCk_DebugOverlay_ObservationBankUtils::TryGetPublishedSource(Entity, Observation))
        { return {}; }
        Count = Observation.Integers.Num();
    }

    if (Count == 0) { return {}; }
    return FString::Printf(TEXT("I:%dattr"), Count);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_IntegerAttributes)

// --------------------------------------------------------------------------------------------------------------------
