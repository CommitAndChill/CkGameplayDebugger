#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_SourceAdmission.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/ContextOwner/CkContextOwner_Fragment.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkLabel/CkLabel_Fragment.h"
#include "Engine/World.h"

namespace
{
    struct FCk_DebugOverlay_SourceAdmission
    {
        CK_GENERATED_BODY(FCk_DebugOverlay_SourceAdmission);

        int64 Generation = 0;
        bool Published = false;
        TArray<FCk_Handle> Roots;
        TSet<FCk_Handle> KnownMembers;
        TSet<FCk_Handle> CurrentMembers;
        TMap<FCk_Handle, FCk_Handle> PublishedParents;
    };

    auto Allocated(const FCk_Handle& H) -> bool
    {
        if (!H.IsRegistryValid()) { return false; }
        return H.Get_RegistryView().IsValid(H.Get_Entity());
    }

    auto Live(const FCk_Handle& H) -> bool
    {
        return Allocated(H) && ck::IsValid(H) &&
            !H.Has_Any<ck::FTag_DestroyEntity_Initiate,
                ck::FTag_DestroyEntity_EndPlay, ck::FTag_DestroyEntity_Teardown,
                ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>();
    }

    auto SameRegistry(const FCk_Handle& A, const FCk_Handle& B) -> bool
    {
        return Allocated(A) && Allocated(B) &&
            A.Get_RegistryView().Get_RegistryHandle() ==
                B.Get_RegistryView().Get_RegistryHandle();
    }

    auto ExactWorldRoot(UWorld* World, const FCk_Handle& Root) -> bool
    {
        if (!IsInGameThread() || !IsValid(World) ||
            World->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed) ||
            !World->IsGameWorld() || World->bIsTearingDown || World->IsBeingCleanedUp() ||
            !Live(Root) || !Root.Has<TWeakObjectPtr<UWorld>>())
        { return false; }
        const auto Registry = Root.Get_RegistryView();
        return Root.Get<TWeakObjectPtr<UWorld>>().Get() == World &&
            FCk_Handle{Registry.Get_TransientEntity(), Registry.Get_RegistryHandle()} == Root &&
            UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*World) == Root;
    }

    auto DirectParent(const FCk_Handle& Child, const FCk_Handle& Root,
        FCk_Handle& OutParent) -> bool
    {
        OutParent = {};
        if (!Live(Child) || !SameRegistry(Child, Root) ||
            !Child.Has<ck::FFragment_LifetimeOwner>())
        { return false; }
        const auto Parent = Child.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        if (!Live(Parent) || !SameRegistry(Parent, Root) || Parent == Child)
        { return false; }
        OutParent = Parent;
        return true;
    }

    // Invalid ancestry fails closed. A complete walk is bounded by the finite live registry.
    auto FindScopeRoot(const FCk_Handle& Entity, const FCk_Handle& WorldRoot,
        const FCk_DebugOverlay_SourceAdmission& Scope,
        TArray<FCk_Handle>& OutChain, bool& OutScoped,
        bool& OutKnownLineage) -> bool
    {
        OutChain.Reset();
        OutScoped = false;
        OutKnownLineage = false;
        auto Seen = TSet<FCk_Handle>{};
        auto Current = Entity;
        while (Current != WorldRoot)
        {
            if (!Live(Current) || !SameRegistry(Current, WorldRoot) || Seen.Contains(Current))
            { return false; }
            Seen.Add(Current);
            OutChain.Add(Current);
            OutKnownLineage |= Scope.KnownMembers.Contains(Current);
            if (Scope.Roots.Contains(Current))
            {
                OutScoped = true;
                return true;
            }
            FCk_Handle Parent;
            if (!DirectParent(Current, WorldRoot, Parent)) { return false; }
            Current = Parent;
        }
        return true;
    }

    auto Fail(FString& OutReason, const TCHAR* Why) -> bool
    { OutReason = Why; return false; }
}

auto UCk_DebugOverlay_SourceAdmission_UE::TryBegin(UWorld* World,
    const FCk_Handle& WorldRoot, const int64 Generation,
    const TArray<FCk_Handle>& DirectRoots, FString& OutReason) -> bool
{
    OutReason.Reset();
    if (!ExactWorldRoot(World, WorldRoot) || Generation <= 0 || DirectRoots.IsEmpty())
    { return Fail(OutReason, TEXT("Invalid world, generation, or empty source roots")); }

    auto Distinct = TSet<FCk_Handle>{};
    for (const auto& Root : DirectRoots)
    {
        FCk_Handle Parent;
        if (Distinct.Contains(Root) || !DirectParent(Root, WorldRoot, Parent) ||
            Parent != WorldRoot)
        { return Fail(OutReason, TEXT("Source root is duplicate, foreign, or not a live direct world child")); }
        if (Root.Has_Any<ck::FFragment_Transform, ck::FFragment_GameplayLabel>())
        { return Fail(OutReason, TEXT("Source root already has selectable composition")); }
        if (Root.Has<ck::FFragment_ContextOwner>())
        {
            const auto Context = Root.Get<ck::FFragment_ContextOwner>().Get_Entity();
            if (Context != Root && Context != WorldRoot)
            { return Fail(OutReason, TEXT("Source root has an unexpected context owner")); }
        }
        Distinct.Add(Root);
    }

    if (WorldRoot.Has<FCk_DebugOverlay_SourceAdmission>())
    {
        const auto& Old = WorldRoot.Get<FCk_DebugOverlay_SourceAdmission>();
        if (Generation <= Old.Generation)
        { return Fail(OutReason, TEXT("Source generation did not advance")); }
        for (const auto& OldRoot : Old.Roots)
        {
            if (Allocated(OldRoot))
            { return Fail(OutReason, TEXT("Previous source root still allocated")); }
        }
        for (const auto& OldMember : Old.KnownMembers)
        {
            if (Allocated(OldMember))
            { return Fail(OutReason, TEXT("Previous known source member still allocated")); }
        }
    }

    if (!ExactWorldRoot(World, WorldRoot))
    { return Fail(OutReason, TEXT("World changed before source registration")); }
    auto Next = FCk_DebugOverlay_SourceAdmission{};
    Next.Generation = Generation;
    Next.Roots = DirectRoots;
    Next.KnownMembers = MoveTemp(Distinct); // closed roots remain known
    auto MutableRoot = WorldRoot;
    MutableRoot.AddOrReplace<FCk_DebugOverlay_SourceAdmission>(MoveTemp(Next));
    return true;
}

auto UCk_DebugOverlay_SourceAdmission_UE::TryPublish(UWorld* World,
    const FCk_Handle& WorldRoot, const int64 Generation,
    const TArray<FCk_Handle>& Members, FString& OutReason) -> bool
{
    OutReason.Reset();
    if (!ExactWorldRoot(World, WorldRoot) ||
        !WorldRoot.Has<FCk_DebugOverlay_SourceAdmission>())
    { return Fail(OutReason, TEXT("Exact source scope unavailable")); }
    const auto Old = WorldRoot.Get<FCk_DebugOverlay_SourceAdmission>();
    if (Old.Generation != Generation || Members.IsEmpty())
    { return Fail(OutReason, TEXT("Source scope generation or member set invalid")); }

    auto Proposed = TSet<FCk_Handle>{};
    for (const auto& Member : Members)
    {
        if (!Live(Member) || !SameRegistry(Member, WorldRoot) || Proposed.Contains(Member) ||
            Member == WorldRoot)
        { return Fail(OutReason, TEXT("Invalid, foreign, or duplicate source member")); }
        Proposed.Add(Member);
    }
    for (const auto& Root : Old.Roots)
    {
        FCk_Handle Parent;
        if (!Proposed.Contains(Root) || !DirectParent(Root, WorldRoot, Parent) ||
            Parent != WorldRoot)
        { return Fail(OutReason, TEXT("Retained source root missing or moved")); }
    }

    auto Parents = TMap<FCk_Handle, FCk_Handle>{};
    for (const auto& Member : Members)
    {
        auto Seen = TSet<FCk_Handle>{};
        auto Current = Member;
        while (!Old.Roots.Contains(Current))
        {
            if (Seen.Contains(Current) || !Proposed.Contains(Current))
            { return Fail(OutReason, TEXT("Source member owner chain is cyclic or incomplete")); }
            Seen.Add(Current);
            FCk_Handle Parent;
            if (!DirectParent(Current, WorldRoot, Parent) || Parent == WorldRoot ||
                !Proposed.Contains(Parent))
            { return Fail(OutReason, TEXT("Source member escapes retained roots")); }
            Parents.Add(Current, Parent);
            Current = Parent;
        }
        Parents.Add(Current, WorldRoot);
    }
    for (const auto& Member : Members)
    {
        if (!Live(Member) || !SameRegistry(Member, WorldRoot))
        { return Fail(OutReason, TEXT("Source member changed before publication")); }
    }
    if (!ExactWorldRoot(World, WorldRoot))
    { return Fail(OutReason, TEXT("World changed before publication")); }

    auto Next = Old;
    Next.Published = true;
    for (const auto& Member : Proposed) { Next.KnownMembers.Add(Member); }
    Next.CurrentMembers = MoveTemp(Proposed);
    Next.PublishedParents = MoveTemp(Parents);
    auto MutableRoot = WorldRoot;
    MutableRoot.AddOrReplace<FCk_DebugOverlay_SourceAdmission>(MoveTemp(Next));
    return true;
}

auto UCk_DebugOverlay_SourceAdmission_UE::TryClose(UWorld* World,
    const FCk_Handle& WorldRoot, const int64 Generation, FString& OutReason) -> bool
{
    OutReason.Reset();
    if (!ExactWorldRoot(World, WorldRoot) ||
        !WorldRoot.Has<FCk_DebugOverlay_SourceAdmission>())
    { return Fail(OutReason, TEXT("Exact source scope unavailable")); }
    const auto Old = WorldRoot.Get<FCk_DebugOverlay_SourceAdmission>();
    if (Old.Generation != Generation)
    { return Fail(OutReason, TEXT("Source generation mismatch")); }
    if (!Old.Published) { return true; }
    auto Next = Old;
    Next.Published = false;
    Next.CurrentMembers.Reset();
    Next.PublishedParents.Reset();
    auto MutableRoot = WorldRoot;
    MutableRoot.AddOrReplace<FCk_DebugOverlay_SourceAdmission>(MoveTemp(Next));
    return true;
}

auto UCk_DebugOverlay_SourceAdmission_UE::IsAdmitted(const FCk_Handle& Entity) -> bool
{
    if (!IsInGameThread() || !Live(Entity)) { return false; }
    const auto Registry = Entity.Get_RegistryView();
    const FCk_Handle WorldRoot{Registry.Get_TransientEntity(), Registry.Get_RegistryHandle()};
    if (!Live(WorldRoot) || !WorldRoot.Has<TWeakObjectPtr<UWorld>>()) { return false; }
    auto* World = WorldRoot.Get<TWeakObjectPtr<UWorld>>().Get();
    if (!ExactWorldRoot(World, WorldRoot)) { return false; }
    if (Entity == WorldRoot || !WorldRoot.Has<FCk_DebugOverlay_SourceAdmission>())
    { return true; }

    const auto& Scope = WorldRoot.Get<FCk_DebugOverlay_SourceAdmission>();
    if (Scope.Generation <= 0) { return false; }
    // Retain identity even if a known member was moved outside its source root.
    if (Scope.KnownMembers.Contains(Entity) && !Scope.Published) { return false; }
    TArray<FCk_Handle> Chain;
    bool Scoped = false;
    bool KnownLineage = false;
    if (!FindScopeRoot(Entity, WorldRoot, Scope, Chain, Scoped, KnownLineage)) { return false; }
    if (!Scoped) { return !KnownLineage; }
    if (!Scope.Published || !Scope.CurrentMembers.Contains(Entity)) { return false; }
    for (int32 I = 0; I < Chain.Num(); ++I)
    {
        const auto* CapturedParent = Scope.PublishedParents.Find(Chain[I]);
        FCk_Handle CurrentParent;
        if (CapturedParent == nullptr || !DirectParent(Chain[I], WorldRoot, CurrentParent) ||
            CurrentParent != *CapturedParent)
        { return false; }
    }
    return true;
}
