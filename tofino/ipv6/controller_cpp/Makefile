ifndef SDE_INSTALL
$(error SDE_INSTALL is not set)
endif

CXX := /usr/bin/gcc
CPPFLAGS := -I$(SDE_INSTALL)/include -DSDE_INSTALL=\"$(SDE_INSTALL)\" \
			-DPROG_NAME=\"telescope\"
CXXFLAGS = -g -std=c++17 -Wall -Wextra -Werror -MMD -MF $@.d
BF_LIBS  := -L$(SDE_INSTALL)/lib -ldriver -ltarget_utils -ltarget_sys
LDLIBS   := $(BF_LIBS) -lm -ldl -lpthread -lstdc++
LDFLAGS  := -Wl,-rpath,$(SDE_INSTALL)/lib

SOURCES := Register.cpp MonitoredTable.cpp ForwardTable.cpp MirrorManager.cpp MulticastGroup.cpp Node.cpp PortManager.cpp \
	PortsTable.cpp LocalClient.cpp main.cpp

OBJS := $(SOURCES:.cpp=.o)

TARGET := controller_ipv6

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $(OBJS) $(LDLIBS) $(LDFLAGS)

.PHONY: all clean

clean:
	-@rm -f $(OBJS) zlog-cfg-cur bf_drivers.log* *.d *~ $(TARGET)