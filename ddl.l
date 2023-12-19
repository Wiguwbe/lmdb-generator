%{
#include "ddl.h"
#include "structs.h"
#include <string.h>
%}

%option noyywrap


%%

"table"	{ return KW_TABLE; }
"PK"	{ return KW_PK; }
"FK"	{ return KW_FK; }
"index"	{ return KW_INDEX; }

"ID"	{ return TY_ID; }
"INT"	{ return TY_INT; }
"CHAR"	{ return TY_CHAR; }

"("	{ return OP_PAREN; }
")"	{ return CL_PAREN; }
"{"	{ return OP_BRACE; }
"}"	{ return CL_BRACE; }
"."	{ return DOT; }
","	{ return COMMA; }

[a-zA-Z_][a-zA-Z_0-9]*	{ yylval.str = strdup(yytext); return IDENTIFIER; }
[0-9]+	{ yylval.str = strdup(yytext); return NUMBER; }

[ \t\n]+	{ /* ignore */ }
%%
