// integrator_ui.cpp

#include "../controls/integrator.h"
#include "../resource.h"

extern void TrimLine(string& line);
extern void GetParamStepsString(string& outputString, int numSteps);
extern void GetParamStepsValues(vector<double>& outputVector, int numSteps);

extern int g_minNumParamSteps;
extern int g_maxNumParamSteps;

static int s_dlgResult = IDCANCEL;

#define CSI_UI_INCLUDE_LEARN_DIALOGS
#include "learn_dialog.cpp"
#undef CSI_UI_INCLUDE_LEARN_DIALOGS

#define CSI_UI_INCLUDE_CONFIG_DIALOGS
#include "config_dialog.cpp"
#undef CSI_UI_INCLUDE_CONFIG_DIALOGS
