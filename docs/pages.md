# GitHub Pages: v1.0.0 deployment

The owner approved initial public publication, Pages deployment and the
v1.0.0 release on November 17, 2026. Both supplied CC BY 4.0 splash images are
authorized. The original LCD artwork is the default; hidden Down switching
during the splash, title navigation, the source QR and emulated CHERI LEDs
are included.

Source: https://github.com/xjamesmorris/gta6-cheri-sonata

Site: https://xjamesmorris.github.io/gta6-cheri-sonata/

These are the repository and Pages targets. Deployments must satisfy the
gates below.

The workflow builds the shared C++ game with the existing locked Nix/Emscripten
preview shell. Pushes to `main` and ordinary pull requests run browser/native
checks with a read-only contents token. Checkout does not persist credentials.
There is no `pull_request_target`, privileged fork execution, self-hosted
runner, release job or automatic site enablement.

The hosted Ubuntu 24.04 build job loads an AppArmor profile for the Nix-store
Chromium executable so it can create its user-namespace sandbox. Chromium
sandboxing stays enabled; neither global user-namespace restrictions nor
AppArmor are disabled. This runner-only profile follows
[Ubuntu's application-specific guidance](https://documentation.ubuntu.com/release-notes/24.04/#unprivileged-user-namespace-restrictions)
and does not change the developer workstation.

Only a manual dispatch from `main` with `deploy: true` and the repository
variable `PAGES_PUBLICATION_APPROVED=true` uploads a site artifact and permits
the deployment job. Build jobs cannot deploy; only the separate deployment job
receives `pages: write` and `id-token: write`. That job executes no checked-out
project code and uses the `github-pages` environment.

The artifact contains exactly six files: `index.html`, `preview.js`, `game.js`,
`game.wasm`, `artwork-license.txt` and `code-license.txt`. Test screenshots,
compiler outputs, caches and source PNG metadata are not included. The page
retains its explicit **EMULATED** protection banner and MIT code-license link.
Relative asset URLs work under the repository Pages subpath.

## Owner publication and deployment procedure

Initial publication and deployment are authorized, but must still satisfy
the repository and environment gates:

1. Review the final source and documented verification boundaries. Run
   `./tools/check` and `./tools/check-browser` without an artwork override to
   check the original-artwork default, including hidden splash Down and the
   public project-subpath, mobile and unsigned-counter regressions.
2. Audit the entire fresh history that would become public, including all
   refs. A working-tree scan or push-only check is not sufficient. Passing a
   publication check is not publication approval.
3. Configure Pages to use GitHub Actions and restrict the `github-pages`
   deployment environment to `main`. Preserve and satisfy any configured
   required-reviewer and self-review protections; do not remove or bypass
   them. The workflow does not prescribe additional reviewer identities.
4. For the owner-approved deployment, set `PAGES_PUBLICATION_APPROVED=true`
   and manually dispatch the workflow from `main` with `deploy: true`.
   The configure action has `enablement: false`: it cannot create the site.
5. Complete any environment approvals, confirm the deployment result and check
   the published project URL. Unset the approval variable to put future
   deployments on hold again.

A site deployment is not a firmware release. There is no tag/release workflow;
the owner-authorized v1.0.0 tag and release are separate publication actions.

## Verified action revisions

Release tags and their commit objects were checked against the owning GitHub
repositories on **November 16, 2026**. Workflow references use full immutable
commit IDs, not floating version tags:

| Action | Release | Commit |
| --- | --- | --- |
| `actions/checkout` | `v7.0.1` | `3d3c42e5aac5ba805825da76410c181273ba90b1` |
| `cachix/install-nix-action` | `v31.11.5` | `3dc140bfc76c49fbd4dd958429132e3746bcb086` |
| `actions/upload-pages-artifact` | `v5.0.0` | `fc324d3547104276b827a68afc52ff2a11cc49c9` |
| `actions/configure-pages` | `v6.0.0` | `45bfe0192ca1faeb007ade9deae92b16b8254a0d` |
| `actions/deploy-pages` | `v5.0.1` | `368f82528645a54fb793d4d04e342629a3f51346` |

The pinned artifact action also pins its nested `actions/upload-artifact`
dependency to `bbbca2ddaa5d8feaa63e36b76fdaad77386f024f` (`v7.0.0`).
Recheck upstream release notes, inputs and immutable revisions when updating.

Primary sources:

- [GitHub custom Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
- [GitHub Actions secure-use reference](https://docs.github.com/en/actions/reference/security/secure-use)
- [Checkout release](https://github.com/actions/checkout/releases/tag/v7.0.1)
- [Nix installer release](https://github.com/cachix/install-nix-action/releases/tag/v31.11.5)
- [Upload Pages artifact release](https://github.com/actions/upload-pages-artifact/releases/tag/v5.0.0)
- [Configure Pages release](https://github.com/actions/configure-pages/releases/tag/v6.0.0)
- [Deploy Pages release](https://github.com/actions/deploy-pages/releases/tag/v5.0.1)
- [Pinned artifact action definition](https://github.com/actions/upload-pages-artifact/blob/fc324d3547104276b827a68afc52ff2a11cc49c9/action.yml)
