#include <stdio.h>
#include "../src/lexer.h"

int main(int argc, char *argv[]) {
	const char *str;
	if(argc != 2) str = "echo hola | tee hola.txt | cat > to_file.txt";
	else str = argv[1];

	Lexer *lx = lexer_new(str);
	Token tok = {0};
	printf("%s\n", str);
	while(lexer_next(lx, &tok)) {
		printf("%s(%.*s) ", token_kind_name(tok.kind), tok.len, tok.start);
	}
}
