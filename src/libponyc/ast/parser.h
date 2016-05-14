#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "source.h"

bool pass_parse(ast_t* package, source_t* source, errors_t* errors);

#endif
