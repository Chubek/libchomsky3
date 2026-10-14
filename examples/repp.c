#include "AwkPP-CRuntime.h"


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


typedef enum
{
   TOKEN_Ident,
   TOKEN_Number,
   TOKEN_Keyword,
} Token;


Token lex(awkpp_input_t *input)
{
    awkpp_lexer_t *lexer = awkpp_load("Spec.lx");
}
