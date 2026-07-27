# [PM-SAMPLE-0001] Clarify a synthetic fixture requirement

ID: PM-SAMPLE-0001
Status: accepted
Date: 2026-01-02
Governing plan section: SAMPLE
Owner approval: fixture-only

## Before

The sample validator accepts an unspecified result.

## After

The sample validator accepts exactly one explicit result.

## Reason

This synthetic record exercises the plan-modification schema.

## Acceptance impact

The fixture must include a stable ID, accepted status, and all evidence
headings.

## Evidence

`tools/verify_m1_workflow.sh`
