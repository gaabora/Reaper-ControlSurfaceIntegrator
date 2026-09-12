#ifdef CSI_UI_INCLUDE_CONFIG_DIALOGS
WDL_DLGRET dlgProcMainConfig(HWND, UINT message, WPARAM wParam, LPARAM) {
    if (message == WM_COMMAND && LOWORD(wParam) == IDC_BUTTON_ControlPanel) {
        ControlPanelAction::OpenOrFocus();
        return 1;
    }
    return 0;
}
#endif
