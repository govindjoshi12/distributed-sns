# Hack: utf8_range (which is protobuf's dependency) currently doesn't have a pkgconfig file, so we need to explicitly
# tweak the list of libraries to link against to fix the build.

PROTOBUF_UTF8_RANGE_LINK_LIBS = -lutf8_validity

govind_laptop = true

grpc_loc = /home/csce662
install_dir = $(grpc_loc)/.lib
ifeq ($(GOVIND_LAPTOP), true)
grpc_loc = /home/govind/Documents/cs/lib/
install_dir = /home/govind/.local
endif

export PKG_CONFIG_PATH = $(install_dir)/lib/pkgconfig:$(grpc_loc)/grpc/third_party/re2:$(install_dir)/share/pkgconfig/


HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)
CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc `
CXXFLAGS += -std=c++17 -g

ifeq ($(SYSTEM),Darwin)
LDFLAGS += -L/usr/local/lib `pkg-config --libs --static protobuf grpc++  `\
           $(PROTOBUF_UTF8_RANGE_LINK_LIBS) \
           -pthread\
           -lgrpc++_reflection\
           -ldl
else
LDFLAGS += -L/usr/local/lib `pkg-config --libs --static protobuf grpc++  `\
           $(PROTOBUF_UTF8_RANGE_LINK_LIBS) \
           -pthread\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
           -ldl -lglog
endif

PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`
PROTOS_PATH = .

all: system-check tsd tsc coordinator 

tsc: client.o coordinator.pb.o coordinator.grpc.pb.o sns.pb.o sns.grpc.pb.o tsc.o
	$(CXX) $^ $(LDFLAGS) -g -o $@

tsd: coordinator.pb.o coordinator.grpc.pb.o sns.pb.o sns.grpc.pb.o tsd.o
	$(CXX) $^ $(LDFLAGS) -g -o $@

coordinator: coordinator.pb.o coordinator.grpc.pb.o coordinator.o
	$(CXX) $^ $(LDFLAGS) -g -o $@

.PRECIOUS: %.grpc.pb.cc
%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.txt *.o *.pb.cc *.pb.h tsc tsd coordinator 


# The following is to test your system and ensure a smoother experience.
# They are by no means necessary to actually compile a grpc-enabled software.

PROTOC_CMD = which $(PROTOC)
PROTOC_CHECK_CMD = $(PROTOC) --version | grep -q 'libprotoc.3\|libprotoc [0-9][0-9]\.'
PLUGIN_CHECK_CMD = which $(GRPC_CPP_PLUGIN)
HAS_PROTOC = $(shell $(PROTOC_CMD) > /dev/null && echo true || echo false)
ifeq ($(HAS_PROTOC),true)
HAS_VALID_PROTOC = $(shell $(PROTOC_CHECK_CMD) 2> /dev/null && echo true || echo false)
endif
HAS_PLUGIN = $(shell $(PLUGIN_CHECK_CMD) > /dev/null && echo true || echo false)

SYSTEM_OK = false
ifeq ($(HAS_VALID_PROTOC),true)
ifeq ($(HAS_PLUGIN),true)
SYSTEM_OK = true
endif
endif

system-check:
ifneq ($(HAS_VALID_PROTOC),true)
	@echo " DEPENDENCY ERROR"
	@echo
	@echo "You don't have protoc 3.0.0 installed in your path."
	@echo "Please install Google protocol buffers 3.0.0 and its compiler."
	@echo "You can find it here:"
	@echo
	@echo "   https://github.com/google/protobuf/releases/tag/v3.0.0"
	@echo
	@echo "Here is what I get when trying to evaluate your version of protoc:"
	@echo
	-$(PROTOC) --version
	@echo
	@echo
endif
ifneq ($(HAS_PLUGIN),true)
	@echo " DEPENDENCY ERROR"
	@echo
	@echo "You don't have the grpc c++ protobuf plugin installed in your path."
	@echo "Please install grpc. You can find it here:"
	@echo
	@echo "   https://github.com/grpc/grpc"
	@echo
	@echo "Here is what I get when trying to detect if you have the plugin:"
	@echo
	-which $(GRPC_CPP_PLUGIN)
	@echo
	@echo
endif
ifneq ($(SYSTEM_OK),true)
	@false
endif
