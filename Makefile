SRC     = free.c
OUT     = free
CFLAGS  = -Wall -Wextra
IDIR    = -I/usr/local/include
LDIR    = -L/usr/local/lib
SHARED  = -lkvm -lm -lintl 
DEFS    = -DENABLE_LOCALE

all:
	${CC} ${SRC} ${CFLAGS} ${DEFS} ${IDIR} ${LDIR} ${SHARED} -o ${OUT}

clean:
	rm -f ${OUT}

trans-init:
	@mkdir -p po
	@echo "Directory po has been created."
	@echo -n "Now create a directory of the language "
	@echo "you want to translate."
	@echo "For example: mkdir po/fr"
	@printf "\n"
	@echo "After creating the directory, run the following command: "
	@echo "xgettext --keyword=_ --language=C --add-comments -o po/free.pot free.c"
	@printf "\n"
	@echo "This will create a pot file called free.pot in the po directory."
	@echo "You've to edit this file as later it will be served as a template file."
	@printf "\n"
	@echo "After completing the above steps, run the following command: "
	@echo "msginit --input=po/free.pot --locale=fr --output=po/fr/free.po"
	@printf "\n"
	@echo "The above command will generate a po file from that pot template."
	@echo "This is the file where all translation of that language will go."
	@printf "\n"
	@echo -n "Open the file in an editor, and translate every msgid section and "
	@echo "write those translation in msgstr section."
	@printf "\n"
	@echo "After this step, run the following command: "
	@echo "msgfmt --output-file=po/fr/free.mo po/fr/free.po"
	@printf "\n"
	@echo "If everything is okay, above command will not print anything stdout."
	@printf "\n"
	@echo "Please do not translate command line options (such as --total, --version, etc.)."
	@printf "\n"
	@echo "For an example, see the tr folder. Have fun!"
