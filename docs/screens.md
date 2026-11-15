# Operator screens: the shared package

Sedona Studio and the web editor (`editor/`) read and write the same package of operator
screens: `mcs-sedona-ui`, file `mcs-ui.json`, at most 512 KiB. Either program refuses a package
that breaks the rules as a whole; what one writes the other reads back unchanged.

| Version | Adds |
|---|---|
| 1 | views bound to a folder, background SVG, widgets (12 kinds) |
| 2 | `library` of SVG pictures with states, `objects` placed on a view |
| 3 | `components` (graphic components with points inside), `parts` placed on a view |

The version is derived from the content: 3 when there are components or parts, otherwise 2 when
there is a library or objects.

## Version 3: graphic components

A **component** is a small picture of its own: symbols and widgets placed in per cent of its box,
whose values are named **points** instead of slots. A **part** is a component placed on a view;
it says which slot feeds which point. A part is selected, moved and removed as one item.

```json
{"format":"mcs-sedona-ui","version":3,"id":"plant","title":"Plant",
 "views":[{"id":"ahu","title":"AHU","folder":"app/plant/ahu1","svg":"",
   "parts":[{"id":"p1","componentId":"sl-c-fan","label":"Supply fan","x":12,"y":20,
     "bind":{"run":{"path":"fan","slot":"run","type":"bool"},
             "speed":{"path":"fan","slot":"out","type":"float"}}}]}],
 "library":[ ...the pictures the components are drawn with... ],
 "components":[{"id":"sl-c-fan","title":"Fan","width":120,"height":114,
   "points":[{"id":"run","label":"Running","kind":"bool","slots":["run","out"]},
             {"id":"fault","label":"Fault","kind":"bool","slots":["fault","alarm"]},
             {"id":"speed","label":"Speed, %","kind":"number","slots":["speed","out"]}],
   "objects":[{"id":"pic","assetId":"sl-fan","label":"","x":0,"y":0,"rotation":0,"mode":"bool",
               "binding":{"path":".","slot":"run","type":"bool"},
               "fault":{"path":".","slot":"fault","type":"bool"}}],
   "widgets":[{"id":"val","kind":"value","label":"speed","path":".","slot":"speed","type":"float",
               "x":20,"y":78,"unit":"%","decimals":0,"appearance":"value","width":80}]}]}
```

Rules, the same in `web/src/components.js`, `server/src/ComponentRules.java` and Studio's
`storage.ScreenStore`:

- `components`: at most 64; `width`/`height` 0..2000 px; at most 16 points, 16 objects, 16 widgets.
- A point: `id` (a name), `label`, `kind` (`bool`, `number` or `any`) and `slots` - up to 8 slot
  names tried, in order, when the part is attached to a block.
- `objects` and `widgets` of a component have the shape they have on a view, with `x`/`y` in per
  cent of the component's box. Inside a component a binding names a point: `path` is `"."` and
  `slot` is the point's id. Charts (`trend`, `bars`, `table`) are not allowed inside.
- `parts` of a view: at most 64; `x`/`y` in per cent of the screen (the top left corner); `bind`
  maps point ids to ordinary bindings (`path` relative to the view's folder). A `bool` point takes
  a bool slot, a `number` point a numeric one. A point without a binding shows as "no data";
  an unbound fault signal of a symbol is left out.
- To draw a part, each symbol and widget of the component is placed at the part's corner plus its
  own offset and reads the slot bound to its point.

## Libraries

A library is a file `screen-library.json`:

```json
{"format":"mcs-sedona-ui-library","version":1,"id":"sedona-hvac","title":"Sedona Line",
 "assets":[ ...pictures, as in "library"... ], "components":[ ...as above... ]}
```

An asset or a component of a library may carry `"group": "Air/Fans"` - its folder in the
palette (folders within folders). The group stays in the library and is not copied into packages.

Studio plugs libraries in as jars: any `*.jar` in its `libraries/` folder that holds this file
(data only, nothing in the jar is run). What is placed on a screen is copied into the package, so
a package never depends on a library. The HVAC library "Sedona Line" is drawn by
`sedona-studio/screen-libraries/hvac/build_library.py`; the web editor carries a copy as
`editor/web/src/hvacLibrary.json` (refresh it from there when the library changes) and can load
another `screen-library.json` from the Components panel.
