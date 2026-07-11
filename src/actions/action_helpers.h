#pragma once
// action_helpers.h — Shared lightweight helpers used across action classes.
// Included by reaper_actions.h before the action_base/actions_* headers.

//FIXME: can we get rid ot this file and move its content together with action_context.h content to action_context.cpp and leave only definitions in action_context.h (and create definitions for what was in action_helpers

// Send routing index
// Hardware outputs occupy the first GetTrackNumSends(track,1) slots;
// software sends follow, so the effective index is slot + numHW.
inline int GetSendEffectiveIndex(MediaTrack* track, ActionContext* context) {
    return context->GetSlotIndex() + GetTrackNumSends(track, 1);
}



// FX slot guard
// Calls fn(track) only when the track exists AND has an FX at GetSlotIndex().
// Clears the widget in all other cases.
template <typename Fn>
inline void WithFXSlot(ActionContext* context, Fn fn) {
    if (MediaTrack* track = context->GetTrack()) {
        if (TrackFX_GetCount(track) > context->GetSlotIndex())
            fn(track);
        else
            context->ClearWidget();
    } else
        context->ClearWidget();
}

// Transport scrub test
// Returns true when the surface is actively rewinding or fast-forwarding.
inline bool IsScrubbing(ActionContext* context) {
    return context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding();
}

// Meter solo-mute guard
// Clears the widget if another track is soloed and this one isn't;
// otherwise pushes volToNormalized(rawPeak) to the widget.
inline void UpdateMeterValue(ActionContext* context, MediaTrack* track, double rawPeak) {
    if (AnyTrackSolo(NULL) && !GetMediaTrackInfo_Value(track, "I_SOLO"))
        context->ClearWidget();
    else
        context->UpdateWidgetValue(volToNormalized(rawPeak));
}

// Last-Touched FX accessor
// Calls fn(track, fxSlot, fxParam) when a last-touched FX param exists and its
// track is resolvable.  Clears the widget otherwise.
// fn signature: void(MediaTrack*, int slot, int param)
template <typename Fn>
inline void WithLastTouchedFX(ActionContext* context, Fn fn) {
    int trackNum = 0, fxSlotNum = 0, fxParamNum = 0;
    if (GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum)) {
        if (MediaTrack* track = DAW::GetTrack(trackNum))
            fn(track, fxSlotNum, fxParamNum);
        else
            context->ClearWidget();
    } else
        context->ClearWidget();
}

// Send/receive linked-track speech
// Speaks the track name + number for a linked send/receive track via OSARA.
// cat: 0 = send ("P_DESTTRACK"), -1 = receive ("P_SRCTRACK")
inline void SpeakLinkedTrack(ActionContext* context, int cat, const char* trackKey, const char* noTrackMsg) {
    if (MediaTrack* track = context->GetTrack()) {
        MediaTrack* linked = (MediaTrack*) GetSetTrackSendInfo(track, cat, context->GetSlotIndex(), trackKey, 0);
        if (linked) {
            const char* name = (const char*) GetSetMediaTrackInfo(linked, "P_NAME", NULL);
            char tmp[MEDBUF];
            snprintf(tmp, sizeof(tmp), "Track %d%s%s", context->GetTrackNavigationManager()->GetIdFromTrack(linked), name && *name ? " " : "", name ? name : "");
            context->GetCSI()->Speak(tmp);
        } else
            context->GetCSI()->Speak(noTrackMsg);
    }
}
