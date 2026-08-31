# Expected invalid results

- `product-page-surface-settings.conf`: reject Device settings inside a Page Surface assignment.
- `surface-midi-output-collision.txt`: link both Feedback Value blocks that own MIDI output key `[0xB0, 0x20]`.
- `surface-osc-input-collision.txt`: link both Input blocks that consume `/control/value`.
- `surface-osk-alpha.txt`: reject an eight-digit OSK layout color because OSK colors are opaque RGB values.
- `surface-channels.txt`: reject a non-positive Surface channel count.
- `surface-initialize-midi.txt`: reject a SysEx initialization message without its terminal `0xF7` byte.
- `main-zone-body.zon`: reject `IncludedZones` on a zone layer, multiple button events on one binding, and the removed free Widget wildcard syntax.
- `main-layer-target.zon`: interpret as a Main zone and reject Target on `Role=Layer`.
- `fx-main-metadata.zon`: interpret as an FX zone and reject Main Role metadata.
- `LearnFX.fxzon`: reject the removed free Widget wildcard syntax in `FXWidgets`.
- `learnfx-body/LearnFX.fxzon`: require a Parameter entry and reject a modifier declaration inside GeneratedBindings.
- `semantic-wrapper.snippet`: reject the removed semantic Slot wrapper.
- `single-slash-comment.zon`: interpret as a Main zone and reject data before `@Meta`; offer the legacy-import comment conversion only in import mode.
