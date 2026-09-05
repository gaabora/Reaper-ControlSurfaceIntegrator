#pragma once

#include "preamble.h"

struct ZoneFileCreateRequest {
    string documentType;
    string zoneName;
    string alias;
    string purpose;
    string matchFx;
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
    static bool IsSafeText(const string& value, size_t maximumLength);
    static bool ResolveMetadata(const ZoneFileCreateRequest& request, bool& isFx, string& metadata, string& errorMessage);
    static bool HasCaseInsensitiveFile(const filesystem::path& directory, const string& fileName);
    static bool FindCaseInsensitiveZoneId(const filesystem::path& collectionRoot, const string& zoneName, filesystem::path& duplicatePath);
    static bool ValidateCandidate(const string& profileId, const filesystem::path& targetPath, bool isFx, const string& source, string& errorMessage);
    static bool WriteCompletedTemporaryFile(const filesystem::path& targetPath, const string& source, string& errorMessage);

public:
    static ZoneFileCreateResult Create(ZoneManager* zoneManager, const ZoneFileCreateRequest& request);
};
