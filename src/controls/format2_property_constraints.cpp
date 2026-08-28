#include "format2_property_constraints.h"

static const Format2PropertySyntax* FindFormat2ConstraintProperty(const std::vector<Format2PropertySyntax>& properties, const std::string& name) {
    for (const Format2PropertySyntax& property : properties) {
        if (property.name == name) return &property;
    }
    return nullptr;
}

static std::vector<std::string> SplitFormat2ConstraintArguments(const std::string& value) {
    std::vector<std::string> arguments;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t separator = value.find('|', start);
        arguments.push_back(value.substr(start, separator == std::string::npos ? std::string::npos : separator - start));
        if (separator == std::string::npos) break;
        start = separator + 1;
    }
    return arguments;
}

static bool ParseFormat2Constraint(const std::string& constraint, std::string& name, std::vector<std::string>& arguments) {
    const std::size_t leftParenthesis = constraint.find('(');
    if (leftParenthesis == std::string::npos || constraint.back() != ')') return false;
    name = constraint.substr(0, leftParenthesis);
    arguments = SplitFormat2ConstraintArguments(constraint.substr(leftParenthesis + 1, constraint.size() - leftParenthesis - 2));
    return !name.empty() && !arguments.empty();
}

static bool Format2ConstraintPropertyEquals(const Format2PropertySyntax* property, const std::string& expected) {
    return property && !property->value.list && !property->value.scalar.quoted && property->value.scalar.text == expected;
}

std::vector<Format2ConstraintViolation> ValidateFormat2PropertyConstraints(const std::vector<Format2PropertySyntax>& properties, const std::vector<std::string>& constraints, const Format2SourceLocation& ownerLocation) {
    std::vector<Format2ConstraintViolation> violations;
    for (const std::string& constraint : constraints) {
        std::string name;
        std::vector<std::string> arguments;
        if (!ParseFormat2Constraint(constraint, name, arguments)) {
            violations.push_back({ "Invalid Surface I/O catalog constraint: " + constraint, ownerLocation });
            continue;
        }
        if (name == "ExactlyOne" && arguments.size() >= 2) {
            int presentCount = 0;
            for (const std::string& argument : arguments) {
                if (FindFormat2ConstraintProperty(properties, argument)) presentCount++;
            }
            if (presentCount != 1) violations.push_back({ "Exactly one of " + arguments[0] + " or " + arguments[1] + " is required", ownerLocation });
            continue;
        }
        if (name == "AllOrNone" && arguments.size() >= 2) {
            int presentCount = 0;
            for (const std::string& argument : arguments) {
                if (FindFormat2ConstraintProperty(properties, argument)) presentCount++;
            }
            if (presentCount != 0 && presentCount != static_cast<int>(arguments.size())) violations.push_back({ arguments[0] + " and " + arguments[1] + " must be declared together", ownerLocation });
            continue;
        }
        if (name == "Requires" && arguments.size() == 2) {
            const Format2PropertySyntax* trigger = FindFormat2ConstraintProperty(properties, arguments[0]);
            if (trigger && !FindFormat2ConstraintProperty(properties, arguments[1])) violations.push_back({ arguments[0] + " requires " + arguments[1], trigger->nameLocation });
            continue;
        }
        if (name == "WhenEqualsRequire" && arguments.size() == 3) {
            const Format2PropertySyntax* trigger = FindFormat2ConstraintProperty(properties, arguments[0]);
            if (Format2ConstraintPropertyEquals(trigger, arguments[1]) && !FindFormat2ConstraintProperty(properties, arguments[2])) violations.push_back({ arguments[0] + "=" + arguments[1] + " requires " + arguments[2], trigger->nameLocation });
            continue;
        }
        if (name == "WhenPresentEquals" && arguments.size() == 3) {
            const Format2PropertySyntax* trigger = FindFormat2ConstraintProperty(properties, arguments[0]);
            const Format2PropertySyntax* required = FindFormat2ConstraintProperty(properties, arguments[1]);
            if (trigger && !Format2ConstraintPropertyEquals(required, arguments[2])) violations.push_back({ arguments[0] + " requires " + arguments[1] + "=" + arguments[2], trigger->nameLocation });
            continue;
        }
        violations.push_back({ "Unknown Surface I/O catalog constraint: " + constraint, ownerLocation });
    }
    return violations;
}
