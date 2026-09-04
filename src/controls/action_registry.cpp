// action_registry.cpp — CSurfIntegrator::InitActionsDictionary()
//
// Populates the actions_ map from the ACTION_TYPE_LIST macro.
// Kept separate from integrator.cpp to reduce per-TU compile cost.

#include "integrator.h"
#include "../actions/reaper_actions.h"
#include "../actions/manager_actions.h"

void CSurfIntegrator::InitActionsDictionary() {
#define X(className, strName) actions_.insert({ strName, std::make_unique<className>() });
    ACTION_TYPE_LIST(X)
#undef X
}
