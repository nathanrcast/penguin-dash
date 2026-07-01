# Penguin Dash

A kid-friendly (ages 6–10) downhill racer for Android tablets: slide Tux the penguin down snowy
slopes on his belly, grab herring, and beat the clock. Ad-free, offline, open source.

**Penguin Dash is a rebranded fork of [Extreme Tux Racer](https://sourceforge.net/projects/extremetuxracer/)**,
being modernized and ported to Android. It remains licensed under the **GPL** (see `COPYING`); original
project docs are preserved in `UPSTREAM_README` and authors in `AUTHORS`.

Fork base: Extreme Tux Racer **0.8.4** source tarball (imported 2026-06-30; upstream ships tarballs,
no public git, so there is no upstream commit history). Play-tested + approved 2026-06-30.

## Status

**Porting.** See [`docs/port_plan.md`](docs/port_plan.md). Desktop build is C++/**autotools** +
**SFML 2.5** + **OpenGL**. Main port challenge: the renderer uses **legacy immediate-mode GL**
(`glBegin`/`glVertex`, ~76 call sites) which does not exist in GLES — it needs a GLES2 rewrite. A
play-tested Flathub build (`net.sourceforge.ExtremeTuxRacer` 0.8.4) runs on `.13`.

## Credits & license

- Original game: Extreme Tux Racer team (`AUTHORS`), descended from Tux Racer.
- Licensed under the GNU General Public License (`COPYING`). Source stays open per the GPL.
