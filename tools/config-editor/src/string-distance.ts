const MAX_SUGGESTIONS = 3;
const MAX_NORMALIZED_DISTANCE = 0.4;

export function levenshteinDistance(leftValue: string, rightValue: string): number {
    const left = leftValue.toLowerCase();
    const right = rightValue.toLowerCase();
    if (left === right) return 0;
    if (!left.length) return right.length;
    if (!right.length) return left.length;

    let previousRow = Array.from({ length: right.length + 1 }, (_unused, columnIndex) => columnIndex);
    for (let rowIndex = 1; rowIndex <= left.length; rowIndex++) {
        const currentRow = [rowIndex];
        for (let columnIndex = 1; columnIndex <= right.length; columnIndex++) {
            const substitutionCost = left[rowIndex - 1] === right[columnIndex - 1] ? 0 : 1;
            currentRow[columnIndex] = Math.min(currentRow[columnIndex - 1] + 1, previousRow[columnIndex] + 1, previousRow[columnIndex - 1] + substitutionCost);
        }
        previousRow = currentRow;
    }
    return previousRow[right.length];
}

export function suggestSimilarStrings(value: string, candidates: Iterable<string>): string[] {
    if (!value) return [];
    return [...candidates]
        .map((candidate) => {
            const distance = levenshteinDistance(value, candidate);
            const normalizedDistance = distance / Math.max(value.length, candidate.length);
            return { candidate, distance, normalizedDistance };
        })
        .filter((match) => match.normalizedDistance <= MAX_NORMALIZED_DISTANCE)
        .sort((left, right) => left.normalizedDistance - right.normalizedDistance || left.distance - right.distance || Math.abs(left.candidate.length - value.length) - Math.abs(right.candidate.length - value.length) || left.candidate.localeCompare(right.candidate))
        .slice(0, MAX_SUGGESTIONS)
        .map((match) => match.candidate);
}
