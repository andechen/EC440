#include "myshell_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(x) do { \
	if (!(x)) { \
		fprintf(stderr, "%s:%d: Assertion (%s) failed!\n", \
				__FILE__, __LINE__, #x); \
	       	abort(); \
	} \
} while(0)

int main(void){
/*==================== The shell can parse a command that uses multiple features of the shell ====================*/
	printf("\nThe shell can parse a command that uses multiple features of the shell\n\n");
	struct pipeline* my_pipeline = pipeline_build("ls& <txt >harambe| hi >commando\n");
	my_pipeline = pipeline_build("ls& <txt >harambe| hi >commando\n");

	// Test that a pipeline was returned
	TEST_ASSERT(my_pipeline != NULL);
	TEST_ASSERT(my_pipeline->is_background);
	TEST_ASSERT(my_pipeline->commands != NULL);

	// Test the parsed args
	TEST_ASSERT(strcmp("ls", my_pipeline->commands->command_args[0]) == 0);
	TEST_ASSERT(strcmp("hi", my_pipeline->commands->next->command_args[0]) == 0);
	TEST_ASSERT(my_pipeline->commands->command_args[1] == NULL);
	TEST_ASSERT(my_pipeline->commands->next->command_args[1] == NULL);

	// Test the redirect state
	TEST_ASSERT(my_pipeline->commands->redirect_in_path != NULL);
	TEST_ASSERT(strcmp("txt", my_pipeline->commands->redirect_in_path) == 0);

	TEST_ASSERT(my_pipeline->commands->redirect_out_path != NULL);
	TEST_ASSERT(strcmp("harambe", my_pipeline->commands->redirect_out_path) == 0);

	// Test that there are parsed command in the pipeline
	TEST_ASSERT(my_pipeline->commands->next != NULL);

	pipeline_free(my_pipeline);
}