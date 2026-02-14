# Audio Status

PyCoreOS currently has no active runtime audio subsystem.

## Current behavior

- `make run` boots without audio device configuration.
- There are no AC97/HDA drivers wired into the current kernel build.
- `audio/bootchime.voc` is shipped as a project asset but is not played at boot.

## Why this doc changed

Older revisions experimented with VM audio backends. Those paths are not part of the current OS runtime and should be treated as removed until a new audio stack lands.
