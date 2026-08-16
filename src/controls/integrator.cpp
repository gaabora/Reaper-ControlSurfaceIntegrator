#define INCLUDE_LOCALIZE_IMPORT_H
#include "integrator.h"

#include "../resource.h"

extern WDL_DLGRET dlgProcMainConfig(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

int g_minNumParamSteps = 1;
int g_maxNumParamSteps = 30;
int g_debugLevel = DEBUG_LEVEL_ERROR;
bool g_surfaceRawInDisplay;
bool g_surfaceInDisplay;
bool g_surfaceOutDisplay;

bool g_fxParamsWrite;

vector<HWND> g_openDialogs;
void CloseAllDialogs() {
    for (HWND hwnd : g_openDialogs)
        if (IsWindow(hwnd))
            DestroyWindow(hwnd);
    g_openDialogs.clear();
}
void GetPropertiesFromTokens(int start, int finish, const vector<string>& tokens, PropertyList& properties) {
    for (int i = start; i < finish; ++i) {
        std::string_view token = tokens[i];
        auto eqPos = token.find('=');

        if (eqPos != std::string_view::npos && token.find('=', eqPos + 1) == std::string_view::npos) {
            std::string key = std::string(token.substr(0, eqPos));
            std::string value = std::string(token.substr(eqPos + 1));

            PropertyType prop = PropertyList::prop_from_string(key.c_str());
            if (prop != PropertyType_Unknown) {
                properties.set_prop(prop, value.c_str());
            } else {
                properties.set_prop(prop, token.data()); // unknown properties are preserved as Unknown, key=value pair
                if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole("[WARNING] not supported property %s\n", key.c_str());
            }
        }
    }
}

void GetSteppedValues(const vector<string>& params, int start_idx, double& deltaValue, vector<double>& acceleratedDeltaValues, double& rangeMinimum, double& rangeMaximum, vector<double>& steppedValues, vector<int>& acceleratedTickValues) {
    int openSquareIndex = -1, closeSquareIndex = -1;

    for (int i = start_idx; i < params.size(); ++i)
        if (params[i] == "[") {
            openSquareIndex = i;
            break;
        }

    if (openSquareIndex < 0) return;

    for (int i = openSquareIndex + 1; i < params.size(); ++i)
        if (params[i] == "]") {
            closeSquareIndex = i;
            break;
        }

    if (closeSquareIndex > 0) {
        for (int i = openSquareIndex + 1; i < closeSquareIndex; ++i) {
            const char* str = params[i].c_str();

            if (str[0] == '(' && str[strlen(str) - 1] == ')') {
                str++; // skip (

                // (1.0,2.0,3.0) -> acceleratedDeltaValues : mode = 2
                // (1.0) -> deltaValue : mode = 1
                // (1) or (1,2,3) -> acceleratedTickValues : mode = 0
                const int mode = strstr(str, ".") ? strstr(str, ",") ? 2 : 1 : 0;

                while (*str) {
                    if (mode == 0) {
                        int v = 0;
                        if (WDL_NOT_NORMALLY(sscanf(str, "%d", &v) != 1)) break;
                        acceleratedTickValues.push_back(v);
                    } else {
                        double v = 0.0;
                        if (WDL_NOT_NORMALLY(sscanf(str, "%lf", &v) != 1)) break;
                        if (mode == 1) {
                            deltaValue = v;
                            break;
                        }
                        acceleratedDeltaValues.push_back(v);
                    }
                    while (*str && *str != ',') str++;
                    if (*str == ',') str++;
                }
            }
            // todo: support 1-3 syntax? else if (!strstr(str,".") && str[0] != '-' && strstr(str,"-"))
            else {
                // 1.0>3.0 writes to rangeMinimum/rangeMaximum
                // 1 or 1.0 -> steppedValues
                double a = 0.0, b = 0.0;
                const int nmatch = sscanf(str, "%lf>%lf", &a, &b);

                if (nmatch == 2) {
                    rangeMinimum = wdl_min(a, b);
                    rangeMaximum = wdl_max(a, b);
                } else if (WDL_NORMALLY(nmatch == 1)) {
                    steppedValues.push_back(a);
                }
            }
        }
    }
}

static double EnumSteppedValues(int numSteps, int stepNumber) {
    return floor(stepNumber / (double) (numSteps - 1) * 100.0 + 0.5) * 0.01;
}

void GetParamStepsString(string& outputString, int numSteps) { // appends to string
    // When number of steps equals 1, users are typically looking to use a button to reset.
    // A halfway value (0.5) is chosen as a good reset value instead of the previous 0.1.
    if (numSteps == 1) {
        outputString = "0.5";
    } else {
        for (int i = 0; i < numSteps; ++i) {
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "%.2f", EnumSteppedValues(numSteps, i));
            WDL_remove_trailing_decimal_zeros(tmp, 0);
            lstrcatn(tmp, " ", sizeof(tmp));
            outputString += tmp;
        }
    }
}

void GetParamStepsValues(vector<double>& outputVector, int numSteps) {
    outputVector.clear();
    for (int i = 0; i < numSteps; ++i)
        outputVector.push_back(EnumSteppedValues(numSteps, i));
}

void ReplaceAllWith(string& output, const char* charsToReplace, const char* replacement) {
    // replace all occurences of
    // any char in charsToReplace
    // with replacement string
    const string tmp = output;
    const char* p = tmp.c_str();
    output.clear();

    while (*p) {
        if (strchr(charsToReplace, *p) != NULL) output.append(replacement);
        else output.append(p, 1);
        p++;
    }
}

void GetTokens(vector<string>& tokens, const string& line) {
    bool insideQuote = false;
    string token;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == '"') {
            insideQuote = !insideQuote;
            if (!insideQuote) {
                tokens.push_back(token);
                token.clear();
            }
        } else if (isspace(c) && !insideQuote) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }

    if (!token.empty())
        tokens.push_back(token);
}

void GetTokens(vector<string>& tokens, const string& line, char delimiter) {
    istringstream iss(line);
    string token;
    while (getline(iss, token, delimiter))
        tokens.push_back(token);
}

// GetTokenLines moved to surface_parser.cpp (only used there)

int strToHex(string& valueStr) { return strtol(valueStr.c_str(), NULL, 16); }

// Class method implementations extracted to individual .cpp files:
//   ActionContext        -> actions/action_context.cpp
//   Zone                 -> controls/zone.cpp
//   Widget               -> controls/widget.cpp
//   FeedbackProcessors   -> controls/feedback.cpp
//   ZoneManager          -> controls/zone_manager.cpp
//   ModifierManager      -> controls/modifier_manager.cpp
//   TrackNavigationMgr   -> controls/track_navigation_manager.cpp
//   ControlSurface       -> controls/control_surface.cpp
//   Midi I/O + Surface   -> controls/midi/midi_surface.cpp
//   OSC I/O + Surface    -> controls/osc/osc_surface.cpp


////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSurfIntegrator
////////////////////////////////////////////////////////////////////////////////////////////////////////
CSurfIntegrator::CSurfIntegrator() {
    InitActionsDictionary();

    int size = 0;
    int index = projectconfig_var_getoffs("projtimemode", &size);
    timeModeOffs_ = size == 4 ? index : -1;

    index = projectconfig_var_getoffs("projtimemode2", &size);
    timeMode2Offs_ = size == 4 ? index : -1;

    index = projectconfig_var_getoffs("projmeasoffs", &size);
    measOffsOffs_ = size == 4 ? index : -1;

    index = projectconfig_var_getoffs("projtimeoffs", &size);
    timeOffsOffs_ = size == 8 ? index : -1;

    index = projectconfig_var_getoffs("panmode", &size);
    projectPanModeOffs_ = size == 4 ? index : -1;

    // these are supported by ~7.10+, previous versions we fallback to get_config_var() on-demand
    index = projectconfig_var_getoffs("projmetrov1", &size);
    projectMetronomePrimaryVolumeOffs_ = size == 8 ? index : -1;

    index = projectconfig_var_getoffs("projmetrov2", &size);
    projectMetronomeSecondaryVolumeOffs_ = size == 8 ? index : -1;
}

CSurfIntegrator::~CSurfIntegrator() {
    Shutdown();
    midiSurfacesIO_.clear();
    oscSurfacesIO_.clear();
    pages_.clear();
    actions_.clear();
}

int CSurfIntegrator::FindRegisteredReaScriptCommandId(const filesystem::path& scriptPath) const {
    KbdSectionInfo* mainActionSection = ::SectionFromUniqueID(0);
    if (!mainActionSection) return 0;

    const string scriptFilename = scriptPath.filename().string();
    int matchingCommandId = 0;
    for (int actionIndex = 0;; ++actionIndex) {
        const char* actionIdentifier = NULL;
        const int commandId = ::kbd_enumerateActions(mainActionSection, actionIndex, &actionIdentifier);
        (void) actionIdentifier;
        if (commandId == 0) break;
        const char* actionText = ::kbd_getTextFromCmd(commandId, mainActionSection);
        if (!actionText) continue;
        const string actionName = actionText;
        if (actionName.size() < scriptFilename.size() || actionName.compare(actionName.size() - scriptFilename.size(), scriptFilename.size(), scriptFilename) != 0) continue;
        if (matchingCommandId != 0) {
            LogToConsole("[ERROR] More than one registered ReaScript action ends with '%s'; cannot choose a command ID safely\n", scriptFilename.c_str());
            return -1;
        }
        matchingCommandId = commandId;
    }
    return matchingCommandId;
}

int CSurfIntegrator::ResolveReaScriptCommandId(const char* relativeScriptPath, const char* operationName) const {
    string normalizedRelativePath = relativeScriptPath ? relativeScriptPath : "";
    while (!normalizedRelativePath.empty() && (normalizedRelativePath[0] == '/' || normalizedRelativePath[0] == '\\')) normalizedRelativePath.erase(0, 1);
    const filesystem::path scriptPath = filesystem::path(GetResourcePath()) / normalizedRelativePath;
    if (!filesystem::is_regular_file(scriptPath)) {
        LogToConsole("[ERROR] FAILED to %s. ReaScript file does not exist: '%s'\n", operationName, scriptPath.string().c_str());
        return 0;
    }

    const string scriptPathString = scriptPath.string();
    const int registeredCommandId = ::AddRemoveReaScript(true, 0, scriptPathString.c_str(), true);
    if (registeredCommandId != 0) {
        if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] ReaScript registered: '%s', commandId=%d\n", scriptPathString.c_str(), registeredCommandId);
        return registeredCommandId;
    }

    const int existingCommandId = this->FindRegisteredReaScriptCommandId(scriptPath);
    if (existingCommandId > 0) {
        if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] Reusing registered ReaScript: '%s', commandId=%d\n", scriptPathString.c_str(), existingCommandId);
        return existingCommandId;
    }
    if (existingCommandId == 0) LogToConsole("[ERROR] FAILED to %s. AddRemoveReaScript failed and no registered action matches '%s'\n", operationName, scriptPathString.c_str());
    return 0;
}

const char* CSurfIntegrator::GetDescString() { return ProductIdentity::DisplayName; }

const char* CSurfIntegrator::GetConfigString() {
    return "0 0";
}

int CSurfIntegrator::Extended(int call, void* parm1, void* parm2, void* parm3) {
    WDL_MutexLock lock(&csiMutex_);
    if (call == CSURF_EXT_SUPPORTS_EXTENDED_TOUCH) return 1;
    if (call == CSURF_EXT_RESET) Init();
    if (call == CSURF_EXT_SETFXCHANGE) TrackFXListChanged((MediaTrack*) parm1); // parm1=(MediaTrack*)track, whenever FX are added, deleted, or change order
    if (call == CSURF_EXT_SETMIXERSCROLL) {
        MediaTrack* leftPtr = (MediaTrack*) parm1;
        int offset = CSurf_TrackToID(leftPtr, true);
        offset--;
        if (offset < 0) offset = 0;
        SetTrackOffset(offset);
    }
    return 1;
}

static IReaperControlSurface* createFunc(const char* type_string, const char* configString, int* errStats) {
    return new CSurfIntegrator();
}


static HWND configFunc(const char* type_string, HWND parent, const char* initConfigString) {
    HWND hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_CSI), parent, dlgProcMainConfig, (LPARAM) initConfigString);
    if (hwnd) g_openDialogs.push_back(hwnd);
    return hwnd;
}

reaper_csurf_reg_t csurf_integrator_reg = { ProductIdentity::ReaperRegistrationId, ProductIdentity::DisplayName, createFunc, configFunc };

void localize_init(void* (*GetFunc)(const char* name)) {
    *(void**) &importedLocalizeFunc = GetFunc("__localizeFunc");
    *(void**) &importedLocalizeMenu = GetFunc("__localizeMenu");
    *(void**) &importedLocalizeInitializeDialog = GetFunc("__localizeInitializeDialog");
    *(void**) &importedLocalizePrepareDialog = GetFunc("__localizePrepareDialog");
}
