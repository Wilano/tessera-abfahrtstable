# Changelog

## 1.2.0 - 2026-11-10
- Delays now stand out: a departure that runs late keeps its "+2" and its line badge turns amber, so a late one catches
  the eye on the board.
- Cancellations show instead of vanishing: a cancelled departure keeps its row with its line and destination struck
  through and reads "Cancelled" (German "Ausfall") where its countdown would be.
- The helper sensor gains a `cancelled` list (1 for a cancelled run, 0 otherwise) and no longer drops cancelled runs. Update
  your template helper from the README (one changed line and one new attribute) and run *Template: Reload*.

## 1.1.1 - 2026-11-10
- Fix a build regression that kept the firmware from compiling (the plugin went missing on the screen): a row-cap
  `std::min(max_rows_, MAX_DEPARTURES)` mixed an `int` with a `size_t`, which the compiler cannot match. No behaviour
  change — the cap is the same.

## 1.1.0 - 2026-11-10
- The countdown now reads in whole minutes everywhere, never seconds: the next departure shows "5 min" (and "now" under
  a minute) like the ones after it, instead of a ticking "4:32".
- Every row after the first shows minutes too, not a time of day, so a board of far-off departures stays a countdown.
- New inspector option "Time format": minutes only (default), the time of day (24h), or both ("5 min · 14:20").

## 1.0.1 - 2026-11-10
- Leaner on the ESP32-S3: the board shows at most the eight departures the helper sends, so it never builds more rows
  than that; the per-second countdown allocates nothing; the declared layout memory and flash are honest.
- Reads the helper's lists the way the firmware hands an entity its attributes (no change for the viewer).

## 1.0.0 - 2026-11-10
- First version: a board of the next departures of one or more lines from a Home Assistant sensor.
- Several line and destination pairs are mixed and sorted; every row shows its own line and destination.
- On a single cell the next departure is as large as the cell allows; on a wider tile it is a list, as many rows as fit.
- The screen counts down on its own clock: the next departure to the second, the ones after it in whole minutes.
- In the inspector: show the sensor's name as a title, show delays, and a cap on the number of rows.
