# Bianbu system tools (MCP, volume only)

This is a separate system-tool process, not an inference backend. KylinBot owns
planning and execution; the AI Adapter only forwards model requests and schemas.
Python 3.11+, standard library only. Run as the logged-in desktop user, never root.

Exposes `sys__set_volume(percent: integer, 0..100)` and `sys__get_volume()` when
registered with MCP server name `sys`. Does not expose shell, mute, radios, file
access, or privilege escalation. Uses fixed `/usr/bin/wpctl` argv, a clean desktop
runtime environment, strict parameter checking and separate setter/readback checks.
Muted state is preserved. Out-of-range, boolean, float and string inputs are rejected,
not silently clamped. A successful setter without matching readback is not success.

Protocol is newline-delimited JSON-RPC. Execution failures are explicit application
errors (-32010), because KylinBot 0.7.4 does not propagate MCP `result.isError` to
its ToolResult.success. Invalid arguments are -32602. The stream remains usable
after errors. Do not turn arbitrary model prose/JSON into executable commands.

Register via existing MCP config (`enabled=true`, `deferred_loading=false`), command
`/usr/bin/python3`, args `['-I', '/absolute/path/server.py']`, `tool_timeout_secs=15`.
Preserve supervised autonomy and all existing security settings. Update the model
Adapter's allowed tool names to include the two exact `sys__` names; otherwise they
will be removed before reaching the model. Keep ordinary file and chat tools intact.

Tests (no hardware access):

```sh
python3 -m unittest discover -s tools/bianbu-system-tools -p 'test_*.py' -v
```

Integration requires real MCP registration, native model tool_calls, Gateway tool
events, actual wpctl readback, truthful final text, and preservation of existing
voice services. Unit tests alone do not establish that the deployed loop works.
