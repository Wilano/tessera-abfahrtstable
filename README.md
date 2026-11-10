# Departure board

A tile that shows the next departures of **one or more** lines from a stop, mixed into one board and sorted by when
they leave — every row with its own line and destination. The screen counts down on its own clock in whole minutes: the
next departure and the ones after it all read "5 min" (and "now" under a minute). You can switch the time to the clock
(24h) instead, or show both — "5 min · 14:20".

The tile does **not** fetch anything itself. It belongs to a **Home Assistant sensor** — a small template helper you add
once — that reads a departures integration, keeps the line and destination pairs you chose, merges and sorts them, and
exposes them as a few short lists the tile draws. Because the selection lives in that helper, the same plugin works for
any stop and any set of lines: you pick what to show in the helper, not in the tile.

On a single cell the tile shows the next departure as big as the cell allows, with its line and destination above it,
and the one or two after it below where there is room. On a tile of two columns or more it is a list, one row per
departure, as many as fit. It was made for the 4-inch Guition (480 × 480), but works on every board.

## Set up

### 1. Add the helper sensor to Home Assistant

The plugin reads a sensor whose attributes already hold the board as parallel lists: `ats` (departure times as seconds
since 1970), `lines`, `tos` (destinations), `delays` (minutes late), `cancelled` (1 for a cancelled run, 0 otherwise), and a
`stale` flag. A config-flow template helper cannot carry list attributes, so this is a `template:` sensor in YAML (in
`configuration.yaml`, or a package / `templates.yaml` you include). After adding it, call the **template.reload** action
(Developer Tools → Actions) or restart Home Assistant.

Copy the whole block below into `configuration.yaml` (or a package / included `templates.yaml`). **If you already have a
`template:` block, paste only the `- trigger:` list item under your existing `template:` instead of a second
`template:` key.** After adding or editing it, run the **template.reload** action (Developer Tools → Actions → *Template:
Reload*) or restart Home Assistant.

```yaml
# A departure board for Home Assistant: merges the line + destination pairs you choose into short lists the Tessera
# "Departure board" tile draws. Edit only the two lines marked "configure".  After editing: run template.reload.
template:
  - trigger:
      - platform: state
        entity_id: sensor.feuerwehr_neuweiler_sulzbach   # configure: the departures sensor to read
      - platform: time_pattern
        minutes: "/1"
    variables:
      src: sensor.feuerwehr_neuweiler_sulzbach            # configure: the same departures sensor
      # configure: the line + destination pairs to show, mixed. Add as many as you like; "linie" and "ziel" must match
      # the source sensor's values exactly. One pair behaves like a single-line board.
      combos: >
        {{ [
             {'linie': '160', 'ziel': 'St. Ingbert Rendezvous Platz'}
           ] }}
      # --- below here nothing needs changing ---
      items: >
        {% set deps = state_attr(src, 'abfahrten') or [] %}
        {% set tnow = as_timestamp(now()) | int %}
        {% set ns = namespace(items=[]) %}
        {% for d in deps %}
          {% set at = as_timestamp(d.abfahrt) | int(0) %}
          {% if at >= tnow %}
            {% for c in combos %}
              {% if d.linie == c.linie and d.ziel == c.ziel %}
                {% set ns.items = ns.items + [{'at': at, 'line': d.linie, 'to': d.ziel,
                                               'delay': d.verspaetung_minuten | int(0),
                                               'cancelled': 1 if d.ausfall | default(false) else 0}] %}
              {% endif %}
            {% endfor %}
          {% endif %}
        {% endfor %}
        {{ (ns.items | sort(attribute='at'))[:8] }}
    sensor:
      - name: "Abfahrten Tafel"
        unique_id: abfahrten_tafel
        icon: mdi:bus-clock
        state: >
          {% set tnow = as_timestamp(now()) | int %}
          {% set ns = namespace(parts=[]) %}
          {% for it in items[:3] %}
            {% set m = [((it.at - tnow) / 60) | int, 0] | max %}
            {% set ns.parts = ns.parts + [it.line ~ ' ' ~ m ~ ' min'] %}
          {% endfor %}
          {{ ns.parts | join(' | ') if ns.parts else 'keine Abfahrt' }}
        attributes:
          ats: "{{ items | map(attribute='at') | list }}"
          lines: "{{ items | map(attribute='line') | list }}"
          tos: "{{ items | map(attribute='to') | list }}"
          delays: "{{ items | map(attribute='delay') | list }}"
          cancelled: "{{ items | map(attribute='cancelled') | list }}"
          count: "{{ items | count }}"
          stale: "{{ states(src) in ['unavailable', 'unknown'] }}"
```

### 2. Add the plugin and place the tile

1. Add the plugin to a screen in Tessera (Plugins, or the Plugins tab of the screen). The screen builds once.
2. In Layout, place the **Departure board** tile from the library's Plugins group.
3. In the inspector, choose your **Abfahrten Tafel** sensor (only sensors that carry the board's lists are offered).
4. Options: **Show the sensor's name** as a title, **Show delays** (`+2` behind a destination that runs late),
   **Time format** (minutes only — the default — the time of day in 24h, or both: `5 min · 14:20`), and **Rows at
   most** to cap a large tile (0 fills it).

## Good to know

- The screen counts down on its own clock, so the helper needs to update only when Home Assistant gets new times, not
  every minute. The `time_pattern` trigger just keeps past departures from lingering and the state string fresh.
- Many departure integrations cap their list (the DELFI `haltestellen_abfahrtsanzeige` keeps **12** entries, all lines
  mixed). A single hourly line then shows **about two** matches within that window, however large the tile — the board
  is *up to* N rows and never invents one. To fill a large board, widen the source's cap first, or add more lines.
- A departure that runs a minute or more late gets `+2` behind where it goes and its line badge turns amber; turn
  **Show delays** off to leave the `+2` out.
- A cancelled departure keeps its row with its line and destination struck through and reads **Ausfall** where its
  countdown would be, so it is not silently dropped. This needs the `cancelled` list in the helper above (the source
  sensor's `ausfall` flag); without it nothing is marked.
- When the source is away the tile keeps the last times it had and says they may be old.

## How it works

- `tessera-plugin.yaml`: the tile, placed on a sensor (`domains: [sensor]`, `has_attributes: [ats]`), and the lists it
  reads from the sensor's attributes. Lists of texts and numbers come whole, which is why the helper exposes parallel
  lists rather than a list of objects.
- `components/abfahrtstable/`: the ESPHome component that draws the board and counts down on the screen's clock.
- `translations/`: the words on the screen (`screen`) and in the app (`app`), English and German.
