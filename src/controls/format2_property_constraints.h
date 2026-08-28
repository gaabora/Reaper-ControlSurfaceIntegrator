#pragma once

#include <string>
#include <vector>

#include "format2_syntax.h"

struct Format2ConstraintViolation {
    std::string message;
    Format2SourceLocation location;
};

std::vector<Format2ConstraintViolation> ValidateFormat2PropertyConstraints(const std::vector<Format2PropertySyntax>& properties, const std::vector<std::string>& constraints, const Format2SourceLocation& ownerLocation);
