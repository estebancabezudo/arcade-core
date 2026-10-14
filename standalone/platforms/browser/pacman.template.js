/*!
 * Pac-Man Web | arcade-core | 2026-09-29
 * Single-file, standalone JavaScript: original ROM set + Z80 + video + Namco WSG.
 * No WebAssembly, libraries or network services; strict-CSP embeds use generated CSS.
 *
 * Architecture and design
 * The browser runtime owns emulation and Shadow DOM composition. Consumers that
 * require a strict CSP pass stylesheetHref, which mounts an external stylesheet
 * with a Shadow DOM <link>; the legacy embedded stylesheet remains the default for
 * existing standalone consumers. Page-level loading and error UI stay outside the
 * runtime so a failed bundle cannot prevent the host document from rendering.
 *
 * Build: make pacman-web. Output: build/standalone/browser/pacman.js.
 * Usage: <script src="pacman.js"></script> in a UTF-8 HTML page.
 * screenOnly: true renders just the arcade canvas; click or Enter starts a game.
 * Without screenOnly, the original complete control panel remains available.
 * Teclas: flechas/WASD; 5/C/Espacio moneda; 1/2 iniciar; P pausa; M sonido; F fullscreen.
 * Opcional antes de cargar: window.PacmanOptions={container:'#juego',volume:0.65};
 * CSP estricta: agrega stylesheetHref:'./pacman-screen.css' para screenOnly.
 * Montaje manual: PacmanOptions={autoMount:false}; luego PacmanWeb.mount(elemento).
 * API: PacmanWeb.play(), .pause(), .resume(), .reset(), .insertCoin(), .getStats(), .destroy().
 *
 * CPU: transliteracion del Z80 C proporcionado, con conversiones enteras explicitas.
 * Video: graficos/paletas precalculados, cache de tiles modificados y framebuffer reutilizado.
 * Timing: 3,072 MHz / 50.688 T-states por frame; WSG 192 kHz -> PCM16 48 kHz (4:1).
 * Audio: conserva el orden de escrituras WSG diferidas despues de cada instruccion.
 * AudioWorklet integrado mediante Blob; fallback con Web Audio si no esta disponible.
 *
 * Build-time ROM input: common/roms/pacman/. This template has no ROM payload.
 * No se descargan ROMs. Se conservan los derechos que correspondan a sus titulares.
 * El CPU y la semantica de la placa se basan en los fuentes del proyecto suministrado.
 * Este archivo no afirma un optimo universal de rendimiento en todos los navegadores.
 */
(function () {
'use strict';
if (typeof document !== 'undefined' && globalThis.PacmanWeb) return;
const ROM_MANIFEST = [{"name": "pacman.6e", "offset": 0, "size": 4096, "crc32": 3253119760}, {"name": "pacman.6f", "offset": 4096, "size": 4096, "crc32": 443527892}, {"name": "pacman.6h", "offset": 8192, "size": 4096, "crc32": 3168607211}, {"name": "pacman.6j", "offset": 12288, "size": 4096, "crc32": 2172490979}, {"name": "pacman.5e", "offset": 16384, "size": 4096, "crc32": 211044708}, {"name": "pacman.5f", "offset": 20480, "size": 4096, "crc32": 2509237753}, {"name": "82s123.7f", "offset": 24576, "size": 32, "crc32": 801525949}, {"name": "82s126.4a", "offset": 24608, "size": 256, "crc32": 1051961572}, {"name": "82s126.1m", "offset": 24864, "size": 256, "crc32": 2848753343}, {"name": "82s126.3m", "offset": 25120, "size": 256, "crc32": 1998871398}];
const ROM_BASE64 =
"__PACMAN_ROM_BASE64__";

const Z80_FLAG_C = 0x01;
const Z80_FLAG_N = 0x02;
const Z80_FLAG_PV = 0x04;
const Z80_FLAG_X = 0x08;
const Z80_FLAG_H = 0x10;
const Z80_FLAG_Y = 0x20;
const Z80_FLAG_Z = 0x40;
const Z80_FLAG_S = 0x80;
function memory_read(cpu, address) {
  if ((cpu.bus.memory_read === 0)) {
    return ((0xff) & 255);
  }
  return ((cpu.bus.memory_read(cpu.bus.context, address)) & 255);
}

function memory_write(cpu, address, value) {
  if ((cpu.bus.memory_write === 0)) {
    return;
  }
  cpu.bus.memory_write(cpu.bus.context, address, value);
}

function io_read(cpu, port) {
  if ((cpu.bus.io_read === 0)) {
    return ((0xff) & 255);
  }
  return ((cpu.bus.io_read(cpu.bus.context, port)) & 255);
}

function io_write(cpu, port, value) {
  if ((cpu.bus.io_write === 0)) {
    return;
  }
  cpu.bus.io_write(cpu.bus.context, port, value);
}

function interrupt_acknowledge(cpu) {
  if ((cpu.bus.interrupt_acknowledge === 0)) {
    return ((0xff) & 255);
  }
  return ((cpu.bus.interrupt_acknowledge(cpu.bus.context)) & 255);
}

function get_hl(cpu) {
  return ((((((((cpu.h) & 65535) << 8) | cpu.l)) & 65535)) & 65535);
}

function get_register_pair(cpu, pair) {
  switch ((pair & 3)) {
    case 0:
      return ((((((((cpu.b) & 65535) << 8) | cpu.c)) & 65535)) & 65535);
    case 1:
      return ((((((((cpu.d) & 65535) << 8) | cpu.e)) & 65535)) & 65535);
    case 2:
      return ((get_hl(cpu)) & 65535);
    case 3:
      return ((cpu.sp) & 65535);
    default:
      return ((0) & 65535);
  }
}

function set_register_pair(cpu, pair, value) {
  let high = (((((value >> 8)) & 255)) & 255);
  let low = ((((value) & 255)) & 255);
  switch ((pair & 3)) {
    case 0:
      cpu.b = ((high) & 255);
      cpu.c = ((low) & 255);
      break;
    case 1:
      cpu.d = ((high) & 255);
      cpu.e = ((low) & 255);
      break;
    case 2:
      cpu.h = ((high) & 255);
      cpu.l = ((low) & 255);
      break;
    case 3:
      cpu.sp = ((value) & 65535);
      break;
    default:
      break;
  }
}

function get_stack_pair(cpu, pair) {
  switch ((pair & 3)) {
    case 0:
      return ((((((((cpu.b) & 65535) << 8) | cpu.c)) & 65535)) & 65535);
    case 1:
      return ((((((((cpu.d) & 65535) << 8) | cpu.e)) & 65535)) & 65535);
    case 2:
      return ((((((((cpu.h) & 65535) << 8) | cpu.l)) & 65535)) & 65535);
    case 3:
      return ((((((((cpu.a) & 65535) << 8) | cpu.f)) & 65535)) & 65535);
    default:
      return ((0) & 65535);
  }
}

function set_stack_pair(cpu, pair, value) {
  let high = (((((value >> 8)) & 255)) & 255);
  let low = ((((value) & 255)) & 255);
  switch ((pair & 3)) {
    case 0:
      cpu.b = ((high) & 255);
      cpu.c = ((low) & 255);
      break;
    case 1:
      cpu.d = ((high) & 255);
      cpu.e = ((low) & 255);
      break;
    case 2:
      cpu.h = ((high) & 255);
      cpu.l = ((low) & 255);
      break;
    case 3:
      cpu.a = ((high) & 255);
      cpu.f = ((low) & 255);
      break;
    default:
      break;
  }
}

function increment_r(cpu) {
  cpu.r = ((((((cpu.r & 0x80) | ((((cpu.r + 1)) >>> 0) & 0x7f))) & 255)) & 255);
}

function decrement_r(cpu) {
  cpu.r = ((((((cpu.r & 0x80) | ((((cpu.r - 1)) >>> 0) & 0x7f))) & 255)) & 255);
}

function fetch_byte(cpu) {
  let value = ((memory_read(cpu, cpu.pc)) & 255);
  (cpu.pc = ((cpu.pc + 1) & 65535));
  return ((value) & 255);
}

function fetch_word(cpu) {
  let low = ((fetch_byte(cpu)) & 255);
  let high = ((fetch_byte(cpu)) & 255);
  return ((((((((high) & 65535) << 8) | low)) & 65535)) & 65535);
}

function push_word(cpu, value) {
  (cpu.sp = ((cpu.sp - 1) & 65535));
  memory_write(cpu, cpu.sp, (((value >> 8)) & 255));
  (cpu.sp = ((cpu.sp - 1) & 65535));
  memory_write(cpu, cpu.sp, ((value) & 255));
}

function pop_word(cpu) {
  let low = ((memory_read(cpu, cpu.sp)) & 255);
  (cpu.sp = ((cpu.sp + 1) & 65535));
  let high = ((memory_read(cpu, cpu.sp)) & 255);
  (cpu.sp = ((cpu.sp + 1) & 65535));
  return ((((((((high) & 65535) << 8) | low)) & 65535)) & 65535);
}

function read_register_code(cpu, code) {
  switch ((code & 7)) {
    case 0:
      return ((cpu.b) & 255);
    case 1:
      return ((cpu.c) & 255);
    case 2:
      return ((cpu.d) & 255);
    case 3:
      return ((cpu.e) & 255);
    case 4:
      return ((cpu.h) & 255);
    case 5:
      return ((cpu.l) & 255);
    case 6:
      return ((memory_read(cpu, get_hl(cpu))) & 255);
    case 7:
      return ((cpu.a) & 255);
    default:
      return ((0xff) & 255);
  }
}

function write_register_code(cpu, code, value) {
  switch ((code & 7)) {
    case 0:
      cpu.b = ((value) & 255);
      break;
    case 1:
      cpu.c = ((value) & 255);
      break;
    case 2:
      cpu.d = ((value) & 255);
      break;
    case 3:
      cpu.e = ((value) & 255);
      break;
    case 4:
      cpu.h = ((value) & 255);
      break;
    case 5:
      cpu.l = ((value) & 255);
      break;
    case 6:
      memory_write(cpu, get_hl(cpu), value);
      break;
    case 7:
      cpu.a = ((value) & 255);
      break;
    default:
      break;
  }
}

function rotate_accumulator_left_circular(cpu) {
  let old_a = ((cpu.a) & 255);
  let carry = (((((old_a >> 7)) & 255)) & 255);
  cpu.a = ((((((old_a << 1) | carry)) & 255)) & 255);
  cpu.f = (((((((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV)) | (cpu.a & (Z80_FLAG_X | Z80_FLAG_Y))) | (carry ? Z80_FLAG_C : 0))) & 255)) & 255);
  cpu.q = ((cpu.f) & 255);
}

function rotate_accumulator_right_circular(cpu) {
  let old_a = ((cpu.a) & 255);
  let carry = (((old_a & 1)) & 255);
  cpu.a = ((((((old_a >> 1) | (carry << 7))) & 255)) & 255);
  cpu.f = (((((((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV)) | (cpu.a & (Z80_FLAG_X | Z80_FLAG_Y))) | (carry ? Z80_FLAG_C : 0))) & 255)) & 255);
  cpu.q = ((cpu.f) & 255);
}

function rotate_accumulator_left(cpu) {
  let old_a = ((cpu.a) & 255);
  let old_carry = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 255);
  let new_carry = (((((old_a >> 7)) & 255)) & 255);
  cpu.a = ((((((old_a << 1) | old_carry)) & 255)) & 255);
  cpu.f = (((((((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV)) | (cpu.a & (Z80_FLAG_X | Z80_FLAG_Y))) | (new_carry ? Z80_FLAG_C : 0))) & 255)) & 255);
  cpu.q = ((cpu.f) & 255);
}

function rotate_accumulator_right(cpu) {
  let old_a = ((cpu.a) & 255);
  let old_carry = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 255);
  let new_carry = (((old_a & 1)) & 255);
  cpu.a = ((((((old_a >> 1) | (old_carry << 7))) & 255)) & 255);
  cpu.f = (((((((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV)) | (cpu.a & (Z80_FLAG_X | Z80_FLAG_Y))) | (new_carry ? Z80_FLAG_C : 0))) & 255)) & 255);
  cpu.q = ((cpu.f) & 255);
}

function decimal_adjust_accumulator(cpu) {
  let old_a = ((cpu.a) & 255);
  let subtract = !!(((cpu.f & Z80_FLAG_N) !== 0));
  let old_half = !!(((cpu.f & Z80_FLAG_H) !== 0));
  let carry = !!(((cpu.f & Z80_FLAG_C) !== 0));
  let correction = ((0) & 255);
  if ((old_half || ((old_a & 0x0f) > 9))) {
    correction = (((correction | 0x06)) & 255);
  }
  if ((carry || (old_a > 0x99))) {
    correction = (((correction | 0x60)) & 255);
    carry = !!(true);
  }
  let result = 0;
  let half = 0;
  if (subtract) {
    half = !!((old_half && ((old_a & 0x0f) < 0x06)));
    result = (((((old_a - correction)) & 255)) & 255);
  }
  else {
    half = !!(((old_a & 0x0f) > 9));
    result = (((((old_a + correction)) & 255)) & 255);
  }
  let flags = ((result_szxy_flags(result)) & 255);
  if (parity_even(result)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  if (half) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if (subtract) {
    flags = (((flags | Z80_FLAG_N)) & 255);
  }
  if (carry) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  cpu.a = ((result) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function condition_true(cpu, condition) {
  switch ((condition & 7)) {
    case 0:
      return !!(((cpu.f & Z80_FLAG_Z) === 0));
    case 1:
      return !!(((cpu.f & Z80_FLAG_Z) !== 0));
    case 2:
      return !!(((cpu.f & Z80_FLAG_C) === 0));
    case 3:
      return !!(((cpu.f & Z80_FLAG_C) !== 0));
    case 4:
      return !!(((cpu.f & Z80_FLAG_PV) === 0));
    case 5:
      return !!(((cpu.f & Z80_FLAG_PV) !== 0));
    case 6:
      return !!(((cpu.f & Z80_FLAG_S) === 0));
    case 7:
      return !!(((cpu.f & Z80_FLAG_S) !== 0));
    default:
      return !!(false);
  }
}

function alu_add_hl(cpu, value) {
  let hl = ((get_hl(cpu)) & 65535);
  let wide_result = (((((((hl) >>> 0) + ((value) >>> 0))) >>> 0)) >>> 0);
  let result = ((((wide_result) & 65535)) & 65535);
  let flags = (((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV))) & 255);
  flags = (((flags | ((((result >> 8) & (Z80_FLAG_X | Z80_FLAG_Y))) & 255))) & 255);
  if ((((((hl & 0x0fff) + (value & 0x0fff))) >>> 0) > 0x0fff)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if ((wide_result > 0xffff)) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  cpu.h = (((((result >> 8)) & 255)) & 255);
  cpu.l = ((((result) & 255)) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
  cpu.wz = (((((((hl + 1)) >>> 0)) & 65535)) & 65535);
}

function alu_inc(cpu, value) {
  let result = (((((((value + 1)) >>> 0)) & 255)) & 255);
  let flags = ((((cpu.f & Z80_FLAG_C) | result_szxy_flags(result))) & 255);
  if (((value & 0x0f) === 0x0f)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if ((value === 0x7f)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
  return ((result) & 255);
}

function alu_dec(cpu, value) {
  let result = (((((((value - 1)) >>> 0)) & 255)) & 255);
  let flags = (((((cpu.f & Z80_FLAG_C) | result_szxy_flags(result)) | Z80_FLAG_N)) & 255);
  if (((value & 0x0f) === 0x00)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if ((value === 0x80)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
  return ((result) & 255);
}

function alu_add(cpu, value, carry) {
  let a = ((cpu.a) & 255);
  let wide_result = ((((((a) & 65535) + ((value) & 65535)) + ((carry) & 65535))) & 65535);
  let result = ((((wide_result) & 255)) & 255);
  let flags = ((result_szxy_flags(result)) & 255);
  if (((((((((a & 0x0f) + (value & 0x0f))) >>> 0) + carry)) >>> 0) > 0x0f)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if (((((~((a ^ value))) & (a ^ result)) & 0x80) !== 0)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  if ((wide_result > 0xff)) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  cpu.a = ((result) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function alu_sub(cpu, value, borrow) {
  let a = ((cpu.a) & 255);
  let subtrahend = (((((value) & 65535) + ((borrow) & 65535))) & 65535);
  let result = (((((((a) & 65535) - subtrahend)) & 255)) & 255);
  let flags = (((result_szxy_flags(result) | Z80_FLAG_N)) & 255);
  if (((((a ^ value) ^ result) & 0x10) !== 0)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if (((((a ^ value) & (a ^ result)) & 0x80) !== 0)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  if ((((a) & 65535) < subtrahend)) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  cpu.a = ((result) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function alu_and(cpu, value) {
  let result = (((((cpu.a & value)) & 255)) & 255);
  let flags = (((result_szxy_flags(result) | Z80_FLAG_H)) & 255);
  if (parity_even(result)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  cpu.a = ((result) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function alu_xor(cpu, value) {
  let result = (((((cpu.a ^ value)) & 255)) & 255);
  let flags = ((result_szxy_flags(result)) & 255);
  if (parity_even(result)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  cpu.a = ((result) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function alu_or(cpu, value) {
  let result = (((((cpu.a | value)) & 255)) & 255);
  let flags = ((result_szxy_flags(result)) & 255);
  if (parity_even(result)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  cpu.a = ((result) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function alu_cp(cpu, value) {
  let a = ((cpu.a) & 255);
  let result = (((((a - value)) & 255)) & 255);
  let flags = (((((result & Z80_FLAG_S) | (value & (Z80_FLAG_Y | Z80_FLAG_X))) | Z80_FLAG_N)) & 255);
  if ((result === 0)) {
    flags = (((flags | Z80_FLAG_Z)) & 255);
  }
  if (((((a ^ value) ^ result) & 0x10) !== 0)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if (((((a ^ value) & (a ^ result)) & 0x80) !== 0)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  if ((a < value)) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function finish_step(cpu, t_states, completed_instruction) {
  cpu.total_t_states = (cpu.total_t_states + t_states);
  if ((completed_instruction && (cpu.ei_delay > 0))) {
    (cpu.ei_delay = ((cpu.ei_delay - 1) & 255));
  }
  return ((t_states) >>> 0);
}

function cb_rotate_shift(cpu, operation, value) {
  let old_carry = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 255);
  let result = ((value) & 255);
  let carry = ((0) & 255);
  switch ((operation & 7)) {
    case 0:
      carry = (((((value >> 7)) & 255)) & 255);
      result = ((((((value << 1) | carry)) & 255)) & 255);
      break;
    case 1:
      carry = (((value & 1)) & 255);
      result = ((((((value >> 1) | (carry << 7))) & 255)) & 255);
      break;
    case 2:
      carry = (((((value >> 7)) & 255)) & 255);
      result = ((((((value << 1) | old_carry)) & 255)) & 255);
      break;
    case 3:
      carry = (((value & 1)) & 255);
      result = ((((((value >> 1) | (old_carry << 7))) & 255)) & 255);
      break;
    case 4:
      carry = (((((value >> 7)) & 255)) & 255);
      result = (((((value << 1)) & 255)) & 255);
      break;
    case 5:
      carry = (((value & 1)) & 255);
      result = ((((((value >> 1) | (value & 0x80))) & 255)) & 255);
      break;
    case 6:
      carry = (((((value >> 7)) & 255)) & 255);
      result = ((((((value << 1) | 1)) & 255)) & 255);
      break;
    case 7:
      carry = (((value & 1)) & 255);
      result = (((((value >> 1)) & 255)) & 255);
      break;
    default:
      break;
  }
  let flags = ((result_szxy_flags(result)) & 255);
  if (parity_even(result)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  if ((carry !== 0)) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
  return ((result) & 255);
}

function execute_cb(cpu) {
  let opcode = ((memory_read(cpu, cpu.pc)) & 255);
  (cpu.pc = ((cpu.pc + 1) & 65535));
  increment_r(cpu);
  let group = (((((opcode >> 6)) & 255)) & 255);
  let operation = ((((((opcode >> 3) & 7)) & 255)) & 255);
  let code = (((opcode & 7)) & 255);
  let memory_operand = !!((code === 6));
  let value = ((read_register_code(cpu, code)) & 255);
  if ((group === 0)) {
    let result = ((cb_rotate_shift(cpu, operation, value)) & 255);
    write_register_code(cpu, code, result);
    return ((finish_step(cpu, (((memory_operand ? 15 : 8)) >>> 0), !!(true))) >>> 0);
  }
  if ((group === 1)) {
    let mask = (((((((1 << operation)) >>> 0)) & 255)) & 255);
    let bit_set = !!(((value & mask) !== 0));
    let flags = ((((cpu.f & Z80_FLAG_C) | Z80_FLAG_H)) & 255);
    if ((!(bit_set))) {
      flags = (((flags | (Z80_FLAG_Z | Z80_FLAG_PV))) & 255);
    }
    if (((operation === 7) && bit_set)) {
      flags = (((flags | Z80_FLAG_S)) & 255);
    }
    if (memory_operand) {
      flags = (((flags | ((((get_hl(cpu) >> 8) & (Z80_FLAG_X | Z80_FLAG_Y))) & 255))) & 255);
    }
    else {
      flags = (((flags | (value & (Z80_FLAG_X | Z80_FLAG_Y)))) & 255);
    }
    cpu.f = ((flags) & 255);
    cpu.q = ((flags) & 255);
    return ((finish_step(cpu, (((memory_operand ? 12 : 8)) >>> 0), !!(true))) >>> 0);
  }
  if ((group === 2)) {
    let result = (((((value & (((~((((((1 << operation)) >>> 0)) & 255)))) & 255))) & 255)) & 255);
    write_register_code(cpu, code, result);
    return ((finish_step(cpu, (((memory_operand ? 15 : 8)) >>> 0), !!(true))) >>> 0);
  }
{
    let result = (((((value | (((((1 << operation)) >>> 0)) & 255))) & 255)) & 255);
    write_register_code(cpu, code, result);
    return ((finish_step(cpu, (((memory_operand ? 15 : 8)) >>> 0), !!(true))) >>> 0);
  }
}

function alu_adc_hl(cpu, value) {
  let hl = ((get_hl(cpu)) & 65535);
  let carry = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 65535);
  let wide_result = ((((((((((hl) >>> 0) + ((value) >>> 0))) >>> 0) + ((carry) >>> 0))) >>> 0)) >>> 0);
  let result = ((((wide_result) & 65535)) & 65535);
  let flags = ((((((result >> 8) & ((Z80_FLAG_S | Z80_FLAG_X) | Z80_FLAG_Y))) & 255)) & 255);
  if ((result === 0)) {
    flags = (((flags | Z80_FLAG_Z)) & 255);
  }
  if (((((((((hl & 0x0fff) + (value & 0x0fff))) >>> 0) + carry)) >>> 0) > 0x0fff)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if (((((~((hl ^ value))) & (hl ^ result)) & 0x8000) !== 0)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  if ((wide_result > 0xffff)) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  cpu.h = (((((result >> 8)) & 255)) & 255);
  cpu.l = ((((result) & 255)) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function alu_sbc_hl(cpu, value) {
  let hl = ((get_hl(cpu)) & 65535);
  let borrow = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 65535);
  let subtrahend = (((((((value) >>> 0) + ((borrow) >>> 0))) >>> 0)) >>> 0);
  let result = (((((((((hl) >>> 0) - subtrahend)) >>> 0)) & 65535)) & 65535);
  let flags = (((((((result >> 8) & ((Z80_FLAG_S | Z80_FLAG_X) | Z80_FLAG_Y)) | Z80_FLAG_N)) & 255)) & 255);
  if ((result === 0)) {
    flags = (((flags | Z80_FLAG_Z)) & 255);
  }
  if (((((hl ^ value) ^ result) & 0x1000) !== 0)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if (((((hl ^ value) & (hl ^ result)) & 0x8000) !== 0)) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  if ((((hl) >>> 0) < subtrahend)) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  cpu.h = (((((result >> 8)) & 255)) & 255);
  cpu.l = ((((result) & 255)) & 255);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function alu_neg(cpu) {
  let value = ((cpu.a) & 255);
  cpu.a = ((0) & 255);
  alu_sub(cpu, value, ((0) & 255));
}

function load_a_from_interrupt_register(cpu, value) {
  cpu.a = ((value) & 255);
  let flags = ((((cpu.f & Z80_FLAG_C) | result_szxy_flags(value))) & 255);
  if (cpu.iff2) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function block_io_flags(cpu, transferred, k) {
  let flags = (((cpu.b & ((Z80_FLAG_S | Z80_FLAG_X) | Z80_FLAG_Y))) & 255);
  if ((cpu.b === 0)) {
    flags = (((flags | Z80_FLAG_Z)) & 255);
  }
  if (((transferred & 0x80) !== 0)) {
    flags = (((flags | Z80_FLAG_N)) & 255);
  }
  if ((k > 0xff)) {
    flags = (((flags | (Z80_FLAG_H | Z80_FLAG_C))) & 255);
  }
  if (parity_even(((((k & 7) ^ cpu.b)) & 255))) {
    flags = (((flags | Z80_FLAG_PV)) & 255);
  }
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
}

function execute_ed(cpu) {
  let opcode = ((memory_read(cpu, cpu.pc)) & 255);
  (cpu.pc = ((cpu.pc + 1) & 65535));
  increment_r(cpu);
  if (((opcode & 0xc7) === 0x40)) {
    let code = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let port = ((((((((cpu.b) & 65535) << 8) | cpu.c)) & 65535)) & 65535);
    let value = ((io_read(cpu, port)) & 255);
    if ((code !== 6)) {
      write_register_code(cpu, code, value);
    }
    let flags = ((((cpu.f & Z80_FLAG_C) | result_szxy_flags(value))) & 255);
    if (parity_even(value)) {
      flags = (((flags | Z80_FLAG_PV)) & 255);
    }
    cpu.f = ((flags) & 255);
    cpu.q = ((flags) & 255);
    cpu.wz = (((((((port + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((12) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0x41)) {
    let code = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let port = ((((((((cpu.b) & 65535) << 8) | cpu.c)) & 65535)) & 65535);
    let value = ((((code === 6) ? 0 : read_register_code(cpu, code))) & 255);
    io_write(cpu, port, value);
    cpu.wz = (((((((port + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((12) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x42)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    alu_sbc_hl(cpu, get_register_pair(cpu, pair));
    return ((finish_step(cpu, ((15) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x4a)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    alu_adc_hl(cpu, get_register_pair(cpu, pair));
    return ((finish_step(cpu, ((15) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x43)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    let address = ((fetch_word(cpu)) & 65535);
    let value = ((get_register_pair(cpu, pair)) & 65535);
    memory_write(cpu, address, ((value) & 255));
    memory_write(cpu, (((((address + 1)) >>> 0)) & 65535), (((value >> 8)) & 255));
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((20) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x4b)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    let address = ((fetch_word(cpu)) & 65535);
    let low = ((memory_read(cpu, address)) & 255);
    let high = ((memory_read(cpu, (((((address + 1)) >>> 0)) & 65535))) & 255);
    set_register_pair(cpu, pair, ((((((high) & 65535) << 8) | low)) & 65535));
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((20) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0x44)) {
    alu_neg(cpu);
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0x45)) {
    let reti = !!(((opcode & 0x08) !== 0));
    let target = ((pop_word(cpu)) & 65535);
    cpu.pc = ((target) & 65535);
    cpu.wz = ((target) & 65535);
    cpu.iff1 = !!(cpu.iff2);
    if ((reti && (cpu.bus.reti !== 0))) {
      cpu.bus.reti(cpu.bus.context);
    }
    return ((finish_step(cpu, ((14) >>> 0), !!(true))) >>> 0);
  }
  if (((((opcode === 0x46) || (opcode === 0x4e)) || (opcode === 0x66)) || (opcode === 0x6e))) {
    cpu.interrupt_mode = ((0) & 255);
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode === 0x56) || (opcode === 0x76))) {
    cpu.interrupt_mode = ((1) & 255);
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode === 0x5e) || (opcode === 0x7e))) {
    cpu.interrupt_mode = ((2) & 255);
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x47)) {
    cpu.i = ((cpu.a) & 255);
    return ((finish_step(cpu, ((9) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x4f)) {
    cpu.r = ((cpu.a) & 255);
    return ((finish_step(cpu, ((9) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x57)) {
    load_a_from_interrupt_register(cpu, cpu.i);
    return ((finish_step(cpu, ((9) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x5f)) {
    load_a_from_interrupt_register(cpu, cpu.r);
    return ((finish_step(cpu, ((9) >>> 0), !!(true))) >>> 0);
  }
  if (((((opcode === 0xa0) || (opcode === 0xa8)) || (opcode === 0xb0)) || (opcode === 0xb8))) {
    let decrement = !!(((opcode & 0x08) !== 0));
    let repeat = !!(((opcode & 0x10) !== 0));
    let hl = ((get_hl(cpu)) & 65535);
    let de = ((get_register_pair(cpu, ((1) & 255))) & 65535);
    let bc = ((get_register_pair(cpu, ((0) & 255))) & 65535);
    let value = ((memory_read(cpu, hl)) & 255);
    memory_write(cpu, de, value);
    let new_hl = (((decrement ? (((((hl - 1)) >>> 0)) & 65535) : (((((hl + 1)) >>> 0)) & 65535))) & 65535);
    let new_de = (((decrement ? (((((de - 1)) >>> 0)) & 65535) : (((((de + 1)) >>> 0)) & 65535))) & 65535);
    let new_bc = (((((((bc - 1)) >>> 0)) & 65535)) & 65535);
    set_register_pair(cpu, ((2) & 255), new_hl);
    set_register_pair(cpu, ((1) & 255), new_de);
    set_register_pair(cpu, ((0) & 255), new_bc);
    let sum = (((((cpu.a + value)) & 255)) & 255);
    let flags = (((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_C))) & 255);
    if ((new_bc !== 0)) {
      flags = (((flags | Z80_FLAG_PV)) & 255);
    }
    flags = (((flags | (sum & Z80_FLAG_X))) & 255);
    if (((sum & 0x02) !== 0)) {
      flags = (((flags | Z80_FLAG_Y)) & 255);
    }
    cpu.f = ((flags) & 255);
    cpu.q = ((flags) & 255);
    if ((repeat && (new_bc !== 0))) {
      cpu.pc = (((((((cpu.pc - 2)) >>> 0)) & 65535)) & 65535);
      return ((finish_step(cpu, ((21) >>> 0), !!(true))) >>> 0);
    }
    return ((finish_step(cpu, ((16) >>> 0), !!(true))) >>> 0);
  }
  if (((((opcode === 0xa1) || (opcode === 0xa9)) || (opcode === 0xb1)) || (opcode === 0xb9))) {
    let decrement = !!(((opcode & 0x08) !== 0));
    let repeat = !!(((opcode & 0x10) !== 0));
    let hl = ((get_hl(cpu)) & 65535);
    let bc = ((get_register_pair(cpu, ((0) & 255))) & 65535);
    let value = ((memory_read(cpu, hl)) & 255);
    let result = (((((cpu.a - value)) & 255)) & 255);
    let half_carry = !!(((((cpu.a ^ value) ^ result) & 0x10) !== 0));
    let new_hl = (((decrement ? (((((hl - 1)) >>> 0)) & 65535) : (((((hl + 1)) >>> 0)) & 65535))) & 65535);
    let new_bc = (((((((bc - 1)) >>> 0)) & 65535)) & 65535);
    set_register_pair(cpu, ((2) & 255), new_hl);
    set_register_pair(cpu, ((0) & 255), new_bc);
    let flags = ((((cpu.f & Z80_FLAG_C) | Z80_FLAG_N)) & 255);
    if (((result & 0x80) !== 0)) {
      flags = (((flags | Z80_FLAG_S)) & 255);
    }
    if ((result === 0)) {
      flags = (((flags | Z80_FLAG_Z)) & 255);
    }
    if (half_carry) {
      flags = (((flags | Z80_FLAG_H)) & 255);
    }
    if ((new_bc !== 0)) {
      flags = (((flags | Z80_FLAG_PV)) & 255);
    }
    let adjusted = (((((((result - (half_carry ? 1 : 0))) >>> 0)) & 255)) & 255);
    flags = (((flags | (adjusted & Z80_FLAG_X))) & 255);
    if (((adjusted & 0x02) !== 0)) {
      flags = (((flags | Z80_FLAG_Y)) & 255);
    }
    cpu.f = ((flags) & 255);
    cpu.q = ((flags) & 255);
    if (((repeat && (new_bc !== 0)) && (result !== 0))) {
      cpu.pc = (((((((cpu.pc - 2)) >>> 0)) & 65535)) & 65535);
      return ((finish_step(cpu, ((21) >>> 0), !!(true))) >>> 0);
    }
    return ((finish_step(cpu, ((16) >>> 0), !!(true))) >>> 0);
  }
  if (((((opcode === 0xa2) || (opcode === 0xaa)) || (opcode === 0xb2)) || (opcode === 0xba))) {
    let decrement = !!(((opcode & 0x08) !== 0));
    let repeat = !!(((opcode & 0x10) !== 0));
    let old_hl = ((get_hl(cpu)) & 65535);
    (cpu.b = ((cpu.b - 1) & 255));
    let port = ((((((((cpu.b) & 65535) << 8) | cpu.c)) & 65535)) & 65535);
    let value = ((io_read(cpu, port)) & 255);
    memory_write(cpu, old_hl, value);
    let new_hl = (((decrement ? (((((old_hl - 1)) >>> 0)) & 65535) : (((((old_hl + 1)) >>> 0)) & 65535))) & 65535);
    set_register_pair(cpu, ((2) & 255), new_hl);
    let adjusted_c = (((decrement ? (((((cpu.c - 1)) >>> 0)) & 255) : (((((cpu.c + 1)) >>> 0)) & 255))) & 255);
    let k = (((((value) & 65535) + ((adjusted_c) & 65535))) & 65535);
    block_io_flags(cpu, value, k);
    if ((repeat && (cpu.b !== 0))) {
      cpu.pc = (((((((cpu.pc - 2)) >>> 0)) & 65535)) & 65535);
      cpu.wz = (((((((cpu.pc + 1)) >>> 0)) & 65535)) & 65535);
      return ((finish_step(cpu, ((21) >>> 0), !!(true))) >>> 0);
    }
    cpu.wz = (((decrement ? (((((port - 1)) >>> 0)) & 65535) : (((((port + 1)) >>> 0)) & 65535))) & 65535);
    return ((finish_step(cpu, ((16) >>> 0), !!(true))) >>> 0);
  }
  if (((((opcode === 0xa3) || (opcode === 0xab)) || (opcode === 0xb3)) || (opcode === 0xbb))) {
    let decrement = !!(((opcode & 0x08) !== 0));
    let repeat = !!(((opcode & 0x10) !== 0));
    let old_hl = ((get_hl(cpu)) & 65535);
    let port = ((((((((cpu.b) & 65535) << 8) | cpu.c)) & 65535)) & 65535);
    let value = ((memory_read(cpu, old_hl)) & 255);
    io_write(cpu, port, value);
    (cpu.b = ((cpu.b - 1) & 255));
    let new_hl = (((decrement ? (((((old_hl - 1)) >>> 0)) & 65535) : (((((old_hl + 1)) >>> 0)) & 65535))) & 65535);
    set_register_pair(cpu, ((2) & 255), new_hl);
    let new_l = ((((new_hl) & 255)) & 255);
    let k = (((((value) & 65535) + ((new_l) & 65535))) & 65535);
    block_io_flags(cpu, value, k);
    if ((repeat && (cpu.b !== 0))) {
      cpu.pc = (((((((cpu.pc - 2)) >>> 0)) & 65535)) & 65535);
      cpu.wz = (((((((cpu.pc + 1)) >>> 0)) & 65535)) & 65535);
      return ((finish_step(cpu, ((21) >>> 0), !!(true))) >>> 0);
    }
    cpu.wz = (((decrement ? (((((port - 1)) >>> 0)) & 65535) : (((((port + 1)) >>> 0)) & 65535))) & 65535);
    return ((finish_step(cpu, ((16) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x67)) {
    let address = ((get_hl(cpu)) & 65535);
    let memory_value = ((memory_read(cpu, address)) & 255);
    let old_a = ((cpu.a) & 255);
    let new_memory = (((((((((old_a & 0x0f) << 4)) >>> 0) | (memory_value >> 4))) & 255)) & 255);
    let new_a = ((((((old_a & 0xf0) | (memory_value & 0x0f))) & 255)) & 255);
    memory_write(cpu, address, new_memory);
    cpu.a = ((new_a) & 255);
    let flags = ((((cpu.f & Z80_FLAG_C) | result_szxy_flags(new_a))) & 255);
    if (parity_even(new_a)) {
      flags = (((flags | Z80_FLAG_PV)) & 255);
    }
    cpu.f = ((flags) & 255);
    cpu.q = ((flags) & 255);
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((18) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x6f)) {
    let address = ((get_hl(cpu)) & 65535);
    let memory_value = ((memory_read(cpu, address)) & 255);
    let old_a = ((cpu.a) & 255);
    let new_memory = ((((((memory_value << 4) | (old_a & 0x0f))) & 255)) & 255);
    let new_a = ((((((old_a & 0xf0) | (memory_value >> 4))) & 255)) & 255);
    memory_write(cpu, address, new_memory);
    cpu.a = ((new_a) & 255);
    let flags = ((((cpu.f & Z80_FLAG_C) | result_szxy_flags(new_a))) & 255);
    if (parity_even(new_a)) {
      flags = (((flags | Z80_FLAG_PV)) & 255);
    }
    cpu.f = ((flags) & 255);
    cpu.q = ((flags) & 255);
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((18) >>> 0), !!(true))) >>> 0);
  }
  return ((0) >>> 0);
}

function injected_memory_read(context, address) {
  let injected = context;
  if (((!(injected.injected_consumed)) && (address === injected.injected_address))) {
    injected.injected_consumed = !!(true);
    return ((injected.injected_opcode) & 255);
  }
  if ((injected.original_bus.memory_read === 0)) {
    return ((0xff) & 255);
  }
  return ((injected.original_bus.memory_read(injected.original_bus.context, address)) & 255);
}

function injected_memory_write(context, address, value) {
  let injected = context;
  if ((injected.original_bus.memory_write === 0)) {
    return;
  }
  injected.original_bus.memory_write(injected.original_bus.context, address, value);
}

function injected_io_read(context, port) {
  let injected = context;
  if ((injected.original_bus.io_read === 0)) {
    return ((0xff) & 255);
  }
  return ((injected.original_bus.io_read(injected.original_bus.context, port)) & 255);
}

function injected_io_write(context, port, value) {
  let injected = context;
  if ((injected.original_bus.io_write === 0)) {
    return;
  }
  injected.original_bus.io_write(injected.original_bus.context, port, value);
}

function injected_interrupt_acknowledge(context) {
  let injected = context;
  if ((injected.original_bus.interrupt_acknowledge === 0)) {
    return ((0xff) & 255);
  }
  return ((injected.original_bus.interrupt_acknowledge(injected.original_bus.context)) & 255);
}

function injected_reti(context) {
  let injected = context;
  if ((injected.original_bus.reti === 0)) {
    return;
  }
  injected.original_bus.reti(injected.original_bus.context);
}

function get_index(cpu, use_iy) {
  return (((use_iy ? cpu.iy : cpu.ix)) & 65535);
}

function set_index(cpu, use_iy, value) {
  if (use_iy) {
    cpu.iy = ((value) & 65535);
  }
  else {
    cpu.ix = ((value) & 65535);
  }
}

function read_index_register(cpu, use_iy, code) {
  let index = ((get_index(cpu, use_iy)) & 65535);
  switch ((code & 7)) {
    case 0:
      return ((cpu.b) & 255);
    case 1:
      return ((cpu.c) & 255);
    case 2:
      return ((cpu.d) & 255);
    case 3:
      return ((cpu.e) & 255);
    case 4:
      return (((((index >> 8)) & 255)) & 255);
    case 5:
      return ((((index) & 255)) & 255);
    case 7:
      return ((cpu.a) & 255);
    default:
      return ((0xff) & 255);
  }
}

function write_index_register(cpu, use_iy, code, value) {
  let index = ((get_index(cpu, use_iy)) & 65535);
  switch ((code & 7)) {
    case 0:
      cpu.b = ((value) & 255);
      break;
    case 1:
      cpu.c = ((value) & 255);
      break;
    case 2:
      cpu.d = ((value) & 255);
      break;
    case 3:
      cpu.e = ((value) & 255);
      break;
    case 4:
      index = ((((((((value) & 65535) << 8) | (index & 0x00ff))) & 65535)) & 65535);
      set_index(cpu, use_iy, index);
      break;
    case 5:
      index = ((((((index & 0xff00) | value)) & 65535)) & 65535);
      set_index(cpu, use_iy, index);
      break;
    case 7:
      cpu.a = ((value) & 255);
      break;
    default:
      break;
  }
}

function indexed_address(cpu, use_iy, displacement) {
  let offset = ((((displacement < 0x80) ? ((displacement) << 16 >> 16) : (((displacement) << 16 >> 16) - 0x100))) << 16 >> 16);
  return (((((((get_index(cpu, use_iy)) | 0) + offset)) & 65535)) & 65535);
}

function get_index_pair(cpu, use_iy, pair) {
  if (((pair & 3) === 2)) {
    return ((get_index(cpu, use_iy)) & 65535);
  }
  return ((get_register_pair(cpu, pair)) & 65535);
}

function set_index_pair(cpu, use_iy, pair, value) {
  if (((pair & 3) === 2)) {
    set_index(cpu, use_iy, value);
    return;
  }
  set_register_pair(cpu, pair, value);
}

function alu_add_index(cpu, use_iy, value) {
  let index = ((get_index(cpu, use_iy)) & 65535);
  let wide_result = (((((((index) >>> 0) + ((value) >>> 0))) >>> 0)) >>> 0);
  let result = ((((wide_result) & 65535)) & 65535);
  let flags = (((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV))) & 255);
  flags = (((flags | ((((result >> 8) & (Z80_FLAG_X | Z80_FLAG_Y))) & 255))) & 255);
  if ((((((index & 0x0fff) + (value & 0x0fff))) >>> 0) > 0x0fff)) {
    flags = (((flags | Z80_FLAG_H)) & 255);
  }
  if ((wide_result > 0xffff)) {
    flags = (((flags | Z80_FLAG_C)) & 255);
  }
  set_index(cpu, use_iy, result);
  cpu.f = ((flags) & 255);
  cpu.q = ((flags) & 255);
  cpu.wz = (((((((index + 1)) >>> 0)) & 65535)) & 65535);
}

function execute_index_alu(cpu, operation, value) {
  let carry = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 255);
  switch ((operation & 7)) {
    case 0:
      alu_add(cpu, value, ((0) & 255));
      break;
    case 1:
      alu_add(cpu, value, carry);
      break;
    case 2:
      alu_sub(cpu, value, ((0) & 255));
      break;
    case 3:
      alu_sub(cpu, value, carry);
      break;
    case 4:
      alu_and(cpu, value);
      break;
    case 5:
      alu_xor(cpu, value);
      break;
    case 6:
      alu_or(cpu, value);
      break;
    case 7:
      alu_cp(cpu, value);
      break;
    default:
      break;
  }
}

function execute_index_cb(cpu, use_iy) {
  let displacement = ((fetch_byte(cpu)) & 255);
  let opcode = ((fetch_byte(cpu)) & 255);
  let address = ((indexed_address(cpu, use_iy, displacement)) & 65535);
  let value = ((memory_read(cpu, address)) & 255);
  let group = (((((opcode >> 6)) & 255)) & 255);
  let operation = ((((((opcode >> 3) & 7)) & 255)) & 255);
  let destination = (((opcode & 7)) & 255);
  cpu.wz = ((address) & 65535);
  if ((group === 0)) {
    let result = ((cb_rotate_shift(cpu, operation, value)) & 255);
    memory_write(cpu, address, result);
    if ((destination !== 6)) {
      write_register_code(cpu, destination, result);
    }
    return ((finish_step(cpu, ((23) >>> 0), !!(true))) >>> 0);
  }
  if ((group === 1)) {
    let mask = (((((((1 << operation)) >>> 0)) & 255)) & 255);
    let bit_set = !!(((value & mask) !== 0));
    let flags = ((((cpu.f & Z80_FLAG_C) | Z80_FLAG_H)) & 255);
    if ((!(bit_set))) {
      flags = (((flags | (Z80_FLAG_Z | Z80_FLAG_PV))) & 255);
    }
    if (((operation === 7) && bit_set)) {
      flags = (((flags | Z80_FLAG_S)) & 255);
    }
    flags = (((flags | ((((address >> 8) & (Z80_FLAG_X | Z80_FLAG_Y))) & 255))) & 255);
    cpu.f = ((flags) & 255);
    cpu.q = ((flags) & 255);
    return ((finish_step(cpu, ((20) >>> 0), !!(true))) >>> 0);
  }
  if ((group === 2)) {
    let result = (((((value & (((~((((((1 << operation)) >>> 0)) & 255)))) & 255))) & 255)) & 255);
    memory_write(cpu, address, result);
    if ((destination !== 6)) {
      write_register_code(cpu, destination, result);
    }
    return ((finish_step(cpu, ((23) >>> 0), !!(true))) >>> 0);
  }
{
    let result = (((((value | (((((1 << operation)) >>> 0)) & 255))) & 255)) & 255);
    memory_write(cpu, address, result);
    if ((destination !== 6)) {
      write_register_code(cpu, destination, result);
    }
    return ((finish_step(cpu, ((23) >>> 0), !!(true))) >>> 0);
  }
}

function execute_index(cpu, use_iy) {
  let opcode = ((memory_read(cpu, cpu.pc)) & 255);
  (cpu.pc = ((cpu.pc + 1) & 65535));
  increment_r(cpu);
  if ((opcode === 0xcb)) {
    return ((execute_index_cb(cpu, use_iy)) >>> 0);
  }
  if (((opcode & 0xcf) === 0x01)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    let value = ((fetch_word(cpu)) & 65535);
    set_index_pair(cpu, use_iy, pair, value);
    return ((finish_step(cpu, ((14) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x09)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    alu_add_index(cpu, use_iy, get_index_pair(cpu, use_iy, pair));
    return ((finish_step(cpu, ((15) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x03)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    set_index_pair(cpu, use_iy, pair, (((((get_index_pair(cpu, use_iy, pair) + 1)) >>> 0)) & 65535));
    return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x0b)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    set_index_pair(cpu, use_iy, pair, (((((get_index_pair(cpu, use_iy, pair) - 1)) >>> 0)) & 65535));
    return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0x04)) {
    let code = ((((((opcode >> 3) & 7)) & 255)) & 255);
    if ((code === 6)) {
      let displacement = ((fetch_byte(cpu)) & 255);
      let address = ((indexed_address(cpu, use_iy, displacement)) & 65535);
      let result = ((alu_inc(cpu, memory_read(cpu, address))) & 255);
      memory_write(cpu, address, result);
      cpu.wz = ((address) & 65535);
      return ((finish_step(cpu, ((23) >>> 0), !!(true))) >>> 0);
    }
    let value = ((read_index_register(cpu, use_iy, code)) & 255);
    write_index_register(cpu, use_iy, code, alu_inc(cpu, value));
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0x05)) {
    let code = ((((((opcode >> 3) & 7)) & 255)) & 255);
    if ((code === 6)) {
      let displacement = ((fetch_byte(cpu)) & 255);
      let address = ((indexed_address(cpu, use_iy, displacement)) & 65535);
      let result = ((alu_dec(cpu, memory_read(cpu, address))) & 255);
      memory_write(cpu, address, result);
      cpu.wz = ((address) & 65535);
      return ((finish_step(cpu, ((23) >>> 0), !!(true))) >>> 0);
    }
    let value = ((read_index_register(cpu, use_iy, code)) & 255);
    write_index_register(cpu, use_iy, code, alu_dec(cpu, value));
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0x06)) {
    let code = ((((((opcode >> 3) & 7)) & 255)) & 255);
    if ((code === 6)) {
      let displacement = ((fetch_byte(cpu)) & 255);
      let value = ((fetch_byte(cpu)) & 255);
      let address = ((indexed_address(cpu, use_iy, displacement)) & 65535);
      memory_write(cpu, address, value);
      cpu.wz = ((address) & 65535);
      return ((finish_step(cpu, ((19) >>> 0), !!(true))) >>> 0);
    }
    write_index_register(cpu, use_iy, code, fetch_byte(cpu));
    return ((finish_step(cpu, ((11) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode >= 0x40) && (opcode <= 0x7f))) {
    if ((opcode === 0x76)) {
      cpu.halted = !!(true);
      return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
    }
    let destination = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let source = (((opcode & 7)) & 255);
    if (((source === 6) || (destination === 6))) {
      let displacement = ((fetch_byte(cpu)) & 255);
      let address = ((indexed_address(cpu, use_iy, displacement)) & 65535);
      if ((source === 6)) {
        let value = ((memory_read(cpu, address)) & 255);
        write_register_code(cpu, destination, value);
      }
      else {
        let value = ((read_register_code(cpu, source)) & 255);
        memory_write(cpu, address, value);
      }
      cpu.wz = ((address) & 65535);
      return ((finish_step(cpu, ((19) >>> 0), !!(true))) >>> 0);
    }
    let value = ((read_index_register(cpu, use_iy, source)) & 255);
    write_index_register(cpu, use_iy, destination, value);
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode >= 0x80) && (opcode <= 0xbf))) {
    let operation = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let source = (((opcode & 7)) & 255);
    if ((source === 6)) {
      let displacement = ((fetch_byte(cpu)) & 255);
      let address = ((indexed_address(cpu, use_iy, displacement)) & 65535);
      execute_index_alu(cpu, operation, memory_read(cpu, address));
      cpu.wz = ((address) & 65535);
      return ((finish_step(cpu, ((19) >>> 0), !!(true))) >>> 0);
    }
    execute_index_alu(cpu, operation, read_index_register(cpu, use_iy, source));
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x22)) {
    let address = ((fetch_word(cpu)) & 65535);
    let value = ((get_index(cpu, use_iy)) & 65535);
    memory_write(cpu, address, ((value) & 255));
    memory_write(cpu, (((((address + 1)) >>> 0)) & 65535), (((value >> 8)) & 255));
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((20) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x2a)) {
    let address = ((fetch_word(cpu)) & 65535);
    let low = ((memory_read(cpu, address)) & 255);
    let high = ((memory_read(cpu, (((((address + 1)) >>> 0)) & 65535))) & 255);
    set_index(cpu, use_iy, ((((((high) & 65535) << 8) | low)) & 65535));
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((20) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xe1)) {
    set_index(cpu, use_iy, pop_word(cpu));
    return ((finish_step(cpu, ((14) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xe5)) {
    push_word(cpu, get_index(cpu, use_iy));
    return ((finish_step(cpu, ((15) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xe3)) {
    let old_index = ((get_index(cpu, use_iy)) & 65535);
    let low = ((memory_read(cpu, cpu.sp)) & 255);
    let high = ((memory_read(cpu, (((((cpu.sp + 1)) >>> 0)) & 65535))) & 255);
    memory_write(cpu, cpu.sp, ((old_index) & 255));
    memory_write(cpu, (((((cpu.sp + 1)) >>> 0)) & 65535), (((old_index >> 8)) & 255));
    let new_index = ((((((((high) & 65535) << 8) | low)) & 65535)) & 65535);
    set_index(cpu, use_iy, new_index);
    cpu.wz = ((new_index) & 65535);
    return ((finish_step(cpu, ((23) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xe9)) {
    cpu.pc = ((get_index(cpu, use_iy)) & 65535);
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xf9)) {
    cpu.sp = ((get_index(cpu, use_iy)) & 65535);
    return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
  }
  (cpu.pc = ((cpu.pc - 1) & 65535));
  decrement_r(cpu);
  let cycles = ((z80_step(cpu)) >>> 0);
  if ((cycles === 0)) {
    return ((0) >>> 0);
  }
  cpu.total_t_states = (cpu.total_t_states + 4);
  return (((((cycles + 4)) >>> 0)) >>> 0);
}

function service_nmi(cpu) {
  cpu.nmi_pending = !!(false);
  cpu.halted = !!(false);
  cpu.iff1 = !!(false);
  increment_r(cpu);
  push_word(cpu, cpu.pc);
  cpu.pc = ((0x0066) & 65535);
  cpu.wz = ((0x0066) & 65535);
  return ((finish_step(cpu, ((11) >>> 0), !!(false))) >>> 0);
}

function service_irq(cpu) {
  cpu.halted = !!(false);
  cpu.iff1 = !!(false);
  cpu.iff2 = !!(false);
  if ((cpu.interrupt_mode === 0)) {
    let supplied_opcode = ((interrupt_acknowledge(cpu)) & 255);
    let original_bus = cpu.bus;
    let original_pc = ((cpu.pc) & 65535);
    let injected = {original_bus: original_bus, injected_address: (((((original_pc - 1)) >>> 0)) & 65535), injected_opcode: supplied_opcode, injected_consumed: false};
    cpu.bus = {context: injected, memory_read: injected_memory_read, memory_write: injected_memory_write, io_read: injected_io_read, io_write: injected_io_write, interrupt_acknowledge: injected_interrupt_acknowledge, reti: injected_reti};
    cpu.pc = (((((((original_pc - 1)) >>> 0)) & 65535)) & 65535);
    let instruction_cycles = ((z80_step(cpu)) >>> 0);
    cpu.bus = original_bus;
    if ((instruction_cycles === 0)) {
      cpu.pc = ((original_pc) & 65535);
      return ((0) >>> 0);
    }
    cpu.total_t_states = (cpu.total_t_states + 2);
    return (((((instruction_cycles + 2)) >>> 0)) >>> 0);
  }
  increment_r(cpu);
  if ((cpu.interrupt_mode === 1)) {
    interrupt_acknowledge(cpu);
    push_word(cpu, cpu.pc);
    cpu.pc = ((0x0038) & 65535);
    cpu.wz = ((0x0038) & 65535);
    return ((finish_step(cpu, ((13) >>> 0), !!(false))) >>> 0);
  }
{
    let vector = ((interrupt_acknowledge(cpu)) & 255);
    let table_address = ((((((((cpu.i) & 65535) << 8) | vector)) & 65535)) & 65535);
    let low = ((memory_read(cpu, table_address)) & 255);
    let high = ((memory_read(cpu, (((((table_address + 1)) >>> 0)) & 65535))) & 255);
    let target = ((((((((high) & 65535) << 8) | low)) & 65535)) & 65535);
    push_word(cpu, cpu.pc);
    cpu.pc = ((target) & 65535);
    cpu.wz = ((target) & 65535);
    return ((finish_step(cpu, ((19) >>> 0), !!(false))) >>> 0);
  }
}

function opcode_is_supported(opcode) {
  if ((((opcode >= 0x40) && (opcode <= 0x7f)) || ((opcode >= 0x80) && (opcode <= 0xbf)))) {
    return !!(true);
  }
  if ((((((opcode & 0xc7) === 0x04) || ((opcode & 0xc7) === 0x05)) || ((opcode & 0xcf) === 0x03)) || ((opcode & 0xcf) === 0x0b))) {
    return !!(true);
  }
  if (((opcode === 0x10) || ((opcode & 0xe7) === 0x20))) {
    return !!(true);
  }
  if (((opcode === 0xdd) || (opcode === 0xfd))) {
    return !!(true);
  }
  if ((opcode === 0xed)) {
    return !!(true);
  }
  if ((opcode === 0xcb)) {
    return !!(true);
  }
  if (((((((((opcode === 0x07) || (opcode === 0x0f)) || (opcode === 0x17)) || (opcode === 0x1f)) || (opcode === 0x27)) || (opcode === 0x2f)) || (opcode === 0x37)) || (opcode === 0x3f))) {
    return !!(true);
  }
  if ((((opcode === 0xce) || (opcode === 0xde)) || (opcode === 0xdb))) {
    return !!(true);
  }
  if (((((((((opcode === 0x02) || (opcode === 0x0a)) || (opcode === 0x12)) || (opcode === 0x1a)) || (opcode === 0x22)) || (opcode === 0x2a)) || (opcode === 0x32)) || (opcode === 0x3a))) {
    return !!(true);
  }
  if (((((((opcode === 0x08) || (opcode === 0xd9)) || (opcode === 0xe3)) || (opcode === 0xe9)) || (opcode === 0xeb)) || (opcode === 0xf9))) {
    return !!(true);
  }
  if (((opcode & 0xcf) === 0x09)) {
    return !!(true);
  }
  if (((opcode & 0xc7) === 0xc0)) {
    return !!(true);
  }
  if (((opcode & 0xc7) === 0xc2)) {
    return !!(true);
  }
  if (((opcode & 0xc7) === 0xc4)) {
    return !!(true);
  }
  if (((opcode & 0xc7) === 0xc7)) {
    return !!(true);
  }
  if (((opcode & 0xcf) === 0xc1)) {
    return !!(true);
  }
  if (((opcode & 0xcf) === 0xc5)) {
    return !!(true);
  }
  switch (opcode) {
    case 0x00:
    case 0x01:
    case 0x06:
    case 0x0e:
    case 0x11:
    case 0x16:
    case 0x18:
    case 0x1e:
    case 0x21:
    case 0x26:
    case 0x2e:
    case 0x31:
    case 0x36:
    case 0x3e:
    case 0xc3:
    case 0xc6:
    case 0xc9:
    case 0xcd:
    case 0xd3:
    case 0xd6:
    case 0xe6:
    case 0xee:
    case 0xf3:
    case 0xf6:
    case 0xfb:
    case 0xfe:
      return !!(true);
    default:
      return !!(false);
  }
}

function z80_set_irq(cpu, asserted) {
  cpu.irq_line = !!(asserted);
}

function z80_pulse_nmi(cpu) {
  cpu.nmi_pending = !!(true);
}

function z80_step(cpu) {
  if (cpu.nmi_pending) {
    return ((service_nmi(cpu)) >>> 0);
  }
  if (((cpu.irq_line && cpu.iff1) && (cpu.ei_delay === 0))) {
    return ((service_irq(cpu)) >>> 0);
  }
  if (cpu.halted) {
    increment_r(cpu);
    return ((finish_step(cpu, ((4) >>> 0), !!(false))) >>> 0);
  }
  let opcode = ((memory_read(cpu, cpu.pc)) & 255);
  if ((!(opcode_is_supported(opcode)))) {
    return ((0) >>> 0);
  }
  (cpu.pc = ((cpu.pc + 1) & 65535));
  increment_r(cpu);
  if (((opcode === 0xdd) || (opcode === 0xfd))) {
    return ((execute_index(cpu, !!((opcode === 0xfd)))) >>> 0);
  }
  if ((opcode === 0xcb)) {
    return ((execute_cb(cpu)) >>> 0);
  }
  if ((opcode === 0xed)) {
    return ((execute_ed(cpu)) >>> 0);
  }
  if (((opcode & 0xcf) === 0x09)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    alu_add_hl(cpu, get_register_pair(cpu, pair));
    return ((finish_step(cpu, ((11) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x03)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    set_register_pair(cpu, pair, (((((get_register_pair(cpu, pair) + 1)) >>> 0)) & 65535));
    return ((finish_step(cpu, ((6) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0x0b)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    set_register_pair(cpu, pair, (((((get_register_pair(cpu, pair) - 1)) >>> 0)) & 65535));
    return ((finish_step(cpu, ((6) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0x04)) {
    let code = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let value = ((read_register_code(cpu, code)) & 255);
    write_register_code(cpu, code, alu_inc(cpu, value));
    return ((finish_step(cpu, ((((code === 6) ? 11 : 4)) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0x05)) {
    let code = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let value = ((read_register_code(cpu, code)) & 255);
    write_register_code(cpu, code, alu_dec(cpu, value));
    return ((finish_step(cpu, ((((code === 6) ? 11 : 4)) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x10)) {
    let displacement = ((fetch_byte(cpu)) & 255);
    (cpu.b = ((cpu.b - 1) & 255));
    if ((cpu.b !== 0)) {
      let offset = ((((displacement < 0x80) ? ((displacement) << 16 >> 16) : (((displacement) << 16 >> 16) - 0x100))) << 16 >> 16);
      cpu.pc = (((((((cpu.pc) | 0) + offset)) & 65535)) & 65535);
      cpu.wz = ((cpu.pc) & 65535);
      return ((finish_step(cpu, ((13) >>> 0), !!(true))) >>> 0);
    }
    return ((finish_step(cpu, ((8) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xe7) === 0x20)) {
    let displacement = ((fetch_byte(cpu)) & 255);
    let condition = ((((((opcode >> 3) & 3)) & 255)) & 255);
    let take = !!(false);
    switch (condition) {
      case 0:
        take = !!(((cpu.f & Z80_FLAG_Z) === 0));
        break;
      case 1:
        take = !!(((cpu.f & Z80_FLAG_Z) !== 0));
        break;
      case 2:
        take = !!(((cpu.f & Z80_FLAG_C) === 0));
        break;
      case 3:
        take = !!(((cpu.f & Z80_FLAG_C) !== 0));
        break;
      default:
        break;
    }
    if (take) {
      let offset = ((((displacement < 0x80) ? ((displacement) << 16 >> 16) : (((displacement) << 16 >> 16) - 0x100))) << 16 >> 16);
      cpu.pc = (((((((cpu.pc) | 0) + offset)) & 65535)) & 65535);
      cpu.wz = ((cpu.pc) & 65535);
      return ((finish_step(cpu, ((12) >>> 0), !!(true))) >>> 0);
    }
    return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
  }
  if ((((opcode >= 0x40) && (opcode <= 0x7f)) && (opcode !== 0x76))) {
    let destination = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let source = (((opcode & 7)) & 255);
    let value = ((read_register_code(cpu, source)) & 255);
    write_register_code(cpu, destination, value);
    return ((finish_step(cpu, (((((source === 6) || (destination === 6)) ? 7 : 4)) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode >= 0x80) && (opcode <= 0xbf))) {
    let operation = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let source = (((opcode & 7)) & 255);
    let value = ((read_register_code(cpu, source)) & 255);
    let old_carry = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 255);
    switch (operation) {
      case 0:
        alu_add(cpu, value, ((0) & 255));
        break;
      case 1:
        alu_add(cpu, value, old_carry);
        break;
      case 2:
        alu_sub(cpu, value, ((0) & 255));
        break;
      case 3:
        alu_sub(cpu, value, old_carry);
        break;
      case 4:
        alu_and(cpu, value);
        break;
      case 5:
        alu_xor(cpu, value);
        break;
      case 6:
        alu_or(cpu, value);
        break;
      case 7:
        alu_cp(cpu, value);
        break;
      default:
        break;
    }
    return ((finish_step(cpu, ((((source === 6) ? 7 : 4)) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x07)) {
    rotate_accumulator_left_circular(cpu);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x0f)) {
    rotate_accumulator_right_circular(cpu);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x17)) {
    rotate_accumulator_left(cpu);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x1f)) {
    rotate_accumulator_right(cpu);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x27)) {
    decimal_adjust_accumulator(cpu);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x2f)) {
    cpu.a = (((((~(cpu.a))) & 255)) & 255);
    cpu.f = ((((((((cpu.f & (((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV) | Z80_FLAG_C)) | (cpu.a & (Z80_FLAG_X | Z80_FLAG_Y))) | Z80_FLAG_H) | Z80_FLAG_N)) & 255)) & 255);
    cpu.q = ((cpu.f) & 255);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x37)) {
    cpu.f = (((((((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV)) | (cpu.a & (Z80_FLAG_X | Z80_FLAG_Y))) | Z80_FLAG_C)) & 255)) & 255);
    cpu.q = ((cpu.f) & 255);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x3f)) {
    let old_carry = !!(((cpu.f & Z80_FLAG_C) !== 0));
    cpu.f = (((((((cpu.f & ((Z80_FLAG_S | Z80_FLAG_Z) | Z80_FLAG_PV)) | (cpu.a & (Z80_FLAG_X | Z80_FLAG_Y))) | (old_carry ? Z80_FLAG_H : Z80_FLAG_C))) & 255)) & 255);
    cpu.q = ((cpu.f) & 255);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xce)) {
    let carry = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 255);
    alu_add(cpu, fetch_byte(cpu), carry);
    return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xde)) {
    let borrow = ((((cpu.f & Z80_FLAG_C) ? 1 : 0)) & 255);
    alu_sub(cpu, fetch_byte(cpu), borrow);
    return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xdb)) {
    let low_port = ((fetch_byte(cpu)) & 255);
    let port = ((((((((cpu.a) & 65535) << 8) | low_port)) & 65535)) & 65535);
    cpu.a = ((io_read(cpu, port)) & 255);
    cpu.wz = (((((((port + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((11) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x02)) {
    let address = ((((((((cpu.b) & 65535) << 8) | cpu.c)) & 65535)) & 65535);
    memory_write(cpu, address, cpu.a);
    return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x0a)) {
    let address = ((((((((cpu.b) & 65535) << 8) | cpu.c)) & 65535)) & 65535);
    cpu.a = ((memory_read(cpu, address)) & 255);
    return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x12)) {
    let address = ((((((((cpu.d) & 65535) << 8) | cpu.e)) & 65535)) & 65535);
    memory_write(cpu, address, cpu.a);
    return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x1a)) {
    let address = ((((((((cpu.d) & 65535) << 8) | cpu.e)) & 65535)) & 65535);
    cpu.a = ((memory_read(cpu, address)) & 255);
    return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x22)) {
    let address = ((fetch_word(cpu)) & 65535);
    memory_write(cpu, address, cpu.l);
    memory_write(cpu, (((((address + 1)) >>> 0)) & 65535), cpu.h);
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((16) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x2a)) {
    let address = ((fetch_word(cpu)) & 65535);
    cpu.l = ((memory_read(cpu, address)) & 255);
    cpu.h = ((memory_read(cpu, (((((address + 1)) >>> 0)) & 65535))) & 255);
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((16) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x32)) {
    let address = ((fetch_word(cpu)) & 65535);
    memory_write(cpu, address, cpu.a);
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((13) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x3a)) {
    let address = ((fetch_word(cpu)) & 65535);
    cpu.a = ((memory_read(cpu, address)) & 255);
    cpu.wz = (((((((address + 1)) >>> 0)) & 65535)) & 65535);
    return ((finish_step(cpu, ((13) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0x08)) {
    let temporary = 0;
    temporary = ((cpu.a) & 255);
    cpu.a = ((cpu.a_alt) & 255);
    cpu.a_alt = ((temporary) & 255);
    temporary = ((cpu.f) & 255);
    cpu.f = ((cpu.f_alt) & 255);
    cpu.f_alt = ((temporary) & 255);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xd9)) {
    let temporary = 0;
    temporary = ((cpu.b) & 255);
    cpu.b = ((cpu.b_alt) & 255);
    cpu.b_alt = ((temporary) & 255);
    temporary = ((cpu.c) & 255);
    cpu.c = ((cpu.c_alt) & 255);
    cpu.c_alt = ((temporary) & 255);
    temporary = ((cpu.d) & 255);
    cpu.d = ((cpu.d_alt) & 255);
    cpu.d_alt = ((temporary) & 255);
    temporary = ((cpu.e) & 255);
    cpu.e = ((cpu.e_alt) & 255);
    cpu.e_alt = ((temporary) & 255);
    temporary = ((cpu.h) & 255);
    cpu.h = ((cpu.h_alt) & 255);
    cpu.h_alt = ((temporary) & 255);
    temporary = ((cpu.l) & 255);
    cpu.l = ((cpu.l_alt) & 255);
    cpu.l_alt = ((temporary) & 255);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xe3)) {
    let memory_low = ((memory_read(cpu, cpu.sp)) & 255);
    let memory_high = ((memory_read(cpu, (((((cpu.sp + 1)) >>> 0)) & 65535))) & 255);
    let old_l = ((cpu.l) & 255);
    let old_h = ((cpu.h) & 255);
    memory_write(cpu, cpu.sp, old_l);
    memory_write(cpu, (((((cpu.sp + 1)) >>> 0)) & 65535), old_h);
    cpu.l = ((memory_low) & 255);
    cpu.h = ((memory_high) & 255);
    cpu.wz = ((get_hl(cpu)) & 65535);
    return ((finish_step(cpu, ((19) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xe9)) {
    cpu.pc = ((get_hl(cpu)) & 65535);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xeb)) {
    let old_d = ((cpu.d) & 255);
    let old_e = ((cpu.e) & 255);
    cpu.d = ((cpu.h) & 255);
    cpu.e = ((cpu.l) & 255);
    cpu.h = ((old_d) & 255);
    cpu.l = ((old_e) & 255);
    return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
  }
  if ((opcode === 0xf9)) {
    cpu.sp = ((get_hl(cpu)) & 65535);
    return ((finish_step(cpu, ((6) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0xc1)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    let value = ((pop_word(cpu)) & 65535);
    set_stack_pair(cpu, pair, value);
    return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xcf) === 0xc5)) {
    let pair = ((((((opcode >> 4) & 3)) & 255)) & 255);
    push_word(cpu, get_stack_pair(cpu, pair));
    return ((finish_step(cpu, ((11) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0xc0)) {
    let condition = ((((((opcode >> 3) & 7)) & 255)) & 255);
    if (condition_true(cpu, condition)) {
      let target = ((pop_word(cpu)) & 65535);
      cpu.pc = ((target) & 65535);
      cpu.wz = ((target) & 65535);
      return ((finish_step(cpu, ((11) >>> 0), !!(true))) >>> 0);
    }
    return ((finish_step(cpu, ((5) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0xc2)) {
    let condition = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let target = ((fetch_word(cpu)) & 65535);
    cpu.wz = ((target) & 65535);
    if (condition_true(cpu, condition)) {
      cpu.pc = ((target) & 65535);
    }
    return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0xc4)) {
    let condition = ((((((opcode >> 3) & 7)) & 255)) & 255);
    let target = ((fetch_word(cpu)) & 65535);
    cpu.wz = ((target) & 65535);
    if (condition_true(cpu, condition)) {
      push_word(cpu, cpu.pc);
      cpu.pc = ((target) & 65535);
      return ((finish_step(cpu, ((17) >>> 0), !!(true))) >>> 0);
    }
    return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
  }
  if (((opcode & 0xc7) === 0xc7)) {
    let target = (((((opcode & 0x38)) & 65535)) & 65535);
    push_word(cpu, cpu.pc);
    cpu.pc = ((target) & 65535);
    cpu.wz = ((target) & 65535);
    return ((finish_step(cpu, ((11) >>> 0), !!(true))) >>> 0);
  }
  switch (opcode) {
    case 0x00:
      return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
    case 0x01:
{
        let value = ((fetch_word(cpu)) & 65535);
        cpu.b = (((((value >> 8)) & 255)) & 255);
        cpu.c = ((((value) & 255)) & 255);
        return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
      }
    case 0x06:
      cpu.b = ((fetch_byte(cpu)) & 255);
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0x0e:
      cpu.c = ((fetch_byte(cpu)) & 255);
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0x11:
{
        let value = ((fetch_word(cpu)) & 65535);
        cpu.d = (((((value >> 8)) & 255)) & 255);
        cpu.e = ((((value) & 255)) & 255);
        return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
      }
    case 0x16:
      cpu.d = ((fetch_byte(cpu)) & 255);
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0x18:
{
        let displacement = ((fetch_byte(cpu)) & 255);
        let offset = ((((displacement < 0x80) ? ((displacement) << 16 >> 16) : (((displacement) << 16 >> 16) - 0x100))) << 16 >> 16);
        cpu.pc = (((((((cpu.pc) | 0) + offset)) & 65535)) & 65535);
        cpu.wz = ((cpu.pc) & 65535);
        return ((finish_step(cpu, ((12) >>> 0), !!(true))) >>> 0);
      }
    case 0x1e:
      cpu.e = ((fetch_byte(cpu)) & 255);
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0x21:
{
        let value = ((fetch_word(cpu)) & 65535);
        cpu.h = (((((value >> 8)) & 255)) & 255);
        cpu.l = ((((value) & 255)) & 255);
        return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
      }
    case 0x26:
      cpu.h = ((fetch_byte(cpu)) & 255);
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0x2e:
      cpu.l = ((fetch_byte(cpu)) & 255);
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0x31:
      cpu.sp = ((fetch_word(cpu)) & 65535);
      return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
    case 0x36:
{
        let value = ((fetch_byte(cpu)) & 255);
        memory_write(cpu, get_hl(cpu), value);
        return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
      }
    case 0x3e:
      cpu.a = ((fetch_byte(cpu)) & 255);
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0x76:
      cpu.halted = !!(true);
      return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
    case 0xc3:
{
        let target = ((fetch_word(cpu)) & 65535);
        cpu.pc = ((target) & 65535);
        cpu.wz = ((target) & 65535);
        return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
      }
    case 0xc6:
      alu_add(cpu, fetch_byte(cpu), ((0) & 255));
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0xc9:
{
        let target = ((pop_word(cpu)) & 65535);
        cpu.pc = ((target) & 65535);
        cpu.wz = ((target) & 65535);
        return ((finish_step(cpu, ((10) >>> 0), !!(true))) >>> 0);
      }
    case 0xcd:
{
        let target = ((fetch_word(cpu)) & 65535);
        push_word(cpu, cpu.pc);
        cpu.pc = ((target) & 65535);
        cpu.wz = ((target) & 65535);
        return ((finish_step(cpu, ((17) >>> 0), !!(true))) >>> 0);
      }
    case 0xd3:
{
        let low_port = ((fetch_byte(cpu)) & 255);
        let port = ((((((((cpu.a) & 65535) << 8) | low_port)) & 65535)) & 65535);
        io_write(cpu, port, cpu.a);
        cpu.wz = (((((((port + 1)) >>> 0)) & 65535)) & 65535);
        return ((finish_step(cpu, ((11) >>> 0), !!(true))) >>> 0);
      }
    case 0xd6:
      alu_sub(cpu, fetch_byte(cpu), ((0) & 255));
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0xe6:
      alu_and(cpu, fetch_byte(cpu));
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0xee:
      alu_xor(cpu, fetch_byte(cpu));
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0xf3:
      cpu.iff1 = !!(false);
      cpu.iff2 = !!(false);
      cpu.ei_delay = ((0) & 255);
      return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
    case 0xf6:
      alu_or(cpu, fetch_byte(cpu));
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    case 0xfb:
      cpu.iff1 = !!(true);
      cpu.iff2 = !!(true);
      cpu.ei_delay = ((2) & 255);
      return ((finish_step(cpu, ((4) >>> 0), !!(true))) >>> 0);
    case 0xfe:
      alu_cp(cpu, fetch_byte(cpu));
      return ((finish_step(cpu, ((7) >>> 0), !!(true))) >>> 0);
    default:
      return ((0) >>> 0);
  }
}

function z80_reset(cpu) {
  cpu.a = 0;
  cpu.f = 0;
  cpu.b = 0;
  cpu.c = 0;
  cpu.d = 0;
  cpu.e = 0;
  cpu.h = 0;
  cpu.l = 0;
  cpu.a_alt = 0;
  cpu.f_alt = 0;
  cpu.b_alt = 0;
  cpu.c_alt = 0;
  cpu.d_alt = 0;
  cpu.e_alt = 0;
  cpu.h_alt = 0;
  cpu.l_alt = 0;
  cpu.ix = 0;
  cpu.iy = 0;
  cpu.sp = 0;
  cpu.pc = 0;
  cpu.i = 0;
  cpu.r = 0;
  cpu.iff1 = false;
  cpu.iff2 = false;
  cpu.interrupt_mode = 0;
  cpu.halted = false;
  cpu.irq_line = false;
  cpu.nmi_pending = false;
  cpu.ei_delay = 0;
  cpu.wz = 0;
  cpu.q = 0;
  cpu.total_t_states = 0;
}
function z80_init(cpu, bus) { cpu.bus = bus; z80_reset(cpu); }
const PARITY = new Uint8Array(256); const SZXY = new Uint8Array(256);
for(let i=0;i<256;i++){let p=i; p ^= p>>4; p ^= p>>2; p ^= p>>1; PARITY[i] = (p & 1)^1; SZXY[i]=(i & 168)|(i===0?64:0);}
function parity_even(value){return PARITY[value];}
function result_szxy_flags(value){return SZXY[value];}

// Hardware clocks, memory decode and WSG are instruction-synchronous.
const CPU_HZ = 3072000, FRAME_CYCLES = 50688, VBLANK_CYCLES = 43008;
const FRAME_SECONDS = FRAME_CYCLES / CPU_HZ, SAMPLE_RATE = 48000;
const WIDTH = 224, HEIGHT = 288;
function crc32(bytes) {
  let c = -1;
  for (let i = 0; i < bytes.length; i++) {
    c ^= bytes[i];
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
  }
  return (c ^ -1) >>> 0;
}
function decodeROMs() {
  const text = atob(ROM_BASE64);
  const bytes = new Uint8Array(text.length);
  for (let i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i);
  const roms = Object.create(null);
  for (const r of ROM_MANIFEST) {
    const data = bytes.subarray(r.offset, r.offset + r.size);
    if (data.length !== r.size || crc32(data) !== r.crc32) throw new Error('ROM dañada: ' + r.name);
    roms[r.name] = data;
  }
  return {bytes, roms};
}
const ROM_SET = decodeROMs();
class Machine {
  constructor() {
    this.rom = ROM_SET.bytes.subarray(0, 16384);
    this.memory = new Uint8Array(65536);
    this.dirty = new Uint8Array(1024);
    this.coordinates = new Uint8Array(16);
    this.sound = new Uint8Array(32);
    this.pending = new Uint8Array(32);
    this.frequency = new Uint32Array(3);
    this.phase = new Uint32Array(3);
    this.wave = new Uint8Array(3);
    this.volume = new Uint8Array(3);
    this.waveform = new Int8Array(256);
    for (let i = 0; i < 256; i++) this.waveform[i] = (ROM_SET.roms['82s126.1m'][i] & 15) - 8;
    this.audio = new Int16Array(2048);
    this.cpu = {};
    const bus = {
      context: this, memory_read: boardRead, memory_write: boardWrite,
      io_read: boardIoRead, io_write: boardIoWrite,
      interrupt_acknowledge: boardAcknowledge, reti: boardReti
    };
    z80_init(this.cpu, bus);
    this.reset();
  }
  reset() {
    z80_reset(this.cpu);
    this.memory.fill(0); this.dirty.fill(1); this.coordinates.fill(0);
    this.sound.fill(0); this.pending.fill(0); this.frequency.fill(0);
    this.phase.fill(0); this.wave.fill(0); this.volume.fill(0);
    this.input0 = this.input1 = this.dsw2 = 255;
    this.dsw1 = 0xc9; // Same DIP switches as pacman-sim in the supplied project.
    this.latch = this.vector = this.raster = this.frame = 0;
    this.vblank = this.enabled = this.flip = false;
    this.pendingMask = this.audioCycles = this.decimationCount = this.decimationSum = 0;
    this.audioCount = this.watchdogKicks = 0;
  }
  applySoundWrites() {
    let mask = this.pendingMask;
    while (mask) {
      const i = 31 - Math.clz32(mask & -mask);
      mask &= mask - 1;
      this.sound[i] = this.pending[i] & 15;
    }
    const r = this.sound;
    this.wave[0] = (r[5] & 7) << 5;
    this.wave[1] = (r[10] & 7) << 5;
    this.wave[2] = (r[15] & 7) << 5;
    this.volume[0] = r[21]; this.volume[1] = r[26]; this.volume[2] = r[31];
    this.frequency[0] = r[16] | r[17] << 4 | r[18] << 8 | r[19] << 12 | r[20] << 16;
    this.frequency[1] = r[22] << 4 | r[23] << 8 | r[24] << 12 | r[25] << 16;
    this.frequency[2] = r[27] << 4 | r[28] << 8 | r[29] << 12 | r[30] << 16;
    this.pendingMask = 0;
  }
  sample() {
    let mix = 0;
    if (this.enabled) {
      const phase = this.phase, vol = this.volume, freq = this.frequency, wave = this.wave;
      for (let i = 0; i < 3; i++) {
        if (vol[i]) {
          mix += this.waveform[wave[i] | ((phase[i] >>> 16) & 31)] * vol[i];
          phase[i] = (phase[i] + freq[i]) >>> 0;
        }
      }
    }
    // Match the two integer truncations in the validated PCM16 C host.
    this.decimationSum += (mix * 32767 / 384) | 0;
    if (++this.decimationCount === 4) {
      this.audio[this.audioCount++] = (this.decimationSum / 4) | 0;
      this.decimationCount = this.decimationSum = 0;
    }
  }
  step() {
    const t = z80_step(this.cpu);
    if (!t) throw new Error('Z80: instrucción no implementada en PC=0x' + this.cpu.pc.toString(16));
    this.raster += t;
    if (this.raster >= FRAME_CYCLES) { this.raster -= FRAME_CYCLES; this.frame++; }
    this.audioCycles += t;
    while (this.audioCycles >= 16) { this.audioCycles -= 16; this.sample(); }
    // Critical ordering: emit samples for the instruction BEFORE register writes.
    if (this.pendingMask) this.applySoundWrites();
    const vb = this.raster >= VBLANK_CYCLES;
    if (vb && !this.vblank && (this.latch & 1)) this.cpu.irq_line = true;
    this.vblank = vb;
    return t;
  }
  runFrame() {
    this.audioCount = 0;
    const next = this.frame + 1;
    while (this.frame < next) this.step();
    return this.audioCount;
  }
}
function boardRead(m, a) {
  let d = a & 0x7fff;
  if (d < 0x4000) return m.rom[d];
  d = a & 0x5fff;
  if ((d >= 0x4000 && d < 0x4800) || (d >= 0x4c00 && d <= 0x4fff)) return m.memory[d];
  switch (a & 0x50c0) {
    case 0x5000: return m.input0;
    case 0x5040: return m.input1;
    case 0x5080: return m.dsw1;
    case 0x50c0: return m.dsw2;
    default: return 0xbf;
  }
}
function boardWrite(m, a, v) {
  if ((a & 0x7fff) < 0x4000) return;
  let d = a & 0x5fff;
  if (d >= 0x4000 && d < 0x4800) {
    if (m.memory[d] !== v) { m.memory[d] = v; m.dirty[d & 1023] = 1; }
    return;
  }
  if (d >= 0x4800 && d < 0x4c00) return;
  if (d >= 0x4c00 && d <= 0x4fff) { m.memory[d] = v; return; }
  d = a & 0x50c7;
  if (d >= 0x5000 && d <= 0x5007) {
    const bit = d & 7, on = v & 1;
    m.latch = (m.latch & ~(1 << bit)) | (on << bit);
    if (bit === 0 && !on) m.cpu.irq_line = false;
    else if (bit === 1) m.enabled = !!on;
    else if (bit === 3) m.flip = !!on;
    return;
  }
  d = a & 0x50ff;
  if (d >= 0x5040 && d <= 0x505f) {
    d -= 0x5040; m.pending[d] = v; m.pendingMask |= 1 << d; return;
  }
  if (d >= 0x5060 && d <= 0x506f) { m.coordinates[d - 0x5060] = v; return; }
  if ((a & 0x50c0) === 0x50c0) m.watchdogKicks++;
}
function boardIoRead() { return 255; }
function boardIoWrite(m, port, value) { if (!(port & 255)) m.vector = value; }
function boardAcknowledge(m) { return m.vector; }
function boardReti() {}

class Renderer {
  constructor(machine) {
    this.m = machine;
    this.background = new Uint32Array(288 * 224);
    this.native = new Uint32Array(288 * 224);
    this.pixels = new Uint32Array(WIDTH * HEIGHT);
    this.tilePixels = new Uint8Array(256 * 64);
    this.spritePixels = new Uint8Array(64 * 256);
    this.coloredTiles = new Uint32Array(256 * 32 * 64);
    this.palette = new Uint32Array(32);
    this.colors = new Uint32Array(128);
    this.lookup = new Uint8Array(128);
    this.tileX = new Int16Array(1024); this.tileY = new Int16Array(1024);
    this.tileX.fill(-1); this.tileY.fill(-1);
    const roms = ROM_SET.roms;
    const little = new Uint8Array(new Uint32Array([0x01020304]).buffer)[0] === 4;
    for (let i = 0; i < 32; i++) {
      const v = roms['82s123.7f'][i];
      const r = (v & 1) * 33 + ((v >> 1) & 1) * 71 + ((v >> 2) & 1) * 151;
      const g = ((v >> 3) & 1) * 33 + ((v >> 4) & 1) * 71 + ((v >> 5) & 1) * 151;
      const b = ((v >> 6) & 1) * 81 + ((v >> 7) & 1) * 174;
      this.palette[i] = little ? (0xff000000 | b << 16 | g << 8 | r) >>> 0 : (r << 24 | g << 16 | b << 8 | 255) >>> 0;
    }
    for (let i = 0; i < 128; i++) {
      this.lookup[i] = roms['82s126.4a'][i] & 15;
      this.colors[i] = this.palette[this.lookup[i]];
    }
    for (let tile = 0; tile < 256; tile++) {
      for (let y = 0; y < 8; y++) for (let x = 0; x < 8; x++) {
        const v = roms['pacman.5e'][tile * 16 + (x < 4 ? 8 : 0) + y], bit = x & 3;
        const p = ((v >>> (7 - bit)) & 1) << 1 | ((v >>> (3 - bit)) & 1);
        this.tilePixels[tile * 64 + y * 8 + x] = p;
        for (let color = 0; color < 32; color++) this.coloredTiles[(tile * 32 + color) * 64 + y * 8 + x] = this.colors[color * 4 + p];
      }
    }
    for (let sprite = 0; sprite < 64; sprite++) {
      for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
        const v = roms['pacman.5f'][sprite * 64 + ((x + 4) & 15 & 12) * 2 + (y < 8 ? y : y + 24)];
        const bit = x & 3;
        this.spritePixels[sprite * 256 + y * 16 + x] = ((v >>> (7 - bit)) & 1) << 1 | ((v >>> (3 - bit)) & 1);
      }
    }
    for (let ty = 0; ty < 28; ty++) for (let tx = 0; tx < 36; tx++) {
      const row = ty + 2, col = tx - 2;
      const offset = (col & 32) ? row + ((col & 31) << 5) : col + (row << 5);
      this.tileX[offset] = tx * 8; this.tileY[offset] = ty * 8;
    }
    this.background.fill(this.palette[0]);
  }
  render() {
    const m = this.m, mem = m.memory, bg = this.background, cache = this.coloredTiles;
    for (let i = 0; i < 1024; i++) {
      if (!m.dirty[i]) continue;
      m.dirty[i] = 0;
      const x = this.tileX[i], y = this.tileY[i];
      if (x < 0) continue;
      let src = (mem[0x4000 + i] * 32 + (mem[0x4400 + i] & 31)) * 64;
      let dst = y * 288 + x;
      for (let row = 0; row < 8; row++, dst += 288, src += 8) {
        bg[dst] = cache[src]; bg[dst+1] = cache[src+1]; bg[dst+2] = cache[src+2]; bg[dst+3] = cache[src+3];
        bg[dst+4] = cache[src+4]; bg[dst+5] = cache[src+5]; bg[dst+6] = cache[src+6]; bg[dst+7] = cache[src+7];
      }
    }
    const fb = this.native;
    if (m.flip) { for (let i = 0, j = fb.length - 1; j >= 0; i++, j--) fb[i] = bg[j]; }
    else fb.set(bg);
    for (let offset = 14; offset >= 0; offset -= 2) {
      const attr = mem[0x4ff0 + offset], color = (mem[0x4ff1 + offset] & 31) << 2;
      const sprite = (attr >> 2) * 256;
      const sx = 272 - m.coordinates[offset + 1], sy = m.coordinates[offset] - 31 + (offset <= 4 ? 1 : 0);
      this.sprite(sprite, color, sx, sy, attr & 1, attr & 2);
      this.sprite(sprite, color, sx - 256, sy, offset <= 4 ? attr & 2 : attr & 1, offset <= 4 ? attr & 1 : attr & 2);
    }
    let out = 0;
    for (let x = 0; x < 288; x++) for (let y = 223; y >= 0; y--) this.pixels[out++] = fb[y * 288 + x];
    return this.pixels;
  }
  sprite(base, color, sx, sy, flipX, flipY) {
    const left = Math.max(16, sx), right = Math.min(272, sx + 16);
    const top = Math.max(0, sy), bottom = Math.min(224, sy + 16);
    if (left >= right || top >= bottom) return;
    const gfx = this.spritePixels, fb = this.native, colors = this.colors, lookup = this.lookup;
    for (let y = top; y < bottom; y++) {
      const row = base + (flipY ? 15 - (y - sy) : y - sy) * 16;
      for (let x = left; x < right; x++) {
        const p = color | gfx[row + (flipX ? 15 - (x - sx) : x - sx)];
        if (lookup[p]) fb[y * 288 + x] = colors[p];
      }
    }
  }
}

// Embedded AudioWorklet. Blob URLs are local, temporary and revoked after loading.
const WORKLET_SOURCE = `
class PacmanPCM extends AudioWorkletProcessor {
  constructor() {
    super(); this.ring=new Int16Array(32768); this.r=0; this.w=0; this.n=0;
    this.frac=0; this.ratio=48000/sampleRate; this.playing=false;
    this.startAt=0; this.under=0; this.over=0; this.blocks=0;
    this.port.onmessage=({data:d})=>{
      if(d.type==='reset') {
        this.r=this.w=this.n=0; this.frac=0; this.startAt=d.frame; this.playing=true;
      } else if(d.type==='stop') { this.playing=false; this.r=this.w=this.n=0; }
      else if(d.type==='pcm') {
        const a=new Int16Array(d.buffer);
        if(this.n+d.count>32768){this.over++; this.r=this.w=this.n=0;this.frac=0;}
        for(let i=0;i<d.count;i++){this.ring[this.w]=a[i];this.w=(this.w+1)&32767;}
        this.n+=d.count;
        this.port.postMessage({type:'recycle',buffer:d.buffer},[d.buffer]);
      }
    };
  }
  process(inputs,outputs) {
    const out=outputs[0][0]; if(!out)return true;
    let starved=false;
    for(let i=0;i<out.length;i++) {
      if(!this.playing||currentFrame+i<this.startAt){out[i]=0;continue;}
      if(this.ratio===1) {
        if(!this.n){out[i]=0;starved=true;continue;}
        out[i]=this.ring[this.r]/32768;this.r=(this.r+1)&32767;this.n--;
      } else {
        if(this.n<Math.ceil(this.ratio)+1){out[i]=0;starved=true;continue;}
        const a=this.ring[this.r],b=this.ring[(this.r+1)&32767];
        out[i]=(a+(b-a)*this.frac)/32768;
        this.frac+=this.ratio;
        const take=Math.floor(this.frac);this.frac-=take;this.r=(this.r+take)&32767;this.n-=take;
      }
    }
    if(starved)this.under++;
    if(++this.blocks===128){this.blocks=0;this.port.postMessage({type:'stats',queued:this.n,underruns:this.under,overruns:this.over});}
    return true;
  }
}
registerProcessor('pacman-pcm-v1',PacmanPCM);
`;
class AudioEngine {
  constructor(volume) {
    this.context = null; this.node = null; this.gain = null;
    this.mode = 'pendiente'; this.volume = volume; this.muted = false;
    this.pool = []; this.sources = new Set(); this.pendingPause = null;
    this.stats = {queued: 0, underruns: 0, overruns: 0};
    this.nextTime = 0; this.failure = ''; this.initializing = null;
  }
  init() {
    if (this.initializing) return this.initializing;
    this.initializing = this.initialize();
    return this.initializing;
  }
  async initialize() {
    const AC = globalThis.AudioContext || globalThis.webkitAudioContext;
    if (!AC) { this.mode = 'sin audio'; return false; }
    try {
      try { this.context = new AC({sampleRate: SAMPLE_RATE, latencyHint: 'interactive'}); }
      catch (_) { this.context = new AC(); }
      // Called directly from a click/keydown: do not defer the initial resume.
      const resume = this.context.resume();
      this.gain = this.context.createGain();
      this.gain.gain.value = this.muted ? 0 : this.volume;
      this.gain.connect(this.context.destination);
      await resume;
      this.mode = 'Web Audio';
      if (this.context.audioWorklet && typeof AudioWorkletNode !== 'undefined') {
        let url = null;
        try {
          url = URL.createObjectURL(new Blob([WORKLET_SOURCE], {type: 'application/javascript'}));
          await this.context.audioWorklet.addModule(url);
          this.node = new AudioWorkletNode(this.context, 'pacman-pcm-v1', {
            numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [1]
          });
          this.node.connect(this.gain);
          this.node.port.onmessage = ({data}) => {
            if (data.type === 'recycle') { if (this.pool.length < 12) this.pool.push(data.buffer); }
            else if (data.type === 'stats') this.stats = data;
          };
          this.node.onprocessorerror = () => {
            this.failure = 'El procesador de audio se detuvo.';
            this.node.disconnect(); this.node = null; this.mode = 'Web Audio';
            this.nextTime = this.context.currentTime + 0.03;
          };
          this.mode = 'AudioWorklet';
          for (let i = 0; i < 6; i++) this.pool.push(new ArrayBuffer(4096));
        } catch (e) {
          this.failure = e.message || String(e);
          if (this.node) this.node.disconnect();
          this.node = null;
          // File pages and restrictive CSPs still have scheduled Web Audio.
        } finally { if (url) URL.revokeObjectURL(url); }
      }
      return true;
    } catch (e) {
      this.failure = e.message || String(e); this.mode = 'sin audio';
      if (this.context) { try { await this.context.close(); } catch (_) {} }
      this.context = null;
      return false;
    }
  }
  async resume() {
    await this.init();
    if (this.pendingPause) { await this.pendingPause; this.pendingPause = null; }
    if (this.context && this.context.state !== 'running') await this.context.resume();
  }
  clearSources() {
    for (const source of this.sources) {
      source.onended = null;
      try { source.stop(); } catch (_) {}
      source.disconnect();
    }
    this.sources.clear();
  }
  arm() {
    if (!this.context) return 0;
    this.clearSources();
    const frame = Math.ceil((this.context.currentTime + 0.03) * this.context.sampleRate);
    this.nextTime = frame / this.context.sampleRate;
    if (this.node) this.node.port.postMessage({type: 'reset', frame});
    return this.nextTime;
  }
  write(pcm, n) {
    if (!this.context || !n) return;
    if (this.node) {
      const buffer = this.pool.pop() || new ArrayBuffer(4096);
      const copy = new Int16Array(buffer);
      for (let i = 0; i < n; i++) copy[i] = pcm[i];
      this.node.port.postMessage({type: 'pcm', buffer, count: n}, [buffer]);
    } else {
      const buffer = this.context.createBuffer(1, n, SAMPLE_RATE);
      const data = buffer.getChannelData(0);
      for (let i = 0; i < n; i++) data[i] = pcm[i] / 32768;
      const source = this.context.createBufferSource();
      source.buffer = buffer; source.connect(this.gain);
      if (this.nextTime < this.context.currentTime) {
        this.nextTime = this.context.currentTime + 0.015; this.stats.underruns++;
      }
      this.sources.add(source);
      source.onended = () => { this.sources.delete(source); source.disconnect(); };
      source.start(this.nextTime); this.nextTime += n / SAMPLE_RATE;
    }
  }
  setVolume(value) {
    this.volume = Math.max(0, Math.min(1, Number.isFinite(+value) ? +value : 0.65));
    if (this.gain) this.gain.gain.setTargetAtTime(this.muted ? 0 : this.volume, this.context.currentTime, 0.012);
  }
  setMuted(value) { this.muted = !!value; this.setVolume(this.volume); }
  pause() {
    this.clearSources();
    if (this.node) this.node.port.postMessage({type: 'stop'});
    if (this.context && this.context.state !== 'closed') this.pendingPause = this.context.suspend().catch(() => {});
  }
  dispose() {
    this.pause();
    if (this.node) { this.node.port.onmessage = null; this.node.disconnect(); }
    if (this.gain) this.gain.disconnect();
    if (this.context && this.context.state !== 'closed') this.context.close().catch(() => {});
  }
}

const APP_STYLE = "__PACMAN_APP_CSS__";
const APP_HTML = `
<main class="shell">
  <header><div><h1>Pac-Man</h1><div class="byline">arcade-core · ROM original</div></div>
    <div class="tools"><button class="icon" data-action="mute" title="Silenciar (M)" aria-label="Silenciar audio" aria-pressed="false">♪</button><button class="icon" data-action="fullscreen" title="Pantalla completa (F)" aria-label="Pantalla completa">⛶</button></div>
  </header>
  <div class="screen"><canvas width="224" height="288" aria-label="Pantalla del juego Pac-Man"></canvas>
    <div class="overlay"><div class="card"><h2>Iniciando Pac-Man</h2><p>Comprobando y ejecutando las ROMs integradas.</p><button class="primary" data-action="play" disabled>Iniciando…</button><small>JavaScript puro · sin conexiones externas</small></div></div>
  </div>
  <div class="actions"><button data-action="coin">Moneda <kbd>5</kbd></button><button data-action="start1">1 jugador <kbd>1</kbd></button><button data-action="start2">2 jugadores <kbd>2</kbd></button><button data-action="pause">Pausa <kbd>P</kbd></button><button data-action="reset" title="Reiniciar la máquina">Reiniciar</button></div>
  <div class="lower"><nav class="touch" aria-label="Dirección"><button class="up" data-direction="1" aria-label="Arriba">↑</button><button class="left" data-direction="2" aria-label="Izquierda">←</button><button class="down" data-direction="8" aria-label="Abajo">↓</button><button class="right" data-direction="4" aria-label="Derecha">→</button></nav><label class="volume">Volumen <input type="range" min="0" max="100" value="65" aria-label="Volumen del juego"></label></div>
  <div class="instructions"><kbd>↑ ← ↓ →</kbd> o <kbd>W A S D</kbd> para mover · <kbd>5</kbd> moneda · <kbd>1 / 2</kbd> comenzar<br><kbd>P</kbd> pausa · <kbd>M</kbd> sonido · <kbd>F</kbd> pantalla completa</div>
  <div class="stats" aria-label="Estado de la emulación">Z80 3,072 MHz · 60,606 Hz · audio 48 kHz</div>
  <div class="message" role="status" aria-live="polite"></div>
</main>`;

// Screen-only mode creates no toolbar, buttons, overlay, or performance panel.
const SCREEN_STYLE = "__PACMAN_SCREEN_CSS__";
const SCREEN_HTML = `
<div class="screen">
  <canvas width="224" height="288" tabindex="0" aria-label="Pac-Man. Clic o Enter para jugar. Flechas o WASD para mover. P para pausar."></canvas>
  <div class="message" role="status" aria-live="polite"></div>
</div>`;

class PacmanApp {
  constructor(host, options = {}) {
    this.host = host; this.options = options; this.destroyed = false; this.running = false;
    this.screenOnly = options.screenOnly === true;
    this.booted = false; this.hasPlayed = false; this.transition = 0; this.startPromise = null;
    this.m = new Machine(); this.renderer = new Renderer(this.m);
    const volume = Number.isFinite(options.volume) ? Math.max(0, Math.min(1, options.volume)) : 0.65;
    this.audio = new AudioEngine(volume);
    this.shadow = host.attachShadow({mode: 'open'});
    if (options.stylesheetHref) {
      const stylesheet = document.createElement('link');
      stylesheet.rel = 'stylesheet'; stylesheet.href = options.stylesheetHref;
      this.shadow.appendChild(stylesheet);
    } else {
      const style = document.createElement('style'); style.textContent = this.screenOnly ? SCREEN_STYLE : APP_STYLE;
      this.shadow.appendChild(style);
    }
    const content = document.createElement('div'); content.innerHTML = this.screenOnly ? SCREEN_HTML : APP_HTML;
    this.shadow.appendChild(content);
    this.q = s => this.shadow.querySelector(s);
    this.canvas = this.q('canvas'); this.ctx = this.canvas.getContext('2d', {alpha: false, desynchronized: true});
    if (!this.ctx) throw new Error('Canvas 2D no está disponible en este navegador.');
    this.ctx.imageSmoothingEnabled = false;
    this.image = new ImageData(new Uint8ClampedArray(this.renderer.pixels.buffer), WIDTH, HEIGHT);
    this.directions = new Map(); this.directionBit = 0;
    this.coinFrames = this.start1Frames = this.start2Frames = 0; this.startDelay = -1;
    this.epoch = this.produced = this.drawnFrame = 0;
    this.frameMs = this.renderMs = this.emulatedSinceStats = 0;
    this.resyncs = 0; this.statsTime = performance.now();
    this.abort = new AbortController(); this.events = {signal: this.abort.signal};
    const volumeInput = this.q('input');
    if (volumeInput) volumeInput.value = String(Math.round(volume * 100));
    this.bindEvents();
    this.ready = new Promise(resolve => { this.readyResolve = resolve; });
    this.bootBatch();
  }
  bindEvents() {
    this.shadow.addEventListener('click', event => {
      const button = event.target.closest('button[data-action]');
      if (!button || button.disabled || this.destroyed) return;
      const action = button.dataset.action;
      if (action === 'play') { if (this.hasPlayed) this.resume(); else this.play(); }
      else if (action === 'coin') this.insertCoin();
      else if (action === 'start1') this.startPlayer(1);
      else if (action === 'start2') this.startPlayer(2);
      else if (action === 'pause') this.togglePause();
      else if (action === 'reset') this.reset();
      else if (action === 'mute') this.toggleMute();
      else if (action === 'fullscreen') this.fullscreen();
      if (action !== 'fullscreen') button.blur();
    }, this.events);
    const volumeInput = this.q('input');
    if (volumeInput) volumeInput.addEventListener('input', e => this.audio.setVolume(+e.target.value / 100), this.events);
    if (this.screenOnly) this.bindScreenEvents();
    document.addEventListener('keydown', e => this.key(e, true), this.events);
    document.addEventListener('keyup', e => this.key(e, false), this.events);
    window.addEventListener('blur', () => this.releaseDirections(), this.events);
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) { this.releaseDirections(); if (this.running) this.pause('Pausa al cambiar de pestaña'); }
    }, this.events);
    for (const button of this.shadow.querySelectorAll('[data-direction]')) {
      const release = e => {
        this.directions.delete('pointer' + e.pointerId); this.updateDirection(); button.classList.remove('held');
      };
      button.addEventListener('pointerdown', e => {
        if (e.button !== 0) return;
        e.preventDefault(); button.setPointerCapture(e.pointerId);
        this.directions.set('pointer' + e.pointerId, +button.dataset.direction);
        this.updateDirection(); button.classList.add('held');
      }, this.events);
      button.addEventListener('pointerup', release, this.events);
      button.addEventListener('pointercancel', release, this.events);
      button.addEventListener('lostpointercapture', release, this.events);
      button.addEventListener('contextmenu', e => e.preventDefault(), this.events);
    }
  }
  bindScreenEvents() {
    // A primary click starts once, or resumes a pause; playing clicks only focus.
    const activate = () => {
      this.canvas.focus({preventScroll: true});
      if (!this.booted || this.destroyed) return;
      if (!this.hasPlayed) this.play();
      else if (!this.running) this.resume();
    };
    this.canvas.addEventListener('click', activate, this.events);
    let gesture = null;
    this.canvas.addEventListener('pointerdown', event => {
      if (!event.isPrimary || event.button !== 0) return;
      if (event.pointerType === 'mouse') return;
      event.preventDefault();
      this.canvas.setPointerCapture(event.pointerId);
      gesture = {id: event.pointerId, x: event.clientX, y: event.clientY};
      activate();
    }, this.events);
    this.canvas.addEventListener('pointermove', event => {
      if (!gesture || gesture.id !== event.pointerId) return;
      const dx = event.clientX - gesture.x, dy = event.clientY - gesture.y;
      if (Math.max(Math.abs(dx), Math.abs(dy)) < 8) return;
      const bit = Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 4 : 2) : (dy > 0 ? 8 : 1);
      this.directions.set('screenPointer', bit); this.updateDirection();
      gesture.x = event.clientX; gesture.y = event.clientY;
    }, this.events);
    const release = event => {
      if (!gesture || gesture.id !== event.pointerId) return;
      gesture = null; this.directions.delete('screenPointer'); this.updateDirection();
    };
    for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) {
      this.canvas.addEventListener(name, release, this.events);
    }
    this.canvas.addEventListener('contextmenu', event => event.preventDefault(), this.events);
  }
  key(event, down) {
    if (!this.booted || this.destroyed) return;
    // Do not capture another widget's controls when embedded into a larger page.
    if (this.screenOnly && document.activeElement !== this.host && document.activeElement !== document.body) return;
    const tag = event.target.tagName;
    const innerTag = this.shadow.activeElement && this.shadow.activeElement.tagName;
    if (event.target.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(tag) || /^(INPUT|TEXTAREA|SELECT)$/.test(innerTag)) return;
    if (event.ctrlKey || event.metaKey || event.altKey) return;
    const code = event.code;
    const dirs = {ArrowUp: 1, KeyW: 1, ArrowLeft: 2, KeyA: 2, ArrowRight: 4, KeyD: 4, ArrowDown: 8, KeyS: 8};
    if (dirs[code]) {
      event.preventDefault();
      if (down && !event.repeat) this.directions.set(code, dirs[code]);
      else if (!down) this.directions.delete(code);
      this.updateDirection(); return;
    }
    if (!['Digit5', 'Numpad5', 'KeyC', 'Space', 'Digit1', 'Numpad1', 'Digit2', 'Numpad2', 'Enter', 'KeyP', 'Escape', 'KeyM', 'KeyF'].includes(code)) return;
    event.preventDefault();
    if (!down || event.repeat) return;
    if (['Digit5', 'Numpad5', 'KeyC', 'Space'].includes(code)) this.insertCoin();
    else if (['Digit1', 'Numpad1'].includes(code)) this.startPlayer(1);
    else if (['Digit2', 'Numpad2'].includes(code)) this.startPlayer(2);
    else if (code === 'Enter') { if (!this.hasPlayed) this.play(); else if (!this.running) this.resume(); else this.startPlayer(1); }
    else if (code === 'KeyP' || code === 'Escape') this.togglePause();
    else if (code === 'KeyM') this.toggleMute();
    else if (code === 'KeyF') this.fullscreen();
  }
  updateDirection() { this.directionBit = 0; for (const bit of this.directions.values()) this.directionBit = bit; }
  releaseDirections() {
    this.directions.clear(); this.directionBit = 0;
    for (const button of this.shadow.querySelectorAll('.held')) button.classList.remove('held');
  }
  bootBatch() {
    if (this.destroyed) return;
    try {
      const end = Math.min(360, this.m.frame + 40);
      while (this.m.frame < end) this.m.runFrame();
      if (this.m.frame < 360) { this.bootTimer = setTimeout(() => this.bootBatch(), 0); return; }
      this.booted = true; this.render();
      this.overlay('El Pac-Man original', 'Las ROMs están listas. Jugar añade una moneda e inicia una partida.', 'Jugar');
      this.readyResolve(this); this.readyResolve = () => {};
    } catch (e) { this.fail(e); this.readyResolve(this); }
  }
  overlay(title, text, label) {
    if (this.screenOnly) { this.notify(title + '. ' + text); return; }
    this.q('.card h2').textContent = title; this.q('.card p').textContent = text;
    const button = this.q('[data-action=play]'); button.textContent = label; button.disabled = !this.booted;
    this.q('.overlay').hidden = false;
  }
  async resume() {
    if (this.destroyed || !this.booted || this.running) return;
    if (this.startPromise) return this.startPromise;
    const transition = ++this.transition;
    this.startPromise = (async () => {
      try {
        await this.audio.resume();
        if (this.destroyed || transition !== this.transition) return;
        if (document.hidden) { this.pause('Pausa al cambiar de pestaña'); return; }
        this.running = true;
        const overlay = this.q('.overlay'), pauseButton = this.q('[data-action=pause]');
        if (overlay) overlay.hidden = true;
        if (pauseButton) pauseButton.innerHTML = 'Pausa <kbd>P</kbd>';
        if (this.screenOnly) this.notify('');
        this.rebase(); this.statsTime = performance.now(); this.emulatedSinceStats = 0;
        this.pump(); this.animate();
        if (!this.audio.context) this.notify('El navegador no ha habilitado audio; el juego continúa sin sonido.');
      } catch (e) { this.notify('No se pudo activar el audio: ' + e.message + '. Pulsa Continuar para reintentar.'); }
      finally { this.startPromise = null; }
    })();
    return this.startPromise;
  }
  async play() {
    if (!this.booted) return;
    this.coinFrames = 2; this.startDelay = 18; this.hasPlayed = true;
    await this.resume();
  }
  insertCoin() { if (!this.booted) return; this.coinFrames = 2; this.resume(); }
  startPlayer(player) {
    if (!this.booted) return;
    this.hasPlayed = true;
    if (player === 2) this.start2Frames = 4; else this.start1Frames = 4;
    this.resume();
  }
  applyInputs() {
    this.m.input0 = 255 & ~this.directionBit; this.m.input1 = 255;
    if (this.coinFrames > 0) { this.m.input0 &= ~32; this.coinFrames--; }
    if (this.startDelay >= 0 && --this.startDelay === 0) { this.start1Frames = 4; this.startDelay = -1; }
    if (this.start1Frames > 0) { this.m.input1 &= ~32; this.start1Frames--; }
    if (this.start2Frames > 0) { this.m.input1 &= ~64; this.start2Frames--; }
  }
  rebase() {
    this.produced = 0;
    this.epoch = this.audio.context ? this.audio.arm() : performance.now() / 1000;
  }
  pump() {
    if (!this.running || this.destroyed) return;
    try {
      if (this.audio.context && this.audio.context.state !== 'running') { this.pause('El navegador suspendió el audio'); return; }
      let now = this.audio.context ? this.audio.context.currentTime : performance.now() / 1000;
      let target = (now - this.epoch + (this.audio.context ? 0.055 : FRAME_SECONDS)) * SAMPLE_RATE;
      // Never fast-forward a long browser stall or a background interval.
      if (target - this.produced > SAMPLE_RATE * 0.16) {
        this.resyncs++; this.rebase();
        now = this.audio.context ? this.audio.context.currentTime : performance.now() / 1000;
        target = (now - this.epoch + (this.audio.context ? 0.055 : FRAME_SECONDS)) * SAMPLE_RATE;
      }
      const begin = performance.now(); let frames = 0;
      while (this.produced < target && frames < 8) {
        this.applyInputs();
        const n = this.m.runFrame();
        this.audio.write(this.m.audio, n); this.produced += n; frames++; this.emulatedSinceStats++;
      }
      if (frames) this.frameMs = this.frameMs * 0.95 + ((performance.now() - begin) / frames) * 0.05;
      const wall = performance.now();
      if (wall - this.statsTime >= 1000) {
        const fps = this.emulatedSinceStats * 1000 / (wall - this.statsTime);
        const stats = this.q('.stats');
        if (stats) stats.textContent = fps.toFixed(1) + ' cuadros/s · ' + (this.frameMs + this.renderMs).toFixed(2) + ' ms/cuadro · ' + this.audio.mode;
        this.statsTime = wall; this.emulatedSinceStats = 0;
      }
      const aheadMs = (this.produced - target) / SAMPLE_RATE * 1000;
      this.timer = setTimeout(() => this.pump(), Math.max(4, Math.min(12, aheadMs)));
    } catch (e) { this.fail(e); }
  }
  animate() {
    if (!this.running || this.destroyed) return;
    if (this.m.frame !== this.drawnFrame) this.render();
    this.animation = requestAnimationFrame(() => this.animate());
  }
  render() {
    const start = performance.now();
    this.renderer.render(); this.ctx.putImageData(this.image, 0, 0);
    this.drawnFrame = this.m.frame;
    this.renderMs = this.renderMs * 0.95 + (performance.now() - start) * 0.05;
  }
  pause(reason = 'Partida en pausa') {
    this.transition++; this.running = false;
    clearTimeout(this.timer); cancelAnimationFrame(this.animation);
    this.audio.pause(); this.releaseDirections();
    if (this.booted) {
      this.overlay(reason, 'La máquina está detenida. Continúa cuando estés listo.', 'Continuar');
      const button = this.q('[data-action=pause]'), stats = this.q('.stats');
      if (button) button.innerHTML = 'Continuar <kbd>P</kbd>';
      if (stats) stats.textContent = 'En pausa · ' + this.audio.mode;
    }
  }
  togglePause() { if (this.running) this.pause(); else this.resume(); }
  toggleMute() {
    this.audio.setMuted(!this.audio.muted);
    const button = this.q('[data-action=mute]');
    if (!button) return;
    button.textContent = this.audio.muted ? '♪̸' : '♪';
    button.setAttribute('aria-pressed', String(this.audio.muted));
    button.setAttribute('aria-label', this.audio.muted ? 'Activar audio' : 'Silenciar audio');
    button.title = this.audio.muted ? 'Activar audio (M)' : 'Silenciar (M)';
  }
  fullscreen() {
    let p;
    if (document.fullscreenElement) p = document.exitFullscreen();
    else if (this.host.requestFullscreen) p = this.host.requestFullscreen();
    else { this.notify('Pantalla completa no disponible en este navegador.'); return; }
    if (p && p.catch) p.catch(() => this.notify('El navegador no permitió cambiar a pantalla completa.'));
  }
  reset() {
    if (!this.booted) return;
    this.pause(); this.booted = false; this.hasPlayed = false;
    this.m.reset(); this.coinFrames = this.start1Frames = this.start2Frames = 0; this.startDelay = -1;
    this.overlay('Reiniciando Pac-Man', 'Ejecutando el arranque original.', 'Iniciando…');
    this.q('.message').classList.remove('error');
    this.notify(''); this.bootBatch();
  }
  notify(text) { this.q('.message').textContent = text; }
  fail(error) {
    this.pause(); this.booted = false;
    this.overlay('No se pudo continuar', error.message || String(error), 'Error');
    this.notify(error.message || String(error));
    if (this.screenOnly) this.q('.message').classList.add('error');
    console.error('[PacmanWeb]', error);
  }
  getStats() {
    return {
      version: '1.2.0', running: this.running, booted: this.booted,
      frame: this.m.frame, pc: this.m.cpu.pc, cpuTStates: this.m.cpu.total_t_states,
      nominalFrameRate: CPU_HZ / FRAME_CYCLES, cpuAndAudioMs: this.frameMs, renderMs: this.renderMs,
      audioMode: this.audio.mode, audioState: this.audio.context ? this.audio.context.state : 'unavailable',
      outputSampleRate: this.audio.context ? this.audio.context.sampleRate : 0,
      muted: this.audio.muted, volume: this.audio.volume,
      underruns: this.audio.stats.underruns, overruns: this.audio.stats.overruns, resyncs: this.resyncs,
      audioFallbackReason: this.audio.failure
    };
  }
  destroy() {
    if (this.destroyed) return;
    this.pause(); this.destroyed = true;
    clearTimeout(this.bootTimer); this.abort.abort(); this.audio.dispose();
    this.readyResolve(this); this.shadow.replaceChildren();
  }
}

// CommonJS export is only for deterministic offline tests; browser use auto-mounts.
if (typeof module !== 'undefined' && module.exports && typeof document === 'undefined') {
  module.exports = {Machine, Renderer, z80_init, z80_reset, z80_step, boardRead, boardWrite, ROM_MANIFEST, ROM_SET, WORKLET_SOURCE};
  return;
}
if (typeof document === 'undefined') return;
let currentApp = null;
const api = {
  version: '1.2.0',
  roms: ROM_MANIFEST.map(r => Object.freeze({...r})),
  mount(target, options = {}) {
    const container = typeof target === 'string' ? document.querySelector(target) : target;
    if (!container || typeof container.appendChild !== 'function') throw new Error('Contenedor Pac-Man no encontrado.');
    if (currentApp) { const oldHost = currentApp.host; currentApp.destroy(); oldHost.remove(); }
    const host = document.createElement('pacman-arcade');
    container.appendChild(host);
    currentApp = new PacmanApp(host, options);
    return currentApp;
  },
  get app() { return currentApp; },
  getStats() { return currentApp ? currentApp.getStats() : null; },
  play() { return currentApp && currentApp.play(); },
  pause() { if (currentApp) currentApp.pause(); },
  resume() { return currentApp && currentApp.resume(); },
  reset() { if (currentApp) currentApp.reset(); },
  insertCoin() { if (currentApp) currentApp.insertCoin(); },
  destroy() { if (currentApp) { const host = currentApp.host; currentApp.destroy(); host.remove(); currentApp = null; } }
};
globalThis.PacmanWeb = api;
const script = document.currentScript;
const initialOptions = globalThis.PacmanOptions || {};
function autoMount() {
  if (initialOptions.autoMount === false || currentApp) return;
  try {
    const selector = initialOptions.container || (script && script.dataset.container);
    const container = selector ? document.querySelector(selector) : document.body;
    if (!selector && Array.from(document.body.children).every(el => el.tagName === 'SCRIPT')) {
      if (!document.querySelector('meta[name=viewport]')) {
        const meta = document.createElement('meta'); meta.name = 'viewport';
        meta.content = 'width=device-width,initial-scale=1,viewport-fit=cover'; document.head.appendChild(meta);
      }
      if (!document.title) document.title = 'Pac-Man · arcade-core';
    }
    api.mount(container, initialOptions);
  } catch (e) {
    const error = document.createElement('pre'); error.textContent = 'Pac-Man: ' + (e.message || e);
    error.className = 'pacman-load-error';
    document.body.appendChild(error); console.error('[PacmanWeb]', e);
  }
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', autoMount, {once: true});
else autoMount();

})();
