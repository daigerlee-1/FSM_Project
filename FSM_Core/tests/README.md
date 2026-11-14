# FSM host tests

From the repository root, run with GCC on PATH:

```powershell
& ./FSM_Core/tests/run_tests.ps1
```

The runner builds a temporary executable with strict C99 warnings and removes it
after execution. `support/prj_typedefs.h` supplies host-only types because the
target project header is not included in this repository. Keep this support
directory out of production include paths.

Compile-only tests exercise `FSM_DEFINE_CONFIG`: valid arrays, the 65535 count
boundary, state arrays shorter/longer than STATE_MAX, 65536 transitions, and
zero/overflowing state/event counts. Negative cases must fail with the expected
named typedef assertion; an unrelated compiler error does not count as a pass.
The boundary fixtures declare arrays with `extern` and use syntax-only compilation,
so they allocate no large runtime buffers.

Coverage includes same-context Init/Dispatch/Do rejection from initial entry,
guard, exit, action, transition entry, and doAction; callback ordering; preservation
of queued events and execution ownership; posting and querying from callbacks;
independent instance execution; rejection-count persistence and saturation;
empty queues, blocked/unmatched events, invalid configuration, and null or
zero-initialized contexts.

These tests do not establish target ABI compatibility, MISRA/QAC compliance, or
task/ISR synchronization. They intentionally test cooperative callback reentry.
