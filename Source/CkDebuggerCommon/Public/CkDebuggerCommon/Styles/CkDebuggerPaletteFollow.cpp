#include "CkDebuggerPaletteFollow.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"

// ====================================================================================================================

namespace ck_debug_palette_follow
{
    struct FFollowed
    {
        TWeakPtr<FSlateStyleSet> Live;
        ck::debug_style::FStyleSetFactory Factory;
        bool IsLight = false;
    };

    auto Get_Followed() -> TArray<FFollowed>&
    {
        static auto Followed = TArray<FFollowed>{};
        return Followed;
    }

    auto Get_TickerHandle() -> FTSTicker::FDelegateHandle&
    {
        static auto Handle = FTSTicker::FDelegateHandle{};
        return Handle;
    }

    template <typename T_WidgetStyle>
    auto DoCopyWidgetStyle(
        FSlateStyleSet& InLiveStyle,
        const FSlateStyleSet& InFreshStyle,
        FName InKey) -> bool
    {
        if (NOT InFreshStyle.HasWidgetStyle<T_WidgetStyle>(InKey) || NOT InLiveStyle.HasWidgetStyle<T_WidgetStyle>(InKey))
        { return false; }

        const_cast<T_WidgetStyle&>(InLiveStyle.GetWidgetStyle<T_WidgetStyle>(InKey)) = InFreshStyle.GetWidgetStyle<T_WidgetStyle>(InKey);
        return true;
    }

    auto OnTick(
        float InDeltaTime) -> bool
    {
        const auto IsLight = CkStyle::IsLight();
        auto& Followed = Get_Followed();
        auto AnyCopied = false;

        for (auto& Entry : Followed)
        {
            const auto Live = Entry.Live.Pin();
            if (NOT Live.IsValid() || Entry.IsLight == IsLight)
            { continue; }

            Entry.IsLight = IsLight;
            ck::debug_style::Request_CopyStyleInPlace(*Live, *Entry.Factory());
            AnyCopied = true;
        }

        if (NOT AnyCopied || NOT FSlateApplication::IsInitialized())
        { return true; }

        FSlateApplication::Get().InvalidateAllWidgets(false);

        if (GIsEditor)
        {
            auto Info = FNotificationInfo{FText::FromString(TEXT("Ck tools switched palette with the editor theme. A Ck window that still shows the old palette applies it when reopened."))};
            Info.ExpireDuration = 6.0f;
            FSlateNotificationManager::Get().AddNotification(Info);
        }

        return true;
    }
}

// ====================================================================================================================

auto
    ck::debug_style::
    Follow_Palette(
        const TSharedRef<FSlateStyleSet>& InLiveStyle,
        FStyleSetFactory InFactory)
    -> void
{
    using namespace ck_debug_palette_follow;

    Get_Followed().Add(FFollowed{InLiveStyle, MoveTemp(InFactory), CkStyle::IsLight()});

    if (NOT Get_TickerHandle().IsValid())
    { Get_TickerHandle() = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&OnTick)); }
}

auto
    ck::debug_style::
    Unfollow_Palette(
        const FSlateStyleSet& InLiveStyle)
    -> void
{
    using namespace ck_debug_palette_follow;

    Get_Followed().RemoveAll([&InLiveStyle](const FFollowed& InEntry)
    {
        const auto Live = InEntry.Live.Pin();
        return NOT Live.IsValid() || Live.Get() == &InLiveStyle;
    });

    if (Get_Followed().IsEmpty() && Get_TickerHandle().IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(Get_TickerHandle());
        Get_TickerHandle().Reset();
    }
}

auto
    ck::debug_style::
    Request_CopyStyleInPlace(
        FSlateStyleSet& InLiveStyle,
        const FSlateStyleSet& InFreshStyle)
    -> void
{
    using namespace ck_debug_palette_follow;

    for (const auto& Key : static_cast<const ISlateStyle&>(InFreshStyle).GetStyleKeys())
    {
        const auto* FreshBrush = InFreshStyle.GetOptionalBrush(Key, nullptr, nullptr);
        if (FreshBrush != nullptr)
        {
            const auto* LiveBrush = InLiveStyle.GetOptionalBrush(Key, nullptr, nullptr);
            if (LiveBrush != nullptr && LiveBrush->GetResourceName().IsNone() && FreshBrush->GetResourceName().IsNone())
            { *const_cast<FSlateBrush*>(LiveBrush) = *FreshBrush; }

            continue;
        }

        if (DoCopyWidgetStyle<FTextBlockStyle>(InLiveStyle, InFreshStyle, Key)) { continue; }
        if (DoCopyWidgetStyle<FButtonStyle>(InLiveStyle, InFreshStyle, Key))    { continue; }
        if (DoCopyWidgetStyle<FCheckBoxStyle>(InLiveStyle, InFreshStyle, Key))  { continue; }
        if (DoCopyWidgetStyle<FTableRowStyle>(InLiveStyle, InFreshStyle, Key))  { continue; }
    }
}

// ====================================================================================================================
