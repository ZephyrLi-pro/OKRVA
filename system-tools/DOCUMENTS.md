# Scoped document summaries

Output layout is now **one file per source document**. A three-document task
creates three independent Markdown files, each containing only its own source,
mtime and original-text key points. Source stems are sanitized into names such as
`文档摘要-b_network-YYYYMMDD-HHMMSS-8hex.md`. The save tool returns a verified
receipt per document ID with source name, path, byte count and SHA-256. The
Adapter requires distinct paths and all N receipts before announcing completion.
No aggregate file is generated. Existing reports are not replaced or removed.

All entries and sources are checked before any write. A later write failure
reports the actual confirmed count and keeps any partial files; that batch cannot
be retried automatically. Both legacy aggregate names and new source-based names
remain eligible for scoped listing/Trash. Filename words are treated as opaque
identifiers, never as commands.

Generated Desktop reports can also be listed with `list_reports` and moved to
the system Trash with `trash_reports`. Only direct regular user-owned files
matching the generated report name/header are eligible. Opaque IDs bind a
ten-minute listing to inode/mtime/size and SHA-256; changed files stop before
moving. GIO performs the move, and original-path absence, `.trashinfo` original
location and Trash file bytes are checked. No permanent delete, recursion,
wildcard path input, Trash emptying or automatic retry is provided.

The model selects preview/all/named cleanup. Vague "several" requests only list
files; moving requires explicit all-report scope or complete report names in the
user's current request. KylinBot executes the native calls. Both tool names must
be in the Adapter allowlist and `document_tasks` must be enabled. Generic chat
cannot issue `docs__trash_reports` outside this validated workflow. The original
generic write/approval policy is unchanged. Existing user reports are preserved
during acceptance; real effect tests use separate synthetic reports.

The model selects a recent-document summary task and its count. KylinBot executes
`docs__recent_documents`, one `docs__read_document` per input, then
`docs__save_report`. The Adapter binds every continuation to the original history,
catalog, issued call IDs, batch and returned source metadata.

The MCP service exposes configured Documents/Desktop roots, opaque document IDs
and a new-report writer. It does not accept arbitrary model paths, execute shell
commands, invoke inference or perform OCR. Existing generic file-write approval
policy is not widened. Sources remain unchanged, and report creation cannot
overwrite an existing file.

Recent-file discovery sorts actual nanosecond modification times. Newer empty,
unsupported or oversized files are reported as errors instead of being silently
replaced with older files. Hidden entries and symlinks are excluded. TXT/MD use
UTF-8; PDF/DOC/DOCX/WPS/PPT/PPTX use the shared native DocumentReader CLI.

Summaries currently use model-selected original sentences, up to three per file,
with source names, modification times and paths. Native line wraps are normalized
and presentation page markers/titles are omitted from summary candidates. Each
saved point must be present in the extracted source, and save verifies the file
bytes by readback. This deliberately prevents generated numeric facts.

Limits: 1–5 files, 20 MiB per file, 24000 extracted characters per file, bounded
summary inference time. Unsupported, changed, empty or unextractable documents
stop the batch before saving. Scanned pages are not OCRed.

Build the shared reader in `kylin-aiassistant-lite/tools/document-reader-cli`:

```sh
qmake reader-cli.pro
make -j1
```

It uses the existing `kylin-aiassistant/global` sources, QtCore, ukui-log4qt,
pdftotext and LibreOffice. No QtWebEngine, KyTensor, cloud endpoint or additional
Python package is required by this module. The generated fixture libraries in
the development test script are not runtime dependencies.

Run the standalone service with explicit trusted roots and helper:

```sh
python3 -I document_mcp.py --documents-root /path/to/Documents --desktop-root /path/to/Desktop --reader-cli /path/to/kylin-document-reader
```

Adapter configuration requires `semantic_system_planner=true`,
`grounded_system_planner=true`, `document_tasks=true` and the three prefixed MCP
tool names in its allowlist. Gateway server name is `docs`; Office extraction
requires a tool deadline of 300 seconds. These are configured by the guarded
installer after isolated acceptance. The model and speech backends are unchanged.
