#pragma once
bool IsAnyScriptLoaded();
void DisableScripts();
bool IsScriptsDisabled();
// True when the game functions behind the by-name shutdown (HasScriptLoaded,
// ForceCleanupForAllThreadsWithThisName, TerminateAllScriptsWithThisName)
// are resolved on this build.
bool CanDisableScriptsByName();
