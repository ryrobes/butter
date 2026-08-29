# Butter

Butter is an Omarchy-native explanation layer for storage. It turns Btrfs,
Snapper, Limine, home-folder use, and Docker storage into a small set of
plain-language answers. Observation is read-only; recovery remedies and a
user-confirmed Home Shadow are explicit, narrowly bounded exceptions.

## Install on Omarchy or Arch

Until the AUR package is available, install the current public release from
source:

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative rsync btrfs-progs polkit
git clone https://github.com/ryrobes/butter.git
cd butter
./install.sh
```

The script builds as your normal user, then asks once for administrator access
to install the app and its narrowly scoped recovery helpers under `/usr/local`.
It also installs Butter's app-launcher entry, scalable icon, and Polkit actions.
Open **Butter** from the Omarchy launcher or run `butter` in a terminal.

To update later:

```bash
cd butter
git pull --ff-only
./install.sh
```

The installer does not alter Butter's Home Shadow configuration, completed
generations, scan history, or free-space history.

## Build without installing

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/butter
```

The app follows the active Omarchy palette from
`~/.local/state/omarchy/current/theme/colors.toml` and reloads when the theme
directory is replaced.

## v0 scope

- Read-only Btrfs capacity and allocation pressure
- A subtle free-space history gathered across app launches and Shadow runs
- Persistent device-error counters
- Omarchy Snapper retention and timeline policy
- Limine recovery entries visible at startup
- Snapper cleanup and Limine sync service state
- One-prompt, read-only comparison of Btrfs subvolumes and Snapper records
- On-demand space estimates for unmanaged recovery copies only
- An unprivileged, home-only treemap with ranked paths and exact locations
- Detection of large regenerable project output and dependency/cache trees
- Read-only Docker storage accounting with volumes kept in a protected category
- Home Shadow: scheduled, immutable Home generations on a second physical drive

The home scan does not ask for elevation, follow symlinks, cross into mounted
filesystems, or inspect the rest of the OS. It runs with reduced CPU priority
and idle-class disk I/O, streams partial totals, and can be stopped without
discarding the results gathered so far. Butter keeps a bounded drill-down model
rather than retaining a record for every file it sees.

The Right Now card keeps a small local free-space ledger and draws the actual
free fraction as a quiet background horizon. Butter records once per app
process and once when Home Shadow is invoked, coalesces observations within 15
minutes, and retains up to 800 points from roughly the last year. The ledger
contains timestamps and capacity numbers only—no filenames or directory data.

When an Omarchy default agent is configured and its supported headless interface
is available, Butter can ask it for the two lines of prose in **Right now**.
Butter sends a compact, path-free JSON briefing with rounded capacity,
recovery, Home Shadow, project-output, and Docker facts. The deterministic
severity, measurements, chips, and actions remain authoritative and render
immediately. Agent output is schema-constrained, length-limited, cached by the
fact set, and never interpreted as a command. A timeout, malformed response,
missing login, unsupported agent, or unavailable network silently leaves the
deterministic synopsis in place. This first adapter supports Codex when it is
the selected Omarchy default; other default agents currently take the fallback
path.

Known regenerable project artifacts can be removed only from their evidence
modal. Butter re-detects the artifact at the exact reviewed path inside Home,
refuses review-only types such as Python environments, invokes no shell or
administrator access, and constrains removal to one filesystem. Docker storage
remains explanatory only, and volumes are always presented as protected data.

## Home Shadow

Home Shadow is intentionally not an rsync mirror. After the user chooses and
confirms a writable folder on another physical drive, Butter records the
filesystem UUID and machine identity, creates a user-level systemd timer, and
runs the unprivileged `butter-shadow` helper daily when the drive is present.
The helper refuses the Home disk, another partition on the same backing disk,
Windows-style filesystems that cannot preserve Linux metadata, changed drive
identity, and an empty mount path left behind by a disconnected drive.

Every successful run is assembled in a fresh incomplete directory and promoted
to a timestamped generation only after rsync completes. Later generations use
hard links for unchanged files. Source deletions are therefore absent from the
new generation but remain available in every earlier generation that contained
them. Butter never applies ordinary source deletion to a resumed tree, never
modifies a completed generation, and never prunes one automatically. If Home
changes during a long copy or the destination fills, the last successful
generation remains current and the app reports what happened. A failed first
copy remains in the incomplete area; **Try again** resumes that same tree and
removes newly excluded cache paths from the unfinished copy before promoting
it after a clean run. Ordinary files that vanished from Home stay in that
unfinished copy. Butter keeps a bounded rsync error summary so a partial
transfer does not silently fall back to “ready.”

Source folders or files that explicitly return “permission denied” are recorded
as skipped and do not prevent promotion. Files that vanish while a live app is
changing them are recorded separately and are also safe to skip—even when both
conditions occur in the same run. Receiver-side, destination, I/O, and metadata
failures still stop promotion. Safe source-side skips do not replace the normal
“current” status; their exact evidence remains available in the expandable run
details.

The initial exclusions combine recursive, universally disposable caches with
exact regenerable artifact paths found by the Home scan, including small
manifest-backed dependency and build trees. Cargo `target-*` directories count
only beside a Cargo manifest. Python environments are omitted from the Shadow
only when a nearby dependency manifest proves they can be recreated; they stay
review-only in Butter's cleanup UI. Ambiguous `dist`, `out`, generic build
folders, and unmanifested environments remain included. Completed scans refresh
the filter list for future runs. Recreating a cleared Shadow on the same
identified drive retains the previous reviewed filters, so a reset does not
silently restore hundreds of gigabytes of known build weight. The destination
is not encrypted by Butter; its privacy follows the selected drive.

The deeper recovery check runs `butter-probe` once through Polkit. The helper
accepts no filesystem paths or arbitrary commands, performs a fixed read-only
inventory, measures only mismatched copies, and exits. Butter does not keep a privileged service
running and never removes a snapshot. Packaged installs include a narrow Polkit
action with an explanatory prompt; the authorization is retained briefly by
Polkit rather than by Butter.

When an unmanaged copy has enough independent evidence, the review drawer can
offer a guarded removal through the separately authorized `butter-remedy`
helper. It accepts only the snapshot number and exact
subvolume ID that Butter reviewed, then re-checks Snapper ownership, Limine
visibility, read-only state, mount/current-root identity, nested subvolumes, and
metadata immediately before deletion. Any discrepancy stops the operation. If
those checks cannot prove safety, Butter offers a copyable evidence report for
an Omarchy agent instead.

The backend has a provider boundary so future ZFS support can keep its native
topology and vocabulary rather than being squeezed into Btrfs concepts.

## License

Butter is available under the [MIT License](LICENSE).
