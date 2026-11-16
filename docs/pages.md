# GitHub Pages: prepared, not enabled

Both supplied CC BY images are authorized. Runtime artwork switching, title
navigation, the source QR screen and emulated CHERI LEDs are included.
The workflow is prepared and independently checked, but public publication
and deployment still require separate owner approval.

The workflow builds the shared C++ game with the existing locked Nix/Emscripten
preview shell. Pushes to `main` and ordinary pull requests run browser/native
checks with a read-only contents token. Checkout does not persist credentials.
There is no `pull_request_target`, privileged fork execution, self-hosted
runner, release job or automatic site enablement.

Only a manual dispatch from `main` with `deploy: true` and the repository
variable `PAGES_PUBLICATION_APPROVED=true` uploads a site artifact and permits
the deployment job. Build jobs cannot deploy; only the separate deployment job
receives `pages: write` and `id-token: write`. That job executes no checked-out
project code and uses the `github-pages` environment.

The artifact contains only HTML, JavaScript, WebAssembly and the two license
notices. Test screenshots, compiler outputs, caches and source PNG metadata
are not included. Relative asset URLs work under a repository Pages subpath.

## Future owner actions

Do not perform these merely because the workflow exists:

1. Confirm final source synchronization, test the runtime artwork/menu/QR/LED
   behavior and project-subpath hosting, complete independent validation, review
   provenance and licensing, and explicitly approve initial publication.
2. Audit the entire fresh history that would become public, including all
   refs. A working-tree scan or push-only check is not sufficient. Passing a
   publication check is not publication approval.
3. Configure Pages to use GitHub Actions. Create/protect `github-pages` with
   required reviewers, prevent self-review where available, and restrict
   deployment branches to `main` before enabling the approval variable.
4. After the owner approves that deployment, set the approval variable and
   manually dispatch the workflow from `main` with deployment enabled.
   The configure action has `enablement: false`: it cannot create the site.
5. Review and approve the environment deployment. Unset the approval variable
   to put future deployments on hold again.

A site deployment is not a firmware release. There is no tag/release workflow;
1.0 remains conditional on completed CHERI support and release acceptance.

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
