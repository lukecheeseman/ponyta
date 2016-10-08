#include "../evaluate/equality.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/ds/hash.h"
#include <assert.h>

static equality_entry_t* equality_entry_dup(equality_entry_t* entry)
{
  equality_entry_t* e = POOL_ALLOC(equality_entry_t);
  e->type = entry->type;
  e->member = entry->member;
  e->head = entry->head;
  e->current = entry->current;
  return e;
}

static size_t equality_hash(equality_entry_t* entry)
{
  //TODO: concat and hash is probably better
  return ponyint_hash_str(entry->type) ^ ponyint_hash_str(entry->member);
}

static bool equality_cmp(equality_entry_t* a, equality_entry_t* b)
{
  return a->type == b->type && a->member == b->member;
}

static void equality_free(equality_entry_t* equality)
{
  POOL_FREE(equality_entry_t, equality);
}

DEFINE_HASHMAP(equality_tab, equality_tab_t, equality_entry_t,
  equality_hash, equality_cmp, ponyint_pool_alloc_size, ponyint_pool_free_size,
  equality_free);

equality_entry_t* search_equality(equality_tab_t* table, const char* type_name, const char* member_name)
{
  assert(table != NULL);
  assert(type_name != NULL);
  assert(member_name != NULL);

  equality_entry_t e1 = {type_name, member_name, NULL, NULL};
  equality_entry_t* e2 = equality_tab_get(table, &e1);
  return e2;
}

void mark_check_equality(equality_tab_t* table, const char* type_name, const char* member_name,
                         ast_t* lhs, ast_t* rhs)
{
  assert(table != NULL);
  assert(type_name != NULL);
  assert(member_name != NULL);
  assert(lhs != NULL);
  assert(rhs != NULL);

  equality_list_t* new_link = POOL_ALLOC(equality_list_t);
  new_link->lhs = ast_dup(lhs);
  new_link->rhs = ast_dup(rhs);

  equality_entry_t e1 = {type_name, member_name, new_link, new_link};
  equality_entry_t* e2 = equality_tab_get(table, &e1);
  if(e2 != NULL)
  {
    e2->current->next = new_link;
    e2->current = new_link;
  }
  else
    equality_tab_put(table, equality_entry_dup(&e1));
}

equality_tab_t* equality_tab_new()
{
  equality_tab_t* table = POOL_ALLOC(equality_tab_t);
  equality_tab_init(table, 512);
  return table;
}

bool ast_equal(ast_t* left, ast_t* right)
{
  if(left == NULL || right == NULL)
    return left == NULL && right == NULL;

  // Look through all of the constant expression directives
  if(ast_id(left) == TK_CONSTANT)
    return ast_equal(ast_child(left), right);

  if(ast_id(right) == TK_CONSTANT)
    return ast_equal(left, ast_child(right));

  if(ast_id(left) != ast_id(right))
    return false;

  switch(ast_id(left))
  {
    case TK_ID:
    case TK_STRING:
      return ast_name(left) == ast_name(right);

    case TK_INT:
      return lexint_cmp(ast_int(left), ast_int(right)) == 0;

    case TK_FLOAT:
      return ast_float(left) == ast_float(right);

    // In this case we only compare the names of the objects; giving identity
    // equivalence
    case TK_CONSTANT_OBJECT:
      return ast_equal(ast_child(left), ast_child(right));

    default:
      break;
  }

  ast_t* l_child = ast_child(left);
  ast_t* r_child = ast_child(right);

  while(l_child != NULL && r_child != NULL)
  {
    if(!ast_equal(l_child, r_child))
      return false;

    l_child = ast_sibling(l_child);
    r_child = ast_sibling(r_child);
  }

  return l_child == NULL && r_child == NULL;
}
