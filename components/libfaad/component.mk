
COMPONENT_ADD_INCLUDEDIRS := include codebook . frotend/include
# -DFIXED_POINT
COMPONENT_SRCDIRS:= . codebook frotend
CFLAGS += -DHAVE_MEMCPY -DSTDC_HEADERS -DHAVE_INTTYPES_H -DHAVE_INTTYPES_H -DHAVE_STRINGS_H -Wno-error=unused-function -Wno-unused-function -Wno-error=unused-variable -Wno-unused-variable -Wno-error=maybe-uninitialized -Wno-maybe-uninitialized -Wno-error=unused-value -Wno-unused-but-set-variable 
