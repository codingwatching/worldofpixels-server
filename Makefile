# https://stackoverflow.com/a/18258352
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

SRC_FILES = $(call rwildcard, src/, *.cpp)
OBJ_FILES = $(SRC_FILES:src/%.cpp=build/%.o)
DEP_FILES = $(OBJ_FILES:.o=.d)

TARGET    = out

OPT_REL   = -O2
LD_REL    = -s

OPT_DBG  = -Og -g
LD_DBG   =

OPT_UDBG   = -Og -g -fsanitize=address
LD_UDBG    = -fsanitize=address

CPPFLAGS += -std=c++17
CPPFLAGS += -MMD -MP

UWS       = ./lib/uWebSockets
JSON      = ./lib/json
NAGA      = ./lib/naga-utils
LIB_FILES = $(NAGA)/libnaga.a $(UWS)/libuWS.a

CPPFLAGS += -I ./src/
CPPFLAGS += -I $(UWS)/src/
CPPFLAGS += -I $(NAGA)/src/
CPPFLAGS += -I $(JSON)/include/
LDFLAGS  += -L $(UWS)/
LDFLAGS  += -L $(NAGA)/

LIBPNGLDL = $(shell libpng-config --ldflags)
LDLIBS   += -lssl -lz -lcrypto -lcurl -lpthread -lpq $(LIBPNGLDL)

ifeq ($(OS),Windows_NT)
	LDLIBS += -luv -lWs2_32 -lpsapi -liphlpapi -luserenv
endif

.PHONY: all rel udbg dirs clean clean-all

all: CPPFLAGS += $(OPT_DBG)
all: LDFLAGS += $(LD_DBG)
all: dirs $(TARGET)

udbg: CPPFLAGS += $(OPT_UDBG)
udbg: LDFLAGS  += $(LD_UDBG)
udbg: dirs $(TARGET)

rel: CPPFLAGS += $(OPT_REL)
rel: LDFLAGS  += $(LD_REL)
rel: dirs $(TARGET)

$(TARGET): $(OBJ_FILES) $(LIB_FILES)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

dirs:
	mkdir -p build

build/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) -c -o $@ $<


$(UWS)/libuWS.a:
	$(MAKE) -C $(UWS) -f ../uWebSockets.mk

$(NAGA)/libnaga.a:
	$(MAKE) -C $(NAGA)

clean:
	- $(RM) $(TARGET) $(OBJ_FILES) $(DEP_FILES)

clean-all: clean
	$(MAKE) -C $(UWS) -f ../uWebSockets.mk clean
	$(MAKE) -C $(NAGA) clean

-include $(DEP_FILES)
