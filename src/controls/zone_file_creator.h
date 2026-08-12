#pragma once

#include "preamble.h"

struct ZoneFileCreateRequest {
    string scaffoldType;
    string zoneName;
    string alias;
    string navigator;
};

struct ZoneFileCreateResult {
    bool success = false;
    string path;
    string message;
};

class ZoneFileCreator
{
private:
    static string LowerAscii(const string& value);
    static bool IsContainedPath(const filesystem::path& root, const filesystem::path& candidate);
    static bool IsSafeZoneName(const string& value);
    static bool IsSafeAlias(const string& value);
    static bool IsSupportedNavigator(const string& value);
    static bool ResolveRelativeDirectory(const string& scaffoldType, filesystem::path& relativeDirectory, bool& isFx);
    static bool HasCaseInsensitiveFile(const filesystem::path& directory, const string& fileName);
    static bool FindDuplicateZoneName(const filesystem::path& profileRoot, const string& zoneName, filesystem::path& duplicatePath);
    static bool WriteCompletedTemporaryFile(const filesystem::path& targetPath, const string& source, string& errorMessage);

public:
    static ZoneFileCreateResult Create(ZoneManager* zoneManager, const ZoneFileCreateRequest& request);
};
