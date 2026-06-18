// SteamSwitch — Tauri shell.
//
// Thin native host for the existing web/ UI. It:
//   1. spawns the Python sidecar (server.py in dev, the bundled server[.exe] in
//      release) and reads its stdout for the `STEAMSWITCH_SIDECAR_READY <url>` line,
//   2. opens the window with that URL injected as `window.__SIDECAR__` (bridge.js
//      reads it), and
//   3. kills the sidecar when the window closes.
//
// WebView2 is initialised asynchronously by Tauri, so the window never blocks the
// UI thread (the ~21s "Not Responding" freeze under pywebview is gone).

// No extra console window on Windows release builds.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::io::{BufRead, BufReader};
use std::process::{Child, Command, Stdio};
use std::sync::Mutex;

use tauri::{Manager, WebviewUrl, WebviewWindowBuilder, WindowEvent};

/// Holds the sidecar child so we can kill it on exit.
struct Sidecar(Mutex<Option<Child>>);

/// Build the command that starts the sidecar. Dev runs the Python source from the
/// repo root; release runs the bundled `server[.exe]` next to the app executable.
fn sidecar_command() -> Command {
    let mut cmd;
    #[cfg(debug_assertions)]
    {
        let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .expect("repo root")
            .to_path_buf();
        let py = if cfg!(windows) { "python" } else { "python3" };
        cmd = Command::new(py);
        cmd.arg(root.join("server.py")).current_dir(root);
    }
    #[cfg(not(debug_assertions))]
    {
        let exe = std::env::current_exe().expect("current_exe");
        let dir = exe.parent().expect("exe dir").to_path_buf();
        let name = if cfg!(windows) { "server.exe" } else { "server" };
        cmd = Command::new(dir.join(name));
    }
    // Let the OS pick a free port; the sidecar reports the real one in its READY line.
    cmd.arg("--port").arg("0");
    cmd
}

/// Start the sidecar and block until it prints its READY line (or its stdout ends).
/// Returns the child and the discovered base URL.
fn start_sidecar() -> (Child, String) {
    let mut cmd = sidecar_command();
    cmd.stdout(Stdio::piped());
    // Release: discard the sidecar's stderr (no console to write to). Dev: inherit it
    // so server.py's logs show in the `cargo tauri dev` terminal.
    #[cfg(not(debug_assertions))]
    cmd.stderr(Stdio::null());
    // Windows: spawn the (console-subsystem) sidecar WITHOUT a console window —
    // otherwise a terminal pops up alongside the app. Redirected stdout still works.
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        const CREATE_NO_WINDOW: u32 = 0x0800_0000;
        cmd.creation_flags(CREATE_NO_WINDOW);
    }
    let mut child = cmd.spawn().expect("failed to start the SteamSwitch sidecar");

    let stdout = child.stdout.take().expect("sidecar stdout");
    let mut reader = BufReader::new(stdout);
    let mut url = String::new();
    let mut line = String::new();
    while reader.read_line(&mut line).unwrap_or(0) > 0 {
        if let Some(rest) = line.trim().strip_prefix("STEAMSWITCH_SIDECAR_READY ") {
            url = rest.to_string();
            break;
        }
        line.clear();
    }

    // Keep draining stdout so the pipe never fills and stalls the sidecar.
    std::thread::spawn(move || {
        let mut l = String::new();
        while reader.read_line(&mut l).unwrap_or(0) > 0 {
            l.clear();
        }
    });

    (child, url)
}

fn main() {
    let (child, url) = start_sidecar();
    if url.is_empty() {
        // Sidecar didn't report a URL — nothing to talk to. Bail loudly in dev.
        eprintln!("SteamSwitch: sidecar failed to start (no READY line).");
    }

    tauri::Builder::default()
        .manage(Sidecar(Mutex::new(Some(child))))
        .setup(move |app| {
            // Injected before the page's own scripts run; bridge.js reads it.
            let init = format!("window.__SIDECAR__ = {url:?};");
            WebviewWindowBuilder::new(app, "main", WebviewUrl::App("index.html".into()))
                .title("SteamSwitch")
                .inner_size(1100.0, 720.0)
                .min_inner_size(820.0, 520.0)
                .initialization_script(&init)
                .build()?;
            Ok(())
        })
        .on_window_event(|window, event| {
            if matches!(event, WindowEvent::Destroyed) {
                if let Some(state) = window.try_state::<Sidecar>() {
                    if let Ok(mut guard) = state.0.lock() {
                        if let Some(mut child) = guard.take() {
                            let _ = child.kill();
                        }
                    }
                }
            }
        })
        .run(tauri::generate_context!())
        .expect("error while running SteamSwitch");
}
