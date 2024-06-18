hello-world: hello.o
	clang -e _start -o hello hello.o

otooldiff: hello.o
	zsh -c 'git difftool --no-index =(otool -Thlxv ~/pg/test.o) =(otool -Thlxv hello.o)'

bindiff: hello.o
	zsh -c 'git difftool --no-index =(xxd ~/pg/test.o) =(xxd hello.o)'

hello.o: gen
	./gen

.PHONY: clean
clean:
	rm -f hello.o hello

