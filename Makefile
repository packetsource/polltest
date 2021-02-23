# Definitions
CC = gcc
LD = gcc
CFLAGS = -g 
LDFLAGS = -g
LIB = 
EXE = polltest
OBJ = polltest.o

# General single file Go rule
%: %.go
	go build $<

# General C rule
%.o: %.c $(INCLUDE)
	$(CC) $(CFLAGS) -c $<

# Default rule
_default: $(EXE) source

# Main app
$(EXE): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIB)

# Clean
clean:
	rm -rf $(OBJ) *.o source
