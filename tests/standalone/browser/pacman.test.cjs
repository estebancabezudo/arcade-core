'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { createHash } = require('node:crypto');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '../../..');
const bundlePath = path.join(root, 'build/standalone/browser/pacman.js');
const bundle = require(bundlePath);
const baseline = require('./original-baseline.json');
const hash = array => createHash('sha256')
  .update(Buffer.from(array.buffer, array.byteOffset, array.byteLength)).digest('hex');

test('all ten ROM regions are present and validated at load time', () => {
  assert.equal(bundle.ROM_MANIFEST.length, 10);
  assert.equal(bundle.ROM_SET.bytes.length, 25376);
  for (const entry of bundle.ROM_MANIFEST) {
    assert.equal(bundle.ROM_SET.roms[entry.name].length, entry.size);
  }
});

test('versionable source contains a placeholder, not the ROM payload', () => {
  const text = readFileSync(path.join(root, 'standalone/platforms/browser/pacman.template.js'), 'utf8');
  assert.equal(text.split('"__PACMAN_ROM_BASE64__"').length, 2);
  assert.ok(!text.includes(Buffer.from(bundle.ROM_SET.bytes).toString('base64')));
});

test('720-frame CPU, memory, video and audio match the supplied original bundle', () => {
  const machine = new bundle.Machine();
  const renderer = new bundle.Renderer(machine);
  const audio = createHash('sha256');
  const pcm = Buffer.alloc(4096);
  let samples = 0;
  for (let frame = 0; frame < 720; frame++) {
    machine.input0 = machine.input1 = 255;
    if (frame >= 360 && frame < 362) machine.input0 &= ~32;
    if (frame >= 378 && frame < 382) machine.input1 &= ~32;
    if (frame >= 450 && frame < 650) machine.input0 &= ~2;
    const count = machine.runFrame();
    for (let i = 0; i < count; i++) pcm.writeInt16LE(machine.audio[i], i * 2);
    audio.update(pcm.subarray(0, count * 2));
    samples += count;
  }
  assert.deepEqual({
    frame: machine.frame, pc: machine.cpu.pc, tStates: machine.cpu.total_t_states,
    samples, memory: hash(machine.memory), video: hash(renderer.render()),
    audio: audio.digest('hex')
  }, baseline);
});

test('corrupt embedded bytes fail CRC instead of starting the CPU', () => {
  const text = readFileSync(bundlePath, 'utf8');
  const corrupt = text.replace(/(const ROM_BASE64 =\n")[A-Za-z0-9+/]/, '$1A');
  assert.notEqual(corrupt, text);
  assert.throws(() => vm.runInNewContext(corrupt, { atob }), /ROM dañada/);
});

test('WSG writes do not change audio retroactively within the instruction', () => {
  const machine = new bundle.Machine();
  machine.rom = new Uint8Array(16384); // NOP instructions.
  machine.enabled = true;
  machine.audioCycles = 12;
  machine.decimationCount = 3;
  bundle.boardWrite(machine, 0x5055, 15);
  assert.equal(machine.sound[21], 0);
  machine.step();
  assert.equal(machine.audioCount, 1);
  assert.equal(machine.audio[0], 0);
  assert.equal(machine.sound[21], 15);
  assert.equal(machine.pendingMask, 0);
});

test('unchanged tiles reuse the same output framebuffer', () => {
  const machine = new bundle.Machine();
  const renderer = new bundle.Renderer(machine);
  const frame = renderer.render();
  const before = hash(frame);
  assert.equal(machine.dirty.some(Boolean), false);
  assert.equal(renderer.render(), frame);
  assert.equal(hash(frame), before);
});

test('machine reset restores deterministic boot state', () => {
  const machine = new bundle.Machine();
  for (let i = 0; i < 12; i++) machine.runFrame();
  machine.reset();
  const fresh = new bundle.Machine();
  assert.equal(machine.frame, 0);
  assert.equal(machine.cpu.pc, fresh.cpu.pc);
  assert.equal(machine.input0, 255);
  assert.deepEqual(machine.memory, fresh.memory);
  assert.deepEqual(machine.sound, fresh.sound);
});
