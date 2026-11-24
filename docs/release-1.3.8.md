# SoundCurrent EQ 1.3.8

- Remediated CodeQL integer-multiplication overflow alert in `tests/dsp_response.cpp` by widening float samples to `double` before squaring and using `std::size_t` buffer indexing.
- Updated package and installer metadata for release `1.3.8`.

## Upgrade

Use **Quit**, then install 1.3.8 over the existing application. Settings, setups, and custom profiles are retained. The public Windows package continues to use the interim VB-CABLE route; the unsigned native-driver implementation remains preserved for later production signing.
