"""Minimal libretro host for driving Genesis Plus GX headlessly from Python.

Used as the oracle for lockstep verification: boot the retail ROM, feed a scripted input
stream, and read back the 68k work RAM (64 KB at $FF0000) and the framebuffer each frame.

GPGX stores work RAM with the bytes of each word swapped on little-endian hosts
(READ_BYTE(base, addr) is base[addr ^ 1]); ram() undoes that and returns the image exactly
as the 68k sees it, so byte offsets match the RAM map in the asm rebuild.
"""
import ctypes as C
import struct
import zlib
import os

# --- libretro ABI ------------------------------------------------------------------------
RETRO_API_VERSION = 1
RETRO_DEVICE_JOYPAD = 1
RETRO_MEMORY_SAVE_RAM = 0
RETRO_MEMORY_SYSTEM_RAM = 2
RETRO_PIXEL_FORMAT_0RGB1555, RETRO_PIXEL_FORMAT_XRGB8888, RETRO_PIXEL_FORMAT_RGB565 = 0, 1, 2

ENV_GET_OVERSCAN = 2
ENV_GET_CAN_DUPE = 3
ENV_GET_SYSTEM_DIRECTORY = 9
ENV_SET_PIXEL_FORMAT = 10
ENV_GET_VARIABLE = 15
ENV_SET_VARIABLES = 16
ENV_GET_VARIABLE_UPDATE = 17
ENV_GET_LOG_INTERFACE = 27
ENV_GET_SAVE_DIRECTORY = 31
ENV_SET_CONTROLLER_INFO = 35
ENV_SET_MEMORY_MAPS = 36
ENV_GET_INPUT_BITMASKS = 51 | 0x10000
ENV_GET_CORE_OPTIONS_VERSION = 52
ENV_SET_CORE_OPTIONS = 53
ENV_SET_CORE_OPTIONS_INTL = 54
ENV_SET_CORE_OPTIONS_V2 = 67
ENV_SET_CORE_OPTIONS_V2_INTL = 68

# RetroPad ids -> Genesis buttons as GPGX maps them
JOY = dict(B=0, Y=1, SELECT=2, START=3, UP=4, DOWN=5, LEFT=6, RIGHT=7, A=8, X=9, L=10, R=11)
GENESIS = {  # Genesis name -> retropad id
    'A': JOY['Y'], 'B': JOY['B'], 'C': JOY['A'], 'X': JOY['L'], 'Y': JOY['X'], 'Z': JOY['R'],
    'START': JOY['START'], 'MODE': JOY['SELECT'],
    'UP': JOY['UP'], 'DOWN': JOY['DOWN'], 'LEFT': JOY['LEFT'], 'RIGHT': JOY['RIGHT'],
}

environment_t = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
video_refresh_t = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
audio_sample_t = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
audio_sample_batch_t = C.CFUNCTYPE(C.c_size_t, C.POINTER(C.c_int16), C.c_size_t)
input_poll_t = C.CFUNCTYPE(None)
input_state_t = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)


class GameInfo(C.Structure):
    _fields_ = [('path', C.c_char_p), ('data', C.c_void_p), ('size', C.c_size_t), ('meta', C.c_char_p)]


class Geometry(C.Structure):
    _fields_ = [('base_width', C.c_uint), ('base_height', C.c_uint), ('max_width', C.c_uint),
                ('max_height', C.c_uint), ('aspect_ratio', C.c_float)]


class Timing(C.Structure):
    _fields_ = [('fps', C.c_double), ('sample_rate', C.c_double)]


class AvInfo(C.Structure):
    _fields_ = [('geometry', Geometry), ('timing', Timing)]


class Variable(C.Structure):
    _fields_ = [('key', C.c_char_p), ('value', C.c_char_p)]


DEFAULT_OPTIONS = {
    'genesis_plus_gx_system_hw': 'mega drive / genesis',
    'genesis_plus_gx_region_detect': 'ntsc-u',
    'genesis_plus_gx_bios': 'disabled',
    'genesis_plus_gx_lock_on': 'disabled',
    'genesis_plus_gx_addr_error': 'enabled',
    'genesis_plus_gx_no_sprite_limit': 'disabled',
    'genesis_plus_gx_overscan': 'disabled',
    'genesis_plus_gx_gg_extra': 'disabled',
    'genesis_plus_gx_aspect_ratio': 'auto',
    'genesis_plus_gx_render': 'single field',
    'genesis_plus_gx_frameskip': 'disabled',
    'genesis_plus_gx_audio_filter': 'disabled',
    'genesis_plus_gx_ym2612': 'mame (ym2612)',
    'genesis_plus_gx_sound_output': 'stereo',
    'genesis_plus_gx_blargg_ntsc_filter': 'disabled',
    'genesis_plus_gx_force_dtack': 'enabled',
    'genesis_plus_gx_system_bram': 'per bios',
    'genesis_plus_gx_cd_latency': 'enabled',
}


class Core:
    def __init__(self, so_path, options=None, sysdir='/tmp'):
        self.lib = C.CDLL(os.path.abspath(so_path))
        self.options = dict(DEFAULT_OPTIONS, **(options or {}))
        self.pixel_format = RETRO_PIXEL_FORMAT_0RGB1555
        self.frame = None            # (bytes, width, height, pitch)
        self.frame_count = 0
        self.audio_samples = 0
        self.buttons = [0, 0]        # bitmask of retropad ids per port
        self._cstrings = [C.c_char_p(sysdir.encode())]
        self._value_strs = {}
        self.pending_frame_handlers = []

        lib = self.lib
        lib.retro_api_version.restype = C.c_uint
        lib.retro_get_memory_data.restype = C.c_void_p
        lib.retro_get_memory_data.argtypes = [C.c_uint]
        lib.retro_get_memory_size.restype = C.c_size_t
        lib.retro_get_memory_size.argtypes = [C.c_uint]
        lib.retro_serialize_size.restype = C.c_size_t
        lib.retro_serialize.restype = C.c_bool
        lib.retro_serialize.argtypes = [C.c_void_p, C.c_size_t]
        lib.retro_unserialize.restype = C.c_bool
        lib.retro_unserialize.argtypes = [C.c_void_p, C.c_size_t]
        lib.retro_load_game.restype = C.c_bool
        lib.retro_load_game.argtypes = [C.POINTER(GameInfo)]
        lib.retro_get_system_av_info.argtypes = [C.POINTER(AvInfo)]
        assert lib.retro_api_version() == RETRO_API_VERSION

        # keep callback objects alive on self
        self._cb_env = environment_t(self._environment)
        self._cb_video = video_refresh_t(self._video)
        self._cb_audio = audio_sample_t(self._audio)
        self._cb_audio_batch = audio_sample_batch_t(self._audio_batch)
        self._cb_poll = input_poll_t(lambda: None)
        self._cb_state = input_state_t(self._input_state)
        lib.retro_set_environment(self._cb_env)
        lib.retro_set_video_refresh(self._cb_video)
        lib.retro_set_audio_sample(self._cb_audio)
        lib.retro_set_audio_sample_batch(self._cb_audio_batch)
        lib.retro_set_input_poll(self._cb_poll)
        lib.retro_set_input_state(self._cb_state)
        lib.retro_init()

    # -- callbacks ---------------------------------------------------------------------------
    def _environment(self, cmd, data):
        if cmd == ENV_GET_CAN_DUPE:
            C.cast(data, C.POINTER(C.c_bool))[0] = True
            return True
        if cmd == ENV_GET_OVERSCAN:
            C.cast(data, C.POINTER(C.c_bool))[0] = False
            return True
        if cmd in (ENV_GET_SYSTEM_DIRECTORY, ENV_GET_SAVE_DIRECTORY):
            C.cast(data, C.POINTER(C.c_char_p))[0] = self._cstrings[0]
            return True
        if cmd == ENV_SET_PIXEL_FORMAT:
            fmt = C.cast(data, C.POINTER(C.c_int))[0]
            if fmt in (RETRO_PIXEL_FORMAT_RGB565, RETRO_PIXEL_FORMAT_0RGB1555):
                self.pixel_format = fmt
                return True
            return False
        if cmd == ENV_GET_VARIABLE:
            var = C.cast(data, C.POINTER(Variable))[0]
            key = var.key.decode() if var.key else ''
            if key in self.options:
                s = self._value_strs.setdefault(key, C.c_char_p(self.options[key].encode()))
                C.cast(data, C.POINTER(Variable))[0].value = s.value
                return True
            return False
        if cmd == ENV_GET_VARIABLE_UPDATE:
            C.cast(data, C.POINTER(C.c_bool))[0] = False
            return True
        if cmd == ENV_GET_CORE_OPTIONS_VERSION:
            C.cast(data, C.POINTER(C.c_uint))[0] = 0   # we only answer GET_VARIABLE
            return True
        if cmd in (ENV_SET_VARIABLES, ENV_SET_CONTROLLER_INFO, ENV_SET_MEMORY_MAPS,
                   ENV_SET_CORE_OPTIONS, ENV_SET_CORE_OPTIONS_INTL, ENV_SET_CORE_OPTIONS_V2,
                   ENV_SET_CORE_OPTIONS_V2_INTL):
            return True
        return False

    def _video(self, data, width, height, pitch):
        self.frame_count += 1
        if data:
            buf = C.string_at(data, pitch * height)
            self.frame = (buf, width, height, pitch)

    def _audio(self, l, r):
        self.audio_samples += 1

    def _audio_batch(self, data, frames):
        self.audio_samples += frames
        return frames

    def _input_state(self, port, device, index, id_):
        if port < len(self.buttons) and (device & 0xFF) == RETRO_DEVICE_JOYPAD:
            return 1 if (self.buttons[port] >> id_) & 1 else 0
        return 0

    # -- API -----------------------------------------------------------------------------------
    def load(self, rom_path):
        self._rom_path = C.c_char_p(os.path.abspath(rom_path).encode())
        info = GameInfo(self._rom_path, None, 0, None)
        if not self.lib.retro_load_game(C.byref(info)):
            raise RuntimeError('retro_load_game failed')
        av = AvInfo()
        self.lib.retro_get_system_av_info(C.byref(av))
        self.av = av
        self.lib.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD)
        self.lib.retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD)
        return av

    def set_buttons(self, port, names):
        """names: iterable of Genesis button names ('A','B','C','START','UP',...)."""
        mask = 0
        for n in names:
            mask |= 1 << GENESIS[n.upper()]
        self.buttons[port] = mask

    def run(self, frames=1):
        for _ in range(frames):
            self.lib.retro_run()

    def ram(self):
        """68k work RAM as the CPU sees it: 65536 bytes, big-endian byte order, offset 0 = $FF0000."""
        p = self.lib.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)
        n = self.lib.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM)
        raw = C.string_at(p, n)
        # undo GPGX's in-word byte swap on little-endian hosts
        b = bytearray(raw)
        b[0::2], b[1::2] = raw[1::2], raw[0::2]
        return bytes(b)

    def poke_w(self, addr, val):
        """Write a big-endian word into 68k work RAM (GPGX keeps it word-swapped on little-endian hosts)."""
        p = self.lib.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)
        n = self.lib.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM)
        buf = (C.c_uint8 * n).from_address(p)
        o = addr & 0xFFFF
        buf[o ^ 1] = (val >> 8) & 0xFF
        buf[(o + 1) ^ 1] = val & 0xFF

    def ram_w(self, addr):
        """Big-endian word at a 68k address ($FFxxxx or offset)."""
        r = self.ram()
        o = addr & 0xFFFF
        return (r[o] << 8) | r[o + 1]

    def serialize(self):
        n = self.lib.retro_serialize_size()
        buf = C.create_string_buffer(n)
        if not self.lib.retro_serialize(buf, n):
            raise RuntimeError('serialize failed')
        return buf.raw

    def unserialize(self, blob):
        if not self.lib.retro_unserialize(blob, len(blob)):
            raise RuntimeError('unserialize failed')

    def screenshot(self, path):
        if not self.frame:
            return False
        buf, w, h, pitch = self.frame
        rows = []
        for y in range(h):
            row = bytearray([0])
            line = buf[y * pitch: y * pitch + w * 2]
            px = struct.unpack('<%dH' % w, line)
            if self.pixel_format == RETRO_PIXEL_FORMAT_RGB565:
                for p in px:
                    r = (p >> 11) & 0x1F; g = (p >> 5) & 0x3F; b = p & 0x1F
                    row += bytes(((r * 255) // 31, (g * 255) // 63, (b * 255) // 31))
            else:
                for p in px:
                    r = (p >> 10) & 0x1F; g = (p >> 5) & 0x1F; b = p & 0x1F
                    row += bytes(((r * 255) // 31, (g * 255) // 31, (b * 255) // 31))
            rows.append(bytes(row))
        png = _png(w, h, b''.join(rows))
        with open(path, 'wb') as f:
            f.write(png)
        return True


def _png(w, h, raw_rows):
    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(raw_rows, 6)) + chunk(b'IEND', b'')
