# Audio drivers (QEMU and VirtualBox)

PyCoreOS supports three audio backends. The kernel probes in order and uses the first that is present.

## QEMU

- **AC97** (default): `-device AC97,audiodev=aud0`  
  The default `make run` uses this. Works with the built-in Intel ICH AC97 driver.

- **Intel HDA**: `-device intel-hda,audiodev=aud0`  
  To use Intel HD Audio in QEMU instead of AC97, run QEMU with `intel-hda` and the same `-audiodev`; the HDA driver will take over.

Example with Intel HDA:
```bash
qemu-system-i386 -cdrom build/pycoreos.iso -m 1024M -vga std \
  -audiodev alsa,id=aud0 -machine pcspk-audiodev=aud0 \
  -device intel-hda,audiodev=aud0
```

## VirtualBox

- **ICH AC97**: In VM **Settings → Audio**, set **Host Driver** to your host (e.g. ALSA/Pulse) and **Controller** to **ICH AC97**. The guest uses the same Intel AC97 driver as QEMU.

- **Intel HD Audio** (default in many templates): Set **Controller** to **Intel HD Audio**. The guest uses the Intel HDA driver and should work without extra steps.

If you do not hear sound, ensure **Audio** is enabled and the controller matches one of the two above.
