//! Provider subsystem — re-exported from `kylinbot-providers`.

pub use kylinbot_providers::*;

// Keep traits.rs as a file module so its #[cfg(test)] block compiles.
#[path = "traits.rs"]
pub mod traits;
