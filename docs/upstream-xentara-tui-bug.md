# Upstream bug report: `xentara-tui` selected row overwrites the box border

Send to embedded ocean / Xentara support. Once fixed upstream, delete the
second `sed` in [`../docker/Dockerfile.xentara-example`](../docker/Dockerfile.xentara-example)
and this file.

**Component:** `/usr/local/bin/xentara-tui` in `xentara/xentara-tryout:latest`
(build 2026-07-24, Xentara 2.0.11)

**Symptom:** In the Model Tree pane, the right-hand box border is missing on
whichever row the cursor is on, so the border appears to jump or double as you
navigate with the arrow keys.

**Cause:** `drawLine()` computes the pad for a selected row one character wider
than for an unselected one, while both start at `$col + 1`:

```php
$visLen = mb_strlen(preg_replace('/\033\[[0-9;]*m/', '', $text));
$pad  = str_repeat(' ', max(0, $w - 2 - $visLen));   // selected: 1 too wide
moveTo($row, $col + 1);
if ($selected) {
    echo BG_SEL . WHITE . BOLD . " " . $text . $pad . RESET;
} else {
    echo " " . $text . str_repeat(' ', max(0, $w - 3 - $visLen)) . RESET;
}
```

A selected row therefore writes `1 + $visLen + ($w - 2 - $visLen) = $w - 1`
characters starting at `$col + 1`, ending on `$col + $w - 1` - exactly the
column where `drawBox()` drew the right border - and replaces it with a
highlighted space.

**Fix:** pad the selected row like the others.

```diff
-$pad  = str_repeat(' ', max(0, $w - 2 - $visLen));
+$pad  = str_repeat(' ', max(0, $w - 3 - $visLen));
```

**Also worth fixing in the same file** (not patched here, listed for
completeness):

- The `'RESIZE'` case in the key handler is never reachable - nothing produces
  that value, and there is no `SIGWINCH` handler. Terminal size is only
  refreshed as a side effect of `draw()`. Combined with the deliberate use of
  cursor-home instead of clear-screen for full redraws, shrinking the terminal
  leaves stale rows on screen.
- `/usr/bin/run-xentara.sh` and `/usr/local/bin/xentara-tui` ship with CRLF
  line endings in this build. The first makes the default `CMD` fail with
  `exec /usr/bin/run-xentara.sh: no such file or directory`; the second makes
  the shebang `#!/usr/bin/env php\r`.
