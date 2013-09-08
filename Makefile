PROD=ottd_preview
CC=clang
LD=$(CC)
ARCH=
CFLAGS=-Werror -Wno-multichar -std=c99 -D_GNU_SOURCE -O3 -DHAVE_LIBPNG $(ARCH) -I/usr/local/include
LIBS=$(ARCH) -L/usr/local/lib -lz -llzma -llzo2 -lpng
OBJS=main.o ottd_preloader.o ottd_loader.o ottd_png.o ottd_date.o

all: $(PROD)

$(PROD): $(OBJS)
	$(LD) $^ -o $(PROD) $(LIBS)

%.o: %.c ottd.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(PROD)
