# TDR-0001: Authored C and foreign ABI boundaries

- **Status:** Accepted
- **Date:** 2026-07-27
- **Implements:** D2-A

## Decision

Simulation, gameplay, client orchestration, tools, and every public project API
are authored in C17. A third-party implementation that requires C++ is built as
a private foreign island behind one narrow authored C adapter.

The initial allowed foreign islands are:

- Dear ImGui plus generated Dear Bindings implementation.
- A native WebRTC implementation if it wins the M9 transport spike.
- Tracy's C++ profiler client in profile-only builds.
- The optional Python Gymnasium wrapper outside the authoritative C RL API.

No project gameplay behavior is implemented in those islands.

## ABI constraints

- Public headers compile as both C17 and C++ without changing layout.
- C++ exceptions are caught inside the island.
- C++ object lifetime is represented by opaque handles and explicit
  create/destroy calls.
- Strings and bytes cross as pointer/length views or caller-owned buffers.
- Callbacks use C calling conventions, a context pointer, and documented thread
  rules.
- Foreign allocation never requires the caller to invoke a mismatched
  allocator.
- Sanitizer tests repeatedly create, fail, use, and destroy each adapter.

## Consequences

This adds adapter code and prevents convenient direct use of C++ APIs. It keeps
the authored codebase, deterministic core, headless product, and public ABI
independent of third-party language/runtime decisions. A dependency can be
replaced without rewriting gameplay call sites.

## Rejected alternatives

- Author the engine in C++: conflicts with D2-A.
- Allow C++ types in internal public headers: makes the boundary contagious and
  prevents a genuinely C headless/RL surface.
- Fork every C++ dependency into C immediately: too expensive before a
  dependency wins its representative spike.
