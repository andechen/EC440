#include "myshell_parser.h"
#include "stddef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WHITESPACE_CHARS "\n\t "
#define SPECIAL_CHARS "&|<>"
#define AMPERSAND "&"
#define PIPE "|"
#define RED_IN "<"
#define RED_OUT ">"

//Function to search for a character in the command line
bool findChars(char* line, char* chars){
	if(strstr(line, chars)){
		return true;
	}else{
		return false;
	}

}

//Fuction to delete any occurences of a character/word in the command line
char* deleteWords(char* s, char* oldWord, char* newWord){
	char* result;
	int i = 0, c = 0, newWordLength = strlen(newWord), oldWordLength = strlen(oldWord);

	for(i = 0; s[i] != '\0'; i++){
		if(strstr(&s[i], oldWord) == &s[i]){
			c++;
			i += oldWordLength - 1;
		}
	}
	result = (char*)malloc(i + c * (newWordLength - oldWordLength) + 1);
	
	i = 0; 
    while (*s) { 
        if (strstr(s, oldWord) == s) { 
            strcpy(&result[i], newWord); 
            i += newWordLength; 
            s += oldWordLength; 
        } 
        else
            result[i++] = *s++; 
    } 
  
    result[i] = '\0'; 
    return result; 

}

//Initialising a pipeline_command struct
struct pipeline_command *pipeline_command_alloc(){
	struct pipeline_command *pipeline_c = malloc(sizeof(struct pipeline_command));

	if(pipeline_c){
		for(int i = 0; i < MAX_ARGV_LENGTH; ++i){
			pipeline_c->command_args[i] = NULL;
		}
		pipeline_c->next = NULL;
		pipeline_c->redirect_in_path = NULL;
		pipeline_c->redirect_out_path = NULL;
	}
	return pipeline_c;
}

//Initialising a pipeline struct
struct pipeline* pipeline_alloc(){
	struct pipeline_command *pipeline_c = pipeline_command_alloc();
	struct pipeline *pipeline = malloc(sizeof(struct pipeline));

	pipeline->commands = pipeline_c;
	pipeline->is_background = false;
	
	return pipeline;
}

//Linking the relevant commands with members of the pipeline_command struct
struct pipeline_command* p_command_link(char* cline, struct pipeline_command* p_command){
	int pos = 0;
	
	if(findChars(cline, RED_IN)){
		char* tokens;
		char* line = strdup(cline);
		tokens = strtok(line, RED_IN);
		tokens = strtok(NULL, RED_IN);
		tokens = strtok(tokens, WHITESPACE_CHARS);
		tokens = strtok(tokens, SPECIAL_CHARS);
		p_command->redirect_in_path = tokens;
		line = deleteWords(cline, tokens, "");
		cline = strdup(line);
		free(line);
	}

	if(findChars(cline, RED_OUT)){
		char* tokens;
		char* line = strdup(cline);
		tokens = strtok(line, RED_OUT);
		tokens = strtok(NULL, RED_OUT);
		tokens = strtok(tokens, WHITESPACE_CHARS);
		tokens = strtok(tokens, SPECIAL_CHARS);
		p_command->redirect_out_path = tokens;
		line = deleteWords(cline, tokens, "");
		cline = strdup(line);
		free(line);
	}
	char* tokens;
	char* tok;
	
	while((tokens = strtok_r(cline, WHITESPACE_CHARS, &cline))){
		while((tok = strtok_r(tokens, SPECIAL_CHARS, &tokens))){
			p_command->command_args[pos] = tok;
		}
		pos++;
	}
	
	return p_command;
}

//Linking the relevant commands with members of the pipeline struct and linking the pipelines
void pipelines(char* pipe_tokens, struct pipeline *pipe){
	struct pipeline_command *last = pipeline_command_alloc();

	last = p_command_link(pipe_tokens, last);

	if(pipe->commands->command_args[0] == NULL){
		pipe->commands = last;
	}
	else{
		struct pipeline_command *temp = pipeline_command_alloc();
		temp = pipe->commands;
		while(temp->next != NULL){
			temp = temp->next;
		}
		temp->next = last;
	}
	return;
}

//Building the pipeline
struct pipeline *pipeline_build(const char *command_line)
{
	struct pipeline* pipeline_v = pipeline_alloc();
	char* cline = strdup(command_line);
	char* pipe_tokens;
	
	if(findChars(cline, AMPERSAND)){
		char* line = strdup(cline);
		line = strtok(line, AMPERSAND);
		pipeline_v->is_background = true;
	}
	
	if(findChars(cline, PIPE)){
		while((pipe_tokens = strtok_r(cline, PIPE, &cline))){
			pipelines(pipe_tokens, pipeline_v);
		}

	}else{
		while((pipe_tokens = strtok_r(cline, PIPE, &cline))){
			pipelines(pipe_tokens, pipeline_v);
		}
	}
	return pipeline_v;
	
	//return NULL;
}

//Freeing the allocated memory of the structs
void pipeline_free(struct pipeline *pipeline)
{
	struct pipeline_command *current_command = pipeline_command_alloc();
	struct pipeline_command *next_command = pipeline_command_alloc();

	current_command = pipeline->commands;
	
	while(current_command != NULL){
		next_command = current_command->next;
		free(current_command);
		current_command = next_command;
	}

	free(next_command);
	free(current_command);
	free(pipeline);
}
