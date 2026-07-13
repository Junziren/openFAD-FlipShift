# Acceptance checks

1. Insert the device on a stereo audio track and confirm silence remains silent.
2. Feed a 440 Hz sine into Classic mode. At +100 Hz, verify the dominant output is near 540 Hz; at -100 Hz, verify it is near 340 Hz.
3. Change engines during sustained audio and confirm the 35 ms crossfade does not click.
4. In Spectral mode, test Scale at 0.5x, 1x and 2x using a sine and a chord.
5. Sweep Translate across zero and confirm there is no parameter discontinuity.
6. Exercise all eight filter modes with white noise and verify Amount and Mix remain bounded.
7. Automate Shift, Scale, Pivot and Filter Amount at audio-session tempo.
8. Test at 44.1, 48, 88.2 and 96 kHz with Low, Normal and High quality settings.
9. Toggle Compact/Expanded visualization and verify audio output is unchanged.
10. Reload the Live set and confirm all automated parameters and preset values restore.
