#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "error: file path not provided\n");
		fprintf(stderr, "usage: %s <file>\n", *argv);
		exit(1);
	}
	char *path = argv[1];

	FILE *input = fopen(path, "r");
	if (!input) {
		fprintf(stderr, "error: could not read file '%s'\n", path);
		exit(1);
	}

	long size = strlen(path);
	path[size - 2] = 'c';
	path[size - 1] = '\0';

	FILE *output = fopen(path, "w");
	if (!output) {
		fprintf(stderr, "error: could not write file '%s'\n", path);
		exit(1);
	}

	fprintf(output, "#include <stdio.h>\n");
	fprintf(output, "char data[30000];\n");
	fprintf(output, "int main(void) {\n");
	fprintf(output, "char *p = data;\n");

	bool running = true;
	while (running) {
		switch (fgetc(input)) {
		case '+':
			fprintf(output, "(*p)++;\n");
			break;

		case '-':
			fprintf(output, "(*p)--;\n");
			break;

		case '>':
			fprintf(output, "p++;\n");
			break;

		case '<':
			fprintf(output, "p--;\n");
			break;

		case '.':
			fprintf(output, "fputc(*p, stdout);\n");
			break;

		case ',':
			fprintf(output, "*p = fgetc(stdin);\n");
			break;

		case '[':
			fprintf(output, "while (*p) {\n");
			break;

		case ']':
			fprintf(output, "}\n");
			break;

		case EOF:
			running = false;
			break;
		}
	}

	fprintf(output, "}\n");
	fclose(input);
	fclose(output);

	char *command = malloc(10 + 2 * size);
	if (!command) {
		fprintf(stderr, "error: could not allocate command\n");
		exit(1);
	}

	strcpy(command, "cc -Ofast -o ");
	strcpy(command + strlen(command), path);
	command[strlen(command) - 2] = ' ';
	command[strlen(command) - 1] = '\0';
	strcpy(command + strlen(command), path);

	if (system(command) != 0) {
		fprintf(stderr, "error: could not generate executable\n");
		exit(1);
	}

	free(command);
	unlink(path);
}
