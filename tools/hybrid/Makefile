# Links the GPGX libretro objects (built with HOOK_CPU=1) and the oracle shim into one core.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
GPGX := $(ROOT)/ref/gpgx
OBJS := $(shell find $(GPGX) -name '*.o')
OUT  := $(ROOT)/build/nhl_oracle.so
# same defines and include paths the core itself was compiled with (see ref/gpgx/hook_build.log)
DEFS := -DUSE_LIBTREMOR -DUSE_LIBCHDR -DHAVE_LROUND -DHAVE_STDINT_H -DZ7_ST -DHAVE_SYS_PARAM_H -DHOOK_CPU \
  -DUSE_16BPP_RENDERING -DFRONTEND_SUPPORTS_RGB565 -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DHAVE_ZLIB \
  -DMAXROMSIZE=33554432 -D__LIBRETRO__ -DUSE_LIBRETRO_VFS -DM68K_OVERCLOCK_SHIFT=20 -DZ80_OVERCLOCK_SHIFT=20 \
  -DHAVE_YM3438_CORE -DHAVE_OPLL_CORE -DINLINE="static inline"
INCS := -I$(GPGX)/core -I$(GPGX)/core/z80 -I$(GPGX)/core/m68k -I$(GPGX)/core/ntsc -I$(GPGX)/core/sound \
  -I$(GPGX)/core/input_hw -I$(GPGX)/core/cd_hw -I$(GPGX)/core/cart_hw -I$(GPGX)/core/cart_hw/svp -I$(GPGX)/core/debug \
  -I$(GPGX)/core/cd_hw/libchdr/include -I$(GPGX)/libretro -I$(GPGX)/libretro/libretro-common/include
CFLAGS := -O2 -fPIC -std=gnu11 -Wall $(DEFS) $(INCS)

all: $(OUT)
$(ROOT)/build/oracle_shim.o: $(ROOT)/tools/hybrid/oracle_shim.c
	mkdir -p $(ROOT)/build
	$(CC) $(CFLAGS) -c $< -o $@
$(OUT): $(ROOT)/build/oracle_shim.o $(OBJS)
	$(CC) -shared -o $@ $^ -Wl,--version-script=$(ROOT)/tools/hybrid/link.T -Wl,--no-undefined -lm
clean:
	rm -f $(OUT) $(ROOT)/build/oracle_shim.o
