## ADDED Requirements

### Requirement: File naming follows session module convention
The VectorDB session backend module SHALL be named `session_backend_vectordb.rs` to match the established `session_*.rs` naming pattern in the `kylinbot-infra` crate.

#### Scenario: File exists with correct name
- **WHEN** listing files in `crates/kylinbot-infra/src/`
- **THEN** the file `session_backend_vectordb.rs` exists
- **AND** the file `vector_db_session_backend.rs` does not exist

### Requirement: Module declaration matches file name
The module declaration in `lib.rs` SHALL use the module name `session_backend_vectordb`.

#### Scenario: Module is correctly declared
- **WHEN** reading `crates/kylinbot-infra/src/lib.rs`
- **THEN** it contains `pub mod session_backend_vectordb;`
- **AND** it does NOT contain `pub mod vector_db_session_backend;`

### Requirement: All imports use new module path
All code referencing the VectorDB session backend SHALL use the updated module path `session_backend_vectordb`.

#### Scenario: Gateway imports updated
- **WHEN** reading `crates/kylinbot-gateway/src/lib.rs`
- **THEN** it uses `kylinbot_infra::session_backend_vectordb::VectorDBSessionBackend`

#### Scenario: ACP server imports updated
- **WHEN** reading `crates/kylinbot-channels/src/orchestrator/acp_server.rs`
- **THEN** it uses `kylinbot_infra::session_backend_vectordb::VectorDBSessionBackend`

#### Scenario: Loop agent imports updated
- **WHEN** reading `crates/kylinbot-runtime/src/agent/loop_.rs`
- **THEN** it uses `kylinbot_infra::session_backend_vectordb::VectorDBSessionBackend`
