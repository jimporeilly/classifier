# classifier_app

## Arduino firmware backups (hardware safety)

Files under `arduino/` (e.g. `mega_stepper_controller.ino`) are untracked by
git and flashed to hardware that drives physical hip steppers and linear
actuators on a hexapod. There is no git history to fall back on for them.

**Before editing any `.ino` file, copy the current version into a sibling
`old/` directory first**, timestamped:

```
cp arduino/<sketch_dir>/<name>.ino arduino/<sketch_dir>/old/<name>.$(date +%Y-%m-%dT%H%M%S).ino
```

Do this even for small edits — a bad firmware change on this rig can cause a
motor collision requiring a hard power cut, and mid-session context can be
lost before the change is reviewed. This applies to any other untracked file
that governs physical hardware behavior, not just `.ino` files.
