#ifdef CSI_UI_INCLUDE_CONFIG_DIALOGS
WDL_DLGRET dlgProcMainConfig(HWND hwndDlg, UINT message, WPARAM, LPARAM) {
    if (message != WM_INITDIALOG) return 0;

    ControlPanelAction::OpenOrFocus();
    HWND parentWindow = GetParent(hwndDlg);
    if (parentWindow) PostMessage(parentWindow, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
    EndDialog(hwndDlg, 0);
    return 0;
}
#endif
