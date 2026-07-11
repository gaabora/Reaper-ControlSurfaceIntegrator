#pragma once
// track_nav_manager.h — TrackVCAFolderMode enum and TrackNavigationManager class
#include "preamble.h"
#include "navigator.h"

enum class TrackVCAFolderMode {
    Track,
    VCA,
    Folder,
    SelectedTracks
};

class TrackNavigationManager
{
protected:
    CSurfIntegrator* const csi_;
    IPageContext* const page_;
    bool followMCP_;
    bool synchPages_;
    bool isScrollLinkEnabled_;
    bool isScrollSynchEnabled_;
    TrackVCAFolderMode currentTrackVCAFolderMode_ = TrackVCAFolderMode::Track;
    int targetScrollLinkChannel_ = 0;
    int trackOffset_ = 0; // Offset in tracks_ of the first channel on the surface
    int vcaTrackOffset_ = 0;
    int folderTrackOffset_ = 0;
    int selectedTracksOffset_ = 0;
    vector<MediaTrack*> tracks_;
    vector<MediaTrack*> selectedTracks_;
    vector<MediaTrack*> selectedTracksInclMaster_;

    bool isFolderViewActive_ = false;
    int currentFolderTrackID_ = 0; // 0 is for root folder
    MediaTrack* parentOfCurrentFolderTrack_ = nullptr;

    vector<MediaTrack*> vcaTopLeadTracks_;
    MediaTrack* vcaLeadTrack_ = NULL;
    vector<MediaTrack*> vcaLeadTracks_;
    vector<MediaTrack*> vcaSpillTracks_;

    vector<MediaTrack*> folderTopParentTracks_;
    MediaTrack* folderParentTrack_ = NULL;
    vector<MediaTrack*> folderParentTracks_;
    vector<MediaTrack*> folderSpillTracks_;
    map<MediaTrack*, vector<MediaTrack*>> folderDictionary_;

    vector<unique_ptr<Navigator>> fixedTrackNavigators_;
    vector<unique_ptr<Navigator>> trackNavigators_;
    unique_ptr<Navigator> masterTrackNavigator_;
    unique_ptr<Navigator> selectedTrackNavigator_;
    unique_ptr<Navigator> focusedFXNavigator_;

    void setTrackOffset(int trackOffset) {
        if (trackOffset <= 0) {
            trackOffset_ = 0;
            return;
        }

        int maxOffset = static_cast<int>(tracks_.size() - trackNavigators_.size());
        if (maxOffset < 0) maxOffset = 0;

        if (trackOffset > maxOffset) trackOffset_ = maxOffset;
        else trackOffset_ = trackOffset;
    }

    void ForceScrollLink() {
        // Make sure selected track is visible on the control surface
        MediaTrack* selectedTrack = GetSelectedTrack();

        if (selectedTrack != NULL) {
            // Is the selected track already visible on the surface?
            for (auto& trackNavigator : trackNavigators_)
                if (selectedTrack == trackNavigator->GetTrack()) return;

            // Check if the selected track is in the current folder
            MediaTrack* parentTrack = GetParentTrack(selectedTrack);
            int parentTrackId = parentTrack ? GetIdFromTrack(parentTrack) : 0;

            if (currentFolderTrackID_ != parentTrackId) {
                // If not, chenge the current folder to the selected track's parent
                currentFolderTrackID_ = parentTrackId;
                RebuildTracks();
            }

            // Find the selected track in the tracks_ list
            auto it = std::find(tracks_.begin(), tracks_.end(), selectedTrack);
            if (it != tracks_.end()) {
                setTrackOffset(static_cast<int>(std::distance(tracks_.begin(), it)));
            }
        }
    }

public:
    TrackNavigationManager(CSurfIntegrator* const csi, IPageContext* page, bool followMCP, bool synchPages, bool isScrollLinkEnabled, bool isScrollSynchEnabled)
        : csi_(csi)
        , page_(page)
        , followMCP_(followMCP)
        , synchPages_(synchPages)
        , isScrollLinkEnabled_(isScrollLinkEnabled)
        , isScrollSynchEnabled_(isScrollSynchEnabled)
        , masterTrackNavigator_(CreateMasterTrackNavigator(csi_, page_))
        , selectedTrackNavigator_(std::make_unique<Navigator>(csi, page, NavigatorType::SelectedTrackNavigator, [page](Navigator&) -> MediaTrack* {
            return page->GetTrackNavigationManager()->GetSelectedTrack(true);
        }))
        , focusedFXNavigator_(CreateFocusedFXNavigator(csi_, page_)) {}

    ~TrackNavigationManager() {
        fixedTrackNavigators_.clear();
        trackNavigators_.clear();
    }

    void RebuildTracks();
    void RebuildSelectedTracks();
    void AdjustSelectedTrackBank(int amount);
    bool GetSynchPages() { return synchPages_; }
    bool GetScrollLink() { return isScrollLinkEnabled_; }
    bool GetFollowMCP() { return followMCP_; }
    int GetNumTracks() { return CSurf_NumTracks(followMCP_); }
    Navigator* GetMasterTrackNavigator() { return masterTrackNavigator_.get(); }
    Navigator* GetSelectedTrackNavigator() { return selectedTrackNavigator_.get(); }
    Navigator* GetFocusedFXNavigator() { return focusedFXNavigator_.get(); }

    bool GetIsTrackVisible(MediaTrack* track) { return IsTrackVisible(track, followMCP_); }

    void ToggleFolderView() {
        isFolderViewActive_ = !isFolderViewActive_;
        if (isFolderViewActive_) {
            // Entering folder view: show the root level
            currentFolderTrackID_ = 0;
            trackOffset_ = 0;
        } else {
            // currentFolderTrackID_ is equal to the absolute offset of the first track in tracks_ before toggling
            trackOffset_ += currentFolderTrackID_;
            // When in flat mode, currentFolderTrackID_ must be zero
            currentFolderTrackID_ = 0;
        }
    }

    bool GetIsFolderViewActive() const { return isFolderViewActive_; }

    void ActivateVCAMode() { currentTrackVCAFolderMode_ = TrackVCAFolderMode::VCA; }

    void ActivateFolderMode() { currentTrackVCAFolderMode_ = TrackVCAFolderMode::Folder; }

    void ActivateSelectedTracksMode() { currentTrackVCAFolderMode_ = TrackVCAFolderMode::SelectedTracks; }

    void DeactivateVCAMode() { if (currentTrackVCAFolderMode_ == TrackVCAFolderMode::VCA) currentTrackVCAFolderMode_ = TrackVCAFolderMode::Track; }

    void DeactivateFolderMode() { if (currentTrackVCAFolderMode_ == TrackVCAFolderMode::Folder) currentTrackVCAFolderMode_ = TrackVCAFolderMode::Track; }

    void DeactivateSelectedTracksMode() { if (currentTrackVCAFolderMode_ == TrackVCAFolderMode::SelectedTracks) currentTrackVCAFolderMode_ = TrackVCAFolderMode::Track; }

    string GetCurrentTrackVCAFolderModeDisplay() const {
        switch (currentTrackVCAFolderMode_) {
            case TrackVCAFolderMode::VCA: return "VCA";
            case TrackVCAFolderMode::Folder: return "Folder";
            case TrackVCAFolderMode::SelectedTracks: return "SelectedTracks";
            case TrackVCAFolderMode::Track: return "Track";
            default: return "";
        }
    }
    static const char* GetAutoModeDisplayNameNoOverride(int modeIndex) {
        switch (modeIndex) {
            case 0: return "Trim";
            case 1: return "Read";
            case 2: return "Touch";
            case 3: return "Write";
            case 4: return "Latch";
            case 5: return "LtchPre";
            default:
                WDL_ASSERT(false);
                return "?";
        }
    }

    const char* GetAutoModeDisplayName(int modeIndex) {
        int globalOverride = GetGlobalAutomationOverride();

        if (globalOverride > -1) // -1=no override, 0=trim/read, 1=read, 2=touch, 3=write, 4=latch, 5=bypass
            return GetAutoModeDisplayNameNoOverride(globalOverride);
        else
            return GetAutoModeDisplayNameNoOverride(modeIndex);
    }

    const char* GetGlobalAutoModeDisplayName() {
        int globalOverride = GetGlobalAutomationOverride();

        if (globalOverride == -1) return "NoOverride";
        else if (globalOverride > -1) // -1=no override, 0=trim/read, 1=read, 2=touch, 3=write, 4=latch, 5=bypass
            return GetAutoModeDisplayNameNoOverride(globalOverride);
        else return "";
    }

    void NextInputMonitorMode(MediaTrack* track) {
        // I_RECMON : int  *: record monitor (0=off, 1=normal, 2=not when playing (tapestyle))
        int recMonitorMode = (int) GetMediaTrackInfo_Value(track, "I_RECMON");

        // I_RECMONITEMS : int  *: monitor items while recording (0=off, 1=on)
        int recMonitorItemMode = (int) GetMediaTrackInfo_Value(track, "I_RECMONITEMS");

        if (recMonitorMode == 0) {
            recMonitorMode = 1;
            recMonitorItemMode = 0;
        } else if (recMonitorMode == 1 && recMonitorItemMode == 0) {
            recMonitorMode = 2;
            recMonitorItemMode = 0;
        } else if (recMonitorMode == 2 && recMonitorItemMode == 0) {
            recMonitorMode = 1;
            recMonitorItemMode = 1;
        } else if (recMonitorMode == 1 && recMonitorItemMode == 1) {
            recMonitorMode = 2;
            recMonitorItemMode = 1;
        } else if (recMonitorMode == 2 && recMonitorItemMode == 1) {
            recMonitorMode = 0;
            recMonitorItemMode = 0;
        }

        GetSetMediaTrackInfo(track, "I_RECMON", &recMonitorMode);
        GetSetMediaTrackInfo(track, "I_RECMONITEMS", &recMonitorItemMode);
    }

    const char* GetCurrentInputMonitorMode(MediaTrack* track) {
        // I_RECMON : int  *: record monitor (0=off, 1=normal, 2=not when playing (tapestyle))
        int recMonitorMode = (int) GetMediaTrackInfo_Value(track, "I_RECMON");

        // I_RECMONITEMS : int  *: monitor items while recording (0=off, 1=on)
        int recMonitorItemMode = (int) GetMediaTrackInfo_Value(track, "I_RECMONITEMS");

        if (recMonitorMode == 0) return "Off";
        else if (recMonitorMode == 1 && recMonitorItemMode == 0) return "Input";
        else if (recMonitorMode == 2 && recMonitorItemMode == 0) return "Auto";
        else if (recMonitorMode == 1 && recMonitorItemMode == 1) return "Input+";
        else if (recMonitorMode == 2 && recMonitorItemMode == 1) return "Auto+";
        else return "";
    }

    const vector<MediaTrack*>& GetSelectedTracks(bool includeMaster) { return includeMaster ? selectedTracksInclMaster_ : selectedTracks_; }

    void SetTrackOffset(int trackOffset) {
        if (isScrollSynchEnabled_) {
            if (isFolderViewActive_) {
                // Find the track at trackOffset in the tracks_ list
                MediaTrack* track = GetTrackFromId(trackOffset + 1);
                auto it = std::find(tracks_.begin(), tracks_.end(), track);
                if (it == tracks_.end())
                    return; // not in the current folder, don't scroll the channels

                trackOffset = static_cast<int>(std::distance(tracks_.begin(), it));
            }

            setTrackOffset(trackOffset);
        }
    }

    void AdjustTrackBank(int amount) {
        if (currentTrackVCAFolderMode_ != TrackVCAFolderMode::Track) return;

        setTrackOffset(trackOffset_ + amount);

        if (isScrollSynchEnabled_) {
            if (MediaTrack* leftmostTrack = DAW::GetTrack(currentFolderTrackID_ + 1 + trackOffset_))
                SetMixerScroll(leftmostTrack);
        }
    }

    void AdjustVCABank(int amount) {
        if (currentTrackVCAFolderMode_ != TrackVCAFolderMode::VCA) return;

        vcaTrackOffset_ += amount;

        if (vcaTrackOffset_ < 0) vcaTrackOffset_ = 0;

        int top = 0;

        if (vcaLeadTrack_ == NULL) top = (int) vcaTopLeadTracks_.size() - 1;
        else top = (int) vcaSpillTracks_.size() - 1;

        if (vcaTrackOffset_ > top) vcaTrackOffset_ = top;
    }

    void AdjustFolderBank(int amount) {
        if (currentTrackVCAFolderMode_ != TrackVCAFolderMode::Folder) return;

        folderTrackOffset_ += amount;

        if (folderTrackOffset_ < 0) folderTrackOffset_ = 0;

        int top = 0;

        if (folderParentTrack_ == NULL) top = (int) folderTopParentTracks_.size() - 1;
        else top = (int) folderSpillTracks_.size() - 1;

        if (folderTrackOffset_ > top) folderTrackOffset_ = top;
    }

    void AdjustSelectedTracksBank(int amount) {
        if (currentTrackVCAFolderMode_ != TrackVCAFolderMode::SelectedTracks) return;

        selectedTracksOffset_ += amount;
        if (selectedTracksOffset_ < 0) selectedTracksOffset_ = 0;
        int top = (int) selectedTracks_.size() - 1;
        if (selectedTracksOffset_ > top) selectedTracksOffset_ = top;
    }

    Navigator* GetNavigatorForChannel(int channelNum) {
        for (auto& trackNavigator : trackNavigators_)
            if (trackNavigator->GetChannelNum() == channelNum) return trackNavigator.get();
        trackNavigators_.push_back(make_unique<Navigator>(csi_, page_, NavigatorType::TrackNavigator
            , [this, channelNum](Navigator &) -> MediaTrack * { return this->GetTrackFromChannel(channelNum); }
            , channelNum
        ));
        return trackNavigators_.back().get();
    }

    Navigator* GetNavigatorForTrack(MediaTrack* track) {
        for (auto& fixedTrackNavigator : fixedTrackNavigators_)
            if (fixedTrackNavigator->GetTrack() == track) return fixedTrackNavigator.get();
        fixedTrackNavigators_.push_back(make_unique<Navigator>(csi_, page_, NavigatorType::FixedTrackNavigator
            , [track](Navigator&) -> MediaTrack* { return track; }
        ));
        return fixedTrackNavigators_.back().get();
    }

    MediaTrack* GetTrackFromChannel(int channelNumber) {
        switch (currentTrackVCAFolderMode_) {
            case TrackVCAFolderMode::Track: {
                channelNumber += trackOffset_;
                if (channelNumber < GetNumTracks() && channelNumber < tracks_.size() && DAW::ValidateTrackPtr(tracks_[channelNumber]))
                    return tracks_[channelNumber];
                break;
            }

            case TrackVCAFolderMode::VCA: {
                channelNumber += vcaTrackOffset_;
                auto& tracks = (vcaLeadTrack_ == nullptr) ? vcaTopLeadTracks_ : vcaSpillTracks_;
                if (channelNumber < tracks.size() && DAW::ValidateTrackPtr(tracks[channelNumber]))
                    return tracks[channelNumber];
                break;
            }

            case TrackVCAFolderMode::Folder: {
                channelNumber += folderTrackOffset_;
                auto& tracks = (folderParentTrack_ == nullptr) ? folderTopParentTracks_ : folderSpillTracks_;
                if (channelNumber < tracks.size() && DAW::ValidateTrackPtr(tracks[channelNumber]))
                    return tracks[channelNumber];
                break;
            }

            case TrackVCAFolderMode::SelectedTracks: {
                channelNumber += selectedTracksOffset_;
                if (channelNumber < selectedTracks_.size() && DAW::ValidateTrackPtr(selectedTracks_[channelNumber]))
                    return selectedTracks_[channelNumber];
                break;
            }

            default:
                break;
        }

        return nullptr;
    }

    MediaTrack* GetTrackFromId(int trackNumber) {
        if (trackNumber <= GetNumTracks()) return CSurf_TrackFromID(trackNumber, followMCP_);
        else return NULL;
    }

    int GetIdFromTrack(MediaTrack* track) { return CSurf_TrackToID(track, followMCP_); }

    MediaTrack* SetCurrentFolder(MediaTrack* track) {
        if (track == nullptr)
            currentFolderTrackID_ = 0;
        else if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") != 1)
            return nullptr;
        else
            currentFolderTrackID_ = CSurf_TrackToID(track, followMCP_);
        trackOffset_ = 0;
        // If CSI follows the TCP or the MPC, then the selection cannot be outside the folder we just enter:
        // as we were previously outside this folder, the selected track cannot be inside: it needs to be changed.
        // Select the first track in the folder
        if (isScrollLinkEnabled_) return GetTrackFromId(currentFolderTrackID_ + 1);
        else return nullptr;
    }

    MediaTrack* ExitCurrentFolder() {
        MediaTrack* exitedFolderTrack = GetTrackFromId(currentFolderTrackID_);
        SetCurrentFolder(parentOfCurrentFolderTrack_); // parentOfCurrentFolderTrack_ will be updated on track list rebuild
        // If CSI follows the TCP or the MPC, then the selection cannot be outside the folder we just enter:
        // as we were previously in a child folder, the selected track cannot be at this level: it needs to be changed.
        // Select the folder just exited
        if (isScrollLinkEnabled_) return exitedFolderTrack;
        else return nullptr;
    }

    bool IsAtRootFolderLevel() { return currentFolderTrackID_ == 0; }

    bool GetIsVCASpilled(MediaTrack* track) {
        if (vcaLeadTrack_ == NULL && (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") != 0 || DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") != 0))
            return true;
        else if (vcaLeadTrack_ == track) return true;
        else return false;
    }

    void ToggleVCASpill(MediaTrack* track) {
        if (currentTrackVCAFolderMode_ != TrackVCAFolderMode::VCA) return;

        if (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") == 0 && DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") == 0) return;

        if (vcaLeadTrack_ == track) {
            if (vcaLeadTracks_.size() > 0) {
                vcaLeadTrack_ = vcaLeadTracks_[vcaLeadTracks_.size() - 1];
                vcaLeadTracks_.erase(vcaLeadTracks_.begin() + vcaLeadTracks_.size() - 1);
            } else
                vcaLeadTrack_ = NULL;
        } else if (vcaLeadTrack_ != NULL) {
            vcaLeadTracks_.push_back(vcaLeadTrack_);
            vcaLeadTrack_ = track;
        } else
            vcaLeadTrack_ = track;

        vcaTrackOffset_ = 0;
    }

    bool GetIsFolderSpilled(MediaTrack* track) {
        if (find(folderTopParentTracks_.begin(), folderTopParentTracks_.end(), track) != folderTopParentTracks_.end()) return true;
        else if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 1) return true;
        else return false;
    }

    void ToggleFolderSpill(MediaTrack* track) {
        if (currentTrackVCAFolderMode_ != TrackVCAFolderMode::Folder) return;
        if (folderTopParentTracks_.size() == 0) return;
        if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") != 1) return;

        if (folderParentTrack_ == track) {
            if (folderParentTracks_.size() > 0) {
                folderParentTrack_ = folderParentTracks_[folderParentTracks_.size() - 1];
                folderParentTracks_.erase(folderParentTracks_.begin() + folderParentTracks_.size() - 1);
            } else
                folderParentTrack_ = NULL;
        } else if (folderParentTrack_ != NULL) {
            folderParentTracks_.push_back(folderParentTrack_);
            folderParentTrack_ = track;
        } else
            folderParentTrack_ = track;
        folderTrackOffset_ = 0;
    }

    void ToggleSynchPages() { synchPages_ = !synchPages_; }

    void ToggleFollowMCP() { followMCP_ = !followMCP_; }

    void ToggleScrollLink(int targetChannel) {
        targetScrollLinkChannel_ = targetChannel - 1 < 0 ? 0 : targetChannel - 1;
        isScrollLinkEnabled_ = !isScrollLinkEnabled_;
        OnTrackSelection();
    }

    MediaTrack* GetSelectedTrack(bool includeMaster = false) {
        if (includeMaster)
            return (selectedTracksInclMaster_.size() > 0) ? selectedTracksInclMaster_[0] : nullptr;
        else
            return (selectedTracks_.size() > 0) ? selectedTracks_[0] : nullptr;
    }

    //  Page only uses the following:
    void OnTrackSelection() { if (isScrollLinkEnabled_) ForceScrollLink(); }

    void OnTrackListChange() {
        RebuildTracks();
        AdjustTrackBank(0); // make sure the track offset is correct
        if (isScrollLinkEnabled_) ForceScrollLink();
    }

    void OnTrackSelectionBySurface(MediaTrack* track) {
        if (isScrollLinkEnabled_) {
            if (IsTrackVisible(track, true))
                SetMixerScroll(track); // scroll selected MCP tracks into view
            if (IsTrackVisible(track, false))
                DAW::SendCommandMessage(40913); // scroll selected TCP tracks into view
        }
    }

    bool GetIsControlTouched(MediaTrack* track, int touchedControl) {
        if (track == GetMasterTrackNavigator()->GetTrack())
            return GetIsNavigatorTouched(GetMasterTrackNavigator(), touchedControl);

        for (auto& trackNavigator : trackNavigators_)
            if (track == trackNavigator->GetTrack())
                return GetIsNavigatorTouched(trackNavigator.get(), touchedControl);

        if (MediaTrack* selectedTrack = GetSelectedTrack(true))
            if (track == selectedTrack)
                return GetIsNavigatorTouched(GetSelectedTrackNavigator(), touchedControl);

        if (MediaTrack* focusedFXTrack = GetFocusedFXNavigator()->GetTrack())
            if (track == focusedFXTrack)
                return GetIsNavigatorTouched(GetFocusedFXNavigator(), touchedControl);
        return false;
    }

    bool GetIsNavigatorTouched(Navigator* navigator, int touchedControl) {
        if (touchedControl == 0) return navigator->GetIsVolumeTouched();
        else if (touchedControl == 1) {
            if (navigator->GetIsPanTouched() || navigator->GetIsPanLeftTouched())
                return true;
        } else if (touchedControl == 2) {
            if (navigator->GetIsPanWidthTouched() || navigator->GetIsPanRightTouched())
                return true;
        }
        return false;
    }

    void RebuildVCASpill() {
        if (currentTrackVCAFolderMode_ != TrackVCAFolderMode::VCA) return;

        vcaTopLeadTracks_.clear();
        vcaSpillTracks_.clear();

        unsigned int leadTrackVCALeaderGroup = 0;
        unsigned int leadTrackVCALeaderGroupHigh = 0;

        if (vcaLeadTrack_ != NULL) {
            leadTrackVCALeaderGroup = DAW::GetTrackGroupMembership(vcaLeadTrack_, "VOLUME_VCA_LEAD");
            leadTrackVCALeaderGroupHigh = DAW::GetTrackGroupMembershipHigh(vcaLeadTrack_, "VOLUME_VCA_LEAD");
            vcaSpillTracks_.push_back(vcaLeadTrack_);
        }

        // Get Visible Tracks
        for (int tidx = 1; tidx <= GetNumTracks(); ++tidx) {
            MediaTrack* track = CSurf_TrackFromID(tidx, followMCP_);

            if (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") != 0 && DAW::GetTrackGroupMembership(track, "VOLUME_VCA_FOLLOW") == 0)
                vcaTopLeadTracks_.push_back(track);

            if (DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") != 0 && DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_FOLLOW") == 0)
                vcaTopLeadTracks_.push_back(track);

            if (vcaLeadTrack_ != NULL) {
                bool isFollower = false;

                unsigned int leadTrackVCAFollowerGroup = DAW::GetTrackGroupMembership(track, "VOLUME_VCA_FOLLOW");
                unsigned int leadTrackVCAFollowerGroupHigh = DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_FOLLOW");

                if ((leadTrackVCALeaderGroup & leadTrackVCAFollowerGroup) || (leadTrackVCALeaderGroupHigh & leadTrackVCAFollowerGroupHigh)) {
                    isFollower = true;
                }
                if (isFollower)
                    vcaSpillTracks_.push_back(track);
            }
        }
    }

    void RebuildFolderTracks() {
        if (currentTrackVCAFolderMode_ != TrackVCAFolderMode::Folder) return;

        folderTopParentTracks_.clear();
        folderDictionary_.clear();
        folderSpillTracks_.clear();

        vector<vector<MediaTrack*>*> currentDepthTracks;

        for (int i = 1; i <= GetNumTracks(); i++) {
            MediaTrack* track = CSurf_TrackFromID(i, followMCP_);

            if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 1) {
                if (currentDepthTracks.size() == 0)
                    folderTopParentTracks_.push_back(track);
                else
                    currentDepthTracks.back()->push_back(track);

                folderDictionary_[track].push_back(track);

                currentDepthTracks.push_back(&folderDictionary_[track]);
            } else if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 0 && currentDepthTracks.size() > 0) {
                currentDepthTracks.back()->push_back(track);
            } else if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") < 0 && currentDepthTracks.size() > 0) {
                currentDepthTracks.back()->push_back(track);

                int folderBackTrack = (int) -GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH");

                for (int i = 0; i < folderBackTrack && currentDepthTracks.size() > 0; i++)
                    currentDepthTracks.pop_back();
            }
        }

        if (folderParentTrack_ != NULL)
            for (int i = 0; i < folderDictionary_[folderParentTrack_].size(); ++i)
                folderSpillTracks_.push_back(folderDictionary_[folderParentTrack_][i]);
    }

    void EnterPage() {
        WDL_ASSERT(false);
        /*
         if (colorTracks_)
         {
         // capture track colors
         for (auto *navigator : trackNavigators_)
         if (MediaTrack *track = DAW::GetTrackFromGUID(navigator->GetTrackGUID(), followMCP_))
         trackColors_[navigator->GetTrackGUID()] = DAW::GetTrackColor(track);
         }
         */
    }

    void LeavePage() {
        WDL_ASSERT(false);
        /*
         if (colorTracks_)
         {
         DAW::PreventUIRefresh(1);
         // reset track colors
         for (auto *navigator : trackNavigators_)
         if (MediaTrack *track = DAW::GetTrackFromGUID(navigator->GetTrackGUID(), followMCP_))
         if (trackColors_.count(navigator->GetTrackGUID()) > 0)
         GetSetMediaTrackInfo(track, "I_CUSTOMCOLOR", &trackColors_[navigator->GetTrackGUID()]);
         DAW::PreventUIRefresh(-1);
         }
         */
    }
};

// =============================================================================
// Factory helper for TrackNavigator — defined here because it requires
// TrackNavigationManager to be complete (needs GetTrackFromChannel).
// =============================================================================
inline std::unique_ptr<Navigator>
CreateTrackNavigator(CSurfIntegrator* csi, IPageContext* page, TrackNavigationManager* tnm, int channelNum) {
    return std::make_unique<Navigator>(csi, page, NavigatorType::TrackNavigator
        , [tnm, channelNum](Navigator&) -> MediaTrack* { return tnm->GetTrackFromChannel(channelNum); }
        , channelNum
    );
}

// Needs TrackNavigationManager complete (GetSelectedTrack is a member of it).
inline std::unique_ptr<Navigator>
CreateSelectedTrackNavigator(CSurfIntegrator* csi, IPageContext* page) {
    return std::make_unique<Navigator>(csi, page, NavigatorType::SelectedTrackNavigator, [page](Navigator&) -> MediaTrack* {
        return page->GetTrackNavigationManager()->GetSelectedTrack(true);
    });
}
