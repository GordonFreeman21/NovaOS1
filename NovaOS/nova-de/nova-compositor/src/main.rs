//! NovaOS Wayland Compositor
//! 
//! A modern, GPU-accelerated Wayland compositor built with Rust and wlroots.

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

mod compositor;
mod render;
mod input;
mod output;
mod seat;
mod cursor;
mod xdg_shell;
mod layer_shell;
mod desktop;

use compositor::Compositor;
use wlr_sys::*;

/// Running state for the compositor
struct CompositorState {
    running: AtomicBool,
    compositor: RefCell<Option<Compositor>>,
}

impl CompositorState {
    fn new() -> Self {
        Self {
            running: AtomicBool::new(true),
            compositor: RefCell::new(None),
        }
    }
    
    fn stop(&self) {
        self.running.store(false, Ordering::SeqCst);
    }
    
    fn is_running(&self) -> bool {
        self.running.load(Ordering::SeqCst)
    }
}

/// Initialize logging
fn init_logging() {
    env_logger::Builder::from_env(
        env_logger::Env::default().default_filter_or("info")
    ).init();
}

/// Parse command line arguments
fn parse_args() -> Result<(), String> {
    let args: Vec<String> = std::env::args().collect();
    
    for arg in args.iter() {
        match arg.as_str() {
            "--help" | "-h" => {
                println!("NovaOS Wayland Compositor");
                println!();
                println!("Usage: nova-compositor [OPTIONS]");
                println!();
                println!("Options:");
                println!("  -h, --help       Show this help message");
                println!("  -V, --version    Show version information");
                println!("  --backend BACK   Set backend (auto, drm, wayland, x11)");
                println!("  --config FILE    Use custom config file");
                println!("  --debug          Enable debug output");
                return Err("help".to_string());
            }
            "--version" | "-V" => {
                println!("nova-compositor {}", env!("CARGO_PKG_VERSION"));
                return Err("version".to_string());
            }
            _ => {}
        }
    }
    
    Ok(())
}

/// Main entry point
fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize logging
    init_logging();
    
    log::info!("Starting NovaOS Wayland Compositor v{}", env!("CARGO_PKG_VERSION"));
    
    // Parse arguments
    if let Err(e) = parse_args() {
        if e == "help" || e == "version" {
            return Ok(());
        }
    }
    
    // Create compositor state
    let state = Rc::new(CompositorState::new());
    
    // Setup signal handlers
    setup_signal_handlers(state.clone());
    
    // Initialize and run compositor
    run_compositor(state)?;
    
    log::info!("Compositor shutdown complete");
    
    Ok(())
}

/// Setup Unix signal handlers
fn setup_signal_handlers(state: Rc<CompositorState>) {
    use signal_hook::consts::signal::*;
    
    let state_clone = state.clone();
    signal_hook::flag::register(SIGINT, move || {
        log::info!("Received SIGINT, shutting down...");
        state_clone.stop();
    }).expect("Failed to register SIGINT handler");
    
    let state_clone = state.clone();
    signal_hook::flag::register(SIGTERM, move || {
        log::info!("Received SIGTERM, shutting down...");
        state_clone.stop();
    }).expect("Failed to register SIGTERM handler");
}

/// Run the compositor event loop
fn run_compositor(state: Rc<CompositorState>) -> Result<(), Box<dyn std::error::Error>> {
    // Create and initialize the compositor
    let mut compositor = Compositor::new()?;
    
    // Initialize outputs (displays)
    compositor.init_outputs()?;
    
    // Initialize input devices
    compositor.init_input()?;
    
    // Initialize shell protocols
    compositor.init_shells()?;
    
    // Start XWayland if enabled
    compositor.start_xwayland()?;
    
    // Store compositor in state
    *state.compositor.borrow_mut() = Some(compositor);
    
    // Run the main event loop
    while state.is_running() {
        // Dispatch Wayland events
        if let Some(ref mut comp) = *state.compositor.borrow_mut() {
            comp.dispatch_events()?;
            
            // Render all outputs
            comp.render_outputs()?;
        }
        
        // Small sleep to prevent busy-waiting
        std::thread::sleep(std::time::Duration::from_millis(16));
    }
    
    // Cleanup
    if let Some(mut comp) = state.compositor.borrow_mut().take() {
        comp.shutdown()?;
    }
    
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_compositor_state() {
        let state = CompositorState::new();
        assert!(state.is_running());
        state.stop();
        assert!(!state.is_running());
    }
}
