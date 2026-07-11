//
//  control_surface_integrator.cpp
//  reaper_control_surface_integrator
//
//

#define INCLUDE_LOCALIZE_IMPORT_H
#include "integrator.h"
#include "midi/midi_widgets.h"
#include "midi/widget_factory.h"
#include "osc/osc_widgets.h"
#include "../actions/reaper_actions.h"
#include "../actions/manager_actions.h"

#include "../resource.h"

extern WDL_DLGRET dlgProcMainConfig(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

extern reaper_plugin_info_t *g_reaper_plugin_info;

extern void WidgetMoved(ZoneManager *zoneManager, Widget *widget, int modifier);

int g_minNumParamSteps = 1;
int g_maxNumParamSteps = 30;
int g_debugLevel = DEBUG_LEVEL_ERROR;
bool g_surfaceRawInDisplay;
bool g_surfaceInDisplay;
bool g_surfaceOutDisplay;

bool g_fxParamsWrite;

void GetPropertiesFromTokens(int start, int finish, const vector<string>& tokens, PropertyList& properties)
{
    for (int i = start; i < finish; ++i) {
        std::string_view token = tokens[i];
        auto eqPos = token.find('=');

        if (eqPos != std::string_view::npos && token.find('=', eqPos + 1) == std::string_view::npos) {
            std::string key = std::string(token.substr(0, eqPos));
            std::string value = std::string(token.substr(eqPos + 1));

            PropertyType prop = PropertyList::prop_from_string(key.c_str());
            if (prop != PropertyType_Unknown) {
                properties.set_prop(prop, value.c_str());
            } else {
                properties.set_prop(prop, token.data()); // unknown properties are preserved as Unknown, key=value pair
                if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole("[WARNING] CSI does not support property named %s\n", key.c_str());
            }
        }
    }
}

void GetSteppedValues(const vector<string> &params, int start_idx, double &deltaValue, vector<double> &acceleratedDeltaValues, double &rangeMinimum, double &rangeMaximum, vector<double> &steppedValues, vector<int> &acceleratedTickValues)
{
    int openSquareIndex = -1, closeSquareIndex = -1;
    
    for (int i = start_idx; i < params.size(); ++i)
        if (params[i] == "[")
        {
            openSquareIndex = i;
            break;
        }

    if (openSquareIndex < 0) return;
    
    for (int i = openSquareIndex + 1; i < params.size(); ++i)
        if (params[i] == "]")
        {
            closeSquareIndex = i;
            break;
        }
    
    if (closeSquareIndex > 0)
    {
        for (int i = openSquareIndex + 1; i < closeSquareIndex; ++i)
        {
            const char *str = params[i].c_str();

            if (str[0] == '(' && str[strlen(str)-1] == ')')
            {
                str++; // skip (

                // (1.0,2.0,3.0) -> acceleratedDeltaValues : mode = 2
                // (1.0) -> deltaValue : mode = 1
                // (1) or (1,2,3) -> acceleratedTickValues : mode = 0
                const int mode = strstr(str, ".") ? strstr(str, ",") ? 2 : 1 : 0;

                while (*str)
                {
                    if (mode == 0)
                    {
                        int v = 0;
                        if (WDL_NOT_NORMALLY(sscanf(str, "%d", &v) != 1)) break;
                        acceleratedTickValues.push_back(v);
                    }
                    else
                    {
                        double v = 0.0;
                        if (WDL_NOT_NORMALLY(sscanf(str,"%lf", &v) != 1)) break;
                        if (mode == 1)
                        {
                            deltaValue = v;
                            break;
                        }
                        acceleratedDeltaValues.push_back(v);
                    }

                    while (*str && *str != ',') str++;
                    if (*str == ',') str++;
                }
            }
            // todo: support 1-3 syntax? else if (!strstr(str,".") && str[0] != '-' && strstr(str,"-"))
            else
            {
                // 1.0>3.0 writes to rangeMinimum/rangeMaximum
                // 1 or 1.0 -> steppedValues
                double a = 0.0, b = 0.0;
                const int nmatch = sscanf(str, "%lf>%lf", &a, &b);

                if (nmatch == 2)
                {
                    rangeMinimum = wdl_min(a,b);
                    rangeMaximum = wdl_max(a,b);
                }
                else if (WDL_NORMALLY(nmatch == 1))
                {
                    steppedValues.push_back(a);
                }
            }
        }
    }
}

static double EnumSteppedValues(int numSteps, int stepNumber)
{
    return floor(stepNumber / (double)(numSteps - 1)  *100.0 + 0.5)  *0.01;
}

void GetParamStepsString(string& outputString, int numSteps) // appends to string 
{
    // When number of steps equals 1, users are typically looking to use a button to reset.
    // A halfway value (0.5) is chosen as a good reset value instead of the previous 0.1.
    if (numSteps == 1)
    {
        outputString = "0.5";
    }
    else
    {
        for (int i = 0; i < numSteps; ++i)
        {
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "%.2f", EnumSteppedValues(numSteps, i));
            WDL_remove_trailing_decimal_zeros(tmp, 0);
            lstrcatn(tmp, " ", sizeof(tmp));
            outputString += tmp;
        }
    }
}

void GetParamStepsValues(vector<double> &outputVector, int numSteps)
{
    outputVector.clear();

    for (int i = 0; i < numSteps; ++i)
        outputVector.push_back(EnumSteppedValues(numSteps, i));
}

void TrimLine(string &line)
{
    const string tmp = line;
    const char *p = tmp.c_str();

    // remove leading and trailing spaces
    // condense whitespace to single whitespace
    // stop copying at "//" (comment)
    line.clear();
    for (;;)
    {
        // advance over whitespace
        while (*p > 0 && isspace(*p))
            p++;

        // a single / at the beginning of a line indicates a comment
        if (!*p || p[0] == '/') break;

        if (line.length())
            line.append(" ",1);

        // copy non-whitespace to output
        while (*p && (*p < 0 || !isspace(*p)))
        {
           if (p[0] == '/' && p[1] == '/') break; // existing behavior, maybe not ideal, but a comment can start anywhere
           line.append(p++,1);
        }
    }
    if (!line.empty() && g_debugLevel > DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] %s\n", line.c_str());
}

void ReplaceAllWith(string &output, const char *charsToReplace, const char *replacement)
{
    // replace all occurences of
    // any char in charsToReplace
    // with replacement string
    const string tmp = output;
    const char *p = tmp.c_str();
    output.clear();

    while (*p)
    {
        if (strchr(charsToReplace, *p) != NULL)
        {
            output.append(replacement);
        }
        else
            output.append(p,1);
        p++;
    }
}

void GetTokens(vector<string> &tokens, const string &line) {
    bool insideQuote = false;
    string token;
    
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        
        if (c == '"') {
            insideQuote = !insideQuote;
            if (!insideQuote) {
                tokens.push_back(token);
                token.clear();
            }
        } else if (isspace(c) && !insideQuote) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    
    if (!token.empty())
        tokens.push_back(token);
}

void GetTokens(vector<string> &tokens, const string &line, char delimiter)
{
    istringstream iss(line);
    string token;
    while (getline(iss, token, delimiter))
        tokens.push_back(token);
}

vector<vector<string>> GetTokenLines(ifstream &file, string endToken, int &lastProcessedLine) {
    vector<vector<string>> tokenLines;
    for (string line; getline(file, line);) {
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

int strToHex(string &valueStr)
{
    return strtol(valueStr.c_str(), NULL, 16);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MidiPort
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    int port, refcnt;
    void *dev;
    
    MidiPort(int portidx, void *devptr) : port(portidx), refcnt(1), dev(devptr) { };
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi I/O Manager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static WDL_TypedBuf<MidiPort> s_midiInputs;
static WDL_TypedBuf<MidiPort> s_midiOutputs;

void ReleaseMidiInput(midi_Input *input)
{
    for (int i = 0; i < s_midiInputs.GetSize(); ++i)
        if (s_midiInputs.Get()[i].dev == (void*)input)
        {
            if (!--s_midiInputs.Get()[i].refcnt)
            {
                input->stop();
                delete input;
                s_midiInputs.Delete(i);
                break;
            }
        }
}

void ReleaseMidiOutput(midi_Output *output)
{
    for (int i = 0; i < s_midiOutputs.GetSize(); ++i)
        if (s_midiOutputs.Get()[i].dev == (void*)output)
        {
            if (!--s_midiOutputs.Get()[i].refcnt)
            {
                delete output;
                s_midiOutputs.Delete(i);
                break;
            }
        }
}

static midi_Input *GetMidiInputForPort(int inputPort)
{
    for (int i = 0; i < s_midiInputs.GetSize(); ++i)
        if (s_midiInputs.Get()[i].port == inputPort)
        {
            s_midiInputs.Get()[i].refcnt++;
            return (midi_Input *)s_midiInputs.Get()[i].dev;
        }
    
    midi_Input *newInput = CreateMIDIInput(inputPort);
    
    if (newInput)
    {
        newInput->start();
        MidiPort midiInputPort(inputPort, newInput);
        s_midiInputs.Add(midiInputPort);
    }
    
    return newInput;
}

static midi_Output *GetMidiOutputForPort(int outputPort)
{
    for (int i = 0; i < s_midiOutputs.GetSize(); ++i)
        if (s_midiOutputs.Get()[i].port == outputPort)
        {
            s_midiOutputs.Get()[i].refcnt++;
            return (midi_Output *)s_midiOutputs.Get()[i].dev;
        }
    
    midi_Output *newOutput = CreateMIDIOutput(outputPort, false, NULL);
    
    if (newOutput)
    {
        MidiPort midiOutputPort(outputPort, newOutput);
        s_midiOutputs.Add(midiOutputPort);
    }
    
    return newOutput;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct OSCSurfaceSocket
////////////////////////////////7/////////////////////////////////////////////////////////////////////////////////////////
{
    string surfaceName;
    oscpkt::UdpSocket *socket;
    int refcnt;
    
    OSCSurfaceSocket(const string &name, oscpkt::UdpSocket *s)
    {
        surfaceName = name;
        socket = s;
        refcnt = 1;
    }
    ~OSCSurfaceSocket()
    {
      delete socket;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC I/O Manager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static WDL_PtrList<OSCSurfaceSocket> s_inputSockets;
static WDL_PtrList<OSCSurfaceSocket> s_outputSockets;

static oscpkt::UdpSocket *GetInputSocketForPort(string surfaceName, int inputPort)
{
    for (int i = 0; i < s_inputSockets.GetSize(); ++i)
        if (s_inputSockets.Get(i)->surfaceName == surfaceName)
        {
            s_inputSockets.Get(i)->refcnt++;
            return s_inputSockets.Get(i)->socket; // return existing
        }
    
    // otherwise make new
    oscpkt::UdpSocket *newInputSocket = new oscpkt::UdpSocket();
    
    if (newInputSocket)
    {
        newInputSocket->bindTo(inputPort);
        
        if (! newInputSocket->isOk())
        {
            //cerr << "Error opening port " << PORT_NUM << ": " << inSocket_.errorMessage() << "\n";
            return NULL;
        }
        
        s_inputSockets.Add(new OSCSurfaceSocket(surfaceName, newInputSocket));
        return newInputSocket;
    }
    
    return NULL;
}

static oscpkt::UdpSocket *GetOutputSocketForAddressAndPort(const string &surfaceName, const string &address, int outputPort)
{
    for (int i = 0; i < s_outputSockets.GetSize(); ++i)
        if (s_outputSockets.Get(i)->surfaceName == surfaceName)
        {
            s_outputSockets.Get(i)->refcnt++;
            return s_outputSockets.Get(i)->socket; // return existing
        }
    
    // otherwise make new
    oscpkt::UdpSocket *newOutputSocket = new oscpkt::UdpSocket();
    
    if (newOutputSocket)
    {
        if ( ! newOutputSocket->connectTo(address, outputPort))
        {
            //cerr << "Error connecting " << remoteDeviceIP_ << ": " << outSocket_.errorMessage() << "\n";
            return NULL;
        }
        
        if ( ! newOutputSocket->isOk())
        {
            //cerr << "Error opening port " << outPort_ << ": " << outSocket_.errorMessage() << "\n";
            return NULL;
        }

        s_outputSockets.Add(new OSCSurfaceSocket(surfaceName, newOutputSocket));
        return newOutputSocket;
    }
    
    return NULL;
}

static void collectFilesOfType(const string &type, const string &searchPath, vector<string> &results)
{
    filesystem::path zonePath { searchPath };
    
    if (filesystem::exists(searchPath) && filesystem::is_directory(searchPath))
        for (auto &file : filesystem::recursive_directory_iterator(searchPath))
            if (file.path().extension() == type)
                results.push_back(file.path().string());
}

//////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
//////////////////////////////////////////////////////////////////////////////
void Midi_ControlSurface::ProcessMidiWidget(int &lineNumber, ifstream &surfaceTemplateFile, const vector<string> &in_tokens)
{
    if (in_tokens.size() < 2)
        return;

    string widgetClass;
    if (in_tokens.size() > 2)
        widgetClass = in_tokens[2];

    AddWidget(this, in_tokens[1].c_str());

    Widget *widget = GetWidgetByName(in_tokens[1]);
    if (widget == NULL)
    {
        LogToConsole("[ERROR] FAILED to ProcessMidiWidget: no widgets found by name '%s'. Line %d\n", in_tokens[1].c_str(), lineNumber);
        return;
    }

    vector<vector<string>> tokenLines = GetTokenLines(surfaceTemplateFile, "WidgetEnd", lineNumber);
    if (tokenLines.size() < 1)
        return;

    for (int i = 0; i < (int)tokenLines.size(); ++i)
    {
        MidiWidgetContext ctx;
        ctx.csi        = csi_;
        ctx.surface    = this;
        ctx.widget     = widget;
        ctx.widgetClass = widgetClass;
        ctx.tokens     = tokenLines[i];
        ctx.size       = (int)tokenLines[i].size();

        if (ctx.size > 3)
        {
            ctx.message1.midi_message[0] = strToHex(tokenLines[i][1]);
            ctx.message1.midi_message[1] = strToHex(tokenLines[i][2]);
            ctx.message1.midi_message[2] = strToHex(tokenLines[i][3]);

            ctx.oneByteKey   = to_string(ctx.message1.midi_message[0] * 0x10000);
            ctx.twoByteKey   = to_string(ctx.message1.midi_message[0] * 0x10000 + ctx.message1.midi_message[1] * 0x100);
            ctx.threeByteKey = to_string(ctx.message1.midi_message[0] * 0x10000 + ctx.message1.midi_message[1] * 0x100 + ctx.message1.midi_message[2]);
        }
        if (ctx.size > 6)
        {
            ctx.message2.midi_message[0] = strToHex(tokenLines[i][4]);
            ctx.message2.midi_message[1] = strToHex(tokenLines[i][5]);
            ctx.message2.midi_message[2] = strToHex(tokenLines[i][6]);

            ctx.threeByteKeyMsg2 = to_string(ctx.message2.midi_message[0] * 0x10000 + ctx.message2.midi_message[1] * 0x100 + ctx.message2.midi_message[2]);
        }

        const string &widgetType = tokenLines[i][0];
        if (!MidiWidgetRegistry::Dispatch(widgetType, ctx))
            LogToConsole("[WARN] Unknown MIDI widget type '%s' in widget '%s'. Line %d\n", widgetType.c_str(), in_tokens[1].c_str(), lineNumber);
    }
}

//////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurface
//////////////////////////////////////////////////////////////////////////////
void OSC_ControlSurface::ProcessOSCWidget(int &lineNumber, ifstream &surfaceTemplateFile, const vector<string> &in_tokens)
{
    if (in_tokens.size() < 2)
        return;
    
    AddWidget(this, in_tokens[1].c_str());

    Widget *widget = GetWidgetByName(in_tokens[1]);
    
    if (widget == NULL) {
        LogToConsole("[ERROR] FAILED to ProcessOSCWidget: widget not found by name %s. Line %d\n", in_tokens[1].c_str(), lineNumber);
        return;
    }
    
    vector<vector<string>> tokenLines = GetTokenLines(surfaceTemplateFile, "WidgetEnd", lineNumber);

    for (int i = 0; i < (int)tokenLines.size(); ++i)
    {
        if (tokenLines[i].size() > 1 && tokenLines[i][0] == "Control")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<CSIMessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "AnyPress")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<AnyPress_CSIMessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "Touch")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<Touch_CSIMessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "X32Fader")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<X32_Fader_OSC_MessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "X32RotaryToEncoder")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<X32_RotaryToEncoder_OSC_MessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_Processor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_FeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_IntProcessor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_IntFeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32Processor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_X32FeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32IntProcessor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_X32IntFeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32FaderProcessor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_X32FaderFeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32RotaryToEncoder")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_X32_RotaryToEncoderFeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
    }
}

//////////////////////////////////////////////////////////////////////////////
// ControlSurface
//////////////////////////////////////////////////////////////////////////////
void ControlSurface::ProcessValues(const vector<vector<string>> &lines)
{
    bool inStepSizes = false;
    bool inAccelerationValues = false;
        
    for (int i = 0; i < (int)lines.size(); ++i)
    {
        if (lines[i].size() > 0)
        {
            if (lines[i][0] == "StepSize")
            {
                inStepSizes = true;
                continue;
            }
            else if (lines[i][0] == "StepSizeEnd")
            {
                inStepSizes = false;
                continue;
            }
            else if (lines[i][0] == "AccelerationValues")
            {
                inAccelerationValues = true;
                continue;
            }
            else if (lines[i][0] == "AccelerationValuesEnd")
            {
                inAccelerationValues = false;
                continue;
            }

            if (lines[i].size() > 1)
            {
                const string &widgetClass = lines[i][0];
                
                if (inStepSizes)
                    stepSize_[widgetClass] = atof(lines[i][1].c_str());
                else if (lines[i].size() > 2 && inAccelerationValues)
                {
                    
                    if (lines[i][1] == "Dec")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValuesForDecrement_[widgetClass][strtol(lines[i][j].c_str(), NULL, 16)] = j - 2;
                    else if (lines[i][1] == "Inc")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValuesForIncrement_[widgetClass][strtol(lines[i][j].c_str(), NULL, 16)] = j - 2;
                    else if (lines[i][1] == "Val")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValues_[widgetClass].push_back(atof(lines[i][j].c_str()));
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
//////////////////////////////////////////////////////////////////////////////
void Midi_ControlSurface::ProcessMIDIWidgetFile(const string &filePath, Midi_ControlSurface *surface)
{
    int lineNumber = 0;
    vector<vector<string>> valueLines;
    
    stepSize_.clear();
    accelerationValuesForDecrement_.clear();
    accelerationValuesForIncrement_.clear();
    accelerationValues_.clear();

    try
    {
        ifstream file(filePath);
        
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] ProcessMIDIWidgetFile: %s\n", GetRelativePath(filePath.c_str()));
        for (string line; getline(file, line) ; )
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
                ProcessValues(valueLines);

            if (tokens.size() > 0 && (tokens[0] == "Widget"))
                ProcessMidiWidget(lineNumber, file, tokens);
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole("[ERROR] FAILED to ProcessMIDIWidgetFile in %s, around line %d\n", filePath.c_str(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }
}

//////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurface
//////////////////////////////////////////////////////////////////////////////
void OSC_ControlSurface::ProcessOSCWidgetFile(const string &filePath)
{
    int lineNumber = 0;
    vector<vector<string>> valueLines;
    
    stepSize_.clear();
    accelerationValuesForDecrement_.clear();
    accelerationValuesForIncrement_.clear();
    accelerationValues_.clear();

    try
    {
        ifstream file(filePath);
        
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] ProcessOSCWidgetFile: %s\n", GetRelativePath(filePath.c_str()));
        for (string line; getline(file, line) ; )
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
                ProcessValues(valueLines);

            if (tokens.size() > 0 && (tokens[0] == "Widget"))
                ProcessOSCWidget(lineNumber, file, tokens);
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole("[ERROR] FAILED to ProcessOSCWidgetFile in %s, around line %d\n", filePath.c_str(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSurfIntegrator
////////////////////////////////////////////////////////////////////////////////////////////////////////    void InitActionsDictionary() {
void CSurfIntegrator::InitActionsDictionary() {
  #define X(className, strName) actions_.insert({strName, std::make_unique<className>()});
    ACTION_TYPE_LIST(X)
  #undef X
}
void CSurfIntegrator::Init()
{
    pages_.clear();
    
    string currentBroadcaster;
    
    Page *currentPage = NULL;
    
    string CSIFolderPath = string(GetResourcePath()) + "/CSI";
    
    if ( ! filesystem::exists(CSIFolderPath))
    {
        LogToConsole("[ERROR] Missing CSI Folder. Please check your installation, cannot find %s\n", CSIFolderPath.c_str());
        return;
    }
    
    string iniFilePath = string(GetResourcePath()) + "/CSI/CSI.ini";
    
    if ( ! filesystem::exists(iniFilePath))
    {
        LogToConsole("[ERROR] Missing CSI.ini. Please check your installation, cannot find %s\n", iniFilePath.c_str());
        return;
    }

    
    int lineNumber = 0;
    
    try
    {
        ifstream iniFile(iniFilePath);
               
        for (string line; getline(iniFile, line) ; )
        {
            TrimLine(line);
            
            if (lineNumber == 0)
            {
                PropertyList pList;
                vector<string> properties;
                properties.push_back(line.c_str());
                GetPropertiesFromTokens(0, 1, properties, pList);

                const char *versionProp = pList.get_prop(PropertyType_Version);
                if (versionProp)
                {
                    if (!IsSameString(versionProp, s_MajorVersionToken)) {
                        LogToConsole("[ERROR] CSI.ini version mismatch. -- Your CSI.ini file is not %s.\n", s_MajorVersionToken);
                        //FIXME: so what? make backup and generate new, or at least prompt to confirm
                        iniFile.close();
                        return;
                    }
                    else
                    {
                        lineNumber++;
                        continue;
                    }
                }
                else
                {
                    LogToConsole("[ERROR] CSI.ini has no version.\n");
                    //FIXME: so what? generate new, or at least prompt to confirm
                    iniFile.close();
                    return;
                }
            }
            
            if (IsCommentedOrEmpty(line))
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line.c_str());
            
            if (tokens.size() > 0) {
                PropertyList pList;
                GetPropertiesFromTokens(0, (int) tokens.size(), tokens, pList);
                
                if (const char *typeProp = pList.get_prop(PropertyType_SurfaceType))
                {
                    if (const char *nameProp = pList.get_prop(PropertyType_SurfaceName))
                    {
                        if (const char *channelCountProp = pList.get_prop(PropertyType_SurfaceChannelCount))
                        {
                            int channelCount = atoi(channelCountProp);
                            
                            if (IsSameString(typeProp, s_MidiSurfaceToken) && tokens.size() == 7)
                            {
                                if (pList.get_prop(PropertyType_MidiInput) != NULL &&
                                    pList.get_prop(PropertyType_MidiOutput) != NULL &&
                                    pList.get_prop(PropertyType_MIDISurfaceRefreshRate) != NULL &&
                                    pList.get_prop(PropertyType_MaxMIDIMesssagesPerRun) != NULL)
                                {
                                    int midiIn = atoi(pList.get_prop(PropertyType_MidiInput));
                                    int midiOut = atoi(pList.get_prop(PropertyType_MidiOutput));
                                    int surfaceRefreshRate = atoi(pList.get_prop(PropertyType_MIDISurfaceRefreshRate));
                                    int maxMIDIMesssagesPerRun = atoi(pList.get_prop(PropertyType_MaxMIDIMesssagesPerRun));
                                    
                                    midiSurfacesIO_.push_back(make_unique<Midi_ControlSurfaceIO>(this, nameProp, channelCount, GetMidiInputForPort(midiIn), GetMidiOutputForPort(midiOut), surfaceRefreshRate, maxMIDIMesssagesPerRun));
                                }
                            }
                            else if ((IsSameString(typeProp, s_OSCSurfaceToken) || IsSameString(typeProp, s_OSCX32SurfaceToken)) && tokens.size() == 7)
                            {
                                if (pList.get_prop(PropertyType_ReceiveOnPort) != NULL &&
                                    pList.get_prop(PropertyType_TransmitToPort) != NULL &&
                                    pList.get_prop(PropertyType_TransmitToIPAddress) != NULL &&
                                    pList.get_prop(PropertyType_MaxPacketsPerRun) != NULL)
                                {
                                    const char *receiveOnPort = pList.get_prop(PropertyType_ReceiveOnPort);
                                    const char *transmitToPort = pList.get_prop(PropertyType_TransmitToPort);
                                    const char *transmitToIPAddress = pList.get_prop(PropertyType_TransmitToIPAddress);
                                    int maxPacketsPerRun = atoi(pList.get_prop(PropertyType_MaxPacketsPerRun));
                                    
                                    if (IsSameString(typeProp, s_OSCSurfaceToken))
                                        oscSurfacesIO_.push_back(make_unique<OSC_ControlSurfaceIO>(this, nameProp, channelCount, receiveOnPort, transmitToPort, transmitToIPAddress, maxPacketsPerRun));
                                    else if (IsSameString(typeProp, s_OSCX32SurfaceToken))
                                        oscSurfacesIO_.push_back(make_unique<OSC_X32ControlSurfaceIO>(this, nameProp, channelCount, receiveOnPort, transmitToPort, transmitToIPAddress, maxPacketsPerRun));
                                }
                            }
                        }
                    }
                }
                else if (const char *pageNameProp = pList.get_prop(PropertyType_PageName))
                {
                    bool followMCP = true;
                    bool synchPages = true;
                    bool isScrollLinkEnabled = false;
                    bool isScrollSynchEnabled = false;

                    currentPage = NULL;
                    
                    if (tokens.size() > 1)
                    {
                        if (const char *pageFollowsMCPProp = pList.get_prop(PropertyType_PageFollowsMCP))
                        {
                            if (IsSameString(pageFollowsMCPProp, "No"))
                                followMCP = false;
                        }
                        
                        if (const char *synchPagesProp = pList.get_prop(PropertyType_SynchPages))
                        {
                            if (IsSameString(synchPagesProp, "No"))
                                synchPages = false;
                        }
                        
                        if (const char *scrollLinkProp = pList.get_prop(PropertyType_ScrollLink))
                        {
                            if (IsSameString(scrollLinkProp, "Yes"))
                                isScrollLinkEnabled = true;
                        }
                        
                        if (const char *scrollSynchProp = pList.get_prop(PropertyType_ScrollSynch))
                        {
                            if (IsSameString(scrollSynchProp, "Yes"))
                                isScrollSynchEnabled = true;
                        }

                        pages_.push_back(make_unique<Page>(this, pageNameProp, followMCP, synchPages, isScrollLinkEnabled, isScrollSynchEnabled));
                        currentPage = pages_.back().get();
                    }
                }
                else if (const char *broadcasterProp = pList.get_prop(PropertyType_Broadcaster))
                {
                    currentBroadcaster = broadcasterProp;
                }
                else if (currentPage && tokens.size() > 2 && currentBroadcaster != "" && pList.get_prop(PropertyType_Listener) != NULL)
                {
                    if (currentPage && tokens.size() > 2 && currentBroadcaster != "")
                    {
                        
                        ControlSurface *broadcaster = NULL;
                        ControlSurface *listener = NULL;
                        
                        const vector<unique_ptr<ControlSurface>> &surfaces = currentPage->GetSurfaces();
                        
                        string currentSurface = string(pList.get_prop(PropertyType_Listener));
                        
                        for (int i = 0; i < surfaces.size(); ++i)
                        {
                            if (surfaces[i]->GetName() == currentBroadcaster)
                                broadcaster = surfaces[i].get();
                            if (surfaces[i]->GetName() == currentSurface)
                                 listener = surfaces[i].get();
                        }
                        
                        if (broadcaster != NULL && listener != NULL)
                        {
                            broadcaster->GetZoneManager()->AddListener(listener);
                            listener->GetZoneManager()->SetListenerCategories(pList);
                        }
                    }
                }
                else if (currentPage && tokens.size() == 5)
                {
                    if (const char *surfaceProp = pList.get_prop(PropertyType_Surface))
                    {
                        if (const char *surfaceFolderProp = pList.get_prop(PropertyType_SurfaceFolder))
                        {
                            if (const char *startChannelProp = pList.get_prop(PropertyType_StartChannel))
                            {
                                int startChannel = atoi(startChannelProp);
                                
                                string baseDir = string(GetResourcePath()) + string("/CSI/Surfaces/");
                                     
                                if ( ! filesystem::exists(baseDir))
                                {
                                    LogToConsole("[ERROR] Missing Surfaces Folder %s\n", baseDir.c_str());

                                    return;
                                }
                                
                                string surfaceFile = baseDir + surfaceFolderProp + "/Surface.txt";
                                
                                if ( ! filesystem::exists(surfaceFile))
                                {
                                    LogToConsole("[ERROR] Missing Surfaces File %s\n", surfaceFile.c_str());
                                }
                                
                                string zoneFolder = baseDir + surfaceFolderProp + "/Zones";
                                if (const char *zoneFolderProp = pList.get_prop(PropertyType_ZoneFolder))
                                    zoneFolder = baseDir + zoneFolderProp + "/Zones";
                                
                                if ( ! filesystem::exists(zoneFolder))
                                {
                                    LogToConsole("[ERROR] Missing Zone Folder %s\n", zoneFolder.c_str());
                                }
                                
                                string fxZoneFolder = baseDir + surfaceFolderProp + "/FXZones";
                                if (const char *fxZoneFolderProp = pList.get_prop(PropertyType_FXZoneFolder))
                                    fxZoneFolder = baseDir + fxZoneFolderProp + "/FXZones";

                                if ( ! filesystem::exists(fxZoneFolder))
                                {
                                    try
                                    {
                                        RecursiveCreateDirectory(fxZoneFolder.c_str(), 0);
                                    }
                                    catch (const std::exception& e)
                                    {
                                        LogToConsole("[ERROR] FAILED to Init. Unable to create folder %s\n", fxZoneFolder.c_str());
                                        LogToConsole("Exception: %s\n", e.what());

                                        return;
                                    }

                                }

                                bool foundIt = false;
                                
                                for (auto &io : midiSurfacesIO_)
                                {
                                    if (IsSameString(surfaceProp, io->GetName()))
                                    {
                                        foundIt = true;
                                        currentPage->GetSurfaces().push_back(make_unique<Midi_ControlSurface>(this, currentPage, surfaceProp, startChannel, surfaceFile.c_str(), zoneFolder.c_str(), fxZoneFolder.c_str(), io.get()));
                                        break;
                                    }
                                }
                                
                                if ( ! foundIt)
                                {
                                    for (auto &io : oscSurfacesIO_)
                                    {
                                        if (IsSameString(surfaceProp, io->GetName()))
                                        {
                                            foundIt = true;
                                            currentPage->GetSurfaces().push_back(make_unique<OSC_ControlSurface>(this, currentPage, surfaceProp, startChannel, surfaceFile.c_str(), zoneFolder.c_str(), fxZoneFolder.c_str(), io.get()));
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            lineNumber++;
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole("[ERROR] FAILED to Init in %s, around line %d\n", iniFilePath.c_str(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }
    
    if (pages_.size() == 0)
        pages_.push_back(make_unique<Page>(this, "Home", false, false, false, false));
    
    for (auto &page : pages_)
    {
        for (auto &surface : page->GetSurfaces())
            surface->ForceClear();

        page->OnInitialization();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack *TrackNavigator::GetTrack()
{
    return trackNavigationManager_->GetTrackFromChannel(channelNum_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterTrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack *MasterTrackNavigator::GetTrack()
{
    return GetMasterTrack(NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// SelectedTrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack *SelectedTrackNavigator::GetTrack()
{
    return page_->GetTrackNavigationManager()->GetSelectedTrack(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// FocusedFXNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack *FocusedFXNavigator::GetTrack()
{
    int trackNumber = 0;
    int itemNumber = 0;
    int takeNumber = 0;
    int fxIndex = 0;
    int paramIndex = 0;
    
    int retVal = GetTouchedOrFocusedFX(1, &trackNumber, &itemNumber, &takeNumber, &fxIndex, &paramIndex);

    trackNumber++;
    
    if (retVal && ! (paramIndex & 0x01)) // Track FX
    {
        if (trackNumber > 0)
            return DAW::GetTrack(trackNumber);
        else if (trackNumber == 0)
            return GetMasterTrack(NULL);
        else
            return NULL;
    }
    else
        return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ActionContext
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ActionContext::ActionContext(CSurfIntegrator *const csi, Action *action, Widget *widget, Zone *zone, int paramIndex, const vector<string> &paramsAndProperties) : csi_(csi), action_(action), widget_(widget), zone_(zone), paramIndex_(paramIndex)
{
    vector<string> params;
    for (int i = 0; i < (int)(paramsAndProperties).size(); ++i) {
        if ((paramsAndProperties)[i].find("=") == string::npos)
            params.push_back((paramsAndProperties)[i]);
        sourceParams_.push_back((paramsAndProperties)[i]);
    }
    GetPropertiesFromTokens(0, (int)(paramsAndProperties).size(), paramsAndProperties, widgetProperties_);

    const char* feedback = widgetProperties_.get_prop(PropertyType_Feedback);
    if (feedback && IsSameString(feedback, "No"))
        provideFeedback_ = false;

    const char* holdDelay = widgetProperties_.get_prop(PropertyType_HoldDelay);
    if (holdDelay)
        holdDelayMs_ = atoi(holdDelay);

    const char* holdRepeatInterval = widgetProperties_.get_prop(PropertyType_HoldRepeatInterval);
    if (holdRepeatInterval)
        holdRepeatIntervalMs_ = atoi(holdRepeatInterval);

    const char* runCount = widgetProperties_.get_prop(PropertyType_RunCount);
    if (runCount)
        runCount_ = atoi(runCount);
    if (runCount_ < 1) {
        runCount_ = 1;
        this->LogMessage(string("invalid value for RunCount '") + runCount + "'", DEBUG_LEVEL_WARNING);
    }

    const char* blinkTime = widgetProperties_.get_prop(PropertyType_Blink);
    if (blinkTime)
        SetBlinkInterval(atoi(blinkTime));

    const char * meterMode  = widgetProperties_.get_prop(PropertyType_MeterMode);
    if (meterMode  &&  strlen(meterMode) > 0)
        strncpy(meterMode_, meterMode, sizeof(meterMode_) - 1);

    for (int i = 0; i < (int)(paramsAndProperties).size(); ++i) {
        if (paramsAndProperties[i] == "NoFeedback") {
            provideFeedback_ = false;
        } else if (paramsAndProperties[i] == "Blink") {
            SetBlinkInterval(INHERIT_VALUE);
        }
    }

    string actionName = "";
    
    if (params.size() > 0)
        actionName = params[0];
    
    // Action with int param, could include leading minus sign
    if (params.size() > 1 && (isdigit(params[1][0]) ||  params[1][0] == '-'))  // C++ 2003 says empty strings can be queried without catastrophe :)
    {
        intParam_= atol(params[1].c_str());
    }
    
    if (actionName == "Bank" && (params.size() > 2 && (isdigit(params[2][0]) ||  params[2][0] == '-')))  // C++ 2003 says empty strings can be queried without catastrophe :)
    {
        stringParam_ = params[1];
        intParam_= atol(params[2].c_str());
    }

    // Action with param index, must be positive
    if (params.size() > 1 && isdigit(params[1][0]))  // C++ 2003 says empty strings can be queried without catastrophe :)
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    // Action with string param
    if (params.size() > 1)
        stringParam_ = params[1];
    
    if (actionName == "TrackVolumeDB" || actionName == "TrackSendVolumeDB")
    {
        rangeMinimum_ = -144.0;
        rangeMaximum_ = 24.0;
    }
    
    if (actionName == "TrackPanPercent" ||
        actionName == "TrackPanWidthPercent" ||
        actionName == "TrackPanLPercent" ||
        actionName == "TrackPanRPercent")
    {
        rangeMinimum_ = -100.0;
        rangeMaximum_ = 100.0;
    }
   
    if ((actionName == "Reaper" ||
         actionName == "ReaperDec" ||
         actionName == "ReaperInc") && params.size() > 1)
    {
        if (isdigit(params[1][0])) {
            commandId_ =  atol(params[1].c_str());
        } else {
            commandId_ = NamedCommandLookup(params[1].c_str());
            if (commandId_ == 0) {
                commandId_ = 65535;
                this->LogMessage(string("no actions found for '") + this->GetStringParam() + "'", DEBUG_LEVEL_ERROR);
            }
        }

        commandText_ = DAW::GetCommandName(commandId_);

        for (int id : GetCSI()->GetReloadingCommandIds()) {
            if (id == commandId_) {
                needsReloadAfterRun_ = true;
                break;
            }
        }

        int feedbackState = GetToggleCommandState(commandId_);
        if (feedbackState == -1 && provideFeedback_) {
            provideFeedback_ = false;
            this->LogMessage(string("action '") + DAW::GetCommandName(commandId_) + "' does not provide feedback", DEBUG_LEVEL_NOTICE);
        }
    }
        
    if ((actionName == "FXParam" || actionName == "JSFXParam") &&
          params.size() > 1 && isdigit(params[1][0]))
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    if (actionName == "FXParamValueDisplay" && params.size() > 1 && isdigit(params[1][0]))
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    if (actionName == "FXParamNameDisplay" && params.size() > 1 && isdigit(params[1][0]))
    {
        paramIndex_ = atol(params[1].c_str());
        
        if (params.size() > 2 && params[2] != "{" && params[2] != "[")
               fxParamDisplayName_ = params[2];
    }
    
    if (actionName == "FixedTextDisplay" && (params.size() > 2 && (isdigit(params[2][0]))))  // C++ 2003 says empty strings can be queried without catastrophe :)
    {
        stringParam_ = params[1];
        paramIndex_= atol(params[2].c_str());
    }
    
    if (params.size() > 0)
        SetColor(params, supportsColor_, supportsTrackColor_, colorValues_);
    
    GetSteppedValues(widget, action_, zone_, paramIndex_, params, widgetProperties_, deltaValue_, acceleratedDeltaValues_, rangeMinimum_, rangeMaximum_, steppedValues_, acceleratedTickValues_);

    if (acceleratedTickValues_.size() < 1)
        acceleratedTickValues_.push_back(0);

    ProcessActionTitle(actionName);
    
    const char* osdValue = widgetProperties_.get_prop(PropertyType_OSD);
    osdData_ = osd_data(osdValue ? osdValue : "?");
    if (osdData_.message == "No")
        osdData_.message.clear();
    else if (osdData_.message == "?")
        osdData_.message = actionTitle_;

    if (actionName == "InvalidAction")
        osdData_.bgColor = osd_data::COLOR_ERROR;
}

void ActionContext::ProcessActionTitle(string origName)
{
    if (commandId_ > 0) {
        actionTitle_ = DAW::GetCommandName(commandId_);
        return;
    }
    const ActionType actionType = this->GetAction()->GetType();
    const char* actionName = this->GetAction()->GetName();
    const char* stringParam = this->GetStringParam();
    
    switch (actionType) {
        case ActionType::InvalidAction:
            actionTitle_ = "InvalidAction: " + origName;
            break;
        case ActionType::TrackAutoMode:
            actionTitle_ = string("Automation: ") + TrackNavigationManager::GetAutoModeDisplayNameNoOverride(atoi(stringParam));
            break;
        default:
            if (IsSameString(stringParam, "{")|| IsSameString(stringParam, "["))
                actionTitle_ = actionName; //TODO: fix parser?
            else
                actionTitle_ = (stringParam && stringParam[0] != '\0') ? string(actionName) + " " + stringParam : actionName;
            break;
    }
}

IPageContext *ActionContext::GetPage()
{
    return widget_->GetSurface()->GetPage();
}

ControlSurface *ActionContext::GetSurface()
{
    return widget_->GetSurface();
}

MediaTrack *ActionContext::GetTrack()
{
    return zone_->GetNavigator()->GetTrack();
}

vector<MediaTrack*> ActionContext::GetSelectedTracks(bool includeMaster) {
    if (this->GetZone()->GetNavigator()->GetType() == NavigatorType::SelectedTrackNavigator) {
        return this->GetPage()->GetTrackNavigationManager()->GetSelectedTracks(includeMaster);
    } else {
        MediaTrack* track = this->GetTrack();
        return track ? vector<MediaTrack*>{ track } : vector<MediaTrack*>{};
    }
}

int ActionContext::GetSlotIndex()
{
    return zone_->GetSlotIndex();
}

const char *ActionContext::GetName()
{
    return zone_->GetAlias();
}

void ActionContext::RequestUpdate()
{
    if (provideFeedback_)
        action_->RequestUpdate(this);
}

void ActionContext::ClearWidget()
{
    UpdateWidgetValue(0.0);
    UpdateWidgetValue("");
}

void ActionContext::UpdateColorValue(double value)
{
    if (supportsColor_) {
        currentColorIndex_ = value == 0 ? 0 : 1;
        if (colorValues_.size() > currentColorIndex_)
            widget_->UpdateColorValue(colorValues_[currentColorIndex_]);
    }
}

void ActionContext::UpdateWidgetValue(double value)
{
    if (steppedValues_.size() > 0)
        SetSteppedValueIndex(value);

    if (isFeedbackInverted_) {
        value = 1.0 - value;
    } else if (blinkSet_) {
        bool shouldBlink = (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE);
        if (shouldBlink && UpdateBlinkState()) {
            value = 1.0 - value;
        }
    }
    widget_->UpdateValue(widgetProperties_, value);

    UpdateColorValue(value);
    
    if (osdData_.IsAwaitFeedback())
        ProcessOSD(value, true);

    if (supportsTrackColor_)
        UpdateTrackColor();
}

void ActionContext::UpdateJSFXWidgetSteppedValue(double value)
{
    if (steppedValues_.size() > 0)
        SetSteppedValueIndex(value);
}

void ActionContext::UpdateTrackColor()
{
    if (MediaTrack* track = zone_->GetNavigator()->GetTrack())
    {
        rgba_color color = DAW::GetTrackColor(track);
        widget_->UpdateColorValue(color);
    }
}

void ActionContext::UpdateWidgetValue(const char* value)
{
    widget_->UpdateValue(widgetProperties_, value ? value : "");
}

void ActionContext::ForceWidgetValue(const char* value)
{
    widget_->ForceValue(widgetProperties_, value ? value : "");
}

void ActionContext::LogAction(double value)
{
    if (g_debugLevel < DEBUG_LEVEL_INFO) return;
    if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
    if (value < 0 && GetRangeMinimum() >= 0) return;
    if (value > 0 && GetRangeMinimum() < 0) return;

    std::ostringstream oss;
    if (supportsColor_) {
        oss << " { ";
        for (size_t i = 0; i < colorValues_.size(); ++i) {
            oss << " " << colorValues_[i].r << " " << colorValues_[i].g << " " << colorValues_[i].b;
            if (i != colorValues_.size() - 1) oss << ", ";
        }
        oss << " }[" << currentColorIndex_ << "]";
    }
    if (!provideFeedback_) oss << " FeedBack=No";
    if (isValueInverted_) oss << " Invert";
    if (isFeedbackInverted_) oss << " InvertFB";
    if (holdDelayMs_ > 0) oss << " HoldDelay=" << holdDelayMs_;
    if (holdRepeatIntervalMs_ > 0) oss << " HoldRepeatInterval=" << holdRepeatIntervalMs_;
    if (runCount_ > 1) oss << " RunCount=" << runCount_;
    if (blinkSet_) {
        oss << " Blink";
        if (blinkIntervalMs_ > 0)
            oss << "=" << blinkIntervalMs_;
    }

    LogToConsole("[INFO] @%s/{%s}: [%s] '%s' > %s (%s) val:%0.2f ctx:%s\n"
        ,this->GetSurface()->GetName()
        ,this->GetZone()->GetName()
        ,this->GetWidget()->GetName()
        ,JoinStringVector(sourceParams_, " ").c_str()
        ,actionTitle_.c_str()
        ,oss.str().c_str()
        ,value
        ,this->GetName()
    );
}

void ActionContext::LogMessage(const std::string& msg, DebugLevel debugLevel) {
    if (g_debugLevel >= debugLevel) {
        LogToConsole("[%s] @%s/{%s}: [%s] %s(%s) # %s\n"
            ,DebugLevelToString(debugLevel)
            ,this->GetSurface()->GetName()
            ,this->GetZone()->GetName()
            ,this->GetWidget()->GetName()
            ,this->GetAction()->GetName()
            ,this->GetStringParam()
            ,msg.c_str()
        );
    }
}

int ActionContext::ClampValueWithWarning(int value, int min, int max) {
    int clamped = std::max(min, std::min(value, max));
    if (clamped != value) {
        this->LogMessage("invalid value = " + to_string(value) + " (allowed: " + to_string(min) + "–" + to_string(max) + ")", DEBUG_LEVEL_WARNING);
    }
    return clamped;
}

int ActionContext::GetBlinkInterval() {
    return blinkIntervalMs_ == INHERIT_VALUE ? this->GetSurface()->GetBlinkTime() : blinkIntervalMs_;
}
int ActionContext::GetHoldDelay() {
    return holdDelayMs_ == INHERIT_VALUE ? this->GetSurface()->GetHoldTime() : holdDelayMs_;
}

// runs once button pressed/released
void ActionContext::DoAction(double value)
{
    DWORD nowTs = GetTickCount();
    int holdDelayMs = this->GetHoldDelay();
    deferredValue_ = value;

    if ((isDoublePress_ || GetWidget()->HasDoublePressActions()) && value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
        if (doublePressStartTs_ == 0 || nowTs > doublePressStartTs_ + GetSurface()->GetDoublePressTime()) {
            doublePressStartTs_ = nowTs;
            if (isDoublePress_) return; // throttle normal press
        } else {
            doublePressStartTs_ = 0;
            if (!isDoublePress_ && holdDelayMs == 0) return; // block normal press inside double-press window
        }
    } 

    if (holdRepeatIntervalMs_ > 0) {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
            holdRepeatActive_ = false;
        } else {
            if (holdDelayMs == 0) {
                holdRepeatActive_ = true;
                lastHoldRepeatTs_ = nowTs;
            }
        }
    }
    if (holdDelayMs > 0) {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
            holdActive_ = false;
        } else {
            holdActive_ = true;
            lastHoldStartTs_ = nowTs;
        }
    } else {
        PerformAction(value);
    }
}

// runs in loop to support button hold/repeat actions
void ActionContext::RunDeferredActions()
{
    int holdDelayMs = GetHoldDelay();

    if (holdDelayMs > 0
        && holdActive_
        && lastHoldStartTs_ > 0
        && GetTickCount() > (lastHoldStartTs_ + holdDelayMs)
    ) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] HOLD [%s] %d ms\n", GetWidget()->GetName(), GetTickCount() - lastHoldStartTs_);
        PerformAction(deferredValue_);
        holdActive_ = false; // to mark that this action with it's defined hold delay was performed and separate it from repeated action trigger
        if (holdRepeatIntervalMs_ > 0) {
            holdRepeatActive_ = true;
            lastHoldRepeatTs_ = GetTickCount();
        }
    }
    if (holdRepeatIntervalMs_ > 0
        && holdRepeatActive_
        && lastHoldRepeatTs_ > 0
        && GetTickCount() > (lastHoldRepeatTs_ + holdRepeatIntervalMs_)
    ) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] REPEAT [%s] %d ms\n", GetWidget()->GetName(), GetTickCount() - lastHoldRepeatTs_);
        lastHoldRepeatTs_ = GetTickCount();
        PerformAction(deferredValue_);
    }
}

void ActionContext::PerformAction(double value)
{
    if (!steppedValues_.empty())
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (steppedValuesIndex_ == steppedValues_.size() - 1)
        {
            if (steppedValues_[0] < steppedValues_[steppedValuesIndex_]) // GAW -- only wrap if 1st value is lower
                steppedValuesIndex_ = 0;
        }
        else steppedValuesIndex_++;

        for (int i = 0; i < runCount_; ++i)
            DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
    else for (int i = 0; i < runCount_; ++i)
        DoRangeBoundAction(value);
}

void ActionContext::DoRelativeAction(double delta)
{
    if (steppedValues_.size() > 0)
        DoSteppedValueAction(delta);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + (deltaValue_ != 0.0 ? (delta > 0 ? deltaValue_ : -deltaValue_) : delta));
}

void ActionContext::DoRelativeAction(int accelerationIndex, double delta)
{
    if (steppedValues_.size() > 0)
        DoAcceleratedSteppedValueAction(accelerationIndex, delta);
    else if (acceleratedDeltaValues_.size() > 0)
        DoAcceleratedDeltaValueAction(accelerationIndex, delta);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) +  (deltaValue_ != 0.0 ? (delta > 0 ? deltaValue_ : -deltaValue_) : delta));
}

void ActionContext::ProcessOSD(double value, bool fromFeedback)
{
    if (!GetSurface()->IsOsdEnabled()) return;
    if (GetWidget()->IsVirtual()) return;
    if (osdData_.message.empty()) return;
    if (!fromFeedback && OsdIgnoresButtonRelease() && value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
    if (value < 0 && GetRangeMinimum() >= 0) return;
    if (value > 0 && GetRangeMinimum() < 0) return;

    int colorIdx = (int) value;
    if (osdData_.bgColors.empty()) {
        if (supportsColor_ && !colorValues_.empty()) {

            if (colorValues_.size() == 1) colorIdx = 0;
            if ((int) colorValues_.size() - 1 < colorIdx) colorIdx = (int) colorValues_.size() - 1;

            char hexColor[8];
            snprintf(hexColor, sizeof(hexColor), "#%02X%02X%02X", colorValues_[colorIdx].r, colorValues_[colorIdx].g, colorValues_[colorIdx].b);

            osdData_.bgColor = hexColor;
        } else if (GetWidget()->GetIsTwoState()) {
            osdData_.bgColor = (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) ? "1" : "0";
        }
    } else {
        if (osdData_.bgColors.size() == 1) colorIdx = 0;
        if ((int) osdData_.bgColors.size() - 1 < colorIdx) colorIdx = (int) osdData_.bgColors.size() - 1;
        osdData_.bgColor = osdData_.bgColors[colorIdx];
    }
    if (osdData_.timeoutMs == 0) osdData_.timeoutMs = GetSurface()->GetOSDTime();

    if ((action_->IsVolumeRelated() || action_->IsPanRelated()) && !(action_->IsDisplayRelated() || action_->IsMeterRelated())) {
        if (MediaTrack *track = this->GetTrack()) {
            ostringstream oss;
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            oss << "[" << trackName_ << "] ";
            if (action_->IsPanRelated()) {
                if (pan == 0.0) oss << "Center";
                else oss << std::fixed << std::setprecision(2) << std::abs(pan * 100) << "%" << (pan > 0 ? "R" : "L");
            } else oss << std::fixed << std::setprecision(2) << VAL2DB(vol) << " dB";
            
            osdData_.message = oss.str();
        } else {
            osdData_.message = string(action_->GetName()) + ": No track selected";
        }
        osdData_.SetAwaitFeedback(false);
        return GetCSI()->EnqueueOSD(osdData_);
    }

    if (action_->IsFxRelated() && !(action_->IsDisplayRelated() || action_->IsMeterRelated())) {
        osdData_.message = (fxParamDisplayName_.empty() ? fxParamDescription_ : fxParamDisplayName_);
        osdData_.message += (osdData_.message.empty()) ? "No FX selected" : ": " + GetTrackFxParamFormattedValue();
        osdData_.SetAwaitFeedback(false);
        return GetCSI()->EnqueueOSD(osdData_);
    }

    const ActionType actionType = action_->GetType();

    if (actionType == ActionType::CycleDebugLevel) {
        osdData_.message = string(action_->GetName()) + ": " + DebugLevelToString(g_debugLevel);
        osdData_.SetAwaitFeedback(false);
        return GetCSI()->EnqueueOSD(osdData_);
    }

    bool awaitFeedback = osdData_.IsAwaitFeedback();
    if (provideFeedback_) {
        if (awaitFeedback) {
            if (fromFeedback) {
                osdData_.SetAwaitFeedback(false);
            } else {
                return;
            }
        } else {
            return osdData_.SetAwaitFeedback(!fromFeedback);
        }
    } else {
        osdData_.SetAwaitFeedback(false);
    }

    GetCSI()->EnqueueOSD(osdData_);
}

bool ActionContext::OsdIgnoresButtonRelease() {
    const ActionType actionType = this->GetAction()->GetType();
    if (actionType == ActionType::Bank
     || actionType == ActionType::SetShift
     || actionType == ActionType::SetOption
     || actionType == ActionType::SetControl
     || actionType == ActionType::SetAlt
     || actionType == ActionType::SetFlip
     || actionType == ActionType::SetGlobal
     || actionType == ActionType::SetMarker
     || actionType == ActionType::SetNudge
     || actionType == ActionType::SetZoom
     || actionType == ActionType::SetScrub
     || actionType == ActionType::FXParam
     || actionType == ActionType::JSFXParam
     || actionType == ActionType::TCPFXParam
     || actionType == ActionType::LastTouchedFXParam
     || actionType == ActionType::TrackVolume
     || actionType == ActionType::SoftTakeover7BitTrackVolume
     || actionType == ActionType::SoftTakeover14BitTrackVolume
     || actionType == ActionType::TrackVolumeDB
     || actionType == ActionType::TrackPan
     || actionType == ActionType::TrackPanPercent
     || actionType == ActionType::TrackPanWidth
     || actionType == ActionType::TrackPanWidthPercent
     || actionType == ActionType::TrackPanL
     || actionType == ActionType::TrackPanLPercent
     || actionType == ActionType::TrackPanR
     || actionType == ActionType::TrackPanRPercent
     || actionType == ActionType::TrackPanAutoLeft
     || actionType == ActionType::TrackPanAutoRight
     || actionType == ActionType::TrackSendVolume
     || actionType == ActionType::TrackSendVolumeDB
     || actionType == ActionType::TrackSendPan
     || actionType == ActionType::TrackSendPanPercent
     || actionType == ActionType::TrackReceiveVolume
     || actionType == ActionType::TrackReceiveVolumeDB
     || actionType == ActionType::TrackReceivePan
     || actionType == ActionType::TrackReceivePanPercent
     || actionType == ActionType::MoveCursor
     || actionType == ActionType::TrackVolumeWithMeterAverageLR
     || actionType == ActionType::TrackVolumeWithMeterMaxPeakLR
    ) {
        return false;
    }
    return true;
}

void ActionContext::DoRangeBoundAction(double value)
{
    this->LogAction(value);

    if (value > rangeMaximum_)
        value = rangeMaximum_;
    
    if (value < rangeMinimum_)
        value = rangeMinimum_;
    
    if (isValueInverted_)
        value = 1.0 - value;
    
    for (int i = 0; i < runCount_; ++i)
        action_->Do(this, value);

    this->ProcessOSD(value, false);
}

void ActionContext::DoSteppedValueAction(double delta)
{
    if (delta > 0)
    {
        steppedValuesIndex_++;
        
        if (steppedValuesIndex_ > (int)steppedValues_.size() - 1)
            steppedValuesIndex_ = (int)steppedValues_.size() - 1;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
    else
    {
        steppedValuesIndex_--;
        
        if (steppedValuesIndex_ < 0 )
            steppedValuesIndex_ = 0;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
}

void ActionContext::DoAcceleratedSteppedValueAction(int accelerationIndex, double delta)
{
    if (delta > 0)
    {
        accumulatedIncTicks_++;
        accumulatedDecTicks_ = accumulatedDecTicks_ - 1 < 0 ? 0 : accumulatedDecTicks_ - 1;
    }
    else if (delta < 0)
    {
        accumulatedDecTicks_++;
        accumulatedIncTicks_ = accumulatedIncTicks_ - 1 < 0 ? 0 : accumulatedIncTicks_ - 1;
    }
    
    accelerationIndex = accelerationIndex > (int)acceleratedTickValues_.size() - 1 ? (int)acceleratedTickValues_.size() - 1 : accelerationIndex;
    accelerationIndex = accelerationIndex < 0 ? 0 : accelerationIndex;
    
    if (delta > 0 && accumulatedIncTicks_ >= acceleratedTickValues_[accelerationIndex])
    {
        accumulatedIncTicks_ = 0;
        accumulatedDecTicks_ = 0;
        
        steppedValuesIndex_++;
        
        if (steppedValuesIndex_ > (int)steppedValues_.size() - 1)
            steppedValuesIndex_ = (int)steppedValues_.size() - 1;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
    else if (delta < 0 && accumulatedDecTicks_ >= acceleratedTickValues_[accelerationIndex])
    {
        accumulatedIncTicks_ = 0;
        accumulatedDecTicks_ = 0;
        
        steppedValuesIndex_--;
        
        if (steppedValuesIndex_ < 0 )
            steppedValuesIndex_ = 0;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
}

void ActionContext::DoAcceleratedDeltaValueAction(int accelerationIndex, double delta)
{
    accelerationIndex = accelerationIndex > (int)acceleratedDeltaValues_.size() - 1 ? (int)acceleratedDeltaValues_.size() - 1 : accelerationIndex;
    accelerationIndex = accelerationIndex < 0 ? 0 : accelerationIndex;
    
    if (delta > 0.0)
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + acceleratedDeltaValues_[accelerationIndex]);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) - acceleratedDeltaValues_[accelerationIndex]);
}

void ActionContext::GetColorValues(vector<rgba_color> &colorValues, const vector<string> &colors)
{
    for (int i = 0; i < (int)colors.size(); ++i)
    {
        rgba_color colorValue;
        
        if (GetColorValue(colors[i].c_str(), colorValue))
            colorValues.push_back(colorValue);
    }
}

void ActionContext::SetColor(const vector<string> &params, bool &supportsColor, bool &supportsTrackColor, vector<rgba_color> &colorValues)
{
    vector<int> rawValues;
    vector<string> hexColors;

    int openCurlyIndex = 0;
    int closeCurlyIndex = 0;
    
    for (int i = 0; i < params.size(); ++i)
        if (params[i] == "{")
        {
            openCurlyIndex = i;
            break;
        }
    
    for (int i = 0; i < params.size(); ++i)
        if (params[i] == "}")
        {
            closeCurlyIndex = i;
            break;
        }
   
    if (openCurlyIndex != 0 && closeCurlyIndex != 0)
    {
        for (int i = openCurlyIndex + 1; i < closeCurlyIndex; ++i)
        {
            const string &strVal = params[i];
            
            if (strVal[0] == '#')
            {
                hexColors.push_back(strVal);
                continue;
            }
            
            if (strVal == "Track")
            {
                supportsTrackColor = true;
                break;
            }
            else if (strVal[0])
            {
                char *ep = NULL;
                const int value = strtol(strVal.c_str(), &ep, 10);
                if (ep && !*ep)
                    rawValues.push_back(wdl_clamp(value, 0, 255));
            }
        }
        
        if (hexColors.size() > 0)
        {
            supportsColor = true;

            GetColorValues(colorValues, hexColors);
        }
        else if (rawValues.size() % 3 == 0 && rawValues.size() > 2)
        {
            supportsColor = true;
            
            for (int i = 0; i < rawValues.size(); i += 3)
            {
                rgba_color color;
                
                color.r = rawValues[i];
                color.g = rawValues[i + 1];
                color.b = rawValues[i + 2];
                
                colorValues.push_back(color);
            }
        }
    }
}

void ActionContext::GetSteppedValues(Widget *widget, Action *action,  Zone *zone, int paramNumber, const vector<string> &params, const PropertyList &widgetProperties, double &deltaValue, vector<double> &acceleratedDeltaValues, double &rangeMinimum, double &rangeMaximum, vector<double> &steppedValues, vector<int> &acceleratedTickValues)
{
    ::GetSteppedValues(params, 0, deltaValue, acceleratedDeltaValues, rangeMinimum, rangeMaximum, steppedValues, acceleratedTickValues);
    
    if (deltaValue == 0.0 && widget->GetStepSize() != 0.0)
        deltaValue = widget->GetStepSize();
    
    if (acceleratedDeltaValues.size() == 0 && widget->GetAccelerationValues().size() != 0)
        acceleratedDeltaValues = widget->GetAccelerationValues();
         
    if (steppedValues.size() > 0 && acceleratedTickValues.size() == 0)
    {
        double stepSize = deltaValue;
        
        if (stepSize != 0.0)
        {
            stepSize *= 10000.0;

            int stepCount = (int) steppedValues.size();
            int baseTickCount = (NUM_ELEM(s_tickCounts_) > stepCount)
                ? s_tickCounts_[stepCount]
                : s_tickCounts_[NUM_ELEM(s_tickCounts_) - 1]
            ;
            int tickCount = int(baseTickCount / stepSize + 0.5);
            acceleratedTickValues.push_back(tickCount);


        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Zone
///////////////////////////////////////////////////////////////////////////////////////////////////////
void Zone::InitSubZones(const vector<string> &subZones, const char *widgetSuffix)
{
    map<const string, CSIZoneInfo> &zoneInfo = zoneManager_->GetZoneInfo();

    for (int i = 0; i < (int)subZones.size(); ++i)
    {
        if (zoneInfo.find(subZones[i]) != zoneInfo.end())
        {
            subZones_.push_back(make_unique<SubZone>(csi_, zoneManager_, GetNavigator(), GetSlotIndex(), subZones[i], zoneInfo[subZones[i]].alias, zoneInfo[subZones[i]].filePath, this));
            zoneManager_->LoadZoneFile(subZones_.back().get(), widgetSuffix);
        }
    }
}

int Zone::GetSlotIndex()
{
    if (name_ == "TrackSend")
        return zoneManager_->GetTrackSendOffset();
    if (name_ == "TrackReceive")
        return zoneManager_->GetTrackReceiveOffset();
    if (name_ == "TrackFXMenu")
        return zoneManager_->GetTrackFXMenuOffset();
    if (name_ == "SelectedTrack")
        return slotIndex_;
    if (name_ == "SelectedTrackSend")
        return slotIndex_ + zoneManager_->GetSelectedTrackSendOffset();
    if (name_ == "SelectedTrackReceive")
        return slotIndex_ + zoneManager_->GetSelectedTrackReceiveOffset();
    if (name_ == "SelectedTrackFXMenu")
        return slotIndex_ + zoneManager_->GetSelectedTrackFXMenuOffset();
    if (name_ == "MasterTrackFXMenu")
        return slotIndex_ + zoneManager_->GetMasterTrackFXMenuOffset();
    else return slotIndex_;
}

void Zone::AddWidget(Widget *widget)
{
    if (find(widgets_.begin(), widgets_.end(), widget) == widgets_.end())
        widgets_.push_back(widget);
}

void Zone::Activate()
{
    UpdateCurrentActionContextModifiers();
    //TODO: fix WidgetN forme HomeZone stops working if subzone has Shift+WidgetN but no WidgetN / subsone requires redefining WidgetN if there are WidgetN+ModifierX
    for (auto &widget : widgets_)
    {
        if (IsSameString(widget->GetName(), "OnZoneActivation"))
            for (auto &actionContext :  GetActionContexts(widget))
                actionContext->DoAction(1.0);

        widget->Configure(GetActionContexts(widget));
    }

    isActive_ = true;
    
    if (IsSameString(GetName(), "VCA"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->ActivateVCAMode();
    else if (IsSameString(GetName(), "Folder"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->ActivateFolderMode();
    else if (IsSameString(GetName(), "SelectedTracks"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->ActivateSelectedTracksMode();

    zoneManager_->GetSurface()->SendOSCMessage(GetName());

    for (auto &subZone : subZones_)
        subZone->Deactivate();

    for (auto &includedZone : includedZones_)
        includedZone->Activate();
}

void Zone::Deactivate()
{
    if (!isActive_)
        return;
    for (auto &widget : widgets_)
    {
        for (auto &actionContext : GetActionContexts(widget))
        {
            actionContext->UpdateWidgetValue(0.0);
            actionContext->UpdateWidgetValue("");

            if (IsSameString(widget->GetName(), "OnZoneDeactivation"))
                actionContext->DoAction(1.0);
        }
    }

    isActive_ = false;
    
    if (IsSameString(GetName(), "VCA"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->DeactivateVCAMode();
    else if (IsSameString(GetName(), "Folder"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->DeactivateFolderMode();
    else if (IsSameString(GetName(), "SelectedTracks"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->DeactivateSelectedTracksMode();
    
    for (auto &includedZone : includedZones_)
        includedZone->Deactivate();

    for (auto &subZone : subZones_)
        subZone->Deactivate();
}

void Zone::RequestUpdate()
{
    if (! isActive_)
        return;
    
    for (auto &subZone : subZones_)
        subZone->RequestUpdate();

    for (auto &includedZone : includedZones_)
        includedZone->RequestUpdate();
    
    for (auto widget : widgets_)
    {
        if ( ! widget->GetHasBeenUsedByUpdate())
        {
            widget->SetHasBeenUsedByUpdate();
            RequestUpdateWidget(widget);
        }
    }
}

void Zone::SetXTouchDisplayColors(const char *colors)
{
    for (auto &widget : widgets_)
       widget->SetXTouchDisplayColors(colors);
}

void Zone::RestoreXTouchDisplayColors()
{
    for (auto &widget : widgets_)
        widget->RestoreXTouchDisplayColors();
}

void Zone::DoAction(Widget *widget, bool &isUsed, double value)
{
    if (! isActive_ || isUsed)
        return;
        
    for (auto &subZone : subZones_)
        subZone->DoAction(widget, isUsed, value);

    if (isUsed)
        return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end())
    {
        isUsed = true;
        
        for (auto &actionContext : GetActionContexts(widget))
            actionContext->DoAction(value);
    }
    else
    {
        for (auto &includedZone : includedZones_)
            includedZone->DoAction(widget, isUsed, value);
    }
}

void Zone::DoRelativeAction(Widget *widget, bool &isUsed, double delta)
{
    if (! isActive_ || isUsed)
        return;
    
    for (auto &subZone : subZones_)
        subZone->DoRelativeAction(widget, isUsed, delta);

    if (isUsed)
        return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end())
    {
        isUsed = true;

        for (auto &actionContext : GetActionContexts(widget))
            actionContext->DoRelativeAction(delta);
    }
    else
    {
        for (auto &includedZone : includedZones_)
            includedZone->DoRelativeAction(widget, isUsed, delta);
    }
}

void Zone::DoRelativeAction(Widget *widget, bool &isUsed, int accelerationIndex, double delta)
{
    if (! isActive_ || isUsed)
        return;

    for (auto &subZone : subZones_)
        subZone->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end())
    {
        isUsed = true;

        for (auto &actionContext : GetActionContexts(widget))
            actionContext->DoRelativeAction(accelerationIndex, delta);
    }
    else
    {
        for (auto &includedZone : includedZones_)
            includedZone->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    }
}

void Zone::DoTouch(Widget *widget, const char *widgetName, bool &isUsed, double value)
{
    if (! isActive_ || isUsed)
        return;

    for (auto &subZone : subZones_)
        subZone->DoTouch(widget, widgetName, isUsed, value);
    
    if (isUsed)
        return;
    
    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end())
    {
        isUsed = true;

        for (auto &actionContext : GetActionContexts(widget))
            actionContext->DoTouch(value);
    }
    else 
    {
        for (auto &includedZone : includedZones_)
            includedZone->DoTouch(widget, widgetName, isUsed, value);
    }
}

void Zone::UpdateCurrentActionContextModifiers()
{
    for (auto &widget : widgets_)
    {
        UpdateCurrentActionContextModifier(widget);
        widget->Configure(GetActionContexts(widget, currentActionContextModifiers_[widget]));
    }
    
    for (auto &includedZone : includedZones_)
        includedZone->UpdateCurrentActionContextModifiers();

    for (auto &subZone : subZones_)
        subZone->UpdateCurrentActionContextModifiers();
}

void Zone::UpdateCurrentActionContextModifier(Widget *widget)
{
    for(int i = 0; i < (int)widget->GetSurface()->GetModifiers().size(); ++i)
    {
        if(actionContextDictionary_[widget].count(widget->GetSurface()->GetModifiers()[i]) > 0)
        {
            currentActionContextModifiers_[widget] = widget->GetSurface()->GetModifiers()[i];
            break;
        }
    }
}

ActionContext *Zone::AddActionContext(Widget *widget, int modifier, Zone *zone, const char *actionName, vector<string> &params)
{
    const auto& action = csi_->GetAction(actionName);
    if (!IsSameString(action->GetName(), actionName) && IsSameString(action->GetName(), "InvalidAction")) LogToConsole("[ERROR] @%s/{%s} [%s] InvalidAction: %s\n"
        ,widget->GetSurface()->GetName()
        ,zone->GetName()
        ,widget->GetName()
        ,actionName
    );

    actionContextDictionary_[widget][modifier].push_back(make_unique<ActionContext>(csi_, action, widget, zone, 0, params));

    return actionContextDictionary_[widget][modifier].back().get();
}

const vector<unique_ptr<ActionContext>> &Zone::GetActionContexts(Widget *widget)
{
    if(currentActionContextModifiers_.count(widget) == 0)
        UpdateCurrentActionContextModifier(widget);
    
    bool isTouched = false;
    bool isToggled = false;
    
    if(widget->GetSurface()->GetIsChannelTouched(widget->GetChannelNumber()))
        isTouched = true;

    if(widget->GetSurface()->GetIsChannelToggled(widget->GetChannelNumber()))
        isToggled = true;
    
    if(currentActionContextModifiers_.count(widget) > 0 && actionContextDictionary_.count(widget) > 0)
    {
        int modifier = currentActionContextModifiers_[widget];
        
        if(isTouched && isToggled && actionContextDictionary_[widget].count(modifier + 3) > 0)
            return actionContextDictionary_[widget][modifier + 3];
        else if(isTouched && actionContextDictionary_[widget].count(modifier + 1) > 0)
            return actionContextDictionary_[widget][modifier + 1];
        else if(isToggled && actionContextDictionary_[widget].count(modifier + 2) > 0)
            return actionContextDictionary_[widget][modifier + 2];
        else if(actionContextDictionary_[widget].count(modifier) > 0)
            return actionContextDictionary_[widget][modifier];
    }

    return emptyContexts_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Widget
////////////////////////////////////////////////////////////////////////////////////////////////////////
ZoneManager *Widget::GetZoneManager()
{
    return surface_->GetZoneManager();
}

void Widget::Configure(const vector<unique_ptr<ActionContext>> &contexts)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->Configure(contexts);
}

void  Widget::UpdateValue(const PropertyList &properties, double value)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetValue(properties, value);
}

void  Widget::UpdateValue(const PropertyList &properties, const char * const &value)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetValue(properties, value);
}

void  Widget::ForceValue(const PropertyList &properties, const char * const &value)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->ForceValue(properties, value);
}

void  Widget::UpdateColorValue(const rgba_color &color)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetColorValue(color);
}

void Widget::SetXTouchDisplayColors(const char *colors)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetXTouchDisplayColors(colors);
}

void Widget::RestoreXTouchDisplayColors()
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->RestoreXTouchDisplayColors();
}

void  Widget::ForceClear()
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->ForceClear();
}

void Widget::LogInput(double value)
{
    if (g_surfaceInDisplay) LogToConsole("wIN <- %s %s %f\n", GetSurface()->GetName(), GetName(), value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_FeedbackProcessor::SendMidiSysExMessage(MIDI_event_ex_t *midiMessage)
{
    surface_->SendMidiSysExMessage(midiMessage);
}

void Midi_FeedbackProcessor::SendMidiMessage(int first, int second, int third)
{
    bool updateMeters = surface_->GetHasMCUMeters() && first == 0xd0; // MUST UPDATE METERS REGARDLESS OF LAST MESSAGE SENT AS METERS ON THE WILL DECAY TO OFF IF NOT UPDATE REGULARLY
 
    if (updateMeters  ||  first != lastMessageSent_.midi_message[0] || second != lastMessageSent_.midi_message[1] || third != lastMessageSent_.midi_message[2])
    {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%02x %02x %02x", first, second, third);

        if (updateMeters)
            snprintf(buffer, sizeof(buffer), "%02x %02x", first, second);

        this->LogMessage(buffer);
        ForceMidiMessage(first, second, third);
    }
}

void Midi_FeedbackProcessor::ForceMidiMessage(int first, int second, int third)
{
    lastMessageSent_.midi_message[0] = first;
    lastMessageSent_.midi_message[1] = second;
    lastMessageSent_.midi_message[2] = third;
    surface_->SendMidiMessage(first, second, third);
}

void Midi_FeedbackProcessor::LogMessage(char* value)
{
    if (g_surfaceOutDisplay) LogToConsole("@S:'%s' [W:'%s'] MIDI: %s\n", surface_->GetName(), widget_->GetName(), value);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void OSC_FeedbackProcessor::SetColorValue(const rgba_color &color)
{
    if (lastColor_ != color)
    {
        lastColor_ = color;
        char tmp[32];
        surface_->SendOSCMessage(this, (oscAddress_ + "/Color").c_str(), color.rgba_to_string(tmp));
    }
}

void OSC_FeedbackProcessor::ForceValue(const PropertyList &properties, double value)
{
    if ((GetTickCount() - GetWidget()->GetLastIncomingMessageTime()) < 50) // adjust the 50 millisecond value to give you smooth behaviour without making updates sluggish
        return;

    lastDoubleValue_ = value;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), value);
}

void OSC_FeedbackProcessor::ForceValue(const PropertyList &properties, const char * const &value)
{
    lastStringValue_ = value;
    char tmp[MEDBUF];
    surface_->SendOSCMessage(this, oscAddress_.c_str(), GetWidget()->GetSurface()->GetRestrictedLengthText(value,tmp,sizeof(tmp)));
}

void OSC_FeedbackProcessor::ForceClear()
{
    lastDoubleValue_ = 0.0;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), 0.0);
    
    lastStringValue_ = "";
    surface_->SendOSCMessage(this, oscAddress_.c_str(), "");
}

void OSC_IntFeedbackProcessor::ForceClear()
{
    lastDoubleValue_ = 0.0;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), (int)0);
}

void OSC_IntFeedbackProcessor::ForceValue(const PropertyList &properties, double value)
{
    lastDoubleValue_ = value;
    
    surface_->SendOSCMessage(this, oscAddress_.c_str(), (int)value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ZoneManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
ZoneManager::ZoneManager(CSurfIntegrator *const csi, ControlSurface *surface, const string &zoneFolder, const string &fxZoneFolder) : csi_(csi), surface_(surface), zoneFolder_(zoneFolder), fxZoneFolder_(fxZoneFolder == "" ? zoneFolder : fxZoneFolder) {}

Navigator *ZoneManager::GetNavigatorForTrack(MediaTrack *track) { return surface_->GetPage()->GetTrackNavigationManager()->GetNavigatorForTrack(track); }
Navigator *ZoneManager::GetMasterTrackNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetMasterTrackNavigator(); }
Navigator *ZoneManager::GetSelectedTrackNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetSelectedTrackNavigator(); }
Navigator *ZoneManager::GetFocusedFXNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetFocusedFXNavigator(); }
int ZoneManager::GetNumChannels() { return surface_->GetNumChannels(); }

void ZoneManager::Initialize()
{
    PreProcessZones();

    if (zoneInfo_.find("Home") == zoneInfo_.end())
        return LogToConsole("[ERROR] Missing Home Zone for %s\n", surface_->GetName());

    homeZone_ = make_unique<Zone>(csi_, this, GetSelectedTrackNavigator(), 0, "Home", "Home", zoneInfo_["Home"].filePath);
    LoadZoneFile(homeZone_.get(), "");
    zoneInfo_["Home"].isLoaded = true;
    zoneInfo_["Home"].isReferenced = true;
    
    if (zoneInfo_.find("LastTouchedFXParam") != zoneInfo_.end()) {
        lastTouchedFXParamZone_ = make_shared<Zone>(csi_, this, GetFocusedFXNavigator(), 0, "LastTouchedFXParam", "LastTouchedFXParam", zoneInfo_["LastTouchedFXParam"].filePath);
        LoadZoneFile(lastTouchedFXParamZone_.get(), "");
        zoneInfo_["LastTouchedFXParam"].isLoaded = true;
    }

    vector<string> zoneList;

    if (zoneInfo_.find("GoZones") != zoneInfo_.end()) {
        if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] GoZones.zon is DEPRICATED and support of file will be removed in future.\n");
        LoadZoneMetadata(zoneInfo_["GoZones"].filePath.c_str(), zoneList);

        try {
            LoadZones(goZones_, zoneList);
        } catch (const std::exception& e) {
            LogToConsole("[ERROR] %s in GoZones section in file %s\n", e.what(), GetRelativePath(zoneInfo_["GoZones"].filePath.c_str()));
        }
    } else {
        for (const auto& entry : zoneInfo_) {
            if (IsSameString(entry.first, "FXEpilogue")
             || IsSameString(entry.first, "FXPrologue")
             || IsSameString(entry.first, "FXRowLayout")
             || IsSameString(entry.first, "FXWidgetLayout")
             || IsSameString(entry.first, "GoZones")
            ) continue;
            if (!entry.second.isLoaded && !entry.second.isFxZone) {
                zoneList.push_back(entry.first);
            }
        }
        LoadZones(goZones_, zoneList);
    
        for (const auto& entry : zoneInfo_) {
            CSIZoneInfo zoneInfo = entry.second;
            if (zoneInfo.isLoaded && !zoneInfo.isReferenced)
                if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole("[WARNING] Zone '%s' was loaded but never referenced! %s\n", entry.first.c_str(), GetRelativePath(zoneInfo.filePath.c_str()));
            if (!zoneInfo.isLoaded && zoneInfo.isReferenced)
                LogToConsole("[ERROR] Zone '%s' was referenced but not loaded! (%s)\n", entry.first.c_str(), GetRelativePath(zoneInfo.filePath.c_str()));
        }
    }

    homeZone_->Activate();
}

void ZoneManager::PreProcessZoneFile(const string &filePath)
{
    try {
        ifstream file(filePath);
        
        CSIZoneInfo info;
        info.filePath = filePath;

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] PreProcessZoneFile: %s\n", GetRelativePath(filePath.c_str()));

        info.isFxZone = 0 == strncmp(fxZoneFolder_.c_str(), filePath.c_str(), fxZoneFolder_.length());

        for (string line; getline(file, line);) {
            TrimLine(line);
            
            if (IsCommentedOrEmpty(line))
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line);

            PropertyList pList;
            GetPropertiesFromTokens(0, (int) tokens.size(), tokens, pList);
// "AU:", "AUi:", "VST:", "VST3:", "VST3i:", "VSTi:", "JS:", "Rewire:", "CLAP:", "CLAPi:", 
            if (tokens[0] == "Zone" && tokens.size() > 1) {
                info.alias = tokens.size() > 2 ? tokens[2] : tokens[1];

                if (const char *propValue =  pList.get_prop(PropertyType_NavType)) {
                    NavigatorType type = Navigator::NameToType(propValue);

                    if (type != NavigatorType::Invalid) {
                        info.navigator = propValue;
                    } else {
                        LogToConsole("[ERROR] Invalid value for property NavType=%s (supported: %s) in file %s\n"
                            ,propValue
                            ,JoinStringVector(Navigator::GetSupportedNames(), ", ").c_str()
                            ,GetRelativePath(filePath.c_str())
                        );
                    }
                }
                //TODO: GoSubZone LeaveSubZone GoZone GoHome validity check

                AddZoneFilePath(tokens[1], info);
            }

            break;
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole("[ERROR] FAILED to PreProcessZoneFile in %s\n", filePath.c_str());
        LogToConsole("Exception: %s\n", e.what());
    }
}

static ModifierManager s_modifierManager(NULL);

void ZoneManager::GetWidgetNameAndModifiers(const string &line, string &baseWidgetName, int &modifier, bool &isValueInverted, bool &isFeedbackInverted, bool &hasHoldModifier, bool &HasDoublePressPseudoModifier, bool &isDecrease, bool &isIncrease)
{
    vector<string> tokens;
    GetTokens(tokens, line, '+');
    
    baseWidgetName = tokens[tokens.size() - 1];

    if (tokens.size() > 1)
    {
        for (int i = 0; i < tokens.size() - 1; ++i)
        {
            if (tokens[i].find("Touch") != string::npos)
                modifier += 1;
            else if (tokens[i] == "Toggle")
                modifier += 2;
            else if (tokens[i] == "Invert")
                isValueInverted = true;
            else if (tokens[i] == "InvertFB")
                isFeedbackInverted = true;
            else if (tokens[i] == "Hold")
                hasHoldModifier = true;
            else if (tokens[i] == "DoublePress")
                HasDoublePressPseudoModifier = true;
            else if (tokens[i] == "Decrease")
                isDecrease = true;
            else if (tokens[i] == "Increase")
                isIncrease = true;
        }
    }
    
    tokens.erase(tokens.begin() + tokens.size() - 1);
    
    modifier += s_modifierManager.GetModifierValue(tokens);
}

void ZoneManager::GetNavigatorsForZone(const char *zoneName, const char *navigatorName, vector<Navigator *> &navigators)
{
    if (IsSameString(navigatorName, "MasterTrackNavigator") || IsSameString(zoneName, "MasterTrack"))
        navigators.push_back(GetMasterTrackNavigator());
    else if (IsSameString(zoneName, "MasterTrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i)
            navigators.push_back(GetMasterTrackNavigator());
    else if (IsSameString(navigatorName, "TrackNavigator") ||
             IsSameString(zoneName, "Track") ||
             IsSameString(zoneName, "VCA") ||
             IsSameString(zoneName, "Folder") ||
             IsSameString(zoneName, "SelectedTracks") ||
             IsSameString(zoneName, "TrackSend") ||
             IsSameString(zoneName, "TrackReceive") ||
             IsSameString(zoneName, "TrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i)
        {
            Navigator *channelNavigator = GetSurface()->GetPage()->GetTrackNavigationManager()->GetNavigatorForChannel(i + GetSurface()->GetChannelOffset());
            if (channelNavigator)
                navigators.push_back(channelNavigator);
        }
    else if (IsSameString(zoneName, "SelectedTrack") ||
             IsSameString(zoneName, "SelectedTrackSend") ||
             IsSameString(zoneName, "SelectedTrackReceive") ||
             IsSameString(zoneName, "SelectedTrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i)
            navigators.push_back(GetSelectedTrackNavigator());
    else if (IsSameString(navigatorName, "FocusedFXNavigator"))
        navigators.push_back(GetFocusedFXNavigator());
    //TODO: what about FixedTrackNavigator?
    else
        navigators.push_back(GetSelectedTrackNavigator());
}


void ZoneManager::LoadZones(vector<unique_ptr<Zone>>& zones, vector<string>& zoneList) {
    string missingZoneNames;

    for (const string& line : zoneList) {
        vector<string> tokens;
        GetTokens(tokens, line);

        if (tokens.empty()) continue;

        const string& zoneName = tokens[0];
        string navigatorName = tokens.size() > 1 ? tokens[1] : "";

        const auto& zoneInfoPair = zoneInfo_.find(zoneName);
        if (zoneInfoPair == zoneInfo_.end()) {
            missingZoneNames += " " + line;
            continue;
        }

        CSIZoneInfo& zoneInfo = zoneInfoPair->second;
        vector<Navigator*> navigators;
        GetNavigatorsForZone(zoneName.c_str(), navigatorName.c_str(), navigators);

        if (navigators.empty()) continue;
        for (int j = 0; j < navigators.size(); ++j) {
            string alias = zoneInfo.alias;
            string widgetSuffix = "";

            if (navigators.size() == 1) {
                bool alreadyLoaded = false;
                for (int i = 0; i < zones.size(); ++i) {
                    if (zones[i]->GetName() == zoneName && zones[i]->GetNavigator()->GetName() == navigators[j]->GetName()) {
                        alreadyLoaded = true;
                        break;
                    }
                }
                if (alreadyLoaded)
                    continue;
            } else {
                alias += to_string(j + 1);
                widgetSuffix = to_string(j + 1);
            }

            auto zone = make_unique<Zone>(csi_, this, navigators[j], j, zoneName, alias, zoneInfo.filePath);
            LoadZoneFile(zone.get(), widgetSuffix.c_str());
            zones.push_back(std::move(zone));
            zoneInfo.isLoaded = true;
        }
    }

    if (!missingZoneNames.empty())
        LogToConsole("No .zon files found for zones: %s\n", missingZoneNames.c_str());
}

void ZoneManager::LoadZoneFile(Zone *zone, const char *widgetSuffix)
{
    LoadZoneFile(zone, zone->GetSourceFilePath(), widgetSuffix);
}

void ZoneManager::LoadZoneFile(Zone *zone, const char *filePath, const char *widgetSuffix)
{
    int lineNumber = 0;
    bool isInIncludedZonesSection = false;
    vector<string> includedZonesList;
    bool isInSubZonesSection = false;
    vector<string> subZonesList;

    try
    {
        ifstream file(filePath);
        
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] @%s/{%s} # LoadZoneFile: %s\n", surface_->GetName(), zone->GetName(), GetRelativePath(filePath));
        
        for (string line; getline(file, line);) {
            TrimLine(line);
            
            lineNumber++;
            
            if (IsCommentedOrEmpty(line))
                continue;
            
            if (line == s_BeginAutoSection || line == s_EndAutoSection)
                continue;
            
            ReplaceAllWith(line, ZoneManager::PIPE_CHARACTER, widgetSuffix); //TODO: VALIDATION for PIPE_CHARACTER missuze
// only be used in contexts where CSI dynamically generates multiple Zones:
//   Track (expands to Track1, Track2, etc.), SelectedTracks, TrackFXMenu/SelectedTrackFXMenu, TrackSend/SelectedTrackSend, TrackReceive/SelectedTrackReceive, Folder/VCA
// NOT to use in SelectedTrack, FX zones. In these cases, each widget should be explicitly defined.

            vector<string> tokens;
            GetTokens(tokens, line);
            
            if (tokens[0] == "Zone" || tokens[0] == "ZoneEnd")
                continue;
            
            else if (tokens[0] == "SubZones")
                isInSubZonesSection = true;
            else if (tokens[0] == "SubZonesEnd")
            {
                isInSubZonesSection = false;
                zone->InitSubZones(subZonesList, widgetSuffix); //FIXME prevent recursion
            }
            else if (isInSubZonesSection)
                subZonesList.push_back(tokens[0]);
            
            else if (tokens[0] == "IncludedZones")
                isInIncludedZonesSection = true;
            else if (tokens[0] == "IncludedZonesEnd")
            {
                isInIncludedZonesSection = false;
                try {
                    LoadZones(zone->GetIncludedZones(), includedZonesList);  //FIXME prevent recursion
                } catch (const std::exception& e) {
                    LogToConsole("[ERROR] %s in IncludedZones section in file %s\n", e.what(), GetRelativePath(zone->GetSourceFilePath()));
                }
            }
            else if (isInIncludedZonesSection)
                includedZonesList.push_back(tokens[0]);
            
            else if (tokens.size() > 1)
            {
                if (tokens.size() < 2) {
                    LogToConsole("[ERROR] Not enough params at line %d in %s\n", lineNumber, GetRelativePath(filePath));
                    continue;
                }
                string widgetName;
                int modifier = 0;
                bool isValueInverted = false;
                bool isFeedbackInverted = false;
                bool hasHoldModifier = false;
                bool HasDoublePressPseudoModifier = false;
                bool isDecrease = false;
                bool isIncrease = false;
                
                GetWidgetNameAndModifiers(tokens[0].c_str(), widgetName, modifier, isValueInverted, isFeedbackInverted, hasHoldModifier, HasDoublePressPseudoModifier, isDecrease, isIncrease);
                
                Widget *widget = GetSurface()->GetWidgetByName(widgetName);
                                            
                if (widget == NULL)
                    continue;

                zone->AddWidget(widget);

                vector<string> memberParams;

                for (int i = 1; i < tokens.size(); ++i)
                    memberParams.push_back(tokens[i]);
                
                // For legacy .zon definitions
                if (tokens[1] == "NullDisplay")
                    continue;

                ActionContext *context;
                try {
                    context = zone->AddActionContext(widget, modifier, zone, tokens[1].c_str(), memberParams);
                } catch (const std::exception& e) {
                    LogToConsole("[ERROR] FAILED to AddActionContext for line '%s': %s\n", line.c_str(), e.what());
                    continue;
                }

                if (context == NULL)
                    continue;

                if (IsSameString(tokens[1], "GoZone") || IsSameString(tokens[1], "GoSubZone")) {
                    string zoneName = context->GetStringParam();
                    if (zoneInfo_.find(zoneName) != zoneInfo_.end()) {
                        zoneInfo_[zoneName].isReferenced = true;
                    }
                }
                if (IsSameString(tokens[1], "LastTouchedFXParam")) {
                    if (zoneInfo_.find("LastTouchedFXParam") != zoneInfo_.end()) {
                        zoneInfo_["LastTouchedFXParam"].isReferenced = true;
                    }
                }

                if (isValueInverted)
                    context->SetIsValueInverted();
                    
                if (isFeedbackInverted)
                    context->SetIsFeedbackInverted();
                
                if (hasHoldModifier && context->GetHoldDelay() == 0)
                    context->SetHoldDelay(ActionContext::INHERIT_VALUE);
                
                if (HasDoublePressPseudoModifier) {
                    context->SetDoublePress();
                    widget->SetHasDoublePressActions();
                }
                
                vector<double> range;
                
                if (isDecrease)
                {
                    range.push_back(-2.0);
                    range.push_back(1.0);
                    context->SetRange(range);
                }
                else if (isIncrease)
                {
                    range.push_back(0.0);
                    range.push_back(2.0);
                    context->SetRange(range);
                }
            }
        }

        if (zoneInfo_.find(zone->GetName()) != zoneInfo_.end()) {
            zoneInfo_[zone->GetName()].isLoaded = true;
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole("[ERROR] FAILED to LoadZoneFile in %s, around line %d\n", zone->GetSourceFilePath(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }
}

void ZoneManager::AddListener(ControlSurface *surface)
{
    listeners_.push_back(surface->GetZoneManager());
}

void ZoneManager::SetListenerCategories(PropertyList &pList)
{
    if (const char *property =  pList.get_prop(PropertyType_GoHome))
        if (IsSameString(property, "Yes"))
            listensToGoHome_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_SelectedTrackSends))
        if (IsSameString(property, "Yes"))
            listensToSends_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_SelectedTrackReceives))
        if (IsSameString(property, "Yes"))
            listensToReceives_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_FXMenu))
        if (IsSameString(property, "Yes"))
            listensToFXMenu_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_SelectedTrackFX))
        if (IsSameString(property, "Yes"))
            listensToSelectedTrackFX_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_Modifiers))
        if (IsSameString(property, "Yes"))
            surface_->SetListensToModifiers();
}

void ZoneManager::CheckFocusedFXState()
{
    int trackNumber = 0;
    int itemNumber = 0;
    int takeNumber = 0;
    int fxSlot = 0;
    int paramIndex = 0;
    
    bool retVal = GetTouchedOrFocusedFX(1, &trackNumber, &itemNumber, &takeNumber, &fxSlot, &paramIndex);
    
    if (! isFocusedFXMappingEnabled_)
        return;

    if (! retVal || (retVal && (paramIndex & 0x01)))
    {
        if (focusedFXZone_ != NULL)
            ClearFocusedFX();
        
        return;
    }
    
    if (fxSlot < 0)
        return;
    
    MediaTrack *focusedTrack = NULL;

    trackNumber++;
    
    if (retVal && ! (paramIndex & 0x01))
    {
        if (trackNumber > 0)
            focusedTrack = DAW::GetTrack(trackNumber);
        else if (trackNumber == 0)
            focusedTrack = GetMasterTrack(NULL);
    }
    
    if (focusedTrack)
    {
        char fxName[MEDBUF];
        TrackFX_GetFXName(focusedTrack, fxSlot, fxName, sizeof(fxName));
        
        if(focusedFXZone_ != NULL && focusedFXZone_->GetSlotIndex() == fxSlot && IsSameString(fxName, focusedFXZone_->GetName()))
            return;
        else
            ClearFocusedFX();
        
        if (zoneInfo_.find(fxName) != zoneInfo_.end())
        {
            focusedFXZone_ = make_shared<Zone>(csi_, this, GetFocusedFXNavigator(), fxSlot, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath.c_str());
            LoadZoneFile(focusedFXZone_.get(), "");
            focusedFXZone_->Activate();
        }            
    }
}

void ZoneManager::GoSelectedTrackFX()
{
    selectedTrackFXZones_.clear();
    
    if (MediaTrack *selectedTrack = surface_->GetPage()->GetTrackNavigationManager()->GetSelectedTrack(true))
    {
        for (int i = 0; i < TrackFX_GetCount(selectedTrack); ++i)
        {
            char fxName[MEDBUF];
            
            TrackFX_GetFXName(selectedTrack, i, fxName, sizeof(fxName));
            
            if (zoneInfo_.find(fxName) != zoneInfo_.end())
            {
                shared_ptr<Zone> zone = make_shared<Zone>(csi_, this, GetSelectedTrackNavigator(), i, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath);
                LoadZoneFile(zone.get(), "");
                selectedTrackFXZones_.push_back(zone);
                zone->Activate();
            }
        }
    }
}

void ZoneManager::GoFXSlot(MediaTrack *track, Navigator *navigator, int fxSlot)
{
    if (fxSlot > TrackFX_GetCount(track) - 1)
        return;
        
    char fxName[MEDBUF];
    
    TrackFX_GetFXName(track, fxSlot, fxName, sizeof(fxName));

    if (zoneInfo_.find(fxName) != zoneInfo_.end())
    {
        ClearFXSlot();        
        fxSlotZone_ = make_shared<Zone>(csi_, this, navigator, fxSlot, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath);
        LoadZoneFile(fxSlotZone_.get(), "");
        fxSlotZone_->Activate();
    }
    else
        TrackFX_SetOpen(track, fxSlot, true);
}

void ZoneManager::UpdateCurrentActionContextModifiers()
{  
    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->UpdateCurrentActionContextModifiers();

    if (lastTouchedFXParamZone_ != NULL)
        lastTouchedFXParamZone_->UpdateCurrentActionContextModifiers();
    
    if (focusedFXZone_ != NULL)
        focusedFXZone_->UpdateCurrentActionContextModifiers();
    
    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->UpdateCurrentActionContextModifiers();
    
    if (fxSlotZone_ != NULL)
        fxSlotZone_->UpdateCurrentActionContextModifiers();
    
    if (homeZone_ != NULL)
        homeZone_->UpdateCurrentActionContextModifiers();
    
    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->UpdateCurrentActionContextModifiers();
}

void ZoneManager::PreProcessZones()
{
    if (zoneFolder_[0] == 0)
        return LogToConsole("[ERROR] Please check your CSI.ini, cannot find Zone folder for %s in: %s/CSI/Zones/", GetSurface()->GetName(), GetResourcePath());

    vector<string>  zoneFilesToProcess;
    collectFilesOfType(".zon", zoneFolder_, zoneFilesToProcess);

    if (zoneFilesToProcess.size() == 0)
        return LogToConsole("[ERROR] Cannot find Zone files for %s in: %s", GetSurface()->GetName(), zoneFolder_.c_str());
    
    collectFilesOfType(".zon", fxZoneFolder_, zoneFilesToProcess);

    for (const string &zoneFile : zoneFilesToProcess)
        PreProcessZoneFile(zoneFile);
}

void ZoneManager::DoAction(Widget *widget, double value)
{
    widget->LogInput(value);
    
    bool isUsed = false;
    
    DoAction(widget, value, isUsed);
}
    
void ZoneManager::DoAction(Widget *widget, double value, bool &isUsed)
{
    if (!widget->IsVirtual() && value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE && surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);
    
    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoAction(widget, isUsed, value);
    
    if (isUsed)
        return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoAction(widget, isUsed, value);

    if (isUsed)
        return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoAction(widget, isUsed, value);
    
    if (isUsed)
        return;
    
    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoAction(widget, isUsed, value);
    
    if (isUsed)
        return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoAction(widget, isUsed, value);
    
    if (isUsed)
        return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoAction(widget, isUsed, value);

    if (isUsed)
        return;

    if (homeZone_ != NULL)
        homeZone_->DoAction(widget, isUsed, value);
}

void ZoneManager::DoRelativeAction(Widget *widget, double delta)
{
    widget->LogInput(delta);
    
    bool isUsed = false;
    
    DoRelativeAction(widget, delta, isUsed);
}

void ZoneManager::DoRelativeAction(Widget *widget, double delta, bool &isUsed)
{
    if (surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);

    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoRelativeAction(widget, isUsed, delta);

    if (isUsed)
        return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoRelativeAction(widget, isUsed, delta);

    if (isUsed)
        return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoRelativeAction(widget, isUsed, delta);
    
    if (isUsed)
        return;
    
    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoRelativeAction(widget, isUsed, delta);
    
    if (isUsed)
        return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoRelativeAction(widget, isUsed, delta);
    
    if (isUsed)
        return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoRelativeAction(widget, isUsed, delta);
    
    if (isUsed)
        return;

    if (homeZone_ != NULL)
        homeZone_->DoRelativeAction(widget, isUsed, delta);
}

void ZoneManager::DoRelativeAction(Widget *widget, int accelerationIndex, double delta)
{
    widget->LogInput(delta);
    
    bool isUsed = false;
    
    DoRelativeAction(widget, accelerationIndex, delta, isUsed);
}

void ZoneManager::DoRelativeAction(Widget *widget, int accelerationIndex, double delta, bool &isUsed)
{
    if (surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);

    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed)
        return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;
    
    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed)
        return;

    if (homeZone_ != NULL)
        homeZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
}

void ZoneManager::DoTouch(Widget *widget, double value)
{
    widget->LogInput(value);
    
    bool isUsed = false;
    
    DoTouch(widget, value, isUsed);
}

void ZoneManager::DoTouch(Widget *widget, double value, bool &isUsed)
{
    surface_->TouchChannel(widget->GetChannelNumber(), value != 0);
       
    // GAW -- temporary
    //if (surface_->GetModifiers().GetSize() > 0 && value != 0.0) // ignore touch releases for Learn mode
        //WidgetMoved(this, widget, surface_->GetModifiers().Get()[0]);

    //if (learnFocusedFXZone_ != NULL)
        //learnFocusedFXZone_->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed)
        return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if (isUsed)
        return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if (isUsed)
        return;

    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if (isUsed)
        return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if (isUsed)
        return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed)
        return;

    if (homeZone_ != NULL)
        homeZone_->DoTouch(widget, widget->GetName(), isUsed, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ModifierManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ModifierManager::RecalculateModifiers()
{
    if (surface_ == NULL && page_ == NULL)
        return;
    
    if (modifierCombinations_.ResizeOK(1,false))
      modifierCombinations_.Get()[0] = 0 ;
           
    Modifiers activeModifierIndices[MaxModifiers];
    int activeModifierIndices_cnt = 0;
    
    for (int i = 0; i < MaxModifiers; ++i)
        if (modifiers_[i].isEngaged)
            activeModifierIndices[activeModifierIndices_cnt++] = (Modifiers)i;
    
    if (activeModifierIndices_cnt>0)
    {
        GetCombinations(activeModifierIndices,activeModifierIndices_cnt, modifierCombinations_);
        qsort(modifierCombinations_.Get(), modifierCombinations_.GetSize(), sizeof(modifierCombinations_.Get()[0]), intcmp_rev);
    }
    
    modifierVector_.clear();
    
    for (int i = 0; i < modifierCombinations_.GetSize(); ++i)
        modifierVector_.push_back(modifierCombinations_.Get()[i]);
    
    if (surface_ != NULL)
        surface_->GetZoneManager()->UpdateCurrentActionContextModifiers();
    else if (page_ != NULL)
        page_->UpdateCurrentActionContextModifiers();
}

void ModifierManager::SetLatchModifier(bool value, Modifiers modifier, int latchTime)
{
    if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
        const char* modifierName = stringFromModifier(modifier);
        DWORD keyReleasedTime = GetTickCount();
        DWORD heldTime = keyReleasedTime - modifiers_[modifier].pressedTime;
        if (heldTime >= (DWORD) latchTime) {
            if (modifiers_[modifier].isLocked == true) {
                modifiers_[modifier].isLocked = false;
                if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] [%s] UNLOCK\n", modifierName);
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s Unlock", modifierName);
                csi_->Speak(tmp);
            }
            modifiers_[modifier].isEngaged = false;
        } else {
            auto modifierName = stringFromModifier(modifier);
            if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] [%s] [LOCK]\n", modifierName);
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "%s Lock", modifierName);
            csi_->Speak(tmp);
            modifiers_[modifier].isLocked = true;
        }
        modifiers_[modifier].pressedTime = 0;
    } else {
        if (modifiers_[modifier].isEngaged == false) {
            modifiers_[modifier].isEngaged = true;
            modifiers_[modifier].pressedTime = GetTickCount();
        } else {
            modifiers_[modifier].pressedTime = 0;
        }
    }
    
    RecalculateModifiers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TrackNavigationManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void TrackNavigationManager::RebuildTracks()
{
    int oldTracksSize = (int) tracks_.size();

    tracks_.clear();

    if (isFolderViewActive_)
    {
        parentOfCurrentFolderTrack_ = nullptr;
        std::vector<MediaTrack*> ancestorStack;

        // Find the parent of the current folder track, and the index of the first track in the folder
        int trackID = 1;
        if (currentFolderTrackID_ > 0 && currentFolderTrackID_ < GetNumTracks())
        {
            for (; trackID <= GetNumTracks(); trackID++)
            {
                MediaTrack* track = CSurf_TrackFromID(trackID, followMCP_);
                int depthOffset = static_cast<int>(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH"));

                if (trackID == currentFolderTrackID_)
                {
                    if (depthOffset == 1)
                    {
                        trackID++; // The next track is the first one in the folder (a folder cannot be empty)
                    }
                    else // The currentFolderTrackID_ is not a folder actually
                    {
                        if (!ancestorStack.empty())
                        {
                            currentFolderTrackID_ = GetIdFromTrack(ancestorStack.back()); // Back to last parent folder
                            trackID = currentFolderTrackID_ + 1; // Next track is the first one in the folder
                            ancestorStack.pop_back();
                        }
                        else
                        {
                            currentFolderTrackID_ = 0; // Back to the root level
                            trackID = 1;
                        }
                    }
                    break;
                }

                if (depthOffset > 0)
                    ancestorStack.push_back(track);
                else if (depthOffset < 0 && !ancestorStack.empty())
                    ancestorStack.pop_back();
            }
        }

        // Set the parent folder ancestor stack
        if (!ancestorStack.empty())
            parentOfCurrentFolderTrack_ = ancestorStack.back();

        // List the tracks in the folder
        int relativeDepth = 0; // Where 0 is the level of the current folder content
        for (; trackID <= GetNumTracks(); trackID++)
        {
            MediaTrack* track = CSurf_TrackFromID(trackID, followMCP_);
            if (!track)
                continue;

            if (relativeDepth == 0 && IsTrackVisible(track, followMCP_))
                tracks_.push_back(track);

            relativeDepth += static_cast<int>(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH"));

            // Last track of the folder
            if (relativeDepth < 0)
                break;
        }
    }
    else
    {
        for (int i = 1; i <= GetNumTracks(); ++i)
        {
            MediaTrack* track = CSurf_TrackFromID(i, followMCP_);
            if (!track)
                continue;

            if (IsTrackVisible(track, followMCP_))
                tracks_.push_back(track);
        }
    }

    if (tracks_.size() < oldTracksSize)
    {
        for (int i = oldTracksSize; i > tracks_.size(); i--)
            page_->ForceClearTrack(i - trackOffset_);
    }

    if (tracks_.size() != oldTracksSize)
        page_->ForceUpdateTrackColors();
}

void TrackNavigationManager::RebuildSelectedTracks()
{
    if (currentTrackVCAFolderMode_ == TrackVCAFolderMode::VCA || currentTrackVCAFolderMode_ == TrackVCAFolderMode::Folder)
        return;

    int oldTracksSize = (int) selectedTracks_.size();
    
    selectedTracksInclMaster_.clear();
    selectedTracks_.clear();
    for (int i = 0; i <= GetNumTracks(); i++) {
        MediaTrack* track = GetTrackFromId(i);
        if (*(int*)GetSetMediaTrackInfo(track, "I_SELECTED", NULL)) {
            selectedTracksInclMaster_.push_back(track);
            if (i > 0) selectedTracks_.push_back(track);
        }
    }

    //FIXME compare all changes
    if (selectedTracks_.size() < oldTracksSize)
    {
        for (int i = oldTracksSize; i > selectedTracks_.size(); i--)
            page_->ForceClearTrack(i - selectedTracksOffset_);
    }
    
    if (selectedTracks_.size() != oldTracksSize)
        page_->ForceUpdateTrackColors();
}

void TrackNavigationManager::AdjustSelectedTrackBank(int amount)
{
    if (MediaTrack *selectedTrack = GetSelectedTrack())
    {
        int trackNum = GetIdFromTrack(selectedTrack);
        
        trackNum += amount;
        
        if (trackNum < 1)
            trackNum = 1;
        
        if (trackNum > GetNumTracks())
            trackNum = GetNumTracks();
        
        if (MediaTrack *trackToSelect = GetTrackFromId(trackNum))
        {
            SetOnlyTrackSelected(trackToSelect);
            if (GetScrollLink())
                SetMixerScroll(trackToSelect);

            page_->OnTrackSelection(trackToSelect);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ControlSurface::Stop()
{
    if (isRewinding_ || isFastForwarding_) // set the cursor to the Play position
        CSurf_OnPlay();
 
    page_->SignalStop();
    CancelRewindAndFastForward();
    CSurf_OnStop();
}

void ControlSurface::Play()
{
    page_->SignalPlay();
    CancelRewindAndFastForward();
    CSurf_OnPlay();
}

void ControlSurface::Record()
{
    page_->SignalRecord();
    CancelRewindAndFastForward();
    CSurf_OnRecord();
}

void ControlSurface::OnTrackSelection(MediaTrack *track)
{
    string onTrackSelection("OnTrackSelection");
    
    if (widgetsByName_.count(onTrackSelection) > 0)
    {
        if (GetMediaTrackInfo_Value(track, "I_SELECTED"))
            zoneManager_->DoAction(widgetsByName_[onTrackSelection].get(), 1.0);
        else
            zoneManager_->OnTrackDeselection();
        
        zoneManager_->OnTrackSelection();
    }
}

void ControlSurface::ForceClearTrack(int trackNum)
{
    for (auto widget : widgets_)
        if (widget->GetChannelNumber() + channelOffset_ == trackNum)
            widget->ForceClear();
}

void ControlSurface::ForceUpdateTrackColors()
{
    for (auto trackColorFeedbackProcessor : trackColorFeedbackProcessors_)
        trackColorFeedbackProcessor->ForceUpdateTrackColors();
}

rgba_color ControlSurface::GetTrackColorForChannel(int channel)
{
    rgba_color white;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    if (channel < 0 || channel >= numChannels_)
        return white;
    
    if (MediaTrack *track = page_->GetTrackNavigationManager()->GetNavigatorForChannel(channel + channelOffset_)->GetTrack())
        return DAW::GetTrackColor(track);
    else
        return white;
}

void ControlSurface::RequestUpdate()
{
    for (auto widget : widgets_)
        widget->ClearHasBeenUsedByUpdate();

    zoneManager_->RequestUpdate();

    const PropertyList properties;
    
    for (auto &widget : widgets_)
    {
        if ( ! widget->GetHasBeenUsedByUpdate())
        {
            widget->SetHasBeenUsedByUpdate();
            
            rgba_color color;
            widget->UpdateValue(properties, 0.0);
            widget->UpdateValue(properties, "");
            widget->UpdateColorValue(color);
        }
    }

    if (isRewinding_)
    {
        if (GetCursorPosition() == 0)
            StopRewinding();
        else
        {
            CSurf_OnRew(0);

            if (speedX5_ == true)
            {
                CSurf_OnRew(0);
                CSurf_OnRew(0);
                CSurf_OnRew(0);
                CSurf_OnRew(0);
            }
        }
    }
        
    else if (isFastForwarding_)
    {
        if (GetCursorPosition() > GetProjectLength(NULL))
            StopFastForwarding();
        else
        {
            CSurf_OnFwd(0);
            
            if (speedX5_ == true)
            {
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
            }
        }
    }
}

bool ControlSurface::GetShift()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetShift();
    else
        return page_->GetModifierManager()->GetShift();
}

bool ControlSurface::GetOption()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetOption();
    else
        return page_->GetModifierManager()->GetOption();
}

bool ControlSurface::GetControl()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetControl();
    else
        return page_->GetModifierManager()->GetControl();
}

bool ControlSurface::GetAlt()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetAlt();
    else
        return page_->GetModifierManager()->GetAlt();
}

bool ControlSurface::GetFlip()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetFlip();
    else
        return page_->GetModifierManager()->GetFlip();
}

bool ControlSurface::GetGlobal()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetGlobal();
    else
        return page_->GetModifierManager()->GetGlobal();
}

bool ControlSurface::GetMarker()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetMarker();
    else
        return page_->GetModifierManager()->GetMarker();
}

bool ControlSurface::GetNudge()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetNudge();
    else
        return page_->GetModifierManager()->GetNudge();
}

bool ControlSurface::GetZoom()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetZoom();
    else
        return page_->GetModifierManager()->GetZoom();
}

bool ControlSurface::GetScrub()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetScrub();
    else
        return page_->GetModifierManager()->GetScrub();
}

void ControlSurface::SetModifierValue(int value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
        modifierManager_->SetModifierValue(value);
    else if (usesLocalModifiers_)
        modifierManager_->SetModifierValue(value);
    else
        page_->GetModifierManager()->SetModifierValue(value);
}

void ControlSurface::SetShift(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetShift(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetShift(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetShift(value, latchTime_);
    else
        page_->GetModifierManager()->SetShift(value, latchTime_);
}

void ControlSurface::SetOption(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetOption(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetOption(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetOption(value, latchTime_);
    else
        page_->GetModifierManager()->SetOption(value, latchTime_);
}

void ControlSurface::SetControl(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetControl(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetControl(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetControl(value, latchTime_);
    else
        page_->GetModifierManager()->SetControl(value, latchTime_);
}

void ControlSurface::SetAlt(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetAlt(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetAlt(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetAlt(value, latchTime_);
    else
        page_->GetModifierManager()->SetAlt(value, latchTime_);
}

void ControlSurface::SetFlip(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetFlip(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetFlip(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetFlip(value, latchTime_);
    else
        page_->GetModifierManager()->SetFlip(value, latchTime_);
}

void ControlSurface::SetGlobal(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetGlobal(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetGlobal(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetGlobal(value, latchTime_);
    else
        page_->GetModifierManager()->SetGlobal(value, latchTime_);
}

void ControlSurface::SetMarker(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetMarker(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetMarker(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetMarker(value, latchTime_);
    else
        page_->GetModifierManager()->SetMarker(value, latchTime_);
}

void ControlSurface::SetNudge(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetNudge(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetNudge(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetNudge(value, latchTime_);
    else
        page_->GetModifierManager()->SetNudge(value, latchTime_);
}

void ControlSurface::SetZoom(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetZoom(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetZoom(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetZoom(value, latchTime_);
    else
        page_->GetModifierManager()->SetZoom(value, latchTime_);
}

void ControlSurface::SetScrub(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetScrub(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetScrub(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetScrub(value, latchTime_);
    else
        page_->GetModifierManager()->SetScrub(value, latchTime_);
}

const vector<int> &ControlSurface::GetModifiers()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetModifiers();
    else
        return page_->GetModifierManager()->GetModifiers();
}

void ControlSurface::ClearModifier(const char *modifier)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->ClearModifier(modifier);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->ClearModifier(modifier);
    }
    else if (usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifier(modifier);
    else
        page_->GetModifierManager()->ClearModifier(modifier);
}

void ControlSurface::ClearModifiers()
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->ClearModifiers();
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->ClearModifiers();
    }
    else if (usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifiers();
    else
        page_->GetModifierManager()->ClearModifiers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurfaceIO
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_ControlSurfaceIO::HandleExternalInput(Midi_ControlSurface *surface)
{
    if (midiInput_)
    {
        midiInput_->SwapBufsPrecise(GetTickCount(), GetTickCount());
        MIDI_eventlist *list = midiInput_->GetReadBuf();
        int bpos = 0;
        MIDI_event_t *evt;
        while ((evt = list->EnumItems(&bpos)))
            surface->ProcessMidiMessage((MIDI_event_ex_t*)evt);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
Midi_ControlSurface::Midi_ControlSurface(CSurfIntegrator *const csi, IPageContext *page, const char *name, int channelOffset, const char *surfaceFile, const char *zoneFolder, const char *fxZoneFolder, Midi_ControlSurfaceIO *surfaceIO)
: ControlSurface(csi, page, name, surfaceIO->GetChannelCount(), channelOffset), surfaceIO_(surfaceIO)
{
    MidiWidgetRegistry::EnsureRegistered();
    ProcessMIDIWidgetFile(surfaceFile, this);
    InitHardwiredWidgets(this);
    InitializeMeters();
    ParseOSKLayout(surfaceFile);
    InitZoneManager(csi_, this, zoneFolder, fxZoneFolder);
}

void Midi_ControlSurface::ProcessMidiMessage(const MIDI_event_ex_t *evt)
{
    if (g_surfaceRawInDisplay)
    {
        LogToConsole("IN <- %s %02x %02x %02x \n", name_.c_str(), evt->midi_message[0], evt->midi_message[1], evt->midi_message[2]);
        // LogStackTraceToConsole();
    }

    string threeByteKey = to_string(evt->midi_message[0]  * 0x10000 + evt->midi_message[1]  * 0x100 + evt->midi_message[2]);
    string twoByteKey = to_string(evt->midi_message[0]  * 0x10000 + evt->midi_message[1]  * 0x100);
    string oneByteKey = to_string(evt->midi_message[0] * 0x10000);

    // At this point we don't know how much of the message comprises the key, so try all three
    if (CSIMessageGeneratorsByMessage_.find(threeByteKey) != CSIMessageGeneratorsByMessage_.end())
        CSIMessageGeneratorsByMessage_[threeByteKey]->ProcessMidiMessage(evt);
    else if (CSIMessageGeneratorsByMessage_.find(twoByteKey) != CSIMessageGeneratorsByMessage_.end())
        CSIMessageGeneratorsByMessage_[twoByteKey]->ProcessMidiMessage(evt);
    else if (CSIMessageGeneratorsByMessage_.find(oneByteKey) != CSIMessageGeneratorsByMessage_.end())
        CSIMessageGeneratorsByMessage_[oneByteKey]->ProcessMidiMessage(evt);
}

void Midi_ControlSurface::SendMidiSysExMessage(MIDI_event_ex_t *midiMessage)
{
    surfaceIO_->QueueMidiSysExMessage(midiMessage);
    
    if (g_surfaceOutDisplay)
    {
        string output = "OUT->";
        
        output += name_ + " ";

        char buf[32];

        for (int i = 0; i < midiMessage->size; ++i)
        {
            snprintf(buf, sizeof(buf), "%02x ", midiMessage->midi_message[i]);
            output += buf;
        }
        
        output += " # Midi_ControlSurface::SendMidiSysExMessage\n";

        ShowConsoleMsg(output.c_str());
    }
}

void Midi_ControlSurface::SendMidiMessage(int first, int second, int third)
{
    surfaceIO_->SendMidiMessage(first, second, third);
    
    if (g_surfaceOutDisplay) LogToConsole("%s %02x %02x %02x # Midi_ControlSurface::SendMidiMessage\n", ("OUT->" + name_).c_str(), first, second, third);
}

 ////////////////////////////////////////////////////////////////////////////////////////////////////////
 // OSC_ControlSurfaceIO
 ////////////////////////////////////////////////////////////////////////////////////////////////////////

OSC_X32ControlSurfaceIO::OSC_X32ControlSurfaceIO(CSurfIntegrator *const csi, const char *surfaceName, int channelCount, const char *receiveOnPort, const char *transmitToPort, const char *transmitToIpAddress, int maxPacketsPerRun) : OSC_ControlSurfaceIO(csi, surfaceName, channelCount, receiveOnPort, transmitToPort, transmitToIpAddress, maxPacketsPerRun) {}

OSC_ControlSurfaceIO::OSC_ControlSurfaceIO(CSurfIntegrator *const csi, const char *surfaceName, int channelCount, const char *receiveOnPort, const char *transmitToPort, const char *transmitToIpAddress, int maxPacketsPerRun) : csi_(csi), name_(surfaceName), channelCount_(channelCount)
{
    // private:
    maxPacketsPerRun_ = maxPacketsPerRun < 0 ? 0 : maxPacketsPerRun;

    if (!IsSameString(receiveOnPort, transmitToPort)) {
        inSocket_  = GetInputSocketForPort(surfaceName, atoi(receiveOnPort));
        outSocket_ = GetOutputSocketForAddressAndPort(surfaceName, transmitToIpAddress, atoi(transmitToPort));
    }
    else // WHEN INPUT AND OUTPUT SOCKETS ARE THE SAME -- DO MAGIC :)
    {
        oscpkt::UdpSocket *inSocket = GetInputSocketForPort(surfaceName, atoi(receiveOnPort));

        struct addrinfo hints;
        struct addrinfo *addressInfo;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;      // IPV4
        hints.ai_socktype = SOCK_DGRAM; // UDP
        hints.ai_flags = 0x00000001;    // socket address is intended for bind
        getaddrinfo(transmitToIpAddress, transmitToPort, &hints, &addressInfo);
        memcpy(&(inSocket->remote_addr), (void*)(addressInfo->ai_addr), addressInfo->ai_addrlen);

        inSocket_  = inSocket;
        outSocket_ = inSocket;
    }
 }

OSC_ControlSurfaceIO::~OSC_ControlSurfaceIO()
{
    Sleep(33);
    
    int count = 0;
    while (packetQueue_.GetSize()>=sizeof(int) && ++count < 100)
    {
        BeginRun();
        if (count) Sleep(33);
    }

    if (inSocket_)
    {
        for (int x = 0; x < s_inputSockets.GetSize(); ++x)
        {
            if (s_inputSockets.Get(x)->socket == inSocket_)
            {
                if (!--s_inputSockets.Get(x)->refcnt)
                    s_inputSockets.Delete(x, true);
                break;
            }
        }
    }
    if (outSocket_ && outSocket_ != inSocket_)
    {
        for (int x = 0; x < s_outputSockets.GetSize(); ++x)
        {
            if (s_outputSockets.Get(x)->socket == outSocket_)
            {
                if (!--s_outputSockets.Get(x)->refcnt)
                    s_outputSockets.Delete(x, true);
                break;
            }
        }
    }
}

void OSC_ControlSurfaceIO::HandleExternalInput(OSC_ControlSurface *surface)
{
   if (inSocket_ != NULL && inSocket_->isOk())
   {
       while (inSocket_->receiveNextPacket(0))  // timeout, in ms
       {
           packetReader_.init(inSocket_->packetData(), inSocket_->packetSize());
           oscpkt::Message *message;
           
           while (packetReader_.isOk() && (message = packetReader_.popMessage()) != 0)
           {
               if (message->arg().isFloat())
               {
                   float value = 0;
                   message->arg().popFloat(value);
                   surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
               }
               else if (message->arg().isInt32())
               {
                   int value;
                   message->arg().popInt32(value);
                   surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
               }
           }
       }
   }
}

void OSC_X32ControlSurfaceIO::HandleExternalInput(OSC_ControlSurface *surface)
{
   if (inSocket_ != NULL && inSocket_->isOk())
   {
       while (inSocket_->receiveNextPacket(0))  // timeout, in ms
       {
           packetReader_.init(inSocket_->packetData(), inSocket_->packetSize());
           oscpkt::Message *message;
           
           while (packetReader_.isOk() && (message = packetReader_.popMessage()) != 0)
           {
               if (message->arg().isFloat())
               {
                   float value = 0;
                   message->arg().popFloat(value);
                   surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
               }
               else if (message->arg().isInt32())
               {
                   int value;
                   message->arg().popInt32(value);
                   
                   if (message->addressPattern() == "/-stat/selidx")
                   {
                       string x32Select = message->addressPattern() + "/";
                       
                       if (value < 10)
                           x32Select += "0";

                       char buf[64];
                       snprintf(buf, sizeof(buf), "%d", value);
                       x32Select += buf;
                                              
                       surface->ProcessOSCMessage(x32Select.c_str(), 1.0);
                   }
                   else
                       surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
               }
           }
       }
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
OSC_ControlSurface::OSC_ControlSurface(CSurfIntegrator *const csi, IPageContext *page, const char *name, int channelOffset, const char *templateFilename, const char *zoneFolder, const char *fxZoneFolder, OSC_ControlSurfaceIO *surfaceIO) : ControlSurface(csi, page, name, surfaceIO->GetChannelCount(), channelOffset), surfaceIO_(surfaceIO)

{
    ProcessOSCWidgetFile(templateFilename);
    InitHardwiredWidgets(this);
    ParseOSKLayout(templateFilename);
    InitZoneManager(csi_, this, zoneFolder, fxZoneFolder);
}

void OSC_ControlSurface::ProcessOSCMessage(const char *message, double value)
{
    if (CSIMessageGeneratorsByMessage_.find(message) != CSIMessageGeneratorsByMessage_.end())
        CSIMessageGeneratorsByMessage_[message]->ProcessMessage(value);
    
    if (g_surfaceInDisplay) LogToConsole("IN <- %s %s %f\n", name_.c_str(), message, value);
}

void OSC_ControlSurface::SendOSCMessage(const char *zoneName)
{
    string oscAddress(zoneName);
    ReplaceAllWith(oscAddress, s_BadFileChars, "_");
    oscAddress = "/" + oscAddress;

    surfaceIO_->SendOSCMessage(oscAddress.c_str());
        
    if (g_surfaceOutDisplay) LogToConsole("->LoadingZone---->%s\n", name_.c_str());
}

void OSC_ControlSurface::SendOSCMessage(const char *oscAddress, int value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %d # Surface::SendOSCMessage 1\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(const char *oscAddress, double value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %f # Surface::SendOSCMessage 2\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(const char *oscAddress, const char *value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %s # Surface::SendOSCMessage 3\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, double value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
    
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %f # Surface::SendOSCMessage 4\n", feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, int value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);

    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %s %d # Surface::SendOSCMessage 5\n", name_.c_str(), feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, const char *value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);

    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %s %s # Surface::SendOSCMessage 6\n", name_.c_str(), feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
static struct
{
    MIDI_event_ex_t evt;
    char data[MEDBUF];
} s_midiSysExData;

void Midi_ControlSurface::SendSysexInitData(int line[], int numElem)
{
    memset(s_midiSysExData.data, 0, sizeof(s_midiSysExData.data));

    s_midiSysExData.evt.frame_offset=0;
    s_midiSysExData.evt.size=0;
    
    for (int i = 0; i < numElem; ++i)
        s_midiSysExData.evt.midi_message[s_midiSysExData.evt.size++] = line[i];
    
    SendMidiSysExMessage(&s_midiSysExData.evt);
}

void Midi_ControlSurface::InitializeMCU()
{
    int line1[] = { 0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7 };
    int line2[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x00, 0xF7 };
    int line3[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x21, 0x01, 0xF7 };
    int line4[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x00, 0x01, 0xF7 };
    int line5[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x01, 0x01, 0xF7 };
    int line6[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x02, 0x01, 0xF7 };
    int line7[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x03, 0x01, 0xF7 };
    int line8[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x04, 0x01, 0xF7 };
    int line9[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x05, 0x01, 0xF7 };
    int line10[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x06, 0x01, 0xF7 };
    int line11[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x07, 0x01, 0xF7 };

    SendSysexInitData(line1, NUM_ELEM(line1));
    SendSysexInitData(line2, NUM_ELEM(line2));
    SendSysexInitData(line3, NUM_ELEM(line3));
    SendSysexInitData(line4, NUM_ELEM(line4));
    SendSysexInitData(line5, NUM_ELEM(line5));
    SendSysexInitData(line6, NUM_ELEM(line6));
    SendSysexInitData(line7, NUM_ELEM(line7));
    SendSysexInitData(line8, NUM_ELEM(line8));
    SendSysexInitData(line9, NUM_ELEM(line9));
    SendSysexInitData(line10, NUM_ELEM(line10));
    SendSysexInitData(line11, NUM_ELEM(line11));
}

void Midi_ControlSurface::InitializeMCUXT()
{
    int line1[] = { 0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7 };
    int line2[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x00, 0xF7 };
    int line3[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x21, 0x01, 0xF7 };
    int line4[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x00, 0x01, 0xF7 };
    int line5[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x01, 0x01, 0xF7 };
    int line6[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x02, 0x01, 0xF7 };
    int line7[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x03, 0x01, 0xF7 };
    int line8[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x04, 0x01, 0xF7 };
    int line9[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x05, 0x01, 0xF7 };
    int line10[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x06, 0x01, 0xF7 };
    int line11[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x07, 0x01, 0xF7 };
    
    SendSysexInitData(line1, NUM_ELEM(line1));
    SendSysexInitData(line2, NUM_ELEM(line2));
    SendSysexInitData(line3, NUM_ELEM(line3));
    SendSysexInitData(line4, NUM_ELEM(line4));
    SendSysexInitData(line5, NUM_ELEM(line5));
    SendSysexInitData(line6, NUM_ELEM(line6));
    SendSysexInitData(line7, NUM_ELEM(line7));
    SendSysexInitData(line8, NUM_ELEM(line8));
    SendSysexInitData(line9, NUM_ELEM(line9));
    SendSysexInitData(line10, NUM_ELEM(line10));
    SendSysexInitData(line11, NUM_ELEM(line11));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSurfIntegrator
////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char * const Control_Surface_Integrator = "Control Surface Integrator";

CSurfIntegrator::CSurfIntegrator()
{
    InitActionsDictionary();

    int size = 0;
    int index = projectconfig_var_getoffs("projtimemode", &size);
    timeModeOffs_ = size==4 ? index : -1;
    
    index = projectconfig_var_getoffs("projtimemode2", &size);
    timeMode2Offs_ = size==4 ? index : -1;
    
    index = projectconfig_var_getoffs("projmeasoffs", &size);
    measOffsOffs_ = size==4 ? index : - 1;
    
    index = projectconfig_var_getoffs("projtimeoffs", &size);
    timeOffsOffs_ = size==8 ? index : -1;
    
    index = projectconfig_var_getoffs("panmode", &size);
    projectPanModeOffs_ = size==4 ? index : -1;

    // these are supported by ~7.10+, previous versions we fallback to get_config_var() on-demand
    index = projectconfig_var_getoffs("projmetrov1", &size);
    projectMetronomePrimaryVolumeOffs_ = size==8 ? index : -1;

    index = projectconfig_var_getoffs("projmetrov2", &size);
    projectMetronomeSecondaryVolumeOffs_ = size==8 ? index : -1;
}

CSurfIntegrator::~CSurfIntegrator()
{
    Shutdown();

    midiSurfacesIO_.clear();
    oscSurfacesIO_.clear();
    pages_.clear();
    actions_.clear();
}

const char *CSurfIntegrator::GetTypeString()
{
    return "CSI";
}

const char *CSurfIntegrator::GetDescString()
{
    return Control_Surface_Integrator;
}

const char *CSurfIntegrator::GetConfigString() // string of configuration data
{
    snprintf(configtmp, sizeof(configtmp),"0 0");
    return configtmp;
}

int CSurfIntegrator::Extended(int call, void *parm1, void *parm2, void *parm3)
{
    if (call == CSURF_EXT_SUPPORTS_EXTENDED_TOUCH)
    {
        return 1;
    }
    
    if (call == CSURF_EXT_RESET)
    {
       Init();
    }
    
    if (call == CSURF_EXT_SETFXCHANGE)
    {
        // parm1=(MediaTrack*)track, whenever FX are added, deleted, or change order
        TrackFXListChanged((MediaTrack*)parm1);
    }
        
    if (call == CSURF_EXT_SETMIXERSCROLL)
    {
        MediaTrack *leftPtr = (MediaTrack *)parm1;
        
        int offset = CSurf_TrackToID(leftPtr, true);
        
        offset--;
        
        if (offset < 0)
            offset = 0;
            
        SetTrackOffset(offset);
    }
        
    return 1;
}

static IReaperControlSurface *createFunc(const char *type_string, const char *configString, int *errStats)
{
    return new CSurfIntegrator();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ControlSurface - OSK (On-Screen Keyboard) Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////

void ControlSurface::ParseOskProperties(const string &propsPart, OskWidgetInfo &info) {
    if (propsPart.empty()) return;
    
    vector<string> tokens;
    GetTokens(tokens, propsPart);
    
    for (const auto &token : tokens) {
        if (token == "OSKHidden") {
            info.hidden = true;
            continue;
        }
        
        auto eqPos = token.find('=');
        if (eqPos == string::npos) continue;
        
        string key = token.substr(0, eqPos);
        string val = token.substr(eqPos + 1);
        
        // Remove quotes if present
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);
        
        if (key == "Shape") info.shape = val;
        else if (key == "Width") info.width = (float)atof(val.c_str());
        else if (key == "Height") info.height = (float)atof(val.c_str());
        else if (key == "Group") info.group = val;
        else if (key == "Label") info.label = val;
        else if (key == "Color") info.color = val;
    }
}

void ControlSurface::BuildCachedLayoutString() {
    cachedOskLayoutString_.clear();
    
    for (const auto &row : oskLayout_) {
        if (!cachedOskLayoutString_.empty())
            cachedOskLayoutString_ += "\n";
        
        bool firstCell = true;
        for (const auto &cell : row.cells) {
            if (!firstCell) cachedOskLayoutString_ += "|";
            firstCell = false;
            
            if (cell.isSpacer) {
                char buf[32];
                snprintf(buf, sizeof(buf), "SPACER:%.2f", cell.spacerWidth);
                cachedOskLayoutString_ += buf;
            } else {
                cachedOskLayoutString_ += cell.widget.name;
                cachedOskLayoutString_ += ":Shape=";
                cachedOskLayoutString_ += cell.widget.shape;
                
                char buf[64];
                snprintf(buf, sizeof(buf), ",Width=%.2f,Height=%.2f", cell.widget.width, cell.widget.height);
                cachedOskLayoutString_ += buf;
                
                if (!cell.widget.group.empty()) {
                    cachedOskLayoutString_ += ",Group=";
                    cachedOskLayoutString_ += cell.widget.group;
                }
                if (!cell.widget.label.empty()) {
                    cachedOskLayoutString_ += ",Label=";
                    cachedOskLayoutString_ += cell.widget.label;
                }
                if (!cell.widget.color.empty()) {
                    cachedOskLayoutString_ += ",Color=";
                    cachedOskLayoutString_ += cell.widget.color;
                }
            }
        }
    }
}

void ControlSurface::ParseOSKLayout(const string &surfaceFilePath) {
    surfaceFilePath_ = surfaceFilePath;
    oskLayout_.clear();
    
    try {
        ifstream file(surfaceFilePath);
        if (!file.is_open()) return;
        
        OskRow currentRow;
        bool hasRow = false;
        
        for (string line; getline(file, line);) {
            TrimLine(line);
            
            // Check for # OSKRow
            if (line == "# OSKRow") {
                if (hasRow && !currentRow.cells.empty())
                    oskLayout_.push_back(std::move(currentRow));
                currentRow = OskRow();
                hasRow = true;
                continue;
            }
            
            // Check for # OSKSpacer Width=N
            if (line.find("# OSKSpacer") == 0) {
                float width = 0.5f;
                auto pos = line.find("Width=");
                if (pos != string::npos)
                    width = (float)atof(line.c_str() + pos + 6);
                if (hasRow) {
                    OskCell cell;
                    cell.isSpacer = true;
                    cell.spacerWidth = width;
                    currentRow.cells.push_back(cell);
                }
                continue;
            }
            
            // Check for Widget line with possible # properties
            if (line.find("Widget ") == 0) {
                auto hashPos = line.find('#');
                string widgetPart = (hashPos != string::npos) ? line.substr(0, hashPos) : line;
                string propsPart = (hashPos != string::npos) ? line.substr(hashPos + 1) : "";
                
                vector<string> tokens;
                GetTokens(tokens, widgetPart);
                if (tokens.size() < 2) continue;
                
                OskWidgetInfo info;
                info.name = tokens[1];
                
                ParseOskProperties(propsPart, info);
                
                if (info.hidden) continue;  // Skip hidden widgets
                
                if (!hasRow) {
                    currentRow = OskRow();
                    hasRow = true;
                }
                
                OskCell cell;
                cell.isSpacer = false;
                cell.widget = info;
                currentRow.cells.push_back(cell);
            }
        }
        
        if (hasRow && !currentRow.cells.empty())
            oskLayout_.push_back(std::move(currentRow));
        
        BuildCachedLayoutString();
        
    } catch (const std::exception &e) {
        LogToConsole("[ERROR] ParseOSKLayout failed for %s: %s\n", surfaceFilePath.c_str(), e.what());
    }
}

void ControlSurface::PublishOSKLayout() {
    if (!isOskEnabled_) return;
    
    string key = string("Layout_") + name_;
    ::SetExtState("CSI_OSK", key.c_str(), cachedOskLayoutString_.c_str(), false);
}

void ControlSurface::PublishOSKState() {
    if (!isOskEnabled_) return;
    
    // Throttle to ~10Hz (every 3rd call of 30Hz)
    if (++oskRunCounter_ < 3) return;
    oskRunCounter_ = 0;
    
    string state;
    
    for (const auto &row : oskLayout_) {
        for (const auto &cell : row.cells) {
            if (cell.isSpacer) continue;
            
            Widget *w = GetWidgetByName(cell.widget.name);
            if (!w) continue;
            
            double value = w->GetLastFeedbackValue();
            rgba_color color = w->GetLastFeedbackColor();
            
            if (!state.empty()) state += ";";
            
            char buf[128];
            snprintf(buf, sizeof(buf), "%s=V:%.2f,C:#%02X%02X%02X", 
                cell.widget.name.c_str(), value,
                (unsigned char)color.r, (unsigned char)color.g, (unsigned char)color.b);
            state += buf;
        }
    }
    
    if (state != cachedOskStateString_) {
        cachedOskStateString_ = state;
        string key = string("State_") + name_;
        ::SetExtState("CSI_OSK", key.c_str(), state.c_str(), false);
    }
}

void ControlSurface::PublishOSKLabels() {
    if (!isOskEnabled_) return;
    if (!zoneManager_) return;
    
    string labels;
    
    // Collect labels from active zones
    // Walk all widgets and find their current ActionContexts, look for KeyLabel property
    for (const auto &row : oskLayout_) {
        for (const auto &cell : row.cells) {
            if (cell.isSpacer) continue;
            
            Widget *w = GetWidgetByName(cell.widget.name);
            if (!w) continue;
            
            // Get the current action contexts for this widget (respects modifiers)
            const auto &contexts = zoneManager_->GetCurrentActionContextsForWidget(w);
            
            string keyLabel;
            for (const auto &ctx : contexts) {
                const char *kl = ctx->GetWidgetProperties().get_prop(PropertyType_KeyLabel);
                if (kl && kl[0] != '\0') {
                    keyLabel = kl;
                    break;
                }
            }
            
            // Fallback: use action title
            if (keyLabel.empty()) {
                for (const auto &ctx : contexts) {
                    const char *title = ctx->GetActionTitle();
                    if (title && title[0] != '\0') {
                        keyLabel = title;
                        break;
                    }
                }
            }
            
            // Final fallback: widget name or label override
            if (keyLabel.empty()) {
                if (!cell.widget.label.empty())
                    keyLabel = cell.widget.label;
                else
                    keyLabel = cell.widget.name;
            }
            
            if (!labels.empty()) labels += ";";
            labels += cell.widget.name;
            labels += "=";
            labels += keyLabel;
        }
    }
    
    if (labels != cachedOskLabelsString_) {
        cachedOskLabelsString_ = labels;
        string key = string("Labels_") + name_;
        ::SetExtState("CSI_OSK", key.c_str(), labels.c_str(), false);
    }
}

static HWND configFunc(const char *type_string, HWND parent, const char *initConfigString)
{
    HWND hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_CSI), parent, dlgProcMainConfig, (LPARAM)initConfigString);
    if (hwnd) g_openDialogs.push_back(hwnd);
    return hwnd;
}

reaper_csurf_reg_t csurf_integrator_reg =
{
    "CSI",
    Control_Surface_Integrator,
    createFunc,
    configFunc,
};


void localize_init(void * (*GetFunc)(const char *name))
{
    *(void **)&importedLocalizeFunc = GetFunc("__localizeFunc");
    *(void **)&importedLocalizeMenu = GetFunc("__localizeMenu");
    *(void **)&importedLocalizeInitializeDialog = GetFunc("__localizeInitializeDialog");
    *(void **)&importedLocalizePrepareDialog = GetFunc("__localizePrepareDialog");
}
