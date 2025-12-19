CC      := cc
CFLAGS  := -Wall -Wextra -std=c11 -Iinclude
LDFLAGS :=

TARGET := gradecalc

SRCS := \
  src/main.c \
  src/csv.c \
  src/grades.c \
  src/io.c \
  src/calc.c \
  src/ui.c

OBJS := $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
