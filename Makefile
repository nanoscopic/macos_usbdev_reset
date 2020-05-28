TARGET = devreset

all: $(TARGET)

$(TARGET): devreset.c
	gcc devreset.c -framework IOKit -framework CoreFoundation -o devreset

clean:
	$(RM) $(TARGET)