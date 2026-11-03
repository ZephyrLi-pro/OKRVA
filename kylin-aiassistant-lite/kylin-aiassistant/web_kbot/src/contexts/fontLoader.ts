const loaded: Set<string> = new Set();

function loadLocalFont(
  family: string,
  filename: string,
  weights: { weight: number; style: string; file: string }[] = [
    { weight: 400, style: 'normal', file: 'Regular' },
    { weight: 500, style: 'normal', file: 'Medium' },
    { weight: 600, style: 'normal', file: 'SemiBold' },
  ],
) {
  const id = `lfont-${family.replace(/\s+/g, '-').toLowerCase()}`;
  if (loaded.has(id)) return;
  loaded.add(id);

  const FONT_BASE = '/fonts';

  let css = '';
  for (const { weight, style, file } of weights) {
    css += `
@font-face {
  font-family: '${family}';
  src: url('${FONT_BASE}/${filename}-${file}.woff2') format('woff2'),
       url('${FONT_BASE}/${filename}-${file}.woff') format('woff'),
       url('${FONT_BASE}/${filename}-${file}.ttf') format('truetype');
  font-weight: ${weight};
  font-style: ${style};
  font-display: swap;
}`;
  }

  const style = document.createElement('style');
  style.id = id;
  style.textContent = css;
  document.head.appendChild(style);
}

export function loadUiFont(font: string) {
  if (font === 'inter') loadLocalFont('Inter', 'Inter');
  if (font === 'segoe') loadLocalFont('Segoe UI', 'SegoeUI');
  if (font === 'sf') loadLocalFont('SF Pro Text', 'SFProText');
}

export function loadMonoFont(font: string) {
  if (font === 'jetbrains') loadLocalFont('JetBrains Mono', 'JetBrainsMono');
  if (font === 'fira') loadLocalFont('Fira Code', 'FiraCode');
  if (font === 'cascadia') loadLocalFont('Cascadia Code', 'CascadiaCode');
}
