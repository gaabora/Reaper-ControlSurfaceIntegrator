#pragma once

#include "../preamble.h"

class OSC_ControlSurface;
struct Format2PropertySyntax;
struct Format2SurfacePrimitive;

enum class Format2OscRuntimeLoadResult {
    Loaded,
    Rejected,
};

class Format2OscRuntimeLoader
{
private:
    static const Format2PropertySyntax* FindProperty(const Format2SurfacePrimitive& primitive, const string& name);
    static string ReadAddress(const Format2SurfacePrimitive& primitive);
    static int ReadIntegerProperty(const Format2SurfacePrimitive& primitive, const string& name, int defaultValue);
    static bool IsSupported(const Format2SurfacePrimitive& primitive);

public:
    static Format2OscRuntimeLoadResult Load(const string& filePath, OSC_ControlSurface* surface);
};
