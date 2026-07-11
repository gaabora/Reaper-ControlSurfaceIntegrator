//
//  surface_parser.cpp — SurfaceTemplateParser implementation.
//
//  Parsing logic that was previously split across two private method pairs:
//    Midi_ControlSurface::ProcessMidiWidget / ProcessMIDIWidgetFile
//    OSC_ControlSurface::ProcessOSCWidget   / ProcessOSCWidgetFile
//  now lives here.  The surface member functions become one-line delegates.
//

#include "integrator.h"
#include "surface_parser.h"
#include "midi/midi_surface.h"
#include "midi/widget_factory.h"
#include "osc/osc_surface.h"
#include "osc/osc_widgets.h"
#include "../actions/reaper_actions.h"
#include "../actions/manager_actions.h"

// ---------------------------------------------------------------------------
// File-local helper: reads lines from an ifstream until 'endToken' is found.
// (Moved here from integrator.cpp — only used by surface template parsers.)
// ---------------------------------------------------------------------------
static vector<vector<string>> GetTokenLines(ifstream &file, string endToken,
                                             int &lastProcessedLine)
{
    vector<vector<string>> tokenLines;
    for (string line; getline(file, line);)
    {
        TrimLine(line);
        lastProcessedLine++;

        if (IsCommentedOrEmpty(line))
            continue;

        vector<string> tokens;
        GetTokens(tokens, line);

        if (tokens[0] == endToken)
            break;

        tokenLines.push_back(tokens);
    }
    return tokenLines;
}

// ===========================================================================
// MIDI
// ===========================================================================
void SurfaceTemplateParser::ParseMidiTemplate(const string &filePath,
                                              Midi_ControlSurface *surface)
{
    int lineNumber = 0;
    vector<vector<string>> valueLines;

    surface->stepSize_.clear();
    surface->accelerationValuesForDecrement_.clear();
    surface->accelerationValuesForIncrement_.clear();
    surface->accelerationValues_.clear();

    try
    {
        ifstream file(filePath);

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] ParseMidiTemplate: %s\n", GetRelativePath(filePath.c_str()));

        for (string line; getline(file, line);)
        {
            TrimLine(line);
            lineNumber++;

            if (IsCommentedOrEmpty(line))
                continue;

            // Strip inline # comments from Widget lines (used for OSK properties)
            string parseLine = line;
            if (parseLine.find("Widget ") == 0) {
                auto hashPos = parseLine.find('#');
                if (hashPos != string::npos)
                    parseLine = parseLine.substr(0, hashPos);
            }

            vector<string> tokens;
            GetTokens(tokens, parseLine.c_str());

            if (tokens.size() > 0 && tokens[0] != "Widget")
                valueLines.push_back(tokens);

            if (tokens.size() > 0 && tokens[0] == "AccelerationValuesEnd")
                surface->ProcessValues(valueLines);

            if (tokens.size() > 0 && tokens[0] == "Widget")
                ParseMidiWidget(lineNumber, file, tokens, surface);
        }
    }
    catch (const std::exception &e)
    {
        LogToConsole("[ERROR] FAILED to ParseMidiTemplate in %s, around line %d\n",
                     filePath.c_str(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }
}

void SurfaceTemplateParser::ParseMidiWidget(int &lineNumber, ifstream &file,
                                            const vector<string> &tokens,
                                            Midi_ControlSurface *surface)
{
    if (tokens.size() < 2)
        return;

    string widgetClass;
    if (tokens.size() > 2)
        widgetClass = tokens[2];

    surface->AddWidget(surface, tokens[1].c_str());

    Widget *widget = surface->GetWidgetByName(tokens[1]);
    if (widget == NULL)
    {
        LogToConsole("[ERROR] FAILED to ParseMidiWidget: no widget found by name '%s'. Line %d\n",
                     tokens[1].c_str(), lineNumber);
        return;
    }

    vector<vector<string>> tokenLines = GetTokenLines(file, "WidgetEnd", lineNumber);
    if (tokenLines.empty())
        return;

    for (int i = 0; i < (int)tokenLines.size(); ++i)
    {
        MidiWidgetContext ctx;
        ctx.csi         = surface->csi_;
        ctx.surface     = surface;
        ctx.widget      = widget;
        ctx.widgetClass = widgetClass;
        ctx.tokens      = tokenLines[i];
        ctx.size        = (int)tokenLines[i].size();

        if (ctx.size > 3)
        {
            ctx.message1.midi_message[0] = strToHex(tokenLines[i][1]);
            ctx.message1.midi_message[1] = strToHex(tokenLines[i][2]);
            ctx.message1.midi_message[2] = strToHex(tokenLines[i][3]);

            ctx.oneByteKey   = to_string(ctx.message1.midi_message[0] * 0x10000);
            ctx.twoByteKey   = to_string(ctx.message1.midi_message[0] * 0x10000 +
                                         ctx.message1.midi_message[1] * 0x100);
            ctx.threeByteKey = to_string(ctx.message1.midi_message[0] * 0x10000 +
                                         ctx.message1.midi_message[1] * 0x100 +
                                         ctx.message1.midi_message[2]);
        }
        if (ctx.size > 6)
        {
            ctx.message2.midi_message[0] = strToHex(tokenLines[i][4]);
            ctx.message2.midi_message[1] = strToHex(tokenLines[i][5]);
            ctx.message2.midi_message[2] = strToHex(tokenLines[i][6]);

            ctx.threeByteKeyMsg2 = to_string(ctx.message2.midi_message[0] * 0x10000 +
                                             ctx.message2.midi_message[1] * 0x100 +
                                             ctx.message2.midi_message[2]);
        }

        const string &widgetType = tokenLines[i][0];
        if (!MidiWidgetRegistry::Dispatch(widgetType, ctx))
            LogToConsole("[WARN] Unknown MIDI widget type '%s' in widget '%s'. Line %d\n",
                         widgetType.c_str(), tokens[1].c_str(), lineNumber);
    }
}

// ===========================================================================
// OSC
// ===========================================================================
void SurfaceTemplateParser::ParseOSCTemplate(const string &filePath,
                                             OSC_ControlSurface *surface)
{
    int lineNumber = 0;
    vector<vector<string>> valueLines;

    surface->stepSize_.clear();
    surface->accelerationValuesForDecrement_.clear();
    surface->accelerationValuesForIncrement_.clear();
    surface->accelerationValues_.clear();

    try
    {
        ifstream file(filePath);

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] ParseOSCTemplate: %s\n", GetRelativePath(filePath.c_str()));

        for (string line; getline(file, line);)
        {
            TrimLine(line);
            lineNumber++;

            if (IsCommentedOrEmpty(line))
                continue;

            // Strip inline # comments from Widget lines (used for OSK properties)
            string parseLine = line;
            if (parseLine.find("Widget ") == 0) {
                auto hashPos = parseLine.find('#');
                if (hashPos != string::npos)
                    parseLine = parseLine.substr(0, hashPos);
            }

            vector<string> tokens;
            GetTokens(tokens, parseLine);

            if (tokens.size() > 0 && tokens[0] != "Widget")
                valueLines.push_back(tokens);

            if (tokens.size() > 0 && tokens[0] == "AccelerationValuesEnd")
                surface->ProcessValues(valueLines);

            if (tokens.size() > 0 && tokens[0] == "Widget")
                ParseOSCWidget(lineNumber, file, tokens, surface);
        }
    }
    catch (const std::exception &e)
    {
        LogToConsole("[ERROR] FAILED to ParseOSCTemplate in %s, around line %d\n",
                     filePath.c_str(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }
}

void SurfaceTemplateParser::ParseOSCWidget(int &lineNumber, ifstream &file,
                                           const vector<string> &tokens,
                                           OSC_ControlSurface *surface)
{
    if (tokens.size() < 2)
        return;

    surface->AddWidget(surface, tokens[1].c_str());

    Widget *widget = surface->GetWidgetByName(tokens[1]);
    if (widget == NULL)
    {
        LogToConsole("[ERROR] FAILED to ParseOSCWidget: widget not found by name %s. Line %d\n",
                     tokens[1].c_str(), lineNumber);
        return;
    }

    vector<vector<string>> tokenLines = GetTokenLines(file, "WidgetEnd", lineNumber);

    for (int i = 0; i < (int)tokenLines.size(); ++i)
    {
        if (tokenLines[i].size() > 1 && tokenLines[i][0] == "Control")
            surface->CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1],
                make_unique<CSIMessageGenerator>(surface->csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "AnyPress")
            surface->CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1],
                make_unique<AnyPress_CSIMessageGenerator>(surface->csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "Touch")
            surface->CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1],
                make_unique<Touch_CSIMessageGenerator>(surface->csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "X32Fader")
            surface->CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1],
                make_unique<X32_Fader_OSC_MessageGenerator>(surface->csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "X32RotaryToEncoder")
            surface->CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1],
                make_unique<X32_RotaryToEncoder_OSC_MessageGenerator>(surface->csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_Processor")
            widget->GetFeedbackProcessors().push_back(
                make_unique<OSC_FeedbackProcessor>(surface->csi_, surface, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_IntProcessor")
            widget->GetFeedbackProcessors().push_back(
                make_unique<OSC_IntFeedbackProcessor>(surface->csi_, surface, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32Processor")
            widget->GetFeedbackProcessors().push_back(
                make_unique<OSC_X32FeedbackProcessor>(surface->csi_, surface, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32IntProcessor")
            widget->GetFeedbackProcessors().push_back(
                make_unique<OSC_X32IntFeedbackProcessor>(surface->csi_, surface, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32FaderProcessor")
            widget->GetFeedbackProcessors().push_back(
                make_unique<OSC_X32FaderFeedbackProcessor>(surface->csi_, surface, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32RotaryToEncoder")
            widget->GetFeedbackProcessors().push_back(
                make_unique<OSC_X32_RotaryToEncoderFeedbackProcessor>(surface->csi_, surface, widget, tokenLines[i][1]));
    }
}
