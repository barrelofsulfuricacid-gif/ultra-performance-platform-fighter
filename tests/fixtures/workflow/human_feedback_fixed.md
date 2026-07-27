# [HFB-SAMPLE-0001] Synthetic control prompt is unclear

ID: HFB-SAMPLE-0001
Status: fixed
Reported: 2026-01-02
Affected commit: 1111111111111111111111111111111111111111
Fixed commit: 3333333333333333333333333333333333333333

## Feedback

The sample control prompt does not name the confirm key.

## Expected experience

The available confirm key is visible before input is required.

## Observed experience

The synthetic prompt only says `Continue`.

## Evidence

Fixture screenshot `sample-prompt-001`.

## Resolution

The synthetic prompt now says `Press Enter to continue`.

## Verification

The bookkeeping record names corrective commit
`3333333333333333333333333333333333333333`; the fixture retest passes.
