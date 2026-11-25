# Decision: Public release packaging strategy

## Question

For potential public release (Medium polish MVP), how should the VST3 plugin be packaged and distributed?

**Options:**
- (a) **GitHub Releases** (simple: zip with VST3 binary + README)
- (b) **Installer** (Windows .msi / Mac .dmg / Linux .AppImage or Snap)
- (c) **Linux distro packages** (Arch AUR, etc.; more work, later)
- (d) **Plugin repo** (Plugin Alliance, Splice, etc.; curated; requires approval)

For MVP (potential release, not committed): Recommend **(a) GitHub Releases.** Zip with pre-built VST3 binary + README covers 80% of users. Installers/distro packages post-MVP.

## Type: Decision (HITL Grilling)

**Assigned to:** <awaiting claim>

**Blockers:** None

**Blocks:** "Documentation scope" (packaging affects what docs to include)
