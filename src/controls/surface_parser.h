#pragma once
//
//  surface_parser.h — SurfaceTemplateParser: extracts surface template file
//  parsing out of Midi_ControlSurface and OSC_ControlSurface.
//
//  Previously each surface class contained two private methods:
//    ProcessMidiWidget / ProcessMIDIWidgetFile  (Midi_ControlSurface)
//    ProcessOSCWidget  / ProcessOSCWidgetFile   (OSC_ControlSurface)
//
//  Those bodies now live in this class/file.  The surface member functions
//  become one-line delegates.  SurfaceTemplateParser is declared friend of
//  ControlSurface (and the two concrete subclasses) so it can reach the
//  protected members (csi_, CSIMessageGeneratorsByMessage_, stepSize_, etc.)
//  without exposing them as public API.
//
#include "preamble.h"

// Forward declarations — full headers included in surface_parser.cpp only.
class Midi_ControlSurface;
class OSC_ControlSurface;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SurfaceTemplateParser
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    // Parse a MIDI surface template file (.mst / .ost) and populate the surface.
    // Equivalent to the body of Midi_ControlSurface::ProcessMIDIWidgetFile.
    static void ParseMidiTemplate(const string &filePath, Midi_ControlSurface *surface);

    // Parse an OSC surface template file and populate the surface.
    // Equivalent to the body of OSC_ControlSurface::ProcessOSCWidgetFile.
    static void ParseOSCTemplate(const string &filePath, OSC_ControlSurface *surface);

    // Per-widget-block helpers.  Called from ParseMidiTemplate/ParseOSCTemplate
    // above, and also from the thin-delegate member functions in integrator.cpp.
    static void ParseMidiWidget(int &lineNumber, ifstream &file,
                                const vector<string> &tokens,
                                Midi_ControlSurface *surface);

    static void ParseOSCWidget(int &lineNumber, ifstream &file,
                               const vector<string> &tokens,
                               OSC_ControlSurface *surface);
};
