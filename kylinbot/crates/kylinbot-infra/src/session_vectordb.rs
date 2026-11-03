//! Local-file session backend for VectorDB memory mode.
//!
//! Messages are stored as append-only JSONL files via [`SessionStore`].
//! Session metadata is stored alongside each transcript as
//! `{sessions_dir}/{session_key}_meta.json`.

use crate::session_backend::{SessionBackend, SessionMetadata, SessionState};
use crate::session_store::SessionStore;
use chrono::{DateTime, Utc};
use kylinbot_api::provider::ChatMessage;
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Serialize, Deserialize)]
struct VectorDBSessionMeta {
    name: Option<String>,
    created_at: DateTime<Utc>,
    last_activity: DateTime<Utc>,
    #[serde(default)]
    message_count: usize,
    #[serde(default = "default_idle")]
    state: String,
    turn_id: Option<String>,
    turn_started_at: Option<DateTime<Utc>>,
}

fn default_idle() -> String {
    "idle".into()
}

pub struct VectorDBSessionBackend {
    store: SessionStore,
    sessions_dir: PathBuf,
}

impl VectorDBSessionBackend {
    pub fn new(workspace_dir: &Path) -> std::io::Result<Self> {
        let store = SessionStore::new(workspace_dir)?;
        let sessions_dir = workspace_dir.join("sessions");
        if !sessions_dir.exists() {
            std::fs::create_dir_all(&sessions_dir)?;
        }
        Ok(Self {
            store,
            sessions_dir,
        })
    }

    fn meta_path(&self, session_key: &str) -> PathBuf {
        self.sessions_dir.join(format!("{session_key}_meta.json"))
    }

    fn load_meta(&self, session_key: &str) -> Option<VectorDBSessionMeta> {
        let path = self.meta_path(session_key);
        if !path.exists() {
            return None;
        }
        let content = std::fs::read_to_string(&path).ok()?;
        serde_json::from_str(&content).ok()
    }

    fn save_meta(&self, session_key: &str, meta: &VectorDBSessionMeta) -> std::io::Result<()> {
        let path = self.meta_path(session_key);
        let json = serde_json::to_string_pretty(meta)
            .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
        std::fs::write(&path, json)
    }

    fn delete_meta(&self, session_key: &str) -> std::io::Result<()> {
        let path = self.meta_path(session_key);
        if path.exists() {
            std::fs::remove_file(&path)?;
        }
        Ok(())
    }
}

impl SessionBackend for VectorDBSessionBackend {
    fn load(&self, session_key: &str) -> Vec<ChatMessage> {
        self.store.load(session_key)
    }

    fn append(&self, session_key: &str, message: &ChatMessage) -> std::io::Result<()> {
        self.store.append(session_key, message)?;
        let now = Utc::now();
        let mut meta = self
            .load_meta(session_key)
            .unwrap_or_else(|| VectorDBSessionMeta {
                name: None,
                created_at: now,
                last_activity: now,
                message_count: 0,
                state: "idle".into(),
                turn_id: None,
                turn_started_at: None,
            });
        meta.last_activity = now;
        meta.message_count += 1;
        self.save_meta(session_key, &meta)
    }

    fn remove_last(&self, session_key: &str) -> std::io::Result<bool> {
        let removed = self.store.remove_last(session_key)?;
        if removed && let Some(mut meta) = self.load_meta(session_key) {
            meta.message_count = meta.message_count.saturating_sub(1);
            self.save_meta(session_key, &meta)?;
        }
        Ok(removed)
    }

    fn list_sessions(&self) -> Vec<String> {
        self.store.list_sessions()
    }

    fn compact(&self, session_key: &str) -> std::io::Result<()> {
        self.store.compact(session_key)
    }

    fn clear_messages(&self, session_key: &str) -> std::io::Result<usize> {
        let count = self.store.clear_messages(session_key)?;
        if count > 0
            && let Some(mut meta) = self.load_meta(session_key)
        {
            meta.message_count = 0;
            meta.last_activity = Utc::now();
            self.save_meta(session_key, &meta)?;
        }
        Ok(count)
    }

    fn delete_session(&self, session_key: &str) -> std::io::Result<bool> {
        let deleted = self.store.delete_session(session_key)?;
        if deleted {
            self.delete_meta(session_key)?;
        }
        Ok(deleted)
    }

    fn set_session_name(&self, session_key: &str, name: &str) -> std::io::Result<()> {
        if let Some(mut meta) = self.load_meta(session_key) {
            meta.name = if name.is_empty() {
                None
            } else {
                Some(name.to_string())
            };
            self.save_meta(session_key, &meta)?;
        }
        Ok(())
    }

    fn get_session_name(&self, session_key: &str) -> std::io::Result<Option<String>> {
        Ok(self.load_meta(session_key).and_then(|m| m.name))
    }

    fn get_session_metadata(&self, session_key: &str) -> Option<SessionMetadata> {
        let meta = self.load_meta(session_key)?;
        Some(SessionMetadata {
            key: session_key.to_string(),
            name: meta.name,
            created_at: meta.created_at,
            last_activity: meta.last_activity,
            message_count: meta.message_count,
        })
    }

    fn set_session_state(
        &self,
        session_key: &str,
        state: &str,
        turn_id: Option<&str>,
    ) -> std::io::Result<()> {
        if let Some(mut meta) = self.load_meta(session_key) {
            meta.state = state.to_string();
            meta.turn_id = turn_id.map(str::to_string);
            meta.turn_started_at = if state == "running" {
                Some(Utc::now())
            } else {
                None
            };
            self.save_meta(session_key, &meta)?;
        }
        Ok(())
    }

    fn get_session_state(&self, session_key: &str) -> std::io::Result<Option<SessionState>> {
        Ok(self.load_meta(session_key).map(|meta| SessionState {
            state: meta.state,
            turn_id: meta.turn_id,
            turn_started_at: meta.turn_started_at,
        }))
    }

    fn list_sessions_with_metadata(&self) -> Vec<SessionMetadata> {
        self.store
            .list_sessions()
            .into_iter()
            .filter_map(|key| self.get_session_metadata(&key))
            .collect()
    }

    fn list_running_sessions(&self) -> Vec<SessionMetadata> {
        self.list_sessions_with_metadata()
            .into_iter()
            .filter(|meta| {
                self.load_meta(&meta.key)
                    .map(|session_meta| session_meta.state == "running")
                    .unwrap_or(false)
            })
            .collect()
    }

    fn list_stuck_sessions(&self, threshold_secs: u64) -> Vec<SessionMetadata> {
        #[allow(clippy::cast_possible_wrap)]
        let cutoff = Utc::now() - chrono::Duration::seconds(threshold_secs as i64);
        self.list_sessions_with_metadata()
            .into_iter()
            .filter(|meta| {
                self.load_meta(&meta.key)
                    .map(|session_meta| {
                        session_meta.state == "running"
                            && session_meta
                                .turn_started_at
                                .is_some_and(|started| started < cutoff)
                    })
                    .unwrap_or(false)
            })
            .collect()
    }

    fn cleanup_stale(&self, ttl_hours: u32) -> std::io::Result<usize> {
        let cutoff = Utc::now() - chrono::Duration::hours(i64::from(ttl_hours));
        let stale_keys: Vec<String> = self
            .list_sessions_with_metadata()
            .into_iter()
            .filter(|meta| {
                self.load_meta(&meta.key)
                    .map(|session_meta| session_meta.last_activity < cutoff)
                    .unwrap_or(false)
            })
            .map(|meta| meta.key)
            .collect();
        let count = stale_keys.len();
        for key in &stale_keys {
            let _ = self.store.delete_session(key);
            self.delete_meta(key)?;
        }
        Ok(count)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn append_creates_jsonl_and_meta_files() {
        let tmp = TempDir::new().unwrap();
        let backend = VectorDBSessionBackend::new(tmp.path()).unwrap();

        backend
            .append("gw_abc123", &ChatMessage::user("hello"))
            .unwrap();

        assert!(tmp.path().join("sessions/gw_abc123.jsonl").exists());
        assert!(tmp.path().join("sessions/gw_abc123_meta.json").exists());
    }

    #[test]
    fn metadata_tracks_message_count() {
        let tmp = TempDir::new().unwrap();
        let backend = VectorDBSessionBackend::new(tmp.path()).unwrap();

        backend
            .append("cli_main", &ChatMessage::user("one"))
            .unwrap();
        backend
            .append("cli_main", &ChatMessage::assistant("two"))
            .unwrap();

        let meta = backend.get_session_metadata("cli_main").unwrap();
        assert_eq!(meta.message_count, 2);
    }

    #[test]
    fn delete_session_removes_meta_file() {
        let tmp = TempDir::new().unwrap();
        let backend = VectorDBSessionBackend::new(tmp.path()).unwrap();

        backend
            .append("cli_main", &ChatMessage::user("hello"))
            .unwrap();
        assert!(tmp.path().join("sessions/cli_main_meta.json").exists());

        assert!(backend.delete_session("cli_main").unwrap());
        assert!(!tmp.path().join("sessions/cli_main_meta.json").exists());
    }
}
