# Penguin Dash — agent notes

Extreme Tux Racer fork, ported to Android.

Desktop build stays upstream autotools. The port work:

- **GLES2 renderer rewrite (M0–M6): DONE** — background + audit in `docs/gles_render_audit.md`.
- **Android shell:** direct-EGL/NativeActivity under `android/` — milestones and device-verification state live in `docs/android_port_plan.md`. Read it before touching `android/`.
- **Performance:** plan, state and gotchas in `docs/performance_review.md`.
- Overall port strategy: `docs/port_plan.md`; codebase notes: `docs/codebase_modernization_review.md`.

Verify rendering changes on a real device — emulator GLES behavior does not match hardware.

**Public repo (GPL-2.0).** Lab details (build hosts, devices, push flow) go in the gitignored `CLAUDE.local.md`.
