#ifdef CSI_UI_INCLUDE_CONFIG_DIALOGS
static bool s_editMode = false;

static const char* s_genericOSCSurface = "Generic OSC Surface";
static const char* s_BehringerX32Surface = "Behringer X32 Surface";

static string s_pageName;
static string s_oldSurfaceName;
static string s_surfaceName;
static string s_surfaceType;
static int s_surfaceInPort = 0;
static int s_surfaceOutPort = 0;
static int s_surfaceChannelCount = 0;
static int s_surfaceRefreshRate = 0;
static int s_surfaceDefaultRefreshRate = 15;
static int s_surfaceMaxPacketsPerRun = 0;
static int s_surfaceDefaultMaxPacketsPerRun = 0; // No restriction, send all queued packets
static int s_surfaceMaxSysExMessagesPerRun = 0;
static int s_surfaceDefaultMaxSysExMessagesPerRun = 200;
static string s_surfaceRemoteDeviceIP;
static int s_pageIndex = 0;
static bool s_followMCP = false;
static bool s_synchPages = true;
static bool s_isScrollLinkEnabled = false;
static bool s_scrollSynch = false;

static string s_pageSurface;
static string s_pageSurfaceFolder;
static string s_pageSurfaceZoneFolder;
static string s_pageSurfaceFXZoneFolder;
static int s_channelOffset = 0;

// TODO: on reload close all windows to prevent crash
// bool CALLBACK CloseWindowProc(HWND hwnd, LPARAM lParam) {
//     DWORD processId = 0;
//     GetWindowThreadProcessId(hwnd, &processId);
//     if (processId == GetCurrentProcessId()) {
//         PostMessage(hwnd, WM_CLOSE, 0, 0);
//     }
//     return true;
// }

// void CloseAllWindows() {
//     EnumWindows(CloseWindowProc, 0);
// }

////////////////////////////////////////////////////////////////////////////////////////////////////////
// structs
////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SurfaceLine {
    string type;
    string name;
    int channelCount = 0;
    int inPort = 0;
    int outPort = 0;
    int surfaceRefreshRate = s_surfaceDefaultRefreshRate;
    int surfaceMaxSysExMessagesPerRun = s_surfaceDefaultMaxSysExMessagesPerRun;
    int surfaceMaxPacketsPerRun = s_surfaceDefaultMaxPacketsPerRun;
    string remoteDeviceIP;

    SurfaceLine() {}

    SurfaceLine(const char* const aType, string aName, int aChannelCount, int anInPort, int anOutPort, int aRefreshRate, int maxSysExMessages)
        : type(aType)
        , name(aName)
        , channelCount(aChannelCount)
        , inPort(anInPort)
        , outPort(anOutPort)
        , surfaceRefreshRate(aRefreshRate)
        , surfaceMaxSysExMessagesPerRun(maxSysExMessages) {}

    SurfaceLine(string aType, string aName, int aChannelCount, int anInPort, int anOutPort, int aRefreshRate, int maxPackets, string aRemoteDeviceIP)
        : type(aType)
        , name(aName)
        , channelCount(aChannelCount)
        , inPort(anInPort)
        , outPort(anOutPort)
        , surfaceRefreshRate(aRefreshRate)
        , surfaceMaxPacketsPerRun(maxPackets)
        , remoteDeviceIP(aRemoteDeviceIP) {}
};

static vector<unique_ptr<SurfaceLine>> s_surfaces;

struct PageSurfaceLine {
    string surface;
    string pageSurface;
    string pageSurfaceFolder;
    string pageSurfaceZoneFolder;
    string pageSurfaceFXZoneFolder;
    int channelOffset;

    PageSurfaceLine() {
        channelOffset = 0;
    }
};

// Broadcast/Listen
struct Listener {
    string name;
    bool goHome = false;
    bool sends = false;
    bool receives = false;
    bool fxMenu = false;
    bool selectedTrackFX = false;
    bool modifiers = false;

    Listener(string aName, bool aGoHome, bool aSends, bool aReceives, bool anFXMenu, bool aSelectedTrackFX, bool aModifiers)
        : name(aName)
        , goHome(aGoHome)
        , sends(aSends)
        , receives(aReceives)
        , fxMenu(anFXMenu)
        , selectedTrackFX(aSelectedTrackFX)
        , modifiers(aModifiers) {}

    Listener(string aName)
        : name(aName) {}
};

struct Broadcaster {
    string name;
    vector<unique_ptr<Listener>> listeners;
};

struct PageLine {
    string name;
    bool followMCP = true;
    bool synchPages = true;
    bool isScrollLinkEnabled = false;
    bool isScrollSynchEnabled = false;
    vector<unique_ptr<PageSurfaceLine>> surfaces;
    vector<unique_ptr<Broadcaster>> broadcasters;

    PageLine() {}
};

// Scratch pad to get in and out of dialogs easily
static vector<unique_ptr<Broadcaster>> s_broadcasters;

static void TransferBroadcasters(vector<unique_ptr<Broadcaster>>& source, vector<unique_ptr<Broadcaster>>& destination) {
    destination.clear();

    for (int i = 0; i < source.size(); ++i) {
        destination.push_back(make_unique<Broadcaster>());

        Broadcaster* destinationBroadcaster = destination.back().get();

        destinationBroadcaster->name = source[i]->name;

        for (auto& listener : source[i]->listeners)
            destinationBroadcaster->listeners.push_back(make_unique<Listener>(listener->name, listener->goHome, listener->sends, listener->receives, listener->fxMenu, listener->selectedTrackFX, listener->modifiers));
    }
}

static vector<unique_ptr<PageLine>> s_pages;

static void AddComboEntry(HWND hwndDlg, int x, const char* buf, int comboId) {
    int a = (int) SendDlgItemMessage(hwndDlg, comboId, CB_ADDSTRING, 0, (LPARAM) buf);
    SendDlgItemMessage(hwndDlg, comboId, CB_SETITEMDATA, a, x);
}

static void AddListEntry(HWND hwndDlg, string buf, int comboId) {
    SendDlgItemMessage(hwndDlg, comboId, LB_ADDSTRING, 0, (LPARAM) buf.c_str());
}

static bool CharsOK(HWND hwndDlg, string chars) {
    for (int i = 0; i < chars.length(); ++i)
        if (!(isdigit(chars[i]) || isalpha(chars[i]) || chars[i] == '_')) {
            MessageBox(g_hwnd, __LOCALIZE("Alphnumeric and Underscore only", "csi_mbox"), __LOCALIZE("Character Check Failed", "csi_mbox"), MB_OK);
            return false;
        };

    return true;
}

static WDL_DLGRET dlgProcPage(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            if (s_editMode) {
                SetDlgItemText(hwndDlg, IDC_EDIT_PageName, s_pageName.c_str());

                CheckDlgButton(hwndDlg, IDC_RADIO_TCP, !s_followMCP);
                CheckDlgButton(hwndDlg, IDC_RADIO_MCP, s_followMCP);

                CheckDlgButton(hwndDlg, IDC_CHECK_SynchPages, s_synchPages);
                CheckDlgButton(hwndDlg, IDC_CHECK_ScrollLink, s_isScrollLinkEnabled);
                CheckDlgButton(hwndDlg, IDC_CHECK_ScrollSynch, s_scrollSynch);
            }
            OnDialogInit(hwndDlg);
        } break;

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        char buf[MEDBUF];
                        GetDlgItemText(hwndDlg, IDC_EDIT_PageName, buf, sizeof(buf));
                        s_pageName = buf;

                        if (!CharsOK(hwndDlg, buf))
                            break;

                        if (IsDlgButtonChecked(hwndDlg, IDC_RADIO_TCP))
                            s_followMCP = false;
                        else if (IsDlgButtonChecked(hwndDlg, IDC_RADIO_MCP))
                            s_followMCP = true;

                        s_synchPages = !!IsDlgButtonChecked(hwndDlg, IDC_CHECK_SynchPages);
                        s_isScrollLinkEnabled = !!IsDlgButtonChecked(hwndDlg, IDC_CHECK_ScrollLink);
                        s_scrollSynch = !!IsDlgButtonChecked(hwndDlg, IDC_CHECK_ScrollSynch);

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

static vector<string> s_surfaceFolders;

static WDL_DLGRET dlgProcPageSurface(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO_PageSurfaceFolder));
            WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO_PageSurface));

            s_surfaceFolders.clear();
            try {
                s_surfaceFolders = ProductPaths::FromReaperResourcePath().ListSurfaceIds();
            } catch (const std::exception& error) {
                LogToConsole("[ERROR] Failed to list product surfaces: %s\n", error.what());
            }

            for (auto surfaceFolder : s_surfaceFolders)
                AddComboEntry(hwndDlg, 0, surfaceFolder.c_str(), IDC_COMBO_PageSurfaceFolder);

            for (auto& surface : s_surfaces)
                AddComboEntry(hwndDlg, 0, surface->name.c_str(), IDC_COMBO_PageSurface);

            if (s_editMode) {
                int index = (int) SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PageSurfaceFolder), CB_FINDSTRINGEXACT, -1, (LPARAM) s_pageSurfaceFolder.c_str());
                if (index >= 0)
                    SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PageSurfaceFolder), CB_SETCURSEL, index, 0);
                else
                    SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PageSurfaceFolder), CB_SETCURSEL, 0, 0);

                index = (int) SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PageSurface), CB_FINDSTRINGEXACT, -1, (LPARAM) s_pageSurface.c_str());
                if (index >= 0)
                    SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PageSurface), CB_SETCURSEL, index, 0);
                else
                    SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PageSurface), CB_SETCURSEL, 0, 0);

                SetDlgItemInt(hwndDlg, IDC_EDIT_ChannelOffset, s_channelOffset, false);
            } else {
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PageSurfaceFolder), CB_SETCURSEL, 0, 0);
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PageSurface), CB_SETCURSEL, 0, 0);
                SetDlgItemText(hwndDlg, IDC_EDIT_ChannelOffset, "0");
            }
            OnDialogInit(hwndDlg);
        } break;

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        char buf[MEDBUF];

                        GetDlgItemText(hwndDlg, IDC_COMBO_PageSurface, buf, sizeof(buf));
                        s_pageSurface = buf;
                        if (s_pageSurface.empty()) {
                            MessageBox(hwndDlg, "Select a configured MIDI or OSC surface first.", ProductIdentity::DisplayName, MB_OK);
                            break;
                        }

                        GetDlgItemText(hwndDlg, IDC_COMBO_PageSurfaceFolder, buf, sizeof(buf));
                        s_pageSurfaceFolder = buf;
                        if (s_pageSurfaceFolder.empty()) {
                            const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
                            const string message = "No surface configuration is available. Add <surface-id>.txt under " + productPaths.UserSurfacesRoot().string() + " or install a vendor surface under " + productPaths.VendorSurfacesRoot().string() + ".";
                            MessageBox(hwndDlg, message.c_str(), ProductIdentity::DisplayName, MB_OK);
                            break;
                        }
                        s_pageSurfaceZoneFolder = buf;
                        s_pageSurfaceFXZoneFolder = buf;

                        GetDlgItemText(hwndDlg, IDC_EDIT_ChannelOffset, buf, sizeof(buf));
                        s_channelOffset = atoi(buf);

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

static WDL_DLGRET dlgProcMidiSurface(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            char buf[MEDBUF];
            int currentIndex = 0;
            WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO_MidiIn));
            WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO_MidiOut));

            for (int i = 0; i < GetNumMIDIInputs(); ++i)
                if (GetMIDIInputName(i, buf, sizeof(buf))) {
                    AddComboEntry(hwndDlg, i, buf, IDC_COMBO_MidiIn);
                    if (s_editMode && s_surfaceInPort == i)
                        SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_MidiIn), CB_SETCURSEL, currentIndex, 0);
                    currentIndex++;
                }

            currentIndex = 0;

            for (int i = 0; i < GetNumMIDIOutputs(); ++i)
                if (GetMIDIOutputName(i, buf, sizeof(buf))) {
                    AddComboEntry(hwndDlg, i, buf, IDC_COMBO_MidiOut);
                    if (s_editMode && s_surfaceOutPort == i)
                        SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_MidiOut), CB_SETCURSEL, currentIndex, 0);
                    currentIndex++;
                }

            if (s_editMode) {
                SetDlgItemText(hwndDlg, IDC_EDIT_MidiSurfaceName, s_surfaceName.c_str());
                SetDlgItemInt(hwndDlg, IDC_EDIT_NumChannels, s_surfaceChannelCount, true);
                SetDlgItemInt(hwndDlg, IDC_EDIT_MidiSurfaceRefreshRate, s_surfaceRefreshRate, true);
                SetDlgItemInt(hwndDlg, IDC_EDIT_MidiSurfaceMaxSysExMessagesPerRun, s_surfaceMaxSysExMessagesPerRun, true);
            } else {
                SetDlgItemInt(hwndDlg, IDC_EDIT_MidiSurfaceRefreshRate, s_surfaceDefaultRefreshRate, true);
                SetDlgItemInt(hwndDlg, IDC_EDIT_MidiSurfaceMaxSysExMessagesPerRun, s_surfaceDefaultMaxSysExMessagesPerRun, true);
                SetDlgItemText(hwndDlg, IDC_EDIT_MidiSurfaceName, "");
                SetDlgItemText(hwndDlg, IDC_EDIT_NumChannels, "0");
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_MidiIn), CB_SETCURSEL, 0, 0);
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_MidiOut), CB_SETCURSEL, 0, 0);
            }
            OnDialogInit(hwndDlg);
        } break;

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        char buf[MEDBUF];
                        GetDlgItemText(hwndDlg, IDC_EDIT_MidiSurfaceName, buf, sizeof(buf));
                        s_surfaceName = buf;

                        if (!CharsOK(hwndDlg, buf))
                            break;

                        GetDlgItemText(hwndDlg, IDC_EDIT_NumChannels, buf, sizeof(buf));
                        s_surfaceChannelCount = atoi(buf);

                        BOOL translated;
                        s_surfaceRefreshRate = GetDlgItemInt(hwndDlg, IDC_EDIT_MidiSurfaceRefreshRate, &translated, true);

                        s_surfaceMaxSysExMessagesPerRun = GetDlgItemInt(hwndDlg, IDC_EDIT_MidiSurfaceMaxSysExMessagesPerRun, &translated, true);

                        int currentSelection = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_MidiIn, CB_GETCURSEL, 0, 0);
                        if (currentSelection >= 0)
                            s_surfaceInPort = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_MidiIn, CB_GETITEMDATA, currentSelection, 0);
                        currentSelection = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_MidiOut, CB_GETCURSEL, 0, 0);
                        if (currentSelection >= 0)
                            s_surfaceOutPort = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_MidiOut, CB_GETITEMDATA, currentSelection, 0);

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

static WDL_DLGRET dlgProcOSCSurface(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO_Type));
            AddComboEntry(hwndDlg, 0, s_genericOSCSurface, IDC_COMBO_Type);
            AddComboEntry(hwndDlg, 1, s_BehringerX32Surface, IDC_COMBO_Type);

            if (s_editMode) {
                SetDlgItemText(hwndDlg, IDC_EDIT_OSCSurfaceName, s_surfaceName.c_str());

                if (s_surfaceType == s_OSCSurfaceToken)
                    SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_Type), CB_SETCURSEL, 0, 0);
                else
                    SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_Type), CB_SETCURSEL, 1, 0);

                SetDlgItemInt(hwndDlg, IDC_EDIT_NumChannels, s_surfaceChannelCount, true);
                SetDlgItemText(hwndDlg, IDC_EDIT_OSCRemoteDeviceIP, s_surfaceRemoteDeviceIP.c_str());
                SetDlgItemInt(hwndDlg, IDC_EDIT_OSCInPort, s_surfaceInPort, true);
                SetDlgItemInt(hwndDlg, IDC_EDIT_OSCOutPort, s_surfaceOutPort, true);
                SetDlgItemInt(hwndDlg, IDC_EDIT_MaxPackets, s_surfaceMaxPacketsPerRun, true);
            } else {
                SetDlgItemText(hwndDlg, IDC_EDIT_NumChannels, "0");
                SetDlgItemText(hwndDlg, IDC_EDIT_OSCSurfaceName, "");
                SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_Type), CB_SETCURSEL, 0, 0);
                SetDlgItemText(hwndDlg, IDC_EDIT_OSCRemoteDeviceIP, "");
                SetDlgItemText(hwndDlg, IDC_EDIT_OSCInPort, "");
                SetDlgItemText(hwndDlg, IDC_EDIT_OSCOutPort, "");
                SetDlgItemInt(hwndDlg, IDC_EDIT_MaxPackets, s_surfaceDefaultMaxPacketsPerRun, false);
            }
            OnDialogInit(hwndDlg);
        } break;

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        char buf[MEDBUF];

                        GetDlgItemText(hwndDlg, IDC_EDIT_OSCSurfaceName, buf, sizeof(buf));
                        s_surfaceName = buf;

                        if (!CharsOK(hwndDlg, buf))
                            break;

                        int index = (int) SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_Type), CB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            if (index == 0)
                                s_surfaceType = s_OSCSurfaceToken;
                            else
                                s_surfaceType = s_OSCX32SurfaceToken;
                        }

                        GetDlgItemText(hwndDlg, IDC_EDIT_NumChannels, buf, sizeof(buf));
                        s_surfaceChannelCount = atoi(buf);

                        GetDlgItemText(hwndDlg, IDC_EDIT_OSCRemoteDeviceIP, buf, sizeof(buf));
                        s_surfaceRemoteDeviceIP = buf;

                        GetDlgItemText(hwndDlg, IDC_EDIT_OSCInPort, buf, sizeof(buf));
                        s_surfaceInPort = atoi(buf);

                        GetDlgItemText(hwndDlg, IDC_EDIT_OSCOutPort, buf, sizeof(buf));
                        s_surfaceOutPort = atoi(buf);

                        GetDlgItemText(hwndDlg, IDC_EDIT_MaxPackets, buf, sizeof(buf));
                        s_surfaceMaxPacketsPerRun = atoi(buf);

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

static void SetCheckBoxes(HWND hwndDlg, Listener* listener) {
    if (listener == NULL)
        return;

    char tmp[MEDBUF];
    snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("%s Listens to", "csi_osc"), listener->name.c_str());
    SetDlgItemText(hwndDlg, IDC_ListenCheckboxes, tmp);

    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_GoHome), BM_SETCHECK, listener->goHome ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Sends), BM_SETCHECK, listener->sends ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Receives), BM_SETCHECK, listener->receives ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_FXMenu), BM_SETCHECK, listener->fxMenu ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Modifiers), BM_SETCHECK, listener->modifiers ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_SelectedTrackFX), BM_SETCHECK, listener->selectedTrackFX ? BST_CHECKED : BST_UNCHECKED, 0);
}

static void ClearCheckBoxes(HWND hwndDlg) {
    SetDlgItemText(hwndDlg, IDC_ListenCheckboxes, __LOCALIZE("Surface Listens to", "csi_osc"));

    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_GoHome), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Sends), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Receives), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_FXMenu), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Modifiers), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_SelectedTrackFX), BM_SETCHECK, BST_UNCHECKED, 0);
}

HWND s_hwndMainDlg;

static WDL_DLGRET dlgProcAdvancedSetup(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_AddBroadcaster));
            WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_AddListener));
            for (auto& surface : s_surfaces)
                AddComboEntry(hwndDlg, 0, surface->name.c_str(), IDC_AddBroadcaster);
            SendMessage(GetDlgItem(hwndDlg, IDC_AddBroadcaster), CB_SETCURSEL, 0, 0);

            for (auto& surface : s_surfaces)
                AddComboEntry(hwndDlg, 0, surface->name.c_str(), IDC_AddListener);
            SendMessage(GetDlgItem(hwndDlg, IDC_AddListener), CB_SETCURSEL, 0, 0);

            TransferBroadcasters(s_pages[s_pageIndex]->broadcasters, s_broadcasters);

            if (s_broadcasters.size() > 0) {
                for (auto& broadcaster : s_broadcasters)
                    AddListEntry(hwndDlg, broadcaster->name, IDC_LIST_Broadcasters);

                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Broadcasters), LB_SETCURSEL, 0, 0);
            }

            SetDlgItemInt(hwndDlg, IDC_EDIT_DebugLevel, g_debugLevel, FALSE);
            CheckDlgButton(hwndDlg, IDC_CHECK_ShowRawInput, g_surfaceRawInDisplay);
            CheckDlgButton(hwndDlg, IDC_CHECK_ShowInput, g_surfaceInDisplay);
            CheckDlgButton(hwndDlg, IDC_CHECK_ShowOutput, g_surfaceOutDisplay);
            CheckDlgButton(hwndDlg, IDC_CHECK_WriteFXParams, g_fxParamsWrite);
            OnDialogInit(hwndDlg);
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_LIST_Broadcasters:
                    if (HIWORD(wParam) == LBN_SELCHANGE) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        if (broadcasterIndex >= 0) {
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Listeners), LB_RESETCONTENT, 0, 0);

                            for (auto& listener : s_broadcasters[broadcasterIndex]->listeners)
                                AddListEntry(hwndDlg, listener->name, IDC_LIST_Listeners);

                            if (s_broadcasters.size() > 0)
                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Listeners), LB_SETCURSEL, 0, 0);

                            if (broadcasterIndex >= 0 && s_broadcasters[broadcasterIndex]->listeners.size() > 0)
                                SetCheckBoxes(hwndDlg, s_broadcasters[broadcasterIndex]->listeners[0].get());
                            else
                                ClearCheckBoxes(hwndDlg);
                        } else {
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Listeners), LB_RESETCONTENT, 0, 0);
                        }
                    }
                    break;

                case IDC_LIST_Listeners:
                    if (HIWORD(wParam) == LBN_SELCHANGE) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0 && listenerIndex >= 0)
                            SetCheckBoxes(hwndDlg, s_broadcasters[broadcasterIndex]->listeners[listenerIndex].get());
                    }
                    break;

                case ID_BUTTON_AddBroadcaster:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_AddBroadcaster, LB_GETCURSEL, 0, 0);
                        if (broadcasterIndex >= 0) {
                            char broadcasterName[MEDBUF];
                            GetDlgItemText(hwndDlg, IDC_AddBroadcaster, broadcasterName, sizeof(broadcasterName));

                            bool foundit = false;
                            for (auto& broadcaster : s_broadcasters)
                                if (broadcasterName == broadcaster->name)
                                    foundit = true;
                            if (!foundit) {
                                s_broadcasters.push_back(make_unique<Broadcaster>());
                                s_broadcasters.back().get()->name = broadcasterName;
                                AddListEntry(hwndDlg, broadcasterName, IDC_LIST_Broadcasters);
                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Broadcasters), LB_SETCURSEL, s_broadcasters.size() - 1, 0);
                                ClearCheckBoxes(hwndDlg);
                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Listeners), LB_RESETCONTENT, 0, 0);
                            }
                        }
                    }
                    break;

                case ID_BUTTON_AddListener:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_AddListener, LB_GETCURSEL, 0, 0);
                        if (broadcasterIndex >= 0 && listenerIndex >= 0) {
                            char listenerName[MEDBUF];
                            GetDlgItemText(hwndDlg, IDC_AddListener, listenerName, sizeof(listenerName));

                            bool foundit = false;
                            for (auto& listener : s_broadcasters[broadcasterIndex]->listeners)
                                if (listenerName == listener->name)
                                    foundit = true;
                            if (!foundit) {
                                s_broadcasters[broadcasterIndex]->listeners.push_back(make_unique<Listener>(listenerName));
                                AddListEntry(hwndDlg, listenerName, IDC_LIST_Listeners);
                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Listeners), LB_SETCURSEL, s_broadcasters[broadcasterIndex]->listeners.size() - 1, 0);
                                ClearCheckBoxes(hwndDlg);

                                char tmp[MEDBUF];
                                snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("%s Listens to", "csi_osc"), s_broadcasters[broadcasterIndex]->listeners[s_broadcasters[broadcasterIndex]->listeners.size() - 1]->name.c_str());
                                SetDlgItemText(hwndDlg, IDC_ListenCheckboxes, tmp);
                            }
                        }
                    }
                    break;

                case IDC_CHECK_GoHome:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0 && listenerIndex >= 0) {
                            if (SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_GoHome), BM_GETCHECK, 0, 0) == BST_CHECKED)
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->goHome = true;
                            else
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->goHome = false;
                        }
                    }
                    break;

                case IDC_CHECK_Sends:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0 && listenerIndex >= 0) {
                            if (SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Sends), BM_GETCHECK, 0, 0) == BST_CHECKED)
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->sends = true;
                            else
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->sends = false;
                        }
                    }
                    break;

                case IDC_CHECK_Receives:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0 && listenerIndex >= 0) {
                            if (SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Receives), BM_GETCHECK, 0, 0) == BST_CHECKED)
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->receives = true;
                            else
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->receives = false;
                        }
                    }
                    break;

                case IDC_CHECK_FXMenu:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0 && listenerIndex >= 0) {
                            if (SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_FXMenu), BM_GETCHECK, 0, 0) == BST_CHECKED)
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->fxMenu = true;
                            else
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->fxMenu = false;
                        }
                    }
                    break;

                case IDC_CHECK_Modifiers:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0 && listenerIndex >= 0) {
                            if (SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_Modifiers), BM_GETCHECK, 0, 0) == BST_CHECKED)
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->modifiers = true;
                            else
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->modifiers = false;
                        }
                    }
                    break;

                case IDC_CHECK_SelectedTrackFX:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0 && listenerIndex >= 0) {
                            if (SendMessage(GetDlgItem(hwndDlg, IDC_CHECK_SelectedTrackFX), BM_GETCHECK, 0, 0) == BST_CHECKED)
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->selectedTrackFX = true;
                            else
                                s_broadcasters[broadcasterIndex]->listeners[listenerIndex]->selectedTrackFX = false;
                        }
                    }
                    break;

                case ID_RemoveBroadcaster:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0) {
                            s_broadcasters.erase(s_broadcasters.begin() + broadcasterIndex);
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Broadcasters), LB_RESETCONTENT, 0, 0);
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Listeners), LB_RESETCONTENT, 0, 0);
                            ClearCheckBoxes(hwndDlg);

                            if (s_broadcasters.size() > 0) {
                                for (auto& broadcaster : s_broadcasters)
                                    AddListEntry(hwndDlg, broadcaster->name, IDC_LIST_Broadcasters);

                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Broadcasters), LB_SETCURSEL, s_broadcasters.size() - 1, 0);
                            }
                        }
                    }
                    break;

                case ID_RemoveListener:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int broadcasterIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Broadcasters, LB_GETCURSEL, 0, 0);
                        int listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                        if (broadcasterIndex >= 0 && listenerIndex >= 0) {
                            s_broadcasters[broadcasterIndex]->listeners.erase(s_broadcasters[broadcasterIndex]->listeners.begin() + listenerIndex);
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Listeners), LB_RESETCONTENT, 0, 0);
                            ClearCheckBoxes(hwndDlg);
                            if (s_broadcasters[broadcasterIndex]->listeners.size() > 0) {
                                for (auto& listener : s_broadcasters[broadcasterIndex]->listeners)
                                    AddListEntry(hwndDlg, listener->name, IDC_LIST_Listeners);

                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Listeners), LB_SETCURSEL, s_broadcasters[broadcasterIndex]->listeners.size() - 1, 0);

#ifdef WIN32
                                listenerIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Listeners, LB_GETCURSEL, 0, 0);

                                if (listenerIndex >= 0)
                                    SetCheckBoxes(hwndDlg, s_broadcasters[broadcasterIndex]->listeners[listenerIndex].get());
#endif
                            }
                        }
                    }
                    break;

                case IDCANCEL:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        EndDialog(hwndDlg, 0);
                    }
                    break;

                case IDOK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        BOOL success = FALSE;
                        int newDebugLevel = GetDlgItemInt(hwndDlg, IDC_EDIT_DebugLevel, &success, FALSE);
                        if (success)
                            g_debugLevel = std::clamp(newDebugLevel, 0, 4);
                        g_surfaceRawInDisplay = IsDlgButtonChecked(hwndDlg, IDC_CHECK_ShowRawInput) != 0;
                        g_surfaceInDisplay = IsDlgButtonChecked(hwndDlg, IDC_CHECK_ShowInput) != 0;
                        g_surfaceOutDisplay = IsDlgButtonChecked(hwndDlg, IDC_CHECK_ShowOutput) != 0;
                        g_fxParamsWrite = IsDlgButtonChecked(hwndDlg, IDC_CHECK_WriteFXParams) != 0;

                        TransferBroadcasters(s_broadcasters, s_pages[s_pageIndex]->broadcasters);

                        EndDialog(hwndDlg, 0);
                    }
                    break;
            }
        } break;
        case WM_DESTROY:
            OnDialogDestroy(hwndDlg, 0);
            break;
    }

    return 0;
}

WDL_DLGRET dlgProcMainConfig(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_LIST_Pages:
                    if (HIWORD(wParam) == LBN_DBLCLK) {
#ifdef WIN32
                        // pretend we clicked the Edit button
                        SendMessage(GetDlgItem(hwndDlg, IDC_BUTTON_EditPage), BM_CLICK, 0, 0);
#endif
                    } else if (HIWORD(wParam) == LBN_SELCHANGE) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Pages, LB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_RESETCONTENT, 0, 0);

                            s_pageIndex = index;

                            for (auto& surface : s_pages[s_pageIndex]->surfaces)
                                AddListEntry(hwndDlg, surface->pageSurface, IDC_LIST_PageSurfaces);

                            if (s_pages[index]->surfaces.size() > 0)
                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_SETCURSEL, 0, 0);

                        } else {
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_RESETCONTENT, 0, 0);
                        }
                    }
                    break;

                case IDC_LIST_Surfaces:
                    if (HIWORD(wParam) == LBN_DBLCLK) {
#ifdef WIN32
                        // pretend we clicked the Edit button
                        SendMessage(GetDlgItem(hwndDlg, IDC_BUTTON_EditSurface), BM_CLICK, 0, 0);
#endif
                    }
                    break;

                case IDC_LIST_PageSurfaces:
                    if (HIWORD(wParam) == LBN_DBLCLK) {
#ifdef WIN32
                        // pretend we clicked the Edit button
                        SendMessage(GetDlgItem(hwndDlg, IDC_BUTTON_EditPageSurface), BM_CLICK, 0, 0);
#endif
                    }
                    break;

                case IDC_BUTTON_AddMidiSurface:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        s_dlgResult = false;
                        s_editMode = false;
                        DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_MidiSurface), hwndDlg, dlgProcMidiSurface);
                        if (s_dlgResult == IDOK) {
                            s_surfaces.push_back(make_unique<SurfaceLine>(s_MidiSurfaceToken, s_surfaceName, s_surfaceChannelCount, s_surfaceInPort, s_surfaceOutPort, s_surfaceRefreshRate, s_surfaceMaxSysExMessagesPerRun));

                            AddListEntry(hwndDlg, s_surfaceName.c_str(), IDC_LIST_Surfaces);
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Surfaces), LB_SETCURSEL, s_surfaces.size() - 1, 0);
                        }
                    }
                    break;

                case IDC_BUTTON_AddOSCSurface:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        s_dlgResult = false;
                        s_editMode = false;
                        DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_OSCSurface), hwndDlg, dlgProcOSCSurface);
                        if (s_dlgResult == IDOK) {
                            s_surfaces.push_back(make_unique<SurfaceLine>(s_OSCSurfaceToken, s_surfaceName, s_surfaceChannelCount, s_surfaceInPort, s_surfaceOutPort, s_surfaceRefreshRate, s_surfaceMaxPacketsPerRun, s_surfaceRemoteDeviceIP));

                            AddListEntry(hwndDlg, s_surfaceName.c_str(), IDC_LIST_Surfaces);
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Surfaces), LB_SETCURSEL, s_surfaces.size() - 1, 0);
                        }
                    }
                    break;

                case IDC_BUTTON_AddPage:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        s_dlgResult = false;
                        s_editMode = false;
                        DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_Page), hwndDlg, dlgProcPage);
                        if (s_dlgResult == IDOK) {
                            s_pages.push_back(make_unique<PageLine>());

                            PageLine* page = s_pages.back().get();

                            page->name = s_pageName;
                            page->followMCP = s_followMCP;
                            page->synchPages = s_synchPages;
                            page->isScrollLinkEnabled = s_isScrollLinkEnabled;
                            page->isScrollSynchEnabled = s_scrollSynch;

                            AddListEntry(hwndDlg, s_pageName.c_str(), IDC_LIST_Pages);
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Pages), LB_SETCURSEL, s_pages.size() - 1, 0);
                        }
                    }
                    break;

                case IDC_BUTTON_AddPageSurface:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        s_dlgResult = false;
                        s_editMode = false;

                        DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_PageSurface), hwndDlg, dlgProcPageSurface);
                        if (s_dlgResult == IDOK) {
                            int index = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Pages, LB_GETCURSEL, 0, 0);
                            if (index >= 0) {
                                s_pages[index]->surfaces.push_back(make_unique<PageSurfaceLine>());
                                PageSurfaceLine* pageSurface = s_pages[index]->surfaces.back().get();

                                pageSurface->pageSurface = s_pageSurface;
                                pageSurface->pageSurfaceFolder = s_pageSurfaceFolder;
                                pageSurface->pageSurfaceZoneFolder = s_pageSurfaceZoneFolder;
                                pageSurface->pageSurfaceFXZoneFolder = s_pageSurfaceFXZoneFolder;
                                pageSurface->channelOffset = s_channelOffset;

                                AddListEntry(hwndDlg, s_pageSurface, IDC_LIST_PageSurfaces);
                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_SETCURSEL, s_pages[index]->surfaces.size() - 1, 0);
                            }
                        }
                    }
                    break;

                case IDC_BUTTON_EditSurface:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Surfaces, LB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            char surfaceNameBuf[MEDBUF];
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Surfaces), LB_GETTEXT, index, (LPARAM) (LPCTSTR) (surfaceNameBuf));
                            s_surfaceName = surfaceNameBuf;
                            s_oldSurfaceName = surfaceNameBuf;
                            s_surfaceType = s_surfaces[index]->type;
                            s_surfaceChannelCount = s_surfaces[index]->channelCount;
                            s_surfaceInPort = s_surfaces[index]->inPort;
                            s_surfaceOutPort = s_surfaces[index]->outPort;
                            s_surfaceRemoteDeviceIP = s_surfaces[index]->remoteDeviceIP;
                            s_surfaceRefreshRate = s_surfaces[index]->surfaceRefreshRate;
                            s_surfaceMaxPacketsPerRun = s_surfaces[index]->surfaceMaxPacketsPerRun;
                            s_surfaceMaxSysExMessagesPerRun = s_surfaces[index]->surfaceMaxSysExMessagesPerRun;

                            s_dlgResult = false;
                            s_editMode = true;

                            string type = s_surfaces[index]->type;

                            if (type == s_MidiSurfaceToken)
                                DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_MidiSurface), hwndDlg, dlgProcMidiSurface);
                            else if (type == s_OSCSurfaceToken || type == s_OSCX32SurfaceToken)
                                DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_OSCSurface), hwndDlg, dlgProcOSCSurface);

                            if (s_dlgResult == IDOK) {
                                s_surfaces[index]->name = s_surfaceName;
                                s_surfaces[index]->type = s_surfaceType;
                                s_surfaces[index]->channelCount = s_surfaceChannelCount;
                                s_surfaces[index]->remoteDeviceIP = s_surfaceRemoteDeviceIP;
                                s_surfaces[index]->inPort = s_surfaceInPort;
                                s_surfaces[index]->outPort = s_surfaceOutPort;
                                s_surfaces[index]->surfaceRefreshRate = s_surfaceRefreshRate;
                                s_surfaces[index]->surfaceMaxPacketsPerRun = s_surfaceMaxPacketsPerRun;
                                s_surfaces[index]->surfaceMaxSysExMessagesPerRun = s_surfaceMaxSysExMessagesPerRun;

                                if (s_oldSurfaceName != s_surfaceName) {
                                    for (auto& page : s_pages) {
                                        for (auto& surface : page->surfaces) {
                                            if (surface->surface == s_oldSurfaceName)
                                                surface->surface = s_surfaceName;

                                            if (surface->pageSurface == s_oldSurfaceName)
                                                surface->pageSurface = s_surfaceName;
                                        }

                                        for (auto& broadcaster : page->broadcasters) {
                                            if (broadcaster->name == s_oldSurfaceName)
                                                broadcaster->name = s_surfaceName;

                                            for (auto& listener : broadcaster->listeners)
                                                if (listener->name == s_oldSurfaceName)
                                                    listener->name = s_surfaceName;
                                        }

                                        if (s_pageIndex >= 0) {
                                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_RESETCONTENT, 0, 0);

                                            for (auto& surface : s_pages[s_pageIndex]->surfaces)
                                                AddListEntry(hwndDlg, surface->pageSurface, IDC_LIST_PageSurfaces);

                                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_SETCURSEL, index, 0);
                                        }
                                    }
                                }
                            }

                            s_editMode = false;
                        }
                    }
                    break;

                case IDC_BUTTON_EditPage:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Pages, LB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Pages), LB_GETTEXT, index, (LPARAM) (LPCTSTR) (s_pageName.c_str()));

                            s_dlgResult = false;
                            s_editMode = true;

                            s_followMCP = s_pages[index]->followMCP;
                            s_synchPages = s_pages[index]->synchPages;
                            s_isScrollLinkEnabled = s_pages[index]->isScrollLinkEnabled;
                            s_scrollSynch = s_pages[index]->isScrollSynchEnabled;

                            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_Page), hwndDlg, dlgProcPage);
                            if (s_dlgResult == IDOK) {
                                s_pages[index]->name = s_pageName;
                                s_pages[index]->followMCP = s_followMCP;
                                s_pages[index]->synchPages = s_synchPages;
                                s_pages[index]->isScrollLinkEnabled = s_isScrollLinkEnabled;
                                s_pages[index]->isScrollSynchEnabled = s_scrollSynch;

                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Pages), LB_RESETCONTENT, 0, 0);
                                for (auto& page : s_pages)
                                    AddListEntry(hwndDlg, page->name, IDC_LIST_Pages);
                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Pages), LB_SETCURSEL, index, 0);
                            }

                            s_editMode = false;
                        }
                    }
                    break;

                case IDC_BUTTON_Advanced:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_AdvancedSetup), hwndDlg, dlgProcAdvancedSetup);
                    }
                    break;

                case IDC_BUTTON_EditPageSurface:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_PageSurfaces, LB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            s_dlgResult = false;
                            s_editMode = true;

                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_GETTEXT, index, (LPARAM) (LPCTSTR) (s_pageSurfaceFolder.c_str()));

                            int pageIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Pages, LB_GETCURSEL, 0, 0);
                            if (pageIndex >= 0) {
                                s_channelOffset = s_pages[pageIndex]->surfaces[index]->channelOffset;
                                s_pageSurface = s_pages[pageIndex]->surfaces[index]->pageSurface;
                                s_pageSurfaceFolder = s_pages[pageIndex]->surfaces[index]->pageSurfaceFolder;
                                s_pageSurfaceZoneFolder = s_pages[pageIndex]->surfaces[index]->pageSurfaceZoneFolder;
                                s_pageSurfaceFXZoneFolder = s_pages[pageIndex]->surfaces[index]->pageSurfaceFXZoneFolder;

                                DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_PageSurface), hwndDlg, dlgProcPageSurface);

                                if (s_dlgResult == IDOK) {
                                    s_pages[pageIndex]->surfaces[index]->channelOffset = s_channelOffset;
                                    s_pages[pageIndex]->surfaces[index]->pageSurfaceFolder = s_pageSurfaceFolder;
                                    s_pages[pageIndex]->surfaces[index]->pageSurfaceZoneFolder = s_pageSurfaceZoneFolder;
                                    s_pages[pageIndex]->surfaces[index]->pageSurfaceFXZoneFolder = s_pageSurfaceFXZoneFolder;
                                    s_pages[pageIndex]->surfaces[index]->pageSurface = s_pageSurface;
                                    SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_RESETCONTENT, 0, 0);

                                    for (auto& surface : s_pages[pageIndex]->surfaces)
                                        AddListEntry(hwndDlg, surface->pageSurface, IDC_LIST_PageSurfaces);

                                    SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_SETCURSEL, index, 0);
                                }
                            }

                            s_editMode = false;
                        }
                    }
                    break;

                case IDC_BUTTON_RemoveSurface:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Surfaces, LB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            string deletedSurface = s_surfaces[index]->name;

                            s_surfaces.erase(s_surfaces.begin() + index);

                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Surfaces), LB_RESETCONTENT, 0, 0);
                            for (auto& surface : s_surfaces)
                                AddListEntry(hwndDlg, surface->name, IDC_LIST_Surfaces);
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Surfaces), LB_SETCURSEL, index, 0);

                            if (s_pageIndex >= 0) {
                                for (int i = (int) s_pages[s_pageIndex]->broadcasters.size() - 1; i >= 0; --i) {
                                    if (s_pages[s_pageIndex]->broadcasters[i]->name == deletedSurface) {
                                        s_pages[s_pageIndex]->broadcasters[i]->listeners.clear();
                                        s_pages[s_pageIndex]->broadcasters.erase(s_pages[s_pageIndex]->broadcasters.begin() + i);
                                    } else {
                                        for (int k = (int) s_pages[s_pageIndex]->broadcasters[i]->listeners.size() - 1; k >= 0; --k)
                                            if (s_pages[s_pageIndex]->broadcasters[i]->listeners[k]->name == deletedSurface)
                                                s_pages[s_pageIndex]->broadcasters[i]->listeners.erase(s_pages[s_pageIndex]->broadcasters[i]->listeners.begin() + k);
                                    }
                                }

                                for (int i = (int) s_pages[s_pageIndex]->surfaces.size() - 1; i >= 0; --i)
                                    if (s_pages[s_pageIndex]->surfaces[i]->pageSurface == deletedSurface)
                                        s_pages[s_pageIndex]->surfaces.erase(s_pages[s_pageIndex]->surfaces.begin() + i);

                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_RESETCONTENT, 0, 0);

                                for (auto& surface : s_pages[s_pageIndex]->surfaces)
                                    AddListEntry(hwndDlg, surface->pageSurface, IDC_LIST_PageSurfaces);

                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_SETCURSEL, index, 0);
                            }
                        }
                    }
                    break;

                case IDC_BUTTON_RemovePage:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int index = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Pages, LB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            s_pages.erase(s_pages.begin() + index);

                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Pages), LB_RESETCONTENT, 0, 0);
#ifdef WIN32
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_RESETCONTENT, 0, 0);
#endif
                            for (auto& page : s_pages)
                                AddListEntry(hwndDlg, page->name, IDC_LIST_Pages);
                            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Pages), LB_SETCURSEL, index, 0);
                        }
                    }
                    break;

                case IDC_BUTTON_RemovePageSurface:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        int pageIndex = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_Pages, LB_GETCURSEL, 0, 0);
                        if (pageIndex >= 0) {
                            int index = (int) SendDlgItemMessage(hwndDlg, IDC_LIST_PageSurfaces, LB_GETCURSEL, 0, 0);
                            if (index >= 0) {
                                s_pages[pageIndex]->surfaces.erase(s_pages[pageIndex]->surfaces.begin() + index);

                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_RESETCONTENT, 0, 0);

                                for (auto& surface : s_pages[pageIndex]->surfaces)
                                    AddListEntry(hwndDlg, surface->pageSurface, IDC_LIST_PageSurfaces);
                                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_SETCURSEL, index, 0);
                            }
                        }
                    }
                    break;
            }
        } break;

        case WM_INITDIALOG: {
            s_hwndMainDlg = hwndDlg;

            const string iniFilePath = ProductPaths::FromReaperResourcePath().ConfigFile().string();

            ifstream iniFile(iniFilePath);

            int lineNumber = 0;

            for (string line; getline(iniFile, line);) {
                TrimLine(line);

                lineNumber++;

                if (lineNumber == 1) {
                    PropertyList pList;
                    vector<string> properties;
                    properties.push_back(line.c_str());
                    GetPropertiesFromTokens(0, 1, properties, pList);

                    const char* versionProp = pList.get_prop(PropertyType_Version);
                    if (versionProp) {
                        if (!IsSameString(versionProp, s_MajorVersionToken)) {
                            LogToConsole("[ERROR] %s version mismatch. The configuration version is not %s.\n", ProductIdentity::ConfigFilename, s_MajorVersionToken);
                            //FIXME: so what? make backup and generate new, or at least prompt to confirm
                            iniFile.close();
                            break;
                        } else
                            continue;
                    }
                }

                if (!IsCommentedOrEmpty(line)) {
                    vector<string> tokens;
                    GetTokens(tokens, line.c_str());

                    PropertyList pList;
                    vector<string> properties;
                    properties.push_back(line.c_str());
                    GetPropertiesFromTokens(0, (int) tokens.size(), tokens, pList);

                    if (const char* surfaceTypeProp = pList.get_prop(PropertyType_SurfaceType)) {
                        if (const char* surfaceNameProp = pList.get_prop(PropertyType_SurfaceName)) {
                            if (const char* surfaceChannelCountProp = pList.get_prop(PropertyType_SurfaceChannelCount)) {
                                if (IsSameString(surfaceTypeProp, s_MidiSurfaceToken) && tokens.size() == 7) {
                                    if (pList.get_prop(PropertyType_MidiInput) != NULL && pList.get_prop(PropertyType_MidiOutput) != NULL && pList.get_prop(PropertyType_MIDISurfaceRefreshRate) != NULL && pList.get_prop(PropertyType_MaxMIDIMesssagesPerRun) != NULL) {
                                        s_surfaces.push_back(make_unique<SurfaceLine>(surfaceTypeProp, surfaceNameProp, atoi(surfaceChannelCountProp), atoi(pList.get_prop(PropertyType_MidiInput)), atoi(pList.get_prop(PropertyType_MidiOutput)), atoi(pList.get_prop(PropertyType_MIDISurfaceRefreshRate)), atoi(pList.get_prop(PropertyType_MaxMIDIMesssagesPerRun))));

                                        AddListEntry(hwndDlg, s_surfaces.back().get()->name, IDC_LIST_Surfaces);
                                    }
                                } else if ((IsSameString(surfaceTypeProp, s_OSCSurfaceToken) || IsSameString(surfaceTypeProp, s_OSCX32SurfaceToken)) && tokens.size() == 7) {
                                    if (pList.get_prop(PropertyType_ReceiveOnPort) != NULL && pList.get_prop(PropertyType_TransmitToPort) != NULL && pList.get_prop(PropertyType_TransmitToIPAddress) != NULL && pList.get_prop(PropertyType_MaxPacketsPerRun) != NULL) {
                                        s_surfaces.push_back(make_unique<SurfaceLine>(surfaceTypeProp, surfaceNameProp, atoi(surfaceChannelCountProp), atoi(pList.get_prop(PropertyType_ReceiveOnPort)), atoi(pList.get_prop(PropertyType_TransmitToPort)), 0, atoi(pList.get_prop(PropertyType_MaxPacketsPerRun)), pList.get_prop(PropertyType_TransmitToIPAddress)));

                                        AddListEntry(hwndDlg, s_surfaces.back().get()->name, IDC_LIST_Surfaces);
                                    }
                                }
                            }
                        }
                    }

                    else if (const char* pageNameProp = pList.get_prop(PropertyType_PageName)) {
                        bool followMCP = true;
                        bool synchPages = true;
                        bool isScrollLinkEnabled = false;
                        bool isScrollSynchEnabled = false;

                        if (const char* pageFollowsMCPProp = pList.get_prop(PropertyType_PageFollowsMCP)) {
                            if (IsSameString(pageFollowsMCPProp, "No"))
                                followMCP = false;
                        }

                        if (const char* synchPagesProp = pList.get_prop(PropertyType_SynchPages)) {
                            if (IsSameString(synchPagesProp, "No"))
                                synchPages = false;
                        }

                        if (const char* scrollLinkProp = pList.get_prop(PropertyType_ScrollLink)) {
                            if (IsSameString(scrollLinkProp, "Yes"))
                                isScrollLinkEnabled = true;
                        }

                        if (const char* scrollSynchProp = pList.get_prop(PropertyType_ScrollSynch)) {
                            if (IsSameString(scrollSynchProp, "Yes"))
                                isScrollSynchEnabled = true;
                        }

                        s_pages.push_back(make_unique<PageLine>());

                        PageLine* page = s_pages.back().get();

                        page->name = pageNameProp;
                        page->followMCP = followMCP;
                        page->synchPages = synchPages;
                        page->isScrollLinkEnabled = isScrollLinkEnabled;
                        page->isScrollSynchEnabled = isScrollSynchEnabled;

                        AddListEntry(hwndDlg, page->name, IDC_LIST_Pages);
                    } else if (const char* broadcasterNameProp = pList.get_prop(PropertyType_Broadcaster)) {
                        if (s_pages.size() > 0) {
                            s_pages[s_pages.size() - 1]->broadcasters.push_back(make_unique<Broadcaster>());
                            s_pages[s_pages.size() - 1]->broadcasters.back().get()->name = broadcasterNameProp;
                        }
                    } else if (const char* listenerProp = pList.get_prop(PropertyType_Listener)) {
                        if (tokens.size() > 0 && s_pages.size() > 0 && s_pages[s_pages.size() - 1]->broadcasters.size() > 0) {
                            s_pages[s_pages.size() - 1]->broadcasters[s_pages[s_pages.size() - 1]->broadcasters.size() - 1]->listeners.push_back(make_unique<Listener>(listenerProp));

                            Listener* listener = s_pages[s_pages.size() - 1]->broadcasters[s_pages[s_pages.size() - 1]->broadcasters.size() - 1]->listeners.back().get();

                            if (const char* listenerProp = pList.get_prop(PropertyType_GoHome))
                                if (IsSameString(listenerProp, "Yes"))
                                    listener->goHome = true;

                            if (const char* listenerProp = pList.get_prop(PropertyType_Modifiers))
                                if (IsSameString(listenerProp, "Yes"))
                                    listener->modifiers = true;

                            if (const char* listenerProp = pList.get_prop(PropertyType_FXMenu))
                                if (IsSameString(listenerProp, "Yes"))
                                    listener->fxMenu = true;

                            if (const char* listenerProp = pList.get_prop(PropertyType_SelectedTrackFX))
                                if (IsSameString(listenerProp, "Yes"))
                                    listener->selectedTrackFX = true;

                            if (const char* listenerProp = pList.get_prop(PropertyType_SelectedTrackSends))
                                if (IsSameString(listenerProp, "Yes"))
                                    listener->sends = true;

                            if (const char* listenerProp = pList.get_prop(PropertyType_SelectedTrackReceives))
                                if (IsSameString(listenerProp, "Yes"))
                                    listener->receives = true;
                        }
                    } else if (const char* surfaceProp = pList.get_prop(PropertyType_Surface)) {
                        if (const char* surfaceFolderProp = pList.get_prop(PropertyType_SurfaceFolder)) {
                            if (s_pages.size() > 0) {
                                s_pages[s_pages.size() - 1]->surfaces.push_back(make_unique<PageSurfaceLine>());
                                PageSurfaceLine* surface = s_pages[s_pages.size() - 1]->surfaces.back().get();

                                surface->pageSurface = surfaceProp;
                                surface->pageSurfaceFolder = surfaceFolderProp;

                                if (const char* surfaceZoneFolderProp = pList.get_prop(PropertyType_ZoneFolder))
                                    surface->pageSurfaceZoneFolder = surfaceZoneFolderProp[0] == '\0' ? surfaceFolderProp : surfaceZoneFolderProp;
                                else
                                    surface->pageSurfaceZoneFolder = surfaceFolderProp;

                                if (const char* surfaceFXZoneFolderProp = pList.get_prop(PropertyType_FXZoneFolder))
                                    surface->pageSurfaceFXZoneFolder = surfaceFXZoneFolderProp[0] == '\0' ? surfaceFolderProp : surfaceFXZoneFolderProp;
                                else
                                    surface->pageSurfaceFXZoneFolder = surfaceFolderProp;

                                if (const char* assignedSurfaceStartChannelProp = pList.get_prop(PropertyType_StartChannel))
                                    surface->channelOffset = atoi(assignedSurfaceStartChannelProp);
                            }
                        }
                    }
                }
            }

            if (s_pages.size() == 0) {
                s_pages.push_back(make_unique<PageLine>());

                PageLine* page = s_pages.back().get();

                page->name = "Home";
                page->followMCP = false;
                page->synchPages = false;
                page->isScrollLinkEnabled = false;
                page->isScrollSynchEnabled = false;

                AddListEntry(hwndDlg, page->name, IDC_LIST_Pages);
            }

            if (s_surfaces.size() > 0)
                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Surfaces), LB_SETCURSEL, 0, 0);

            if (s_pages.size() > 0) {
                SendMessage(GetDlgItem(hwndDlg, IDC_LIST_Pages), LB_SETCURSEL, 0, 0);

                // the messages above don't trigger the user-initiated code, so pretend the user selected them
                SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDC_LIST_Pages, LBN_SELCHANGE), 0);

                if (s_pages[0]->surfaces.size() > 0)
                    SendMessage(GetDlgItem(hwndDlg, IDC_LIST_PageSurfaces), LB_SETCURSEL, 0, 0);
            }
        } break;

        case WM_DESTROY: {
            s_surfaces.clear();

            for (auto& page : s_pages) {
                page->surfaces.clear();

                for (auto& broadcaster : page->broadcasters)
                    broadcaster->listeners.clear();

                page->broadcasters.clear();
            }

            s_pages.clear();
        } break;

        case WM_USER + 1024: {
            const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
            try {
                productPaths.EnsureUserDirectories();
            } catch (const std::exception& error) {
                LogToConsole("[ERROR] Failed to create product configuration directories: %s\n", error.what());
                break;
            }
            FILE* iniFile = fopenUTF8(productPaths.ConfigFile().string().c_str(), "wb");

            if (iniFile) {
                PropertyList plist;

                fprintf(iniFile, "%s=%s\n", plist.string_from_prop(PropertyType_Version), s_MajorVersionToken);

                fprintf(iniFile, "\n");

                for (auto& surface : s_surfaces) {
                    string type = surface->type;

                    if (type == s_MidiSurfaceToken) {
                        fprintf(iniFile, "%s=%s %s=%s %s=%d %s=%d %s=%d ", plist.string_from_prop(PropertyType_SurfaceType), surface->type.c_str(), plist.string_from_prop(PropertyType_SurfaceName), surface->name.c_str(), plist.string_from_prop(PropertyType_SurfaceChannelCount), surface->channelCount, plist.string_from_prop(PropertyType_MidiInput), surface->inPort, plist.string_from_prop(PropertyType_MidiOutput), surface->outPort);

                        int refreshRate = surface->surfaceRefreshRate < 1 ? s_surfaceDefaultRefreshRate : surface->surfaceRefreshRate;
                        fprintf(iniFile, "%s=%d ", plist.string_from_prop(PropertyType_MIDISurfaceRefreshRate), refreshRate);

                        int maxSysExMessagesPerRun = surface->surfaceMaxSysExMessagesPerRun < 1 ? s_surfaceDefaultMaxSysExMessagesPerRun : surface->surfaceMaxSysExMessagesPerRun;
                        fprintf(iniFile, "%s=%d ", plist.string_from_prop(PropertyType_MaxMIDIMesssagesPerRun), maxSysExMessagesPerRun);
                    }

                    else if (type == s_OSCSurfaceToken || type == s_OSCX32SurfaceToken) {
                        fprintf(iniFile, "%s=%s %s=%s %s=%d %s=%d %s=%d ", plist.string_from_prop(PropertyType_SurfaceType), surface->type.c_str(), plist.string_from_prop(PropertyType_SurfaceName), surface->name.c_str(), plist.string_from_prop(PropertyType_SurfaceChannelCount), surface->channelCount, plist.string_from_prop(PropertyType_ReceiveOnPort), surface->inPort, plist.string_from_prop(PropertyType_TransmitToPort), surface->outPort);

                        fprintf(iniFile, "%s=%s ", plist.string_from_prop(PropertyType_TransmitToIPAddress), surface->remoteDeviceIP.c_str());

                        int maxPacketsPerRun = surface->surfaceMaxPacketsPerRun < 0 ? s_surfaceDefaultMaxPacketsPerRun : surface->surfaceMaxPacketsPerRun;

                        fprintf(iniFile, "%s=%d ", plist.string_from_prop(PropertyType_MaxPacketsPerRun), maxPacketsPerRun);
                    }

                    fprintf(iniFile, "\n");
                }

                fprintf(iniFile, "\n");

                for (auto& page : s_pages) {
                    fprintf(iniFile, "%s=%s", plist.string_from_prop(PropertyType_PageName), page->name.c_str());

                    fprintf(iniFile, " %s=%s", plist.string_from_prop(PropertyType_PageFollowsMCP), page->followMCP == true ? "Yes" : "No");

                    fprintf(iniFile, " %s=%s", plist.string_from_prop(PropertyType_SynchPages), page->synchPages == true ? "Yes" : "No");

                    fprintf(iniFile, " %s=%s", plist.string_from_prop(PropertyType_ScrollLink), page->isScrollLinkEnabled == true ? "Yes" : "No");

                    fprintf(iniFile, " %s=%s", plist.string_from_prop(PropertyType_ScrollSynch), page->isScrollSynchEnabled == true ? "Yes" : "No");

                    fprintf(iniFile, "\n");

                    for (auto& surface : page->surfaces) {
                        fprintf(iniFile, "\t%s=%s %s=%s %s=%s %s=%s %s=%d\n", plist.string_from_prop(PropertyType_Surface), surface->pageSurface.c_str(), plist.string_from_prop(PropertyType_SurfaceFolder), surface->pageSurfaceFolder.c_str(), plist.string_from_prop(PropertyType_ZoneFolder), surface->pageSurfaceZoneFolder.c_str(), plist.string_from_prop(PropertyType_FXZoneFolder), surface->pageSurfaceFXZoneFolder.c_str(), plist.string_from_prop(PropertyType_StartChannel), surface->channelOffset);
                    }

                    fprintf(iniFile, "\n");

                    for (auto& broadcaster : page->broadcasters) {
                        if (broadcaster->listeners.size() == 0)
                            continue;

                        fprintf(iniFile, "\t%s=%s\n", plist.string_from_prop(PropertyType_Broadcaster), broadcaster->name.c_str());

                        for (auto& listener : broadcaster->listeners) {
                            fprintf(iniFile, "\t%s=%s ", plist.string_from_prop(PropertyType_Listener), listener->name.c_str());

                            fprintf(iniFile, "%s=%s ", plist.string_from_prop(PropertyType_GoHome), listener->goHome == true ? "Yes" : "No");
                            fprintf(iniFile, "%s=%s ", plist.string_from_prop(PropertyType_SelectedTrackSends), listener->sends == true ? "Yes" : "No");
                            fprintf(iniFile, "%s=%s ", plist.string_from_prop(PropertyType_SelectedTrackReceives), listener->receives == true ? "Yes" : "No");
                            fprintf(iniFile, "%s=%s ", plist.string_from_prop(PropertyType_FXMenu), listener->fxMenu == true ? "Yes" : "No");
                            fprintf(iniFile, "%s=%s ", plist.string_from_prop(PropertyType_Modifiers), listener->modifiers == true ? "Yes" : "No");
                            fprintf(iniFile, "%s=%s ", plist.string_from_prop(PropertyType_SelectedTrackFX), listener->selectedTrackFX == true ? "Yes" : "No");

                            fprintf(iniFile, "\n");
                        }

                        fprintf(iniFile, "\n");
                    }
                }

                fclose(iniFile);
            }
        } break;
    }

    return 0;
}
#endif
