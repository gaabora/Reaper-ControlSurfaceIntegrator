#include "integrator.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TrackNavigationManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void TrackNavigationManager::RebuildTracks() {
    WDL_MutexLockExclusive lock(&tracksMutex_);
    int oldTracksSize = (int) tracks_.size();

    tracks_.clear();

    if (isFolderViewActive_) {
        parentOfCurrentFolderTrack_ = nullptr;
        std::vector<MediaTrack*> ancestorStack;

        // Find the parent of the current folder track, and the index of the first track in the folder
        int trackID = 1;
        if (currentFolderTrackID_ > 0 && currentFolderTrackID_ < GetNumTracks()) {
            for (; trackID <= GetNumTracks(); trackID++) {
                MediaTrack* track = CSurf_TrackFromID(trackID, followMCP_);
                int depthOffset = static_cast<int>(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH"));

                if (trackID == currentFolderTrackID_) {
                    if (depthOffset == 1) {
                        trackID++; // next track is the first one in the folder (a folder cannot be empty)
                    } else { // currentFolderTrackID_ is not a folder actually
                        if (!ancestorStack.empty()) {
                            currentFolderTrackID_ = GetIdFromTrack(ancestorStack.back()); // Back to last parent folder
                            trackID = currentFolderTrackID_ + 1; // Next track is the first one in the folder
                            ancestorStack.pop_back();
                        } else {
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
        if (!ancestorStack.empty()) parentOfCurrentFolderTrack_ = ancestorStack.back();

        // List the tracks in the folder
        int relativeDepth = 0; // Where 0 is the level of the current folder content
        for (; trackID <= GetNumTracks(); trackID++) {
            MediaTrack* track = CSurf_TrackFromID(trackID, followMCP_);
            if (!track) continue;

            if (relativeDepth == 0 && IsTrackVisible(track, followMCP_))
                tracks_.push_back(track);

            relativeDepth += static_cast<int>(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH"));

            if (relativeDepth < 0) break; // Last track of the folder
        }
    } else {
        for (int i = 1; i <= GetNumTracks(); ++i) {
            MediaTrack* track = CSurf_TrackFromID(i, followMCP_);
            if (!track)
                continue;

            if (IsTrackVisible(track, followMCP_))
                tracks_.push_back(track);
        }
    }

    if (tracks_.size() < oldTracksSize) {
        for (int i = oldTracksSize; i > tracks_.size(); i--)
            page_->ForceClearTrack(i - trackOffset_);
    }

    if (tracks_.size() != oldTracksSize) 
        page_->ForceUpdateTrackColors();
}

void TrackNavigationManager::RebuildSelectedTracks() {
    if (currentTrackVCAFolderMode_ == TrackVCAFolderMode::VCA || currentTrackVCAFolderMode_ == TrackVCAFolderMode::Folder) return;
    WDL_MutexLockExclusive lock(&tracksMutex_);

    int oldTracksSize = (int) selectedTracks_.size();

    selectedTracksInclMaster_.clear();
    selectedTracks_.clear();
    for (int i = 0; i <= GetNumTracks(); i++) {
        MediaTrack* track = GetTrackFromId(i);
        if (*(int*) GetSetMediaTrackInfo(track, "I_SELECTED", NULL)) {
            selectedTracksInclMaster_.push_back(track);
            if (i > 0) selectedTracks_.push_back(track);
        }
    }

    //FIXME compare all changes
    if (selectedTracks_.size() < oldTracksSize) {
        for (int i = oldTracksSize; i > selectedTracks_.size(); i--)
            page_->ForceClearTrack(i - selectedTracksOffset_);
    }

    if (selectedTracks_.size() != oldTracksSize)
        page_->ForceUpdateTrackColors();
}

void TrackNavigationManager::AdjustSelectedTrackBank(int amount) {
    if (MediaTrack* selectedTrack = GetSelectedTrack()) {
        int trackNum = GetIdFromTrack(selectedTrack);

        trackNum += amount;

        if (trackNum < 1) trackNum = 1;
        if (trackNum > GetNumTracks()) trackNum = GetNumTracks();
        if (MediaTrack* trackToSelect = GetTrackFromId(trackNum)) {
            SetOnlyTrackSelected(trackToSelect);
            if (GetScrollLink())
                SetMixerScroll(trackToSelect);

            page_->OnTrackSelection(trackToSelect);
        }
    }
}
