# main source files extension
SOURCE_EXT = .cpp

CC = g++
# used to make individual rules
CPP = cpp
CFLAGS = -g -Werror -Wall -Wextra -std=c++11 -x c++
LDFLAGS = 

# dirs for source and object files
SDIR = 		src
ODIR = 		obj

find_files_src = $(wildcard $(dir)/*$(SOURCE_EXT))
find_files_d = $(wildcard $(dir)/*.d)
SRCDIRS = $(shell find $(SDIR) -type d)
OBJDIRS = $(patsubst $(SDIR)%,$(ODIR)%,$(SRCDIRS))

SRC = 		$(foreach dir,$(SRCDIRS),$(find_files_src))
OBJ =		$(patsubst $(SDIR)/%$(SOURCE_EXT),$(ODIR)/%.o,$(SRC))
DEP =		$(OBJ:.o=.d)

# output file
MAIN = nfso

build: $(MAIN)

$(MAIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

include $(DEP)

$(ODIR)/%.d: $(SDIR)/%.cpp dirs
	$(CPP) $(CFLAGS) $< -MM -MT "$(@:.d=.o) $@" > $@
	@echo '	$(CC) $(CFLAGS) -c -o $(@:.d=.o) $<' >> $@
	@echo '	@touch -c $@' >> $@

.PHONY: dirs
dirs:
	mkdir -p $(OBJDIRS)

.PHONY: clean
clean:
	rm -f $(OBJ) $(MAIN) $(DEP)
	rm -rd $(ODIR)
