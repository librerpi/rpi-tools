elf-tool: elf-tool.c
	${CC} $< -o $@

install: elf-tool
	mkdir -pv ${out}/bin
	cp elf-tool ${out}/bin/
