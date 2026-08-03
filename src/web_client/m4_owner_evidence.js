(function (root) {
  "use strict";

  var evidenceSchema = 1;
  var allowedDecisions = ["pending", "approve", "changes"];
  var rubricDimensions = [
    { id: "input_immediacy", label: "Input immediacy" },
    { id: "ground_control", label: "Ground control" },
    { id: "air_control", label: "Air control" },
    { id: "collision_stability", label: "Collision stability" },
    { id: "combat_clarity", label: "Combat clarity" },
    { id: "combo_expression", label: "Combo expression" },
    { id: "recovery_edge_play", label: "Recovery / edge play" },
    { id: "overall_fun", label: "Overall fun" },
  ];

  function isoNow() {
    return new Date().toISOString();
  }

  function defaultEnvironment() {
    if (typeof navigator === "undefined") {
      return "";
    }
    return navigator.userAgent || "";
  }

  function freshEvidence(sourceRevision) {
    var rubric = {};
    rubricDimensions.forEach(function (dimension) {
      rubric[dimension.id] = 0;
    });
    return {
      schema: evidenceSchema,
      sourceRevision: sourceRevision,
      tester: "",
      buildReference: "",
      environment: defaultEnvironment(),
      techniqueResults: {},
      rubric: rubric,
      noCriticalCollisionAnomaly: false,
      completedMatch: false,
      twoSupportedInputs: false,
      realGamepad: false,
      completedMatches: 0,
      decision: "pending",
      notes: "",
      createdAt: isoNow(),
      updatedAt: isoNow(),
    };
  }

  function cleanString(value) {
    return typeof value === "string" ? value : "";
  }

  function cleanScore(value) {
    var score = Number(value);
    if (!Number.isInteger(score) || score < 1 || score > 5) {
      return 0;
    }
    return score;
  }

  function normalizeEvidence(candidate, checklist) {
    var evidence = freshEvidence(checklist.sourceRevision);
    if (
      !candidate ||
      candidate.schema !== evidenceSchema ||
      candidate.sourceRevision !== checklist.sourceRevision
    ) {
      return evidence;
    }

    evidence.tester = cleanString(candidate.tester);
    evidence.buildReference = cleanString(candidate.buildReference);
    evidence.environment = cleanString(candidate.environment);
    evidence.notes = cleanString(candidate.notes);
    evidence.createdAt = cleanString(candidate.createdAt) || evidence.createdAt;
    evidence.updatedAt = cleanString(candidate.updatedAt) || evidence.updatedAt;
    evidence.noCriticalCollisionAnomaly =
      candidate.noCriticalCollisionAnomaly === true;
    evidence.completedMatch = candidate.completedMatch === true;
    evidence.twoSupportedInputs = candidate.twoSupportedInputs === true;
    evidence.realGamepad = candidate.realGamepad === true;
    evidence.completedMatches = Math.max(
      0,
      Math.min(999, Math.floor(Number(candidate.completedMatches) || 0))
    );
    evidence.decision = allowedDecisions.includes(candidate.decision)
      ? candidate.decision
      : "pending";

    rubricDimensions.forEach(function (dimension) {
      if (candidate.rubric) {
        evidence.rubric[dimension.id] = cleanScore(
          candidate.rubric[dimension.id]
        );
      }
    });

    checklist.techniques.forEach(function (technique) {
      var source = candidate.techniqueResults
        ? candidate.techniqueResults[String(technique.id)]
        : null;
      if (!source) {
        return;
      }
      var result = source.result === "pass" || source.result === "fail"
        ? source.result
        : "";
      var notes = cleanString(source.notes);
      if (result || notes) {
        evidence.techniqueResults[String(technique.id)] = {
          result: result,
          notes: notes,
        };
      }
    });
    return evidence;
  }

  function addOption(select, value, label) {
    var option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    select.appendChild(option);
  }

  function addTextField(container, labelText, value, placeholder) {
    var label = document.createElement("label");
    var input = document.createElement("input");
    label.textContent = labelText;
    input.type = "text";
    input.value = value;
    input.placeholder = placeholder;
    label.appendChild(input);
    container.appendChild(label);
    return input;
  }

  function addCheck(container, labelText, checked) {
    var label = document.createElement("label");
    var input = document.createElement("input");
    var text = document.createElement("span");
    label.className = "pf-m4-owner-collision";
    input.type = "checkbox";
    input.checked = checked;
    text.textContent = labelText;
    label.appendChild(input);
    label.appendChild(text);
    container.appendChild(label);
    return input;
  }

  function markdownCell(value) {
    return cleanString(value)
      .replace(/\|/g, "\\|")
      .replace(/\r?\n/g, "<br>");
  }

  function decisionLabel(decision) {
    if (decision === "approve") {
      return "Approve M4 combat feel";
    }
    if (decision === "changes") {
      return "Request changes and retest";
    }
    return "Pending";
  }

  function downloadText(filename, text, mediaType) {
    var blob = new Blob([text], { type: mediaType });
    var url = URL.createObjectURL(blob);
    var anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = filename;
    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
    URL.revokeObjectURL(url);
  }

  root.PFInstallM4OwnerEvidence = function (
    section,
    checklist,
    runtimeStatusElement
  ) {
    var storageKey =
      "pf-m4-owner-evidence-v" +
      evidenceSchema +
      "-r" +
      checklist.sourceRevision;
    var storageAvailable = true;
    var stored = null;
    try {
      stored = root.localStorage.getItem(storageKey);
    } catch (error) {
      storageAvailable = false;
    }

    var parsed = null;
    if (stored) {
      try {
        parsed = JSON.parse(stored);
      } catch (error) {
        parsed = null;
      }
    }
    var evidence = normalizeEvidence(parsed, checklist);
    var techniqueViews = {};

    var panel = document.createElement("details");
    panel.id = "pf-m4-owner-evidence";
    panel.className = "pf-m4-owner";
    panel.dataset.schema = String(evidenceSchema);
    panel.dataset.sourceRevision = checklist.sourceRevision;
    panel.dataset.techniqueCount = String(checklist.techniques.length);
    panel.dataset.storage = storageAvailable ? "local" : "session-only";
    panel.dataset.ready = "false";
    panel.setAttribute("aria-label", "M4 owner playtest evidence");

    var summary = document.createElement("summary");
    summary.id = "pf-m4-owner-evidence-summary";
    summary.textContent = "Owner evidence · 0/61 passed";
    panel.appendChild(summary);

    var body = document.createElement("div");
    body.className = "pf-m4-owner-body";
    var copy = document.createElement("p");
    copy.className = "pf-m4-owner-copy";
    copy.textContent =
      "Execute each registry recipe through ordinary browser input, then record " +
      "the observed result. Tactical and emergent rows are owner observations " +
      "of legal sequences built from independently verified mechanics; this " +
      "checklist does not add technique-only simulation behavior or harnesses. " +
      "Results persist only in this browser until exported and never promote " +
      "the registry automatically.";
    body.appendChild(copy);

    var storageNote = document.createElement("p");
    storageNote.className = "pf-m4-owner-copy";
    storageNote.textContent = storageAvailable
      ? "Draft evidence is saved locally after every change."
      : "Local browser storage is unavailable. Keep this page open and export before closing it.";
    body.appendChild(storageNote);

    var metadata = document.createElement("div");
    metadata.className = "pf-m4-owner-meta";
    var testerInput = addTextField(
      metadata,
      "Tester / owner",
      evidence.tester,
      "Required for acceptance"
    );
    var buildInput = addTextField(
      metadata,
      "Build / commit",
      evidence.buildReference,
      "Full commit hash"
    );
    var environmentInput = addTextField(
      metadata,
      "Browser, OS, and input devices",
      evidence.environment,
      "Example: Edge / Windows / two gamepads"
    );
    body.appendChild(metadata);

    var progressRow = document.createElement("div");
    progressRow.className = "pf-m4-owner-progress";
    var progress = document.createElement("progress");
    progress.max = checklist.techniques.length;
    progress.value = 0;
    progress.setAttribute("aria-label", "Advanced-technique owner checks passed");
    var progressText = document.createElement("span");
    progressText.id = "pf-m4-owner-progress";
    progressText.textContent = "0 pass · 0 fail · 61 untested";
    progressRow.appendChild(progress);
    progressRow.appendChild(progressText);
    body.appendChild(progressRow);

    var toolbar = document.createElement("div");
    toolbar.className = "pf-m4-owner-toolbar";
    var filterLabel = document.createElement("label");
    filterLabel.textContent = "Show recipes";
    var filter = document.createElement("select");
    filter.id = "pf-m4-owner-filter";
    addOption(filter, "all", "All 61");
    addOption(filter, "untested", "Untested");
    addOption(filter, "fail", "Failed");
    addOption(filter, "pass", "Passed");
    filterLabel.appendChild(filter);
    toolbar.appendChild(filterLabel);
    var markdownButton = document.createElement("button");
    markdownButton.type = "button";
    markdownButton.id = "pf-m4-owner-export-markdown";
    markdownButton.textContent = "Export Markdown";
    toolbar.appendChild(markdownButton);
    var jsonButton = document.createElement("button");
    jsonButton.type = "button";
    jsonButton.id = "pf-m4-owner-export-json";
    jsonButton.textContent = "Export JSON";
    toolbar.appendChild(jsonButton);
    var resetButton = document.createElement("button");
    resetButton.type = "button";
    resetButton.id = "pf-m4-owner-reset";
    resetButton.textContent = "Reset Evidence";
    toolbar.appendChild(resetButton);
    body.appendChild(toolbar);

    var techniqueList = document.createElement("div");
    techniqueList.className = "pf-m4-owner-list";
    techniqueList.id = "pf-m4-owner-techniques";
    techniqueList.setAttribute("aria-label", "61 advanced-technique browser recipes");
    body.appendChild(techniqueList);

    function resultFor(techniqueId) {
      var key = String(techniqueId);
      if (!evidence.techniqueResults[key]) {
        evidence.techniqueResults[key] = { result: "", notes: "" };
      }
      return evidence.techniqueResults[key];
    }

    function renderTechnique(technique) {
      var result = resultFor(technique.id);
      var item = document.createElement("details");
      item.className = "pf-m4-owner-technique";
      item.dataset.techniqueId = String(technique.id);
      item.dataset.registryStatus = technique.registryStatus;
      var itemSummary = document.createElement("summary");
      var name = document.createElement("span");
      name.textContent = "#" + technique.id + " · " + technique.name;
      var resultLabel = document.createElement("span");
      resultLabel.className = "pf-m4-owner-result";
      itemSummary.appendChild(name);
      itemSummary.appendChild(resultLabel);
      item.appendChild(itemSummary);

      var itemBody = document.createElement("div");
      itemBody.className = "pf-m4-owner-technique-body";
      var recipe = document.createElement("p");
      recipe.className = "pf-m4-owner-recipe";
      recipe.textContent = technique.recipe;
      itemBody.appendChild(recipe);
      var actions = document.createElement("div");
      actions.className = "pf-m4-owner-actions";
      [
        { value: "pass", label: "Observed pass" },
        { value: "fail", label: "Observed fail" },
        { value: "", label: "Clear" },
      ].forEach(function (choice) {
        var button = document.createElement("button");
        button.type = "button";
        button.dataset.result = choice.value || "untested";
        button.textContent = choice.label;
        button.addEventListener("click", function () {
          resultFor(technique.id).result = choice.value;
          updateTechniqueView(technique.id);
          persistAndRefresh();
        });
        actions.appendChild(button);
      });
      itemBody.appendChild(actions);
      var notesLabel = document.createElement("label");
      notesLabel.textContent = "Observation / reproduction notes";
      var notes = document.createElement("textarea");
      notes.value = result.notes;
      notes.placeholder =
        "Required for a failure; include the unexpected state, timing, and input.";
      notes.addEventListener("input", function () {
        resultFor(technique.id).notes = notes.value;
        persistAndRefresh();
      });
      notesLabel.appendChild(notes);
      itemBody.appendChild(notesLabel);
      item.appendChild(itemBody);
      techniqueList.appendChild(item);
      techniqueViews[String(technique.id)] = {
        item: item,
        resultLabel: resultLabel,
        notes: notes,
      };
      updateTechniqueView(technique.id);
    }

    function updateTechniqueView(techniqueId) {
      var result = resultFor(techniqueId);
      var view = techniqueViews[String(techniqueId)];
      if (!view) {
        return;
      }
      var state = result.result || "untested";
      view.item.dataset.result = state;
      view.resultLabel.textContent = state;
    }

    checklist.techniques.forEach(renderTechnique);

    var rubricTitle = document.createElement("h2");
    rubricTitle.textContent = "Mandatory combat rubric";
    body.appendChild(rubricTitle);
    var rubricCopy = document.createElement("p");
    rubricCopy.className = "pf-m4-owner-copy";
    rubricCopy.textContent =
      "Score every dimension from 1 to 5. Acceptance requires scores of at " +
      "least 4 and no critical collision anomaly.";
    body.appendChild(rubricCopy);
    var rubricGrid = document.createElement("div");
    rubricGrid.className = "pf-m4-owner-rubric";
    var rubricSelects = {};
    rubricDimensions.forEach(function (dimension) {
      var label = document.createElement("label");
      label.textContent = dimension.label;
      var select = document.createElement("select");
      select.dataset.rubric = dimension.id;
      addOption(select, "0", "Not scored");
      [1, 2, 3, 4, 5].forEach(function (score) {
        addOption(select, String(score), String(score));
      });
      select.value = String(evidence.rubric[dimension.id]);
      select.addEventListener("change", function () {
        evidence.rubric[dimension.id] = cleanScore(select.value);
        persistAndRefresh();
      });
      label.appendChild(select);
      rubricGrid.appendChild(label);
      rubricSelects[dimension.id] = select;
    });
    var collisionCheck = addCheck(
      rubricGrid,
      "No critical snag, tunneling, jitter, or unexplained collision was observed",
      evidence.noCriticalCollisionAnomaly
    );
    collisionCheck.id = "pf-m4-owner-collision-clear";
    collisionCheck.addEventListener("change", function () {
      evidence.noCriticalCollisionAnomaly = collisionCheck.checked;
      persistAndRefresh();
    });
    body.appendChild(rubricGrid);

    var matchTitle = document.createElement("h2");
    matchTitle.textContent = "Mandatory match gates";
    body.appendChild(matchTitle);
    var matchGrid = document.createElement("div");
    matchGrid.className = "pf-m4-owner-rubric";
    var completeMatchCheck = addCheck(
      matchGrid,
      "Completed a local 1v1 from setup through result and rematch",
      evidence.completedMatch
    );
    completeMatchCheck.id = "pf-m4-owner-complete-match";
    var twoInputsCheck = addCheck(
      matchGrid,
      "Both players used supported human inputs",
      evidence.twoSupportedInputs
    );
    twoInputsCheck.id = "pf-m4-owner-two-inputs";
    var gamepadCheck = addCheck(
      matchGrid,
      "A real supported controller was connected and exercised (Standard Gamepad or Mayflash 0079:1843 GameCube adapter)",
      evidence.realGamepad
    );
    gamepadCheck.id = "pf-m4-owner-real-gamepad";
    var matchesLabel = document.createElement("label");
    matchesLabel.textContent = "Completed human matches (minimum 2)";
    var matchesInput = document.createElement("input");
    matchesInput.id = "pf-m4-owner-match-count";
    matchesInput.type = "number";
    matchesInput.min = "0";
    matchesInput.max = "999";
    matchesInput.step = "1";
    matchesInput.value = String(evidence.completedMatches);
    matchesLabel.appendChild(matchesInput);
    matchGrid.appendChild(matchesLabel);
    body.appendChild(matchGrid);

    completeMatchCheck.addEventListener("change", function () {
      evidence.completedMatch = completeMatchCheck.checked;
      persistAndRefresh();
    });
    twoInputsCheck.addEventListener("change", function () {
      evidence.twoSupportedInputs = twoInputsCheck.checked;
      persistAndRefresh();
    });
    gamepadCheck.addEventListener("change", function () {
      evidence.realGamepad = gamepadCheck.checked;
      persistAndRefresh();
    });
    matchesInput.addEventListener("input", function () {
      evidence.completedMatches = Math.max(
        0,
        Math.min(999, Math.floor(Number(matchesInput.value) || 0))
      );
      persistAndRefresh();
    });

    var decisionGrid = document.createElement("div");
    decisionGrid.className = "pf-m4-owner-meta";
    var decisionLabelElement = document.createElement("label");
    decisionLabelElement.textContent = "Owner decision";
    var decision = document.createElement("select");
    decision.id = "pf-m4-owner-decision";
    addOption(decision, "pending", "Pending");
    addOption(decision, "approve", "Approve M4 combat feel");
    addOption(decision, "changes", "Request changes and retest");
    decision.value = evidence.decision;
    decisionLabelElement.appendChild(decision);
    decisionGrid.appendChild(decisionLabelElement);
    body.appendChild(decisionGrid);

    var notesLabel = document.createElement("label");
    notesLabel.textContent = "Overall observations and requested changes";
    var overallNotes = document.createElement("textarea");
    overallNotes.id = "pf-m4-owner-notes";
    overallNotes.value = evidence.notes;
    notesLabel.appendChild(overallNotes);
    body.appendChild(notesLabel);

    var readiness = document.createElement("p");
    readiness.className = "pf-m4-owner-ready";
    readiness.id = "pf-m4-owner-readiness";
    body.appendChild(readiness);
    panel.appendChild(body);
    section.appendChild(panel);

    function counts() {
      var values = { pass: 0, fail: 0, untested: 0 };
      checklist.techniques.forEach(function (technique) {
        var result = resultFor(technique.id).result;
        if (result === "pass") {
          values.pass += 1;
        } else if (result === "fail") {
          values.fail += 1;
        } else {
          values.untested += 1;
        }
      });
      return values;
    }

    function rubricPassed() {
      return (
        evidence.noCriticalCollisionAnomaly &&
        rubricDimensions.every(function (dimension) {
          return evidence.rubric[dimension.id] >= 4;
        })
      );
    }

    function readinessState() {
      var resultCounts = counts();
      var matchesPassed =
        evidence.completedMatch &&
        evidence.twoSupportedInputs &&
        evidence.realGamepad &&
        evidence.completedMatches >= 2;
      var metadataPassed =
        evidence.tester.trim() !== "" &&
        evidence.buildReference.trim() !== "" &&
        evidence.environment.trim() !== "";
      return {
        counts: resultCounts,
        techniquesPassed:
          resultCounts.pass === checklist.techniques.length &&
          resultCounts.fail === 0,
        rubricPassed: rubricPassed(),
        matchesPassed: matchesPassed,
        metadataPassed: metadataPassed,
        decisionPassed: evidence.decision === "approve",
      };
    }

    function isReady(state) {
      return (
        state.techniquesPassed &&
        state.rubricPassed &&
        state.matchesPassed &&
        state.metadataPassed &&
        state.decisionPassed
      );
    }

    function applyFilter() {
      var selected = filter.value;
      checklist.techniques.forEach(function (technique) {
        var view = techniqueViews[String(technique.id)];
        var result = resultFor(technique.id).result || "untested";
        view.item.hidden = selected !== "all" && selected !== result;
      });
    }

    function refresh() {
      var state = readinessState();
      var ready = isReady(state);
      progress.value = state.counts.pass;
      progressText.textContent =
        state.counts.pass +
        " pass · " +
        state.counts.fail +
        " fail · " +
        state.counts.untested +
        " untested";
      summary.textContent =
        "Owner evidence · " +
        state.counts.pass +
        "/" +
        checklist.techniques.length +
        " passed" +
        (state.counts.fail ? " · " + state.counts.fail + " failed" : "");
      panel.dataset.ready = ready ? "true" : "false";
      panel.dataset.passed = String(state.counts.pass);
      panel.dataset.failed = String(state.counts.fail);
      section.dataset.ownerEvidence = ready ? "complete" : "incomplete";
      readiness.textContent = ready
        ? "Evidence complete and owner-approved. Export Markdown for repository review; the registry remains unchanged until that evidence is committed."
        : "Acceptance remains incomplete: techniques " +
          (state.techniquesPassed ? "pass" : "pending") +
          ", rubric " +
          (state.rubricPassed ? "pass" : "pending") +
          ", match gates " +
          (state.matchesPassed ? "pass" : "pending") +
          ", metadata " +
          (state.metadataPassed ? "pass" : "pending") +
          ", owner decision " +
          (state.decisionPassed ? "approved" : "pending") +
          ".";
      applyFilter();
    }

    function persistAndRefresh() {
      evidence.tester = testerInput.value;
      evidence.buildReference = buildInput.value;
      evidence.environment = environmentInput.value;
      evidence.notes = overallNotes.value;
      evidence.decision = decision.value;
      evidence.updatedAt = isoNow();
      if (storageAvailable) {
        try {
          root.localStorage.setItem(storageKey, JSON.stringify(evidence));
        } catch (error) {
          storageAvailable = false;
          panel.dataset.storage = "session-only";
          storageNote.textContent =
            "Saving failed. Keep this page open and export before closing it.";
        }
      }
      refresh();
    }

    function runtimeStatus() {
      return runtimeStatusElement
        ? cleanString(runtimeStatusElement.textContent).replace(/\s+/g, " ").trim()
        : "unavailable";
    }

    function exportPayload() {
      var state = readinessState();
      return {
        evidenceSchema: evidenceSchema,
        registrySchema: checklist.schema,
        sourceRevision: checklist.sourceRevision,
        exportedAt: isoNow(),
        readyForRepositoryReview: isReady(state),
        runtimeStatus: runtimeStatus(),
        evidence: evidence,
        techniques: checklist.techniques.map(function (technique) {
          var result = resultFor(technique.id);
          return {
            id: technique.id,
            name: technique.name,
            registryStatus: technique.registryStatus,
            result: result.result || "untested",
            notes: result.notes,
          };
        }),
      };
    }

    function exportMarkdown() {
      var payload = exportPayload();
      var state = readinessState();
      var lines = [
        "# M4 owner combat playtest evidence",
        "",
        "- Evidence schema: " + evidenceSchema,
        "- Registry schema: " + checklist.schema,
        "- Pinned source revision: " + checklist.sourceRevision,
        "- Tester / owner: " + (evidence.tester || "not supplied"),
        "- Build / commit: " + (evidence.buildReference || "not supplied"),
        "- Browser, OS, and input devices: " +
          (evidence.environment || "not supplied"),
        "- Started: " + evidence.createdAt,
        "- Updated: " + evidence.updatedAt,
        "- Exported: " + payload.exportedAt,
        "- Owner decision: " + decisionLabel(evidence.decision),
        "- Ready for repository review: " +
          (payload.readyForRepositoryReview ? "yes" : "no"),
        "",
        "Tactical and emergent rows were observed as ordinary legal match " +
          "sequences composed from independently verified mechanics; no " +
          "technique-only behavior or emergent-specific harness was used.",
        "",
        "## Mandatory match gates",
        "",
        "- Complete setup-to-result-to-rematch 1v1: " +
          (evidence.completedMatch ? "pass" : "not complete"),
        "- Both players used supported human inputs: " +
          (evidence.twoSupportedInputs ? "pass" : "not complete"),
        "- Real supported controller exercised: " +
          (evidence.realGamepad ? "pass" : "not complete"),
        "- Completed human matches: " + evidence.completedMatches,
        "",
        "## M0 combat rubric",
        "",
        "| Dimension | Score |",
        "|---|---:|",
      ];
      rubricDimensions.forEach(function (dimension) {
        lines.push(
          "| " + dimension.label + " | " +
            (evidence.rubric[dimension.id] || "not scored") + " |"
        );
      });
      lines.push(
        "",
        "- No critical collision anomaly: " +
          (evidence.noCriticalCollisionAnomaly ? "confirmed" : "not confirmed"),
        "- Rubric gate: " + (state.rubricPassed ? "pass" : "incomplete"),
        "",
        "## Advanced-technique browser recipes",
        "",
        "| # | Technique | Registry state | Owner result | Notes |",
        "|---:|---|---|---|---|"
      );
      payload.techniques.forEach(function (technique) {
        lines.push(
          "| " +
            technique.id +
            " | " +
            markdownCell(technique.name) +
            " | " +
            technique.registryStatus +
            " | " +
            technique.result +
            " | " +
            markdownCell(technique.notes) +
            " |"
        );
      });
      lines.push(
        "",
        "## Overall observations",
        "",
        evidence.notes || "None supplied.",
        "",
        "## Runtime evidence",
        "",
        "`" + runtimeStatus().replace(/`/g, "'") + "`",
        ""
      );
      return lines.join("\n");
    }

    [testerInput, buildInput, environmentInput, overallNotes].forEach(
      function (input) {
        input.addEventListener("input", persistAndRefresh);
      }
    );
    decision.addEventListener("change", persistAndRefresh);
    filter.addEventListener("change", applyFilter);
    markdownButton.addEventListener("click", function () {
      persistAndRefresh();
      downloadText(
        "m4-owner-playtest-" + isoNow().slice(0, 10) + ".md",
        exportMarkdown(),
        "text/markdown;charset=utf-8"
      );
    });
    jsonButton.addEventListener("click", function () {
      persistAndRefresh();
      downloadText(
        "m4-owner-playtest-" + isoNow().slice(0, 10) + ".json",
        JSON.stringify(exportPayload(), null, 2) + "\n",
        "application/json;charset=utf-8"
      );
    });
    var resetArmed = false;
    var resetTimer = null;
    resetButton.addEventListener("click", function () {
      if (!resetArmed) {
        resetArmed = true;
        resetButton.textContent = "Confirm Reset";
        resetTimer = root.setTimeout(function () {
          resetArmed = false;
          resetButton.textContent = "Reset Evidence";
        }, 5000);
        return;
      }
      if (resetTimer !== null) {
        root.clearTimeout(resetTimer);
        resetTimer = null;
      }
      resetArmed = false;
      resetButton.textContent = "Reset Evidence";
      evidence = freshEvidence(checklist.sourceRevision);
      testerInput.value = evidence.tester;
      buildInput.value = evidence.buildReference;
      environmentInput.value = evidence.environment;
      overallNotes.value = evidence.notes;
      decision.value = evidence.decision;
      completeMatchCheck.checked = false;
      twoInputsCheck.checked = false;
      gamepadCheck.checked = false;
      matchesInput.value = "0";
      collisionCheck.checked = false;
      rubricDimensions.forEach(function (dimension) {
        rubricSelects[dimension.id].value = "0";
      });
      checklist.techniques.forEach(function (technique) {
        var view = techniqueViews[String(technique.id)];
        view.notes.value = "";
        updateTechniqueView(technique.id);
      });
      persistAndRefresh();
    });

    refresh();
    return {
      panel: panel,
      exportMarkdown: exportMarkdown,
      exportPayload: exportPayload,
      getEvidence: function () {
        return evidence;
      },
    };
  };
})(typeof globalThis !== "undefined" ? globalThis : this);
