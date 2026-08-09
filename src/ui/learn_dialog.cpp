#ifdef CSI_UI_INCLUDE_LEARN_DIALOGS
// Learn FX dialog state — all mutable state belonging to the FX Learn workflow.
// Grouped here so state is scoped to the dialog lifetime and dangling-pointer
// risks are surfaced in one place rather than scattered across 10+ static vars.
struct LearnFXState {
    bool isUpdatingParameters = false;
    Widget* currentWidget = nullptr;
    int currentModifier = 0;
    string pageSurfaceFXLearnLevel;
    int lastTouchedParamNum = -1;
    double lastTouchedParamValue = -1.0;
    MediaTrack* focusedTrack = nullptr;
    int fxSlot = 0;
    char fxName[MEDBUF] = {};
    char fxAlias[MEDBUF] = {};
    ActionContext* editAdvancedContext = nullptr;  // owned by dlgProcEditAdvanced
};
static LearnFXState s_learnFX;
static HWND s_hwndLearnFXDlg = NULL;

static void OnDialogInit(HWND hwndDlg) {
    g_openDialogs.push_back(hwndDlg);
}

static void OnDialogDestroy(HWND hwndDlg, int ret) {
    g_openDialogs.erase(std::remove(g_openDialogs.begin(), g_openDialogs.end(), hwndDlg), g_openDialogs.end());
    EndDialog(hwndDlg, ret);
}

struct FXRowLayout {
    char suffix[SMLBUF];
    char modifiers[SMLBUF];
    int modifier;

    FXRowLayout() {
        suffix[0] = 0;
        modifiers[0] = 0;
        modifier = 0;
    }
};

static ActionContext* GetFirstContext(ZoneManager* zoneManager, Widget* widget, int modifier) {
    if (widget == NULL)
        return NULL;

    const vector<unique_ptr<ActionContext>>& actionContexts = zoneManager->GetLearnFocusedFXActionContexts(widget, modifier);

    if (actionContexts.size() > 0)
        return actionContexts[0].get();
    else
        return NULL;
}

struct FXCell {
    ZoneManager* const zoneManager;

    vector<Widget*> controlWidgets;
    vector<Widget*> displayWidgets;

    string suffix;
    int modifier = 0;
    int channel = 0;

    FXCell(ZoneManager* const aZoneManager)
        : zoneManager(aZoneManager) {}

    FXCell(ZoneManager* const aZoneManager, string aSuffix, int aModifier, int aChannel)
        : zoneManager(aZoneManager)
        , suffix(aSuffix)
        , modifier(aModifier)
        , channel(aChannel) {}

    ActionContext* GetNameContext(Widget* widget) {
        if (widget == NULL)
            return NULL;

        for (auto displayWidget : displayWidgets) {
            ActionContext* nameContext = GetFirstContext(zoneManager, displayWidget, modifier);

            if (nameContext != NULL && nameContext->GetAction()->GetType() == ActionType::FixedTextDisplay) {
                ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);

                if (paramContext != NULL && nameContext->GetParamIndex() == paramContext->GetParamIndex())
                    return nameContext;
            }
        }

        return NULL;
    }

    ActionContext* GetValueContext(Widget* widget) {
        if (widget == NULL)
            return NULL;

        for (auto displayWidget : displayWidgets) {
            ActionContext* valueContext = GetFirstContext(zoneManager, displayWidget, modifier);

            if (valueContext != NULL && valueContext->GetAction()->GetType() == ActionType::FXParamValueDisplay) {
                ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);

                if (paramContext != NULL && valueContext->GetParamIndex() == paramContext->GetParamIndex())
                    return valueContext;
            }
        }

        return NULL;
    }

    Widget* GetNameWidget(Widget* widget) {
        for (auto displayWidget : displayWidgets) {
            ActionContext* nameContext = GetFirstContext(zoneManager, displayWidget, modifier);

            if (nameContext != NULL && nameContext->GetAction()->GetType() == ActionType::FixedTextDisplay) {
                ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);

                if (paramContext != NULL && nameContext->GetParamIndex() == paramContext->GetParamIndex())
                    return displayWidget;
            }
        }

        return NULL;
    }

    Widget* GetValueWidget(Widget* widget) {
        for (auto displayWidget : displayWidgets) {
            ActionContext* valueContext = GetFirstContext(zoneManager, displayWidget, modifier);

            if (valueContext != NULL && valueContext->GetAction()->GetType() == ActionType::FXParamValueDisplay) {
                ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);

                if (paramContext != NULL && valueContext->GetParamIndex() == paramContext->GetParamIndex())
                    return displayWidget;
            }
        }

        return NULL;
    }

    void SetNameWidget(Widget* widget, const char* displayWidgetName, const char* paramName) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] SetNameWidget %s %s %s\n", widget->GetName(), displayWidgetName, paramName);
        for (auto displayWidget : displayWidgets) {
            if (IsSameString(displayWidget->GetName(), displayWidgetName)) {
                ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);
                ActionContext* nameContext = GetFirstContext(zoneManager, displayWidget, modifier);

                if (nameContext != NULL && paramContext != NULL) {
                    nameContext->SetAction(zoneManager->GetCSI()->GetAction("FixedTextDisplay"));
                    nameContext->SetParamIndex(paramContext->GetParamIndex());
                    nameContext->SetStringParam(paramName);
                }

                break;
            }
        }
    }

    void SetValueWidget(Widget* widget, const char* displayWidgetName) {
        for (auto displayWidget : displayWidgets) {
            if (IsSameString(displayWidget->GetName(), displayWidgetName)) {
                ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);
                ActionContext* nameContext = GetFirstContext(zoneManager, displayWidget, modifier);

                if (nameContext != NULL && paramContext != NULL) {
                    nameContext->SetAction(zoneManager->GetCSI()->GetAction("FXParamValueDisplay"));
                    nameContext->SetParamIndex(paramContext->GetParamIndex());
                    nameContext->SetStringParam("");
                }

                break;
            }
        }
    }

    void ClearNameDisplayWidget(Widget* widget) {
        ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);
        if (paramContext == NULL)
            return;

        for (auto displayWidget : displayWidgets) {
            ActionContext* nameContext = GetFirstContext(zoneManager, displayWidget, modifier);

            if (nameContext != NULL && nameContext->GetParamIndex() == paramContext->GetParamIndex() && nameContext->GetAction()->GetType() == ActionType::FixedTextDisplay) {
                nameContext->SetAction(zoneManager->GetCSI()->GetAction("NoAction"));
                nameContext->SetParamIndex(0);
                nameContext->SetStringParam("");

                break;
            }
        }
    }

    void ClearValueDisplayWidget(Widget* widget) {
        ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);
        if (paramContext == NULL)
            return;

        for (auto displayWidget : displayWidgets) {
            ActionContext* valueContext = GetFirstContext(zoneManager, displayWidget, modifier);

            if (valueContext != NULL && valueContext->GetParamIndex() == paramContext->GetParamIndex() && valueContext->GetAction()->GetType() == ActionType::FXParamValueDisplay) {
                valueContext->SetAction(zoneManager->GetCSI()->GetAction("NoAction"));
                valueContext->SetParamIndex(0);
                valueContext->SetStringParam("");

                break;
            }
        }
    }
};

struct SurfaceFXTemplate {
    ZoneManager* const zoneManager;
    Widget* currentWidget;
    int currentModifier;

    vector<unique_ptr<FXCell>> cells;
    vector<FXRowLayout> fxRowLayouts;
    vector<string> paramWidgets;
    vector<string> paramWidgetParams;
    vector<string> displayRows;
    vector<string> displayRowParams;
    vector<string> ringStyles;
    vector<string> fonts;
    bool hasColor;
    char paramWidget[SMLBUF];
    char nameWidget[SMLBUF];
    char valueWidget[SMLBUF];
    HWND hwnd;

    SurfaceFXTemplate(ZoneManager* const aZoneManager)
        : zoneManager(aZoneManager) {
        currentWidget = NULL;
        currentModifier = 0;
        hasColor = false;
        paramWidget[0] = 0;
        nameWidget[0] = 0;
        valueWidget[0] = 0;
        hwnd = NULL;
    }
};

static vector<unique_ptr<SurfaceFXTemplate>> s_surfaceFXTemplates;

SurfaceFXTemplate* GetSurfaceFXTemplate(HWND hwnd) {
    for (auto& surfaceFXTemplate : s_surfaceFXTemplates)
        if (surfaceFXTemplate->hwnd == hwnd)
            return surfaceFXTemplate.get();

    return NULL;
}

SurfaceFXTemplate* GetSurfaceFXTemplate(ZoneManager* zoneManager) {
    for (auto& surfaceFXTemplate : s_surfaceFXTemplates)
        if (surfaceFXTemplate->zoneManager == zoneManager)
            return surfaceFXTemplate.get();

    return NULL;
}

static FXCell* GetCell(SurfaceFXTemplate* t, Widget* widget, int modifier) {
    if (widget == NULL)
        return NULL;

    for (auto& cell : t->cells) {
        for (auto controlWidget : cell->controlWidgets) {
            if (controlWidget == widget && cell->modifier == modifier)
                return cell.get();
        }
    }

    return NULL;
}

static unsigned int s_buttonColors[][3] = {
    { IDC_FXParamRingColor, IDC_FXParamRingColorBox, 0xffffffff },
    { IDC_FXParamIndicatorColor, IDC_FXParamIndicatorColorBox, 0xffffffff },
    { IDC_FixedTextDisplayForegroundColor, IDC_FXFixedTextDisplayForegroundColorBox, 0xffffffff },
    { IDC_FixedTextDisplayBackgroundColor, IDC_FXFixedTextDisplayBackgroundColorBox, 0xffffffff },
    { IDC_FXParamDisplayForegroundColor, IDC_FXParamValueDisplayForegroundColorBox, 0xffffffff },
    { IDC_FXParamDisplayBackgroundColor, IDC_FXParamValueDisplayBackgroundColorBox, 0xffffffff },
};

static unsigned int& GetButtonColorForID(unsigned int id) {
    for (int x = 0; x < NUM_ELEM(s_buttonColors); ++x)
        if (s_buttonColors[x][0] == id)
            return s_buttonColors[x][2];
    WDL_ASSERT(false);
    return s_buttonColors[0][2];
}

static WDL_DLGRET dlgProcEditAdvanced(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SurfaceFXTemplate* t = GetSurfaceFXTemplate(s_hwndLearnFXDlg);

    if (!t)
        return 0;

    ZoneManager* zoneManager = t->zoneManager;
    Widget* widget = s_learnFX.currentWidget;

    char buf[MEDBUF];

    int modifier = 0;

    const vector<int>& modifiers = zoneManager->GetSurface()->GetModifiers();

    if (modifiers.size() > 0)
        modifier = modifiers[0];

    switch (uMsg) {
        case WM_INITDIALOG: {
            s_learnFX.editAdvancedContext = GetFirstContext(zoneManager, widget, modifier);

            if (s_learnFX.editAdvancedContext == NULL)
                break;

            char titleBuf[MEDBUF];
            titleBuf[0] = 0;

            char modifierBuf[SMLBUF];
            zoneManager->GetSurface()->GetModifierManager()->GetModifierString(modifier, modifierBuf, sizeof(modifierBuf));

            char paramName[MEDBUF];
            GetDlgItemText(s_hwndLearnFXDlg, IDC_FXParamNameEdit, paramName, sizeof(paramName)); //FIXME Assertion failed!

            snprintf(titleBuf, sizeof(titleBuf), "%s - %s%s - %s", widget->GetSurface()->GetName(), modifierBuf, widget->GetName(), paramName);

            SetWindowText(hwndDlg, titleBuf);

            snprintf(buf, sizeof(buf), "%0.2f", s_learnFX.editAdvancedContext->GetDeltaValue());
            SetDlgItemText(hwndDlg, IDC_EDIT_Delta, buf);

            snprintf(buf, sizeof(buf), "%0.2f", s_learnFX.editAdvancedContext->GetRangeMinimum());
            SetDlgItemText(hwndDlg, IDC_EDIT_RangeMin, buf);

            snprintf(buf, sizeof(buf), "%0.2f", s_learnFX.editAdvancedContext->GetRangeMaximum());
            SetDlgItemText(hwndDlg, IDC_EDIT_RangeMax, buf);

            char tmp[MEDBUF];
            const vector<double>& steppedValues = s_learnFX.editAdvancedContext->GetSteppedValues();
            string steps;

            for (int i = 0; i < steppedValues.size(); ++i) {
                steps += format_number(steppedValues[i], tmp, sizeof(tmp));
                steps += "  ";
            }
            SetDlgItemText(hwndDlg, IDC_EditSteps, steps.c_str());

            const vector<double>& acceleratedDeltaValues = s_learnFX.editAdvancedContext->GetAcceleratedDeltaValues();
            string deltas;

            for (int i = 0; i < (int) acceleratedDeltaValues.size(); ++i) {
                deltas += format_number(acceleratedDeltaValues[i], tmp, sizeof(tmp));
                deltas += " ";
            }
            SetDlgItemText(hwndDlg, IDC_EDIT_DeltaValues, deltas.c_str());

            const vector<int>& acceleratedTickCounts = s_learnFX.editAdvancedContext->GetAcceleratedTickCounts();
            string ticks = "";

            for (int i = 0; i < (int) acceleratedTickCounts.size(); ++i) {
                snprintf(buf, sizeof(buf), "%d ", acceleratedTickCounts[i]);
                ticks += buf;
            }
            SetDlgItemText(hwndDlg, IDC_EDIT_TickValues, ticks.c_str());

            // NEW: Set the Free Form text field from the ActionContext.
            SetDlgItemText(hwndDlg, IDC_EDIT_FREE_FORM, s_learnFX.editAdvancedContext->GetFreeFormText());

            OnDialogInit(hwndDlg);
        } break;

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        if (s_learnFX.editAdvancedContext == NULL) {
                            s_dlgResult = IDCANCEL;
                            EndDialog(hwndDlg, 0);
                            return 0;
                        }

                        GetDlgItemText(hwndDlg, IDC_EDIT_Delta, buf, sizeof(buf));
                        s_learnFX.editAdvancedContext->SetDeltaValue(atof(buf));

                        GetDlgItemText(hwndDlg, IDC_EDIT_RangeMin, buf, sizeof(buf));
                        s_learnFX.editAdvancedContext->SetRangeMinimum(atof(buf));

                        GetDlgItemText(hwndDlg, IDC_EDIT_RangeMax, buf, sizeof(buf));
                        s_learnFX.editAdvancedContext->SetRangeMaximum(atof(buf));

                        GetDlgItemText(hwndDlg, IDC_EDIT_DeltaValues, buf, sizeof(buf));
                        vector<string> tokens;
                        GetTokens(tokens, buf);
                        vector<double> deltas;
                        for (int i = 0; i < tokens.size(); ++i)
                            deltas.push_back(atof(tokens[i].c_str()));
                        s_learnFX.editAdvancedContext->SetAccelerationValues(deltas);

                        GetDlgItemText(hwndDlg, IDC_EDIT_TickValues, buf, sizeof(buf));
                        tokens.clear();
                        GetTokens(tokens, buf);
                        vector<int> ticks;
                        for (int i = 0; i < tokens.size(); ++i)
                            ticks.push_back(atoi(tokens[i].c_str()));
                        s_learnFX.editAdvancedContext->SetTickCounts(ticks);

                        GetDlgItemText(hwndDlg, IDC_EditSteps, buf, sizeof(buf));
                        tokens.clear();
                        GetTokens(tokens, buf);
                        vector<double> steps;
                        for (int i = 0; i < tokens.size(); ++i)
                            steps.push_back(atof(tokens[i].c_str()));
                        s_learnFX.editAdvancedContext->SetStepValues(steps);

                        // NEW: Retrieve Free Form Text and store it in the ActionContext.
                        GetDlgItemText(hwndDlg, IDC_EDIT_FREE_FORM, buf, sizeof(buf));
                        s_learnFX.editAdvancedContext->SetFreeFormText(buf);

                        s_dlgResult = IDOK;

                        EndDialog(hwndDlg, 0);
                    }
                    break;

                case IDCANCEL:
                    if (HIWORD(wParam) == BN_CLICKED)
                        EndDialog(hwndDlg, 0);
                    break;
            }
        } break;

        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            break;

        case WM_DESTROY:
            OnDialogDestroy(hwndDlg, 0);
            break;
    }

    return 0;
}

static void LoadTemplates(SurfaceFXTemplate* fxTemplate) {
    ZoneManager* zoneManager = fxTemplate->zoneManager;

    fxTemplate->fxRowLayouts.clear();
    fxTemplate->cells.clear();
    fxTemplate->paramWidget[0] = 0;
    fxTemplate->nameWidget[0] = 0;
    fxTemplate->valueWidget[0] = 0;
    fxTemplate->paramWidgets.clear();
    fxTemplate->displayRows.clear();
    fxTemplate->ringStyles.clear();
    fxTemplate->fonts.clear();
    fxTemplate->hasColor = false;

    map<const string, ZoneInfo>& zoneInfo = zoneManager->GetZoneInfo();

    if (zoneManager == NULL || zoneInfo.find("FXRowLayout") == zoneInfo.end() || zoneInfo.find("FXWidgetLayout") == zoneInfo.end())
        return;

    try {
        ifstream file(zoneInfo["FXRowLayout"].filePath);

        for (string line; getline(file, line);) {
            TrimLine(line);

            if (IsCommentedOrEmpty(line))
                continue;

            if (line.find("Zone") == string::npos) {
                vector<string> tokens;
                GetTokens(tokens, line.c_str());

                if (tokens.size() == 2) {
                    FXRowLayout t;

                    lstrcpyn_safe(t.suffix, tokens[1].c_str(), sizeof(t.suffix));
                    lstrcpyn_safe(t.modifiers, tokens[0].c_str(), sizeof(t.modifiers));
                    t.modifier = zoneManager->GetSurface()->GetModifierManager()->GetModifierValue(tokens[0].c_str());
                    fxTemplate->fxRowLayouts.push_back(t);
                }
            }
        }
    } catch (const std::exception& e) {
        LogToConsole("[ERROR] FAILED to LoadTemplates in %s\n", zoneInfo["FXRowLayout"].filePath.c_str());
        LogToConsole("Exception: %s\n", e.what());
    }

    try {
        ifstream file(zoneInfo["FXWidgetLayout"].filePath);

        for (string line; getline(file, line);) {
            TrimLine(line);

            if (IsCommentedOrEmpty(line))
                continue;

            if (line.find("Zone") == string::npos) {
                vector<string> tokens;
                GetTokens(tokens, line.c_str());

                if (line.find("Zone") == string::npos) {
                    if (tokens[0][0] == '#') {
                        if (tokens[0] == "#WidgetType" && tokens.size() > 1) {
                            fxTemplate->paramWidgets.push_back(tokens[1]);

                            if (tokens.size() > 2)
                                fxTemplate->paramWidgetParams.push_back(line.substr(line.find(tokens[2]), line.length() - 1).c_str());
                        } else if (tokens[0] == "#DisplayRow" && tokens.size() > 1) {
                            fxTemplate->displayRows.push_back(tokens[1]);

                            if (tokens.size() > 2)
                                fxTemplate->displayRowParams.push_back(line.substr(line.find(tokens[2]), line.length() - 1).c_str());
                        } else if (tokens[0] == "#RingStyle" && tokens.size() > 1)
                            fxTemplate->ringStyles.push_back(tokens[1]);
                        else if (tokens[0] == "#DisplayFont" && tokens.size() > 1)
                            fxTemplate->fonts.push_back(tokens[1]);
                        else if (tokens[0] == "#SupportsColor")
                            fxTemplate->hasColor = true;
                    } else {
                        if (tokens.size() > 1 && tokens[1] == "FXParam")
                            lstrcpyn_safe(fxTemplate->paramWidget, tokens[0].c_str(), sizeof(fxTemplate->paramWidget));
                        if (tokens.size() > 1 && tokens[1] == "FixedTextDisplay")
                            lstrcpyn_safe(fxTemplate->nameWidget, tokens[0].c_str(), sizeof(fxTemplate->nameWidget));
                        if (tokens.size() > 1 && tokens[1] == "FXParamValueDisplay")
                            lstrcpyn_safe(fxTemplate->valueWidget, tokens[0].c_str(), sizeof(fxTemplate->valueWidget));
                    }
                }
            }
        }

        if (fxTemplate->fonts.size() > 0 || fxTemplate->hasColor)
            s_learnFX.pageSurfaceFXLearnLevel = "Level3";
        else
            s_learnFX.pageSurfaceFXLearnLevel = "Level2";

    } catch (const std::exception& e) {
        LogToConsole("[ERROR] FAILED to LoadTemplates in %s\n", zoneInfo["FXWidgetLayout"].filePath.c_str());
        LogToConsole("Exception: %s\n", e.what());
    }
}

static void WriteBoilerPlate(FILE* fxFile, string& fxBoilerplatePath) {
    int lineNumber = 0;

    try {
        ifstream file(fxBoilerplatePath);

        for (string line; getline(file, line);) {
            TrimLine(line);

            lineNumber++;

            if (IsCommentedOrEmpty(line))
                continue;

            if (line.find("Zone") == 0)
                continue;

            fprintf(fxFile, "\t%s\n", line.c_str());
        }
    } catch (const std::exception& e) {
        LogToConsole("[ERROR] FAILED to WriteBoilerPlate in %s, around line %d\n", fxBoilerplatePath.c_str(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }
}

static void SaveZone(SurfaceFXTemplate* t) {
    if (s_learnFX.focusedTrack == NULL || s_learnFX.fxName[0] == 0 || t == NULL)
        return;

    ZoneManager* zoneManager = t->zoneManager;

    char buf[MEDBUF];
    buf[0] = 0;

    string editableFxZoneFolder;
    string preparationError;
    bool activatedUserProfile = false;
    if (!zoneManager->PrepareZonePathForWrite(zoneManager->GetFXZoneFolder(), editableFxZoneFolder, activatedUserProfile, preparationError)) {
        LogToConsole("[ERROR] Unable to prepare FX zone profile for writing: %s\n", preparationError.c_str());
        return;
    }

    char path[BUFSIZ];
    snprintf(path, sizeof(path), "%s/AutoGeneratedFXZones", editableFxZoneFolder.c_str());

    try {
        RecursiveCreateDirectory(path, 0);

        string trimmedFXName = s_learnFX.fxName;
        ReplaceAllWith(trimmedFXName, s_BadFileChars, "_");

        char filePath[BUFSIZ];

        snprintf(filePath, sizeof(filePath), "%s/%s.zon", path, trimmedFXName.c_str());

        FILE* fxFile = fopenUTF8(filePath, "wb");

        if (fxFile) {
            fprintf(fxFile, "Zone \"%s\" \"%s\"\n", s_learnFX.fxName, s_learnFX.fxAlias);

            map<const string, ZoneInfo>& zoneInfo = zoneManager->GetZoneInfo();

            if (zoneInfo.find("FXPrologue") != zoneInfo.end()) {
                ifstream file(zoneInfo["FXPrologue"].filePath);

                for (string line; getline(file, line);)
                    if (line.find("Zone") != 0)
                        fprintf(fxFile, "%s\n", line.c_str());
            }

            fprintf(fxFile, "\n%s\n\n", s_BeginAutoSection);

            int previousChannel = 1;

            for (auto& cell : t->cells) {
                char modifierBuf[SMLBUF];

                if (previousChannel > cell->channel) {
                    fprintf(fxFile, "\n\n");
                    previousChannel = 1;
                } else
                    previousChannel++;

                int modifier = cell->modifier;
                zoneManager->GetSurface()->GetModifierManager()->GetModifierString(modifier, modifierBuf, sizeof(modifierBuf));

                for (auto widget : cell->controlWidgets) {
                    fprintf(fxFile, "\t%s%s ", modifierBuf, widget->GetName());

                    if (ActionContext* context = GetFirstContext(zoneManager, widget, modifier)) {
                        char actionName[SMLBUF];
                        snprintf(actionName, sizeof(actionName), "%s", context->GetAction()->GetName());

                        fprintf(fxFile, "%s ", actionName);

                        if (!IsSameString(actionName, "NoAction")) {
                            fprintf(fxFile, "%d ", context->GetParamIndex());

                            context->GetWidgetProperties().save_list(fxFile);

                            fprintf(fxFile, "[ %0.2f>%0.2f ", context->GetRangeMinimum(), context->GetRangeMaximum());

                            fprintf(fxFile, "(");

                            char numBuf[MEDBUF];

                            if (context->GetAcceleratedDeltaValues().size() > 0) {
                                for (int i = 0; i < context->GetAcceleratedDeltaValues().size(); ++i) {
                                    format_number(context->GetAcceleratedDeltaValues()[i], numBuf, sizeof(numBuf));

                                    if (i < context->GetAcceleratedDeltaValues().size() - 1)
                                        fprintf(fxFile, "%s,", numBuf);
                                    else
                                        fprintf(fxFile, "%s", numBuf);
                                }
                            } else {
                                format_number(context->GetDeltaValue(), numBuf, sizeof(numBuf));
                                fprintf(fxFile, "%s", numBuf);
                            }

                            fprintf(fxFile, ") ");

                            fprintf(fxFile, "(");

                            if (context->GetAcceleratedTickCounts().size() > 0) {
                                for (int i = 0; i < context->GetAcceleratedTickCounts().size(); ++i) {
                                    if (i < context->GetAcceleratedTickCounts().size() - 1)
                                        fprintf(fxFile, "%d,", context->GetAcceleratedTickCounts()[i]);
                                    else
                                        fprintf(fxFile, "%d", context->GetAcceleratedTickCounts()[i]);
                                }
                            }

                            fprintf(fxFile, ") ");

                            if (context->GetSteppedValues().size() > 0) {
                                for (int i = 0; i < context->GetSteppedValues().size(); ++i)
                                    fprintf(fxFile, "%0.2f ", context->GetSteppedValues()[i]);
                            }

                            fprintf(fxFile, " ]");

                            // ***** NEW: Append free-form text for this assignment *****
                            {
                                const char* freeText = context->GetFreeFormText();
                                if (freeText && freeText[0] != '\0') {
                                    fprintf(fxFile, " %s", freeText);
                                }
                            }
                        }
                    }

                    fprintf(fxFile, "\n");
                }

                for (auto displayWidget : cell->displayWidgets) {
                    Widget* widget = displayWidget;

                    if (IsSameString(zoneManager->GetSurface()->GetName(), "SCE24")) {
                        if (strstr(widget->GetName(), t->paramWidget) || strstr(widget->GetName(), t->nameWidget) || strstr(widget->GetName(), t->valueWidget))
                            fprintf(fxFile, "\t%s%s ", modifierBuf, widget->GetName());
                    } else
                        fprintf(fxFile, "\t%s%s ", modifierBuf, widget->GetName());

                    if (ActionContext* context = GetFirstContext(zoneManager, widget, modifier)) {
                        char actionName[SMLBUF];
                        snprintf(actionName, sizeof(actionName), "%s", context->GetAction()->GetName());

                        if (IsSameString(zoneManager->GetSurface()->GetName(), "SCE24")) {
                            if (strstr(widget->GetName(), t->paramWidget) || strstr(widget->GetName(), t->nameWidget) || strstr(widget->GetName(), t->valueWidget))
                                fprintf(fxFile, "%s ", actionName);
                        } else
                            fprintf(fxFile, "%s ", actionName);

                        if (!IsSameString(actionName, "NoAction")) {
                            if (IsSameString(actionName, "FixedTextDisplay"))
                                fprintf(fxFile, "\"%s\" %d ", context->GetStringParam(), context->GetParamIndex());
                            else if (IsSameString(actionName, "FXParamValueDisplay"))
                                fprintf(fxFile, "%d ", context->GetParamIndex());

                            context->GetWidgetProperties().save_list(fxFile);
                        }

                        if (IsSameString(zoneManager->GetSurface()->GetName(), "SCE24")) {
                            if (strstr(widget->GetName(), t->paramWidget) || strstr(widget->GetName(), t->nameWidget) || strstr(widget->GetName(), t->valueWidget))
                                fprintf(fxFile, "\n");
                        } else
                            fprintf(fxFile, "\n");
                    }
                }

                fprintf(fxFile, "\n\n");
            }

            fprintf(fxFile, "\n%s\n\n", s_EndAutoSection);

            if (zoneInfo.find("FXEpilogue") != zoneInfo.end()) {
                ifstream file(zoneInfo["FXEpilogue"].filePath);

                for (string line; getline(file, line);)
                    if (line.find("Zone") != 0)
                        fprintf(fxFile, "%s\n", line.c_str());
            }

            fprintf(fxFile, "%s\n", "ZoneEnd");

            fclose(fxFile);
        }

        ZoneInfo info;
        info.filePath = filePath;
        info.alias = s_learnFX.fxAlias;

        zoneManager->AddZoneFilePath(s_learnFX.fxName, info);
    } catch (const std::exception& e) {
        LogToConsole("[ERROR] FAILED to SaveZone %s\n", path);
        LogToConsole("Exception: %s\n", e.what());
    }
}

static void ClearParams(HWND hwndDlg) {
    s_learnFX.isUpdatingParameters = true;

    SetDlgItemText(hwndDlg, IDC_PickRingStyle, "");
    SetDlgItemText(hwndDlg, IDC_PickSteps, "");
    SetWindowText(GetDlgItem(hwndDlg, IDC_FXParamNameEdit), "");
    SetDlgItemText(hwndDlg, IDC_COMBO_PickNameDisplay, "");
    SetDlgItemText(hwndDlg, IDC_COMBO_PickValueDisplay, "");
    SetDlgItemText(hwndDlg, IDC_FixedTextDisplayPickFont, "");
    SetWindowText(GetDlgItem(hwndDlg, IDC_Edit_FixedTextDisplayTop), "");
    SetWindowText(GetDlgItem(hwndDlg, IDC_Edit_FixedTextDisplayBottom), "");

    SetDlgItemText(hwndDlg, IDC_FXParamValueDisplayPickFont, "");
    SetWindowText(GetDlgItem(hwndDlg, IDC_Edit_ParamValueDisplayTop), "");
    SetWindowText(GetDlgItem(hwndDlg, IDC_Edit_ParamValueDisplayBottom), "");

    for (int i = 0; i < NUM_ELEM(s_buttonColors); ++i)
        s_buttonColors[i][2] = 0xedededff;

    RECT rect;
    GetClientRect(hwndDlg, &rect);
    InvalidateRect(hwndDlg, &rect, 0);

    s_learnFX.isUpdatingParameters = false;
}

static void GetFullWidgetName(Widget* widget, int modifier, char* widgetNamBuf, int bufSize) {
    if (widget == NULL)
        return;

    char modifierBuf[SMLBUF];
    widget->GetSurface()->GetModifierManager()->GetModifierString(modifier, modifierBuf, sizeof(modifierBuf));
    snprintf(widgetNamBuf, bufSize, "%s%s", modifierBuf, widget->GetName());
}

static void FillPropertiesParams(HWND hwndDlg, SurfaceFXTemplate* t, Widget* widget, int modifier) {
    FXCell* cell = GetCell(t, widget, modifier);

    if (cell == NULL)
        return;

    if (cell->displayWidgets.size() < 2)
        return;

    ActionContext* paramContext = GetFirstContext(t->zoneManager, widget, modifier);
    ActionContext* nameContext = GetFirstContext(t->zoneManager, cell->GetNameWidget(widget), modifier);
    ActionContext* valueContext = GetFirstContext(t->zoneManager, cell->GetValueWidget(widget), modifier);

    if (paramContext == NULL)
        return;

    s_learnFX.isUpdatingParameters = true;

    char buf[MEDBUF];
    buf[0] = 0;

    rgba_color defaultColor;
    defaultColor.r = 237;
    defaultColor.g = 237;
    defaultColor.b = 237;

    const char* ringColor = paramContext->GetWidgetProperties().get_prop(PropertyType_LEDRingColor);
    if (ringColor) {
        rgba_color color;
        GetColorValue(ringColor, color);
        GetButtonColorForID(IDC_FXParamRingColor) = ColorToNative(color.r, color.g, color.b);
    } else
        GetButtonColorForID(IDC_FXParamRingColor) = ColorToNative(defaultColor.r, defaultColor.g, defaultColor.b);

    const char* pushColor = paramContext->GetWidgetProperties().get_prop(PropertyType_PushColor);
    if (pushColor) {
        rgba_color color;
        GetColorValue(pushColor, color);
        GetButtonColorForID(IDC_FXParamIndicatorColor) = ColorToNative(color.r, color.g, color.b);
    } else
        GetButtonColorForID(IDC_FXParamIndicatorColor) = ColorToNative(defaultColor.r, defaultColor.g, defaultColor.b);

    const char* property;
    const char* foreground;
    const char* background;

    if (nameContext) {
        property = nameContext->GetWidgetProperties().get_prop(PropertyType_Font);
        if (property)
            SetDlgItemText(hwndDlg, IDC_FixedTextDisplayPickFont, property);
        else
            SetDlgItemText(hwndDlg, IDC_FixedTextDisplayPickFont, "");

        property = nameContext->GetWidgetProperties().get_prop(PropertyType_TopMargin);
        if (property)
            SetDlgItemText(hwndDlg, IDC_Edit_FixedTextDisplayTop, property);
        else
            SetDlgItemText(hwndDlg, IDC_Edit_FixedTextDisplayTop, "");

        property = nameContext->GetWidgetProperties().get_prop(PropertyType_BottomMargin);
        if (property)
            SetDlgItemText(hwndDlg, IDC_Edit_FixedTextDisplayBottom, property);
        else
            SetDlgItemText(hwndDlg, IDC_Edit_FixedTextDisplayBottom, "");

        foreground = nameContext->GetWidgetProperties().get_prop(PropertyType_TextColor);
        if (foreground) {
            rgba_color color;
            GetColorValue(foreground, color);
            GetButtonColorForID(IDC_FixedTextDisplayForegroundColor) = ColorToNative(color.r, color.g, color.b);
        } else
            GetButtonColorForID(IDC_FixedTextDisplayForegroundColor) = ColorToNative(defaultColor.r, defaultColor.g, defaultColor.b);

        background = nameContext->GetWidgetProperties().get_prop(PropertyType_BackgroundColor);
        if (background) {
            rgba_color color;
            GetColorValue(background, color);
            GetButtonColorForID(IDC_FixedTextDisplayBackgroundColor) = ColorToNative(color.r, color.g, color.b);
        } else
            GetButtonColorForID(IDC_FixedTextDisplayBackgroundColor) = ColorToNative(defaultColor.r, defaultColor.g, defaultColor.b);
    }

    if (valueContext) {
        property = valueContext->GetWidgetProperties().get_prop(PropertyType_Font);
        if (property)
            SetDlgItemText(hwndDlg, IDC_FXParamValueDisplayPickFont, property);
        else
            SetDlgItemText(hwndDlg, IDC_FXParamValueDisplayPickFont, "");

        property = valueContext->GetWidgetProperties().get_prop(PropertyType_TopMargin);
        if (property)
            SetDlgItemText(hwndDlg, IDC_Edit_ParamValueDisplayTop, property);
        else
            SetDlgItemText(hwndDlg, IDC_Edit_ParamValueDisplayTop, "");

        property = valueContext->GetWidgetProperties().get_prop(PropertyType_BottomMargin);
        if (property)
            SetDlgItemText(hwndDlg, IDC_Edit_ParamValueDisplayBottom, property);
        else
            SetDlgItemText(hwndDlg, IDC_Edit_ParamValueDisplayBottom, "");

        foreground = valueContext->GetWidgetProperties().get_prop(PropertyType_TextColor);
        if (foreground) {
            rgba_color color;
            GetColorValue(foreground, color);
            GetButtonColorForID(IDC_FXParamDisplayForegroundColor) = ColorToNative(color.r, color.g, color.b);
        } else
            GetButtonColorForID(IDC_FXParamDisplayForegroundColor) = ColorToNative(defaultColor.r, defaultColor.g, defaultColor.b);

        background = valueContext->GetWidgetProperties().get_prop(PropertyType_BackgroundColor);
        if (background) {
            rgba_color color;
            GetColorValue(background, color);
            GetButtonColorForID(IDC_FXParamDisplayBackgroundColor) = ColorToNative(color.r, color.g, color.b);
        } else
            GetButtonColorForID(IDC_FXParamDisplayBackgroundColor) = ColorToNative(defaultColor.r, defaultColor.g, defaultColor.b);
    }

    RECT rect;
    GetClientRect(hwndDlg, &rect);
    InvalidateRect(hwndDlg, &rect, 0);

    s_learnFX.isUpdatingParameters = false;
}

static void FillAdvancedParams(HWND hwndDlg, SurfaceFXTemplate* t, Widget* widget, int modifier) {
    FXCell* cell = GetCell(t, widget, modifier);

    if (cell == NULL)
        return;

    if (cell->displayWidgets.size() < 2)
        return;

    ActionContext* paramContext = GetFirstContext(t->zoneManager, widget, modifier);

    if (paramContext == NULL)
        return;

    char buf[MEDBUF];
    buf[0] = 0;

    const char* ringstyle = paramContext->GetWidgetProperties().get_prop(PropertyType_RingStyle);
    if (ringstyle)
        SetDlgItemText(hwndDlg, IDC_PickRingStyle, ringstyle);
    else
        SendMessage(GetDlgItem(hwndDlg, IDC_PickRingStyle), CB_SETCURSEL, 0, 0);

    int numSteps = paramContext->GetNumberOfSteppedValues();
    if (numSteps) {
        snprintf(buf, sizeof(buf), "%d", numSteps);
        SetDlgItemText(hwndDlg, IDC_PickSteps, buf);
    } else
        SetDlgItemText(hwndDlg, IDC_PickSteps, "");

    if (ActionContext* nameContext = cell->GetNameContext(widget))
        SetWindowText(GetDlgItem(hwndDlg, IDC_FXParamNameEdit), nameContext->GetStringParam());
    else
        SetWindowText(GetDlgItem(hwndDlg, IDC_FXParamNameEdit), "");

    if (FXCell* cell = GetCell(t, widget, modifier)) {
        SendDlgItemMessage(hwndDlg, IDC_COMBO_PickNameDisplay, CB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(hwndDlg, IDC_COMBO_PickNameDisplay, CB_ADDSTRING, 0, (LPARAM) "");
        SendDlgItemMessage(hwndDlg, IDC_COMBO_PickValueDisplay, CB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(hwndDlg, IDC_COMBO_PickValueDisplay, CB_ADDSTRING, 0, (LPARAM) "");

        for (auto displayWidget : cell->displayWidgets) {
            SendDlgItemMessage(hwndDlg, IDC_COMBO_PickNameDisplay, CB_ADDSTRING, 0, (LPARAM) displayWidget->GetName());
            SendDlgItemMessage(hwndDlg, IDC_COMBO_PickValueDisplay, CB_ADDSTRING, 0, (LPARAM) displayWidget->GetName());
        }

        if (cell->GetNameWidget(widget)) {
            int index = (int) SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PickNameDisplay), CB_FINDSTRINGEXACT, -1, (LPARAM) cell->GetNameWidget(widget)->GetName());
            if (index >= 0)
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PickNameDisplay), CB_SETCURSEL, index, 0);
            else
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PickNameDisplay), CB_SETCURSEL, 0, 0);
        } else
            SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PickNameDisplay), CB_SETCURSEL, 0, 0);

        if (cell->GetValueWidget(widget)) {
            string nm = cell->GetValueWidget(widget)->GetName();

            int index = (int) SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PickValueDisplay), CB_FINDSTRINGEXACT, -1, (LPARAM) cell->GetValueWidget(widget)->GetName());
            if (index >= 0)
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PickValueDisplay), CB_SETCURSEL, index, 0);
            else
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PickValueDisplay), CB_SETCURSEL, 0, 0);
        } else
            SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PickValueDisplay), CB_SETCURSEL, 0, 0);
    }

    FillPropertiesParams(hwndDlg, t, widget, modifier);

    paramContext->GetWidget()->Configure(t->zoneManager->GetLearnedFocusedFXZone()->GetActionContexts(widget));
}

static void FillParams(HWND hwndDlg, SurfaceFXTemplate* t, Widget* widget, int modifier) {
    SendDlgItemMessage(hwndDlg, IDC_PickRingStyle, CB_RESETCONTENT, 0, 0);

    for (int i = 0; i < t->ringStyles.size(); ++i)
        SendDlgItemMessage(hwndDlg, IDC_PickRingStyle, CB_ADDSTRING, 0, (LPARAM) t->ringStyles[i].c_str());

    SendDlgItemMessage(hwndDlg, IDC_PickSteps, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(hwndDlg, IDC_PickSteps, CB_ADDSTRING, 0, (LPARAM) "0");

    for (int step = g_minNumParamSteps; step <= g_maxNumParamSteps; ++step) {
        char buf[SMLBUF];
        snprintf(buf, sizeof(buf), "%d", step);
        SendDlgItemMessage(hwndDlg, IDC_PickSteps, CB_ADDSTRING, 0, (LPARAM) buf);
    }

    SendDlgItemMessage(hwndDlg, IDC_FixedTextDisplayPickFont, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(hwndDlg, IDC_FXParamValueDisplayPickFont, CB_RESETCONTENT, 0, 0);

    for (int i = 0; i < t->fonts.size(); ++i) {
        SendDlgItemMessage(hwndDlg, IDC_FixedTextDisplayPickFont, CB_ADDSTRING, 0, (LPARAM) t->fonts[i].c_str());
        SendDlgItemMessage(hwndDlg, IDC_FXParamValueDisplayPickFont, CB_ADDSTRING, 0, (LPARAM) t->fonts[i].c_str());
    }

    if (widget == NULL)
        return;

    char buf[MEDBUF];

    FXCell* cell = GetCell(t, widget, modifier);

    if (cell == NULL)
        return;

    if (cell->displayWidgets.size() < 2)
        return;

    ActionContext* paramContext = GetFirstContext(t->zoneManager, widget, modifier);

    char modifierBuf[SMLBUF];
    t->zoneManager->GetSurface()->GetModifierManager()->GetModifierString(modifier, modifierBuf, sizeof(modifierBuf));

    buf[0] = 0;

    if (paramContext == NULL) {
        ClearParams(hwndDlg);
        return;
    }

    if (paramContext->GetAction()->GetType() == ActionType::NoAction)
        ClearParams(hwndDlg);
    else {
        TrackFX_GetParamName(s_learnFX.focusedTrack, s_learnFX.fxSlot, s_learnFX.lastTouchedParamNum, buf, sizeof(buf));
        SetDlgItemText(hwndDlg, IDC_FXParamNameEdit, buf);
        FillAdvancedParams(hwndDlg, t, widget, modifier);
    }
}

static void SetWidgetProperties(ActionContext* context, const char* params) {
    context->GetWidgetProperties().delete_props();

    vector<string> tokens;
    GetTokens(tokens, params);

    for (int i = 0; i < tokens.size(); ++i) {
        vector<string> kvps;
        GetTokens(kvps, tokens[i], '=');

        if (kvps.size() == 2)
            context->GetWidgetProperties().set_prop(PropertyList::prop_from_string(kvps[0].c_str()), kvps[1].c_str());
    }
}

static void HandleAssigment(SurfaceFXTemplate* t, Widget* widget, int modifier, int paramIdx, bool shouldAssign) {
    if (!t)
        return;

    ZoneManager* zoneManager = t->zoneManager;

    if (zoneManager == NULL)
        return;

    if (paramIdx < 0)
        return;

    if (s_learnFX.fxSlot < 0)
        return;

    FXCell* cell = GetCell(t, widget, modifier);

    if (cell == NULL)
        return;

    char buf[MEDBUF];
    buf[0] = 0;

    ActionContext* paramContext = GetFirstContext(zoneManager, widget, modifier);

    if (paramContext == NULL)
        return;

    if (!shouldAssign) {
        if (ActionContext* nameContext = cell->GetNameContext(widget)) {
            nameContext->SetAction(zoneManager->GetCSI()->GetAction("NoAction"));
            nameContext->SetParamIndex(0);
            nameContext->SetStringParam("");
            nameContext->GetWidgetProperties().delete_props();
        }

        if (ActionContext* valueContext = cell->GetValueContext(widget)) {
            valueContext->SetAction(zoneManager->GetCSI()->GetAction("NoAction"));
            valueContext->SetParamIndex(0);
            valueContext->SetStringParam("");
            valueContext->GetWidgetProperties().delete_props();
        }

        paramContext->SetAction(zoneManager->GetCSI()->GetAction("NoAction"));
        paramContext->SetParamIndex(0);
        paramContext->SetStringParam("");
        paramContext->GetWidgetProperties().delete_props();

        SetDlgItemText(t->hwnd, IDC_AssignFXParamDisplay, "");
        ShowWindow(GetDlgItem(t->hwnd, IDC_Unassign), false);
        ShowWindow(GetDlgItem(t->hwnd, IDC_Assign), true);
        EnableWindow(GetDlgItem(t->hwnd, IDC_Assign), false);
        EnableWindow(GetDlgItem(t->hwnd, IDC_DeepEdit), false);
    } else if (paramContext->GetAction()->GetType() != ActionType::FXParam && paramContext->GetAction()->GetType() != ActionType::JSFXParam) {
        paramContext->SetAction(zoneManager->GetCSI()->GetFXParamAction(s_learnFX.fxName));
        paramContext->SetParamIndex(paramIdx);
        paramContext->SetStringParam("");

        char suffix[SMLBUF];
        snprintf(suffix, sizeof(suffix), "%s%d", cell->suffix.c_str(), widget->GetChannelNumber());
        char rawWidgetName[SMLBUF];
        snprintf(rawWidgetName, strlen(widget->GetName()) - strlen(suffix) + 1, "%s", widget->GetName());

        for (int i = 0; i < t->paramWidgetParams.size() && i < t->paramWidgets.size(); ++i) {
            if (t->paramWidgets[i] == rawWidgetName) {
                SetWidgetProperties(paramContext, t->paramWidgetParams[i].c_str());
                break;
            }
        }

        TrackFX_GetParamName(s_learnFX.focusedTrack, s_learnFX.fxSlot, paramIdx, buf, sizeof(buf));

        char fullWidgetName[MEDBUF];
        snprintf(fullWidgetName, sizeof(fullWidgetName), "%s%s%d", t->nameWidget, cell->suffix.c_str(), cell->channel);
        cell->SetNameWidget(widget, fullWidgetName, buf);

        snprintf(fullWidgetName, sizeof(fullWidgetName), "%s%s%d", t->valueWidget, cell->suffix.c_str(), cell->channel);
        cell->SetValueWidget(widget, fullWidgetName);

        if (ActionContext* context = cell->GetNameContext(widget)) {
            if (Widget* nameWidget = cell->GetNameWidget(widget)) {
                snprintf(rawWidgetName, strlen(nameWidget->GetName()) - strlen(suffix) + 1, "%s", nameWidget->GetName());

                for (int i = 0; i < t->displayRowParams.size() && i < t->displayRows.size(); ++i) {
                    if (t->displayRows[i] == rawWidgetName) {
                        SetWidgetProperties(context, t->displayRowParams[i].c_str());
                        context->ForceWidgetValue(context->GetStringParam());
                        break;
                    }
                }
            }
        }

        if (ActionContext* context = cell->GetValueContext(widget)) {
            if (Widget* valueWidget = cell->GetValueWidget(widget)) {
                snprintf(rawWidgetName, strlen(valueWidget->GetName()) - strlen(suffix) + 1, "%s", valueWidget->GetName());

                for (int i = 0; i < t->displayRowParams.size() && i < t->displayRows.size(); ++i) {
                    if (t->displayRows[i] == rawWidgetName) {
                        SetWidgetProperties(context, t->displayRowParams[i].c_str());
                        break;
                    }
                }
            }
        }

        vector<double> steps;

        if (widget->GetIsTwoState()) {
            steps.push_back(0.0);
            steps.push_back(1.0);
            paramContext->SetStepValues(steps);
        }

        EnableWindow(GetDlgItem(t->hwnd, IDC_DeepEdit), true);
        ShowWindow(GetDlgItem(t->hwnd, IDC_Assign), false);
        ShowWindow(GetDlgItem(t->hwnd, IDC_Unassign), true);
        EnableWindow(GetDlgItem(t->hwnd, IDC_Unassign), true);
    }
}

static void ApplyColorsToAll(SurfaceFXTemplate* t, HWND hwndDlg, Widget* widget, int modifier, ActionContext* sourceParamContext, ActionContext* sourceNameContext, ActionContext* sourceValueContext, ZoneManager* zoneManager) {
    for (auto& cell : t->cells) {
        for (auto controlWidget : cell->controlWidgets) {
            if (sourceParamContext != NULL) {
                if (ActionContext* context = GetFirstContext(zoneManager, controlWidget, modifier)) {
                    if (const char* sourceRingColor = sourceParamContext->GetWidgetProperties().get_prop(PropertyType_LEDRingColor))
                        if (context->GetWidgetProperties().get_prop(PropertyType_LEDRingColor))
                            context->GetWidgetProperties().set_prop(PropertyType_LEDRingColor, sourceRingColor);

                    if (const char* sourcePushColor = sourceParamContext->GetWidgetProperties().get_prop(PropertyType_PushColor))
                        if (context->GetWidgetProperties().get_prop(PropertyType_PushColor))
                            context->GetWidgetProperties().set_prop(PropertyType_PushColor, sourcePushColor);

                    context->GetWidget()->Configure(zoneManager->GetLearnedFocusedFXZone()->GetActionContexts(widget));
                }
            }

            if (sourceNameContext != NULL) {
                if (ActionContext* context = cell->GetNameContext(controlWidget)) {
                    if (const char* sourceTextColor = sourceNameContext->GetWidgetProperties().get_prop(PropertyType_TextColor))
                        if (context->GetWidgetProperties().get_prop(PropertyType_TextColor))
                            context->GetWidgetProperties().set_prop(PropertyType_TextColor, sourceTextColor);

                    if (const char* sourceBackgroundColor = sourceNameContext->GetWidgetProperties().get_prop(PropertyType_BackgroundColor))
                        if (context->GetWidgetProperties().get_prop(PropertyType_BackgroundColor))
                            context->GetWidgetProperties().set_prop(PropertyType_BackgroundColor, sourceBackgroundColor);

                    context->ForceWidgetValue(context->GetStringParam());
                }
            }

            if (sourceValueContext != NULL) {
                if (ActionContext* context = cell->GetValueContext(controlWidget)) {
                    if (const char* sourceTextColor = sourceValueContext->GetWidgetProperties().get_prop(PropertyType_TextColor))
                        if (const char* textColor = context->GetWidgetProperties().get_prop(PropertyType_TextColor))
                            context->GetWidgetProperties().set_prop(PropertyType_TextColor, sourceTextColor);

                    if (const char* sourceBackgroundColor = sourceValueContext->GetWidgetProperties().get_prop(PropertyType_BackgroundColor))
                        if (const char* backgroundColor = context->GetWidgetProperties().get_prop(PropertyType_BackgroundColor))
                            context->GetWidgetProperties().set_prop(PropertyType_BackgroundColor, sourceBackgroundColor);

                    context->ForceWidgetValue(context->GetStringParam());
                }
            }
        }
    }
}

static void ApplyFontsAndMarginsToAll(SurfaceFXTemplate* t, HWND hwndDlg, Widget* widget, int modifier, ActionContext* sourceParamContext, ActionContext* sourceNameContext, ActionContext* sourceValueContext) {
    for (auto& cell : t->cells) {
        for (auto controlWidget : cell->controlWidgets) {
            if (sourceNameContext != NULL) {
                if (ActionContext* context = cell->GetNameContext(controlWidget)) {
                    if (const char* sourceTopMargin = sourceNameContext->GetWidgetProperties().get_prop(PropertyType_TopMargin))
                        if (context->GetWidgetProperties().get_prop(PropertyType_TopMargin))
                            context->GetWidgetProperties().set_prop(PropertyType_TopMargin, sourceTopMargin);

                    if (const char* sourceBottomMargin = sourceNameContext->GetWidgetProperties().get_prop(PropertyType_BottomMargin))
                        if (context->GetWidgetProperties().get_prop(PropertyType_BottomMargin))
                            context->GetWidgetProperties().set_prop(PropertyType_BottomMargin, sourceBottomMargin);

                    if (const char* sourceFont = sourceNameContext->GetWidgetProperties().get_prop(PropertyType_Font))
                        if (context->GetWidgetProperties().get_prop(PropertyType_Font))
                            context->GetWidgetProperties().set_prop(PropertyType_Font, sourceFont);

                    context->ForceWidgetValue(context->GetStringParam());
                }
            }

            if (sourceValueContext != NULL) {
                if (ActionContext* context = cell->GetValueContext(controlWidget)) {
                    if (const char* sourceTopMargin = sourceValueContext->GetWidgetProperties().get_prop(PropertyType_TopMargin))
                        if (context->GetWidgetProperties().get_prop(PropertyType_TopMargin))
                            context->GetWidgetProperties().set_prop(PropertyType_TopMargin, sourceTopMargin);

                    if (const char* sourceBottomMargin = sourceValueContext->GetWidgetProperties().get_prop(PropertyType_BottomMargin))
                        if (context->GetWidgetProperties().get_prop(PropertyType_BottomMargin))
                            context->GetWidgetProperties().set_prop(PropertyType_BottomMargin, sourceBottomMargin);

                    if (const char* sourceFont = sourceValueContext->GetWidgetProperties().get_prop(PropertyType_Font))
                        if (context->GetWidgetProperties().get_prop(PropertyType_Font))
                            context->GetWidgetProperties().set_prop(PropertyType_Font, sourceFont);

                    context->ForceWidgetValue(context->GetStringParam());
                }
            }
        }
    }
}

static WDL_DLGRET dlgProcEditFXAlias(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            s_dlgResult = IDCANCEL;
            SetDlgItemText(hwndDlg, IDC_EDIT_FXAlias, s_learnFX.fxAlias);
            OnDialogInit(hwndDlg);
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        GetDlgItemText(hwndDlg, IDC_EDIT_FXAlias, s_learnFX.fxAlias, sizeof(s_learnFX.fxAlias));
                        s_dlgResult = IDOK;
                        EndDialog(hwndDlg, 0);
                    }
                    break;

                case IDCANCEL:
                    if (HIWORD(wParam) == BN_CLICKED)
                        EndDialog(hwndDlg, 0);
            }
            break;

        case WM_DESTROY:
            OnDialogDestroy(hwndDlg, 0);
            break;
    }

    return 0;
}

static Widget* FindWidget(ZoneManager* zoneManager, const string& baseName, const string& suffix, int channel) {
    char widgetName[SMLBUF];

    snprintf(widgetName, sizeof(widgetName), "%s%s%d", baseName.c_str(), suffix.c_str(), channel);
    Widget* widget = zoneManager->GetSurface()->GetWidgetByName(widgetName);

    if (!widget && channel == 1) {
        snprintf(widgetName, sizeof(widgetName), "%s%s", baseName.c_str(), suffix.c_str());
        widget = zoneManager->GetSurface()->GetWidgetByName(widgetName);
    }

    return widget;
}

static void CreateContextMap(SurfaceFXTemplate* t) {
    if (!t)
        return;

    ZoneManager* zoneManager = t->zoneManager;

    t->cells.clear();

    for (int rowLayoutIdx = 0; rowLayoutIdx < t->fxRowLayouts.size(); ++rowLayoutIdx) {
        int modifier = t->fxRowLayouts[rowLayoutIdx].modifier;

        const int channelsNum = zoneManager->GetSurface()->GetNumChannels();
        for (int channel = 1; channel <= channelsNum; ++channel) {
            t->cells.push_back(make_unique<FXCell>(zoneManager, t->fxRowLayouts[rowLayoutIdx].suffix, modifier, channel));

            FXCell* cell = t->cells.back().get();

            for (int widgetTypesIdx = 0; widgetTypesIdx < t->paramWidgets.size(); ++widgetTypesIdx) {
                Widget* widget = FindWidget(zoneManager, t->paramWidgets[widgetTypesIdx], t->fxRowLayouts[rowLayoutIdx].suffix, channel);
                if (widget)
                    cell->controlWidgets.push_back(widget);
            }

            for (int widgetTypesIdx = 0; widgetTypesIdx < t->displayRows.size(); ++widgetTypesIdx) {
                Widget* widget = FindWidget(zoneManager, t->displayRows[widgetTypesIdx], t->fxRowLayouts[rowLayoutIdx].suffix, channel);
                if (widget)
                    cell->displayWidgets.push_back(widget);
            }
        }
    }
}

static void ReleaseFX() {
    s_learnFX.focusedTrack = nullptr;
    s_learnFX.fxSlot = -1;
    s_learnFX.lastTouchedParamNum = -1;
    s_learnFX.lastTouchedParamValue = -1.0;
}

static HFONT hFont16 = NULL;
static HFONT hFont14 = NULL;

static WDL_DLGRET dlgProcLearnFXDeepEdit(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SurfaceFXTemplate* t = GetSurfaceFXTemplate(s_hwndLearnFXDlg);

    ZoneManager* zoneManager = NULL;

    if (t)
        zoneManager = t->zoneManager;

    Widget* widget = s_learnFX.currentWidget;

    char buf[MEDBUF];

    rgba_color color;
    char colorBuf[32];

    switch (uMsg) {
        case WM_INITDIALOG: {
            hFont16 = CreateFont(16, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");

            if (hFont16)
                SendMessage(GetDlgItem(hwndDlg, IDC_SurfaceName), WM_SETFONT, (WPARAM) hFont16, 0);

            hFont14 = CreateFont(14, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");

            if (hFont14)
                SendMessage(GetDlgItem(hwndDlg, IDC_FXParamNameEdit), WM_SETFONT, (WPARAM) hFont14, 0);

            SetWindowText(hwndDlg, s_learnFX.fxAlias);
            SetDlgItemText(hwndDlg, IDC_SurfaceName, t->zoneManager->GetSurface()->GetName());

            FillParams(hwndDlg, t, s_learnFX.currentWidget, s_learnFX.currentModifier);
            OnDialogInit(hwndDlg);
        } break;

        case WM_CLOSE: {
            if (hFont16)
                DeleteObject(hFont16);

            if (hFont14)
                DeleteObject(hFont14);

            DestroyWindow(hwndDlg);
        } break;

        case WM_DESTROY:
            OnDialogDestroy(hwndDlg, 0);
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwndDlg, &ps);

            for (int x = 0; x < NUM_ELEM(s_buttonColors); ++x) {
                const int colorPickerBox = s_buttonColors[x][1];
                const int colorValue = s_buttonColors[x][2];

                HBRUSH brush = CreateSolidBrush(colorValue);

                RECT clientRect, windowRect;
                POINT p;
                GetClientRect(GetDlgItem(hwndDlg, colorPickerBox), &clientRect);
                GetWindowRect(GetDlgItem(hwndDlg, colorPickerBox), &windowRect);
                p.x = windowRect.left;
                p.y = windowRect.top;
                ScreenToClient(hwndDlg, &p);

                windowRect.left = p.x;
                windowRect.right = windowRect.left + clientRect.right;
                windowRect.top = p.y;
                windowRect.bottom = windowRect.top + clientRect.bottom;

                FillRect(hdc, &windowRect, brush);
                DeleteObject(brush);
            }

            EndPaint(hwndDlg, &ps);
        } break;

        case WM_COMMAND: {
            ActionContext* paramContext = NULL;
            ActionContext* nameContext = NULL;
            ActionContext* valueContext = NULL;

            int modifier = 0;

            if (zoneManager) {
                const vector<int>& modifiers = zoneManager->GetSurface()->GetModifiers();

                if (modifiers.size() > 0)
                    modifier = modifiers[0];

                paramContext = GetFirstContext(zoneManager, widget, modifier);
            }

            FXCell* cell = NULL;

            if (t) {
                cell = GetCell(t, widget, modifier);

                if (cell) {
                    nameContext = cell->GetNameContext(widget);
                    valueContext = cell->GetValueContext(widget);
                }
            }

            switch (LOWORD(wParam)) {
                case IDC_Done:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                    }
                    break;

                case IDC_Params:
                    if (HIWORD(wParam) == BN_CLICKED)
                        DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_EditAdvanced), g_hwnd, dlgProcEditAdvanced);
                    break;

                case IDC_PickRingStyle:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_PickRingStyle, CB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            SendDlgItemMessage(hwndDlg, IDC_PickRingStyle, CB_GETLBTEXT, index, (LPARAM) buf);
                            if (paramContext)
                                paramContext->GetWidgetProperties().set_prop(PropertyType_RingStyle, buf);
                        }
                    }
                    break;

                case IDC_FXParamNameEdit:
                    if (HIWORD(wParam) == EN_CHANGE && widget != NULL) {
                        GetDlgItemText(hwndDlg, IDC_FXParamNameEdit, buf, sizeof(buf));
                        SetDlgItemText(s_hwndLearnFXDlg, IDC_AssignFXParamDisplay, buf);
                        if (nameContext)
                            nameContext->SetStringParam(buf);
                    }
                    break;

                case IDC_PickSteps:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_PickSteps, CB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            string outputString;
                            GetParamStepsString(outputString, index);
                            SetDlgItemText(hwndDlg, IDC_EditSteps, outputString.c_str());
                            vector<string> tokens;
                            GetTokens(tokens, outputString.c_str());
                            vector<double> steps;
                            for (int i = 0; i < tokens.size(); ++i)
                                steps.push_back(atof(tokens[i].c_str()));

                            if (paramContext)
                                paramContext->SetStepValues(steps);
                        }
                    }
                    break;

                case IDC_COMBO_PickNameDisplay:
                    if (HIWORD(wParam) == CBN_SELCHANGE && cell != NULL) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_PickNameDisplay, CB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            char displayWidgetName[MEDBUF];
                            SendDlgItemMessage(hwndDlg, IDC_COMBO_PickNameDisplay, CB_GETLBTEXT, index, (LPARAM) displayWidgetName);

                            if (IsSameString(displayWidgetName, ""))
                                cell->ClearNameDisplayWidget(widget);
                            else {
                                char paramName[MEDBUF];
                                GetDlgItemText(hwndDlg, IDC_FXParamNameEdit, paramName, sizeof(paramName));

                                cell->SetNameWidget(widget, displayWidgetName, paramName);
                            }
                        }
                    }
                    break;

                case IDC_COMBO_PickValueDisplay:
                    if (HIWORD(wParam) == CBN_SELCHANGE && cell != NULL) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_PickValueDisplay, CB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            char valueWidgetName[MEDBUF];
                            SendDlgItemMessage(hwndDlg, IDC_COMBO_PickValueDisplay, CB_GETLBTEXT, index, (LPARAM) valueWidgetName);

                            if (IsSameString(valueWidgetName, ""))
                                cell->ClearValueDisplayWidget(widget);
                            else
                                cell->SetValueWidget(widget, valueWidgetName);
                        }
                    }
                    break;

                case IDC_ApplyColorsToAll:
                    if (HIWORD(wParam) == BN_CLICKED && t != NULL)
                        ApplyColorsToAll(t, hwndDlg, widget, modifier, paramContext, nameContext, valueContext, zoneManager);
                    break;

                case IDC_ApplyFontsAndMarginsToAll:
                    if (HIWORD(wParam) == BN_CLICKED && t != NULL)
                        ApplyFontsAndMarginsToAll(t, hwndDlg, widget, modifier, paramContext, nameContext, valueContext);
                    break;

                case IDC_FXParamRingColor:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        GR_SelectColor(hwndDlg, (int*) &GetButtonColorForID(LOWORD(wParam)));

                        ColorFromNative(GetButtonColorForID(IDC_FXParamRingColor), &color.r, &color.g, &color.b);

                        if (paramContext) {
                            paramContext->GetWidgetProperties().set_prop(PropertyType_LEDRingColor, color.rgba_to_string(colorBuf));
                            paramContext->GetWidget()->Configure(zoneManager->GetLearnedFocusedFXZone()->GetActionContexts(widget));
                        }
                        InvalidateRect(hwndDlg, NULL, true);
                    }
                    break;

                case IDC_FXParamIndicatorColor:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        GR_SelectColor(hwndDlg, (int*) &GetButtonColorForID(LOWORD(wParam)));

                        ColorFromNative(GetButtonColorForID(IDC_FXParamIndicatorColor), &color.r, &color.g, &color.b);

                        if (paramContext) {
                            paramContext->GetWidgetProperties().set_prop(PropertyType_PushColor, color.rgba_to_string(colorBuf));
                            paramContext->GetWidget()->Configure(zoneManager->GetLearnedFocusedFXZone()->GetActionContexts(widget));
                        }
                        InvalidateRect(hwndDlg, NULL, true);
                    }
                    break;

                case IDC_FixedTextDisplayForegroundColor:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        GR_SelectColor(hwndDlg, (int*) &GetButtonColorForID(LOWORD(wParam)));

                        ColorFromNative(GetButtonColorForID(IDC_FixedTextDisplayForegroundColor), &color.r, &color.g, &color.b);

                        if (nameContext) {
                            nameContext->GetWidgetProperties().set_prop(PropertyType_TextColor, color.rgba_to_string(colorBuf));
                            nameContext->GetWidget()->UpdateColorValue(color);
                            nameContext->ForceWidgetValue(nameContext->GetStringParam());
                        }
                        InvalidateRect(hwndDlg, NULL, true);
                    }
                    break;

                case IDC_FixedTextDisplayBackgroundColor:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        GR_SelectColor(hwndDlg, (int*) &GetButtonColorForID(LOWORD(wParam)));

                        ColorFromNative(GetButtonColorForID(IDC_FixedTextDisplayBackgroundColor), &color.r, &color.g, &color.b);

                        if (nameContext) {
                            nameContext->GetWidgetProperties().set_prop(PropertyType_BackgroundColor, color.rgba_to_string(colorBuf));
                            nameContext->ForceWidgetValue(nameContext->GetStringParam());
                        }
                        InvalidateRect(hwndDlg, NULL, true);
                    }
                    break;

                case IDC_FXParamDisplayForegroundColor:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        GR_SelectColor(hwndDlg, (int*) &GetButtonColorForID(LOWORD(wParam)));

                        ColorFromNative(GetButtonColorForID(IDC_FXParamDisplayForegroundColor), &color.r, &color.g, &color.b);

                        if (valueContext) {
                            valueContext->GetWidgetProperties().set_prop(PropertyType_TextColor, color.rgba_to_string(colorBuf));
                            valueContext->ForceWidgetValue(nameContext->GetStringParam());
                        }

                        InvalidateRect(hwndDlg, NULL, true);
                    }
                    break;

                case IDC_FXParamDisplayBackgroundColor:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        GR_SelectColor(hwndDlg, (int*) &GetButtonColorForID(LOWORD(wParam)));

                        ColorFromNative(GetButtonColorForID(IDC_FXParamDisplayBackgroundColor), &color.r, &color.g, &color.b);

                        if (valueContext) {
                            valueContext->GetWidgetProperties().set_prop(PropertyType_BackgroundColor, color.rgba_to_string(colorBuf));
                            valueContext->ForceWidgetValue(nameContext->GetStringParam());
                        }

                        InvalidateRect(hwndDlg, NULL, true);
                    }
                    break;

                case IDC_FixedTextDisplayPickFont:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_FixedTextDisplayPickFont, CB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            SendDlgItemMessage(hwndDlg, IDC_FixedTextDisplayPickFont, CB_GETLBTEXT, index, (LPARAM) buf);
                            if (nameContext) {
                                nameContext->GetWidgetProperties().set_prop(PropertyType_Font, buf);
                                nameContext->ForceWidgetValue(nameContext->GetStringParam());
                            }
                        }
                    }
                    break;

                case IDC_Edit_FixedTextDisplayTop:
                    if (HIWORD(wParam) == EN_CHANGE && !s_learnFX.isUpdatingParameters) {
                        buf[0] = 0;

                        GetDlgItemText(hwndDlg, IDC_Edit_FixedTextDisplayTop, buf, sizeof(buf));
                        if (nameContext) {
                            if (buf[0] != 0)
                                nameContext->GetWidgetProperties().set_prop(PropertyType_TopMargin, buf);
                            nameContext->ForceWidgetValue(nameContext->GetStringParam());
                        }
                    }
                    break;

                case IDC_Edit_FixedTextDisplayBottom:
                    if (HIWORD(wParam) == EN_CHANGE && !s_learnFX.isUpdatingParameters) {
                        buf[0] = 0;

                        GetDlgItemText(hwndDlg, IDC_Edit_FixedTextDisplayBottom, buf, sizeof(buf));
                        if (nameContext) {
                            if (buf[0] != 0)
                                nameContext->GetWidgetProperties().set_prop(PropertyType_BottomMargin, buf);
                            nameContext->ForceWidgetValue(nameContext->GetStringParam());
                        }
                    }
                    break;

                case IDC_FXParamValueDisplayPickFont:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_FXParamValueDisplayPickFont, CB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            SendDlgItemMessage(hwndDlg, IDC_FXParamValueDisplayPickFont, CB_GETLBTEXT, index, (LPARAM) buf);
                            if (valueContext) {
                                valueContext->GetWidgetProperties().set_prop(PropertyType_Font, buf);
                                valueContext->ForceWidgetValue(nameContext->GetStringParam());
                            }
                        }
                    }
                    break;

                case IDC_Edit_ParamValueDisplayTop:
                    if (HIWORD(wParam) == EN_CHANGE && !s_learnFX.isUpdatingParameters) {
                        buf[0] = 0;

                        GetDlgItemText(hwndDlg, IDC_Edit_ParamValueDisplayTop, buf, sizeof(buf));
                        if (valueContext) {
                            if (buf[0] != 0)
                                valueContext->GetWidgetProperties().set_prop(PropertyType_TopMargin, buf);
                            valueContext->ForceWidgetValue(nameContext->GetStringParam());
                        }
                    }
                    break;

                case IDC_Edit_ParamValueDisplayBottom:
                    if (HIWORD(wParam) == EN_CHANGE && !s_learnFX.isUpdatingParameters) {
                        buf[0] = 0;

                        GetDlgItemText(hwndDlg, IDC_Edit_ParamValueDisplayBottom, buf, sizeof(buf));
                        if (valueContext) {
                            if (buf[0] != 0)
                                valueContext->GetWidgetProperties().set_prop(PropertyType_BottomMargin, buf);
                            valueContext->ForceWidgetValue(nameContext->GetStringParam());
                        }
                    }
                    break;
            }
        } break;
    }

    return 0;
}

static WDL_DLGRET dlgProcLearnFX(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SurfaceFXTemplate* t = GetSurfaceFXTemplate(hwndDlg);

    ZoneManager* zoneManager = NULL;

    if (t)
        zoneManager = t->zoneManager;

    Widget* widget = s_learnFX.currentWidget;

    switch (uMsg) {
        case WM_INITDIALOG: {
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)&s_learnFX);
            hFont16 = CreateFont(16, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");

            if (hFont16)
                SendMessage(GetDlgItem(hwndDlg, IDC_SurfaceName), WM_SETFONT, (WPARAM) hFont16, 0);

            hFont14 = CreateFont(14, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");

            if (hFont14) {
                SendMessage(GetDlgItem(hwndDlg, IDC_AssignWidgetDisplay), WM_SETFONT, (WPARAM) hFont14, 0);
                SetDlgItemText(hwndDlg, IDC_AssignWidgetDisplay, "Turn Widget");
                SendMessage(GetDlgItem(hwndDlg, IDC_AssignFXParamDisplay), WM_SETFONT, (WPARAM) hFont14, 0);
                SetDlgItemText(hwndDlg, IDC_AssignFXParamDisplay, "Adjust gui param");
            }

            EnableWindow(GetDlgItem(hwndDlg, IDC_DeepEdit), false);

            ShowWindow(GetDlgItem(hwndDlg, IDC_Unassign), false);
            ShowWindow(GetDlgItem(hwndDlg, IDC_Assign), false);
            OnDialogInit(hwndDlg);
        } break;

        case WM_DESTROY:
            OnDialogDestroy(hwndDlg, 0);
            break;

        case WM_CLOSE: {
            s_surfaceFXTemplates.clear();

            if (zoneManager)
                zoneManager->ClearLearnFocusedFXZone();

            ReleaseFX();

            s_learnFX.currentWidget = NULL;
            s_learnFX.currentModifier = -1;

            if (hFont16)
                DeleteObject(hFont16);

            if (hFont14)
                DeleteObject(hFont14);

            s_learnFX = LearnFXState{};   // reset state on dialog close
            DestroyWindow(hwndDlg);
            s_hwndLearnFXDlg = NULL;
        } break;

        case WM_USER + 1024: {
            s_learnFX.lastTouchedParamNum = -1;
            SetWindowText(hwndDlg, s_learnFX.fxAlias);

            if (SurfaceFXTemplate* t = GetSurfaceFXTemplate(hwndDlg)) {
                t->zoneManager->LoadLearnFocusedFXZone(s_learnFX.focusedTrack, s_learnFX.fxName, s_learnFX.fxSlot);
                CreateContextMap(t);
            }
        } break;

        case WM_COMMAND: {
            ActionContext* paramContext = NULL;
            ActionContext* nameContext = NULL;
            ActionContext* valueContext = NULL;

            int modifier = 0;

            if (zoneManager) {
                const vector<int>& modifiers = zoneManager->GetSurface()->GetModifiers();

                if (modifiers.size() > 0)
                    modifier = modifiers[0];

                paramContext = GetFirstContext(zoneManager, widget, modifier);
            }

            FXCell* cell = NULL;

            if (t) {
                cell = GetCell(t, widget, modifier);

                if (cell) {
                    nameContext = cell->GetNameContext(widget);
                    valueContext = cell->GetValueContext(widget);
                }
            }

            switch (LOWORD(wParam)) {
                case IDC_Assign:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int paramNum = s_learnFX.lastTouchedParamNum;
                        if (paramNum < 0)
                            break;

                        if (widget == NULL)
                            break;

                        if (t)
                            HandleAssigment(t, widget, modifier, paramNum, true);
                    }
                    break;

                case IDC_Unassign:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int paramNum = s_learnFX.lastTouchedParamNum;
                        if (paramNum < 0)
                            break;

                        if (widget == NULL)
                            break;

                        if (t)
                            HandleAssigment(t, widget, modifier, paramNum, false);
                    }
                    break;

                case IDC_Alias:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_EditFXAlias), g_hwnd, dlgProcEditFXAlias);

                        if (s_dlgResult == IDOK)
                            SetWindowText(hwndDlg, s_learnFX.fxAlias);
                    }
                    break;

                case IDC_DeepEdit:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        if (s_learnFX.pageSurfaceFXLearnLevel == "Level2")
                            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_LearnFXLevel2), g_hwnd, dlgProcLearnFXDeepEdit);
                        else if (s_learnFX.pageSurfaceFXLearnLevel == "Level3")
                            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_LearnFXLevel3), g_hwnd, dlgProcLearnFXDeepEdit);
                    }
                    break;

                case IDC_Save:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        if (zoneManager) {
                            SaveZone(t);
                            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                        }
                    }
                    break;
            }
        } break;
    }

    return 0;
}

void WidgetMoved(ZoneManager* zoneManager, Widget* widget, int modifier) {
    SurfaceFXTemplate* t = GetSurfaceFXTemplate(zoneManager);

    if (!t)
        return;

    if (!t->hwnd)
        return;

    if (s_learnFX.focusedTrack == NULL)
        return;

    if (zoneManager == NULL)
        return;

    if (zoneManager->GetLearnedFocusedFXZone() == NULL)
        return;

    vector<Widget*> widgets = zoneManager->GetLearnedFocusedFXZone()->GetWidgets();
    if (find(widgets.begin(), widgets.end(), widget) == widgets.end())
        return zoneManager->GetCSI()->ShowErrorOSD(string("Widget [") + widget->GetName() + "] is not defined in FXWidgetLayout");

    s_learnFX.currentWidget = widget;
    s_learnFX.currentModifier = modifier;

    char buf[MEDBUF];
    GetFullWidgetName(widget, modifier, buf, sizeof(buf));
    SetDlgItemText(t->hwnd, IDC_AssignWidgetDisplay, buf);

    if (ActionContext* context = GetFirstContext(zoneManager, widget, modifier)) {
        if (context->GetAction()->GetType() == ActionType::NoAction) {
            SetDlgItemText(t->hwnd, IDC_AssignFXParamDisplay, "");
            EnableWindow(GetDlgItem(t->hwnd, IDC_DeepEdit), false);

            ShowWindow(GetDlgItem(t->hwnd, IDC_Unassign), false);
            ShowWindow(GetDlgItem(t->hwnd, IDC_Assign), true);
            EnableWindow(GetDlgItem(t->hwnd, IDC_Assign), false);
        } else {
            FXCell* cell = GetCell(t, widget, modifier);

            if (cell) {
                if (ActionContext* nameContext = cell->GetNameContext(widget))
                    SetDlgItemText(t->hwnd, IDC_AssignFXParamDisplay, nameContext->GetStringParam());

                EnableWindow(GetDlgItem(t->hwnd, IDC_DeepEdit), true);
                ShowWindow(GetDlgItem(t->hwnd, IDC_Unassign), true);
                ShowWindow(GetDlgItem(t->hwnd, IDC_Assign), false);
                EnableWindow(GetDlgItem(t->hwnd, IDC_Unassign), true);

            } else {
                // GAW - TBD -- if no display, use param num to get plugin supplied name
            }
        }
    } else
        SetDlgItemText(t->hwnd, IDC_AssignFXParamDisplay, "");
}

static void InitLearnFocusedFXDialog(ZoneManager* zoneManager) {
    s_surfaceFXTemplates.push_back(make_unique<SurfaceFXTemplate>(zoneManager));

    SurfaceFXTemplate* t = s_surfaceFXTemplates.back().get();
    LoadTemplates(t);

    t->hwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_LearnFX), g_hwnd, dlgProcLearnFX);

    if (t->hwnd) {
        s_hwndLearnFXDlg = t->hwnd;
        SendMessage(t->hwnd, WM_USER + 1024, 0, 0);
        ShowWindow(t->hwnd, SW_SHOW);
        SetDlgItemText(t->hwnd, IDC_SurfaceName, t->zoneManager->GetSurface()->GetName());

        ShowWindow(GetDlgItem(t->hwnd, IDC_Assign), false);
    }
}

void LaunchLearnFocusedFXDialog(ZoneManager* zoneManager) {
    TrackFX_GetFXName(s_learnFX.focusedTrack, s_learnFX.fxSlot, s_learnFX.fxName, sizeof(s_learnFX.fxName));

    map<const string, ZoneInfo>& zoneInfo = zoneManager->GetZoneInfo();

    memset(s_learnFX.fxAlias, 0, sizeof(s_learnFX.fxAlias));

    if (zoneInfo.find(s_learnFX.fxName) != zoneInfo.end())
        lstrcpyn_safe(s_learnFX.fxAlias, zoneInfo[s_learnFX.fxName].alias.c_str(), sizeof(s_learnFX.fxAlias));
    else
        zoneManager->GetAlias(s_learnFX.fxName, s_learnFX.fxAlias, sizeof(s_learnFX.fxAlias));

    InitLearnFocusedFXDialog(zoneManager);
}

void RequestFocusedFXDialog(ZoneManager* zoneManager) {
    if (s_learnFX.focusedTrack != NULL && s_surfaceFXTemplates.size() == 1 && s_surfaceFXTemplates[0]->zoneManager == zoneManager) {
        SurfaceFXTemplate const* t = s_surfaceFXTemplates[0].get();
        if (t->hwnd != NULL) {
            SendMessage(t->hwnd, WM_CLOSE, 0, 0);
            return;
        }
    }
    if (DAW::CheckTouchedOrFocusedFX(&s_learnFX.focusedTrack, &s_learnFX.fxSlot, &s_learnFX.lastTouchedParamNum))
        LaunchLearnFocusedFXDialog(zoneManager);
    else
        zoneManager->GetCSI()->ShowErrorOSD("No active FX windows!");
}

void ShutdownLearn() {
    return;
    //CloseFocusedFXDialog();
}

void CloseFocusedFXDialog() {
    if (s_hwndLearnFXDlg != NULL)
        SendMessage(s_hwndLearnFXDlg, WM_CLOSE, 0, 0);
}

void UpdateLearnWindow(ZoneManager* zoneManager) {
    SurfaceFXTemplate* t = GetSurfaceFXTemplate(zoneManager);

    if (!t)
        return;

    if (!t->hwnd)
        return;

    int trackNumberOut;
    int fxNumberOut;
    int paramNumberOut;

    if (GetLastTouchedFX(&trackNumberOut, &fxNumberOut, &paramNumberOut)) {
        double minvalOut = 0.0;
        double maxvalOut = 0.0;
        double currentParamValue = TrackFX_GetParam(DAW::GetTrack(trackNumberOut), fxNumberOut, paramNumberOut, &minvalOut, &maxvalOut);

        if (s_learnFX.lastTouchedParamNum != paramNumberOut || s_learnFX.lastTouchedParamValue != currentParamValue) {
            s_learnFX.lastTouchedParamNum = paramNumberOut;
            s_learnFX.lastTouchedParamValue = currentParamValue;

            if (IsWindowVisible(GetDlgItem(t->hwnd, IDC_Assign))) {
                char buf[MEDBUF];
                TrackFX_GetParamName(DAW::GetTrack(trackNumberOut), fxNumberOut, paramNumberOut, buf, sizeof(buf));

                SetDlgItemText(t->hwnd, IDC_AssignFXParamDisplay, buf);
                EnableWindow(GetDlgItem(t->hwnd, IDC_Assign), true);
            }
        }
    }
}

void InitBlankLearnFocusedFXZone(ZoneManager* zoneManager, Zone* fxZone, MediaTrack* track, int fxSlot) {
    SurfaceFXTemplate* t = NULL;

    for (auto& surfaceFXTemplate : s_surfaceFXTemplates) {
        if (surfaceFXTemplate->zoneManager == zoneManager) {
            t = surfaceFXTemplate.get();
            break;
        }
    }

    if (!t)
        return;

    map<const string, ZoneInfo>& zoneInfo = zoneManager->GetZoneInfo();

    if (zoneInfo.find("FXPrologue") != zoneInfo.end())
        zoneManager->LoadZoneFile(fxZone, zoneInfo["FXPrologue"].filePath.c_str(), "");

    vector<string> blankParams;

    for (int rowLayoutIdx = 0; rowLayoutIdx < t->fxRowLayouts.size(); ++rowLayoutIdx) {
        int modifier = t->fxRowLayouts[rowLayoutIdx].modifier;

        const int channelsNum = zoneManager->GetSurface()->GetNumChannels();
        for (int channel = 1; channel <= channelsNum; ++channel) {
            for (int widgetTypesIdx = 0; widgetTypesIdx < t->paramWidgets.size(); ++widgetTypesIdx) {
                Widget* widget = FindWidget(zoneManager, t->paramWidgets[widgetTypesIdx], t->fxRowLayouts[rowLayoutIdx].suffix, channel);
                if (widget) {
                    fxZone->AddWidget(widget);
                    fxZone->AddActionContext(widget, modifier, fxZone, "NoAction", blankParams);
                }
            }

            for (int widgetTypesIdx = 0; widgetTypesIdx < t->displayRows.size(); ++widgetTypesIdx) {
                Widget* widget = FindWidget(zoneManager, t->displayRows[widgetTypesIdx], t->fxRowLayouts[rowLayoutIdx].suffix, channel);
                if (widget) {
                    fxZone->AddWidget(widget);
                    fxZone->AddActionContext(widget, modifier, fxZone, "NoAction", blankParams);
                }
            }
        }
    }

    if (zoneInfo.find("FXEpilogue") != zoneInfo.end())
        zoneManager->LoadZoneFile(fxZone, zoneInfo["FXEpilogue"].filePath.c_str(), "");

    CreateContextMap(t);
}

#endif
