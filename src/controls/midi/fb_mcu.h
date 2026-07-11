#pragma once
// fb_mcu.h — MCU-protocol feedback processors: MCUVUMeter, MCUDisplay, FB_MCU_AssignmentDisplay, MCU_TimeDisplay.

class MCUVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
protected:
    int displayType_;
    int channelNumber_;
    int lastMidiValue_;
    bool isClipOn_;

public:
    virtual ~MCUVUMeter_Midi_FeedbackProcessor() {}
    MCUVUMeter_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int displayType, int channelNumber)
        : Midi_FeedbackProcessor(csi, surface, widget), displayType_(displayType), channelNumber_(channelNumber) {
        lastMidiValue_ = 0;
        isClipOn_ = false;
    }

    virtual const char* GetName() override { return "MCUVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        SendMidiMessage(0xd0, (channelNumber_ << 4) | GetMidiValue(properties, value), 0);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        ForceMidiMessage(0xd0, (channelNumber_ << 4) | GetMidiValue(properties, value), 0);
    }

    int GetMidiValue(const PropertyList& properties, double value) {
        int midiValue = 0;
        double dbValue = VAL2DB(normalizedToVol(value));

        const char* meterMode = nullptr;
        PropertyType propertyType = properties.prop_from_string("MeterMode");
        if (propertyType)
            meterMode = (char*) properties.get_prop(propertyType);
        if (!meterMode)
            meterMode = "XTOUCH";

        if (STRICASECMP(meterMode, "XTouch") == 0) {
            if      (dbValue >= -60.3 && dbValue < -54.1)  midiValue = 0x01;
            else if (dbValue >= -54.1 && dbValue < -48.2)  midiValue = 0x02;
            else if (dbValue >= -48.2 && dbValue < -42.1)  midiValue = 0x03;
            else if (dbValue >= -42.1 && dbValue < -36.2)  midiValue = 0x04;
            else if (dbValue >= -36.2 && dbValue < -30.1)  midiValue = 0x05;
            else if (dbValue >= -30.1 && dbValue < -18.1)  midiValue = 0x06;
            else if (dbValue >= -18.1 && dbValue < -15.1)  midiValue = 0x07;
            else if (dbValue >= -15.1 && dbValue < -12.1)  midiValue = 0x08;
            else if (dbValue >= -12.1 && dbValue < -9.1)   midiValue = 0x09;
            else if (dbValue >= -9.1  && dbValue < -6.1)   midiValue = 0x0a;
            else if (dbValue >= -6.1  && dbValue < -4.6)   midiValue = 0x0b;
            else if (dbValue >= -4.6  && dbValue < -3.1)   midiValue = 0x0c;
            else if (dbValue >= -3.1  && dbValue <= 0.1)   midiValue = 0x0d;
            else if (dbValue >   0.1)                      midiValue = 0x0e;
        } else if (STRICASECMP(meterMode, "MCU") == 0) {
            midiValue = int(value * 0x0f);
            if (midiValue > 0x0e)
                midiValue = 0x0e;
        } else if (STRICASECMP(meterMode, "SSLNucleus2") == 0) {
            if      (dbValue >= -40.5 && dbValue < -30.5) midiValue = 0x03;
            else if (dbValue >= -30.5 && dbValue < -20.5) midiValue = 0x04;
            else if (dbValue >= -20.5 && dbValue < -14.5) midiValue = 0x05;
            else if (dbValue >= -14.5 && dbValue < -10.5) midiValue = 0x06;
            else if (dbValue >= -10.5 && dbValue < -8.5 ) midiValue = 0x07;
            else if (dbValue >= -8.5  && dbValue < -6.5 ) midiValue = 0x08;
            else if (dbValue >= -6.5  && dbValue < -4.5 ) midiValue = 0x09;
            else if (dbValue >= -4.5  && dbValue < -2.5 ) midiValue = 0x0a;
            else if (dbValue >= -2.5  && dbValue <  0   ) midiValue = 0x0b;
            else if (dbValue >=  0                      ) midiValue = 0x0c;
        } else if (STRICASECMP(meterMode, "IconV1M") == 0) {
            if      (dbValue >= -60.1 && dbValue < -48.1)  midiValue = 0x01;
            else if (dbValue >= -48.1 && dbValue < -42.1)  midiValue = 0x02;
            else if (dbValue >= -42.1 && dbValue < -36.1)  midiValue = 0x03;
            else if (dbValue >= -36.1 && dbValue < -30.1)  midiValue = 0x04;
            else if (dbValue >= -30.1 && dbValue < -24.1)  midiValue = 0x05;
            else if (dbValue >= -24.1 && dbValue < -18.1)  midiValue = 0x06;
            else if (dbValue >= -18.1 && dbValue < -12.1)  midiValue = 0x07;
            else if (dbValue >= -12.1 && dbValue < -9.1 )  midiValue = 0x08;
            else if (dbValue >= -9.1  && dbValue < -6.1 )  midiValue = 0x09;
            else if (dbValue >= -6.1  && dbValue < -3.1 )  midiValue = 0x0A;
            else if (dbValue >= -3.1  && dbValue <  0.1 )  midiValue = 0x0B;
            else if (dbValue >=  0.1)                      midiValue = 0x0E;
        }
        return midiValue;
    }
};

class MCUDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int offset_;
    int displayType_;
    int displayRow_;
    int channel_;
    string lastStringSent_;

public:
    virtual ~MCUDisplay_Midi_FeedbackProcessor() {}
    MCUDisplay_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int displayUpperLower, int displayType, int displayRow, int channel)
        : Midi_FeedbackProcessor(csi, surface, widget), offset_(displayUpperLower * 56), displayType_(displayType), displayRow_(displayRow), channel_(channel) {
    }

    virtual const char* GetName() override { return "MCUDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, "");
    }

    virtual void SetValue(const PropertyList& properties, const char* const& inputText) override {
        if (!IsSameString(inputText, lastStringSent_.c_str()))
            ForceValue(properties, inputText);
    }

    virtual void ForceValue(const PropertyList& properties, const char* const& inputText) override {
        lastStringSent_ = inputText;

        char tmp[MEDBUF];
        const char* text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        if (IsSameString(text, SILENCE_DB_STRING)) text = "";

        SysExBuilder builder;
        builder.begin()
            .add(0x00).add(0x00).add(0x66).add(displayType_)
            .add(displayRow_).add(channel_ * 7 + offset_)
            .addText(text, 7)
            .end();
        SendMidiSysExMessage(builder.message());
    }
};

class FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int lastFirstLetter_;

public:
    FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget)
        : Midi_FeedbackProcessor(csi, surface, widget) {
        lastFirstLetter_ = 0x00;
    }

    virtual const char* GetName() override { return "FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
        SendMidiMessage(0xB0, 0x4B, 0x20);
        SendMidiMessage(0xB0, 0x4A, 0x20);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (value == MCU_DISPLAY_SELECTED_TRACK)
        {
            if (lastFirstLetter_ != 0x13) {
                lastFirstLetter_ = 0x13;
                SendMidiMessage(0xB0, 0x4B, 0x13); // S
                SendMidiMessage(0xB0, 0x4A, 0x05); // E
            }
        } else if (value == MCU_DISPLAY_TRACK) {
            if (lastFirstLetter_ != 0x07) {
                lastFirstLetter_ = 0x07;
                SendMidiMessage(0xB0, 0x4B, 0x07); // G
                SendMidiMessage(0xB0, 0x4A, 0x0C); // L
            }
        }
    }
};

class MCU_TimeDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
protected:
    char m_mackie_lasttime[10];
    int m_mackie_lasttime_mode;
    DWORD m_mcu_timedisp_lastforce, m_mcu_meter_lastrun;

public:
    MCU_TimeDisplay_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget)
        : Midi_FeedbackProcessor(csi, surface, widget) {}

    virtual const char* GetName() override { return "MCU_TimeDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
        for (int i = 0; i < 10; ++i)
            SendMidiMessage(0xB0, 0x40 + i, 0x20);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        DWORD now = GetTickCount();
        double pp = (GetPlayState() & 1) ? GetPlayPosition() : GetCursorPosition();
        unsigned char bla[10];
        memset(bla, 0, sizeof(bla));

        int tmode = csi_->GetResolvedTimeMode();

        if (tmode == TIMEMODE_SECONDS) {
            double* toptr = csi_->GetTimeOffsPtr();
            if (toptr) pp += *toptr;
            char buf[64];
            snprintf(buf, sizeof(buf), "%d %02d", (int) pp, ((int) (pp * 100.0)) % 100);
            if (strlen(buf) > sizeof(bla))
                memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla));
            else
                memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));
        } else if (tmode == TIMEMODE_SAMPLES) {
            char buf[128];
            format_timestr_pos(pp, buf, sizeof(buf), TIMEMODE_SAMPLES);
            if (strlen(buf) > sizeof(bla))
                memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla));
            else
                memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));
        } else if (tmode == TIMEMODE_FRAMES) {
            char buf[128];
            format_timestr_pos(pp, buf, sizeof(buf), TIMEMODE_FRAMES);
            char *p = buf, *op = buf;
            int ccnt = 0;
            while (*p) {
                if (*p == ':') {
                    ccnt++;
                    if (ccnt != 3) {
                        p++;
                        continue;
                    }
                    *p = ' ';
                }
                *op++ = *p++;
            }
            *op = 0;
            if (strlen(buf) > sizeof(bla))
                memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla));
            else
                memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));
        } else if (tmode > TIMEMODE_DEFAULT) {
            int num_measures = 0, currentTimeSignatureNumerator = 0;
            double beats = TimeMap2_timeToBeats(NULL, pp, &num_measures, &currentTimeSignatureNumerator, NULL, NULL) + 0.000000000001;
            double nbeats = floor(beats);
            beats -= nbeats;

            if (num_measures <= 0 && pp < 0.0)
                --num_measures;

            int* measptr = csi_->GetMeasOffsPtr();
            int nm = num_measures + 1 + (measptr ? *measptr : 0);

            if (nm < 0)
                bla[0] = '-';
            nm = std::abs(nm);

            if (nm >= 100)
                bla[0] = '0' + (nm / 100) % 10;
            if (nm >= 10)
                bla[1] = '0' + (nm / 10) % 10;
            bla[2] = '0' + (nm) % 10;

            int nb = (pp < 0.0 ? currentTimeSignatureNumerator : 0) + (int) nbeats + 1;
            if (nb >= 10)
                bla[3] = '0' + (nb / 10) % 10;
            bla[4] = '0' + (nb) % 10;

            const int fracbeats = (int) (1000.0 * beats);
            bla[7] = '0' + (fracbeats / 100) % 10;
            bla[8] = '0' + (fracbeats / 10) % 10;
            bla[9] = '0' + (fracbeats % 10);
        } else {
            double* toptr = csi_->GetTimeOffsPtr();
            if (toptr)
                pp += (*toptr);

            int ipp = (int) pp;
            int fr = (int) ((pp - ipp) * 1000.0);

            if (ipp >= 360000)
                bla[0] = '0' + (ipp / 360000) % 10;
            if (ipp >= 36000)
                bla[1] = '0' + (ipp / 36000) % 10;
            if (ipp >= 3600)
                bla[2] = '0' + (ipp / 3600) % 10;
            bla[3] = '0' + (ipp / 600) % 6;
            bla[4] = '0' + (ipp / 60) % 10;
            bla[5] = '0' + (ipp / 10) % 6;
            bla[6] = '0' + (ipp % 10);
            bla[7] = '0' + (fr / 100) % 10;
            bla[8] = '0' + (fr / 10) % 10;
            bla[9] = '0' + (fr % 10);
        }

        if (m_mackie_lasttime_mode != tmode) {
            m_mackie_lasttime_mode = tmode;
            SendMidiMessage(0x90, 0x71, tmode == TIMEMODE_FRAMES ? 0x7F : 0);
            SendMidiMessage(0x90, 0x72, m_mackie_lasttime_mode > TIMEMODE_DEFAULT && tmode < TIMEMODE_SECONDS ? 0x7F : 0);
            for (int x = 0; x < (int) sizeof(bla); ++x)
                SendMidiMessage(0xB0, 0x40 + x, 0x20);
        }

        if (memcmp(m_mackie_lasttime, bla, sizeof(bla))) {
            bool force = false;
            if (now > m_mcu_timedisp_lastforce) {
                m_mcu_timedisp_lastforce = now + 2000;
                force = true;
            }
            for (int x = 0; x < (int) sizeof(bla); ++x) {
                int idx = sizeof(bla) - x - 1;
                if (bla[idx] != m_mackie_lasttime[idx] || force) {
                    SendMidiMessage(0xB0, 0x40 + x, bla[idx]);
                    m_mackie_lasttime[idx] = bla[idx];
                }
            }
        }
    }
};
