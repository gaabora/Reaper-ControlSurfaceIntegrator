#include "settings_values.h"

#include <cstdlib>
#include <iostream>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    std::exit(1);
}

static void TestCompiledDefaults() {
    SettingsValues defaults;
    Require(defaults.GetString("DefaultModifierMode") == "Latch", "default modifier mode");
    Require(defaults.GetString("DefaultPseudoModifierMode") == "Latch", "default pseudo-modifier mode");
    Require(defaults.GetInteger("HoldDelayMs") == 1000, "default hold delay");
    Require(defaults.GetInteger("LongHoldDelayMs") == 2000, "default long hold delay");
    Require(defaults.GetString("DebugLevel") == "Error", "default debug level");
    Require(!defaults.GetBoolean("SurfaceInDisplay"), "default surface input display");
}

static void TestValidOverrides() {
    SettingsValues defaults;
    SettingOverrides overrides;
    overrides.values["DefaultModifierMode"] = "Momentary";
    overrides.values["HoldDelayMs"] = "750";
    overrides.values["LongHoldDelayMs"] = "1500";
    overrides.values["SurfaceInDisplay"] = "1";
    SettingsValues result;
    std::vector<SettingValidationIssue> issues;
    Require(defaults.TryApply(overrides, "Product", result, issues), "valid Product overrides");
    Require(result.GetString("DefaultModifierMode") == "Momentary", "enum override");
    Require(result.GetInteger("HoldDelayMs") == 750, "integer override");
    Require(result.GetBoolean("SurfaceInDisplay"), "boolean override");
}

static void TestInvalidBooleanOverride() {
    SettingsValues defaults;
    SettingOverrides overrides;
    overrides.values["SurfaceInDisplay"] = "true";
    SettingsValues result;
    std::vector<SettingValidationIssue> issues;
    Require(!defaults.TryApply(overrides, "Product", result, issues), "invalid boolean rejection");
    Require(!result.GetBoolean("SurfaceInDisplay"), "invalid boolean set is atomic");
}

static void TestAtomicInvalidOverride() {
    SettingsValues defaults;
    SettingOverrides overrides;
    overrides.values["DefaultModifierMode"] = "Sticky";
    overrides.values["HoldDelayMs"] = "750";
    SettingsValues result;
    std::vector<SettingValidationIssue> issues;
    Require(!defaults.TryApply(overrides, "Product", result, issues), "invalid enum rejection");
    Require(result.GetInteger("HoldDelayMs") == 1000, "invalid set is atomic");

    SettingOverrides structurallyInvalidOverrides;
    structurallyInvalidOverrides.valid = false;
    structurallyInvalidOverrides.values["HoldDelayMs"] = "750";
    SettingsValues structurallyInvalidResult;
    Require(!defaults.TryApply(structurallyInvalidOverrides, "Product", structurallyInvalidResult, issues), "structurally invalid scope rejection");
    Require(structurallyInvalidResult.GetInteger("HoldDelayMs") == 1000, "structurally invalid scope is atomic");
}

static void TestRangeAndRelationship() {
    SettingsValues defaults;
    SettingOverrides rangeOverrides;
    rangeOverrides.values["HoldDelayMs"] = "10";
    SettingsValues rangeResult;
    std::vector<SettingValidationIssue> rangeIssues;
    Require(!defaults.TryApply(rangeOverrides, "Product", rangeResult, rangeIssues), "range rejection");

    SettingOverrides relationshipOverrides;
    relationshipOverrides.values["HoldDelayMs"] = "1000";
    relationshipOverrides.values["LongHoldDelayMs"] = "500";
    SettingsValues relationshipResult;
    std::vector<SettingValidationIssue> relationshipIssues;
    Require(!defaults.TryApply(relationshipOverrides, "Device", relationshipResult, relationshipIssues), "relationship rejection");
}

static void TestProductAndDevicePrecedence() {
    SettingsValues defaults;
    SettingOverrides productOverrides;
    productOverrides.values["HoldDelayMs"] = "1200";
    productOverrides.values["LongHoldDelayMs"] = "2400";
    SettingsValues productSettings;
    std::vector<SettingValidationIssue> productIssues;
    Require(defaults.TryApply(productOverrides, "Product", productSettings, productIssues), "Product settings resolution");

    SettingOverrides deviceOverrides;
    deviceOverrides.values["HoldDelayMs"] = "800";
    SettingsValues deviceSettings;
    std::vector<SettingValidationIssue> deviceIssues;
    Require(productSettings.TryApply(deviceOverrides, "Device", deviceSettings, deviceIssues), "Device settings resolution");
    Require(deviceSettings.GetInteger("HoldDelayMs") == 800, "Device override wins");
    Require(deviceSettings.GetInteger("LongHoldDelayMs") == 2400, "Device inherits Product value");
}

int main() {
    TestCompiledDefaults();
    TestValidOverrides();
    TestAtomicInvalidOverride();
    TestInvalidBooleanOverride();
    TestRangeAndRelationship();
    TestProductAndDevicePrecedence();
    std::cout << "SettingsValues tests passed\n";
    return 0;
}
