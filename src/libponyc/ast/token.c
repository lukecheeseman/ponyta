#include "token.h"
#include "lexer.h"
#include "stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct token_t
{
  token_id id;
  source_t* source;
  size_t line;
  size_t pos;
  char* printed;

  union
  {
    struct
    {
      const char* string;
      size_t str_length;
    };

    double real;
    lexint_t integer;
  };
} token_t;


token_t* token_new(token_id id)
{
  token_t* t = POOL_ALLOC(token_t);
  memset(t, 0, sizeof(token_t));
  t->id = id;
  return t;
}

token_t* token_dup(token_t* token)
{
  assert(token != NULL);
  token_t* t = POOL_ALLOC(token_t);
  memcpy(t, token, sizeof(token_t));
  t->printed = NULL;
  return t;
}


token_t* token_dup_new_id(token_t* token, token_id id)
{
  token_t* new_token = token_dup(token);
  new_token->id = id;
  return new_token;
}


void token_free(token_t* token)
{
  if(token == NULL)
    return;

  if(token->printed != NULL)
    ponyint_pool_free_size(64, token->printed);

  POOL_FREE(token_t, token);
}


// Read accessors

token_id token_get_id(token_t* token)
{
  assert(token != NULL);
  return token->id;
}


const char* token_string(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_STRING || token->id == TK_ID);
  return token->string;
}


size_t token_string_len(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_STRING || token->id == TK_ID);
  return token->str_length;
}


double token_float(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_FLOAT);
  return token->real;
}


lexint_t* token_int(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_INT);
  return &token->integer;
}


const char* token_print(token_t* token)
{
  assert(token != NULL);

  switch(token->id)
  {
    case TK_EOF:
      return "EOF";

    case TK_ID:
    case TK_STRING:
      return token->string;

    case TK_INT:
    {
      lexint_t value = token->integer;
      bool is_negative = value.is_negative;
      if(is_negative)
        lexint_negate(&value, &value);

      if (value.high == 0)
      {
        int size = is_negative ? 65 : 64;
        if (token->printed == NULL)
          token->printed = (char*)ponyint_pool_alloc_size(size);

        if (is_negative)
          snprintf(token->printed, size, "-%llu",
            (unsigned long long)value.low);
        else
          snprintf(token->printed, size, "%llu",
            (unsigned long long)value.low);
        return token->printed;
      }
      else
      {
        if (token->printed == NULL)
        {
          int size = is_negative ? 129 : 128;
          token->printed = (char*)ponyint_pool_alloc_size(size);
        }

        char* start = token->printed;
        if(is_negative)
          *(start++) = '-';

        char* digit = start;

        lexint_t t;
        t.low = value.low;
        t.high = value.high;
        t.is_negative = false;

        lexint_t rem;
        rem.is_negative = false;

        lexint_t tmp;
        while (lexint_cmp64(&t, 0) != 0)
        {
          rem.low = t.low;
          rem.high = t.high;
          lexint_div64(&t, &t, 10);
          lexint_mul64(&tmp, &t, 10);
          lexint_sub(&rem, &rem, &tmp);
          *(digit++) = (char)('0' + rem.low);
        }
        *digit = '\0';

        // the string representation of the value is backwards so reverse it
        char* end = digit - 1;
        while (start < end)
        {
          char tmp = *start;
          *start = *end;
          *end = tmp;
          start++;
          end--;
        }
        return token->printed;
      }
    }

    case TK_FLOAT:
    {
      if(token->printed == NULL)
        token->printed = (char*)ponyint_pool_alloc_size(64);

      int r = snprintf(token->printed, 64, "%g", token->real);

      if(strcspn(token->printed, ".e") == (size_t)r)
        snprintf(token->printed + r, 64 - r, ".0");

      return token->printed;
    }

    case TK_LEX_ERROR:
      return "LEX_ERROR";

    default:
      break;
  }

  const char* p = lexer_print(token->id);
  if(p != NULL)
    return p;

  if(token->printed == NULL)
    token->printed = (char*)ponyint_pool_alloc_size(64);

  snprintf(token->printed, 64, "Unknown_token_%d", token->id);
  return token->printed;
}


char* token_print_escaped(token_t* token)
{
  assert(token != NULL);
  const char* str = NULL;
  size_t str_len = 0;

  if(token->id == TK_STRING)
  {
    str = token->string;
    str_len = token->str_length;
  } else {
    str = token_print(token);
    str_len = strlen(str);
  }

  // Count the number of escapes so we know the size of the new string.
  size_t escapes = 0;
  for(size_t idx = 0; idx < str_len; idx++)
  {
    char c = str[idx];
    if((c == '"') || (c == '\\') || (c == 0))
      escapes++;
  }

  // Return a simple copy of the current string if there are no escapes.
  if(escapes == 0)
  {
    char* copy = (char*)ponyint_pool_alloc_size(str_len + 1);
    memcpy(copy, str, str_len);
    copy[str_len] = 0;
    return copy;
  }

  // Allocate a new buffer for the escaped string.
  size_t escaped_len = str_len + escapes;
  char* escaped = (char*)ponyint_pool_alloc_size(escaped_len + 1);

  // Fill the buffer of the escaped string, one unescaped character at a time.
  size_t escaped_idx = 0;
  for(size_t idx = 0; idx < str_len; idx++)
  {
    char c = str[idx];
    if((c == '"') || (c == '\\'))
    {
      escaped[escaped_idx++] = '\\';
      escaped[escaped_idx++] = c;
    } else if(c == 0) {
      escaped[escaped_idx++] = '\\';
      escaped[escaped_idx++] = '0';
    } else {
      escaped[escaped_idx++] = c;
    }
  }
  escaped[escaped_idx++] = 0;

  return escaped;
}


const char* token_id_desc(token_id id)
{
  switch(id)
  {
    case TK_EOF:    return "EOF";
    case TK_ID:     return "id";
    case TK_STRING: return "string literal";
    case TK_INT:    return "int literal";
    case TK_FLOAT:  return "float literal";
    case TK_TRUE:   return "true literal";
    case TK_FALSE:  return "false literal";
    default: break;
  }

  const char* p = lexer_print(id);
  if(p != NULL)
    return p;

  return "UNKOWN";
}


source_t* token_source(token_t* token)
{
  assert(token != NULL);
  return token->source;
}


size_t token_line_number(token_t* token)
{
  assert(token != NULL);
  return token->line;
}


size_t token_line_position(token_t* token)
{
  assert(token != NULL);
  return token->pos;
}


// Write accessors

void token_set_id(token_t* token, token_id id)
{
  assert(token != NULL);
  token->id = id;
}


void token_set_string(token_t* token, const char* value, size_t length)
{
  assert(token != NULL);
  assert(token->id == TK_STRING || token->id == TK_ID);
  assert(value != NULL);

  if(length == 0)
    length = strlen(value);

  token->string = stringtab_len(value, length);
  token->str_length = length;
}


void token_set_float(token_t* token, double value)
{
  assert(token != NULL);
  assert(token->id == TK_FLOAT);
  token->real = value;
}


void token_set_int(token_t* token, lexint_t* value)
{
  assert(token != NULL);
  assert(token->id == TK_INT);
  token->integer = *value;
}

void token_set_pos(token_t* token, source_t* source, size_t line, size_t pos)
{
  assert(token != NULL);

  if(source != NULL)
    token->source = source;

  token->line = line;
  token->pos = pos;
}
