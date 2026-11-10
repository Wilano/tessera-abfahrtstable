# Changelog

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
