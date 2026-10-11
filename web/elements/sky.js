// <x-sky>: the heliostat's sky (docs/DESIGN.md section 7.2), a variant of
// TesserUI's examples/elements/sky.js for a tree whose source and target are
// named directions. Seen from above, horizon at the edge and zenith in the
// middle: the sun, the named directions, the source and the target, and the
// mirror normal the tracker commands.
//
// It draws /heliostat (in a view: widget("x-sky", "/heliostat")):
//   sun/{azimuth, elevation}, normal/{azimuth, elevation}, state,
//   directions [{name, azimuth, elevation}], source, target, park/{...}
//
// Click a direction: it becomes the target (shift-click: the source; the sun
// is a source too). Click the sky, or use the arrow keys: the current target
// direction moves there.

const R = 100;
const rad = (d) => (d * Math.PI) / 180;
const deg = (r) => (r * 180) / Math.PI;
const SUN = 'Sun';
const esc = (s) => String(s).replace(/[<&"]/g, (c) => ({ '<': '&lt;', '&': '&amp;', '"': '&quot;' })[c]);

class Sky extends HTMLElement {
  #value = {};
  #disabled = false;

  constructor() {
    super();
    this.attachShadow({ mode: 'open' }).innerHTML = `
      <style>
        :host { display: block; max-width: 380px; margin: 0 auto; outline: none; }
        :host([disabled]) svg { cursor: default; }
        :host(:focus-visible) svg { outline: 2px solid var(--accent, #2f5bd3); border-radius: 50%; }
        svg { width: 100%; display: block; cursor: crosshair; touch-action: none; }
        .dome { fill: var(--surface-2, #eceef2); stroke: var(--border, #d9dde3); }
        .ring { fill: none; stroke: var(--border, #d9dde3); stroke-dasharray: 2 3; }
        .sun circle { fill: #f5b400; stroke: #f5b40066; stroke-width: 6; }
        .sun.down circle { fill: none; stroke: #f5b400; stroke-width: 1.5; stroke-dasharray: 2 2; }
        .dir circle { fill: var(--muted, #5d6675); }
        .dir, .sun { cursor: pointer; }
        .source circle.mark { fill: none; stroke: #f5b400; stroke-width: 2; }
        .target { fill: none; stroke: var(--accent, #2f5bd3); stroke-width: 2.5; }
        .normal { fill: var(--ok, #1f9d55); }
        .park { fill: none; stroke: var(--muted, #5d6675); stroke-width: 1.5; }
        .ray { stroke: var(--muted, #5d6675); stroke-dasharray: 3 3; stroke-width: 1; }
        text { fill: var(--muted, #5d6675); font: 9px system-ui, sans-serif; text-anchor: middle; }
        p { margin: 6px 0 0; text-align: center; color: var(--muted, #5d6675); font-size: .9em; font-variant-numeric: tabular-nums; }
        p b { color: var(--text, inherit); font-weight: 600; }
      </style>
      <svg viewBox="-112 -112 224 224" role="img">
        <circle class="dome" r="${R}" />
        <circle class="ring" r="${R / 3}" /><circle class="ring" r="${(2 * R) / 3}" />
        <text y="-102">N</text><text x="106" y="3">E</text><text y="109">S</text><text x="-106" y="3">W</text>
        <line class="ray source-ray" /><line class="ray target-ray" />
        <g class="dirs"></g>
        <g class="sun" data-name="${SUN}"><circle r="7" /></g>
        <rect class="park" x="-5" y="-5" width="10" height="10" />
        <g class="target"><circle r="9" /><path d="M-14 0h8M6 0h8M0 -14v8M0 6v8" /></g>
        <path class="normal" d="M0 -6L6 0 0 6-6 0z" />
      </svg>
      <p></p>`;
    const svg = this.shadowRoot.querySelector('svg');
    svg.addEventListener('pointerdown', (e) => this.#click(e, svg));
    this.addEventListener('keydown', (e) => this.#key(e));
  }

  connectedCallback() {
    if (!this.hasAttribute('tabindex')) this.tabIndex = 0;
  }

  set value(v) {
    this.#value = v ?? {};
    this.#draw();
  }
  get value() {
    return this.#value;
  }
  set disabled(d) {
    this.#disabled = !!d;
    this.toggleAttribute('disabled', this.#disabled);
  }
  get disabled() {
    return this.#disabled;
  }

  #directions() {
    return Array.isArray(this.#value.directions) ? this.#value.directions : [];
  }
  // A name's direction: the sun or a listed one.
  #find(name) {
    const v = this.#value;
    if (name === SUN) return v.sun ? { azimuth: v.sun.azimuth, elevation: v.sun.elevation } : null;
    return this.#directions().find((d) => d.name === name) ?? null;
  }

  #xy(az, el) {
    const r = R * (1 - Math.max(0, Math.min(90, el)) / 90);
    return [r * Math.sin(rad(az)), -r * Math.cos(rad(az))];
  }
  #place(el, az, elev) {
    const [x, y] = this.#xy(az, elev);
    el.setAttribute('transform', `translate(${x.toFixed(1)} ${y.toFixed(1)})`);
    el.style.display = '';
    return [x, y];
  }
  #ray(cls, from, to) {
    const l = this.shadowRoot.querySelector(cls);
    l.style.display = from && to ? '' : 'none';
    if (!from || !to) return;
    l.setAttribute('x1', from[0]);
    l.setAttribute('y1', from[1]);
    l.setAttribute('x2', to[0]);
    l.setAttribute('y2', to[1]);
  }

  #draw() {
    const v = this.#value;
    const root = this.shadowRoot;
    const sun = v.sun ?? { azimuth: 0, elevation: -90 };
    const sunEl = root.querySelector('.sun');
    const sunXY = this.#place(sunEl, sun.azimuth, sun.elevation);
    sunEl.classList.toggle('down', sun.elevation < 0);
    sunEl.classList.toggle('source', v.source === SUN);

    root.querySelector('.dirs').innerHTML = this.#directions()
      .map((d) => {
        const [x, y] = this.#xy(Number(d.azimuth), Number(d.elevation));
        const mark = d.name === v.source ? '<circle class="mark" r="7"/>' : '';
        return `<g class="dir${d.name === v.source ? ' source' : ''}" data-name="${esc(d.name)}" transform="translate(${x.toFixed(1)} ${y.toFixed(1)})">${mark}<circle r="4"/><text y="-8">${esc(d.name)}</text></g>`;
      })
      .join('');

    const source = this.#find(v.source);
    const target = this.#find(v.target);
    const targetEl = root.querySelector('.target');
    const targetXY = target ? this.#place(targetEl, target.azimuth, target.elevation) : null;
    if (!target) targetEl.style.display = 'none';

    const tracking = v.mode === 'track';
    const parked = v.state === 'parked';
    const n = v.normal ?? { azimuth: 0, elevation: 90 };
    const normalEl = root.querySelector('.normal');
    const normalXY = tracking ? this.#place(normalEl, n.azimuth, n.elevation) : null;
    if (!tracking) normalEl.style.display = 'none';
    const parkEl = root.querySelector('.park');
    if (v.park && parked) this.#place(parkEl, v.park.azimuth, v.park.elevation);
    else parkEl.style.display = 'none';
    const sourceXY = source ? (v.source === SUN ? sunXY : this.#xy(source.azimuth, source.elevation)) : null;
    this.#ray('.source-ray', tracking && !parked ? normalXY : null, sourceXY);
    this.#ray('.target-ray', tracking && !parked ? normalXY : null, targetXY);

    const r = (x) => Math.round(Number(x));
    const parts = [`<b>${esc(v.state ?? '')}</b>`, `sun ${r(sun.azimuth)}° / ${r(sun.elevation)}°`];
    if (target) parts.push(`${esc(v.target)} ${r(target.azimuth)}° / ${r(target.elevation)}°`);
    if (tracking) parts.push(`mirror ${r(n.azimuth)}° / ${r(n.elevation)}°`);
    root.querySelector('p').innerHTML = parts.join(' · ');
    root.querySelector('svg').setAttribute('aria-label', root.querySelector('p').textContent);
  }

  #moveTarget(az, el) {
    const name = this.#value.target;
    if (!this.#find(name) || name === SUN) return;
    const azimuth = Math.round((((az % 360) + 360) % 360) * 2) / 2;
    const elevation = Math.round(Math.max(-90, Math.min(90, el)) * 2) / 2;
    this.tesser?.set(`directions/${name}`, { azimuth, elevation });
  }

  #click(e, svg) {
    if (this.#disabled) return;
    const hit = e.target.closest?.('[data-name]');
    if (hit) {
      const name = hit.dataset.name;
      if (e.shiftKey || name === SUN) this.tesser?.set('source', name);
      else this.tesser?.set('target', name);
      return;
    }
    const box = svg.getBoundingClientRect();
    const scale = 224 / box.width;
    const x = (e.clientX - box.left) * scale - 112;
    const y = (e.clientY - box.top) * scale - 112;
    const r = Math.min(R, Math.hypot(x, y));
    this.#moveTarget(deg(Math.atan2(x, -y)), 90 * (1 - r / R));
  }

  #key(e) {
    const step = e.shiftKey ? 5 : 1;
    const d = { ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, step], ArrowDown: [0, -step] }[e.key];
    const target = this.#find(this.#value.target);
    if (!d || this.#disabled || !target) return;
    e.preventDefault();
    this.#moveTarget(Number(target.azimuth) + d[0], Number(target.elevation) + d[1]);
  }
}

customElements.define('x-sky', Sky);
