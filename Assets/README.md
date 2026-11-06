# RESORA visual assets

Source of truth for every graphic in the app. Compiled into the executable through `resora.qrc`
(Qt resources, `:/icons/…`), so nothing is read from the filesystem at runtime.

| Folder | Contents | Format |
| --- | --- | --- |
| `icons/` | Interface icon family, 24 px grid, 1.5 stroke, mitred joins, one chamfered corner, one ember "pixel" accent | SVG |
| `branding/` | `mark.svg` (single source of the mark geometry), `mark-mono.svg` | SVG |
| `illustrations/` | Empty-state artwork | SVG |
| `app/` | `resora.ico` (Windows exe/taskbar icon), `resora.rc` (icon + version info), `generate_icon.py` | ICO / RC |

Colour tokens: icons use only paper `#EFE9DD` (strokes) and ember `#FF5B2E` (accent). `ui/icons.cpp` swaps them
at render time (hover, disabled, on-primary), so never add other colours to an icon.
Adding an icon: drop the SVG in `icons/`, add one `<file>` line to `resora.qrc`. `tst_icons` checks every
compiled icon renders and only uses the two tokens. After changing `branding/mark.svg` run
`python Assets/app/generate_icon.py` to refresh `resora.ico`.
