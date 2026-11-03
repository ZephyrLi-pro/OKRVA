//! KylinBot Desktop — main entry point.

fn main() {
    tracing_subscriber::fmt::init();
    kylinbot_desktop::run();
}
