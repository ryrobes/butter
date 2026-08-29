# Butter

Butter is an Omarchy-native explanation layer for storage. It turns Btrfs,
Snapper, Limine, home-folder use, and Docker storage into a small set of
plain-language answers. Observation is read-only; recovery remedies and a
user-confirmed Home Shadow are explicit, narrowly bounded exceptions.

## Build and run

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
them. Butter never passes rsync `--delete`, never modifies a completed
generation, and never prunes one automatically. If Home changes during a long
copy or the destination fills, the last successful generation remains current
and the app reports what happened. A failed first copy remains in the
incomplete area; **Try again** resumes that same tree and promotes it only after
a clean run. Butter keeps a bounded rsync error summary so a partial transfer
does not silently fall back to “ready.”

The initial exclusions combine a conservative cache baseline with exact
regenerable artifact paths found by the Home scan. Completed scans refresh that
filter list for future runs. Python environments remain review-only. The
destination is not encrypted by Butter; its privacy follows the selected
drive.

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
