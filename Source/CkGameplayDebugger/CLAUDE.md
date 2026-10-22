# Legacy GameplayDebugger profile lifecycle

Maintenance-only module. See the plugin root CLAUDE.md and CkFoundation/CLAUDE.md for shared doctrine.

The bridge owns category activity independently of the loaded profile. Replacing a profile validates all required entries before side effects, then deactivates the old profile, resets profile-associated selection, and activates the replacement only while the category is active. Unload and EndPlay release only activated profile resources. Submenu and action activation/deactivation are idempotent.

Profile lifecycle callbacks are non-reentrant: they must not load or unload profiles, change category/filter activation, or destroy their owning bridge synchronously. CK_ENSURE_IF_NOT rejects nested transitions before profile/selection mutation. Validation-only transition tracking is omitted under CK_DISABLE_ENSURE_CHECKS. Schedule an intended transition after the current callback has returned. Bridge destruction from inside a lifecycle callback violates this contract; engine destruction has already started before EndPlay can report it.

Focused native automation family: Ck.GameplayDebugger.TenetIntegrity. Fixtures use a transient UWorld and a spawned bridge, with concrete BlueprintNativeEvent hook counters.
