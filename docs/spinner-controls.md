# Spinner acceleration

Holding an up/down arrow or the corresponding keyboard key starts at the normal
step. After 350 ms, the repeat step multiplier follows the integral of a half
Gaussian, smoothly increasing toward a cap of 16 steps per repeat (sigma 1.2 s).
Release, a direction-changing fresh press, focus loss, hide, disabling or wheel
input resets the hold. Ordinary clicks and wheel steps retain their existing size.
Existing range limits, locks, immediate parameter updates and undo remain active.
This covers equalizer, calibration, profile editor and Studio advanced controls.

Tests exercise the curve, keyboard and mouse-arrow hold/release, and integer/decimal
step behavior.
