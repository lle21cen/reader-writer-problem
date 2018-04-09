default :
	gcc -o ssu_vim ssu_vim.c printTime.c
	gcc -o ssu_ofm ssu_ofm.c daemon.c
clean :
	rm ssu_ofm ssu_vim
