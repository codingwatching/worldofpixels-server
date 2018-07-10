# https://stackoverflow.com/a/18258352
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

SRC_FILES = $(call rwildcard, src/, *.cpp)
OBJ_FILES = $(SRC_FILES:src/%.cpp=build/%.o)
DEP_FILES = $(OBJ_FILES:.o=.d)

TARGET    = out

OPT_REL   = -O2
LD_REL    = -s
OPT_DBG   = -Og -g

CPPFLAGS += -std=c++14
CPPFLAGS += -MMD -MP

UWS       = ./lib/uWebSockets
JSON      = ./lib/json
LIB_FILES = $(UWS)/libuWS.a

CPPFLAGS += -I ./src/
CPPFLAGS += -I $(UWS)/src/
CPPFLAGS += -I $(JSON)/single_include/
LDFLAGS  += -L $(UWS)/

LDLIBS   += -lssl -lz -lcrypto -lcurl

ifeq ($(OS),Windows_NT)
	LDLIBS += -luv -lpthread -lWs2_32 -lpsapi -liphlpapi -luserenv
endif

.PHONY: all g dirs clean clean-all

all: CPPFLAGS += $(OPT_REL)
all: LDFLAGS  += $(LD_REL)
all: dirs $(TARGET)

g: CPPFLAGS += $(OPT_DBG)
g: LDFLAGS  += $(LD_DBG)
g: dirs $(TARGET)

$(TARGET): $(OBJ_FILES) $(LIB_FILES)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

dirs:
	mkdir -p build/misc

build/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) -c -o $@ $<


$(UWS)/libuWS.a:
	$(MAKE) -C $(UWS) -f ../uWebSockets.mk


clean:
	- $(RM) $(TARGET) $(OBJ_FILES) $(DEP_FILES)

clean-all: clean
	$(MAKE) -C $(UWS) -f ../uWebSockets.mk clean

-include $(DEP_FILES)
