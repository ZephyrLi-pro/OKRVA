import cascadeLayers from '@csstools/postcss-cascade-layers';
import postcssPresetEnv from 'postcss-preset-env';

/** Embedded WebView: qrc/Qt 5.12 ≈ Chromium 69; gateway/Tauri builds may target newer. */
const browsers =
  process.env.KYLINBOT_GATEWAY === '1'
    ? 'chrome >= 87, safari >= 14, firefox >= 78'
    : 'chrome >= 69, safari >= 13, firefox >= 78';

export default {
  plugins: [
    cascadeLayers(),
    postcssPresetEnv({
      stage: 3,
      browsers,
      features: {
        'cascade-layers': false,
      },
    }),
  ],
};
