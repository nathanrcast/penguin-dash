# Penguin Dash — Extreme Tux Racer fork (Android port)

Kids' Android port of Extreme Tux Racer. Desktop build stays upstream autotools; the port work:

- **GLES2 renderer rewrite (M0–M6): DONE** — background + audit in `docs/gles_render_audit.md`.
- **Android shell (current work):** direct-EGL/NativeActivity under `android/` — step list, progress, and device-verification state live in `docs/android_port_plan.md`. Read it before touching `android/`.
- Overall port strategy: `docs/port_plan.md`; codebase notes: `docs/codebase_modernization_review.md`.

Verify rendering changes on a real device — emulator GLES behavior does not match hardware.
