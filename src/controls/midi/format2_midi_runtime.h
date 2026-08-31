#pragma once

#include "../preamble.h"

class Midi_ControlSurface;
struct Format2MidiSysExTextPayloadItem;
struct Format2PropertySyntax;
struct Format2ScalarSyntax;
struct Format2SurfacePrimitive;

enum class Format2MidiRuntimeLoadResult {
    NotFormat2,
    Loaded,
    Rejected,
};

class Format2MidiRuntimeLoader
{
private:
    static const Format2PropertySyntax* FindProperty(const Format2SurfacePrimitive& primitive, const string& name);
    static bool ReadByte(const Format2ScalarSyntax& scalar, int& value);
    static bool ReadBytes(const Format2PropertySyntax* property, vector<int>& values);
    static bool ReadTextPayload(const Format2PropertySyntax* property, vector<Format2MidiSysExTextPayloadItem>& payload);
    static vector<string> MakeTokens(const string& type, const vector<int>& firstMessage, const vector<int>& secondMessage = {});
    static bool IsSupported(const Format2SurfacePrimitive& primitive);

public:
    static Format2MidiRuntimeLoadResult Load(const string& filePath, Midi_ControlSurface* surface);
};
