# Module 1 verification evidence

These text records were produced during the module implementation on 2026-11-01.
Build objects and caches are intentionally excluded from Git. The artifact hash index
records the original local evidence, including objects retained in the D-drive project.
Absolute paths and disk-space measurements describe that implementation session.
The implementation report records the pre-publication phase; GitHub publication occurred later.
QEMU was not run; the full kernel build failed because module 2 is incomplete.

The current published entry uses `call trap`; older snapshots and checks use `jal trap`.
See publication-checks.json and publication-*.log for the newly assembled version.
The historical verifier has one literal-jal mismatch on current code, not an assembly failure.
Raw logs and patches retain the original whitespace emitted by tools.
