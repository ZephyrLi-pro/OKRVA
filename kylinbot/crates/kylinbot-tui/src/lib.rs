//! Ratatui-backed `OnboardUi` implementation.
//!
//! The onboard orchestrator lives in `kylinbot-runtime`; this crate only
//! provides the TUI drawing/input layer. `RatatuiUi` is the one export.

mod onboarding;
mod theme;
mod widgets;

pub use onboarding::RatatuiUi;
