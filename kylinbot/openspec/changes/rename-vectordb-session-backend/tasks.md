## 1. Rename the source file

- [x] 1.1 Rename `crates/kylinbot-infra/src/vector_db_session_backend.rs` to `crates/kylinbot-infra/src/session_backend_vectordb.rs` using `git mv`

## 2. Update module declaration

- [x] 2.1 Update `pub mod vector_db_session_backend;` to `pub mod session_backend_vectordb;` in `crates/kylinbot-infra/src/lib.rs`

## 3. Update all imports

- [x] 3.1 Update import in `crates/kylinbot-gateway/src/lib.rs` from `vector_db_session_backend` to `session_backend_vectordb`
- [x] 3.2 Update import in `crates/kylinbot-channels/src/orchestrator/acp_server.rs` from `vector_db_session_backend` to `session_backend_vectordb`
- [x] 3.3 Update import in `crates/kylinbot-runtime/src/agent/loop_.rs` from `vector_db_session_backend` to `session_backend_vectordb`

## 4. Verification

- [x] 4.1 Run `cargo check` to verify compilation succeeds
- [x] 4.2 Run `cargo test -p kylinbot-infra` to verify tests pass
