#pragma once

#include <cstdint>
#include <string>

#include "format2_syntax.h"

bool ParseFormat2IntegerScalar(const Format2ScalarSyntax& scalar, int& parsedValue);
bool ParseFormat2FiniteScalar(const Format2ScalarSyntax& scalar, double& parsedValue);
bool ParseFormat2ColorScalar(const Format2ScalarSyntax& scalar, std::uint32_t& parsedValue);
std::string ValidateFormat2ValueRule(const Format2PropertySyntax& property, const std::string& rule);
