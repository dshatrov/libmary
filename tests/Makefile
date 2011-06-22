.PHONY: all clean

all:

clean:
	find -maxdepth 1 -type d | grep -v "^\.\?\.\?$$" | sed 's/.\///' | xargs -i0 make -C 0 clean

