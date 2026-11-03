//! Mobile entry point for KylinBot Desktop (iOS/Android).

#[tauri::mobile_entry_point]
fn main() {
    kylinbot_desktop::run();
}
