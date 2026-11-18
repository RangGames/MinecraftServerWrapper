# Third-party notices

- **Nanum Gothic Regular** — Google Fonts distribution, SIL Open Font License 1.1.
  Source: https://github.com/google/fonts/tree/main/ofl/nanumgothic
  Font: `assets/fonts/NanumGothic-Regular.ttf`
  Original copyright and license: `assets/fonts/nanumgothic-OFL.txt`.
- **Noto Sans KR variable** — Google Fonts distribution, SIL Open Font License 1.1.
  Source: https://github.com/google/fonts/tree/main/ofl/notosanskr
  Font: `assets/fonts/NotoSansKR-Variable.ttf` (original filename `NotoSansKR[wght].ttf`).
  Original copyright and license: `assets/fonts/notosanskr-OFL.txt`.
- **miniz 3.0.2** — raw DEFLATE decoder, MIT license.
  Source: https://github.com/richgel999/miniz/tree/3.0.2
  Files: `third_party/miniz/miniz_tinfl.c`, `miniz_tinfl.h`, `miniz_common.h`.
  The decoder's include was changed from `miniz.h` to `miniz_tinfl.h` to build only the low-level decoder;
  `miniz_export.h` is the local static-build export definition.
  Original license: `third_party/miniz/LICENSE`.

Keep the license files with redistributed fonts and source distributions.
