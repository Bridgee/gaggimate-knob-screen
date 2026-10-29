# Embedded web interface

The upstream Preact/Vite application is served by the display firmware.

```sh
npm ci
npm run dev
npm run build
```

For firmware, use `python scripts/k718.py build` from the repository root. It builds, compresses and validates the web assets before embedding them in the application image.
