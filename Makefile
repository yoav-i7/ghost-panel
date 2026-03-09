# Compiler settings
CXX = g++
CXXFLAGS = -O2 -Wall -Wextra
LDFLAGS = -lX11

# Files and directories
TARGET = ghost-panel
SRCS = ghost-panel.cpp
INSTALL_DIR = $(HOME)/.local/bin

# Default target when you just run 'make'
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

# Rule to install the binary to your local bin directory
install: $(TARGET)
	mkdir -p $(INSTALL_DIR)
	install -m 755 $(TARGET) $(INSTALL_DIR)/
	install -m 755 start-ghost-panel.sh $(INSTALL_DIR)/
	install -m 755 toggle-ghost-panel.sh $(INSTALL_DIR)/
	cp gpanel.cfg $(INSTALL_DIR)/
	@echo "Successfully installed to $(INSTALL_DIR)/$(TARGET)"

# Rule to clean up the compiled files
clean:
	rm -f $(TARGET)

.PHONY: all install clean
