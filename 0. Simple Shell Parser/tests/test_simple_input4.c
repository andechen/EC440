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
/*==================== A command that redirects both stdin and stdout can be parsed ====================*/
	printf("\nA command that redirects both stdin and stdout can be parsed\n\n");
    struct pipeline* my_pipeline = pipeline_build("ls <txt >harambe\n");
	my_pipeline = pipeline_build("ls <txt >harambe\n");
    
	// Test that a pipeline was returned
	TEST_ASSERT(my_pipeline != NULL);
	TEST_ASSERT(!my_pipeline->is_background);
	TEST_ASSERT(my_pipeline->commands != NULL);

	// Test the parsed args
	TEST_ASSERT(strcmp("ls", my_pipeline->commands->command_args[0]) == 0);
	TEST_ASSERT(my_pipeline->commands->command_args[1] == NULL);
	

	// Test the redirect state
	TEST_ASSERT(my_pipeline->commands->redirect_in_path != NULL);
	TEST_ASSERT(strcmp("txt", my_pipeline->commands->redirect_in_path) == 0);
	

	TEST_ASSERT(my_pipeline->commands->redirect_out_path != NULL);
	TEST_ASSERT(strcmp("harambe", my_pipeline->commands->redirect_out_path) == 0);
	

	// Test that there is only one parsed command in the pipeline
	TEST_ASSERT(my_pipeline->commands->next == NULL);

	pipeline_free(my_pipeline);
}