<!-- SPDX-License-Identifier: MIT -->

# LLM contribution policy

This policy applies to code, tests, documentation, reviews, audit policies,
commit messages and release material. It adapts the maintainer's usual
human-accountability, disclosure and evidence rules to this project.

## Human responsibility

An LLM is an assisting tool, not a human author, DCO signatory or release
approver. The human contributor remains responsible for understanding the
whole contribution, checking its origin and correctness, validating it,
and explaining it during review.

Do not submit material you cannot understand or defend. Model confidence,
generated explanations and automated reviews are not substitutes for human
judgment. Crediting a tool does not transfer responsibility to it.

## Project disclosure

Development of Grand Time Auto has used **GitHub Copilot**, including
Copilot CLI, **Claude**, and **OpenAI GPT-6 Astra** ("Astra 6").
Assistance has included implementation, tests, documentation and automated
design/review work. This is a project-level disclosure, not a claim that
every tool contributed to every file.

The [CHERI design review](docs/cheri-gameplay-review.md) records its original
reviewer as **unlisted**. That historical redaction is retained; the current
tool disclosure does not identify that reviewer or constitute independent
human review.

Artwork attribution and licensing are recorded separately in
[artwork/README.md](artwork/README.md). Project code is [MIT licensed](LICENSE),
except the files adapted from lowRISC's Apache-2.0 Sonata software, which keep
that license; [third-party notices](THIRD-PARTY-NOTICES.md) list every parent
work. The two supplied splash images and generated splash data are CC BY 4.0.
AI assistance does not waive attribution, copyright or license obligations.

## Contribution disclosure

Describe meaningful LLM assistance in the change description: what it helped
with, which parts of the contribution it affected, and how the result was
checked. Include short prompts or a concise summary when useful to reviewers,
but never expose credentials, confidential information or private operational
details. If a tool identified the problem being fixed, say so.

New substantive LLM-assisted commits should include:

```text
Assisted-by: LLM
```

Trivial spelling or formatting assistance does not require that trailer.
When uncertain, disclose. Existing history is not rewritten merely to add it.

Copilot's `Co-authored-by: Copilot` and `Copilot-Session` trailers are tool
provenance, not a human DCO certification or approval. Public disclosures may
use a tool or model's public name when approved by the maintainer. A model
whose identity cannot be published is called **unlisted**, without details
that would identify it.

## Sign-off and external actions

Only a human may make a Developer Certificate of Origin certification with
their own `Signed-off-by`. Agents must not add or fabricate human sign-offs.
Preserve genuine sign-offs and license notices in imported material.

Without explicit human authorization for the particular action, agents must
not push, publish or change public repositories, create releases, open pull
requests, file issues, post comments or send email. Approval for one action
does not authorize unrelated future actions. Publication scans and passing
tests are prerequisites where applicable, not permission to publish.

Hardware loading also requires explicit authorization and the documented
identity, image and slot-safety checks. Do not bypass a guard to complete a task.

## Evidence and limits

Report failed checks, untested behavior and missing evidence plainly.
Distinguish measurements from inference, and do not treat an `OK` message or
an agent's claim as proof of the intended result.

Native tests and the web simulator emulate CHERI behavior. RTL execution is
not physical-board acceptance. A hardware claim must identify the target and
exact firmware image, use fresh observations, and state what was and was not
checked without publishing private device identifiers.

The [verification record](docs/cheri-gameplay-review.md) and
[architecture](docs/architecture.md) document these boundaries. Follow the
project's build, audit and [guarded loading](docs/board-loading.md) procedures;
do not weaken them to make a generated change appear successful.
