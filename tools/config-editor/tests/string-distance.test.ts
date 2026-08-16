import { describe, expect, test } from "bun:test";
import { levenshteinDistance, suggestSimilarStrings } from "../src/string-distance.ts";

describe("string distance", () => {
    test("calculates case-insensitive Levenshtein distance", () => {
        expect(levenshteinDistance("TrackPan", "trackpan")).toBe(0);
        expect(levenshteinDistance("MCUTrackPan", "TrackPan")).toBe(3);
        expect(levenshteinDistance("kitten", "sitting")).toBe(3);
    });

    test("returns up to three close matches in distance order", () => {
        expect(suggestSimilarStrings("MCUTrackPan", ["TrackVolume", "TrackPanR", "TrackPan", "TrackPanL", "Play"])).toEqual(["TrackPan", "TrackPanL", "TrackPanR"]);
        expect(suggestSimilarStrings("unrelated", ["Play", "Stop", "Record"])).toEqual([]);
    });
});
