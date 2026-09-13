# Touch Deck UI v2

Target: M5Stack Tab5, 1280 x 720 landscape. The implementation follows
`home-concept-v1.png`, adjusted for the icon set and redraw limits available in
the embedded LVGL build.

## Design system

- Canvas `#070A12`; card `#151E30`; utility card `#0B1220`; border `#243044`.
- Primary text `#F1F5F9`; secondary text `#94A3B8`.
- Accents: cyan `#22D3EE`, violet `#A78BFA`, amber `#F59E0B`, green
  `#22C55E`, blue `#3B82F6`, rose `#F43F5E`.
- Montserrat 36 for page and primary values, 28 for primary actions, 24 for
  icons, and 20 for compact labels, hints, status, and navigation.
- Flat rendering only: no gradients, shadows, glow, or whole-card animation.
  The pressed state swaps to an accent fill for immediate touch feedback.
- Icons use LVGL's built-in symbol font. Every icon is paired with text.

## Layout

- 12-column by 4-row content grid with a 12 px gutter.
- Compact header: page title left, explicit USB and agent state right.
- Persistent feedback line directly above navigation.
- Four labelled navigation targets: Home, Work, Media, System.
- Home is asymmetric: two large primary actions, compact edit actions,
  utility rows, and a dedicated telemetry rail.
- Touch targets are at least one 4-row grid cell high (about 125 px), well
  above the recommended minimum.

## Configuration schema v2

Pages accept `icon` and an optional page-level `grid`. Widgets accept:

```json
{
  "label": "Copy",
  "icon": "copy",
  "hint": "Ctrl + C",
  "variant": "compact",
  "color": "cyan",
  "position": { "col": 0, "row": 2, "col_span": 2, "row_span": 1 },
  "action": { "type": "hid_keys", "keys": ["PRIMARY", "C"] }
}
```

`variant` may be empty/default, `compact`, or `utility`. Positions are bounded
to 12 columns and 6 rows. Files without positions retain the original automatic
row-major layout, so version 1 configurations remain usable.

## Deliberate concept corrections

- Lock is shown as `Win + L`, matching the actual HID chord. The generated
  concept's `Ctrl + L` caption was visually useful but functionally incorrect.
- The device has no reliable wall clock before a host time-sync protocol is
  added, so the header does not display a fabricated time.
- Raster icons were replaced by built-in monochrome LVGL symbols because PNG
  and SVG decoders are disabled in the firmware; this also reduces redraw cost.
