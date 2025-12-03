TARGET = devreset

all: universal

$(TARGET)_x86: $(TARGET).c
	gcc $(TARGET).c -framework IOKit -framework CoreFoundation -o $(TARGET)_x86 -target x86_64-apple-macos10.12

$(TARGET)_arm: $(TARGET).c
	gcc $(TARGET).c -framework IOKit -framework CoreFoundation -o $(TARGET)_arm -target arm64-apple-macos11

universal: $(TARGET)_x86 $(TARGET)_arm
	lipo -create -output $(TARGET) $(TARGET)_x86 $(TARGET)_arm

clean:
	$(RM) $(TARGET) $(TARGET)_x86 $(TARGET)_arm
