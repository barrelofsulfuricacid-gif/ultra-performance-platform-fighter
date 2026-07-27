mergeInto(LibraryManager.library, {
  pf_web_set_status__deps: ["$UTF8ToString"],
  pf_web_set_status__sig: "vp",
  pf_web_set_status: function (messagePointer) {
    var status = document.getElementById("pf-status");

    if (!status) {
      status = document.createElement("pre");
      status.id = "pf-status";
      document.body.appendChild(status);
    }

    status.textContent = UTF8ToString(messagePointer);
  },
});
