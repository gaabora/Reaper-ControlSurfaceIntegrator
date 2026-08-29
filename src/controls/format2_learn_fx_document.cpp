#include "format2_learn_fx_document.h"

#include <algorithm>
#include <set>
#include <utility>

static std::string Format2LearnFxFilename(const std::string& sourcePath) {
    const std::size_t separator = sourcePath.find_last_of("/\\");
    return sourcePath.substr(separator == std::string::npos ? 0 : separator + 1);
}

static bool ParseFormat2LearnFxRole(const Format2Token& token, Format2LearnFxWidgetRole& role) {
    if (token.text == "Parameter") role = Format2LearnFxWidgetRole::Parameter;
    else if (token.text == "NameDisplay") role = Format2LearnFxWidgetRole::NameDisplay;
    else if (token.text == "ValueDisplay") role = Format2LearnFxWidgetRole::ValueDisplay;
    else return false;
    return true;
}

class Format2LearnFxDocumentParser {
public:
    Format2LearnFxDocumentParser(const std::string& source, const std::string& sourcePath) : sourcePath_(sourcePath) {
        this->result_.document = ParseFormat2DocumentSource(source, sourcePath, Format2DocumentKind::LearnFx);
    }

    Format2LearnFxParseResult Parse() {
        this->ValidateIdentity();
        for (const Format2SyntaxNode& node : this->result_.document.body) this->ParseTopLevelNode(node);
        if (!this->fxWidgetsSeen_) this->AddDiagnostic("format2.learn-fx.widgets.required", "LearnFX.fxzon requires one FXWidgets block", this->DocumentLocation());
        if (this->fxWidgetsSeen_ && this->parameterCount_ == 0) this->AddDiagnostic("format2.learn-fx.parameter.required", "FXWidgets requires at least one Parameter entry", this->fxWidgetsLocation_);
        return std::move(this->result_);
    }

private:
    std::string sourcePath_;
    Format2LearnFxParseResult result_;
    bool fxWidgetsSeen_ = false;
    bool generatedBindingsSeen_ = false;
    int parameterCount_ = 0;
    Format2SourceLocation fxWidgetsLocation_;

    void AddDiagnostic(const std::string& code, const std::string& message, const Format2SourceLocation& location) {
        this->result_.document.lexical.diagnostics.push_back({ code, message, location });
    }

    Format2SourceLocation DocumentLocation() const {
        if (!this->result_.document.metadata.entries.empty()) return this->result_.document.metadata.entries.front().nameLocation;
        if (!this->result_.document.lexical.tokens.empty()) return this->result_.document.lexical.tokens.front().location;
        return {};
    }

    void ValidateIdentity() {
        if (!this->sourcePath_.empty() && Format2LearnFxFilename(this->sourcePath_) != "LearnFX.fxzon") this->AddDiagnostic("format2.learn-fx.filename", "A Learn FX document must use the exact filename LearnFX.fxzon", this->DocumentLocation());
    }

    void ParseTopLevelNode(const Format2SyntaxNode& node) {
        if (node.kind != Format2SyntaxNodeKind::Block || node.positionalTokens.empty() || node.positionalTokens[0].kind != Format2TokenKind::Bare) {
            this->AddDiagnostic("format2.learn-fx.block", "LearnFX.fxzon accepts only FXWidgets and GeneratedBindings blocks", node.location);
            return;
        }
        const std::string& blockName = node.positionalTokens[0].text;
        if (blockName == "FXWidgets") {
            if (this->fxWidgetsSeen_) {
                this->AddDiagnostic("format2.learn-fx.widgets.duplicate", "FXWidgets can occur only once", node.location);
                return;
            }
            this->fxWidgetsSeen_ = true;
            this->fxWidgetsLocation_ = node.location;
            this->ParseFxWidgets(node);
            return;
        }
        if (blockName == "GeneratedBindings") {
            if (this->generatedBindingsSeen_) {
                this->AddDiagnostic("format2.learn-fx.generated.duplicate", "GeneratedBindings can occur only once", node.location);
                return;
            }
            this->generatedBindingsSeen_ = true;
            this->ParseGeneratedBindings(node);
            return;
        }
        this->AddDiagnostic("format2.learn-fx.block.unknown", "Unknown Learn FX block: " + blockName, node.positionalTokens[0].location);
    }

    void ValidateBlockHeader(const Format2SyntaxNode& node, const std::string& blockName) {
        if (node.positionalTokens.size() != 1 || !node.properties.empty()) this->AddDiagnostic("format2.learn-fx.block.header", blockName + " does not accept an ID or properties", node.location);
    }

    void ParseFxWidgets(const Format2SyntaxNode& node) {
        this->ValidateBlockHeader(node, "FXWidgets");
        std::set<std::string> selectors;
        for (const Format2SyntaxNode& child : node.children) {
            if (child.kind != Format2SyntaxNodeKind::Line || child.positionalTokens.size() != 2 || child.positionalTokens[0].kind != Format2TokenKind::Bare || child.positionalTokens[1].kind != Format2TokenKind::Bare) {
                this->AddDiagnostic("format2.learn-fx.widget.entry", "An FXWidgets entry requires one role and one Widget selector", child.location);
                continue;
            }
            Format2LearnFxWidget entry;
            entry.location = child.location;
            if (!ParseFormat2LearnFxRole(child.positionalTokens[0], entry.role)) {
                this->AddDiagnostic("format2.learn-fx.widget.role", "Unknown FXWidgets role: " + child.positionalTokens[0].text, child.positionalTokens[0].location);
                continue;
            }
            const Format2WidgetSelectorParseResult selectorResult = ParseFormat2WidgetSelector(child.positionalTokens[1], true);
            for (const Format2Diagnostic& diagnostic : selectorResult.diagnostics) this->AddDiagnostic(diagnostic.code, diagnostic.message, diagnostic.location);
            if (!selectorResult.selector) continue;
            entry.selector = *selectorResult.selector;
            entry.defaults = child.properties;
            this->ValidateDefaults(child, entry.defaults);
            if (!selectors.insert(entry.selector.source).second) this->AddDiagnostic("format2.learn-fx.widget.selector.duplicate", "FXWidgets repeats Widget selector " + entry.selector.source, entry.selector.location);
            if (entry.role == Format2LearnFxWidgetRole::Parameter) this->parameterCount_++;
            this->result_.learnFx.widgets.push_back(std::move(entry));
        }
    }

    void ValidateDefaults(const Format2SyntaxNode& node, const std::vector<Format2PropertySyntax>& defaults) {
        std::set<std::string> propertyNames;
        std::size_t firstPropertyOffset = node.endLocation.offset + 1;
        for (const Format2PropertySyntax& property : defaults) {
            firstPropertyOffset = (std::min)(firstPropertyOffset, property.nameLocation.offset);
            if (!IsValidFormat2Identifier(property.name)) this->AddDiagnostic("format2.learn-fx.widget.property.name", "FXWidgets default property is not a valid identifier: " + property.name, property.nameLocation);
            if (!propertyNames.insert(property.name).second) this->AddDiagnostic("format2.learn-fx.widget.property.duplicate", "FXWidgets default property is duplicated: " + property.name, property.nameLocation);
        }
        for (const Format2Token& token : node.positionalTokens) {
            if (token.location.offset > firstPropertyOffset) {
                this->AddDiagnostic("format2.learn-fx.widget.property.order", "FXWidgets default properties must follow the role and Widget selector", token.location);
                break;
            }
        }
    }

    void ParseGeneratedBindings(const Format2SyntaxNode& node) {
        this->ValidateBlockHeader(node, "GeneratedBindings");
        if (node.children.empty()) {
            this->AddDiagnostic("format2.learn-fx.generated.required", "GeneratedBindings requires at least one binding or lifecycle block", node.location);
            return;
        }
        Format2ZoneParseResult generated = ParseFormat2GeneratedBindings(node.children);
        for (const Format2Diagnostic& diagnostic : generated.document.lexical.diagnostics) this->AddDiagnostic(diagnostic.code, diagnostic.message, diagnostic.location);
        this->result_.learnFx.generatedBindings = std::move(generated.zone.bindings);
        this->result_.learnFx.generatedLifecycleBlocks = std::move(generated.zone.lifecycleBlocks);
        for (std::size_t bindingIdx = 0; bindingIdx < this->result_.learnFx.generatedBindings.size(); bindingIdx++) {
            const Format2ZoneBinding& binding = this->result_.learnFx.generatedBindings[bindingIdx];
            this->result_.learnFx.generatedOrder.push_back({ Format2LearnFxGeneratedKind::Binding, bindingIdx, binding.location });
        }
        for (std::size_t lifecycleIdx = 0; lifecycleIdx < this->result_.learnFx.generatedLifecycleBlocks.size(); lifecycleIdx++) {
            const Format2LifecycleBlock& lifecycle = this->result_.learnFx.generatedLifecycleBlocks[lifecycleIdx];
            this->result_.learnFx.generatedOrder.push_back({ Format2LearnFxGeneratedKind::Lifecycle, lifecycleIdx, lifecycle.location });
        }
        std::sort(this->result_.learnFx.generatedOrder.begin(), this->result_.learnFx.generatedOrder.end(), [](const Format2LearnFxGeneratedStatement& left, const Format2LearnFxGeneratedStatement& right) { return left.location.offset < right.location.offset; });
    }
};

Format2LearnFxParseResult ParseFormat2LearnFxDocumentSource(const std::string& source, const std::string& sourcePath) {
    Format2LearnFxDocumentParser parser(source, sourcePath);
    return parser.Parse();
}
